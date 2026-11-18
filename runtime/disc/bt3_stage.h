#pragma once
// BT3 (PS2) stage file read in place: the PCK2 (BMA3 map, MAPD, cameras, effects) is served to the game as TTT's 26-entry map pak;
// the BMA3 is laid out as BMA7 with GE float vertices, every texture is a 16x16 marker the port draws from the stage's own CTEX
#include "disc/bt3.h"
#include <cmath>
#include <random>

namespace bt3 {

static const int STAGE_ENTRIES = 26;
static const int STAGE_TEX = 0xFF;   // bt3_tex_decode entry for stage textures: image < 0x80 = BMA3 texture, 0x80 + n = CTEX of PCK2 entry n, 0xC0 + i = scroll sheet image i
static const int OBJ_TEX = 0xA0, SCROLL_TEX = 0xC0, SURF_TEX = 0xE0, PTCL_TEX = 0x100, PTCL_KEY = 0xFE;   // PTCL_TEX + i: weather image i (entry 10), + 0x20 + i: effect image i (entry 17), + 0x40 + i: storm image i (entry 13)
static const int STAGE_BANKS[5] = {10, 13, 17, 19, 20};   // game pack index = entry + 1: 19 = index 0x14, the night moon / sun sprite

static bool pck2_list(const uint8_t* d, size_t n, std::vector<std::pair<uint32_t, uint32_t>>& e) {
    if (n < 16) return false;
    uint32_t c = u32(d);
    if (c < 8 || c > 64 || 8 + 4 * (size_t)c > n) return false;
    e.clear();
    for (uint32_t i = 0; i < c; i++) {
        uint32_t a = u32(d + 4 + 4 * i), b = u32(d + 8 + 4 * i);
        if (a > b || b > n || a < 4 + 4 * (c + 1)) return false;
        e.push_back({a, b - a});
    }
    return true;
}
static bool is_bma3(const uint8_t* m, size_t n) {
    return n >= 112 && m[0] == 18 && !m[1] && !m[2] && !m[3] && !u32(m + 4) && !u32(m + 8) && u32(m + 12) == 0xFFFFFFFFu && u32(m + 0x18) && u32(m + 0x54) &&
           (size_t)u32(m + 0x54) * 4 + 32 <= n;
}
// header check only (the PCK2 index and the BMA3 header fit in the first 0x200 bytes)
static bool is_stage(const uint8_t* d, size_t n, size_t full) {
    if (n < 16) return false;
    uint32_t c = u32(d);
    if (c < 20 || c > 64 || 8 + 4 * (size_t)c > n) return false;
    uint32_t a = u32(d + 4), b = u32(d + 8), end = u32(d + 4 + 4 * c);
    if (end > full || a >= b || a + 112 > n) return false;
    return is_bma3(d + a, b - a);
}

// EftSurfFile (PCK2 entry 12 after its 0x40 EftSurfParam): version 4, texture records laid out like CTEX entries with byte offsets
static const uint8_t* surf_file(const uint8_t* d, size_t n, size_t* fn) {
    if (n < 0x40 + 32) return nullptr;
    const uint8_t* f = d + 0x40;
    *fn = n - 0x40;
    if (u32(f + 4) != 4 || u32(f + 0x10) > 32 || u32(f + 0x18) > 4096 || (size_t)u32(f + 0x14) + 64 * (size_t)u32(f + 0x10) > *fn || (size_t)u32(f + 0x1C) + 96 * (size_t)u32(f + 0x18) > *fn) return nullptr;
    return f;
}
static bool surf_texture(const uint8_t* d, size_t n, int i, int* w, int* h, std::vector<uint32_t>& px) {
    size_t fn;
    const uint8_t* f = surf_file(d, n, &fn);
    if (!f || i < 0 || (uint32_t)i >= u32(f + 0x10)) return false;
    const uint8_t* r = f + u32(f + 0x14) + 64 * (size_t)i;
    std::vector<uint8_t> t(96 + fn);
    uint32_t hv[2] = {1, 8};
    memcpy(&t[0], hv, 8);
    memcpy(&t[32], r, 64);
    uint32_t a = (96 + u32(r)) / 4, b = (96 + u32(r + 4)) / 4;
    memcpy(&t[32], &a, 4); memcpy(&t[36], &b, 4);
    memcpy(&t[96], f, fn);
    return texture_in(t.data(), (uint32_t)t.size(), 1, 32, 0, w, h, px);
}

// StgOfsTable (PCK2 entry 8): three byte offsets per animation (texture file, node block, track); size = up to the next offset
static bool ofs_table(const uint8_t* d, size_t n, std::vector<std::pair<uint32_t, uint32_t>>& t) {
    t.clear();
    if (n < 0x10 || memcmp(d, "EPCK", 4)) return false;
    uint32_t c = u32(d + 8);
    if (!c || c % 3 || c > 3 * 64 || 0x10 + 4 * (size_t)c > n) return false;
    std::vector<uint32_t> o(c), srt;
    for (uint32_t i = 0; i < c; i++) { o[i] = u32(d + 0x10 + 4 * i); if (o[i] < 0x10 + 4 * c || o[i] >= n) return false; srt.push_back(o[i]); }
    srt.push_back((uint32_t)n);
    std::sort(srt.begin(), srt.end());
    for (uint32_t i = 0; i < c; i++) t.push_back({o[i], *std::upper_bound(srt.begin(), srt.end(), o[i]) - o[i]});
    return true;
}
static bool obj_texture(const uint8_t* d, size_t n, int k, int* w, int* h, std::vector<uint32_t>& px) {
    std::vector<std::pair<uint32_t, uint32_t>> t;
    if (!ofs_table(d, n, t)) return false;
    for (size_t a = 0; a < t.size(); a += 3) {
        const uint8_t* f = d + t[a].first;
        uint32_t fn = t[a].second, c = fn >= 8 ? u32(f) : 0;
        if (k < (int)c) return texture(f, fn, k, w, h, px);
        k -= (int)c;
    }
    return false;
}

static bool stage_texture(const uint8_t* d, size_t n, int k, int* w, int* h, std::vector<uint32_t>& px) {
    std::vector<std::pair<uint32_t, uint32_t>> e;
    if (!pck2_list(d, n, e)) return false;
    if (k >= PTCL_TEX) { int sel = (k - PTCL_TEX) >> 5; size_t en = sel == 2 ? 13 : sel ? 17 : 10; return e.size() > en && e[en].second >= 0x20 && texture(d + e[en].first, e[en].second, (k - PTCL_TEX) & 31, w, h, px); }
    if (k >= OBJ_TEX && k < SCROLL_TEX) return e.size() > 8 && obj_texture(d + e[8].first, e[8].second, k - OBJ_TEX, w, h, px);
    if (k >= SURF_TEX) return e.size() > 12 && surf_texture(d + e[12].first, e[12].second, k - SURF_TEX, w, h, px);
    if (k >= SCROLL_TEX) return e.size() > 5 && e[5].second >= 0x20 && texture(d + e[5].first, e[5].second, k - SCROLL_TEX, w, h, px);
    if (k >= 0x80) {
        size_t i = (size_t)(k - 0x80);
        return i < e.size() && e[i].second >= 0x20 && texture(d + e[i].first, e[i].second, 0, w, h, px);
    }
    const uint8_t* m = d + e[0].first;
    uint32_t mn = e[0].second, tb = u32(m + 0x54) * 4;
    if (!is_bma3(m, mn)) return false;
    return texture_in(m, mn, u32(m + tb), u32(m + tb + 4) * 4, k, w, h, px);
}

struct StageFx { const uint8_t* d = nullptr; std::vector<std::pair<uint32_t, uint32_t>> e; int haze = -1, bub = -1, storm = -1; };   // mod's <map>.fx.txt (-1 = not set) and <map>.fx.bin (PCK2 with entries 13 / 17)
struct StageInfo { std::vector<uint8_t> layers; std::vector<uint8_t> ptcl_src; int ptcl_fx = 0; int ptcl_wtex = 0, ptcl_etex = 0, ptcl_stex = 0, ptcl_water = 0, ptcl_weather = 0, ptcl_geysers = 0, ptcl_smoke = 0, ptcl_steam = 0; int obj_anims = 0, obj_count = 0, obj_tex = 0, obj_verts = 0, tex_anims = 0, surf_reflect = 0, surf_tex = 0, surf_meshes = 0, surf_tris = 0, surf_skipped = 0; int scroll_tex = 0, scroll_tris = 0; float scroll[4] = {0, 0, 0, 0}; int ntex = 0, groups = 0, parts = 0, strips = 0, verts = 0, nodes = 0, octree = 0, zones = 0, meshes = 0, rel = 0, ex = 0; std::vector<int> banks; std::vector<std::array<float, 3>> ex_pos; };

struct Out {
    std::vector<uint8_t> o;
    size_t put(size_t sz, size_t a = 16) { size_t at = (o.size() + a - 1) / a * a; o.resize(at + sz, 0); return at; }
    void w(size_t at, uint32_t v) { memcpy(&o[at], &v, 4); }
    void f(size_t at, float v) { memcpy(&o[at], &v, 4); }
};

static void tex_marker(uint8_t* p, uint32_t id) {
    static const uint8_t MK[8] = {'T', 'T', 'T', 'P', 'N', 'G', 0, 0};
    if (!id) return;
    uint32_t c = ~id;
    memcpy(p, MK, 8); memcpy(p + 8, &id, 4); memcpy(p + 12, &c, 4);
}

// TTT effect texture bank (PCK1 entries 10/13/17/20): a 16x16 256-colour marker
static std::vector<uint8_t> stage_bank(uint32_t id) {
    std::vector<uint8_t> b(128 + 256 + 1024, 0);
    uint32_t hv[] = {1, 8, 4, 4};
    memcpy(&b[0], hv, 16);
    uint32_t sv[] = {0x20, 32 + 4 * 16, 256, 1024, 4, 4, 0, 0, 0, 0};
    memcpy(&b[0x20], sv, 40);
    b[0x48] = 4; b[0x49] = 4; b[0x4B] = 9; b[0x4D] = 1;
    memset(&b[0x54], 0xFF, 12);
    tex_marker(&b[128], id);
    return b;
}

static bool vif_strips(const uint8_t* m, size_t mn, uint32_t geo, std::vector<std::pair<uint32_t, uint32_t>>& st) {
    st.clear();
    if (geo + 12 > mn) return false;
    uint32_t qwc = u32(m + geo) & 0xFFFFFF, size = (qwc << 4) + 32;
    if (m[geo + 3] != 0x60 || geo + size > mn) return false;
    auto be = [&](size_t p) { return (uint32_t)m[p] << 24 | (uint32_t)m[p + 1] << 16 | (uint32_t)m[p + 2] << 8 | m[p + 3]; };
    bool ext = be(geo + 8) == 0x0080016Cu;
    size_t pos = geo + (ext ? 0x1C : 4), fin = geo + size;
    while (pos + 52 < fin) {
        uint32_t mk = be(pos);
        if (mk == 0x70) break;
        if (mk != 0x0080036Cu) { pos += 4; continue; }
        uint32_t cnt = be(pos + 0x24) >> 24;
        size_t base = pos + 0x38;
        if (base + 48 * (size_t)cnt + 4 > fin) break;
        if (cnt >= 3) st.push_back({(uint32_t)base, cnt});
        pos = base + 48 * (size_t)cnt + 4;
    }
    return true;
}

// BT3 texture animation (StgMTexAnim): the stage timers (hdr +0x10 count, +0x14 table, 16 B {flags, count, frames, time, period}) step the
// texture of group 2 materials whose anim points at one; slot = (group 2 subgroup) in file order
struct TexAnimSlot { uint32_t sub, timer, cur; };
static void texanim_slots(const uint8_t* m, size_t mn, std::vector<TexAnimSlot>& sl, std::vector<uint8_t>* blob) {
    sl.clear();
    if (blob) blob->clear();
    auto P = [&](size_t at) -> size_t { return (size_t)u32(m + at) * 4; };
    uint32_t nT = u32(m + 0x10), nG = u32(m + 0x18);
    size_t T0 = P(0x14), G0 = P(0x1C);
    if (!nT || nT > 256 || T0 + 16 * (size_t)nT > mn || nG < 3 || G0 + 8 * (size_t)nG > mn) return;
    uint16_t ns = u16(m + G0 + 16 + 2);
    size_t sp = P(G0 + 16 + 4);
    if (sp + 16 * (size_t)ns > mn) return;
    for (uint32_t s = 0; s < ns; s++) {
        size_t a = P(sp + 16 * s + 8);
        if (!u32(m + sp + 16 * s + 8) || a < T0 || a >= T0 + 16 * (size_t)nT || (a - T0) % 16) continue;
        sl.push_back({s, (uint32_t)((a - T0) / 16), u16(m + sp + 16 * s + 12)});
    }
    if (!blob || sl.empty()) return;
    Out w;
    w.put(16 + 16 * (size_t)nT + 8 * sl.size());
    memcpy(&w.o[0], "BTAN", 4); w.w(4, (uint32_t)sl.size()); w.w(8, nT); w.w(12, P(0x54) + 4 <= mn ? u32(m + P(0x54)) : 0);
    for (uint32_t i = 0; i < nT; i++) {
        const uint8_t* t = m + T0 + 16 * i;
        uint16_t cnt = u16(t + 2);
        size_t fp = P(T0 + 16 * i + 4);
        if (fp + 4 * (size_t)cnt > mn) cnt = 0;
        size_t fo = w.put(2 * (size_t)cnt + 2, 4);
        for (uint32_t k = 0; k < cnt; k++) { uint16_t f = (uint16_t)u32(m + fp + 4 * k); memcpy(&w.o[fo + 2 * k], &f, 2); }
        size_t e = 16 + 16 * (size_t)i;
        uint32_t fl = u16(t) | (uint32_t)cnt << 16;
        w.w(e, fl); w.f(e + 4, f32(t + 12)); w.w(e + 8, (uint32_t)fo);
    }
    for (size_t i = 0; i < sl.size(); i++) { size_t e = 16 + 16 * (size_t)nT + 8 * i; w.w(e, sl[i].timer); w.w(e + 4, sl[i].cur); }
    w.put(0, 16);
    blob->swap(w.o);
}

// BMA3 -> BMA7. tex_id(k) = marker id of BMA3 texture k (0 while only sizes are wanted)
template <class F>
static bool stage_bma7(const uint8_t* m, size_t mn, F tex_id, std::vector<uint8_t>& res, StageInfo& si, bool layers = false, uint32_t token = 0) {
    if (!is_bma3(m, mn)) return false;
    auto P = [&](size_t at) -> size_t { return (size_t)u32(m + at) * 4; };
    auto ok = [&](size_t at, size_t len) { return at + len <= mn; };
    Out w;
    w.put(0x80, 1);
    // textures
    size_t tb = P(0x54);
    uint32_t nt = u32(m + tb);
    if (nt > 0x7F || !ok(tb, 32)) return false;
    si.ntex = (int)nt;
    size_t T0 = w.put(32 + 96 * (size_t)nt), A0 = w.put(16 * (size_t)nt);
    w.w(T0, nt); w.w(T0 + 4, (uint32_t)(T0 + 32) >> 2); w.w(T0 + 8, nt); w.w(T0 + 12, 4 * nt);
    for (uint32_t i = 0; i < nt; i++) {
        size_t px = w.put(256), cl = w.put(1024), e = T0 + 32 + 96 * i;
        tex_marker(&w.o[px], tex_id((int)i));
        w.w(e, (uint32_t)px >> 2); w.w(e + 4, (uint32_t)cl >> 2); w.w(e + 8, 256); w.w(e + 12, 1024); w.w(e + 0x10, 1); w.w(e + 0x14, 4);
        w.w(e + 0x20, i); w.w(e + 0x24, 4 * i);
        w.o[e + 0x28] = 4; w.o[e + 0x29] = 4; w.o[e + 0x2B] = 9; w.w(e + 0x2C, 256);
        memset(&w.o[e + 0x34], 0xFF, 12);
        w.w(A0 + 16 * i + 4, 128); w.w(A0 + 16 * i + 12, i ? 128 : 100);
    }
    // REL: TTT tracks carry 16 keys over 60 frames; a BT3 single-key placement becomes 16 equal keys
    uint32_t nR = u32(m + 0x30);
    size_t R0 = P(0x34);
    if (nR > 0x10000 || (nR && !ok(R0, 80 * (size_t)nR))) return false;
    std::map<size_t, size_t> relmap;
    size_t RT = w.put(80 * (size_t)nR);
    for (uint32_t i = 0; i < nR; i++) {
        size_t e = R0 + 80 * i, ne = RT + 80 * i;
        uint32_t nk = u32(m + e + 0x40), dur = u32(m + e + 0x48);
        size_t ko = P(e + 0x44);
        if (nk > 4096 || (nk && !ok(ko, 32 * (size_t)nk))) return false;
        memcpy(&w.o[ne], m + e, 64);
        bool still = nk <= 1 || !dur;
        uint32_t nk2 = still ? 16 : nk;
        size_t KB = w.put(32 * (size_t)nk2);
        for (uint32_t k = 0; k < nk2; k++) {
            size_t src = ko + 32 * (still ? 0 : k), dst = KB + 32 * k;
            if (nk) memcpy(&w.o[dst], m + src, 32);
            if (still) { w.w(dst + 0x0C, 4 * k); w.w(dst + 0x1C, 0); }
        }
        w.w(ne + 0x40, nk2); w.w(ne + 0x44, (uint32_t)KB); w.w(ne + 0x48, still ? 60 : dur); w.w(ne + 0x4C, 0);
        relmap[e] = ne;
    }
    si.rel = (int)nR;
    // geometry: one display list per BT3 part, GE float uv / colour 8888 / float position (TTT units: x/2, y/4, z/2 of the s16 range)
    uint32_t nG = u32(m + 0x18);
    size_t G0 = P(0x1C);
    if (!nG || nG > 16 || !ok(G0, 8 * (size_t)nG)) return false;
    struct PartRef { size_t old; size_t dl, dlsz; uint32_t tex; };
    std::vector<std::vector<PartRef>> groups(nG);
    const uint32_t VT = 3u | 7u << 2 | 3u << 7;
    const size_t lead = layers ? 16 : 0;   // port layer markers before ORIGIN: F1/F2 = the stage's layer token (GPU side table), F0 = group, F6 = texture animation slot
    std::vector<TexAnimSlot> tas;
    if (lead) texanim_slots(m, mn, tas, nullptr);
    for (uint32_t g = 0; g < nG; g++) {
        uint16_t ns = u16(m + G0 + 8 * g + 2);
        size_t sl = P(G0 + 8 * g + 4);
        if (ns && !ok(sl, 16 * (size_t)ns)) return false;
        for (uint32_t s = 0; s < ns; s++) {
            uint16_t tex = u16(m + sl + 16 * s), np = u16(m + sl + 16 * s + 2);
            size_t pl = P(sl + 16 * s + 4);
            if (np && !ok(pl, 16 * (size_t)np)) return false;
            int slot = -1;
            if (g == 2) for (size_t i = 0; i < tas.size(); i++) if (tas[i].sub == s) slot = (int)i;
            const uint32_t ta = slot >= 0 ? 4 : 0;   // F7 after the strips: the animated texture ends with the part
            for (uint32_t p = 0; p < np; p++) {
                size_t po = pl + 16 * p;
                std::vector<std::pair<uint32_t, uint32_t>> st;
                if (!vif_strips(m, mn, (uint32_t)P(po + 8), st)) return false;
                uint32_t n = (uint32_t)st.size();
                size_t head = al(28 + 8 * n + ta + 8 + 96, 16), body = 0;
                for (auto& t : st) body += al(24 * t.second, 16);
                size_t blk = w.put(lead + head + body), dl = blk + lead, vp = dl + head;
                if (lead) { w.w(blk, 0x00F10000u | (token & 0xFFFF)); w.w(blk + 4, 0x00F20000u | (token >> 16)); w.w(blk + 8, 0x00F00000u | g); if (slot >= 0) w.w(blk + 12, 0x00F60000u | (uint32_t)(slot + 1)); }
                w.w(dl, 0x14000000u); w.w(dl + 4, 0x10000000u); w.w(dl + 8, 0x01000000u | (36 + 8 * n + ta)); w.w(dl + 12, 0x12000180u);
                w.w(dl + 16, 0x07000008u); w.w(dl + 20, 0x09000000u | (28 + 8 * n)); w.w(dl + 24, 0x12000000u | VT);
                float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
                                for (uint32_t i = 0; i < n; i++) {
                    w.w(dl + 28 + 8 * i, 0x01000000u | (uint32_t)(vp - dl));
                    w.w(dl + 32 + 8 * i, 0x04040000u | st[i].second);
                    for (uint32_t k = 0; k < st[i].second; k++) {
                        const uint8_t* v = m + st[i].first + 48 * k;
                        size_t q = vp + 24 * k;
                        float x = f32(v) / 16384.0f, y = f32(v + 4) / 8192.0f, z = f32(v + 8) / 16384.0f;
                        auto cb = [](float c) { int b = (int)std::lround(c * 17.0f / 16.0f); return (uint32_t)std::max(0, std::min(255, b)); };
                        int a = (int)std::lround(f32(v + 0x1C) * 255.0f / 128.0f);
                        uint32_t col = cb(f32(v + 0x10)) | cb(f32(v + 0x14)) << 8 | cb(f32(v + 0x18)) << 16 | (uint32_t)std::max(0, std::min(255, a)) << 24;
                        w.f(q, (f32(v + 0x20) + 48.0f) / 48.0f); w.f(q + 4, (f32(v + 0x24) + 48.0f) / 48.0f); w.w(q + 8, col);
                        w.f(q + 12, x); w.f(q + 16, y); w.f(q + 20, z);
                        float c3[3] = {x, y, z};
                        for (int j = 0; j < 3; j++) { lo[j] = std::min(lo[j], c3[j]); hi[j] = std::max(hi[j], c3[j]); }
                        si.verts++;
                    }
                    vp += al(24 * st[i].second, 16);
                }
                if (ta) w.w(dl + 28 + 8 * n, 0x00F70000u);
                w.w(dl + 28 + 8 * n + ta, 0x13000000u); w.w(dl + 32 + 8 * n + ta, 0x0B000000u);
                size_t bb = dl + 36 + 8 * n + ta;
                if (!n) for (int j = 0; j < 3; j++) lo[j] = hi[j] = 0;
                for (int zi = 0; zi < 2; zi++)
                    for (int yi = 0; yi < 2; yi++)
                        for (int xi = 0; xi < 2; xi++) { w.f(bb, xi ? hi[0] : lo[0]); w.f(bb + 4, yi ? hi[1] : lo[1]); w.f(bb + 8, zi ? hi[2] : lo[2]); bb += 12; }
                groups[g].push_back({po, blk, lead + head + body, tex});
                si.strips += (int)n;
            }
        }
    }
    size_t nparts = 0;
    for (auto& g : groups) nparts += g.size();
    size_t RECS = w.put(32 * std::max<size_t>(nparts, 1));
    std::map<size_t, size_t> partmap;
    {
        size_t r = RECS;
        for (auto& g : groups)
            for (auto& p : g) {
                w.w(r + 4, (uint32_t)p.dlsz); w.w(r + 8, (uint32_t)p.dl); w.w(r + 12, p.tex < nt ? p.tex : 0); w.w(r + 20, (uint32_t)(p.dl + p.dlsz));
                partmap[p.old] = r; r += 32;
            }
    }
    size_t GT = w.put(8 * nG);
    {
        size_t r = RECS;
        for (uint32_t g = 0; g < nG; g++) {
            size_t lst = w.put(4 * std::max<size_t>(groups[g].size(), 1), 4);
            for (size_t k = 0; k < groups[g].size(); k++) { w.w(lst + 4 * k, (uint32_t)r); r += 32; }
            w.w(GT + 8 * g, (uint32_t)groups[g].size()); w.w(GT + 8 * g + 4, (uint32_t)lst);
        }
    }
    si.groups = (int)nG; si.parts = (int)nparts;
    // SP1: nodes and sub-objects (TTT order: kind, count, list, sphere, REL)
    uint32_t nN = u32(m + 0x20);
    size_t N0 = P(0x24);
    if (nN > 64 || !ok(N0, 8 * (size_t)nN)) return false;
    std::map<size_t, size_t> submap;
    std::vector<std::array<float, 4>> node0_sph;
    size_t NT = w.put(20 * std::max<uint32_t>(nN, 1), 4);
    for (uint32_t i = 0; i < nN; i++) {
        uint32_t nsub = u32(m + N0 + 8 * i);
        size_t osub = P(N0 + 8 * i + 4);
        if (nsub > 0x10000 || (nsub && !ok(osub, 32 * (size_t)nsub))) return false;
        size_t SA = w.put(32 * std::max<uint32_t>(nsub, 1), 4);
        std::vector<uint32_t> sec;
        for (uint32_t s = 0; s < nsub; s++) {
            size_t so = osub + 32 * s, ns = SA + 32 * s;
            uint32_t nref = u16(m + so), kind = u16(m + so + 2);
            size_t oref = P(so + 4), rel = P(so + 8);
            std::vector<uint32_t> refs;
            for (uint32_t r = 0; r < nref && ok(oref + 4 * r, 4); r++) { auto it = partmap.find(P(oref + 4 * r)); if (it != partmap.end()) refs.push_back((uint32_t)it->second); }
            size_t lst = w.put(4 * std::max<size_t>(refs.size(), 1), 4);
            for (size_t r = 0; r < refs.size(); r++) w.w(lst + 4 * r, refs[r]);
            sec.insert(sec.end(), refs.begin(), refs.end());
            float sp[4]; for (int j = 0; j < 4; j++) sp[j] = f32(m + so + 0x0C + 4 * j);
            w.w(ns, kind ? kind : 1); w.w(ns + 4, (uint32_t)refs.size()); w.w(ns + 8, (uint32_t)lst);
            for (int j = 0; j < 4; j++) w.f(ns + 0x0C + 4 * j, sp[j]);
            auto rt = relmap.find(rel);
            w.w(ns + 0x1C, rel && rt != relmap.end() ? (uint32_t)rt->second : 0);
            submap[so] = ns;
        }
        size_t SL = w.put(4 * std::max<size_t>(sec.size(), 1), 4);
        for (size_t r = 0; r < sec.size(); r++) w.w(SL + 4 * r, sec[r]);
        w.w(NT + 20 * i, i); w.w(NT + 20 * i + 4, nsub); w.w(NT + 20 * i + 8, (uint32_t)SA); w.w(NT + 20 * i + 12, (uint32_t)sec.size()); w.w(NT + 20 * i + 16, (uint32_t)SL);
    }
    si.nodes = (int)nN;
    // SP3 octree: BMA3 keeps the cube edge in the header, TTT keeps it per node (root edge = hdr 0x60)
    float edge = f32(m + 0x4C);
    std::vector<size_t> on;
    std::map<size_t, size_t> onew;
    size_t root = P(0x50);
    if (root && ok(root, 64)) {
        std::vector<size_t> stack{root};
        while (!stack.empty() && on.size() < 100000) {
            size_t q = stack.back(); stack.pop_back();
            if (onew.count(q) || !ok(q, 64)) continue;
            onew[q] = 0; on.push_back(q);
            for (int c = 7; c >= 0; c--) { size_t ch = P(q + 4 + 4 * c); if (ch) stack.push_back(ch); }
        }
    }
    size_t OB = w.put(64 * std::max<size_t>(on.size(), 1));
    for (size_t k = 0; k < on.size(); k++) onew[on[k]] = OB + 64 * k;
    for (size_t k = 0; k < on.size(); k++) {
        size_t q = on[k], nq = OB + 64 * k;
        for (int c = 0; c < 8; c++) { size_t ch = P(q + 4 + 4 * c); auto it = onew.find(ch); w.w(nq + 4 * c, ch && it != onew.end() ? (uint32_t)it->second : 0); }
        uint32_t depth = u32(m + q + 0x34);
        for (int j = 0; j < 3; j++) w.f(nq + 0x20 + 4 * j, f32(m + q + 0x28 + 4 * j));
        w.f(nq + 0x2C, edge / (float)(1u << std::min<uint32_t>(depth, 20)));
        w.w(nq + 0x34, depth);
        uint32_t nref = u32(m + q + 0x38);
        size_t oref = P(q + 0x3C);
        std::vector<uint32_t> refs;
        for (uint32_t r = 0; r < nref && r < 100000 && ok(oref + 4 * r, 4); r++) { auto it = submap.find(P(oref + 4 * r)); if (it != submap.end()) refs.push_back((uint32_t)it->second); }
        size_t lst = w.put(4 * std::max<size_t>(refs.size(), 1), 4);
        for (size_t r = 0; r < refs.size(); r++) w.w(lst + 4 * r, refs[r]);
        w.w(nq + 0x38, (uint32_t)refs.size()); w.w(nq + 0x3C, (uint32_t)lst);
    }
    si.octree = (int)on.size();
    // SP2: zones, collision meshes (pointers in words in both games), BVH and faces copied as they are
    uint32_t nZ = u32(m + 0x38);
    size_t Z0 = P(0x3C);
    if (nZ > 4096 || (nZ && !ok(Z0, 64 * (size_t)nZ))) return false;
    std::map<size_t, size_t> meshmap;
    auto mesh = [&](size_t old) -> size_t {
        if (!old || !ok(old, 48)) return 0;
        auto it = meshmap.find(old);
        if (it != meshmap.end()) return it->second;
        uint32_t nb = u32(m + old), nf = u32(m + old + 4);
        size_t pb = P(old + 0x0C), pf = P(old + 0x10), pv = P(old + 0x14);
        if (nb > 0x40000 || nf > 0x20000 || !ok(pb, 32 * (size_t)nb) || !ok(pf, 32 * (size_t)nf)) return 0;
        uint32_t nv = 0;
        for (uint32_t k = 0; k < nf; k++) for (int j = 0; j < 3; j++) nv = std::max(nv, u32(m + pf + 32 * k + 4 + 4 * j) + 1);
        if (!nf || nv > 3 * nf || !ok(pv, 16 * (size_t)nv)) return 0;
        size_t h = w.put(48), b = w.put(32 * (size_t)nb), fc = w.put(32 * (size_t)nf), vv = w.put(16 * (size_t)nv);
        memcpy(&w.o[b], m + pb, 32 * (size_t)nb); memcpy(&w.o[fc], m + pf, 32 * (size_t)nf); memcpy(&w.o[vv], m + pv, 16 * (size_t)nv);
        w.w(h, nb); w.w(h + 4, nf); w.w(h + 0x0C, (uint32_t)(b >> 2)); w.w(h + 0x10, (uint32_t)(fc >> 2)); w.w(h + 0x14, (uint32_t)(vv >> 2));
        memcpy(&w.o[h + 0x18], m + old + 0x18, 24);
        meshmap[old] = h; si.meshes++;
        return h;
    };
    size_t ZT = w.put(64 * std::max<uint32_t>(nZ, 1));
    for (uint32_t i = 0; i < nZ; i++) {
        size_t e = Z0 + 64 * i, ne = ZT + 64 * i;
        uint32_t nn = u32(m + e + 8), nsub = u32(m + e + 0x14);
        size_t onn = P(e + 0x0C), osub = P(e + 0x18);
        memcpy(&w.o[ne + 0x1C], m + e + 0x1C, 36);
        w.w(ne + 4, (uint32_t)mesh(P(e + 4)));
        size_t NL = w.put(4 * std::max<uint32_t>(nn, 1), 4);
        if (nn <= 8 && ok(onn, 4 * (size_t)nn)) { memcpy(&w.o[NL], m + onn, 4 * (size_t)nn); w.w(ne + 8, nn); }
        w.w(ne + 0x0C, (uint32_t)NL);
        if (nsub && nsub < 256 && ok(osub, 16 * (size_t)nsub)) {
            size_t SB = w.put(16 * (size_t)nsub);
            for (uint32_t s = 0; s < nsub; s++) { w.w(SB + 16 * s, u32(m + osub + 16 * s)); w.w(SB + 16 * s + 4, u32(m + osub + 16 * s + 4)); w.w(SB + 16 * s + 8, (uint32_t)mesh(P(osub + 16 * s + 8))); }
            w.w(ne + 0x14, nsub); w.w(ne + 0x18, (uint32_t)SB);
        }
    }
    si.zones = (int)nZ;
    // EX: 6 state slots and the fragment list point to sub-objects; duration 60 like the REL tracks
    uint32_t nE = u32(m + 0x44);
    size_t E0 = P(0x48);
    if (nE > 1024 || (nE && !ok(E0, 80 * (size_t)nE))) return false;
    size_t ET = w.put(80 * std::max<uint32_t>(nE, 1));
    si.ex_pos.assign(nE, {0, 0, 0});
    for (uint32_t i = 0; i < nE; i++) {
        size_t e = E0 + 80 * i, ne = ET + 80 * i;
        w.w(ne + 8, u32(m + e + 8));
        bool got = false;
        for (int k = 0; k < 6; k++) {
            size_t so = P(e + 0x0C + 4 * k);
            auto it = so ? submap.find(so) : submap.end();
            w.w(ne + 0x0C + 4 * k, it != submap.end() ? (uint32_t)it->second : 0);
            if (it != submap.end() && !got && ok(so, 32)) { si.ex_pos[i] = {f32(m + so + 0x10), -f32(m + so + 0x14), -f32(m + so + 0x18)}; got = true; }
        }
        uint32_t nref = u32(m + e + 0x24);
        size_t oref = P(e + 0x28);
        std::vector<uint32_t> refs;
        for (uint32_t r = 0; r < nref && r < 4096 && ok(oref + 4 * r, 4); r++) { auto it = submap.find(P(oref + 4 * r)); if (it != submap.end()) refs.push_back((uint32_t)it->second); }
        size_t lst = w.put(4 * std::max<size_t>(refs.size(), 1), 4);
        for (size_t r = 0; r < refs.size(); r++) w.w(lst + 4 * r, refs[r]);
        uint32_t dur = u32(m + e + 0x2C);
        w.w(ne + 0x24, (uint32_t)refs.size()); w.w(ne + 0x28, (uint32_t)lst); w.w(ne + 0x2C, dur ? dur : 60);
    }
    si.ex = (int)nE;
    size_t LIM = w.put(16);
    if (ok(P(0x40), 12)) memcpy(&w.o[LIM], m + P(0x40), 12);
    w.put(0, 32);
    static const uint8_t MAGIC[4] = {0x60, 0x0B, 0x9A, 0x7E};
    memcpy(&w.o[0], MAGIC, 4);
    w.w(4, 1); w.w(8, (uint32_t)nparts); w.w(0x0C, (uint32_t)RECS); w.w(0x10, nN); w.w(0x14, (uint32_t)NT); w.w(0x18, nt); w.w(0x1C, (uint32_t)A0);
    w.w(0x20, nG); w.w(0x24, (uint32_t)GT); w.w(0x28, on.empty() ? 0 : (uint32_t)OB); w.w(0x2C, (uint32_t)T0); w.w(0x38, 0xFFFFFFFFu);
    w.w(0x44, nR); w.w(0x48, (uint32_t)RT); w.w(0x4C, nZ); w.w(0x50, (uint32_t)ZT); w.w(0x54, (uint32_t)LIM); w.w(0x58, nE); w.w(0x5C, (uint32_t)ET); w.f(0x60, edge);
    res.swap(w.o);
    return true;
}

// scroll sheet (BT3 EftStageScroll, PCK2 entries 5 textures and 6 {size, ?, speed, angle} + DBES mesh): the port draws it after the sky
// (GE marker F0) with uv * size + scroll offset. Block: 'BSCR', groups, size, speed per 30 Hz step, dir x/y, then per texture
// {pixels, clut, vertices, count} (offsets from the block) with GE float vertices (uv, colour 8888, position in map units)
template <class F>
static bool stage_scroll(const uint8_t* t5, size_t n5, const uint8_t* d6, size_t n6, F tex_id, std::vector<uint8_t>& o, StageInfo& si) {
    o.clear();
    if (!t5 || !d6 || n5 < 0x20 || u32(t5 + 4) != 8 || n6 < 16 + 16) return false;
    uint32_t ntex = u32(t5);
    const uint8_t* m = d6 + 16;
    size_t mn = n6 - 16;
    if (ntex == 0 || ntex > 32 || memcmp(m, "DBES", 4)) return false;
    uint32_t ng = u32(m + 4), to = u32(m + 8), vo = u32(m + 12);
    if (ng > 64 || 16 + 16 * (size_t)ng > mn || to > mn || vo > mn || to > vo) return false;
    uint32_t nv = (uint32_t)((mn - vo) / 48);
    float size = (float)(int32_t)u32(d6), speed = (float)(int32_t)u32(d6 + 8) * 0.00001f * size, ang = (float)(int32_t)u32(d6 + 12);
    if (ang < 0) { static std::mt19937 rng(std::random_device{}()); ang = std::uniform_real_distribution<float>(0.0f, 360.0f)(rng); }
    float rad = ang * 3.14159265f / 180.0f;
    std::vector<std::vector<uint8_t>> vt(ntex);
    auto cb = [](float c, float k) { return (uint32_t)std::max(0L, std::min(255L, std::lround(c * k))); };
    for (uint32_t g = 0; g < ng; g++) {
        uint32_t cnt = u32(m + 16 + 16 * g), first = u32(m + 16 + 16 * g + 8);
        for (uint32_t i = 0; i < cnt; i++) {
            size_t ta = to + 16 * (size_t)(first + i);
            if (ta + 16 > vo) return false;
            int32_t tx = (int32_t)u32(m + ta + 12);
            if (tx < 0 || (uint32_t)tx >= ntex) continue;
            for (int c = 0; c < 3; c++) {
                uint32_t vi = u32(m + ta + 4 * c);
                if (vi >= nv) return false;
                const uint8_t* v = m + vo + 48 * (size_t)vi;
                uint8_t q[24];
                float uv[2] = {f32(v + 0x20) * size, f32(v + 0x24) * size}, pos[3] = {f32(v) / 16384.0f, f32(v + 4) / 8192.0f, f32(v + 8) / 16384.0f};
                uint32_t col = cb(f32(v + 0x10), 1) | cb(f32(v + 0x14), 1) << 8 | cb(f32(v + 0x18), 1) << 16 | cb(f32(v + 0x1C), 255.0f / 128.0f) << 24;
                memcpy(q, uv, 8); memcpy(q + 8, &col, 4); memcpy(q + 12, pos, 12);
                vt[tx].insert(vt[tx].end(), q, q + 24);
            }
            si.scroll_tris++;
        }
    }
    if (!si.scroll_tris) return false;
    Out w;
    w.put(32 + 16 * (size_t)ntex);
    memcpy(&w.o[0], "BSCR", 4);
    w.w(4, ntex); w.f(8, size); w.f(12, speed); w.f(16, std::cos(rad)); w.f(20, std::sin(rad));
    for (uint32_t i = 0; i < ntex; i++) {
        size_t px = w.put(256), cl = w.put(1024), vb = w.put(vt[i].size());
        tex_marker(&w.o[px], tex_id(SCROLL_TEX + (int)i));
        if (!vt[i].empty()) memcpy(&w.o[vb], vt[i].data(), vt[i].size());
        size_t e = 32 + 16 * i;
        w.w(e, (uint32_t)px); w.w(e + 4, (uint32_t)cl); w.w(e + 8, (uint32_t)vb); w.w(e + 12, (uint32_t)(vt[i].size() / 24));
    }
    w.put(0, 16);
    o.swap(w.o);
    si.scroll_tex = (int)ntex; si.scroll[0] = size; si.scroll[1] = speed; si.scroll[2] = ang; si.scroll[3] = (float)nv;
    return true;
}

// 'BRFL': frames, w, h, colorA[4], colorB[4], lightDir[3], power, animSpeed, fadeDist (EftSurfParam, MAPD +0x58 record over it),
// then per frame w*h indices (rows as the port serves BT3 images) + 256 x 3 floats: the unit normal of each logical palette entry
static bool surf_reflect(const uint8_t* d12, size_t n12, const uint8_t* mapd, size_t nm, const uint8_t* t, size_t n, std::vector<uint8_t>& o) {
    o.clear();
    if (!t || n < 0x20 || u32(t + 4) != 8 || n12 < 0x40) return false;
    uint32_t cnt = u32(t);
    if (cnt < 5 || cnt > 256 || 32 + 64 * (size_t)cnt > n) return false;
    uint32_t frames = cnt - 4;
    float P[17];
    for (int i = 0; i < 12; i++) P[i] = f32(d12 + 4 * i);
    P[12] = f32(d12 + 0x30); P[13] = (float)(int32_t)u32(d12 + 0x34); P[14] = f32(d12 + 0x38);
    if (mapd && nm >= 0x60 && !memcmp(mapd, "MAPD", 4)) {
        uint32_t c = u32(mapd + 0x58), ro = u32(mapd + 0x5C);
        if (c && ro && ro + 0x1F <= nm) {
            const uint8_t* r = mapd + ro;
            for (int i = 0; i < 4; i++) { P[i] = r[0x17 + i]; P[4 + i] = r[0x1B + i]; }
            P[8] = f32(r); P[9] = f32(r + 4); P[10] = f32(r + 8); P[12] = f32(r + 12); P[13] = (float)u16(r + 0x14); P[14] = f32(r + 0x10);
        }
    }
    if (P[14] == 0.0f) P[14] = 1000.0f;
    int W = 0, H = 0;
    std::vector<uint8_t> all;
    for (uint32_t k = 0; k < frames; k++) {
        const uint8_t* e = t + 32 + 64 * (size_t)k;
        uint32_t tp = u32(e) * 4 + 96, pp = u32(e + 4) * 4 + 96, tl = u32(e + 8), pl = u32(e + 12);
        uint64_t tex0; memcpy(&tex0, e + 0x30, 8);
        int w = 1 << ((tex0 >> 26) & 0xF), h = 1 << ((tex0 >> 30) & 0xF);
        if (((tex0 >> 20) & 0x3F) != 19 || tl <= 128 || pl != 1024 + 128 || tp + tl - 128 > n || pp + 1024 > n) return false;
        if (k == 0) { W = w; H = h; } else if (w != W || h != H) return false;
        std::vector<uint8_t> raw(t + tp, t + tp + tl - 128), pal(t + pp, t + pp + 1024), ids(1024);
        for (int i = 0; i < 256; i++) ids[4 * i] = (uint8_t)i;
        std::vector<uint8_t> sidx = bmp_order(raw, w, h), lg = reorder_pal(ids);
        if (sidx.size() < (size_t)w * h || lg.size() < 1024) return false;
        size_t at = all.size();
        all.resize(at + (size_t)w * h + 256 * 12);
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) all[at + (size_t)(h - 1 - y) * w + x] = sidx[(size_t)y * w + x];
        for (int j = 0; j < 256; j++) {
            const uint8_t* c = &pal[4 * lg[4 * j]];
            float v[3] = {c[0] - 128.0f, c[1] - 128.0f, c[2] - 128.0f}, l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            for (float& x : v) x = l > 0 ? x / l : 0.0f;
            memcpy(&all[at + (size_t)w * h + 12 * j], v, 12);
        }
    }
    Out w;
    w.put(16 + 4 * 15);
    memcpy(&w.o[0], "BRFL", 4); w.w(4, frames); w.w(8, (uint32_t)W); w.w(12, (uint32_t)H);
    for (int i = 0; i < 15; i++) w.f(16 + 4 * i, P[i]);
    size_t a = w.put(all.size());
    memcpy(&w.o[a], all.data(), all.size());
    w.put(0, 16);
    o.swap(w.o);
    return true;
}

// animated surfaces (BT3 EftSurf, entry 12): per mesh its scroll and flags; the port animates and draws them after the fighters (GE marker F5),
// sorted like BT3's ordering table. Block: 'BSRF', meshes, textures, water y, has water, work offset, then textures {pixels, clut} and
// meshes {tex, tris, source, -, du, dv, u, v, zBias, flags, centre xyz}; source vertex = GE position xyz, st, colour rgba (BT3 scale)
template <class F>
static bool stage_surf(const uint8_t* d12, size_t n12, const uint8_t* mapd, size_t nm, const uint8_t* d9, size_t n9, F tex_id, std::vector<uint8_t>& o, StageInfo& si) {
    o.clear();
    size_t fn;
    const uint8_t* f = d12 ? surf_file(d12, n12, &fn) : nullptr;
    if (!f) return false;
    uint32_t nt = u32(f + 0x10), nmesh = u32(f + 0x18), to = u32(f + 0x14), mo = u32(f + 0x1C);
    if (!nt || !nmesh) return false;
    float wy = 0; uint32_t hw = 0;
    if (mapd && nm >= 0x18 && !memcmp(mapd, "MAPD", 4)) { uint32_t io = u32(mapd + 0x14); if (io && io + 16 <= nm && (u32(mapd + io + 4) & 2)) { hw = 1; wy = -f32(mapd + io + 8); } }
    Out w;
    w.put(32 + 8 * (size_t)nt + 64 * (size_t)nmesh);
    memcpy(&w.o[0], "BSRF", 4);
    w.w(4, nmesh); w.w(8, nt); w.f(12, wy); w.w(16, hw);
    for (uint32_t i = 0; i < nt; i++) {
        size_t px = w.put(256), cl = w.put(1024);
        tex_marker(&w.o[px], tex_id(SURF_TEX + (int)i));
        w.w(32 + 8 * i, (uint32_t)px); w.w(36 + 8 * i, (uint32_t)cl);
    }
    uint32_t total = 0;
    for (uint32_t i = 0; i < nmesh; i++) {
        const uint8_t* m = f + mo + 96 * (size_t)i;
        uint32_t tex = u32(m), cnt = u32(m + 4), tri = u32(m + 8), fl = u32(m + 0x20);
        size_t e = 32 + 8 * (size_t)nt + 64 * (size_t)i;
        if (tex >= nt || cnt > 0x10000 || (size_t)tri + 144 * (size_t)cnt > fn) { w.w(e + 4, 0); continue; }
        if (fl & 0x800) si.surf_skipped++;
        size_t src = w.put(36 * 3 * (size_t)cnt);
        for (uint32_t k = 0; k < 3 * cnt; k++) {
            const uint8_t* v = f + tri + 48 * (size_t)k;
            float q[9] = {f32(v) / 16384.0f, f32(v + 4) / 8192.0f, f32(v + 8) / 16384.0f, f32(v + 0x10), f32(v + 0x14), f32(v + 0x20), f32(v + 0x24), f32(v + 0x28), f32(v + 0x2C)};
            memcpy(&w.o[src + 36 * k], q, 36);
        }
        w.w(e, tex); w.w(e + 4, cnt); w.w(e + 8, (uint32_t)src);
        memcpy(&w.o[e + 16], m + 0x0C, 16);
        memcpy(&w.o[e + 32], m + 0x1C, 8);
        memcpy(&w.o[e + 40], m + 0x50, 12);
        total += cnt; si.surf_meshes++;
    }
    if (!total) return false;
    w.w(20, 0); w.w(24, total);
    if (si.surf_skipped) {   // reflecting surface (EftSurfRt): normal map frames of entry 9 (indices + their palette of normals), lit per frame
        std::vector<uint8_t> rf;
        if (surf_reflect(d12, n12, mapd, nm, d9, n9, rf)) { size_t r = w.put(rf.size()); memcpy(&w.o[r], rf.data(), rf.size()); w.w(28, (uint32_t)r); si.surf_reflect = (int)u32(rf.data() + 4); }
    }
    w.put(0, 16);
    o.swap(w.o);
    si.surf_tex = (int)nt; si.surf_tris = (int)total;
    return true;
}

// StgModel_DrawAnims data (entry 8 + MAPD objects with anim > 0): node lists with their tracks and VIF batches (VU1 prog 8 vertices
// x y z weight, rgba, s t). Block: 'BOBJ', anims, objects, textures, anim table, object table, texture table; anim {length, nodes, at};
// node {pop, last, batches, at, keys, at, -, -, pos4, rot4, pivotA4, pivotB4}; batch {tex, verts, at, prim}; object {anim, type, pos3, rot3}
template <class F>
static bool stage_objs(const uint8_t* d8, size_t n8, const uint8_t* mapd, size_t nm, F tex_id, std::vector<uint8_t>& o, StageInfo& si) {
    o.clear();
    std::vector<std::pair<uint32_t, uint32_t>> t;
    if (!d8 || !ofs_table(d8, n8, t) || !mapd || nm < 0x18 || memcmp(mapd, "MAPD", 4)) return false;
    uint32_t na = (uint32_t)t.size() / 3, nob = u32(mapd + 8), ob = u32(mapd + 12);
    if (nob > 4096 || (size_t)ob + 64 * (size_t)nob > nm) return false;
    std::vector<uint32_t> objs;
    for (uint32_t i = 0; i < nob; i++) { int32_t a = (int32_t)u32(mapd + ob + 64 * i + 8); if (a > 0 && (uint32_t)a <= na) objs.push_back(i); }
    if (objs.empty()) return false;
    Out w;
    w.put(32);
    memcpy(&w.o[0], "BOBJ", 4);
    std::vector<uint32_t> tbase(na), tcnt(na);
    uint32_t ntex = 0;
    for (uint32_t a = 0; a < na; a++) { tbase[a] = ntex; uint32_t c = t[3 * a].second >= 0x20 ? u32(d8 + t[3 * a].first) : 0; tcnt[a] = c <= 32 && 0x20 + 64 * (size_t)c <= t[3 * a].second ? c : 0; ntex += tcnt[a]; }
    if (ntex > 32) return false;
    size_t tt = w.put(8 * (size_t)ntex);
    for (uint32_t i = 0; i < ntex; i++) {
        size_t px = w.put(256), cl = w.put(1024);
        tex_marker(&w.o[px], tex_id(OBJ_TEX + (int)i));
        w.w(tt + 8 * i, (uint32_t)px); w.w(tt + 8 * i + 4, (uint32_t)cl);
    }
    size_t at = w.put(16 * (size_t)na);
    uint32_t verts = 0;
    for (uint32_t a = 0; a < na; a++) {
        const uint8_t* tf = d8 + t[3 * a].first;
        const uint8_t* nb = d8 + t[3 * a + 1].first; size_t nn = t[3 * a + 1].second;
        const uint8_t* tr = d8 + t[3 * a + 2].first; size_t tn = t[3 * a + 2].second;
        if (nn < 0x10 + 0xF0 || tn < 0x10) continue;
        size_t q = u32(nb) / 4 * 4, k = 0;
        std::vector<size_t> nodes;
        while (q + 0xF0 <= nn && nodes.size() < 256) { nodes.push_back(q); uint32_t sz = u32(nb + q); if (u32(nb + q + 8) || sz < 0xF0) break; q += sz; }
        size_t nt0 = w.put(96 * nodes.size());
        w.w(at + 16 * a, tn >= 0x10 ? (uint32_t)(tr[0xC] | tr[0xD] << 8) : 0); w.w(at + 16 * a + 4, (uint32_t)nodes.size()); w.w(at + 16 * a + 8, (uint32_t)nt0);
        size_t tp = 0;
        for (size_t ni = 0; ni < nodes.size(); ni++, k++) {
            const uint8_t* nd = nb + nodes[ni];
            size_t e = nt0 + 96 * ni, nend = std::min(nn, nodes[ni] + (size_t)u32(nd));
            w.w(e, u32(nd + 4)); w.w(e + 4, ni + 1 == nodes.size() ? 1 : 0);
            memcpy(&w.o[e + 32], nd + 0x10, 64);
            if (tp + 0x10 <= tn) {
                uint32_t kc = (uint32_t)(tr[tp + 0xA] | tr[tp + 0xB] << 8);
                if (tp + 0x10 + 32 * (size_t)kc <= tn) {
                    size_t ko = w.put(32 * (size_t)kc);
                    memcpy(&w.o[ko], tr + tp + 0x10, 32 * (size_t)kc);
                    w.w(e + 16, kc); w.w(e + 20, (uint32_t)ko);
                }
                tp += 0x10 + 32 * (size_t)kc;
            }
            if (!u32(nd + 0xC)) continue;
            std::vector<uint32_t> vw;   // VIF stream of the DMA chain (cnt / ret / end tags)
            for (size_t c = nodes[ni] + 0xF0; c + 16 <= nend;) {
                uint32_t tg = u32(nb + c), qwc = tg & 0xFFFF, id = (tg >> 28) & 7;
                if (id != 1 && id != 6 && id != 7) break;
                vw.push_back(u32(nb + c + 8)); vw.push_back(u32(nb + c + 12));
                for (size_t i = 0; i < 4 * (size_t)qwc && c + 16 + 4 * i + 4 <= nend; i++) vw.push_back(u32(nb + c + 16 + 4 * i));
                c += 16 + 16 * (size_t)qwc;
                if (id != 1) break;
            }
            std::vector<uint32_t> mem(4 * 512, 0);
            std::vector<std::array<uint32_t, 4>> bl;
            std::vector<float> bv;
            for (size_t i = 0; i < vw.size();) {
                uint32_t c = vw[i], cmd = (c >> 24) & 0x7F;
                if (cmd >= 0x60) {
                    uint32_t num = (c >> 16) & 0xFF ? (c >> 16) & 0xFF : 256, vn = (cmd >> 2) & 3, vl = cmd & 3, sz = ((32 >> vl) * (vn + 1) * num + 31) / 32, ad = c & 0x3FF;
                    if (cmd == 0x6C) for (uint32_t j = 0; j < 4 * num && i + 1 + j < vw.size() && 4 * ad + j < mem.size(); j++) mem[4 * ad + j] = vw[i + 1 + j];
                    i += 1 + sz;
                } else if (cmd == 0x17 || cmd == 0x14 || cmd == 0x15) {   // MSCNT: prog 8 runs one batch
                    uint32_t nv = mem[8] & 0x7FFF, prim = (mem[9] >> 15) & 0x7FF, tb = mem[4] & 0x3FFF, tx = 0xFFFFFFFFu;
                    if (nv && 12 + 12 * (size_t)nv <= mem.size()) {
                        for (uint32_t j = 0; j < tcnt[a]; j++) if (u32(tf + u32(tf + 4) * 4 + 64 * j + 0x20) == tb) { tx = tbase[a] + j; break; }
                        if (tx == 0xFFFFFFFFu && tcnt[a]) tx = tbase[a];
                        bl.push_back({tx, nv, (uint32_t)bv.size(), prim});
                        for (uint32_t j = 0; j < nv; j++) {
                            const uint32_t* v = &mem[12 + 12 * j];
                            float f[12]; memcpy(f, v, 48);
                            float r[10] = {f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]};
                            bv.insert(bv.end(), r, r + 10);
                        }
                        verts += nv;
                    }
                    i++;
                } else if (cmd == 0x20) i += 2;
                else if (cmd == 0x30 || cmd == 0x31) i += 5;
                else if (cmd == 0x4A) i += 1 + 2 * ((c >> 16) & 0xFF ? (c >> 16) & 0xFF : 256);
                else if (cmd == 0x50 || cmd == 0x51) i += 1 + 4 * (size_t)(c & 0xFFFF);
                else i++;
            }
            if (bl.empty()) continue;
            size_t bo = w.put(16 * bl.size()), vo = w.put(4 * bv.size());
            memcpy(&w.o[vo], bv.data(), 4 * bv.size());
            for (size_t j = 0; j < bl.size(); j++) { w.w(bo + 16 * j, bl[j][0]); w.w(bo + 16 * j + 4, bl[j][1]); w.w(bo + 16 * j + 8, (uint32_t)(vo + 40 * (size_t)bl[j][2] / 10)); w.w(bo + 16 * j + 12, bl[j][3]); }
            w.w(e + 8, (uint32_t)bl.size()); w.w(e + 12, (uint32_t)bo);
        }
    }
    size_t oo = w.put(32 * objs.size());
    for (size_t i = 0; i < objs.size(); i++) {
        const uint8_t* r = mapd + ob + 64 * (size_t)objs[i];
        float v[6] = {f32(r + 0x20), -f32(r + 0x24), -f32(r + 0x28), f32(r + 0x30), f32(r + 0x34), f32(r + 0x38)};
        w.w(oo + 32 * i, u32(r + 8)); w.w(oo + 32 * i + 4, u32(r + 4));
        for (int j = 0; j < 6; j++) w.f(oo + 32 * i + 8 + 4 * j, v[j]);
    }
    w.w(4, na); w.w(8, (uint32_t)objs.size()); w.w(12, ntex); w.w(16, (uint32_t)at); w.w(20, (uint32_t)oo); w.w(24, (uint32_t)tt);
    w.put(0, 16);
    o.swap(w.o);
    si.obj_anims = (int)na; si.obj_count = (int)objs.size(); si.obj_tex = (int)ntex; si.obj_verts = (int)verts;
    return true;
}

// stage particles (EftWeather kind 2, EftSmoke 4, EftSteam 5, EftGeyser 6): MAPD weather record (+0x60) with entry 10's images, MEF0 lists
// (entry 18) gated by the MAPD stage flags (+0x70: 1 geysers, 2 smoke, 4 steam) with entry 17's images. Block: 'BPTL', weather images,
// effect images, texture table, weather at, geysers, at, smoke sources, at, steam sources, at, light B xyz; records y/z negated like BT3
template <class F>
static bool stage_ptcl(const std::vector<std::pair<uint32_t, uint32_t>>& e, const uint8_t* d, F tex_id, std::vector<uint8_t>& o, StageInfo& si, bool* wthr, bool* mef, bool* wtr, bool* stm, const StageFx* fx = nullptr) {
    o.clear(); *wthr = *mef = *wtr = *stm = false;
    if (const char* v = getenv("TTT_BT3_PTCL")) if (*v == '0') return false;
    if (e.size() < 19 || !e[1].second) return false;
    const uint8_t* m = d + e[1].first; size_t nm = e[1].second;
    if (nm < 0x80 || memcmp(m, "MAPD", 4)) return false;
    auto sec = [&](int at, uint32_t len) -> const uint8_t* { uint32_t c = u32(m + at), of = u32(m + at + 4); return (int32_t)c > 0 && of && (size_t)of + len <= nm ? m + of : nullptr; };
    auto ctex = [&](size_t en) { return en < e.size() && e[en].second >= 0x20 && u32(d + e[en].first + 4) == 8 ? std::min<uint32_t>(u32(d + e[en].first), 32) : 0u; };
    const uint8_t* wr = sec(0x60, 0x18);
    uint32_t nw = wr && (u32(wr + 0x10) & 1) ? ctex(10) : 0;
    const uint8_t* fl = sec(0x70, 4);
    auto fcnt = [&](size_t en) { return fx && fx->d && en < fx->e.size() && fx->e[en].second >= 0x20 && u32(fx->d + fx->e[en].first + 4) == 8 ? std::min<uint32_t>(u32(fx->d + fx->e[en].first), 32) : 0u; };
    uint32_t sf = fl ? u32(fl) : 0, ne = ctex(17), nem = ne;
    if (fx && fx->bub == 1 && fcnt(17) > ne) ne = fcnt(17);   // images from nem on come from the mod's .fx.bin
    uint32_t lc[3] = {0, 0, 0}, lo[3] = {0, 0, 0};
    const uint8_t* mf = e[18].second >= 0x20 ? d + e[18].first : nullptr;
    if (mf && ne && !memcmp(mf, "MEF0", 4))
        for (int l = 0; l < 3; l++) {
            uint32_t c = u32(mf + 8 + 8 * l), of = u32(mf + 12 + 8 * l);
            if ((sf >> l & 1) && c && c < 256 && (size_t)of + 0x30 * (size_t)c <= e[18].second) { lc[l] = c; lo[l] = of; }
        }
    const uint8_t* inf = sec(0x10, 0x10), *wr9 = sec(0x90, 4);   // EftWater (kind 7): MAPD +0x90 bit 0 and entry 17; level = stage info water y (negated like every y)
    bool water = ne && wr9 && (u32(wr9) & 1) && inf && (u32(inf + 4) & 2);
    if (!lc[0] && !lc[1] && !lc[2] && !water) ne = 0;
    uint32_t ns = ctex(13);   // EftStorm (kind 3): entry 13 present
    bool sfx = false;
    if (fx && fx->storm == 0) ns = 0;
    else if (fx && fx->storm == 1 && !ns && fcnt(13)) { ns = fcnt(13); sfx = true; }
    si.ptcl_fx = (fx && fx->storm == 0 ? 4 : 0) | (fx && fx->bub == 0 ? 8 : 0);
    if (!nw && !ne && !ns) return false;
    Out w;
    w.put(80);
    memcpy(&w.o[0], "BPTL", 4);
    w.w(4, nw); w.w(8, ne); w.w(64, ns); w.w(68, fx && fx->bub == 0 ? 1u : 0u);   // +68 bit 0: bubbles off by the mod
    size_t tt = w.put(8 * (size_t)(nw + ne + ns));
    for (uint32_t i = 0; i < nw + ne + ns; i++) {
        size_t px = w.put(256), cl = w.put(1024);
        tex_marker(&w.o[px], tex_id(PTCL_TEX + (int)(i < nw ? i : i < nw + ne ? 0x20 + i - nw : 0x40 + i - nw - ne)));
        w.w(tt + 8 * i, (uint32_t)px); w.w(tt + 8 * i + 4, (uint32_t)cl);
    }
    w.w(12, (uint32_t)tt);
    if (nw) {
        size_t r = w.put(32);
        memcpy(&w.o[r], wr, 0x18);
        w.f(r + 4, -f32(wr + 4)); w.f(r + 8, -f32(wr + 8));
        w.w(16, (uint32_t)r);
    }
    for (int l = 0; l < 3; l++) {
        if (!lc[l]) continue;
        size_t r = w.put(0x30 * (size_t)lc[l]);
        memcpy(&w.o[r], mf + lo[l], 0x30 * (size_t)lc[l]);
        for (uint32_t i = 0; i < lc[l]; i++) { w.f(r + 0x30 * i + 4, -f32(mf + lo[l] + 0x30 * i + 4)); w.f(r + 0x30 * i + 8, -f32(mf + lo[l] + 0x30 * i + 8)); }
        static const int AT[3] = {20, 28, 36};
        w.w(AT[l], lc[l]); w.w(AT[l] + 4, (uint32_t)r);
    }
    if (const uint8_t* lt = sec(0x18, 0x20)) { w.f(44, f32(lt + 0x10)); w.f(48, -f32(lt + 0x14)); w.f(52, -f32(lt + 0x18)); }
    if (water) { w.f(56, -f32(inf + 8)); w.w(60, 1u | (u32(inf) + 1u & 0xFFu) << 8); }   // +60: 1 | (stage id + 1) << 8
    w.put(0, 16);
    o.swap(w.o);
    si.ptcl_wtex = (int)nw; si.ptcl_etex = (int)ne; si.ptcl_stex = (int)ns;
    si.ptcl_src.assign(nw + ne + ns, 0);
    for (uint32_t i = nem; i < ne; i++) si.ptcl_src[nw + i] = 1;
    if (sfx) for (uint32_t i = 0; i < ns; i++) si.ptcl_src[nw + ne + i] = 1;
    si.ptcl_fx = (ne > nem ? 1 : 0) | (sfx ? 2 : 0) | (fx && fx->storm == 0 ? 4 : 0) | (fx && fx->bub == 0 ? 8 : 0); si.ptcl_weather = nw ? (int)std::min<uint32_t>(u32(wr + 0x14) >> 16, 30) : 0;
    si.ptcl_geysers = (int)lc[0]; si.ptcl_smoke = (int)lc[1]; si.ptcl_steam = (int)lc[2];
    *wthr = nw != 0; *mef = lc[0] || lc[1] || lc[2]; *wtr = water; *stm = ns != 0; si.ptcl_water = water ? 1 : 0;
    return true;
}

// TTT map with a mod's .fx (item 65): layer directory with a BPTL only (bubbles images of entry 17, storm images of entry 13), +68 bit 1 =
// TTT map (water level and camera from the port, no BT3 splashes or water sounds)
template <class F>
static bool fx_layers(uint32_t ne, uint32_t ns, F tex_id, std::vector<uint8_t>& lay) {
    lay.clear();
    if (!ne && !ns) return false;
    Out w;
    w.put(80);
    memcpy(&w.o[0], "BPTL", 4);
    w.w(8, ne); w.w(64, ns); w.w(68, 2u); w.w(60, ne ? 1u : 0u);
    size_t tt = w.put(8 * (size_t)(ne + ns));
    for (uint32_t i = 0; i < ne + ns; i++) {
        size_t px = w.put(256), cl = w.put(1024);
        tex_marker(&w.o[px], tex_id(PTCL_TEX + (int)(i < ne ? 0x20 + i : 0x40 + i - ne)));
        w.w(tt + 8 * i, (uint32_t)px); w.w(tt + 8 * i + 4, (uint32_t)cl);
    }
    w.w(12, (uint32_t)tt);
    w.put(0, 16);
    lay.assign(32, 0);
    memcpy(&lay[0], "BLAY", 4);
    uint32_t o = 32; memcpy(&lay[20], &o, 4);
    lay.insert(lay.end(), w.o.begin(), w.o.end());
    return true;
}

// MAPD sized for TTT: 6 + 3 + 1 start points (TTT's engine tables for the last two), stage id and change target 0,
// destructibles placed at their first state when BT3 animates them
static bool stage_mapd(const uint8_t* d, size_t n, const StageInfo& si, std::vector<uint8_t>& o, bool no_obj = false) {
    if (n < 0xB0 || memcmp(d, "MAPD", 4)) return false;
    uint32_t nb = no_obj ? 0 : u32(d + 8), ob = u32(d + 12);
    if (ob < 0x18 || ob > n || ((ob >> 3) < 3)) return false;
    uint32_t na = (ob >> 3) - 3;
    if (na > 64) return false;
    std::vector<std::pair<uint32_t, uint32_t>> sec(na);   // id/count, offset
    std::vector<uint32_t> starts;
    for (uint32_t k = 0; k < na; k++) { sec[k] = {u32(d + 0x10 + 8 * k), u32(d + 0x14 + 8 * k)}; if (sec[k].second) starts.push_back(sec[k].second); }
    starts.push_back((uint32_t)n);
    std::sort(starts.begin(), starts.end());
    auto size_of = [&](uint32_t off) { auto it = std::upper_bound(starts.begin(), starts.end(), off); return it == starts.end() ? 0u : *it - off; };
    std::vector<uint8_t> datab(d + ob, d + std::min<size_t>(n, ob + 64 * (size_t)nb));
    datab.resize(64 * (size_t)nb, 0);
    for (uint32_t i = 0; i < nb; i++) {
        uint8_t* r = &datab[64 * i];
        uint32_t ty = u32(r + 4) & ~0x40000000u;
        memcpy(r + 4, &ty, 4);
        if (!u32(r + 8)) { uint32_t mm = 0xFFFFFFFFu; memcpy(r + 8, &mm, 4); }
        uint32_t ex = u32(r + 16);
        if (f32(r + 0x20) == 0 && f32(r + 0x24) == 0 && f32(r + 0x28) == 0 && ex < si.ex_pos.size())
            for (int j = 0; j < 3; j++) memcpy(r + 0x20 + 4 * j, &si.ex_pos[ex][j], 4);
    }
    auto rec8 = [](std::vector<uint8_t>& v, const float* f) { size_t at = v.size(); v.resize(at + 32); memcpy(&v[at], f, 32); };
    std::vector<std::vector<uint8_t>> body(na);
    for (uint32_t k = 0; k < na; k++) if (sec[k].second) body[k].assign(d + sec[k].second, d + sec[k].second + size_of(sec[k].second));
    if (na > 0 && body[0].size() >= 16) { memset(&body[0][0], 0, 4); memset(&body[0][12], 0, 4); }
    if (na > 3 && sec[3].first >= 1 && body[3].size() >= 64) {   // start points: each side opens in a pair around BT3's point
        float a[8], b[8];
        memcpy(a, &body[3][0], 32);
        if (sec[3].first >= 2) memcpy(b, &body[3][32], 32); else { memcpy(b, a, 32); b[0] = -a[0]; b[2] = -a[2]; }
        float dx = b[0] - a[0], dz = b[2] - a[2], ln = std::sqrt(dx * dx + dz * dz), ux = 1, uz = 0;
        if (ln > 1e-4f) { ux = dz / ln; uz = -dx / ln; }
        const float half = 9.9f;
        std::vector<uint8_t> s;
        const float* src[6] = {a, a, b, b, a, b};
        const float sgn[6] = {1, -1, 1, -1, 0, 0};
        const float* look[6] = {b, b, a, a, b, a};
        for (int i = 0; i < 6; i++) {
            float r[8] = {src[i][0] + ux * half * sgn[i], src[i][1], src[i][2] + uz * half * sgn[i], src[i] == a ? 51.423f : -51.423f, look[i][0], look[i][1], look[i][2], 0};
            rec8(s, r);
        }
        body[3] = s; sec[3].first = 6;
    }
    if (na > 5) {
        static const float T4[3][8] = {{10, 0, 10, 20, 20, 0, 15, 0}, {-10, 0, -10, -20, -20, 0, -15, 0}, {1, 0, 1, 65, 65, 0, -30, 0}};
        body[4].clear(); for (auto& r : T4) rec8(body[4], r); sec[4].first = 3;
        body[5].clear(); rec8(body[5], T4[2]); sec[5].first = 1;
    }
    o.assign(ob, 0);
    memcpy(&o[0], d, 16);
    memcpy(&o[8], &nb, 4);
    o.insert(o.end(), datab.begin(), datab.end());
    for (uint32_t k = 0; k < na; k++) {
        uint32_t off = 0;
        if (sec[k].second) { off = (uint32_t)o.size(); o.insert(o.end(), body[k].begin(), body[k].end()); }
        memcpy(&o[0x10 + 8 * k], &sec[k].first, 4); memcpy(&o[0x14 + 8 * k], &off, 4);
    }
    o.resize(al((uint32_t)o.size(), 16), 0);
    return true;
}

// the 26 TTT entries of a BT3 stage; e15 = TTT's constant table (entry 15 of an original map), tex_id(k) as in texture keys
template <class F>
static bool stage_pak(const uint8_t* d, size_t n, const std::vector<uint8_t>& e15, F tex_id, std::vector<std::vector<uint8_t>>& ent, StageInfo& si, const StageFx* fx = nullptr) {
    std::vector<std::pair<uint32_t, uint32_t>> e;
    if (!pck2_list(d, n, e) || !e[0].second) return false;
    ent.assign(STAGE_ENTRIES, {});
    std::vector<uint8_t> scr, srf, lay;
    if (e.size() > 6 && e[5].second && e[6].second) stage_scroll(d + e[5].first, e[5].second, d + e[6].first, e[6].second, tex_id, scr, si);
    if (e.size() > 12 && e[12].second) stage_surf(d + e[12].first, e[12].second, e[1].second ? d + e[1].first : nullptr, e[1].second, e.size() > 9 && e[9].second ? d + e[9].first : nullptr, e.size() > 9 ? e[9].second : 0, tex_id, srf, si);
    std::vector<uint8_t> tan, obj;
    { std::vector<TexAnimSlot> tas; texanim_slots(d + e[0].first, e[0].second, tas, &tan); si.tex_anims = (int)tas.size(); }
    if (e.size() > 8 && e[8].second && e[1].second) stage_objs(d + e[8].first, e[8].second, d + e[1].first, e[1].second, tex_id, obj, si);
    std::vector<uint8_t> ptl;
    bool p_wthr = false, p_mef = false, p_wtr = false, p_stm = false;
    stage_ptcl(e, d, tex_id, ptl, si, &p_wthr, &p_mef, &p_wtr, &p_stm, fx);
    if (!scr.empty() || !srf.empty() || !tan.empty() || !obj.empty() || !ptl.empty()) {   // layer directory: 'BLAY', scroll sheet, surfaces, texture animation, animated objects, particles offsets
        lay.assign(32, 0);
        memcpy(&lay[0], "BLAY", 4);
        auto add = [&](const std::vector<uint8_t>& b, size_t at) { if (b.empty()) return; uint32_t o = (uint32_t)lay.size(); memcpy(&lay[at], &o, 4); lay.insert(lay.end(), b.begin(), b.end()); lay.resize(al((uint32_t)lay.size(), 16), 0); };
        add(scr, 4); add(srf, 8); add(tan, 12); add(obj, 16); add(ptl, 20);
    }
    si.layers = lay;
    if (!stage_bma7(d + e[0].first, e[0].second, tex_id, ent[0], si, !lay.empty(), tex_id(0))) return false;
    auto raw = [&](size_t i) { return i < e.size() ? std::vector<uint8_t>(d + e[i].first, d + e[i].first + e[i].second) : std::vector<uint8_t>(); };
    std::vector<int> skip;   // TTT_BT3_SKIP (diagnostic): entries served empty, "obj" = no MAPD destructibles
    bool no_obj = false;
    if (const char* sk = getenv("TTT_BT3_SKIP")) {
        std::string t(sk);
        for (size_t a = 0, b; a < t.size(); a = b + 1) { b = t.find(',', a); if (b == std::string::npos) b = t.size(); std::string w = t.substr(a, b - a); if (w == "obj") no_obj = true; else if (!w.empty()) skip.push_back(atoi(w.c_str())); }
    }
    if (e.size() > 1 && e[1].second && !stage_mapd(d + e[1].first, e[1].second, si, ent[1], no_obj)) ent[1] = raw(1);
    for (size_t i : {2, 3, 4, 18, 21, 22, 23}) ent[i] = raw(i);
    if (p_mef && ent[18].size() >= 0x20) for (int l = 0; l < 3; l++) memset(&ent[18][8 + 8 * l], 0, 4);   // the port draws the MEF0 effects; TTT's own emitters off
    if (p_wthr && ent[1].size() >= 0x68) { uint32_t of = u32(&ent[1][0x64]); if (of && of + 0x14 <= ent[1].size()) ent[1][of + 0x10] &= 0xFE; }   // and the weather
    ent[15] = e15;
    for (int b : STAGE_BANKS)
        if ((size_t)b < e.size() && e[b].second >= 0x20 && u32(d + e[b].first + 4) == 8 && !(b == 17 && p_wtr) && !(b == 13 && p_stm)) { ent[b] = stage_bank(tex_id(0x80 + b)); si.banks.push_back(b); }   // water: the port draws BT3's splashes, TTT's own splash bank off
    for (int k : skip) if (k > 1 && k < STAGE_ENTRIES) ent[k].clear();
    if (e.size() > 20) { static int lg = 0; if (lg < 4) { lg++; for (int b : {19, 20}) printf("bt3 mapa: entrada %d (indice 0x%X del juego) %u bytes, cabecera %08X %08X, servida %s\n", b, b + 1, e[b].second, e[b].second >= 8 ? u32(d + e[b].first) : 0, e[b].second >= 8 ? u32(d + e[b].first + 4) : 0, ent[b].empty() ? "no" : "si"); } }
    return true;
}

}   // namespace bt3
