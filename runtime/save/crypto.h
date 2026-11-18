#pragma once
#include <cstdint>
#include <cstring>

// PSP savedata decryption. Algorithm and keys derived from kirk-engine
// (Draan and contributors, GPL v3) and from the sceChnnlsv implementation in PPSSPP.
// Only decryption is needed, and only six of the 128 kirk keys.

static const uint8_t KIRK_K4[16]   = {0x99, 0x24, 0x4C, 0xD2, 0x58, 0xF5, 0x1B, 0xCB, 0xB0, 0x61, 0x9C, 0xA7, 0x38, 0x30, 0x07, 0x5F};
static const uint8_t KIRK_K14[16]  = {0xC8, 0x71, 0xFD, 0xB3, 0xBC, 0xC5, 0xD2, 0xF2, 0xE2, 0xD7, 0x72, 0x9D, 0xDF, 0x82, 0x68, 0x82};
static const uint8_t KIRK_K18[16]  = {0x5D, 0xC7, 0x11, 0x39, 0xD0, 0x19, 0x38, 0xBC, 0x02, 0x7F, 0xDD, 0xDC, 0xB0, 0x83, 0x7D, 0x9D};
static const uint8_t KIRK_K83[16]  = {0xAF, 0xFE, 0x8E, 0xB1, 0x3D, 0xD1, 0x7E, 0xD8, 0x0A, 0x61, 0x24, 0x1C, 0x95, 0x92, 0x56, 0xB6};
static const uint8_t KIRK_K87[16]  = {0x1C, 0x9B, 0xC4, 0x90, 0xE3, 0x06, 0x64, 0x81, 0xFA, 0x59, 0xFD, 0xB6, 0x00, 0xBB, 0x28, 0x70};
static const uint8_t KIRK_K100[16] = {0x03, 0xB3, 0x02, 0xE8, 0x5F, 0xF3, 0x81, 0xB1, 0x3B, 0x8D, 0xAA, 0x2A, 0x90, 0xFF, 0x5E, 0x61};

static const uint8_t CHN_199C[16] = {0x36, 0xA5, 0x3E, 0xAC, 0xC5, 0x26, 0x9E, 0xA3, 0x83, 0xD9, 0xEC, 0x25, 0x6C, 0x48, 0x48, 0x72};
static const uint8_t CHN_19AC[16] = {0xD8, 0xC0, 0xB0, 0xF3, 0x3E, 0x6B, 0x76, 0x85, 0xFD, 0xFB, 0x4D, 0x7D, 0x45, 0x1E, 0x92, 0x03};
static const uint8_t CHN_19CC[16] = {0x70, 0x44, 0xA3, 0xAE, 0xEF, 0x5D, 0xA5, 0xF2, 0x85, 0x7F, 0xF2, 0xD6, 0x94, 0xF5, 0x36, 0x3B};
static const uint8_t CHN_19DC[16] = {0xEC, 0x6D, 0x29, 0x59, 0x26, 0x35, 0xA5, 0x7F, 0x97, 0x2A, 0x0D, 0xBC, 0xA3, 0x26, 0x33, 0x00};

// --- AES 128 ---

struct Aes { uint8_t rk[176]; };

static uint8_t aes_sbox[256], aes_inv[256];
static int aes_tables_done = 0;

static uint8_t aes_mul(uint8_t a, uint8_t b) {
    uint8_t r = 0;
    while (b) {
        if (b & 1) r ^= a;
        a = (uint8_t)((a << 1) ^ ((a & 0x80) ? 0x1B : 0));
        b >>= 1;
    }
    return r;
}

static void aes_tables() {
    if (aes_tables_done) return;
    aes_tables_done = 1;
    uint8_t p = 1, q = 1;
    do {
        p = (uint8_t)(p ^ (p << 1) ^ ((p & 0x80) ? 0x1B : 0));
        q ^= (uint8_t)(q << 1);
        q ^= (uint8_t)(q << 2);
        q ^= (uint8_t)(q << 4);
        if (q & 0x80) q ^= 0x09;
        uint8_t v = (uint8_t)(q ^ (uint8_t)((q << 1) | (q >> 7)) ^ (uint8_t)((q << 2) | (q >> 6)) ^
                              (uint8_t)((q << 3) | (q >> 5)) ^ (uint8_t)((q << 4) | (q >> 4)) ^ 0x63);
        aes_sbox[p] = v;
    } while (p != 1);
    aes_sbox[0] = 0x63;
    for (int i = 0; i < 256; i++) aes_inv[aes_sbox[i]] = (uint8_t)i;
}

static void aes_key(Aes& a, const uint8_t* k) {
    aes_tables();
    memcpy(a.rk, k, 16);
    uint8_t rcon = 1;
    for (int i = 16; i < 176; i += 4) {
        uint8_t t[4];
        memcpy(t, a.rk + i - 4, 4);
        if (i % 16 == 0) {
            uint8_t s = t[0];
            t[0] = (uint8_t)(aes_sbox[t[1]] ^ rcon);
            t[1] = aes_sbox[t[2]];
            t[2] = aes_sbox[t[3]];
            t[3] = aes_sbox[s];
            rcon = aes_mul(rcon, 2);
        }
        for (int j = 0; j < 4; j++) a.rk[i + j] = (uint8_t)(a.rk[i - 16 + j] ^ t[j]);
    }
}

static void aes_block_inv(const Aes& a, uint8_t* s) {
    for (int j = 0; j < 16; j++) s[j] ^= a.rk[160 + j];
    for (int r = 9; r >= 0; r--) {
        uint8_t t[16];
        for (int c = 0; c < 4; c++)
            for (int f = 0; f < 4; f++) t[((c + f) & 3) * 4 + f] = s[c * 4 + f];
        for (int j = 0; j < 16; j++) s[j] = aes_inv[t[j]];
        for (int j = 0; j < 16; j++) s[j] ^= a.rk[r * 16 + j];
        if (r) {
            for (int c = 0; c < 4; c++) {
                uint8_t* p = s + c * 4;
                uint8_t b0 = p[0], b1 = p[1], b2 = p[2], b3 = p[3];
                p[0] = (uint8_t)(aes_mul(b0, 14) ^ aes_mul(b1, 11) ^ aes_mul(b2, 13) ^ aes_mul(b3, 9));
                p[1] = (uint8_t)(aes_mul(b0, 9) ^ aes_mul(b1, 14) ^ aes_mul(b2, 11) ^ aes_mul(b3, 13));
                p[2] = (uint8_t)(aes_mul(b0, 13) ^ aes_mul(b1, 9) ^ aes_mul(b2, 14) ^ aes_mul(b3, 11));
                p[3] = (uint8_t)(aes_mul(b0, 11) ^ aes_mul(b1, 13) ^ aes_mul(b2, 9) ^ aes_mul(b3, 14));
            }
        }
    }
}

static void aes_cbc_dec(const uint8_t* k, const uint8_t* src, uint8_t* dst, int n) {
    Aes a;
    aes_key(a, k);
    uint8_t iv[16];
    memset(iv, 0, 16);
    for (int i = 0; i < n; i += 16) {
        uint8_t ct[16], b[16];
        memcpy(ct, src + i, 16);
        memcpy(b, ct, 16);
        aes_block_inv(a, b);
        for (int j = 0; j < 16; j++) dst[i + j] = (uint8_t)(b[j] ^ iv[j]);
        memcpy(iv, ct, 16);
    }
}

// --- chnnlsv ---

static const uint8_t* kirk_key(int n) {
    switch (n) {
    case 4: return KIRK_K4;
    case 14: return KIRK_K14;
    case 18: return KIRK_K18;
    case 83: return KIRK_K83;
    case 87: return KIRK_K87;
    case 100: return KIRK_K100;
    default: return nullptr;
    }
}

static bool kirk7(uint8_t* buf, int num, int size) {
    const uint8_t* k = kirk_key(num);
    if (!k) return false;
    aes_cbc_dec(k, buf + 20, buf, size);
    return true;
}

static void mem_xor(uint8_t* d, const uint8_t* s, int n) { for (int i = 0; i < n; i++) d[i] ^= s[i]; }
static int chn_type(int m) { return (m == 1 || m == 2) ? 83 : ((m == 3 || m == 4) ? 87 : 100); }
static int chn_num2(int m) { return m == 1 ? 4 : (m == 3 ? 14 : 18); }
static void le32(uint8_t* d, uint32_t v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)(v >> 16); d[3] = (uint8_t)(v >> 24); }

static bool chn_block(uint8_t* out, uint8_t* data, int length, const uint8_t* ctx, uint32_t& counter, int mode) {
    int t = chn_type(mode);
    memcpy(out + 20, ctx, 16);
    if (t == 87) mem_xor(out + 20, CHN_19AC, 16);
    else if (t == 100) mem_xor(out + 20, CHN_19DC, 16);
    if (mode == 2 || mode == 4 || mode == 6) return false;      // needs the console fuses
    if (!kirk7(out, chn_num2(mode), 16)) return false;
    if (t == 87) mem_xor(out, CHN_199C, 16);
    else if (t == 100) mem_xor(out, CHN_19CC, 16);
    uint8_t sp16[16], sp0[16];
    memcpy(sp16, out, 16);
    if (counter == 1) memset(sp0, 0, 16);
    else { memcpy(sp0, sp16, 12); le32(sp0 + 12, counter - 1); }
    for (int i = 20; i < length + 20; i += 16) { memcpy(out + i, sp16, 12); le32(out + i + 12, counter); counter++; }
    if (!kirk7(out, t, length)) return false;
    mem_xor(out, sp0, 16);
    mem_xor(data, out, length);
    return true;
}

// decrypts in place; returns the payload size or -1, the result starts at offset 0
static int psp_decrypt_save(uint8_t* data, int total, int mode, const uint8_t* key) {
    if (total <= 0x10 || (total & 0xF)) return -1;
    int length = total - 0x10;
    uint8_t ctx[16];
    memcpy(ctx, data, 16);
    if (key) mem_xor(ctx, key, 16);
    uint32_t counter = 1;
    static uint8_t out[20 + 2048];
    int off = 0x10, left = length;
    while (left > 0) {
        int n = left >= 2048 ? 2048 : left;
        if (!chn_block(out, data + off, n, ctx, counter, mode)) return -1;
        off += n;
        left -= n;
    }
    memmove(data, data + 0x10, (size_t)length);
    return length;
}
