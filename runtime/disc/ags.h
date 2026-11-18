#pragma once
// AGS layout as the game files have it: 0x20 header, one 128-byte header per state, then per state: element heads
// (64 bytes each), element bodies (32 + anim + 48 texture ref + clip points), pools, names, padded to 16
#include <cstdint>
#include <cstring>
#include <vector>
#include <map>
#include <algorithm>

namespace ags {

struct Ref { uint32_t pos, clip_pos, body; int clips; };
struct State { uint32_t start, end, p0, n0; std::vector<Ref> refs; };

static inline uint32_t rd(const std::vector<uint8_t>& d, uint32_t o) { uint32_t v = 0; if (o + 4 <= d.size()) memcpy(&v, d.data() + o, 4); return v; }
static inline void wr(std::vector<uint8_t>& d, uint32_t o, uint32_t v) { if (o + 4 <= d.size()) memcpy(d.data() + o, &v, 4); }
static inline uint32_t al16(uint32_t x) { return (x + 15) & ~15u; }

static bool parse(const std::vector<uint8_t>& d, std::vector<State>& out) {
    out.clear();
    if (d.size() < 0x20 || memcmp(d.data(), "AGS", 4)) return false;
    uint32_t n = rd(d, 0xC);
    if (!n || n > 4096 || 0x20 + 128ull * n > d.size()) return false;
    uint32_t p = 0x20 + 128 * n;
    for (uint32_t s = 0; s < n; s++) {
        uint32_t h = 0x20 + 128 * s, ne = rd(d, h + 4);
        if (ne > 4096) return false;
        State st; st.start = p;
        p += 64 * ne;
        for (uint32_t e = 0; e < ne; e++) {
            if (p + 32 > d.size()) return false;
            uint32_t anim = rd(d, p + 8), body = p;
            p += 32;
            if (anim) {
                uint32_t k = 0;
                for (int i = 0; i < 16; i++) k += rd(d, p + 4 * i);
                if (k > 100000) return false;
                p += 64 + 32 * k;
            }
            Ref r; r.pos = p; r.clip_pos = 0; r.clips = 0; r.body = body;
            uint32_t clip = rd(d, p + 0x1C);
            p += 48;
            if (clip) {
                uint32_t nc = rd(d, p + 4);
                if (nc > 4) return false;
                p += 48;
                r.clip_pos = p; r.clips = (int)nc;
                uint32_t fr = 0;
                for (uint32_t c = 0; c < nc; c++) fr += rd(d, p + 32 * c + 8);
                p += 32 * nc + 32 * fr;
            }
            st.refs.push_back(r);
            if (p > d.size()) return false;
        }
        st.p0 = p + 32 * rd(d, h + 12);
        st.n0 = rd(d, h + 20);
        p = al16(st.p0 + 8 * st.n0);
        p = al16(p + 8 * rd(d, h + 28) + 16 * rd(d, h + 36));
        p += 16 * rd(d, h + 44);
        p = al16(p + rd(d, h + 100));
        st.end = p;
        if (p > d.size()) return false;
        out.push_back(st);
    }
    return true;
}

static bool textured(const std::vector<uint8_t>& d, const Ref& r) { return rd(d, r.pos + 4) != 0xFFFFFFFFu && r.clips > 0; }
static int on_state(const std::vector<uint8_t>& d, const std::vector<State>& S) {   // first state that draws a label
    for (size_t s = 0; s < S.size(); s++) for (auto& r : S[s].refs) if (textured(d, r)) return (int)s;
    return -1;
}
static int off_state(const std::vector<uint8_t>& d, const std::vector<State>& S) {   // first state that draws none
    for (size_t s = 0; s < S.size(); s++) {
        bool any = false;
        for (auto& r : S[s].refs) any = any || textured(d, r);
        if (!any) return (int)s;
    }
    return -1;
}
// texture and clip rectangle of the label a state draws
static bool rect(const std::vector<uint8_t>& d, const State& st, int* tex, int* tw, int* th, float r[4]) {
    for (auto& f : st.refs) {
        if (!textured(d, f)) continue;
        uint32_t sz = rd(d, f.pos + 8);
        *tex = (int)rd(d, f.pos + 4); *tw = (int)(sz & 0xFFFF); *th = (int)(sz >> 16);
        r[0] = r[1] = 1e9f; r[2] = r[3] = -1e9f;
        for (int c = 0; c < f.clips; c++) {
            uint32_t idx = rd(d, f.clip_pos + 32 * c);
            if (idx >= st.n0) continue;
            float xy[2]; memcpy(xy, d.data() + st.p0 + 8 * idx, 8);
            r[0] = std::min(r[0], xy[0]); r[1] = std::min(r[1], xy[1]); r[2] = std::max(r[2], xy[0]); r[3] = std::max(r[3], xy[1]);
        }
        return r[2] > r[0] && r[3] > r[1];
    }
    return false;
}

// rebuilds `count` states: src[s] = the state copied into s (default s itself, new states state 0); tex[s] = {RHG
// texture, width, height} retargets the copy's textured elements to that whole texture; {texture, width, height, x0, y0,
// x1, y1} to that rectangle of it; {texture, width, height, -1} also retargets elements without clip points (whole textures)
static bool remap(const std::vector<uint8_t>& d, int count, const std::map<int, int>& src, const std::map<int, std::vector<int>>& tex, std::vector<uint8_t>& out) {
    std::vector<State> S;
    if (!parse(d, S)) return false;
    int n = (int)S.size();
    if (count < n) count = n;
    if (count == n && src.empty() && tex.empty()) { out = d; return true; }
    out.assign(d.begin(), d.begin() + 0x20);
    wr(out, 0xC, (uint32_t)count); wr(out, 0x10, (uint32_t)count);
    std::vector<int> from(count);
    for (int s = 0; s < count; s++) {
        auto it = src.find(s);
        from[s] = it != src.end() && it->second >= 0 && it->second < n ? it->second : s < n ? s : 0;
        out.insert(out.end(), d.begin() + 0x20 + 128 * from[s], d.begin() + 0x20 + 128 * (from[s] + 1));
    }
    for (int s = 0; s < count; s++) {
        const State& z = S[from[s]];
        std::vector<uint8_t> b(d.begin() + z.start, d.begin() + z.end);
        auto it = tex.find(s);
        if (it != tex.end()) {
            // new clip corners and the element's pivot (center) get pool entries of their own: pool values are shared
            uint32_t h = 0x20 + 128 * from[s], info = z.p0 - z.start - 32 * rd(d, h + 12), p0 = z.p0 - z.start, n0 = z.n0;
            std::vector<float> add;
            auto pt = [&](uint32_t idx, float* xy) { if (idx < n0) memcpy(xy, b.data() + p0 + 8 * idx, 8); else { xy[0] = add[2 * (idx - n0)]; xy[1] = add[2 * (idx - n0) + 1]; } };
            auto put = [&](float x, float y) { add.push_back(x); add.push_back(y); return n0 + (uint32_t)add.size() / 2 - 1; };
            bool rc = it->second.size() >= 7, whole = it->second.size() == 4 && it->second[3] == -1;
            for (auto& r : z.refs) {
                uint32_t o = r.pos - z.start;
                if (rd(b, o + 4) == 0xFFFFFFFFu) continue;
                if (!r.clips) {
                    if (whole) { wr(b, o + 4, (uint32_t)it->second[0]); wr(b, o + 8, (uint32_t)(it->second[1] & 0xFFFF) | (uint32_t)it->second[2] << 16); }
                    continue;
                }
                wr(b, o + 4, (uint32_t)it->second[0]);
                wr(b, o + 8, (uint32_t)(it->second[1] & 0xFFFF) | (uint32_t)it->second[2] << 16);
                float mn[2] = {1e9f, 1e9f}, mx[2] = {-1e9f, -1e9f};
                for (int c = 0; c < r.clips; c++) { float xy[2]; pt(rd(b, r.clip_pos - z.start + 32 * c), xy); for (int a = 0; a < 2; a++) { mn[a] = std::min(mn[a], xy[a]); mx[a] = std::max(mx[a], xy[a]); } }
                float nx0 = rc ? (float)it->second[3] : 0.0f, ny0 = rc ? (float)it->second[4] : 0.0f;
                float nx1 = (float)it->second[rc ? 5 : 1], ny1 = (float)it->second[rc ? 6 : 2];
                for (int c = 0; c < r.clips; c++) {
                    uint32_t at = r.clip_pos - z.start + 32 * c;
                    float xy[2]; pt(rd(b, at), xy);
                    wr(b, at, put(xy[0] * 2 <= mn[0] + mx[0] ? nx0 : nx1, xy[1] * 2 <= mn[1] + mx[1] ? ny0 : ny1));   // corners can be off by an epsilon
                }
                uint32_t ie = info + 32 * rd(b, r.body - z.start);
                if (ie + 8 <= p0) {
                    float c[2]; pt(rd(b, ie + 4), c);
                    wr(b, ie + 4, put(c[0] + nx0 - mn[0], c[1] + ny0 - mn[1]));
                }
            }
            if (!add.empty()) {
                uint32_t old_end = al16(p0 + 8 * n0), nn = n0 + (uint32_t)add.size() / 2;
                std::vector<uint8_t> pool(b.begin() + p0, b.begin() + p0 + 8 * n0);
                const uint8_t* ab = (const uint8_t*)add.data();
                pool.insert(pool.end(), ab, ab + 4 * add.size());
                while ((p0 + pool.size()) & 15) pool.push_back(0);
                b.erase(b.begin() + p0, b.begin() + old_end);
                b.insert(b.begin() + p0, pool.begin(), pool.end());
                wr(out, 0x20 + 128 * s + 20, nn);
            }
        }
        out.insert(out.end(), b.begin(), b.end());
    }
    while (out.size() & 63) out.push_back(0);
    return true;
}
static bool extend(const std::vector<uint8_t>& d, int count, const std::map<int, std::vector<int>>& tex, std::vector<uint8_t>& out) { return remap(d, count, {}, tex, out); }

}
