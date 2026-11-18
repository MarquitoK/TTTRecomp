#pragma once
// Minimal PNG reader (own inflate): 8 and 16 bits, grey, RGB, palette, with alpha and tRNS, no interlacing.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

struct Inflater {
    const uint8_t* d; size_t n, pos; uint32_t bits; int nbits;
    std::vector<uint8_t>& outp;
    Inflater(const uint8_t* p, size_t tam, std::vector<uint8_t>& o) : d(p), n(tam), pos(0), bits(0), nbits(0), outp(o) {}
    int bit() { if (!nbits) { if (pos >= n) return -1; bits = d[pos++]; nbits = 8; } int b = bits & 1; bits >>= 1; nbits--; return b; }
    int read(int k) { int v = 0; for (int i = 0; i < k; i++) { int b = bit(); if (b < 0) return -1; v |= b << i; } return v; }
    struct Huff { uint16_t counter[16], sim[320]; };
    static bool build(Huff& h, const uint8_t* lengths, int n) {
        memset(h.counter, 0, sizeof(h.counter));
        for (int i = 0; i < n; i++) h.counter[lengths[i]]++;
        h.counter[0] = 0;
        uint16_t shift_amt[16]; shift_amt[1] = 0;
        for (int i = 1; i < 15; i++) shift_amt[i + 1] = shift_amt[i] + h.counter[i];
        for (int i = 0; i < n; i++) if (lengths[i]) h.sim[shift_amt[lengths[i]]++] = (uint16_t)i;
        return true;
    }
    int decoded(const Huff& h) {
        int code = 0, first_one = 0, idx = 0;
        for (int l = 1; l < 16; l++) {
            int b = bit(); if (b < 0) return -1;
            code |= b; int c = h.counter[l];
            if (code - c < first_one) return h.sim[idx + (code - first_one)];
            idx += c; first_one += c; first_one <<= 1; code <<= 1;
        }
        return -1;
    }
    bool block(const Huff& lit, const Huff& dist) {
        static const uint16_t LB[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
        static const uint8_t LE[] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
        static const uint16_t DB[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
        static const uint8_t DE[] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
        for (;;) {
            int s = decoded(lit);
            if (s < 0) return false;
            if (s < 256) { outp.push_back((uint8_t)s); continue; }
            if (s == 256) return true;
            s -= 257; if (s >= 29) return false;
            int e = read(LE[s]); if (e < 0) return false;
            int length = LB[s] + e;
            int ds = decoded(dist); if (ds < 0 || ds >= 30) return false;
            e = read(DE[ds]); if (e < 0) return false;
            size_t d0 = (size_t)DB[ds] + (size_t)e;
            if (d0 > outp.size()) return false;
            size_t base = outp.size() - d0;
            for (int i = 0; i < length; i++) outp.push_back(outp[base + i]);
        }
    }
    bool run(bool zlib = true) {
        if (zlib && n < 2) return false;
        pos = zlib ? 2 : 0;   // zlib header or raw deflate (zip)
        for (;;) {
            int end_v = bit(), type = read(2);
            if (end_v < 0 || type < 0 || type == 3) return false;
            if (type == 0) {
                bits = 0; nbits = 0;
                if (pos + 4 > n) return false;
                uint32_t len = d[pos] | (d[pos + 1] << 8); pos += 4;
                if (pos + len > n) return false;
                outp.insert(outp.end(), d + pos, d + pos + len); pos += len;
            } else {
                Huff lit, dist; uint8_t l[320];
                if (type == 1) {
                    int i = 0;
                    for (; i < 144; i++) l[i] = 8;
                    for (; i < 256; i++) l[i] = 9;
                    for (; i < 280; i++) l[i] = 7;
                    for (; i < 288; i++) l[i] = 8;
                    build(lit, l, 288);
                    for (i = 0; i < 30; i++) l[i] = 5;
                    build(dist, l, 30);
                } else {
                    static const uint8_t ORD[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                    int hlit = read(5) + 257, hdist = read(5) + 1, hclen = read(4) + 4;
                    if (hlit > 286 || hdist > 30) return false;
                    uint8_t cl[19] = {0};
                    for (int i = 0; i < hclen; i++) { int v = read(3); if (v < 0) return false; cl[ORD[i]] = (uint8_t)v; }
                    Huff hc; build(hc, cl, 19);
                    int i = 0;
                    while (i < hlit + hdist) {
                        int s = decoded(hc); if (s < 0) return false;
                        if (s < 16) { l[i++] = (uint8_t)s; continue; }
                        int rep, val = 0;
                        if (s == 16) { if (!i) return false; val = l[i - 1]; rep = 3 + read(2); }
                        else if (s == 17) rep = 3 + read(3);
                        else rep = 11 + read(7);
                        if (i + rep > hlit + hdist) return false;
                        while (rep--) l[i++] = (uint8_t)val;
                    }
                    build(lit, l, hlit);
                    build(dist, l + hlit, hdist);
                }
                if (!block(lit, dist)) return false;
            }
            if (end_v) return true;
        }
    }
};

static inline int png_paeth(int a, int b, int c) {
    int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

// returns RGBA8 (R in the low byte) or an error message
static bool png_decode(const std::vector<uint8_t>& f, int* w, int* h, std::vector<uint32_t>& px, std::string* err, std::vector<uint8_t>* idxs = nullptr, std::vector<uint32_t>* palette = nullptr) {
    static const uint8_t FIRMA[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (f.size() < 33 || memcmp(f.data(), FIRMA, 8)) { *err = "no es PNG"; return false; }
    auto u32 = [&](size_t p) { return ((uint32_t)f[p] << 24) | ((uint32_t)f[p + 1] << 16) | ((uint32_t)f[p + 2] << 8) | f[p + 3]; };
    int W = 0, H = 0, depth = 0, type = 0, interleaved = 0;
    std::vector<uint8_t> idat, pal, trns;
    for (size_t p = 8; p + 8 <= f.size();) {
        uint32_t n = u32(p); const uint8_t* t = &f[p + 4]; size_t body = p + 8;
        if (body + n > f.size()) break;
        if (!memcmp(t, "IHDR", 4) && n >= 13) { W = (int)u32(body); H = (int)u32(body + 4); depth = f[body + 8]; type = f[body + 9]; interleaved = f[body + 12]; }
        else if (!memcmp(t, "PLTE", 4)) pal.assign(&f[body], &f[body] + n);
        else if (!memcmp(t, "tRNS", 4)) trns.assign(&f[body], &f[body] + n);
        else if (!memcmp(t, "IDAT", 4)) idat.insert(idat.end(), &f[body], &f[body] + n);
        else if (!memcmp(t, "IEND", 4)) break;
        p = body + n + 4;
    }
    if (W <= 0 || H <= 0 || W > 16384 || H > 16384) { *err = "dimensiones"; return false; }
    if (interleaved) { *err = "PNG entrelazado (guardalo sin entrelazar)"; return false; }
    int channels = type == 0 ? 1 : type == 2 ? 3 : type == 3 ? 1 : type == 4 ? 2 : type == 6 ? 4 : 0;
    if (!channels || (depth != 8 && depth != 16 && !(type == 3 && (depth == 1 || depth == 2 || depth == 4)) && !(type == 0 && (depth == 1 || depth == 2 || depth == 4)))) { *err = "tipo de color no soportado"; return false; }
    std::vector<uint8_t> cr;
    cr.reserve((size_t)(W * channels * depth / 8 + 1) * H);
    Inflater inf(idat.data(), idat.size(), cr);
    if (!inf.run()) { *err = "datos comprimidos corruptos"; return false; }
    int bpp = (channels * depth + 7) / 8;
    size_t row = ((size_t)W * channels * depth + 7) / 8;
    if (cr.size() < (row + 1) * H) { *err = "datos incompletos"; return false; }
    std::vector<uint8_t> prev(row, 0), cur(row);
    px.resize((size_t)W * H);
    bool with_idx = idxs && palette && type == 3;
    if (with_idx) {
        idxs->resize((size_t)W * H);
        palette->resize(pal.size() / 3);
        for (size_t i = 0; i < palette->size(); i++)
            (*palette)[i] = pal[i * 3] | (pal[i * 3 + 1] << 8) | (pal[i * 3 + 2] << 16) | ((uint32_t)(i < trns.size() ? trns[i] : 255) << 24);
    } else if (idxs) idxs->clear();
    for (int y = 0; y < H; y++) {
        const uint8_t* s = &cr[(row + 1) * y];
        int ft = s[0]; s++;
        for (size_t i = 0; i < row; i++) {
            int a = i >= (size_t)bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= (size_t)bpp ? prev[i - bpp] : 0, x = s[i];
            switch (ft) {
            case 1: x += a; break;
            case 2: x += b; break;
            case 3: x += (a + b) / 2; break;
            case 4: x += png_paeth(a, b, c); break;
            default: break;
            }
            cur[i] = (uint8_t)x;
        }
        for (int x = 0; x < W; x++) {
            int r, g, bl, al = 255;
            if (depth < 8) {
                int idx = (cur[(size_t)x * depth / 8] >> (8 - depth - (x * depth) % 8)) & ((1 << depth) - 1);
                if (with_idx) (*idxs)[(size_t)y * W + x] = (uint8_t)idx;
                if (type == 3) {
                    r = (size_t)idx * 3 + 2 < pal.size() ? pal[idx * 3] : 0; g = (size_t)idx * 3 + 2 < pal.size() ? pal[idx * 3 + 1] : 0; bl = (size_t)idx * 3 + 2 < pal.size() ? pal[idx * 3 + 2] : 0;
                    if ((size_t)idx < trns.size()) al = trns[idx];
                } else { r = g = bl = idx * 255 / ((1 << depth) - 1); }
            } else {
                const uint8_t* q = &cur[(size_t)x * bpp];
                int step = depth / 8;
                auto ch = [&](int k) { return (int)q[k * step]; };
                if (type == 3) {
                    int idx = ch(0);
                    if (with_idx) (*idxs)[(size_t)y * W + x] = (uint8_t)idx;
                    r = (size_t)idx * 3 + 2 < pal.size() ? pal[idx * 3] : 0; g = (size_t)idx * 3 + 2 < pal.size() ? pal[idx * 3 + 1] : 0; bl = (size_t)idx * 3 + 2 < pal.size() ? pal[idx * 3 + 2] : 0;
                    if ((size_t)idx < trns.size()) al = trns[idx];
                } else if (channels <= 2) {
                    r = g = bl = ch(0);
                    if (channels == 2) al = ch(1);
                    else if (trns.size() >= 2 && ((trns[0] << 8) | trns[1]) == (depth == 16 ? ((q[0] << 8) | q[1]) : q[0])) al = 0;
                } else {
                    r = ch(0); g = ch(1); bl = ch(2);
                    if (channels == 4) al = ch(3);
                    else if (trns.size() >= 6 && depth == 8 && trns[1] == q[0] && trns[3] == q[1] && trns[5] == q[2]) al = 0;
                }
            }
            px[(size_t)y * W + x] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)bl << 16) | ((uint32_t)al << 24);
        }
        std::swap(prev, cur);
    }
    *w = W; *h = H;
    return true;
}

// shrinks an RGBA image to w x h (box average, or nearest when growing) and indexes it with up to n colours
static void img_index(const uint32_t* px, int W, int H, int w, int h, int n, std::vector<uint8_t>& idx, std::vector<uint32_t>& pal) {
    std::vector<uint32_t> r((size_t)w * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int x0 = x * W / w, x1 = (x + 1) * W / w, y0 = y * H / h, y1 = (y + 1) * H / h;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            uint64_t s[4] = {0, 0, 0, 0}, c = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) { uint32_t p = px[(size_t)yy * W + xx]; for (int k = 0; k < 4; k++) s[k] += (p >> (8 * k)) & 255; c++; }
            uint32_t v = 0;
            for (int k = 0; k < 4; k++) v |= (uint32_t)((s[k] + c / 2) / c) << (8 * k);
            r[(size_t)y * w + x] = v;
        }
    pal.clear();
    std::map<uint32_t, int> uniques;
    for (uint32_t p : r) { if (uniques.count(p)) continue; if ((int)uniques.size() >= n) break; uniques[p] = (int)pal.size(); pal.push_back(p); }
    idx.resize(r.size());
    bool exact = true;
    for (uint32_t p : r) if (!uniques.count(p)) { exact = false; break; }
    if (exact) { for (size_t i = 0; i < r.size(); i++) idx[i] = (uint8_t)uniques[r[i]]; return; }
    // popularity over 4-bit-per-channel buckets, each entry is its bucket average
    std::map<uint32_t, std::pair<uint64_t, uint64_t[4]>> cub;
    for (uint32_t p : r) {
        uint32_t k = ((p >> 4) & 0xF) | ((p >> 8) & 0xF0) | ((p >> 12) & 0xF00) | ((p >> 16) & 0xF000);
        auto& e = cub[k];
        e.first++;
        for (int j = 0; j < 4; j++) e.second[j] += (p >> (8 * j)) & 255;
    }
    std::vector<std::pair<uint64_t, uint32_t>> order;
    for (auto& kv : cub) {
        uint32_t v = 0;
        for (int j = 0; j < 4; j++) v |= (uint32_t)(kv.second.second[j] / kv.second.first) << (8 * j);
        order.push_back({kv.second.first, v});
    }
    std::sort(order.begin(), order.end(), [](const std::pair<uint64_t, uint32_t>& a, const std::pair<uint64_t, uint32_t>& b) { return a.first > b.first; });
    pal.clear();
    for (size_t i = 0; i < order.size() && (int)i < n; i++) pal.push_back(order[i].second);
    std::map<uint32_t, uint8_t> memo;
    for (size_t i = 0; i < r.size(); i++) {
        auto it = memo.find(r[i]);
        if (it != memo.end()) { idx[i] = it->second; continue; }
        int best = 0; int64_t md = -1;
        for (size_t q = 0; q < pal.size(); q++) {
            int64_t d = 0;
            for (int j = 0; j < 4; j++) { int a = (r[i] >> (8 * j)) & 255, b = (pal[q] >> (8 * j)) & 255; d += (int64_t)(a - b) * (a - b); }
            if (md < 0 || d < md) { md = d; best = (int)q; }
        }
        idx[i] = (uint8_t)best; memo[r[i]] = (uint8_t)best;
    }
}
