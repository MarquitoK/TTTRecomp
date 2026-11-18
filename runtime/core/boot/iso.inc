// ISO9660: finds a file by path and returns its sector and size
static bool iso_buscar(const char* file_path, uint32_t* lbn, uint32_t* size_b) {
    char p[512];
    snprintf(p, sizeof(p), "%s/game.iso", GAME_DIR);
    FILE* f = fopen(p, "rb");
    if (!f) return false;
    uint8_t sec[2048];
    fseek(f, 16 * 2048, SEEK_SET);
    if (fread(sec, 1, 2048, f) != 2048 || sec[0] != 1 || memcmp(sec + 1, "CD001", 5)) { fclose(f); return false; }
    uint32_t dir_lbn, dir_size;
    memcpy(&dir_lbn, sec + 156 + 2, 4);
    memcpy(&dir_size, sec + 156 + 10, 4);

    const char* q = strchr(file_path, ':');
    q = q ? q + 1 : file_path;
    while (*q == '/') q++;
    bool ok = false;
    while (*q) {
        char nombre[64];
        int k = 0;
        while (q[k] && q[k] != '/' && k < 63) { nombre[k] = q[k]; k++; }
        nombre[k] = 0;
        const char* next = q[k] == '/' ? q + k + 1 : q + k;
        ok = false;
        for (uint32_t off = 0; off < dir_size && !ok; off += 2048) {
            fseek(f, (long)dir_lbn * 2048 + off, SEEK_SET);
            if (fread(sec, 1, 2048, f) != 2048) break;
            uint32_t i = 0;
            while (i < 2048) {
                uint8_t len = sec[i];
                if (!len) break;
                uint8_t nlen = sec[i + 32];
                char nm[64];
                uint8_t c = nlen < 63 ? nlen : 63;
                memcpy(nm, sec + i + 33, c);
                nm[c] = 0;
                char* pc = strchr(nm, ';');
                if (pc) *pc = 0;
                if (!strcmp(nm, nombre)) {
                    memcpy(&dir_lbn, sec + i + 2, 4);
                    memcpy(&dir_size, sec + i + 10, 4);
                    ok = true;
                    break;
                }
                i += len;
            }
        }
        if (!ok) break;
        q = next;
    }
    fclose(f);
    if (!ok) return false;
    *lbn = dir_lbn;
    *size_b = dir_size;
    return true;
}

#include "disc/disc.h"


static int io_open(const char* file_path) {
    char p[512];
    long long base = 0, size_b = 0;
    int sectors = 0, virt = 0;
    unsigned lbn = 0, sz = 0;
    if (disc_ready) {
        uint32_t l = 0, t = 0;
        if (sscanf(file_path, "disc0:/sce_lbn0x%x_size0x%x", &lbn, &sz) == 2) { base = (long long)lbn * 2048; size_b = sz; virt = 1; }
        else if (!strncmp(file_path, "umd", 3)) { sectors = 1; virt = 1; size_b = 0x7FFFFFFFFFFLL; }
        else if (disc_find(file_path, &l, &t)) { base = (long long)l * 2048; size_b = t; virt = 1; }
        if (virt) {
            for (int i = 1; i < 32; i++)
                if (!files[i].f && !files[i].virtual_) {
                    files[i].f = nullptr; files[i].base = base; files[i].size_b = size_b; files[i].pos = 0;
                    files[i].sectors = sectors; files[i].virtual_ = 1;
                    if (n_open++ < 6) printf("abierto %.90s -> disco virtual (%lld bytes%s)\n", file_path, size_b, sectors ? ", modo sector" : "");
                    return i;
                }
            return -1;
        }
    }
    if (sscanf(file_path, "disc0:/sce_lbn0x%x_size0x%x", &lbn, &sz) == 2) {
        snprintf(p, sizeof(p), "%s/game.iso", GAME_DIR);
        base = (long long)lbn * 2048;
        size_b = sz;
    } else if (!strncmp(file_path, "umd", 3)) {
        snprintf(p, sizeof(p), "%s/game.iso", GAME_DIR);
        sectors = 1;                        // umd0:/umd1: raw: lseek and read in 2048-byte sectors
    } else {
        uint32_t l = 0, t = 0;
        if (iso_buscar(file_path, &l, &t)) {
            snprintf(p, sizeof(p), "%s/game.iso", GAME_DIR);
            base = (long long)l * 2048;
            size_b = t;
        } else {
            const char* bar = strrchr(file_path, '/');
            snprintf(p, sizeof(p), "%s/%s", GAME_DIR, bar ? bar + 1 : file_path);
        }
    }
    FILE* f = fopen(p, "rb");
    if (!f) {
        if (iso_warned < 6) { printf("falta el archivo %s (pedido como %.80s)\n", p, file_path); iso_warned++; }
        return -1;
    }
    if (!size_b) { fseek(f, 0, SEEK_END); size_b = ftell(f); }
    for (int i = 1; i < 32; i++)
        if (!files[i].f && !files[i].virtual_) {
            files[i].f = f; files[i].base = base; files[i].size_b = size_b; files[i].pos = 0;
            files[i].sectors = sectors; files[i].virtual_ = 0;
            fseek(f, base, SEEK_SET);
            if (n_open++ < 6) printf("abierto %.90s -> %s (%lld bytes%s)\n", file_path, p, size_b, sectors ? ", modo sector" : "");
            return i;
        }
    fclose(f);
    return -1;
}

