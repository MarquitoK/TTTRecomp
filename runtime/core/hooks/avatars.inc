#include "net/online.h"
#include <algorithm>
// Steam avatars in the menus (item 7): 64x64 CLUT8 placeholder textures of a menu RHG receive the avatar (pixels
// quantized to 256 colours, palette rewritten in place); kind 0 = own avatar, 1..4 = own room rows J1..J4
static bool av_want() { return true; }

struct AvTex { uint32_t rhg, tex, pix, pal; int sw, sh, kind, shown, amax, bank, ncol; std::vector<uint8_t> opix, opal; };
int psp_bank_current();
static std::vector<AvTex> av_t;
static std::string av_row[4];

static uint32_t av_bswap(uint32_t v) { return __builtin_bswap32(v); }
static uint32_t av_al(uint32_t x, uint32_t a) { return (x + a - 1) / a * a; }

// entry `code` of a pak (offsets big endian, as 0x088403F0 reads them)
static uint32_t av_entry(uint32_t pak, uint32_t code) {
    if (!pak) return 0;
    uint32_t n = psp_l32(pak);
    if (n > 0xFFFFu) n = av_bswap(n);
    if (!code || code > n) return 0;
    uint32_t off = av_bswap(psp_l32(pak + 4 * code)) & ~3u;
    return off ? pak + off : 0;
}

// layout as the RHG loader 0x0882C050 builds it: TEX at 0x30, DAT and PAL tables aligned to 16, blobs aligned to 128
static int av_register(uint32_t rhg, int first, int max, int bank, const char* what) {
    if (!rhg || psp_l32(rhg) != 0x00474852u) { printf("avatar: %s sin RHG en 0x%08X\n", what, rhg); return 0; }
    uint32_t ntex = psp_l32(rhg + 0x14), ndat = psp_l32(rhg + 0x1C), npal = psp_l32(rhg + 0x24);
    if (!ntex || ntex + ndat + npal > 4096) return 0;
    uint32_t bd = 0x30 + av_al(ntex * 40, 16), bp = bd + av_al(ndat * 36, 16), cur = bp + npal * 32;
    std::vector<uint32_t> doff(ndat), poff(npal);
    for (uint32_t k = 0; k < ndat; k++) { cur = av_al(cur, 128); doff[k] = cur; cur += psp_l32(rhg + bd + 36 * k + 20); }
    for (uint32_t k = 0; k < npal; k++) { cur = av_al(cur, 128); poff[k] = cur; cur += psp_l32(rhg + bp + 32 * k + 8); }
    int found = 0;
    for (uint32_t k = 0; k < ntex && found < max; k++) {
        uint32_t te = rhg + 0x30 + 40 * k, di = psp_l32(te + 4), pi = psp_l32(te + 8);
        if (di >= ndat || pi >= npal || psp_l32(te + 20) != 64 || psp_l32(te + 24) != 64) continue;
        uint32_t de = rhg + bd + 36 * di, pe = rhg + bp + 32 * pi;
        int ncol = (int)psp_l32(pe + 12);
        if (psp_l32(de + 4) != 0x11 || ncol != 256 || (int)psp_l32(pe + 8) < ncol * 4) continue;
        AvTex a;
        a.rhg = rhg; a.tex = te; a.pix = rhg + doff[di]; a.pal = rhg + poff[pi];
        a.sw = (int)psp_l32(de + 8); a.sh = (int)psp_l32(de + 12);
        if (a.sw < 64 || a.sh < 64 || a.sw > 256 || a.sh > 256) continue;
        a.kind = first + found; a.shown = -1; a.amax = 0; a.bank = bank; a.ncol = ncol;
        a.opix.assign((uint8_t*)psp_p(a.pix), (uint8_t*)psp_p(a.pix) + a.sw * a.sh);
        a.opal.assign((uint8_t*)psp_p(a.pal), (uint8_t*)psp_p(a.pal) + ncol * 4);
        for (int i = 0; i < ncol; i++) a.amax = std::max(a.amax, (int)a.opal[i * 4 + 3]);
        if (!a.amax) a.amax = 255;
        for (size_t i = 0; i < av_t.size(); i++) if (av_t[i].pix == a.pix) { av_t.erase(av_t.begin() + i); break; }
        av_t.push_back(a);
        printf("avatar: %s, textura %u (%dx%d, %d colores) para %s, alfa maximo %d%s\n", what, k, a.sw, a.sh, ncol, a.kind ? "fila de la sala" : "avatar propio", a.amax,
               !memcmp(a.opix.data(), "TTTPNG", 6) ? ", png" : "");
        found++;
    }
    if (!found) printf("avatar: %s sin texturas 64x64 de 8 bits (RHG de %u texturas)\n", what, ntex);
    return found;
}

static bool av_live(const AvTex& a) {
    return psp_l32(a.rhg) == 0x00474852u && psp_l32(a.tex + 20) == 64 && psp_l32(a.tex + 24) == 64;
}

// round avatar with a white ring and a dark navy edge (game style plates), 4x4 supersampled edges; no source = silhouette
static void av_compose(const uint8_t* src, uint8_t* out) {
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        float acc[4] = {0, 0, 0, 0};
        for (int sy = 0; sy < 4; sy++) for (int sx = 0; sx < 4; sx++) {
            float fx = x + (sx + 0.5f) / 4, fy = y + (sy + 0.5f) / 4, d = sqrtf((fx - 32) * (fx - 32) + (fy - 32) * (fy - 32));
            int c[3];
            if (d >= 32.0f) continue;
            if (d >= 30.5f) { c[0] = 0x14; c[1] = 0x24; c[2] = 0x58; }
            else if (d >= 27.5f) { c[0] = c[1] = c[2] = 0xFF; }
            else if (src) for (int k = 0; k < 3; k++) c[k] = src[(y * 64 + x) * 4 + k];
            else {
                float hx = fx - 32, hy = fy - 25, bxx = (fx - 32) / 17, byy = (fy - 52) / 14;
                bool fig = hx * hx + hy * hy < 100.0f || (bxx * bxx + byy * byy < 1.0f && fy > 38);
                c[0] = fig ? 0xD9 : 0x7C; c[1] = fig ? 0xDD : 0x83; c[2] = fig ? 0xE4 : 0x91;
            }
            for (int k = 0; k < 3; k++) acc[k] += c[k];
            acc[3] += 1;
        }
        uint8_t* o = out + (y * 64 + x) * 4;
        for (int k = 0; k < 3; k++) o[k] = acc[3] ? (uint8_t)(acc[k] / acc[3]) : 0;
        o[3] = (uint8_t)(acc[3] * 255 / 16);
    }
}

// median cut: boxes of pixels split on their widest channel until 256; each pixel takes its box's mean
static void av_quantize(const uint8_t* rgba, uint8_t* idx, uint8_t* pal, int amax, int ncol) {
    struct Box { int a, b; };
    std::vector<int> ord(4096);
    for (int i = 0; i < 4096; i++) ord[i] = i;
    std::vector<Box> bx{{0, 4096}};
    auto span = [&](const Box& b, int* ch) {
        int lo[4] = {255, 255, 255, 255}, hi[4] = {0, 0, 0, 0};
        for (int i = b.a; i < b.b; i++) for (int k = 0; k < 4; k++) { int v = rgba[ord[i] * 4 + k]; lo[k] = std::min(lo[k], v); hi[k] = std::max(hi[k], v); }
        int best = 0;
        for (int k = 1; k < 4; k++) if (hi[k] - lo[k] > hi[best] - lo[best]) best = k;
        *ch = best;
        return hi[best] - lo[best];
    };
    while ((int)bx.size() < ncol) {
        int pick = -1, ch = 0, wide = 0;
        for (size_t i = 0; i < bx.size(); i++) {
            if (bx[i].b - bx[i].a < 2) continue;
            int c2, s = span(bx[i], &c2);
            if (s > wide) { wide = s; pick = (int)i; ch = c2; }
        }
        if (pick < 0) break;
        Box b = bx[pick];
        std::sort(ord.begin() + b.a, ord.begin() + b.b, [&](int x, int y) { return rgba[x * 4 + ch] < rgba[y * 4 + ch]; });
        int mid = (b.a + b.b) / 2;
        bx[pick] = {b.a, mid};
        bx.push_back({mid, b.b});
    }
    memset(pal, 0, ncol * 4);
    for (size_t n = 0; n < bx.size(); n++) {
        unsigned s[4] = {0, 0, 0, 0};
        for (int i = bx[n].a; i < bx[n].b; i++) { for (int k = 0; k < 4; k++) s[k] += rgba[ord[i] * 4 + k]; idx[ord[i]] = (uint8_t)n; }
        int cnt = bx[n].b - bx[n].a;
        for (int k = 0; k < 3; k++) pal[n * 4 + k] = (uint8_t)(s[k] / cnt);
        pal[n * 4 + 3] = (uint8_t)(s[3] / cnt * amax / 255);
    }
}

// 8 bpp indices in the PSP's 16-byte x 8-row blocks (as tools/open_paks.py stores them)
static void av_put(const AvTex& a, const uint8_t* idx64, const uint8_t* pal) {
    std::vector<uint8_t> lin(a.sw * a.sh);
    for (int y = 0; y < a.sh; y++) for (int x = 0; x < a.sw; x++) lin[y * a.sw + x] = idx64[std::min(y, 63) * 64 + std::min(x, 63)];
    uint8_t* d = (uint8_t*)psp_p(a.pix);
    if (a.sw % 16 || a.sh % 8) memcpy(d, lin.data(), lin.size());
    else {
        int k = 0;
        for (int by = 0; by < a.sh; by += 8) for (int bxx = 0; bxx < a.sw / 16; bxx++) for (int r = 0; r < 8; r++, k += 16) memcpy(d + k, &lin[(by + r) * a.sw + bxx * 16], 16);
    }
    memcpy(psp_p(a.pal), pal, a.ncol * 4);
}

// every frame (mpm_frame in the menus, mpm_w_anim in the multiplayer scene): each live placeholder shows what it should (avatar, placeholder, or nothing when switched off)
static void av_frame() {
    static uint8_t rgba[64 * 64 * 4], round[64 * 64 * 4], idx[4096], pal[1024];
    bool on = av_want();
    for (size_t i = 0; i < av_t.size();) {
        AvTex& a = av_t[i];
        if (!av_live(a) || (a.bank >= 0 && a.bank != psp_bank_current())) { printf("avatar: textura 0x%08X descargada\n", a.pix); av_t.erase(av_t.begin() + i); continue; }
        i++;
        int id = 0;
        if (on) id = a.kind == 0 ? net_avatar(nullptr, nullptr) : !av_row[a.kind - 1].empty() ? net_avatar(av_row[a.kind - 1].c_str(), nullptr) : 0;
        int want = !on ? -2 : id;
        if (want == a.shown) continue;
        if (want == -2) { memset(idx, 0, sizeof(idx)); memset(pal, 0, sizeof(pal)); av_put(a, idx, pal); a.shown = want; continue; }
        bool got = want > 0 && net_avatar(a.kind ? av_row[a.kind - 1].c_str() : nullptr, rgba) == want;
        if (want > 0 && !got) continue;
        av_compose(got ? rgba : nullptr, round);
        av_quantize(round, idx, pal, a.amax, a.ncol);
        av_put(a, idx, pal);
        if (got) printf("avatar: %s escrito en 0x%08X\n", a.kind ? av_row[a.kind - 1].c_str() : "propio", a.pix);
        a.shown = want;
    }
}

// main menu / Options name plate (class 0x08A304D0): its nested pak's RHG 2 holds one placeholder for the own avatar
static void av_plate_loaded(uint32_t pak, uint32_t rhg_code) {
    av_register(av_entry(pak, rhg_code), 0, 1, psp_bank_current(), "placa del nombre");
}

// En linea (AGS 125): the language RHG holds five placeholders (own, J1..J4)
static void av_pak57_loaded(uint32_t pak, uint32_t rhg_code) {
    av_register(av_entry(pak, rhg_code), 0, 5, -1, "En linea");
}

static void av_drop_menu() {
    for (size_t i = 0; i < av_t.size();) if (av_t[i].bank < 0) av_t.erase(av_t.begin() + i); else i++;
    for (auto& r : av_row) r.clear();
}
