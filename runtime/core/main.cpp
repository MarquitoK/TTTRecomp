#include "psp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static bool load_elf(const char* file_path) {
    FILE* f = fopen(file_path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* d = (uint8_t*)malloc(n);
    if (fread(d, 1, n, f) != (size_t)n) { fclose(f); free(d); return false; }
    fclose(f);
    uint32_t phoff; memcpy(&phoff, d + 0x1C, 4);
    uint16_t phnum; memcpy(&phnum, d + 0x2C, 2);
    for (int i = 0; i < phnum; i++) {
        uint32_t t, off, va, pa, fsz, msz;
        memcpy(&t, d + phoff + 32 * i, 4);
        memcpy(&off, d + phoff + 32 * i + 4, 4);
        memcpy(&va, d + phoff + 32 * i + 8, 4);
        memcpy(&pa, d + phoff + 32 * i + 12, 4);
        memcpy(&fsz, d + phoff + 32 * i + 16, 4);
        memcpy(&msz, d + phoff + 32 * i + 20, 4);
        if (t != 1) continue;
        memcpy(psp_p(va), d + off, fsz);
        if (msz > fsz) memset(psp_p(va + fsz), 0, msz - fsz);
    }
    free(d);
    return true;
}

int main(int argc, char** argv) {
    const char* eboot = argc > 1 ? argv[1] : "../../game/EBOOT.OLD";
    psp_mem_init(0x10000000u);
    if (!load_elf(eboot)) { fprintf(stderr, "no puedo cargar %s\n", eboot); return 1; }
    long total = 0;
    for (int i = 0; i < BANKS_N; i++) {
        printf("banco %-14s indice %-6d %5d funciones\n", BANKS[i].nombre, BANKS[i].indice, BANKS[i].n);
        total += BANKS[i].n;
    }
    printf("total %ld funciones, %d importaciones\n", total, psp_imports_n());
    const uint32_t sample[] = {0x08804040u, 0x089C34E8u, 0x08A57200u, 0x08A0717Cu};
    for (uint32_t a : sample)
        printf("resolver 0x%08X -> %s\n", a, psp_resolve(a) ? "ok" : "no");
    psp_bank(23);
    printf("banco 23 activo: %d, 0x08A814A0 -> %s\n", psp_bank_current(),
           psp_resolve(0x08A814A0u) ? "ok" : "no");
    psp_mem_free();
    return 0;
}

void psp_hle(Ctx& c, int id) {
    if (id >= 0 && id < 4096) psp_hle_count[id]++;
    c.r[2] = 0;
    c.r[3] = 0;
}
