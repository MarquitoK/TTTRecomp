#pragma once
#include "psp.h"
#include <cmath>
#include <cstdio>
#define VFX_AI inline __attribute__((always_inline))
void psp_vfpu(Ctx& c, uint32_t pc, uint32_t w);
inline int psp_vfx_on = 1;
VFX_AI static float vf_min(float a, float b) { if (a != a) return b; if (b != b) return a; if (a < b) return a; if (b < a) return b; return std::signbit(a) ? a : b; }
VFX_AI static float vf_max(float a, float b) { if (a != a) return b; if (b != b) return a; if (a > b) return a; if (b > a) return b; return std::signbit(a) ? b : a; }
VFX_AI static double vf_p2(int e) { double r = 1.0; if (e > 0) while (e--) r *= 2.0; else while (e++) r *= 0.5; return r; }
static const float PI_2 = 1.5707963267948966f;
static const int CTRL_SPFX = 0, CTRL_TPFX = 1, CTRL_DPFX = 2, CTRL_CC = 6;
static const uint32_t PFX_IDENT = 0xE4;

static inline int vec_size(uint32_t w) {
    int a = (w >> 7) & 1, b = (w >> 15) & 1;
    return (b << 1 | a) + 1;          // 1 s, 2 p, 3 t, 4 q
}

static inline int mat_size(uint32_t w) {
    int a = (w >> 7) & 1, b = (w >> 15) & 1;
    return (b << 1 | a) + 1;          // 2x2 = 2, 3x3 = 3, 4x4 = 4 (1 does not exist)
}

VFX_AI static void regs_vec(int* out, int vreg, int n) {
    int mtx = (vreg >> 2) & 7, col = vreg & 3, row, trans;
    if (n == 1) { row = (vreg >> 5) & 3; trans = 0; }
    else if (n == 3) { row = (vreg >> 6) & 1; trans = (vreg >> 5) & 1; }
    else { row = (vreg >> 5) & 2; trans = (vreg >> 5) & 1; }
    for (int i = 0; i < n; i++) {
        int idx = mtx * 4;
        if (trans) idx += ((row + i) & 3) + col * 32;
        else idx += col + ((row + i) & 3) * 32;
        out[i] = idx;
    }
}

VFX_AI static void regs_mat(int* out, int vreg, int n) {
    int mtx = (vreg >> 2) & 7, col = vreg & 3;
    int row = (n == 3) ? (vreg >> 6) & 1 : (vreg >> 5) & 2;
    int trans = (vreg >> 5) & 1;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            int idx = mtx * 4;
            if (trans) idx += ((row + i) & 3) + ((col + j) & 3) * 32;
            else idx += ((col + j) & 3) + ((row + i) & 3) * 32;
            out[j * 4 + i] = idx;
        }
}

VFX_AI static void read_vec(Ctx& c, float* d, int vreg, int n) {
    int r[4];
    regs_vec(r, vreg, n);
    for (int i = 0; i < n; i++) d[i] = c.v[r[i]].f;
}

VFX_AI static void write_vec(Ctx& c, const float* d, int vreg, int n) {
    int r[4];
    regs_vec(r, vreg, n);
    for (int i = 0; i < n; i++) c.v[r[i]].f = d[i];
}

VFX_AI static void read_mat(Ctx& c, float* d, int vreg, int n) {
    int r[16];
    regs_mat(r, vreg, n);
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) d[j * 4 + i] = c.v[r[j * 4 + i]].f;
}

VFX_AI static void write_mat(Ctx& c, const float* d, int vreg, int n) {
    int r[16];
    regs_mat(r, vreg, n);
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) c.v[r[j * 4 + i]].f = d[j * 4 + i];
}

VFX_AI static void apply_pfx(Ctx& c, int ctrl, float* v, int n) {
    uint32_t p = c.vctrl[ctrl];
    if (p == PFX_IDENT) return;
    float orig[4] = {v[0], v[1], v[2], v[3]};
    static const float cte[2][4] = {{0.0f, 1.0f, 2.0f, 0.5f}, {3.0f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f}};
    for (int i = 0; i < n; i++) {
        int swz = (p >> (i * 2)) & 3;
        int absv = (p >> (8 + i)) & 1;
        int cons = (p >> (12 + i)) & 1;
        int neg = (p >> (16 + i)) & 1;
        float x;
        if (cons) x = cte[absv][swz];
        else { if (swz >= n) swz = 0; x = orig[swz]; if (absv) x = std::fabs(x); }
        if (neg) x = -x;
        v[i] = x;
    }
}

VFX_AI static void apply_dpfx(Ctx& c, float* d, int vreg, int n) {
    uint32_t p = c.vctrl[CTRL_DPFX];
    int r[4];
    regs_vec(r, vreg, n);
    if (p == 0) { for (int i = 0; i < n; i++) c.v[r[i]].f = d[i]; return; }
    for (int i = 0; i < n; i++) {
        int sat = (p >> (i * 2)) & 3;
        int mask = (p >> (8 + i)) & 1;
        if (mask) continue;
        float x = d[i];
        if (sat == 1) x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
        else if (sat == 3) x = x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
        c.v[r[i]].f = x;
    }
}

VFX_AI static void end_pfx(Ctx& c) {
    c.vctrl[CTRL_SPFX] = PFX_IDENT;
    c.vctrl[CTRL_TPFX] = PFX_IDENT;
    c.vctrl[CTRL_DPFX] = 0;
}

static inline int VD(uint32_t w) { return w & 0x7F; }
static inline int VS(uint32_t w) { return (w >> 8) & 0x7F; }
static inline int VT(uint32_t w) { return (w >> 16) & 0x7F; }

static const float CONSTANTES[32] = {
    // hardware table: 1 HUGE, 2 sqrt2, 3 sqrt1/2, 4 2/sqrt(pi), 5 2/pi, 6 1/pi, 7 pi/4, 8 pi/2, 9 pi,
    // 10 e, 11 log2e, 12 log10e, 13 ln2, 14 ln10, 15 2pi, 16 pi/6, 17 log10(2), 18 log2(10), 19 sqrt3/2
    0.0f, 3.4028235e38f, 1.4142135623730951f, 0.7071067811865476f, 1.1283791670955126f, 0.6366197723675814f,
    0.3183098861837907f, 0.7853981633974483f, 1.5707963267948966f, 3.141592653589793f,
    2.718281828459045f, 1.4426950408889634f, 0.4342944819032518f, 0.6931471805599453f,
    2.302585092994046f, 6.283185307179586f, 0.5235987755982988f, 0.3010299956639812f, 3.321928094887362f,
    0.8660254037844386f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

inline uint32_t vfpu_rng_state = 0x1234ABCDu;

VFX_AI static uint32_t rng32() {
    uint32_t x = vfpu_rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    vfpu_rng_state = x ? x : 0x1234ABCDu;
    return vfpu_rng_state;
}

static inline float round_even(float x) { return std::nearbyint(x); }

static inline int32_t f2i_sat(double v) {
    if (!(v == v)) return 0x7FFFFFFF;
    if (v >= 2147483648.0) return 0x7FFFFFFF;
    if (v <= -2147483648.0) return (int32_t)0x80000000;
    return (int32_t)v;
}

static void __attribute__((noinline)) vfpu_no(Ctx& c, uint32_t pc, uint32_t w, const char* que) {
    (void)c;
    static int n = 0;
    if (n++ < 40) fprintf(stderr, "vfpu sin cubrir (%s) 0x%08X en 0x%08X\n", que, w, pc);
}

VFX_AI static void vfpu_exec(Ctx& c, uint32_t pc, uint32_t w) {
    uint32_t op = w >> 26;
    int n = vec_size(w);
    float s[4] = {0, 0, 0, 0}, t[4] = {0, 0, 0, 0}, d[4] = {0, 0, 0, 0};

    if (op == 0x37) {
        // prefixes and immediates
        int sub = (w >> 23) & 7;
        if (sub == 0) { c.vctrl[CTRL_SPFX] = w & 0xFFFFF; return; }
        if (sub == 2) { c.vctrl[CTRL_TPFX] = w & 0xFFFFF; return; }
        if (sub == 4) { c.vctrl[CTRL_DPFX] = w & 0xFFF; return; }
        if (sub == 6) {                                  // viim: signed 16-bit integer
            int r[1]; regs_vec(r, VT(w), 1);
            c.v[r[0]].f = (float)(int16_t)(w & 0xFFFF);
            end_pfx(c); return;
        }
        if (sub == 7) {                                  // vfim: half float
            int r[1]; regs_vec(r, VT(w), 1);
            uint32_t h = w & 0xFFFF;
            uint32_t sg = (h >> 15) & 1, ex = (h >> 10) & 0x1F, ma = h & 0x3FF;
            float f;
            if (ex == 0) f = (float)ma * (float)vf_p2(-24);
            else if (ex == 31) f = ma ? NAN : INFINITY;
            else f = (float)(ma | 0x400) * (float)vf_p2((int)ex - 25);
            c.v[r[0]].f = sg ? -f : f;
            end_pfx(c); return;
        }
        vfpu_no(c, pc, w, "op37"); return;
    }

    if (op == 0x12 && ((w >> 21) & 31) == 8) {
        // bvf/bvt/bvfl/bvtl are emitted by the emitter; never reached here
        vfpu_no(c, pc, w, "bvx"); return;
    }

    if (op == 0x18) {
        read_vec(c, s, VS(w), n); read_vec(c, t, VT(w), n);
        apply_pfx(c, CTRL_SPFX, s, n); apply_pfx(c, CTRL_TPFX, t, n);
        int sub = (w >> 23) & 7;
        for (int i = 0; i < n; i++) {
            if (sub == 0) d[i] = s[i] + t[i];
            else if (sub == 1) d[i] = s[i] - t[i];
            else if (sub == 2) d[i] = std::ldexp(s[i], (int)t[i]);   // vsbn
            else if (sub == 7) d[i] = s[i] / t[i];
            else { vfpu_no(c, pc, w, "op18"); end_pfx(c); return; }
        }
        apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
    }

    if (op == 0x19) {
        int sub = (w >> 23) & 7;
        read_vec(c, s, VS(w), n);
        apply_pfx(c, CTRL_SPFX, s, n);
        if (sub == 0) {                                  // vmul
            read_vec(c, t, VT(w), n); apply_pfx(c, CTRL_TPFX, t, n);
            for (int i = 0; i < n; i++) d[i] = s[i] * t[i];
            apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
        }
        if (sub == 1 || sub == 4) {                      // vdot, vhdp
            read_vec(c, t, VT(w), n); apply_pfx(c, CTRL_TPFX, t, n);
            float acc = 0;
            for (int i = 0; i < n; i++) acc += (sub == 4 && i == n - 1) ? t[i] : s[i] * t[i];
            d[0] = acc;
            apply_dpfx(c, d, VD(w), 1); end_pfx(c); return;
        }
        if (sub == 2) {                                  // vscl: t is scalar
            read_vec(c, t, VT(w), 1); apply_pfx(c, CTRL_TPFX, t, 1);
            for (int i = 0; i < n; i++) d[i] = s[i] * t[0];
            apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
        }
        if (sub == 5) {                                  // vcrs: unpermuted cross product
            read_vec(c, t, VT(w), n); apply_pfx(c, CTRL_TPFX, t, n);
            d[0] = s[1] * t[2]; d[1] = s[2] * t[0]; d[2] = s[0] * t[1];
            apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
        }
        if (sub == 6) {                                  // vdet
            read_vec(c, t, VT(w), n); apply_pfx(c, CTRL_TPFX, t, n);
            d[0] = s[0] * t[1] - s[1] * t[0];
            apply_dpfx(c, d, VD(w), 1); end_pfx(c); return;
        }
        vfpu_no(c, pc, w, "op19"); end_pfx(c); return;
    }

    if (op == 0x1B) {
        int sub = (w >> 23) & 7;
        read_vec(c, s, VS(w), n); read_vec(c, t, VT(w), n);
        apply_pfx(c, CTRL_SPFX, s, n); apply_pfx(c, CTRL_TPFX, t, n);
        if (sub == 0) {                                  // vcmp
            int cond = w & 15;
            uint32_t cc = 0;
            bool o = false, a = true;
            for (int i = 0; i < n; i++) {
                bool r;
                float x = s[i], y = t[i];
                switch (cond) {
                case 0: r = false; break;
                case 1: r = x == y; break;
                case 2: r = x < y; break;
                case 3: r = x <= y; break;
                case 4: r = true; break;
                case 5: r = x != y; break;
                case 6: r = x >= y; break;
                case 7: r = x > y; break;
                case 8: r = std::fabs(x) == 0.0f; break;
                case 9: r = x != x; break;
                case 10: r = std::isinf(x); break;
                case 11: r = x != x || std::isinf(x); break;
                case 12: r = x != 0.0f; break;
                case 13: r = x == x; break;
                case 14: r = !std::isinf(x); break;
                default: r = !(x != x || std::isinf(x)); break;
                }
                if (r) cc |= 1u << i;
                o |= r; a &= r;
            }
            if (o) cc |= 0x10;
            if (a) cc |= 0x20;
            uint32_t affected = ((1u << n) - 1u) | 0x30u;    // only the compared lanes are touched
            c.vctrl[CTRL_CC] = (c.vctrl[CTRL_CC] & ~affected) | (cc & affected);
            end_pfx(c); return;
        }
        if (sub == 2 || sub == 3) {                      // vmin, vmax
            for (int i = 0; i < n; i++) {
                if (s[i] != s[i] || t[i] != t[i]) d[i] = NAN;
                else d[i] = (sub == 2) ? vf_min(s[i], t[i]) : vf_max(s[i], t[i]);
            }
            apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
        }
        if (sub == 5 || sub == 6 || sub == 7) {          // vscmp, vsge, vslt
            for (int i = 0; i < n; i++) {
                float x = s[i], y = t[i];
                if (sub == 5) d[i] = x < y ? -1.0f : (x > y ? 1.0f : 0.0f);
                else if (sub == 6) d[i] = (x != x || y != y) ? 0.0f : (x >= y ? 1.0f : 0.0f);
                else d[i] = (x != x || y != y) ? 0.0f : (x < y ? 1.0f : 0.0f);
            }
            apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
        }
        vfpu_no(c, pc, w, "op1B"); end_pfx(c); return;
    }

    if (op == 0x34) {
        int rs = (w >> 21) & 0x1F;
        if (rs == 0) {
            int sub = (w >> 16) & 0x1F;
            read_vec(c, s, VS(w), n); apply_pfx(c, CTRL_SPFX, s, n);
            for (int i = 0; i < n; i++) {
                float x = s[i];
                switch (sub) {
                case 0: d[i] = x; break;                                   // vmov
                case 1: d[i] = std::fabs(x); break;                        // vabs
                case 2: d[i] = -x; break;                                  // vneg
                case 3: d[i] = (i == (VD(w) & 3)) ? 1.0f : 0.0f; break;   // vidt
                case 4: d[i] = x < 0 ? 0 : (x > 1 ? 1 : x); break;         // vsat0
                case 5: d[i] = x < -1 ? -1 : (x > 1 ? 1 : x); break;       // vsat1
                case 6: d[i] = 0.0f; break;                                // vzero
                case 7: d[i] = 1.0f; break;                                // vone
                case 16: d[i] = 1.0f / x; break;                           // vrcp
                case 17: d[i] = 1.0f / std::sqrt(x); break;                // vrsq
                case 18: d[i] = std::sin(PI_2 * x); break;        // vsin
                case 19: d[i] = std::cos(PI_2 * x); break;        // vcos
                case 20: d[i] = std::exp2(x); break;                       // vexp2
                case 21: d[i] = std::log2(x); break;                       // vlog2
                case 22: d[i] = std::sqrt(std::fabs(x)); break;            // vsqrt
                case 23: d[i] = std::asin(x) / PI_2; break;       // vasin
                case 24: d[i] = -1.0f / x; break;                          // vnrcp
                case 26: d[i] = -std::sin(PI_2 * x); break;       // vnsin
                case 28: d[i] = 1.0f / std::exp2(x); break;                // vrexp2
                default: vfpu_no(c, pc, w, "op34/0"); end_pfx(c); return;
                }
            }
            apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
        }
        if (rs == 1) {
            int sub = (w >> 16) & 0x1F;
            if (sub == 0) {                              // vrnds: seed
                int r[1]; regs_vec(r, VD(w), 1);
                vfpu_rng_state = c.v[r[0]].u ? c.v[r[0]].u : 0x1234ABCDu;
                end_pfx(c); return;
            }
            if (sub >= 1 && sub <= 3) {                  // vrndi, vrndf1, vrndf2
                int r[4]; regs_vec(r, VD(w), n);
                for (int i = 0; i < n; i++) {
                    uint32_t x = rng32();
                    if (sub == 1) c.v[r[i]].u = x;
                    else if (sub == 2) c.v[r[i]].f = 1.0f + (float)x / 4294967295.0f;
                    else c.v[r[i]].f = 3.0f + (float)x / 4294967295.0f * 2.0f;
                }
                end_pfx(c); return;
            }
            // integer conversions (confirmed table: 24 vuc2i, 25 vc2i, 26 vus2i, 27 vs2i, 28 vi2uc, 29 vi2c, 30 vi2us, 31 vi2s)
            if (sub == 27 || sub == 26) {                // vs2i / vus2i: each word (2 shorts) -> 2 integers
                int r[4]; regs_vec(r, VS(w), n);
                uint32_t o[8];
                for (int i = 0; i < n; i++) {
                    uint32_t v = c.v[r[i]].u;
                    if (sub == 27) { o[i * 2] = (v & 0xFFFF) << 16; o[i * 2 + 1] = v & 0xFFFF0000u; }
                    else { o[i * 2] = (v & 0xFFFF) << 15; o[i * 2 + 1] = (v >> 16) << 15; }
                }
                int m = n * 2 > 4 ? 4 : n * 2;
                int rd[4]; regs_vec(rd, VD(w), m);
                for (int i = 0; i < m; i++) c.v[rd[i]].u = o[i];
                end_pfx(c); return;
            }
            if (sub == 24 || sub == 25) {                // vuc2i / vc2i: 4 bytes of a word -> 4 integers
                int r[1]; regs_vec(r, VS(w), 1);
                uint32_t v = c.v[r[0]].u;
                int rd[4]; regs_vec(rd, VD(w), 4);
                for (int i = 0; i < 4; i++) {
                    uint32_t b = (v >> (i * 8)) & 0xFF;
                    c.v[rd[i]].u = sub == 25 ? (b << 24) : ((b << 24) | (b << 16) | (b << 8) | b) >> 1;
                }
                end_pfx(c); return;
            }
            if (sub == 31 || sub == 30) {                // vi2s / vi2us: 2 integers -> 1 word with 2 shorts
                int r[4]; regs_vec(r, VS(w), n);
                int m = n / 2 < 1 ? 1 : n / 2;
                int rd[4]; regs_vec(rd, VD(w), m);
                for (int i = 0; i < m; i++) {
                    int32_t a = c.v[r[i * 2]].i, b = c.v[r[i * 2 + 1]].i;
                    uint32_t lo, hi;
                    if (sub == 31) { lo = (uint32_t)(a >> 16) & 0xFFFF; hi = (uint32_t)(b >> 16) & 0xFFFF; }
                    else { lo = a < 0 ? 0 : (uint32_t)(a >> 15); hi = b < 0 ? 0 : (uint32_t)(b >> 15); if (lo > 0xFFFF) lo = 0xFFFF; if (hi > 0xFFFF) hi = 0xFFFF; }
                    c.v[rd[i]].u = lo | (hi << 16);
                }
                end_pfx(c); return;
            }
            if (sub == 29) {                             // vi2c: 4 integers -> 4 signed bytes (high byte)
                int r[4]; regs_vec(r, VS(w), 4);
                uint32_t out = 0;
                for (int i = 0; i < 4; i++) out |= ((uint32_t)c.v[r[i]].u >> 24) << (i * 8);
                int rd[1]; regs_vec(rd, VD(w), 1);
                c.v[rd[0]].u = out;
                end_pfx(c); return;
            }
            if (sub == 28) {                             // vi2uc: 4 integers -> 4 saturated unsigned bytes
                int r[4]; regs_vec(r, VS(w), 4);
                uint32_t out = 0;
                for (int i = 0; i < 4; i++) {
                    int32_t x = c.v[r[i]].i;
                    uint32_t b = x < 0 ? 0 : (uint32_t)(x >> 23);
                    if (b > 255) b = 255;
                    out |= b << (i * 8);
                }
                int rd[1]; regs_vec(rd, VD(w), 1);
                c.v[rd[0]].u = out;
                end_pfx(c); return;
            }
            vfpu_no(c, pc, w, "op34/1"); end_pfx(c); return;
        }
        if (rs == 2) {
            int sub = (w >> 16) & 0x1F;
            if (sub >= 26 && sub <= 28) {                // vt4444, vt5551, vt5650: RGBA8888 quad -> 16-bit pair
                int r[4]; regs_vec(r, VS(w), 4);
                uint32_t col[4];
                for (int i = 0; i < 4; i++) {
                    uint32_t in = c.v[r[i]].u;
                    uint32_t cr = in & 0xFF, cg = (in >> 8) & 0xFF, cb = (in >> 16) & 0xFF, ca = (in >> 24) & 0xFF;
                    if (sub == 26) col[i] = ((ca >> 4) << 12) | ((cb >> 4) << 8) | ((cg >> 4) << 4) | (cr >> 4);
                    else if (sub == 27) col[i] = ((ca >> 7) << 15) | ((cb >> 3) << 10) | ((cg >> 3) << 5) | (cr >> 3);
                    else col[i] = ((cb >> 3) << 11) | ((cg >> 2) << 5) | (cr >> 3);
                }
                int rd[4]; regs_vec(rd, VD(w), 2);
                c.v[rd[0]].u = col[0] | (col[1] << 16);
                c.v[rd[1]].u = col[2] | (col[3] << 16);
                end_pfx(c); return;
            }
            if (sub == 16 || sub == 17) {                // vmfvc, vmtvc
                int imm = (sub == 16) ? (VS(w) & 0x7F) : (VD(w) & 0x7F);
                if (imm >= 128 || imm < 16) {
                    int k = imm >= 128 ? imm - 128 : imm;
                    if (sub == 16) { int rd[1]; regs_vec(rd, VD(w), 1); c.v[rd[0]].u = c.vctrl[k & 15]; }
                    else { int r[1]; regs_vec(r, VS(w), 1); c.vctrl[k & 15] = c.v[r[0]].u; }
                }
                end_pfx(c); return;
            }
            read_vec(c, s, VS(w), n); apply_pfx(c, CTRL_SPFX, s, n);
            if (sub == 10) {                             // vsgn
                for (int i = 0; i < n; i++) d[i] = s[i] > 0 ? 1.0f : (s[i] < 0 ? -1.0f : 0.0f);
                apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
            }
            if (sub == 4) {                              // vocp
                for (int i = 0; i < n; i++) d[i] = 1.0f - s[i];
                apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
            }
            if (sub == 5) {                              // vsocp: output is twice as wide
                int m = n * 2 > 4 ? 4 : n * 2;
                for (int i = 0; i < m / 2; i++) {
                    float a = 1.0f - s[i], b = s[i];
                    d[i * 2] = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
                    d[i * 2 + 1] = b < 0.0f ? 0.0f : (b > 1.0f ? 1.0f : b);
                }
                apply_dpfx(c, d, VD(w), m); end_pfx(c); return;
            }
            if (sub == 6 || sub == 7) {                  // vfad, vavg
                float acc = 0;
                for (int i = 0; i < n; i++) acc += s[i];
                d[0] = (sub == 7) ? acc / (float)n : acc;
                apply_dpfx(c, d, VD(w), 1); end_pfx(c); return;
            }
            if (sub == 2) {                              // vbfy1
                d[0] = s[0] + s[1]; d[1] = s[0] - s[1];
                if (n == 4) { d[2] = s[2] + s[3]; d[3] = s[2] - s[3]; }
                apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
            }
            if (sub == 3) {                              // vbfy2
                d[0] = s[0] + s[2]; d[1] = s[1] + s[3];
                d[2] = s[0] - s[2]; d[3] = s[1] - s[3];
                apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
            }
            if (sub == 0 || sub == 1 || sub == 8 || sub == 9) {   // vsrt1..vsrt4
                if (sub == 0) {
                    d[0] = vf_min(s[0], s[1]); d[1] = vf_max(s[0], s[1]);
                    d[2] = vf_min(s[2], s[3]); d[3] = vf_max(s[2], s[3]);
                } else if (sub == 1) {
                    d[0] = vf_min(s[0], s[3]); d[1] = vf_min(s[1], s[2]);
                    d[2] = vf_max(s[1], s[2]); d[3] = vf_max(s[0], s[3]);
                } else if (sub == 8) {
                    d[0] = vf_max(s[0], s[1]); d[1] = vf_min(s[0], s[1]);
                    d[2] = vf_max(s[2], s[3]); d[3] = vf_min(s[2], s[3]);
                } else {
                    d[0] = vf_max(s[0], s[3]); d[1] = vf_max(s[1], s[2]);
                    d[2] = vf_min(s[1], s[2]); d[3] = vf_min(s[0], s[3]);
                }
                apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
            }
            vfpu_no(c, pc, w, "op34/2"); end_pfx(c); return;
        }
        if (rs == 3) {                                   // vcst
            int k = (w >> 16) & 0x1F;
            for (int i = 0; i < n; i++) d[i] = CONSTANTES[k];
            apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
        }
        if (rs >= 16 && rs <= 19) {                      // vf2in/iz/iu/id: float -> integer with scale
            int esc = (w >> 16) & 0x1F;
            read_vec(c, s, VS(w), n); apply_pfx(c, CTRL_SPFX, s, n);
            int r[4]; regs_vec(r, VD(w), n);
            for (int i = 0; i < n; i++) {
                double x = (double)s[i] * vf_p2(esc);
                double y = rs == 16 ? std::nearbyint(x) : rs == 17 ? std::trunc(x) : rs == 18 ? std::ceil(x) : std::floor(x);
                c.v[r[i]].i = f2i_sat(y);
            }
            end_pfx(c); return;
        }
        if (rs == 20) {                                  // vi2f
            int esc = (w >> 16) & 0x1F;
            int r[4]; regs_vec(r, VS(w), n);
            for (int i = 0; i < n; i++) d[i] = (float)((double)c.v[r[i]].i * vf_p2(-esc));
            apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
        }
        if (rs == 21) {                                  // vcmovt / vcmovf
            int tf = (w >> 19) & 1;
            int imm3 = (w >> 16) & 7;
            read_vec(c, s, VS(w), n); apply_pfx(c, CTRL_SPFX, s, n);
            read_vec(c, d, VD(w), n);
            uint32_t cc = c.vctrl[CTRL_CC];
            for (int i = 0; i < n; i++) {
                bool bit = imm3 < 6 ? ((cc >> imm3) & 1) : ((cc >> i) & 1);
                if (tf == 0 ? bit : !bit) d[i] = s[i];
            }
            apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
        }
        vfpu_no(c, pc, w, "op34"); end_pfx(c); return;
    }

    if (op == 0x3C) {
        int sub = (w >> 23) & 7;
        if (sub == 0) {                                  // vmmul
            int m = mat_size(w);
            float S[16], T[16], D[16];
            read_mat(c, S, VS(w), m); read_mat(c, T, VT(w), m);
            for (int a = 0; a < m; a++)
                for (int b = 0; b < m; b++) {
                    float acc = 0;
                    for (int k = 0; k < m; k++) acc += S[b * 4 + k] * T[a * 4 + k];
                    D[a * 4 + b] = acc;
                }
            write_mat(c, D, VD(w), m); end_pfx(c); return;
        }
        if (sub == 2 || sub == 3 || sub == 1) {          // vtfm / vhtfm 2,3,4
            int m = sub + 1;                             // matrix order
            int nt = n;                                  // size of vector t: m (vtfm) or m-1 (vhtfm)
            float S[16];
            read_mat(c, S, VS(w), m);
            read_vec(c, t, VT(w), nt); apply_pfx(c, CTRL_TPFX, t, nt);
            bool homog = (nt == m - 1);
            for (int i = 0; i < m; i++) {
                float acc = 0;
                for (int k = 0; k < m; k++) {
                    float tk = (k < nt) ? t[k] : (homog ? 1.0f : 0.0f);
                    acc += S[i * 4 + k] * tk;
                }
                d[i] = acc;
            }
            apply_dpfx(c, d, VD(w), m); end_pfx(c); return;
        }
        if (sub == 4) {                                  // vmscl
            int m = mat_size(w);
            float S[16], D[16];
            read_mat(c, S, VS(w), m);
            read_vec(c, t, VT(w), 1);
            for (int i = 0; i < m; i++) for (int j = 0; j < m; j++) D[j * 4 + i] = S[j * 4 + i] * t[0];
            write_mat(c, D, VD(w), m); end_pfx(c); return;
        }
        if (sub == 5) {                                  // vcrsp (t) / vqmul (q)
            read_vec(c, s, VS(w), n); read_vec(c, t, VT(w), n);
            apply_pfx(c, CTRL_SPFX, s, n); apply_pfx(c, CTRL_TPFX, t, n);
            if (n == 3) {
                d[0] = s[1] * t[2] - s[2] * t[1];
                d[1] = s[2] * t[0] - s[0] * t[2];
                d[2] = s[0] * t[1] - s[1] * t[0];
            } else {
                d[0] = s[0] * t[3] + s[1] * t[2] - s[2] * t[1] + s[3] * t[0];
                d[1] = -s[0] * t[2] + s[1] * t[3] + s[2] * t[0] + s[3] * t[1];
                d[2] = s[0] * t[1] - s[1] * t[0] + s[2] * t[3] + s[3] * t[2];
                d[3] = -s[0] * t[0] - s[1] * t[1] - s[2] * t[2] + s[3] * t[3];
            }
            apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
        }
        if (sub == 7) {
            if ((w >> 21) & 1) {                         // vrot
                int imm = (w >> 16) & 0x1F;
                read_vec(c, s, VS(w), 1); apply_pfx(c, CTRL_SPFX, s, 1);
                float angle = s[0] * PI_2;
                float sn = std::sin(angle), cs = std::cos(angle);
                if (imm & 0x10) sn = -sn;
                int ls = (imm >> 2) & 3, lc = imm & 3;
                for (int i = 0; i < n; i++) d[i] = 0.0f;
                if (ls == lc) for (int i = 0; i < n; i++) d[i] = sn;
                d[ls] = sn;
                d[lc] = cs;
                apply_dpfx(c, d, VD(w), n); end_pfx(c); return;
            }
            int sub2 = (w >> 16) & 0x1F;
            int m = mat_size(w);
            float S[16], D[16];
            if (sub2 == 0) { read_mat(c, S, VS(w), m); write_mat(c, S, VD(w), m); end_pfx(c); return; }   // vmmov
            for (int i = 0; i < m; i++) for (int j = 0; j < m; j++)
                D[j * 4 + i] = sub2 == 3 ? (i == j ? 1.0f : 0.0f) : sub2 == 6 ? 0.0f : sub2 == 7 ? 1.0f : 0.0f;
            if (sub2 == 3 || sub2 == 6 || sub2 == 7) { write_mat(c, D, VD(w), m); end_pfx(c); return; }
            vfpu_no(c, pc, w, "op3C/7"); end_pfx(c); return;
        }
        vfpu_no(c, pc, w, "op3C"); end_pfx(c); return;
    }

    vfpu_no(c, pc, w, "op");
}

template <uint32_t W> static inline void vfx_t(Ctx& c, uint32_t pc) { vfpu_exec(c, pc, W); }
#define PSP_VFX(pc, w) do { if (psp_vfx_on) vfx_t<w>(c, pc); else psp_vfpu(c, pc, w); } while (0)
