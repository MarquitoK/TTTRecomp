#pragma once
#include "psp.h"
#include "save/crypto.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <direct.h>
#define SAV_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define SAV_MKDIR(p) mkdir(p, 0755)
#endif

// Real sceUtilitySavedata. Data is stored in clear because on the PSP this same function
// encrypted it; the game hands over and expects the plain buffer.
// The layout mirrors the console so encryption can be added later without rework.

#define SAV_GAME 60
#define SAV_NAME 76
#define SAV_FILE 100
#define SAV_BUF 116
#define SAV_BUFSIZE 120
#define SAV_SIZE 124
#define SAV_TITLE 128
#define SAV_SUBTITLE 256
#define SAV_DETAIL 384
#define SAV_LEVEL 1408
#define SAV_MSFREE 1488
#define SAV_MSDATA 1492
#define SAV_USED 1496
#define SAV_SIZES 1532
#define SAV_KEY 1500
#define SAV_VERSION 1516

#define SAV_NO_DATA 0x80110307u

static const char* sav_root() {
    static char r[256] = "";
    if (!r[0]) {
        const char* e = getenv("TTT_SAVE_DIR");
        snprintf(r, sizeof(r), "%s", e && *e ? e : "save");
        std::error_code ec;
        if (!(e && *e) && !std::filesystem::exists("save", ec) && std::filesystem::exists("guardado", ec)) std::filesystem::rename("guardado", "save", ec);   // folder name before the English rename
        SAV_MKDIR(r);
    }
    return r;
}

static void sav_string(uint32_t p, uint32_t off, char* d, int cap) {
    const uint8_t* b = psp_p(p + off);
    int i = 0;
    for (; i < cap - 1 && b[i] >= 0x20 && b[i] < 0x7F; i++) d[i] = (char)b[i];
    d[i] = 0;
}

// the folder is gameName + saveName, as on the console
static bool sav_dir(uint32_t p, char* d, int cap) {
    char g[24], s[32];
    sav_string(p, SAV_GAME, g, sizeof(g));
    sav_string(p, SAV_NAME, s, sizeof(s));
    if (!g[0]) return false;
    for (char* q = s; *q; q++) if (*q == '*' || *q == '?' || *q == '/' || *q == '\\') *q = 0;
    snprintf(d, cap, "%s/%s%s", sav_root(), g, s);
    return true;
}

static void sav_path(uint32_t p, const char* file, char* d, int cap) {
    char dir[320];
    sav_dir(p, dir, sizeof(dir));
    snprintf(d, cap, "%s/%s", dir, file);
}

// --- PARAM.SFO ---

struct SfoField { const char* key; int fmt; const char* txt; uint32_t num; uint32_t cap; };

static void sav_write_sfo(const char* path, uint32_t p, const char* dirname, const char* file) {
    char title[160], subtitle[160], detail[1088];
    sav_string(p, SAV_TITLE, title, sizeof(title));
    sav_string(p, SAV_SUBTITLE, subtitle, sizeof(subtitle));
    const uint8_t* b = psp_p(p + SAV_DETAIL);
    int n = 0;
    while (n < 1023 && b[n] && n < 1024) n++;
    memcpy(detail, b, (size_t)n);
    detail[n] = 0;
    uint32_t level = psp_l32(p + SAV_LEVEL) & 0xFF;

    SfoField c[7] = {
        {"CATEGORY", 0x0204, "MS", 0, 4},
        {"PARENTAL_LEVEL", 0x0404, nullptr, level, 4},
        {"SAVEDATA_DETAIL", 0x0204, detail, 0, 1024},
        {"SAVEDATA_DIRECTORY", 0x0204, dirname, 0, 64},
        {"SAVEDATA_FILE_LIST", 0x0204, file, 0, 3168},
        {"SAVEDATA_TITLE", 0x0204, subtitle, 0, 128},
        {"TITLE", 0x0204, title, 0, 128},
    };

    uint32_t keys_size = 0;
    for (int i = 0; i < 7; i++) keys_size += (uint32_t)strlen(c[i].key) + 1;
    uint32_t keys_pad = (keys_size + 3) & ~3u;
    uint32_t keys_start = 20 + 7 * 16;
    uint32_t data_start = keys_start + keys_pad;
    uint32_t data_size = 0;
    for (int i = 0; i < 7; i++) data_size += c[i].cap;

    uint8_t* f = (uint8_t*)calloc(1, data_start + data_size);
    if (!f) return;
    uint32_t hdr[5] = {0x46535000u, 0x00000101u, keys_start, data_start, 7};
    memcpy(f, hdr, 20);
    uint32_t ko = 0, dof = 0;
    for (int i = 0; i < 7; i++) {
        uint32_t len = c[i].fmt == 0x0404 ? 4 : (uint32_t)strlen(c[i].txt ? c[i].txt : "") + 1;
        if (len > c[i].cap) len = c[i].cap;
        uint8_t e[16];
        memcpy(e + 0, &ko, 2);
        uint16_t fm = (uint16_t)c[i].fmt;
        memcpy(e + 2, &fm, 2);
        memcpy(e + 4, &len, 4);
        memcpy(e + 8, &c[i].cap, 4);
        memcpy(e + 12, &dof, 4);
        memcpy(f + 20 + i * 16, e, 16);
        strcpy((char*)f + keys_start + ko, c[i].key);
        if (c[i].fmt == 0x0404) memcpy(f + data_start + dof, &c[i].num, 4);
        else memcpy(f + data_start + dof, c[i].txt ? c[i].txt : "", len);
        ko += (uint32_t)strlen(c[i].key) + 1;
        dof += c[i].cap;
    }
    FILE* h = fopen(path, "wb");
    if (h) { fwrite(f, 1, data_start + data_size, h); fclose(h); }
    free(f);
}

// --- operations ---

static int sav_crypt_mode(const char* dir);

static uint32_t sav_save(uint32_t p) {
    char dir[320], path[400], file[24], dirname[64];
    if (!sav_dir(p, dir, sizeof(dir))) return SAV_NO_DATA;
    SAV_MKDIR(dir);
    sav_string(p, SAV_FILE, file, sizeof(file));
    if (!file[0]) snprintf(file, sizeof(file), "DATA.BIN");
    char g[24], s[32];
    sav_string(p, SAV_GAME, g, sizeof(g));
    sav_string(p, SAV_NAME, s, sizeof(s));
    snprintf(dirname, sizeof(dirname), "%s%s", g, s);

    uint32_t buf = psp_l32(p + SAV_BUF), size = psp_l32(p + SAV_SIZE);
    if (!buf || !size || size > 0x800000u) { printf("save: parametros invalidos, buf 0x%08X tam %u\n", buf, size); return SAV_NO_DATA; }
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    FILE* h = fopen(path, "wb");
    if (!h) { printf("save: no puedo escribir %s\n", path); return SAV_NO_DATA; }
    fwrite(psp_p(buf), 1, size, h);
    fclose(h);
    snprintf(path, sizeof(path), "%s/PARAM.SFO", dir);
    sav_write_sfo(path, p, dirname, file);
    printf("save: %s/%s, %u bytes\n", dir, file, size);
    fflush(stdout);
    return 0;
}

// encryption mode from the first byte of SAVEDATA_PARAMS in PARAM.SFO
static int sav_crypt_mode(const char* dir) {
    char path[400];
    snprintf(path, sizeof(path), "%s/PARAM.SFO", dir);
    FILE* h = fopen(path, "rb");
    if (!h) return 0;
    fseek(h, 0, SEEK_END);
    long n = ftell(h);
    fseek(h, 0, SEEK_SET);
    if (n < 20 || n > 1 << 20) { fclose(h); return 0; }
    uint8_t* f = (uint8_t*)malloc((size_t)n);
    fread(f, 1, (size_t)n, h);
    fclose(h);
    int mode = 0;
    uint32_t kt, dt, cnt;
    memcpy(&kt, f + 8, 4); memcpy(&dt, f + 12, 4); memcpy(&cnt, f + 16, 4);
    if (cnt < 64 && kt < (uint32_t)n && dt < (uint32_t)n) {
        for (uint32_t i = 0; i < cnt; i++) {
            uint8_t* e = f + 20 + i * 16;
            uint16_t ko; uint32_t dof;
            memcpy(&ko, e, 2); memcpy(&dof, e + 12, 4);
            const char* cl = (const char*)(f + kt + ko);
            if (kt + ko < (uint32_t)n && !strncmp(cl, "SAVEDATA_PARAMS", 15) && dt + dof < (uint32_t)n) {
                uint8_t v = f[dt + dof];
                mode = v == 0x01 ? 1 : (v == 0x21 ? 3 : (v == 0x41 ? 5 : (v ? 1 : 0)));
            }
        }
    }
    free(f);
    return mode;
}

static uint32_t sav_load(uint32_t p) {
    char path[400], file[24];
    sav_string(p, SAV_FILE, file, sizeof(file));
    if (!file[0]) snprintf(file, sizeof(file), "DATA.BIN");
    sav_path(p, file, path, sizeof(path));
    FILE* h = fopen(path, "rb");
    if (!h) { printf("save: no existe %s\n", path); fflush(stdout); return SAV_NO_DATA; }
    fseek(h, 0, SEEK_END);
    long n = ftell(h);
    fseek(h, 0, SEEK_SET);
    uint32_t buf = psp_l32(p + SAV_BUF), cap = psp_l32(p + SAV_BUFSIZE);
    if (!buf || n <= 0) { fclose(h); return SAV_NO_DATA; }
    char dir[320];
    sav_dir(p, dir, sizeof(dir));
    int mode = sav_crypt_mode(dir);
    if (mode) {
        uint8_t* tmp = (uint8_t*)malloc((size_t)n);
        fread(tmp, 1, (size_t)n, h);
        fclose(h);
        int r = psp_decrypt_save(tmp, (int)n, mode, psp_p(p + SAV_KEY));
        if (r < 0) { free(tmp); printf("save: no puedo descifrar %s (modo %d)\n", path, mode); fflush(stdout); return SAV_NO_DATA; }
        n = r;
        if (cap && (uint32_t)n > cap) n = (long)cap;
        memcpy(psp_p(buf), tmp, (size_t)n);
        free(tmp);
        printf("save: cargado %s descifrando en modo %d, %ld bytes\n", path, mode, n);
    } else {
        if (cap && (uint32_t)n > cap) n = (long)cap;
        fread(psp_p(buf), 1, (size_t)n, h);
        fclose(h);
        printf("save: cargado %s, %ld bytes\n", path, n);
    }
    psp_s32(p + SAV_SIZE, (uint32_t)n);
    fflush(stdout);
    return 0;
}

static bool sav_exists(uint32_t p) {
    char path[400], file[24];
    sav_string(p, SAV_FILE, file, sizeof(file));
    if (!file[0]) snprintf(file, sizeof(file), "DATA.BIN");
    sav_path(p, file, path, sizeof(path));
    FILE* h = fopen(path, "rb");
    if (!h) return false;
    fclose(h);
    return true;
}

// GETSIZE checks the folder gameName + saveName, like PPSSPP
static bool sav_dir_exists(uint32_t p) {
    char dir[320];
    if (!sav_dir(p, dir, sizeof(dir))) return false;
    std::error_code ec;
    return std::filesystem::is_directory(dir, ec);
}

static void sav_text(uint32_t dir, const char* t) {
    for (int i = 0; i < 8; i++) psp_s8(dir + (uint32_t)i, i < (int)strlen(t) ? t[i] : 0);
}

// PspUtilitySavedataSizeInfo: +16 sector size, +20 free sectors, +24 free KB, +28 text, +36 needed KB, +48 overwrite KB
static uint32_t sav_sizes(uint32_t p) {
    uint32_t q = psp_l32(p + SAV_SIZES);
    if (!q) return 0;
    printf("save: tamanos pedidos, %d entradas seguras y %d normales\n", (int)psp_l32(q), (int)psp_l32(q + 4));
    psp_s32(q + 16, 32768);
    psp_s32(q + 20, 32768);
    psp_s32(q + 24, 1048576);
    sav_text(q + 28, "1 GB");
    psp_s32(q + 36, 0);
    psp_s32(q + 48, 0);
    return 0;
}

static uint32_t sav_info(uint32_t p) {
    uint32_t ms_free = psp_l32(p + SAV_MSFREE);
    if (ms_free) {
        psp_s32(ms_free + 0, 2048);
        psp_s32(ms_free + 4, 2048);
        psp_s32(ms_free + 8, 65536);
        sav_text(ms_free + 12, "64MB");
    }
    uint32_t data = psp_l32(p + SAV_MSDATA);
    if (data && !sav_exists(p)) return SAV_NO_DATA;
    return 0;
}

// modes: 0 AUTOLOAD, 1 AUTOSAVE, 2 LOAD, 3 SAVE, 4 LISTLOAD, 5 LISTSAVE,
//        6 LISTDELETE, 8 SIZES, 11 LIST, 12 FILES, 15 DELETEDATA, 22 GETSIZE
// the encryption key the game passes: needed to read original saves
static void sav_key(uint32_t p) {
    static int done = 0;
    if (done++ >= 3) return;
    const uint8_t* k = psp_p(p + SAV_KEY);
    int empty = 1;
    char hex[64];
    for (int i = 0; i < 16; i++) { snprintf(hex + i * 3, 4, "%02X ", k[i]); if (k[i]) empty = 0; }
    printf("save: clave %s| version segura %u | modo cifrado %s\n", empty ? "(toda a cero) " : hex,
           psp_l32(p + SAV_VERSION), empty ? "sin clave" : "con clave de juego");
    fflush(stdout);
}

static uint32_t sav_hle(uint32_t p, uint32_t mode) {
    if (!p) return 0;
    sav_key(p);
    {
        char dir[320], g[24], sn[32], ar[24];
        sav_dir(p, dir, sizeof(dir));
        sav_string(p, SAV_GAME, g, sizeof(g));
        sav_string(p, SAV_NAME, sn, sizeof(sn));
        sav_string(p, SAV_FILE, ar, sizeof(ar));
        printf("save: modo %u juego \"%s\" ranura \"%s\" archivo \"%s\" | buf 0x%08X tope %u tam %u | cifrado detectado %d\n",
               mode, g, sn, ar, psp_l32(p + SAV_BUF), psp_l32(p + SAV_BUFSIZE), psp_l32(p + SAV_SIZE), sav_crypt_mode(dir));
        fflush(stdout);
    }
    switch (mode) {
    case 1: case 3: case 5: return sav_save(p);
    case 0: case 2: case 4: return sav_load(p);
    case 8: return sav_info(p);
    case 22: {   // GETSIZE: no data when the save does not exist (the title then starts on New game; TTT_SAVE_GETSIZE=0 restores 0)
        uint32_t r = sav_sizes(p);
        const char* e = getenv("TTT_SAVE_GETSIZE");
        if (!(e && *e == '0') && !sav_dir_exists(p)) { printf("save: modo 22 sin partida guardada, se responde sin datos\n"); fflush(stdout); return 0x80110327u; }
        return r;
    }
    case 11: case 12: return sav_exists(p) ? 0 : SAV_NO_DATA;
    default: return 0;
    }
}
