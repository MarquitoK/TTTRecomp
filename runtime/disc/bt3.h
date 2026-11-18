#pragma once
// BT3 (PS2) character P file read in place by the virtual disc: model, hurtbox, params and textures go to the game
// in TTT form; vertices keep the BT3 floats (GE float position, normal, uv and weights), textures keep their own size
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include <algorithm>
#include <array>
#include <queue>

namespace bt3 {

static const uint32_t VERSION = 14;
static const uint32_t VTYPE = 3u | 3u << 5 | 3u << 7 | 3u << 9 | 1u << 14;   // float uv, normal, position, 2 float weights

static inline uint32_t u32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint16_t u16(const uint8_t* p) { uint16_t v; memcpy(&v, p, 2); return v; }
static inline float f32(const uint8_t* p) { float v; memcpy(&v, p, 4); return v; }
static inline void w32(std::vector<uint8_t>& o, size_t at, uint32_t v) { memcpy(&o[at], &v, 4); }
static inline void wf(std::vector<uint8_t>& o, size_t at, float v) { memcpy(&o[at], &v, 4); }
static inline uint32_t al(uint32_t x, uint32_t a) { return (x + a - 1) / a * a; }

// P file: 252 little-endian entries, index then data at 0x400
static bool is_p(const uint8_t* h, size_t n) {
    if (n < 12) return false;
    uint32_t c = u32(h), o0 = u32(h + 4);
    return c == 252 && o0 >= 4 + 4 * (c + 1) && o0 <= 0x1000 && (o0 & 15) == 0;
}

struct File {
    std::vector<uint8_t> d;
    bool entry(int i, const uint8_t** p, uint32_t* sz) const {
        if (!is_p(d.data(), d.size()) || i < 0 || i >= 252) return false;
        uint32_t a = u32(&d[4 + 4 * i]), b = u32(&d[8 + 4 * i]);
        if (a > b || b > d.size()) return false;
        *p = d.data() + a; *sz = b - a;
        return true;
    }
};

struct Group { int mat; const uint8_t* v; uint32_t nv; uint64_t tex0, tex2; };
struct Block { uint32_t at, len; uint16_t anc, endc, id; std::vector<Group> g; };

// geometry groups (VIF/GIF packets) from i to end
static uint32_t walk(const uint8_t* m, uint32_t i, uint32_t end, std::vector<Group>& g) {
    while (i + 0x58 <= end && m[i + 1] == 0x80) {
        uint32_t num = m[i + 2] * 16u;
        if (i + 4 + num + 3 > end) break;
        uint32_t num2 = m[i + 4 + num + 2] * 16u;
        uint64_t t1, t2; memcpy(&t1, m + i + 0x14, 8); memcpy(&t2, m + i + 0x24, 8);
        uint32_t nv = u16(m + i + 0x34);
        if (i + 0x58 + nv * 48 > end) break;
        g.push_back({(int)((t1 >> 37) & 0x3FFF), m + i + 0x58, nv, t1, t2});
        i += 8 + num + num2 + 4;
    }
    return i;
}

static std::vector<Block> blocks(const uint8_t* m, uint32_t n, uint32_t* endp = nullptr) {
    std::vector<Block> out;
    if (n < 0x70 || memcmp(m, "pmdl", 4)) return out;
    uint32_t cur = u32(m + 0x6C);
    if (endp) *endp = cur;
    while (cur + 16 <= n && out.size() < 512) {
        uint32_t len = u32(m + cur);
        if (len < 64 || (len & 15) || cur + len > n) break;
        Block b; b.at = cur; b.len = len; b.anc = u16(m + cur + 4); b.endc = u16(m + cur + 6); b.id = u16(m + cur + 10) & 0xFF;
        if (len > 0x68) walk(m, cur + 0x68, cur + len, b.g);
        out.push_back(std::move(b));
        cur += len;
        if (endp) *endp = cur;
    }
    return out;
}

// loose triangles after the block chain (skirts, capes): {count, triangles, vertex table, triangle table}; vertex 0x70 B
// (48 B like the chain plus simulation state; +0x1C = own bone, +0x0C = its weight, the rest goes to the parent);
// triangle 32 B: 3 u32 indices, GS TEX0 at +0x10
struct Tri { int mat; const uint8_t* v[3]; };
static std::vector<Tri> loose_tris(const uint8_t* m, uint32_t n, uint32_t at) {
    std::vector<Tri> out;
    if (at + 16 > n) return out;
    uint32_t nv = u32(m + at), nt = u32(m + at + 4), ov = u32(m + at + 8), ot = u32(m + at + 12);
    if (!nv || !nt || nv > 4096 || nt > 4096 || ov != at + 16 || ot != ov + nv * 0x70u || ot + nt * 32u > n) return out;
    for (uint32_t t = 0; t < nt; t++) {
        const uint8_t* r = m + ot + 32 * t;
        Tri tr; uint64_t t1; memcpy(&t1, r + 16, 8);
        tr.mat = (int)((t1 >> 37) & 0x3FFF);
        bool ok = true;
        for (int k = 0; k < 3; k++) { uint32_t i = u32(r + 4 * k); if (i >= nv) ok = false; else tr.v[k] = m + ov + 0x70 * i; }
        if (ok) out.push_back(tr);
    }
    return out;
}

static int part_flag(int id) { return id == 0x30 ? 6 : id == 0x40 ? 1 : id == 0x41 ? 2 : id == 0x42 ? 7 : id == 0x43 ? 8 : 0; }

// texture keys of the port: 0..63 main CTEX entry (= CBP), 64 + 16 * s + j image j of the face CTEX 13 + s, 128 + j blink CTEX 12
static const int KEY_FACE = 64, KEY_BLINK = 128;
struct TexKeys { int count[17] = {0}; std::map<int, std::pair<int, int>> keys; uint8_t glow[KEY_FACE] = {0}; int8_t lay[KEY_FACE] = {0}; bool lay_ok = false; };   // count: images per CTEX entry 11..16

struct OutPart { int outline, key, id; std::vector<const Group*> g; std::vector<const Tri*> t; };

// pMdl / pMdF as the game reads them: header 0xA0, material table, bone table (160 B), part table (32 B; +0x10 = 0x8000 | texture
// key, read by the port), geometry per part (group records: count, 1 bone slot, bone id, VTYPE command, vertex offset)
static void emit(const uint8_t* m, const std::vector<Block>& bl, const std::vector<OutPart>& parts, bool face, std::vector<uint8_t>& o) {
    int nmat = 0;
    for (auto& p : parts) nmat = std::max(nmat, p.outline + 1);
    uint32_t nb = (uint32_t)bl.size(), mt = 0xA0, ex = mt + 32 * nmat, bt = ex + 32, pt = bt + 160 * nb, geo = pt + 32 * (uint32_t)parts.size();
    o.assign(geo, 0);
    memcpy(&o[0], face ? "pMdF" : "pMdl", 4); w32(o, 4, 6); w32(o, 8, nb);
    wf(o, 0x10, 40.0f); wf(o, 0x14, 120.0f); memcpy(&o[0x18], m + 0x18, 16);
    wf(o, 0x40, 1.0f); wf(o, 0x44, 1.0f); wf(o, 0x48, 1.0f);
    w32(o, 0x50, bt); w32(o, 0x54, nmat); w32(o, 0x58, mt); w32(o, 0x5C, (uint32_t)parts.size()); w32(o, 0x60, pt); w32(o, 0x68, ex);
    for (int i = 0; i < nmat; i++) { w32(o, mt + 32 * i, i); w32(o, mt + 32 * i + 4, nmat); }
    w32(o, ex, bt); w32(o, ex + 16, bt);
    for (uint32_t k = 0; k < nb; k++) {
        uint32_t r = bt + 160 * k;
        const Block& b = bl[k];
        w32(o, r, 0xA0);
        uint16_t h[4] = {b.anc, b.endc, 1, b.id};
        memcpy(&o[r + 4], h, 8);
        memcpy(&o[r + 0x10], m + b.at + 0x10, 0x30);
        float c[8] = {0, 0, 0, 1, 0, 0, 0, 0.5f};
        if (k + 1 < nb && bl[k + 1].len > 0x60) {   // the TTT record carries the bounds of the next bone, full size
            const uint8_t* q = m + bl[k + 1].at;
            c[0] = f32(q + 0x40); c[1] = f32(q + 0x44); c[2] = f32(q + 0x48);
            c[4] = f32(q + 0x50) * 2; c[5] = f32(q + 0x54) * 2; c[6] = f32(q + 0x58) * 2;
        }
        memcpy(&o[r + 0x40], c, 32);
    }
    for (size_t p = 0; p < parts.size(); p++) {
        const OutPart& pp = parts[p];
        // loose triangles: one group per triangle and face side, one bone slot per distinct vertex bone (weights w, 1 - w per slot)
        struct EG { uint32_t nv, slots; uint8_t ids[4]; std::vector<float> d; };
        std::vector<EG> eg;
        for (auto* g : pp.g) {
            EG e{g->nv, 1, {(uint8_t)pp.id, 0, 0, 0}, {}};
            for (uint32_t q = 0; q < g->nv; q++) {
                const uint8_t* s = g->v + 48 * q;
                float w = std::min(1.0f, std::max(0.0f, f32(s + 12)));
                float v[10] = {w, 1.0f - w, (f32(s + 32) - 1.0f / 512) * 2, (1.0f + f32(s + 36) - 1.0f / 512) * 2,
                               f32(s + 16), f32(s + 20), f32(s + 24), f32(s), f32(s + 4), f32(s + 8)};
                e.d.insert(e.d.end(), v, v + 10);
            }
            eg.push_back(std::move(e));
        }
        for (auto* t : pp.t) for (int side = 0; side < 2; side++) {
            EG e{3, 0, {0, 0, 0, 0}, {}};
            int sl[3];
            for (int k = 0; k < 3; k++) {
                uint8_t b = (uint8_t)u32(t->v[k] + 0x1C);
                int j = 0;
                while (j < (int)e.slots && e.ids[j] != b) j++;
                if (j == (int)e.slots) e.ids[e.slots++] = b;
                sl[k] = j;
            }
            static const int ORD[2][3] = {{0, 1, 2}, {0, 2, 1}};
            for (int q = 0; q < 3; q++) {
                int k = ORD[side][q];
                const uint8_t* s = t->v[k];
                float w = std::min(1.0f, std::max(0.0f, f32(s + 12))), sg = side ? -1.0f : 1.0f;
                for (uint32_t j = 0; j < e.slots; j++) { e.d.push_back((int)j == sl[k] ? w : 0.0f); e.d.push_back((int)j == sl[k] ? 1.0f - w : 0.0f); }
                float v[8] = {(f32(s + 32) - 1.0f / 512) * 2, (1.0f + f32(s + 36) - 1.0f / 512) * 2,
                              sg * f32(s + 16), sg * f32(s + 20), sg * f32(s + 24), f32(s), f32(s + 4), f32(s + 8)};
                e.d.insert(e.d.end(), v, v + 8);
            }
            eg.push_back(std::move(e));
        }
        uint32_t hdr = 4 + 16 * (uint32_t)eg.size(), vo = al(hdr, 64), nb = 0;
        for (auto& e : eg) nb += (uint32_t)e.d.size() * 4;
        uint32_t base = (uint32_t)o.size(), size = al(vo + nb, 32);
        o.resize(base + size, 0);
        w32(o, base, (uint32_t)eg.size());
        uint32_t cur = vo;
        for (size_t i = 0; i < eg.size(); i++) {
            const EG& e = eg[i];
            uint32_t r = base + 4 + 16 * (uint32_t)i;
            uint16_t h[2] = {(uint16_t)e.nv, (uint16_t)e.slots};
            memcpy(&o[r], h, 4);
            memcpy(&o[r + 4], e.ids, 4);
            uint32_t vt = (VTYPE & ~(7u << 14)) | ((2 * e.slots - 1) << 14);
            w32(o, r + 8, 0x12000000u | vt); w32(o, r + 12, cur);
            memcpy(&o[base + cur], e.d.data(), e.d.size() * 4);
            cur += (uint32_t)e.d.size() * 4;
        }
        uint32_t e = pt + 32 * (uint32_t)p;
        int16_t mm = (int16_t)pp.outline;
        uint16_t key = (uint16_t)(0x8000 | pp.key);
        memcpy(&o[e], &mm, 2); o[e + 2] = 0xFF; o[e + 3] = 0xFF;
        w32(o, e + 4, base); w32(o, e + 8, size); w32(o, e + 12, (uint32_t)part_flag(pp.id));
        memcpy(&o[e + 0x10], &key, 2);
    }
}

// outline layer = material (max 15 per fighter); the texture key goes apart so more textures never break the outline
static int outline_of(int cbp, const TexKeys* tk = nullptr) { return tk && tk->lay_ok && cbp >= 0 && cbp < KEY_FACE ? tk->lay[cbp] : std::min(cbp, 14); }
// more than 15 materials: merge the last ones with equal glow, so a glowing part (halo) never shares its layer with a plain one
static void plan_layers(TexKeys& tk) {
    std::vector<std::vector<int>> l;
    for (auto& kv : tk.keys) if (kv.first >= 0 && kv.first < KEY_FACE) l.push_back({kv.first});
    tk.lay_ok = false;
    if (l.size() <= 15) return;
    while (l.size() > 15) {
        bool done = false;
        for (int i = (int)l.size() - 1; i > 0 && !done; i--)
            for (int j = i - 1; j >= 0 && !done; j--)
                if (tk.glow[l[i][0]] == tk.glow[l[j][0]]) { l[j].insert(l[j].end(), l[i].begin(), l[i].end()); l.erase(l.begin() + i); done = true; }
        if (!done) { l[l.size() - 2].insert(l[l.size() - 2].end(), l.back().begin(), l.back().end()); l.pop_back(); }
    }
    for (int i = 0; i < KEY_FACE; i++) tk.lay[i] = 14;
    for (size_t i = 0; i < l.size(); i++) for (int m : l[i]) tk.lay[m] = (int8_t)i;
    tk.lay_ok = true;
}
static int tex_glow(const File& f, int ent, int img) {   // CTEX entry +0x2D, -1 when the CTEX is not in the file
    const uint8_t* t; uint32_t n;
    if (!f.entry(ent, &t, &n) || n < 0x20 || u32(t + 4) != 8 || img < 0 || (uint32_t)img >= u32(t) || 32u + 64u * (img + 1) > n) return -1;
    return std::min<int>(t[32 + 64 * img + 0x2D], 0x80);
}

// TTT_BT3_LOOSE=0 leaves out the loose triangles after the chain (skirts, capes)
static bool loose_on() { const char* e = getenv("TTT_BT3_LOOSE"); return !(e && *e == '0'); }

static bool model(const uint8_t* m, uint32_t n, std::vector<uint8_t>& o, TexKeys* tk = nullptr) {
    uint32_t end = 0;
    std::vector<Block> bl = blocks(m, n, &end);
    if (bl.empty()) return false;
    std::vector<Tri> lt = loose_on() ? loose_tris(m, n, end) : std::vector<Tri>();
    std::vector<OutPart> parts;
    for (auto& b : bl) {
        std::vector<int> order;
        for (auto& g : b.g) if (std::find(order.begin(), order.end(), g.mat) == order.end()) order.push_back(g.mat);
        for (int mt : order) {
            OutPart p; p.outline = outline_of(mt, tk); p.key = mt; p.id = b.id;
            for (auto& g : b.g) if (g.mat == mt) p.g.push_back(&g);
            parts.push_back(std::move(p));
            if (tk && mt < KEY_FACE) tk->keys[mt] = {11, mt};
        }
    }
    std::vector<int> lord;
    for (auto& t : lt) if (std::find(lord.begin(), lord.end(), t.mat) == lord.end()) lord.push_back(t.mat);
    for (int mt : lord) {
        OutPart p; p.outline = outline_of(mt, tk); p.key = mt; p.id = 3;
        for (auto& t : lt) if (t.mat == mt) p.t.push_back(&t);
        parts.push_back(std::move(p));
        if (tk && mt < KEY_FACE) tk->keys[mt] = {11, mt};
    }
    if (!lt.empty()) printf("bt3: %zu triangulos sueltos (faldas, capas) en %zu partes\n", lt.size(), lord.size());
    emit(m, bl, parts, false, o);
    return true;
}

// face entry (3..10): loose groups after an 8 byte chain word, all on the face bone; with its own CTEX (13..16 for 7..10)
// the textures map by order of TBP0 to its images, like his addon; the rest use the main CTEX
static bool face(const uint8_t* m, uint32_t n, const uint8_t* f, uint32_t fn, int slot, std::vector<uint8_t>& o, TexKeys* tk) {
    o.clear();
    if (fn < 0x60) return true;
    std::vector<Block> bl = blocks(m, n);
    if (bl.empty()) return false;
    std::vector<Group> g;
    walk(f, 8, fn, g);
    if (g.empty()) return true;
    int own = slot >= 7 && slot <= 10 && tk ? tk->count[slot + 6] : 0;
    std::vector<uint64_t> tex;
    for (auto& x : g) if (std::find(tex.begin(), tex.end(), x.tex0) == tex.end()) tex.push_back(x.tex0);
    std::sort(tex.begin(), tex.end(), [](uint64_t a, uint64_t b) { return (a & 0x3FFF) < (b & 0x3FFF); });
    std::vector<OutPart> parts;
    for (size_t i = 0; i < tex.size(); i++) {
        int cbp = (int)((tex[i] >> 37) & 0x3FFF);
        OutPart p; p.outline = outline_of(cbp, tk); p.id = 0x30;
        if ((int)i < own) { p.key = KEY_FACE + 16 * (slot - 7) + (int)i; if (tk) tk->keys[p.key] = {slot + 6, (int)i}; }
        else { p.key = cbp; if (tk && cbp < KEY_FACE) tk->keys[cbp] = {11, cbp}; }
        for (auto& x : g) if (x.tex0 == tex[i]) p.g.push_back(&x);
        parts.push_back(std::move(p));
    }
    emit(m, bl, parts, true, o);
    return true;
}

// params with a different layout in TTT (his converter); the rest are the same
static void param17(const uint8_t* s, uint32_t n, std::vector<uint8_t>& o) {
    o.assign(s, s + n);
    const int ins[4][2] = {{0xCA, 2}, {0x98, 1}, {0x88, 1}, {0x10, 8}};
    for (auto& k : ins) if ((size_t)k[0] <= o.size()) o.insert(o.begin() + k[0], (size_t)k[1], 0);
    o.resize(n);
}
static void param18(const uint8_t* s, uint32_t n, std::vector<uint8_t>& o) {
    o.assign(s, s + n);
    for (uint32_t b = 0; b + 0x30 <= n; b += 0x30) {
        memmove(&o[b + 0x24], &o[b + 0x23], 0x30 - 0x24);
        o[b + 0x23] = 0;
    }
}
static void param19(const uint8_t* s, uint32_t n, std::vector<uint8_t>& o) {
    o.clear();
    for (uint32_t i = 0; i < n; i++) {
        o.push_back(s[i]);
        size_t k = o.size();
        if (k >= 4 && o[k - 3] == 0x0E && o[k - 2] == 0x02 && o[k - 1] == 0x02) o.insert(o.end(), 4, 0);
    }
    o.resize(al((uint32_t)o.size(), 16), 0);
}

// PS2 texture unswizzle, same passes as his addon (bt3_tex.py); python index wrap kept
struct Buf { std::vector<uint8_t>& v; uint8_t& operator[](long i) { long n = (long)v.size(); if (i < 0) i += n; static thread_local uint8_t dummy; return i >= 0 && i < n ? v[i] : (dummy = 0); } };
static void rarr(Buf& s, Buf& d, long o, long n, int loops) { for (int i = 0; i < loops; i++) { d[n] = s[o]; n++; o += 4; } }
static void tbr8(Buf& s, Buf& d, long o, long n) { rarr(s, d, o, n, 8); rarr(s, d, o + 2, n + 8, 8); }
static void tbr4(Buf& s, Buf& d, long o, long n) { rarr(s, d, o, n, 4); o -= 16; rarr(s, d, o, n + 4, 4); o += 18; rarr(s, d, o, n + 8, 4); o -= 16; rarr(s, d, o, n + 12, 4); }
static std::vector<uint8_t> bmp_order(std::vector<uint8_t>& px, long w, long h) {
    std::vector<uint8_t> out(px.size());
    Buf s{px}, d{out};
    long red = w * 2, o = 0, n = (long)out.size() - w;
    long rows = (w * h) / ((w * 4) * 2);
    for (long r = 0; r < rows; r++) {
        for (int a = 0; a < 2; a++) { for (long c = 0; c < w / 16; c++) { tbr8(s, d, o, n); n += 16; o += 32; } n -= red; }
        o -= red * 2 - 17;
        for (int a = 0; a < 2; a++) { for (long c = 0; c < w / 16; c++) { tbr4(s, d, o, n); n += 16; o += 32; } n -= red; }
        o -= 1;
        for (long c = 0; c < w / 16; c++) { tbr4(s, d, o, n); n += 16; o += 32; }
        n -= red;
        for (long c = 0; c < w / 16; c++) { tbr4(s, d, o, n); n += 16; o += 32; }
        o -= red * 2 + 15; n -= red;
        for (int a = 0; a < 2; a++) { for (long c = 0; c < w / 16; c++) { tbr8(s, d, o, n); n += 16; o += 32; } n -= red; }
        o -= 1;
    }
    return out;
}
static std::vector<uint8_t> reorder_px(const std::vector<uint8_t>& px, long width, long height) {
    std::vector<uint8_t> out(px.size());
    long np = 0, op = 0, base = 0, col = 0, cs = 32, count, cols;
    if (width > 128) { count = width / 128; cols = 4; } else { count = 1; cols = width / 32; }
    auto copy = [&](long n, long o) { for (long i = 0; i < cs; i++) if (n + i < (long)out.size() && o + i < (long)px.size()) out[n + i] = px[o + i]; };
    for (long br = 0; br < height / 16; br++) {
        for (int a = 0; a < 8; a++) {
            long bco = col;
            for (long b = 0; b < count; b++) {
                for (long c = 0; c < cols; c++) { copy(np, op); op += width * 16; np += cs; }
                bco += 256; op = bco;
            }
            col += width * 2; op = col;
        }
        base += cs; col = base; op = base;
        if ((br + 1) % 8 == 0) { base = 128 * width * ((br + 1) / 8) / 2; col = base; op = base; }
    }
    return out;
}
static std::vector<uint8_t> interlace(const std::vector<uint8_t>& px) {
    std::vector<uint8_t> b(px.size());
    size_t n = 0, o = 0;
    for (size_t r = 0; r < px.size() / 32; r++) {
        for (int a = 0; a < 2; a++) {
            size_t c = 0;
            for (int k = 0; k < 8; k++) { for (int j = 0; j < 2; j++) { if (o + c + j < px.size()) b[n] = px[o + c + j]; n++; } c += 4; }
            o += 2;
        }
        o += 28;
    }
    return b;
}
static std::vector<uint8_t> reorder_pal(const std::vector<uint8_t>& pl) {
    std::vector<uint8_t> out(pl.size());
    auto cp = [&](size_t op, size_t bp, size_t k) { for (size_t i = 0; i < k; i++) if (op + i < out.size() && bp + i < pl.size()) out[op + i] = pl[bp + i]; };
    size_t bp = 0, op = 0;
    cp(op, bp, 32); bp += 32; op = 64;
    for (int i = 0; i < 7; i++) {
        cp(op, bp, 32); bp += 32; op -= 32;
        cp(op, bp, 32); bp += 32; op += 64;
        cp(op, bp, 64); bp += 64; op += 96;
    }
    cp(op, bp, 32); bp += 32; op -= 32;
    cp(op, bp, 32); bp += 32; op += 32;
    cp(bp, bp, 32);
    return out;
}

// CTEX (.dbt) entry k as RGBA, rows in GS memory order (row 0 = v 0), alpha on the PS2/PSP scale (0..128)
static bool texture_in(const uint8_t* t, uint32_t n, uint32_t cnt, uint32_t tbl, int k, int* w, int* h, std::vector<uint32_t>& px) {
    if (k < 0 || (uint32_t)k >= cnt || tbl + 64 * (k + 1) > n) return false;
    const uint8_t* e = t + tbl + 64 * k;
    uint32_t tp = u32(e) * 4 + 96, pp = u32(e + 4) * 4 + 96, tl = u32(e + 8), pl = u32(e + 12);
    uint64_t tex0; memcpy(&tex0, e + 0x30, 8);
    int psm = (int)((tex0 >> 20) & 0x3F);
    *w = 1 << ((tex0 >> 26) & 0xF); *h = 1 << ((tex0 >> 30) & 0xF);
    long W = *w, H = *h;
    if (!tex0 || tl <= 128 || tp + tl - 128 > n) return false;
    std::vector<uint8_t> raw(t + tp, t + tp + tl - 128), pal;
    if (psm != 0) { if (pl <= 128 || pp + pl - 128 > n) return false; pal.assign(t + pp, t + pp + pl - 128); }
    auto col = [](const uint8_t* c) { return (uint32_t)c[0] | (uint32_t)c[1] << 8 | (uint32_t)c[2] << 16 | (uint32_t)c[3] << 24; };
    px.assign((size_t)W * H, 0);
    if (psm == 20) {
        std::vector<uint8_t> s = interlace(reorder_px(raw, W, H)), s3(s.size() * 2);
        for (size_t i = 0; i < s.size(); i++) { s3[2 * i] = s[i] & 15; s3[2 * i + 1] = s[i] >> 4; }
        std::vector<uint8_t> s4 = bmp_order(s3, W, H);
        size_t np = pal.size() / 4;
        if (!np) return false;
        for (long y = 0; y < H; y++)
            for (long x = 0; x < W; x++) {
                size_t i = (size_t)((H - 1 - y) * W + x);
                px[(size_t)y * W + x] = col(&pal[4 * ((i < s4.size() ? s4[i] : 0) % np)]);
            }
    } else if (psm == 19) {
        std::vector<uint8_t> s = bmp_order(raw, W, H), p = reorder_pal(pal);
        size_t np = p.size() / 4;
        for (long y = 0; y < H; y++)
            for (long x = 0; x < W; x++) {
                size_t i = (size_t)(y * W + x);
                uint8_t ci = i < s.size() ? s[i] : 0;
                if (ci < np) px[(size_t)(H - 1 - y) * W + x] = col(&p[4 * ci]);
            }
    } else if (psm == 0) {
        for (size_t i = 0; i < px.size() && 4 * i + 4 <= raw.size(); i++) px[i] = col(&raw[4 * i]);
    } else return false;
    return true;
}

static bool texture(const uint8_t* t, uint32_t n, int k, int* w, int* h, std::vector<uint32_t>& px) {
    if (n < 0x20 || u32(t + 4) != 8) return false;
    return texture_in(t, n, u32(t), u32(t + 4) * 4, k, w, h, px);
}

static bool tex_size(const uint8_t* t, uint32_t n, int k, int* w, int* h) {
    if (n < 0x20 || u32(t + 4) != 8 || k < 0 || (uint32_t)k >= u32(t) || 32 + 64 * (uint32_t)(k + 1) > n) return false;
    uint64_t tex0; memcpy(&tex0, t + 32 + 64 * k + 0x30, 8);
    if (!tex0) return false;
    *w = 1 << ((tex0 >> 26) & 0xF); *h = 1 << ((tex0 >> 30) & 0xF);
    return true;
}

// ---------------- move list (BT3 043..051 texts -> TTT 043 records and their texts, item G 61) ----------------
struct MoveRec { int cat = 0, a = 0, b = 0, c = 0; std::string name, cmd; };

static void put_utf8(std::string& o, uint32_t ch) {
    if (ch < 0x80) o += (char)ch;
    else if (ch < 0x800) { o += (char)(0xC0 | ch >> 6); o += (char)(0x80 | (ch & 0x3F)); }
    else if (ch < 0x10000) { o += (char)(0xE0 | ch >> 12); o += (char)(0x80 | (ch >> 6 & 0x3F)); o += (char)(0x80 | (ch & 0x3F)); }
    else { o += (char)(0xF0 | ch >> 18); o += (char)(0x80 | (ch >> 12 & 0x3F)); o += (char)(0x80 | (ch >> 6 & 0x3F)); o += (char)(0x80 | (ch & 0x3F)); }
}
static std::vector<uint32_t> cps(const std::string& s) {
    std::vector<uint32_t> o;
    for (size_t i = 0; i < s.size();) {
        uint32_t ch = (uint8_t)s[i], extra = ch >= 0xF0 ? 3 : ch >= 0xE0 ? 2 : ch >= 0xC0 ? 1 : 0;
        if (extra) ch &= 0x3F >> extra;
        i++;
        for (uint32_t e = 0; e < extra && i < s.size(); e++, i++) ch = ch << 6 | ((uint8_t)s[i] & 0x3F);
        o.push_back(ch);
    }
    return o;
}

// the text of entry e: UTF-16 from its BOM (the index can point a few bytes after it) to the NUL; UTF-8 accepted too
static bool move_text(const File& f, int e, std::string& out) {
    out.clear();
    const uint8_t* p; uint32_t n;
    if (!f.entry(e, &p, &n) || !n) return false;
    const uint8_t* lo = f.d.data();
    const uint8_t* s = p;
    for (const uint8_t* q = p; q >= lo + 2 && q + 64 >= p; q -= 2) if (q[0] == 0xFF && q[1] == 0xFE) { s = q; break; }
    if (s[0] == 0xFF && s[1] == 0xFE) {
        const uint8_t* end = lo + f.d.size();
        for (const uint8_t* q = s + 2; q + 1 < end && q < p + n + 64; q += 2) {
            uint32_t ch = q[0] | q[1] << 8;
            if (!ch) break;
            if (ch >= 0xD800 && ch < 0xDC00 && q + 3 < end) { uint32_t lo2 = q[2] | q[3] << 8; ch = 0x10000 + ((ch - 0xD800) << 10) + (lo2 - 0xDC00); q += 2; }
            put_utf8(out, ch);
        }
    } else {
        size_t k = 0;
        if (n >= 3 && p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) k = 3;
        for (; k < n && p[k]; k++) out += (char)p[k];
    }
    out.erase(std::remove(out.begin(), out.end(), '\r'), out.end());
    return !out.empty();
}

// <PAD=...> of the PS2 controls -> TTT glyph string (buttons U+FF67.., ASCII + ( ) /); unknown symbols stay as text
static std::string move_cmd(const std::string& pad, bool holds) {   // holds: square/triangle held glyphs (not in specials)
    std::string s = pad;
    size_t a = s.find("<PAD=");
    if (a != std::string::npos) { s = s.substr(a + 5); size_t b = s.rfind('>'); if (b != std::string::npos) s = s.substr(0, b); }
    std::vector<uint32_t> c = cps(s);
    std::string o; uint32_t last = 0;
    for (size_t i = 0; i < c.size(); i++) {
        uint32_t ch = c[i], g = 0;
        switch (ch) {
            case 0x25CB: g = 0xFF6A; break;   // circle
            case 0x25A1: g = 0xFF6B; break;   // square
            case 0x25B3: g = 0xFF6C; break;   // triangle
            case 0x00D7: g = 0xFF6D; break;   // cross
            case 0x2191: g = 0xFF71; break;
            case 0x2193: g = 0xFF72; break;
            case 0x2192: g = 0xFF73; break;
            case 0x2190: g = 0xFF74; break;
            case 0xFF0B: g = '+'; break;
            case 0xFF08: g = '('; break;
            case 0xFF09: g = ')'; break;
            case 0xFF0F: g = '/'; break;
        }
        if ((ch == 0xFF2C || ch == 0xFF32) && i + 1 < c.size() && c[i + 1] == 0xFF12) { g = ch == 0xFF2C ? 0xFF6E : 0xFF6F; i++; }   // L2, R2
        if (ch == 0x2642) {   // held: square and triangle have their own glyph; in specials TTT shows the plain button
            if (holds && (last == 0xFF6B || last == 0xFF6C)) { o.resize(o.size() - 3); put_utf8(o, last == 0xFF6B ? 0xFF79 : 0xFF7A); last = 0; }
            continue;
        }
        put_utf8(o, g ? g : ch);
        last = g ? g : ch;
    }
    return o;
}

static void move_parse(const std::string& s, std::vector<MoveRec>& out) {
    out.clear();
    int cat = 0; bool open = false, hidden = false;
    MoveRec r; std::string cond, notes, pad, alt;
    auto flush = [&]() {
        if (!open) return;
        std::string cmd = move_cmd(pad.empty() ? alt : pad, r.cat != 0);
        if (!cond.empty()) cmd = cond + (cps(cond).size() > 28 ? ",\n" : ", ") + cmd;
        if (!notes.empty() && r.cat != 0) cmd += "\n" + notes;   // specials: TTT shows the costs and the unblockable mark as icons
        r.cmd = cmd;
        if (!hidden) out.push_back(r);
        open = false;
    };
    size_t pos = 0;
    while (pos < s.size()) {
        size_t nl = s.find('\n', pos);
        std::string l = s.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = nl == std::string::npos ? s.size() : nl + 1;
        if (l.size() > 3 && l[0] == '!' && l[2] == '&') { size_t k = 3; while (k < l.size() && isdigit((unsigned char)l[k])) k++; l = l.substr(0, 2) + l.substr(k); }
        if (l.size() >= 3 && l.compare(0, 3, "!F$") == 0) { flush(); if (l.size() > 3 && isdigit((unsigned char)l[3])) cat = l[3] - '0'; continue; }
        if (l.compare(0, 3, "!F@") == 0) { flush(); break; }
        if (l.size() >= 6 && l.compare(0, 3, "!F*") == 0) {
            flush();
            r = MoveRec(); r.cat = cat == 3 ? 2 : std::min(cat, 2);
            r.a = isdigit((unsigned char)l[3]) ? l[3] - '0' : 0; r.b = isdigit((unsigned char)l[4]) ? l[4] - '0' : 0; r.c = isdigit((unsigned char)l[5]) ? l[5] - '0' : 0;
            r.name = l.substr(6); hidden = r.name == "0" || r.name.empty();
            cond.clear(); notes.clear(); pad.clear(); alt.clear(); open = true;
            continue;
        }
        if (!open) continue;
        if (l.size() >= 4 && l.compare(0, 3, "!F%") == 0) {
            std::string t = l.substr(4);
            if (t == "0" || t.empty()) continue;
            if (l[3] == '0') cond = t; else notes += (notes.empty() ? "" : "\n") + t;
            continue;
        }
        if (l.size() >= 3 && l[0] == '!' && l[2] == '#') {
            if ((l[1] == '1' || l[1] == 'D') && pad.empty()) pad = l.substr(3);
            else if (l[1] != '2' && alt.empty()) alt = l.substr(3);
        }
    }
    flush();
}

// 043 for the game: 32 B records {category, name id, command id, 0, cost b, cost c, unblockable a, skill}, then -1
static void move_dat(const std::vector<MoveRec>& m, std::vector<uint8_t>& o) {
    o.assign(32 * (m.size() + 1), 0);
    for (size_t i = 0; i < m.size(); i++) {
        int32_t v[8] = {m[i].cat, 0, 0, 0, m[i].b, m[i].c, m[i].a, m[i].cat == 0 ? 1 : 0};
        memcpy(&o[32 * i], v, 32);
    }
    int32_t end = -1;
    memcpy(&o[32 * m.size()], &end, 4);
}

// languages of the port (en, es) and jp: entries 44, 45, 43 (slot 2 stays empty: French is not read); the record list follows English
static const int MOVE_ENTRY[4] = {44, 45, -1, 43};
// the index of entries 43..51 is not reliable (it can point inside or before a text, or a text spills into the next entry):
// every BOM between entry 43 and 52 starts a text, told apart by its first category label (bt3_open.py textos_movelist)
static void move_texts(const File& f, std::string (&t)[4]) {
    for (auto& x : t) x.clear();
    const uint8_t *p43, *p52; uint32_t n43, n52;
    if (!f.entry(43, &p43, &n43) || !f.entry(52, &p52, &n52)) return;
    const uint8_t* lo = f.d.data(); const uint8_t* end = lo + f.d.size();
    const uint8_t* a = std::max(lo, p43 - 64); const uint8_t* b = std::min(end, p52 + 64);
    if ((a - lo) & 1) a++;
    std::string es0;
    for (const uint8_t* q = a; q + 1 < b; q += 2) {
        if (q[0] != 0xFF || q[1] != 0xFE) continue;
        std::string o;
        for (const uint8_t* r = q + 2; r + 1 < end; r += 2) {
            uint32_t ch = r[0] | r[1] << 8;
            if (!ch || ch == 0xFEFF) break;
            if (ch >= 0xD800 && ch < 0xDC00 && r + 3 < end) { uint32_t lo2 = r[2] | r[3] << 8; ch = 0x10000 + ((ch - 0xD800) << 10) + (lo2 - 0xDC00); r += 2; }
            put_utf8(o, ch);
        }
        o.erase(std::remove(o.begin(), o.end(), '\r'), o.end());
        if (o.compare(0, 3, "!F$") || o.size() < 8) continue;
        size_t nl = o.find('\n');
        if (nl != std::string::npos && nl < 12 && o.find("!F$", 3) < nl) continue;   // a cut copy of the next text
        std::string lab = o.substr(4, nl == std::string::npos ? std::string::npos : nl - 4);
        while (!lab.empty() && lab.back() == ' ') lab.pop_back();
        int l = lab == "Special Attack" ? 0 : lab == "Ataque especial" ? 1 : (lab == "0" || lab.empty()) ? 4 : -1;
        if (l == 4) { if (es0.empty()) es0 = o; continue; }
        if (l >= 0 && t[l].empty()) t[l] = o;
        if (l < 0 && t[3].empty() && (uint8_t)lab[0] >= 0xE3) t[3] = o;
    }
    if (t[1].empty()) t[1] = es0;   // the short unlabeled Spanish list of some files
}
static bool move_lists(const File& f, std::vector<MoveRec> (&m)[4]) {
    std::string t[4];
    move_texts(f, t);
    for (int l = 0; l < 4; l++) { m[l].clear(); if (!t[l].empty()) move_parse(t[l], m[l]); }
    if (m[0].empty()) for (int l = 1; l < 4; l++) if (!m[l].empty()) { m[0] = m[l]; break; }
    return !m[0].empty();   // each language keeps its own list: BT3 files of one character can differ in records
}

// TTT 1_p entry number -> bytes served from the BT3 file (only for the numbers BT3 provides)
static bool provides(int num, int mode) { return num == 1 || num == 43 || num == 2 || (num >= 3 && num <= 10) || (num >= 12 && num <= 16) || (num >= 28 && num <= 42) || (mode == 1 && num >= 17 && num <= 27); }

static bool ttt_entry(const File& f, int num, std::vector<uint8_t>& o, TexKeys* tk = nullptr) {
    o.clear();
    const uint8_t* p; uint32_t n;
    if (num == 2) return f.entry(2, &p, &n) && model(p, n, o, tk);
    if (num >= 3 && num <= 10) {
        const uint8_t* fp; uint32_t fn;
        return f.entry(2, &p, &n) && f.entry(num, &fp, &fn) && face(p, n, fp, fn, num, o, tk);
    }
    if (num >= 12 && num <= 16) return true;   // the CTEX of faces and blink reach the port as texture keys
    if (num == 43) {   // room for the longest language; the port writes the records of the game's language when it serves the pak
        std::vector<MoveRec> m[4];
        if (!move_lists(f, m)) return false;
        size_t mx = 0;
        for (auto& x : m) mx = std::max(mx, x.size());
        std::vector<MoveRec> r = m[0];
        r.resize(mx);
        move_dat(r, o);
        if (mx > m[0].size()) { int32_t end = -1; memcpy(&o[32 * m[0].size()], &end, 4); }
        return true;
    }
    if (!f.entry(num, &p, &n)) return false;
    if (num == 17) param17(p, n, o);
    else if (num == 18) param18(p, n, o);
    else if (num == 19) param19(p, n, o);
    else o.assign(p, p + n);
    return true;
}

// BT3 cel shading (EBOOT_TTT 66.5, 67.1; decomp ObjDraw_SetLight): each group's TEX0_2 CLUT (CBP / 4 = shading slot, CTEX 11
// image clutFirst + slot, 16x16 CT32 CSM1) is read through the shared light texture and SUBTRACTED from the texture (context 2
// ALPHA 0x62 = Cd - Cs); the coloured entries fall on the shadow side. shade_tpal builds a TPAL table (13.6 format, mode 3
// Subtract, index 0 = shadow) per port material key: main model and face entries
static const uint8_t SHADE_IDX[16] = {247, 231, 216, 199, 183, 167, 151, 135, 119, 103, 87, 71, 55, 38, 23, 7};
static void shade_slots(const File& f, const TexKeys& tk, std::map<int, int>& sl) {
    const uint8_t *m, *fp; uint32_t n, fn;
    if (!f.entry(2, &m, &n)) return;
    for (auto& b : blocks(m, n)) for (auto& g : b.g) sl.emplace(g.mat, (int)((g.tex2 >> 37) & 0x3FFF) / 4);
    for (int slot = 3; slot <= 10; slot++) {   // same keys as face()
        if (!f.entry(slot, &fp, &fn) || fn < 0x60) continue;
        std::vector<Group> g;
        walk(fp, 8, fn, g);
        int own = slot >= 7 ? tk.count[slot + 6] : 0;
        std::vector<uint64_t> tex;
        for (auto& x : g) if (std::find(tex.begin(), tex.end(), x.tex0) == tex.end()) tex.push_back(x.tex0);
        std::sort(tex.begin(), tex.end(), [](uint64_t a, uint64_t b) { return (a & 0x3FFF) < (b & 0x3FFF); });
        for (size_t i = 0; i < tex.size(); i++) {
            int key = (int)i < own ? KEY_FACE + 16 * (slot - 7) + (int)i : (int)((tex[i] >> 37) & 0x3FFF);
            for (auto& x : g) if (x.tex0 == tex[i]) { sl.emplace(key, (int)((x.tex2 >> 37) & 0x3FFF) / 4); break; }
        }
    }
}
// clut(s, px): the 256 colours of shading slot s as stored (row-major), false when missing
// glow_png(ent, img): the glow of an opened CTEX image (PNG chunk), -1 when unknown
template <class F, class G> static bool shade_tpal(const File& f, const TexKeys& tk, F clut, G glow_png, std::vector<uint8_t>& o) {
    o.clear();
    std::map<int, int> sl;
    shade_slots(f, tk, sl);
    std::map<int, std::array<uint8_t, 64>> ramp;
    std::vector<uint8_t> e;
    // glow strength per texture (CTEX entry +0x2D, 0..0x80; BT3 ObjGlow: the fighter blurred and added back, per material)
    auto glow = [&](int key) -> int {
        int ent = 11, img = key;
        if (key >= KEY_FACE && key < KEY_BLINK) { ent = 13 + (key - KEY_FACE) / 16; img = (key - KEY_FACE) % 16; }
        const uint8_t* t; uint32_t n;
        if (!f.entry(ent, &t, &n) || n < 0x20 || u32(t + 4) != 8 || (uint32_t)img >= u32(t) || 32u + 64u * (img + 1) > n) return std::max(0, std::min<int>(glow_png(ent, img), 0x80));
        return std::min<int>(t[32 + 64 * img + 0x2D], 0x80);
    };
    for (auto& kv : sl) {
        if (kv.first > 255) continue;
        auto it = ramp.find(kv.second);
        if (it == ramp.end()) {
            std::vector<uint32_t> px;
            std::array<uint8_t, 64> r{};
            if (!clut(kv.second, px) || px.size() < 256) continue;
            for (int i = 0; i < 16; i++) { int k = SHADE_IDX[15 - i]; uint32_t c = px[(k & ~0x18) | ((k & 8) << 1) | ((k & 16) >> 1)]; memcpy(&r[i * 4], &c, 3); r[i * 4 + 3] = 255; }
            it = ramp.emplace(kv.second, r).first;
        }
        bool any = false;
        for (int i = 0; i < 16; i++) any = any || it->second[i * 4] || it->second[i * 4 + 1] || it->second[i * 4 + 2];
        int gl = glow(kv.first);
        if (!any && !gl) continue;
        size_t at = e.size();
        e.resize(at + 72, 0);
        e[at] = (uint8_t)kv.first; e[at + 1] = 3; e[at + 3] = (uint8_t)gl;
        memcpy(&e[at + 8], it->second.data(), 64);
        if (e.size() / 72 >= 64) break;
    }
    if (e.empty()) return false;
    uint16_t ver = 1, cnt = (uint16_t)(e.size() / 72);
    o.assign({'T', 'P', 'A', 'L'});
    o.resize(8); memcpy(&o[4], &ver, 2); memcpy(&o[6], &cnt, 2);
    o.insert(o.end(), e.begin(), e.end());
    return true;
}
static int shade_first(const File& f) { const uint8_t* m; uint32_t n; return f.entry(2, &m, &n) && n > 0x52 ? m[0x50] : -1; }

// CTEX entry k as indices (rows top-down, as the texture is shown) + its palette with the BT3 alpha (0..128, same scale as TTT effects)
static bool ctex_indexed(const uint8_t* t, uint32_t n, int k, int* w, int* h, int* bpp, std::vector<uint8_t>& idx, std::vector<uint8_t>& pal) {
    if (n < 0x20 || u32(t + 4) != 8) return false;
    uint32_t cnt = u32(t);
    if (k < 0 || (uint32_t)k >= cnt || 32 + 64 * (uint32_t)(k + 1) > n) return false;
    const uint8_t* e = t + 32 + 64 * k;
    uint32_t tp = u32(e) * 4 + 96, pp = u32(e + 4) * 4 + 96, tl = u32(e + 8), pl = u32(e + 12);
    uint64_t tex0; memcpy(&tex0, e + 0x30, 8);
    int psm = (int)((tex0 >> 20) & 0x3F);
    long W = 1L << ((tex0 >> 26) & 0xF), H = 1L << ((tex0 >> 30) & 0xF);
    *w = (int)W; *h = (int)H;
    if (!tex0 || tl <= 128 || tp + (uint64_t)tl - 128 > n) return false;
    std::vector<uint8_t> raw(t + tp, t + tp + tl - 128), p0;
    if (psm != 0) { if (pl <= 128 || pp + (uint64_t)pl - 128 > n) return false; p0.assign(t + pp, t + pp + pl - 128); }
    idx.assign((size_t)W * H, 0);
    if (psm == 20) {
        std::vector<uint8_t> s = interlace(reorder_px(raw, W, H)), s3(s.size() * 2);
        for (size_t i = 0; i < s.size(); i++) { s3[2 * i] = s[i] & 15; s3[2 * i + 1] = s[i] >> 4; }
        std::vector<uint8_t> s4 = bmp_order(s3, W, H);
        for (long y = 0; y < H; y++)
            for (long x = 0; x < W; x++) { size_t i = (size_t)((H - 1 - y) * W + x); idx[(size_t)y * W + x] = (i < s4.size() ? s4[i] : 0) & 15; }
        *bpp = 4; pal.assign(64, 0);
        memcpy(pal.data(), p0.data(), std::min<size_t>(64, p0.size()));
        return true;
    }
    if (psm == 19) {
        std::vector<uint8_t> s = bmp_order(raw, W, H), p = reorder_pal(p0);
        for (long y = 0; y < H; y++)
            for (long x = 0; x < W; x++) { size_t i = (size_t)(y * W + x); idx[(size_t)(H - 1 - y) * W + x] = i < s.size() ? s[i] : 0; }
        *bpp = 8; pal.assign(1024, 0);
        memcpy(pal.data(), p.data(), std::min<size_t>(1024, p.size()));
        return true;
    }
    if (psm != 0) return false;
    // CT32: up to 256 colours kept exact, more are reduced by dropping low bits until they fit
    std::vector<uint32_t> px((size_t)W * H, 0);
    for (size_t i = 0; i < px.size() && 4 * i + 4 <= raw.size(); i++) memcpy(&px[i], &raw[4 * i], 4);
    for (int drop = 0; drop < 8; drop++) {
        uint32_t m = 0xFFu >> drop << drop; m |= m << 8 | m << 16 | m << 24;
        std::map<uint32_t, int> c;
        for (auto v : px) { c.emplace(v & m, (int)c.size()); if (c.size() > 256) break; }
        if (c.size() > 256) continue;
        pal.assign(1024, 0);
        for (auto& kv : c) memcpy(&pal[4 * kv.second], &kv.first, 4);
        for (size_t i = 0; i < px.size(); i++) idx[i] = (uint8_t)c[px[i] & m];
        *bpp = 8;
        return true;
    }
    return false;
}

// every texture key the P file uses (counts of the face and blink CTEX given by the caller)
static void keys_of(const File& f, TexKeys& tk) {
    std::vector<uint8_t> o;
    for (int num = 2; num <= 10; num++) ttt_entry(f, num, o, &tk);
    for (int j = 0; j < tk.count[12] && j < 16; j++) tk.keys[KEY_BLINK + j] = {12, j};
}

// ---------------- animations (BT3 .canm = BPE, TTT .tanm = zlib; same ANM inside) ----------------
static bool bpe_decode(const uint8_t* d, uint32_t n, uint32_t size, std::vector<uint8_t>& out) {
    out.clear(); out.reserve(size);
    uint32_t pos = 0;
    uint8_t left[256], right[256], stack[512];
    while (pos < n && out.size() < size) {
        for (int i = 0; i < 256; i++) { left[i] = (uint8_t)i; right[i] = 0; }
        int c = 0, count = d[pos++];
        for (;;) {
            if (count > 127) { c += count - 127; count = 0; }
            if (c == 256) break;
            for (int k = 0; k <= count && c < 256; k++, c++) {
                if (pos >= n) return false;
                left[c] = d[pos++];
                if (c != left[c]) { if (pos >= n) return false; right[c] = d[pos++]; }
            }
            if (c == 256) break;
            if (pos >= n) return false;
            count = d[pos++];
        }
        if (pos + 2 > n) return false;
        uint32_t blk = 256u * d[pos] + d[pos + 1]; pos += 2;
        int sp = 0;
        for (uint32_t i = 0; i < blk || sp;) {
            uint8_t ch;
            if (sp) ch = stack[--sp];
            else { if (pos >= n) return false; ch = d[pos++]; i++; }
            if (ch == left[ch]) out.push_back(ch);
            else { if (sp > 508) return false; stack[sp++] = right[ch]; stack[sp++] = left[ch]; }
        }
    }
    out.resize(size, 0);
    return true;
}

struct BitOut {
    std::vector<uint8_t>& o; uint32_t acc = 0; int nb = 0;
    explicit BitOut(std::vector<uint8_t>& v) : o(v) {}
    void put(uint32_t v, int n) { acc |= v << nb; nb += n; while (nb >= 8) { o.push_back((uint8_t)acc); acc >>= 8; nb -= 8; } }
    void flush() { if (nb) { o.push_back((uint8_t)acc); acc = 0; nb = 0; } }
};
static uint32_t bit_rev(uint32_t c, int n) { uint32_t r = 0; for (int i = 0; i < n; i++) { r = r << 1 | (c & 1); c >>= 1; } return r; }

// code lengths limited to lim bits (frequencies halved until it fits)
static void huff_lengths(const std::vector<uint32_t>& f0, int lim, std::vector<uint8_t>& len) {
    size_t n = f0.size();
    std::vector<uint32_t> f = f0;
    len.assign(n, 0);
    for (;;) {
        std::vector<int> sym;
        for (size_t i = 0; i < n; i++) if (f[i]) sym.push_back((int)i);
        if (sym.empty()) return;
        if (sym.size() == 1) { len[sym[0]] = 1; return; }
        std::vector<uint64_t> w; std::vector<int> par;
        typedef std::pair<uint64_t, int> E;
        std::priority_queue<E, std::vector<E>, std::greater<E>> q;
        for (int s : sym) { q.push({f[s], (int)w.size()}); w.push_back(f[s]); par.push_back(-1); }
        while (q.size() > 1) {
            E a = q.top(); q.pop(); E b = q.top(); q.pop();
            int id = (int)w.size(); w.push_back(a.first + b.first); par.push_back(-1);
            par[a.second] = id; par[b.second] = id;
            q.push({a.first + b.first, id});
        }
        int mx = 0;
        for (size_t k = 0; k < sym.size(); k++) { int d = 0; for (int p = (int)k; par[p] >= 0; p = par[p]) d++; len[sym[k]] = (uint8_t)d; mx = std::max(mx, d); }
        if (mx <= lim) return;
        for (auto& x : f) if (x) x = (x + 1) >> 1;
    }
}
static void huff_codes(const std::vector<uint8_t>& len, std::vector<uint16_t>& code) {
    int bl[16] = {0}, next[16] = {0};
    for (auto l : len) if (l) bl[l]++;
    for (int b = 1, c = 0; b < 16; b++) { c = (c + bl[b - 1]) << 1; next[b] = c; }
    code.assign(len.size(), 0);
    for (size_t i = 0; i < len.size(); i++) if (len[i]) code[i] = (uint16_t)bit_rev((uint32_t)next[len[i]]++, len[i]);
}

static const uint16_t LBASE[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const uint8_t LEXT[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const uint16_t DBASE[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const uint8_t DEXT[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

struct Sym { uint16_t lit, dist; };   // dist 0 = literal byte lit; else length lit
static int len_code(int l) { int c = 28; while (LBASE[c] > l) c--; return c; }
static int dist_code(int d) { int c = 29; while (DBASE[c] > d) c--; return c; }

static void deflate_block(BitOut& bo, const std::vector<Sym>& s, const uint8_t* raw, uint32_t rn, bool last) {
    std::vector<uint32_t> lf(286, 0), df(30, 0);
    for (auto& x : s) { if (!x.dist) lf[x.lit]++; else { lf[257 + len_code(x.lit)]++; df[dist_code(x.dist)]++; } }
    lf[256] = 1;
    std::vector<uint8_t> ll, dl;
    huff_lengths(lf, 15, ll); huff_lengths(df, 15, dl);
    if (std::all_of(dl.begin(), dl.end(), [](uint8_t v) { return v == 0; })) dl[0] = 1;
    int hlit = 286; while (hlit > 257 && !ll[hlit - 1]) hlit--;
    int hdist = 30; while (hdist > 1 && !dl[hdist - 1]) hdist--;
    std::vector<uint8_t> all(ll.begin(), ll.begin() + hlit);
    all.insert(all.end(), dl.begin(), dl.begin() + hdist);
    std::vector<std::pair<int, int>> rle;
    for (size_t i = 0; i < all.size();) {
        size_t j = i; while (j < all.size() && all[j] == all[i]) j++;
        size_t run = j - i;
        if (all[i] == 0) {
            while (run >= 11) { int r = (int)std::min<size_t>(run, 138); rle.push_back({18, r - 11}); run -= r; }
            if (run >= 3) { rle.push_back({17, (int)run - 3}); run = 0; }
            while (run--) rle.push_back({0, 0});
        } else {
            rle.push_back({all[i], 0}); run--;
            while (run >= 3) { int r = (int)std::min<size_t>(run, 6); rle.push_back({16, r - 3}); run -= r; }
            while (run--) rle.push_back({all[i], 0});
        }
        i = j;
    }
    std::vector<uint32_t> cf(19, 0);
    for (auto& r : rle) cf[r.first]++;
    std::vector<uint8_t> cl; huff_lengths(cf, 7, cl);
    static const uint8_t ORD[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    int hclen = 19; while (hclen > 4 && !cl[ORD[hclen - 1]]) hclen--;
    uint64_t dyn = 17 + 3 * hclen, fix = 3, sto = 0;
    for (auto& r : rle) dyn += cl[r.first] + (r.first == 16 ? 2 : r.first == 17 ? 3 : r.first == 18 ? 7 : 0);
    for (int i = 0; i < 286; i++) if (lf[i]) { dyn += (uint64_t)lf[i] * ll[i]; fix += (uint64_t)lf[i] * (i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8); }
    for (int i = 0; i < 30; i++) if (df[i]) { dyn += (uint64_t)df[i] * dl[i]; fix += (uint64_t)df[i] * 5; }
    for (int i = 0; i < 29; i++) { uint32_t c = lf[257 + i]; dyn += (uint64_t)c * LEXT[i]; fix += (uint64_t)c * LEXT[i]; }
    for (int i = 0; i < 30; i++) { dyn += (uint64_t)df[i] * DEXT[i]; fix += (uint64_t)df[i] * DEXT[i]; }
    sto = 3 + 7 + ((uint64_t)rn + 5 * ((rn + 65534) / 65535 + 1)) * 8;
    if (sto < dyn && sto < fix) {
        uint32_t p = 0;
        do {
            uint32_t k = std::min<uint32_t>(rn - p, 65535);
            bo.put((last && p + k == rn) ? 1 : 0, 1); bo.put(0, 2); bo.flush();
            bo.o.push_back((uint8_t)k); bo.o.push_back((uint8_t)(k >> 8)); bo.o.push_back((uint8_t)~k); bo.o.push_back((uint8_t)(~k >> 8));
            bo.o.insert(bo.o.end(), raw + p, raw + p + k);
            p += k;
        } while (p < rn);
        return;
    }
    std::vector<uint16_t> lc, dc;
    if (fix <= dyn) {
        ll.assign(288, 8); for (int i = 144; i < 256; i++) ll[i] = 9; for (int i = 256; i < 280; i++) ll[i] = 7;
        dl.assign(30, 5);
        bo.put(last, 1); bo.put(1, 2);
    } else {
        std::vector<uint16_t> cc; huff_codes(cl, cc);
        bo.put(last, 1); bo.put(2, 2);
        bo.put(hlit - 257, 5); bo.put(hdist - 1, 5); bo.put(hclen - 4, 4);
        for (int i = 0; i < hclen; i++) bo.put(cl[ORD[i]], 3);
        for (auto& r : rle) {
            bo.put(cc[r.first], cl[r.first]);
            if (r.first == 16) bo.put(r.second, 2); else if (r.first == 17) bo.put(r.second, 3); else if (r.first == 18) bo.put(r.second, 7);
        }
    }
    huff_codes(ll, lc); huff_codes(dl, dc);
    for (auto& x : s) {
        if (!x.dist) { bo.put(lc[x.lit], ll[x.lit]); continue; }
        int c = len_code(x.lit), d = dist_code(x.dist);
        bo.put(lc[257 + c], ll[257 + c]); bo.put(x.lit - LBASE[c], LEXT[c]);
        bo.put(dc[d], dl[d]); bo.put(x.dist - DBASE[d], DEXT[d]);
    }
    bo.put(lc[256], ll[256]);
}

// zlib stream (header 78 DA, Adler-32), LZ77 with hash chains and lazy matching
static void zlib_compress(const uint8_t* d, uint32_t n, std::vector<uint8_t>& o) {
    o.clear(); o.push_back(0x78); o.push_back(0xDA);
    BitOut bo(o);
    const int WB = 15, W = 1 << WB, HB = 15, CHAIN = 256;
    std::vector<int32_t> head(1 << HB, -1), prev(W, -1);
    auto hash = [&](uint32_t i) { return ((uint32_t)d[i] << 10 ^ (uint32_t)d[i + 1] << 5 ^ d[i + 2]) & ((1 << HB) - 1); };
    auto insert = [&](uint32_t i) { if (i + 2 < n) { uint32_t h = hash(i); prev[i & (W - 1)] = head[h]; head[h] = (int32_t)i; } };
    auto best = [&](uint32_t i, int& bd) {
        int bl = 0;
        if (i + 2 >= n) return 0;
        int32_t p = head[hash(i)];
        int lim = (int)std::min<uint32_t>(258, n - i);
        for (int k = 0; k < CHAIN && p >= 0 && i - (uint32_t)p <= (uint32_t)(W - 262); k++) {
            if (d[p + bl] == d[i + bl]) {
                int l = 0; while (l < lim && d[p + l] == d[i + l]) l++;
                if (l > bl) { bl = l; bd = (int)(i - p); if (l >= lim) break; }
            }
            int32_t q = prev[p & (W - 1)];
            if (q >= p) break;
            p = q;
        }
        return bl >= 3 ? bl : 0;
    };
    std::vector<Sym> s;
    uint32_t i = 0, bstart = 0, ins = 0;
    auto upto = [&](uint32_t e) { for (; ins < e; ins++) insert(ins); };
    while (i < n) {
        upto(i);
        int d1 = 0, l1 = best(i, d1);
        if (l1 && l1 < 32 && i + 1 < n) {
            upto(i + 1);
            int d2 = 0, l2 = best(i + 1, d2);
            if (l2 > l1) { s.push_back({d[i], 0}); i++; continue; }
        }
        if (l1) { s.push_back({(uint16_t)l1, (uint16_t)d1}); i += l1; }
        else { s.push_back({d[i], 0}); i++; }
        if (s.size() >= 16384 && i < n) { deflate_block(bo, s, d + bstart, i - bstart, false); s.clear(); bstart = i; }
    }
    deflate_block(bo, s, d + bstart, n - bstart, true);
    bo.flush();
    uint32_t a = 1, b = 0;
    for (uint32_t k = 0; k < n; k++) { a = (a + d[k]) % 65521; b = (b + a) % 65521; }
    uint32_t ad = b << 16 | a;
    for (int k = 3; k >= 0; k--) o.push_back((uint8_t)(ad >> (8 * k)));
}

// BT3 <name>_anm.pak: LE index of 440 entries; entry i = TTT 2_anims entry i
static const uint32_t ANM_COUNT = 440;
static bool is_anm(const uint8_t* h, size_t n) {
    if (n < 12) return false;
    uint32_t c = u32(h), o0 = u32(h + 4);
    return c == ANM_COUNT && o0 >= 4 + 4 * (c + 1) && o0 <= 0x1000 && (o0 & 15) == 0;
}
static bool pak_entry(const std::vector<uint8_t>& d, uint32_t i, const uint8_t** p, uint32_t* sz) {
    if (d.size() < 8 || i >= u32(d.data()) || 8 + 4 * (size_t)i + 4 > d.size()) return false;
    uint32_t a = u32(&d[4 + 4 * i]), b = u32(&d[8 + 4 * i]);
    if (a > b || b > d.size()) return false;
    *p = d.data() + a; *sz = b - a;
    return true;
}

// BT3 .canm (LE size, LE BPE size, BPE) -> TTT .tanm (BE size, BE zlib size, zlib); a .tanm passes as is; no valid header = empty
static int anim_tanm(const uint8_t* p, uint32_t n, bool tanm, std::vector<uint8_t>& o) {
    o.clear();
    if (tanm) { o.assign(p, p + n); return n ? 2 : 0; }
    if (n < 8 || !u32(p) || u32(p) >= (16u << 20) || !u32(p + 4) || 8 + (uint64_t)u32(p + 4) > n) return 0;
    std::vector<uint8_t> raw, z;
    if (!bpe_decode(p + 8, u32(p + 4), u32(p), raw)) return -1;
    zlib_compress(raw.data(), (uint32_t)raw.size(), z);
    uint32_t a = (uint32_t)raw.size(), b = (uint32_t)z.size();
    o.resize(8);
    for (int k = 0; k < 4; k++) { o[k] = (uint8_t)(a >> (24 - 8 * k)); o[4 + k] = (uint8_t)(b >> (24 - 8 * k)); }
    o.insert(o.end(), z.begin(), z.end());
    return 1;
}


// ---------------- effects (BT3 <name>_eff.pak -> TTT 3_effects; same files, LE index -> BE, .dbt -> .atex) ----------------
static const uint32_t EFF_COUNT = 6;
static bool is_eff(const uint8_t* h, size_t n) {
    if (n < 12) return false;
    uint32_t c = u32(h), o0 = u32(h + 4);
    return c == EFF_COUNT && o0 >= 4 + 4 * (c + 1) && o0 <= 0x1000 && (o0 & 15) == 0;
}
typedef std::vector<std::pair<const uint8_t*, uint32_t>> Parts;
static bool le_pak(const uint8_t* d, uint32_t n, Parts& e) {
    e.clear();
    if (n < 8) return false;
    uint32_t c = u32(d);
    if (!c || c > 4096 || 4 + 4 * ((uint64_t)c + 1) > n) return false;
    uint32_t prev = u32(d + 4);
    if (prev < 4 + 4 * (c + 1)) return false;
    for (uint32_t i = 0; i < c; i++) {
        uint32_t a = u32(d + 4 + 4 * i), b = u32(d + 8 + 4 * i);
        if (a > b || b > n) return false;
        e.push_back({d + a, b - a});
    }
    return true;
}
// TTT pak: BE count and N+1 offsets, body from the next multiple of 64, each part padded to 64
static void be_pak(const std::vector<std::vector<uint8_t>>& parts, std::vector<uint8_t>& o) {
    uint32_t n = (uint32_t)parts.size(), start = (4 + 4 * (n + 1) + 63) / 64 * 64;
    o.assign(start, 0);
    auto put = [&](size_t at, uint32_t v) { for (int k = 0; k < 4; k++) o[at + k] = (uint8_t)(v >> (24 - 8 * k)); };
    put(0, n);
    for (uint32_t i = 0; i <= n; i++) {
        put(4 + 4 * i, (uint32_t)o.size());
        if (i == n) break;
        o.insert(o.end(), parts[i].begin(), parts[i].end());
        o.resize((o.size() + 63) / 64 * 64, 0);
    }
}
static bool is_ctex(const uint8_t* t, uint32_t n) {
    if (n < 0x60 || u32(t + 4) != 8) return false;
    uint32_t c = u32(t);
    if (!c || c > 64 || 32 + 64 * c > n) return false;
    for (uint32_t k = 0; k < c; k++) {
        uint64_t tex0; memcpy(&tex0, t + 32 + 64 * k + 0x30, 8);
        int psm = (int)((tex0 >> 20) & 0x3F);
        if (!tex0 || (psm != 0 && psm != 19 && psm != 20)) return false;
    }
    return true;
}
static void atex_swizzle(const std::vector<uint8_t>& lin, uint32_t step, uint32_t h, std::vector<uint8_t>& o) {
    if (step % 16 || h % 8) { o = lin; return; }
    o.assign(lin.size(), 0);
    size_t k = 0;
    for (uint32_t by = 0; by < h; by += 8)
        for (uint32_t bx = 0; bx < step / 16; bx++)
            for (uint32_t r = 0; r < 8; r++, k += 16) memcpy(&o[k], &lin[(size_t)(by + r) * step + bx * 16], 16);
}
// .dbt (CTEX) -> .atex as TTT writes it (open_paks.construir_atex), the BT3 size and palette kept
// clut8: every image as CLUT8 (models: the game draws them only as T8); only: first N images
static bool ctex_to_atex(const uint8_t* t, uint32_t n, std::vector<uint8_t>& o, bool clut8 = false, uint32_t only = 0) {
    uint32_t c = u32(t);
    if (only && only < c) c = only;
    o.assign(0x20 + 0x60 * c, 0);
    auto p32 = [&](size_t at, uint32_t v) { memcpy(&o[at], &v, 4); };
    std::vector<uint8_t> blobs;
    for (uint32_t k = 0; k < c; k++) {
        int w, h, bpp; std::vector<uint8_t> idx, pal, lin, sw;
        if (!ctex_indexed(t, n, (int)k, &w, &h, &bpp, idx, pal)) return false;
        if (clut8 && bpp == 4) { bpp = 8; pal.resize(1024, 0); }
        if (bpp == 4) { lin.resize((size_t)w * h / 2); for (size_t i = 0; i < lin.size(); i++) lin[i] = (uint8_t)((idx[2 * i] & 15) | (idx[2 * i + 1] & 15) << 4); }
        else lin = idx;
        atex_swizzle(lin, bpp == 4 ? w / 2 : w, h, sw);
        uint32_t cur = (uint32_t)(0x20 + 0x60 * c + blobs.size()), e = 0x20 + 0x60 * k;
        p32(e, cur / 4); p32(e + 4, (cur + (uint32_t)sw.size()) / 4); p32(e + 8, (uint32_t)sw.size()); p32(e + 12, (uint32_t)pal.size());
        p32(e + 16, (uint32_t)pal.size() / 4); p32(e + 20, bpp == 4 ? 1 : 4);
        int lw = 0, lh = 0; while ((1 << lw) < w) lw++; while ((1 << lh) < h) lh++;
        o[e + 0x28] = (uint8_t)lh; o[e + 0x29] = (uint8_t)lw; o[e + 0x2B] = bpp == 4 ? 8 : 9;
        memset(&o[e + 0x34], 0xFF, 12);
        blobs.insert(blobs.end(), sw.begin(), sw.end());
        blobs.insert(blobs.end(), pal.begin(), pal.end());
    }
    p32(0, c); p32(4, 8); p32(8, u32(&o[0x30])); p32(12, u32(&o[0x34]));
    o.insert(o.end(), blobs.begin(), blobs.end());
    return true;
}
struct EffStats { int paks = 0, tex = 0, bad = 0, models = 0; };
// one VFX pak (00 model pak, 01 cma, 02 ground, 03 main.dat, then its files): only the textures change
static bool vfx_pak(const uint8_t* d, uint32_t n, std::vector<uint8_t>& o, EffStats& st) {
    o.clear();
    if (!n) { be_pak(std::vector<std::vector<uint8_t>>(1), o); return true; }   // TTT keeps unused VFX as a pak of one empty entry
    Parts e;
    if (!le_pak(d, n, e)) return false;
    std::vector<std::vector<uint8_t>> parts(e.size());
    for (size_t i = 0; i < e.size(); i++) {
        const uint8_t* p = e[i].first; uint32_t m = e[i].second;
        if (i == 0 && m) {   // model pak of the effect (a P file): the game reads only 002 model and 011 textures
            Parts me;
            if (le_pak(p, m, me)) {
                std::vector<std::vector<uint8_t>> mp;
                for (auto& x : me) mp.emplace_back(x.first, x.first + x.second);
                std::vector<uint8_t> mo, ao;
                TexKeys tk;
                if (me.size() > 2 && me[2].second >= 4 && !memcmp(me[2].first, "pmdl", 4)) {
                    if (model(me[2].first, me[2].second, mo, &tk)) {
                        for (uint32_t k = 0, pt = u32(&mo[0x60]); k < u32(&mo[0x5C]); k++) { mo[pt + 32 * k + 0x10] = 0; mo[pt + 32 * k + 0x11] = 0; }   // the game binds the texture
                        mp[2] = std::move(mo); st.models++;
                    } else st.bad++;
                }
                if (me.size() > 11 && is_ctex(me[11].first, me[11].second)) {
                    uint32_t used = tk.keys.empty() ? 0 : (uint32_t)tk.keys.rbegin()->first + 1;   // the textures the model uses (TTT: one CLUT8)
                    if (ctex_to_atex(me[11].first, me[11].second, ao, true, used)) { mp[11] = std::move(ao); st.tex++; }
                    else st.bad++;
                }
                be_pak(mp, parts[i]);
                continue;
            }
        }
        if (i >= 3 && is_ctex(p, m)) {
            if (ctex_to_atex(p, m, parts[i])) { st.tex++; continue; }
            st.bad++;
        }
        parts[i].assign(p, p + m);
    }
    be_pak(parts, o);
    st.paks++;
    return true;
}
// effect_common (00 param, 07 cameras, the rest VFX) from BT3 (LE) to TTT
static bool common_pak(const uint8_t* d, uint32_t n, std::vector<uint8_t>& o, EffStats& st) {
    Parts ec;
    o.clear();
    if (!le_pak(d, n, ec)) return false;
    std::vector<std::vector<uint8_t>> parts(ec.size());
    for (size_t j = 0; j < ec.size(); j++) {
        const uint8_t* p = ec[j].first; uint32_t m = ec[j].second;
        Parts cam;
        if (j == 0 || (j == 7 && !m)) parts[j].assign(p, p + m);
        else if (j == 7 && le_pak(p, m, cam)) { std::vector<std::vector<uint8_t>> cp; for (auto& x : cam) cp.emplace_back(x.first, x.first + x.second); be_pak(cp, parts[j]); }
        else if (!vfx_pak(p, m, parts[j], st)) { st.bad++; parts[j].assign(p, p + m); }
    }
    be_pak(parts, o);
    return true;
}

// TTT 3_effects 0..4 = skill_001..005, 5 = effect_common (00 param, 07 cameras, the rest VFX), 6 = patch_param (empty, or the opened folder's file)
static bool eff_entries(const std::vector<uint8_t>& d, std::vector<uint8_t> (&out)[7], EffStats& st) {
    Parts top;
    for (auto& x : out) x.clear();
    if (!le_pak(d.data(), (uint32_t)d.size(), top) || (top.size() != EFF_COUNT && top.size() != EFF_COUNT + 1)) return false;
    if (top.size() > EFF_COUNT) out[6].assign(top[6].first, top[6].first + top[6].second);   // the opened folder's own 06_patch_param.dat
    for (int i = 0; i < 5; i++) if (!vfx_pak(top[i].first, top[i].second, out[i], st)) st.bad++;
    if (top[5].second && !common_pak(top[5].first, top[5].second, out[5], st)) st.bad++;
    return true;
}

}  // namespace bt3
