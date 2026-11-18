#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>

// Decodificador ATRAC3 (1024 muestras por cuadro). Portado de libavcodec/atrac3.c y atrac.c de
// FFmpeg (Maxim Poliakovski, Benjamin Larsson), licencia LGPL 2.1 o posterior. Modos: mono,
// estereo y estereo conjunto, formato WAV/PSP (sin descifrado RM).

#define AT3_PI 3.14159265358979323846
#define AT3_MUESTRAS 1024
#define AT3_MDCT 512

struct At3Ganancia { int n, nivel[7], pos[7]; };
struct At3Tonal { int pos, n; float c[8]; };

struct At3Canal {
    int bandas, ntonal, conmutar;
    float previo[AT3_MUESTRAS];
    At3Tonal tonal[64];
    At3Ganancia gan[2][4];
    float espectro[AT3_MUESTRAS];
    float imdct[AT3_MUESTRAS];
    float retardo1[46], retardo2[46], retardo3[46];
};

struct At3 {
    int canales, bloque, conjunto, listo;
    At3Canal u[2];
    int mprev[4], mahora[4], msig[4], peso[6];
    uint8_t volteado[4096 + 8];
    float tmp[1070];
    float sal[2][AT3_MUESTRAS];
};

struct At3Bits {
    const uint8_t* d; int bits, pos;
    unsigned leer(int n) {
        unsigned v = 0;
        for (int i = 0; i < n; i++) {
            int b = (pos < bits) ? ((d[pos >> 3] >> (7 - (pos & 7))) & 1) : 0;
            v = (v << 1) | (unsigned)b; pos++;
        }
        return v;
    }
    int leer_s(int n) { unsigned v = leer(n); return n ? (int)(v << (32 - n)) >> (32 - n) : 0; }
    unsigned ver(int n) { int p = pos; unsigned v = leer(n); pos = p; return v; }
    int quedan() const { return bits - pos; }
};

static const uint8_t at3_huff[][2] = {
    {31, 1}, {32, 3}, {33, 3}, {34, 4}, {35, 4}, {36, 5}, {37, 5}, {38, 5}, {39, 5},
    {31, 1}, {32, 3}, {30, 3}, {33, 3}, {29, 3},
    {31, 1}, {32, 3}, {30, 3}, {33, 4}, {29, 4}, {34, 4}, {28, 4},
    {31, 1}, {32, 3}, {30, 3}, {33, 4}, {29, 4}, {34, 5}, {28, 5}, {35, 5}, {27, 5},
    {31, 2}, {32, 3}, {30, 3}, {33, 4}, {29, 4}, {34, 4}, {28, 4}, {38, 4}, {24, 4}, {35, 5},
    {27, 5}, {36, 6}, {26, 6}, {37, 6}, {25, 6},
    {31, 3}, {32, 4}, {30, 4}, {33, 4}, {29, 4}, {34, 4}, {28, 4}, {46, 4}, {16, 4}, {35, 5},
    {27, 5}, {36, 5}, {26, 5}, {37, 5}, {25, 5}, {38, 6}, {24, 6}, {39, 6}, {23, 6}, {40, 6},
    {22, 6}, {41, 6}, {21, 6}, {42, 7}, {20, 7}, {43, 7}, {19, 7}, {44, 7}, {18, 7}, {45, 7},
    {17, 7},
    {31, 3}, {62, 4}, {0, 4}, {32, 5}, {30, 5}, {33, 5}, {29, 5}, {34, 5}, {28, 5}, {35, 5},
    {27, 5}, {36, 5}, {26, 5}, {37, 6}, {25, 6}, {38, 6}, {24, 6}, {39, 6}, {23, 6}, {40, 6},
    {22, 6}, {41, 6}, {21, 6}, {42, 6}, {20, 6}, {43, 6}, {19, 6}, {44, 6}, {18, 6}, {45, 7},
    {17, 7}, {46, 7}, {16, 7}, {47, 7}, {15, 7}, {48, 7}, {14, 7}, {49, 7}, {13, 7}, {50, 7},
    {12, 7}, {51, 7}, {11, 7}, {52, 8}, {10, 8}, {53, 8}, {9, 8}, {54, 8}, {8, 8}, {55, 8},
    {7, 8}, {56, 8}, {6, 8}, {57, 8}, {5, 8}, {58, 8}, {4, 8}, {59, 8}, {3, 8}, {60, 8},
    {2, 8}, {61, 8}, {1, 8},
};
static const uint8_t at3_huff_n[7] = {9, 5, 7, 9, 15, 31, 63};
static const uint8_t at3_clc_len[8] = {0, 4, 3, 3, 4, 4, 5, 6};
static const int8_t at3_clc_mant[4] = {0, 1, -2, -1};
static const int8_t at3_vlc_mant[18] = {0, 0, 0, 1, 0, -1, 1, 0, -1, 0, 1, 1, 1, -1, -1, 1, -1, -1};
static const float at3_inv_quant[8] = {0.0f, 1.0f / 1.5f, 1.0f / 2.5f, 1.0f / 3.5f, 1.0f / 4.5f, 1.0f / 7.5f, 1.0f / 15.5f, 1.0f / 31.5f};
static const uint16_t at3_subbanda[33] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176,
    192, 224, 256, 288, 320, 352, 384, 416, 448, 480, 512, 576, 640, 704, 768, 896, 1024};
static const float at3_matriz[8] = {0.0f, 2.0f, 2.0f, 2.0f, 0.0f, 0.0f, 1.0f, 1.0f};
static const float at3_qmf_mitad[24] = {
    -0.00001461907f, -0.00009205479f, -0.000056157569f, 0.00030117269f, 0.0002422519f, -0.00085293897f,
    -0.0005205574f, 0.0020340169f, 0.00078333891f, -0.0042153862f, -0.00075614988f, 0.0078402944f,
    -0.000061169922f, -0.01344162f, 0.0024626821f, 0.021736089f, -0.007801671f, -0.034090221f,
    0.01880949f, 0.054326009f, -0.043596379f, -0.099384367f, 0.13207909f, 0.46424159f};

// tablas generadas una vez
static int16_t at3_vlc[7][256][2];
static float at3_ventana[AT3_MDCT];
static float at3_sf[64];
static float at3_qmf[48];
static float at3_gan1[16], at3_gan2[31];
static float* at3_cos = nullptr;
static int at3_tablas_hechas = 0;

static void at3_tablas() {
    if (at3_tablas_hechas) return;
    at3_tablas_hechas = 1;
    const uint8_t (*h)[2] = at3_huff;
    for (int t = 0; t < 7; t++) {
        uint32_t codigo = 0;
        for (int i = 0; i < 256; i++) { at3_vlc[t][i][0] = 0; at3_vlc[t][i][1] = 0; }
        for (int i = 0; i < at3_huff_n[t]; i++) {
            int len = h[i][1], simb = (int)h[i][0] - 31;
            uint32_t c = codigo >> (32 - len);
            for (int k = 0; k < (1 << (8 - len)); k++) { at3_vlc[t][(c << (8 - len)) | k][0] = (int16_t)simb; at3_vlc[t][(c << (8 - len)) | k][1] = (int16_t)len; }
            codigo += 1u << (32 - len);
        }
        h += at3_huff_n[t];
    }
    for (int i = 0, j = 255; i < 128; i++, j--) {
        float wi = (float)(sin(((i + 0.5) / 256.0 - 0.5) * AT3_PI) + 1.0);
        float wj = (float)(sin(((j + 0.5) / 256.0 - 0.5) * AT3_PI) + 1.0);
        float w = 0.5f * (wi * wi + wj * wj);
        at3_ventana[i] = at3_ventana[511 - i] = wi / w;
        at3_ventana[j] = at3_ventana[511 - j] = wj / w;
    }
    for (int i = 0; i < 64; i++) at3_sf[i] = (float)pow(2.0, (i - 15) / 3.0);
    for (int i = 0; i < 24; i++) at3_qmf[i] = at3_qmf[47 - i] = at3_qmf_mitad[i] * 2.0f;
    for (int i = 0; i < 16; i++) at3_gan1[i] = powf(2.0f, (float)(4 - i));
    for (int i = -15; i < 16; i++) at3_gan2[i + 15] = powf(2.0f, -1.0f / 8 * i);
    at3_cos = new float[AT3_MDCT * 256];
    for (int n = 0; n < AT3_MDCT; n++)
        for (int k = 0; k < 256; k++)
            at3_cos[n * 256 + k] = (float)(-cos(AT3_PI / 256.0 * (n + 128.5) * (k + 0.5)) / 32768.0);
}

static int at3_vlc_leer(At3Bits& b, int t) {
    unsigned v = b.ver(8);
    int len = at3_vlc[t][v][1];
    if (!len) len = 8;
    b.pos += len;
    return at3_vlc[t][v][0];
}

static void at3_imlt(const float* ent, float* sal, int impar) {
    float e[256];
    memcpy(e, ent, sizeof(e));
    if (impar) for (int i = 0; i < 128; i++) { float t = e[i]; e[i] = e[255 - i]; e[255 - i] = t; }
    for (int n = 0; n < AT3_MDCT; n++) {
        const float* c = at3_cos + n * 256;
        float s = 0;
        for (int k = 0; k < 256; k++) s += e[k] * c[k];
        sal[n] = s * at3_ventana[n];
    }
}

static void at3_coefs(At3Bits& b, int sel, int clc, int* m, int n) {
    if (sel == 1) n /= 2;
    if (clc) {
        int nb = at3_clc_len[sel];
        if (sel > 1) for (int i = 0; i < n; i++) m[i] = nb ? b.leer_s(nb) : 0;
        else for (int i = 0; i < n; i++) { int c = nb ? (int)b.leer(nb) : 0; m[i * 2] = at3_clc_mant[c >> 2]; m[i * 2 + 1] = at3_clc_mant[c & 3]; }
    } else {
        if (sel != 1) for (int i = 0; i < n; i++) m[i] = at3_vlc_leer(b, sel - 1);
        else for (int i = 0; i < n; i++) { int h = at3_vlc_leer(b, 0); m[i * 2] = at3_vlc_mant[h * 2]; m[i * 2 + 1] = at3_vlc_mant[h * 2 + 1]; }
    }
}

static int at3_espectro(At3Bits& b, float* sal) {
    int nsb = (int)b.leer(5), clc = (int)b.leer(1), sel[32], sf[32], m[128];
    for (int i = 0; i <= nsb; i++) sel[i] = (int)b.leer(3);
    for (int i = 0; i <= nsb; i++) if (sel[i]) sf[i] = (int)b.leer(6);
    int i;
    for (i = 0; i <= nsb; i++) {
        int ini = at3_subbanda[i], fin = at3_subbanda[i + 1];
        if (sel[i]) {
            at3_coefs(b, sel[i], clc, m, fin - ini);
            float esc = at3_sf[sf[i]] * at3_inv_quant[sel[i]];
            for (int j = 0; ini < fin; ini++, j++) sal[ini] = m[j] * esc;
        } else memset(sal + ini, 0, (size_t)(fin - ini) * sizeof(float));
    }
    int ini = at3_subbanda[i];
    memset(sal + ini, 0, (size_t)(AT3_MUESTRAS - ini) * sizeof(float));
    return nsb;
}

static int at3_tonales(At3Bits& b, At3Tonal* t, int nbandas) {
    int nc = (int)b.leer(5);
    if (!nc) return 0;
    int msel = (int)b.leer(2);
    if (msel == 2) return -1;
    int modo = msel & 1, cuenta = 0, bf[4], m[8];
    for (int i = 0; i < nc; i++) {
        for (int k = 0; k <= nbandas; k++) bf[k] = (int)b.leer(1);
        int nval = (int)b.leer(3), q = (int)b.leer(3);
        if (q <= 1) return -1;
        if (msel == 3) modo = (int)b.leer(1);
        for (int k = 0; k < (nbandas + 1) * 4; k++) {
            if (!bf[k >> 2]) continue;
            int ncomp = (int)b.leer(3);
            for (int c = 0; c < ncomp; c++) {
                int sf = (int)b.leer(6);
                if (cuenta >= 64) return -1;
                At3Tonal& x = t[cuenta];
                x.pos = k * 64 + (int)b.leer(6);
                int nv = nval + 1;
                if (nv > AT3_MUESTRAS - x.pos) nv = AT3_MUESTRAS - x.pos;
                float esc = at3_sf[sf] * at3_inv_quant[q];
                at3_coefs(b, q, modo, m, nv);
                x.n = nv;
                for (int j = 0; j < nv; j++) x.c[j] = m[j] * esc;
                cuenta++;
            }
        }
    }
    return cuenta;
}

static int at3_ganancias(At3Bits& b, At3Ganancia* g, int nbandas) {
    int k;
    for (k = 0; k <= nbandas; k++) {
        g[k].n = (int)b.leer(3);
        for (int j = 0; j < g[k].n; j++) {
            g[k].nivel[j] = (int)b.leer(4);
            g[k].pos[j] = (int)b.leer(5);
            if (j && g[k].pos[j] <= g[k].pos[j - 1]) return -1;
        }
    }
    for (; k < 4; k++) g[k].n = 0;
    return 0;
}

static void at3_compensar(float* ent, float* prev, At3Ganancia* ahora, At3Ganancia* sig, float* sal) {
    float esc = sig->n ? at3_gan1[sig->nivel[0]] : 1.0f;
    int pos = 0;
    if (!ahora->n) {
        for (; pos < 256; pos++) sal[pos] = ent[pos] * esc + prev[pos];
    } else {
        for (int i = 0; i < ahora->n; i++) {
            int ult = ahora->pos[i] << 3;
            float lev = at3_gan1[ahora->nivel[i]];
            float inc = at3_gan2[(i + 1 < ahora->n ? ahora->nivel[i + 1] : 4) - ahora->nivel[i] + 15];
            for (; pos < ult; pos++) sal[pos] = (ent[pos] * esc + prev[pos]) * lev;
            for (; pos < ult + 8; pos++) { sal[pos] = (ent[pos] * esc + prev[pos]) * lev; lev *= inc; }
        }
        for (; pos < 256; pos++) sal[pos] = ent[pos] * esc + prev[pos];
    }
    memcpy(prev, ent + 256, 256 * sizeof(float));
}

static void at3_iqmf(const float* lo, const float* hi, int n, float* sal, float* retardo, float* tmp) {
    memcpy(tmp, retardo, 46 * sizeof(float));
    float* p3 = tmp + 46;
    for (int i = 0; i < n; i += 2) {
        p3[2 * i] = lo[i] + hi[i]; p3[2 * i + 1] = lo[i] - hi[i];
        p3[2 * i + 2] = lo[i + 1] + hi[i + 1]; p3[2 * i + 3] = lo[i + 1] - hi[i + 1];
    }
    float* p1 = tmp;
    for (int j = n; j; j--) {
        float s1 = 0, s2 = 0;
        for (int i = 0; i < 48; i += 2) { s1 += p1[i] * at3_qmf[i]; s2 += p1[i + 1] * at3_qmf[i + 1]; }
        sal[0] = s2; sal[1] = s1; p1 += 2; sal += 2;
    }
    memcpy(retardo, tmp + n * 2, 46 * sizeof(float));
}

static int at3_unidad(At3& q, At3Bits& b, At3Canal& u, float* sal, int canal, int conjunto) {
    At3Ganancia* g1 = u.gan[u.conmutar];
    At3Ganancia* g2 = u.gan[1 - u.conmutar];
    if (conjunto && (canal & 1)) { if (b.leer(2) != 3) return -1; }
    else if (b.leer(6) != 0x28) return -1;
    u.bandas = (int)b.leer(2);
    if (at3_ganancias(b, g2, u.bandas)) return -1;
    u.ntonal = at3_tonales(b, u.tonal, u.bandas);
    if (u.ntonal < 0) return -1;
    int nsb = at3_espectro(b, u.espectro);
    int ult = -1;
    for (int i = 0; i < u.ntonal; i++) {
        if (u.tonal[i].pos + u.tonal[i].n > ult) ult = u.tonal[i].pos + u.tonal[i].n;
        for (int j = 0; j < u.tonal[i].n; j++) u.espectro[u.tonal[i].pos + j] += u.tonal[i].c[j];
    }
    int nb = (at3_subbanda[nsb + 1] - 1) >> 8;
    if (ult >= 0 && ((ult + 256) >> 8) > nb) nb = (ult + 256) >> 8;
    for (int band = 0; band < 4; band++) {
        if (band <= nb) at3_imlt(&u.espectro[band * 256], u.imdct, band & 1);
        else memset(u.imdct, 0, sizeof(u.imdct));
        at3_compensar(u.imdct, &u.previo[band * 256], &g1[band], &g2[band], &sal[band * 256]);
    }
    u.conmutar ^= 1;
    return 0;
}

static void at3_desmatriz(float* s1, float* s2, const int* prev, const int* ahora) {
    for (int i = 0, band = 0; band < 1024; band += 256, i++) {
        int a = prev[i], c = ahora[i], n = band;
        if (a != c) {
            float l1 = at3_matriz[a * 2], r1 = at3_matriz[a * 2 + 1], l2 = at3_matriz[c * 2], r2 = at3_matriz[c * 2 + 1];
            for (; n < band + 8; n++) {
                float f = (n - band) * 0.125f, c1 = s1[n], c2 = s2[n];
                c2 = c1 * (l1 + f * (l2 - l1)) + c2 * (r1 + f * (r2 - r1));
                s1[n] = c2; s2[n] = c1 * 2.0f - c2;
            }
        }
        for (; n < band + 256; n++) {
            float c1 = s1[n], c2 = s2[n];
            if (c == 0) { s1[n] = c2 * 2.0f; s2[n] = (c1 - c2) * 2.0f; }
            else if (c == 1) { s1[n] = (c1 + c2) * 2.0f; s2[n] = c2 * -2.0f; }
            else { s1[n] = c1 + c2; s2[n] = c1 - c2; }
        }
    }
}

static void at3_pesos_canal(int idx, int flag, float* w) {
    if (idx == 7) { w[0] = 1.0f; w[1] = 1.0f; return; }
    w[0] = (idx & 7) / 7.0f; w[1] = sqrtf(2 - w[0] * w[0]);
    if (flag) { float t = w[0]; w[0] = w[1]; w[1] = t; }
}

static void at3_ponderar(float* s1, float* s2, const int* p) {
    if (p[1] == 7 && p[3] == 7) return;
    float w[2][2];
    at3_pesos_canal(p[1], p[0], w[0]);
    at3_pesos_canal(p[3], p[2], w[1]);
    for (int band = 256; band < 1024; band += 256) {
        int n = band;
        for (; n < band + 8; n++) {
            float f = (n - band) * 0.125f;
            s1[n] *= w[0][0] + f * (w[0][1] - w[0][0]);
            s2[n] *= w[1][0] + f * (w[1][1] - w[1][0]);
        }
        for (; n < band + 256; n++) { s1[n] *= w[1][0]; s2[n] *= w[1][1]; }
    }
}

static int at3_iniciar(At3& q, int canales, int bloque, int conjunto) {
    at3_tablas();
    memset(&q, 0, sizeof(q));
    if (canales < 1 || canales > 2 || bloque <= 0 || bloque > 4096) return -1;
    if (bloque != 96 * canales && bloque != 152 * canales && bloque != 192 * canales) return -1;
    if (conjunto && canales != 2) return -1;
    q.canales = canales; q.bloque = bloque; q.conjunto = conjunto;
    q.peso[0] = 0; q.peso[1] = 7; q.peso[2] = 0; q.peso[3] = 7; q.peso[4] = 0; q.peso[5] = 7;
    for (int i = 0; i < 4; i++) q.mprev[i] = q.mahora[i] = q.msig[i] = 3;
    q.listo = 1;
    return 0;
}

// devuelve 0 y deja 1024 muestras por canal en q.sal; -1 si el cuadro es invalido
static int at3_decodificar(At3& q, const uint8_t* d) {
    if (!q.listo) return -1;
    int ch = q.canales;
    if (q.conjunto) {
        At3Bits b{d, q.bloque * 8, 0};
        if (at3_unidad(q, b, q.u[0], q.sal[0], 0, 1)) return -1;
        for (int i = 0; i < q.bloque; i++) q.volteado[i] = d[q.bloque - 1 - i];
        const uint8_t* p = q.volteado;
        for (int i = 4; *p == 0xF8; i++, p++) if (i >= q.bloque) return -1;
        At3Bits b2{p, (int)(q.volteado + q.bloque - p) * 8, 0};
        memmove(q.peso, q.peso + 2, 4 * sizeof(int));
        q.peso[4] = (int)b2.leer(1); q.peso[5] = (int)b2.leer(3);
        for (int i = 0; i < 4; i++) { q.mprev[i] = q.mahora[i]; q.mahora[i] = q.msig[i]; q.msig[i] = (int)b2.leer(2); }
        if (at3_unidad(q, b2, q.u[1], q.sal[1], 1, 1)) return -1;
        at3_desmatriz(q.sal[0], q.sal[1], q.mprev, q.mahora);
        at3_ponderar(q.sal[0], q.sal[1], q.peso);
    } else {
        for (int i = 0; i < ch; i++) {
            At3Bits b{d + i * q.bloque / ch, q.bloque * 8 / ch, 0};
            if (at3_unidad(q, b, q.u[i], q.sal[i], i, 0)) return -1;
        }
    }
    for (int i = 0; i < ch; i++) {
        float* p1 = q.sal[i];
        at3_iqmf(p1, p1 + 256, 256, p1, q.u[i].retardo1, q.tmp);
        at3_iqmf(p1 + 768, p1 + 512, 256, p1 + 512, q.u[i].retardo2, q.tmp);
        at3_iqmf(p1, p1 + 512, 512, p1, q.u[i].retardo3, q.tmp);
    }
    return 0;
}
