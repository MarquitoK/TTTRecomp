#include "psp.h"
#include "perf.h"
#include "patches/all.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csetjmp>
#include <ctime>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#ifdef __linux__
#include <execinfo.h>
#endif

// boot probe: runs module_start and records which imports are requested and in what order

static uint32_t GP = 0;
static uint32_t STACK_SIZE = 0x0A000000u;
static uint32_t brk_free = 0x08C00000u;
static jmp_buf exit_jmp;
static int last_hle = -1;
static long total_hle = 0;
static long order_n = 0;
static int order[256];

struct Block { uint32_t dir, size_b; };
static Block blocks[512];
static int n_blocks = 0;



static bool load_elf(const char* file_path, uint32_t* gp_out) {
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
    uint32_t modinfo_off = 0;
    for (int i = 0; i < phnum; i++) {
        uint32_t t, off, va, pa, fsz, msz;
        memcpy(&t, d + phoff + 32 * i, 4);
        memcpy(&off, d + phoff + 32 * i + 4, 4);
        memcpy(&va, d + phoff + 32 * i + 8, 4);
        memcpy(&pa, d + phoff + 32 * i + 12, 4);
        memcpy(&fsz, d + phoff + 32 * i + 16, 4);
        memcpy(&msz, d + phoff + 32 * i + 20, 4);
        if (t != 1) continue;
        if (i == 0) modinfo_off = pa & 0x7FFFFFFFu;
        memcpy(psp_p(va), d + off, fsz);
        if (msz > fsz) memset(psp_p(va + fsz), 0, msz - fsz);
    }
    if (modinfo_off && gp_out) memcpy(gp_out, d + modinfo_off + 32, 4);
    free(d);
    return true;
}

static uint32_t reserve_mem(uint32_t size_b, uint32_t align) {
    if (align < 64) align = 64;
    brk_free = (brk_free + align - 1) & ~(align - 1);
    uint32_t d = brk_free;
    brk_free += (size_b + 63) & ~63u;
    if (brk_free > 0x0E000000u) printf("aviso: la memoria del juego pasa de 0x0E000000 y pisa la zona volatil (baja TTT_HEAP_KB)\n");
    if (n_blocks < 512) { blocks[n_blocks].dir = d; blocks[n_blocks].size_b = size_b; n_blocks++; }
    return d;
}

// port reserve: grows from the game's blocks up to the volatile zone (0x0E000000); never returned
uint32_t psp_reserve_left() { return brk_free < 0x0E000000u ? 0x0E000000u - brk_free : 0; }
uint32_t psp_reserve(uint32_t n) {
    static int logs = 0;
    if (n > psp_reserve_left() && logs++ < 20) { printf("memoria: reserva de %u KB con solo %u KB libres (tope 0x%08X)\n", n / 1024, psp_reserve_left() / 1024, brk_free); fflush(stdout); }
    if ((uint64_t)brk_free + n + 128 > 0x10000000ull) { printf("memoria: reserva de %u KB imposible, no se da\n", n / 1024); fflush(stdout); return 0; }
    uint32_t d = reserve_mem(n, 64); memset(psp_p(d), 0, n); return d;
}
#include <map>
static std::map<uint32_t, uint32_t> pa_free, pa_used;
static uint32_t pa_total = 0;
uint32_t psp_alloc(uint32_t n) {
    n = (n + 63) & ~63u;
    for (int pass = 0; pass < 2; pass++) {
        for (auto it = pa_free.begin(); it != pa_free.end(); ++it) {
            if (it->second < n) continue;
            uint32_t a = it->first, left = it->second - n;
            pa_free.erase(it);
            if (left) pa_free[a + n] = left;
            pa_used[a] = n; pa_total += n;
            memset(psp_p(a), 0, n);
            return a;
        }
        uint32_t chunk = n > 0x400000u ? n : 0x400000u;
        if (chunk > psp_reserve_left()) chunk = n;
        uint32_t r = psp_reserve(chunk);
        if (!r) return 0;
        auto pv = pa_free.empty() ? pa_free.end() : std::prev(pa_free.end());
        if (pv != pa_free.end() && pv->first + pv->second == r) pv->second += chunk; else pa_free[r] = chunk;   // chunks are contiguous: one free run
    }
    return 0;
}
void psp_gpu_tex_forget(uint32_t a, uint32_t n);
void psp_free(uint32_t a) {
    auto it = pa_used.find(a);
    if (it == pa_used.end()) return;
    uint32_t b = a, n = it->second;
    psp_gpu_tex_forget(a, n);
    pa_total -= n; pa_used.erase(it);
    auto nx = pa_free.find(b + n);
    if (nx != pa_free.end()) { n += nx->second; pa_free.erase(nx); }
    auto pv = pa_free.lower_bound(b);
    if (pv != pa_free.begin()) { --pv; if (pv->first + pv->second == b) { b = pv->first; n += pv->second; pa_free.erase(pv); } }
    pa_free[b] = n;
}
uint32_t psp_alloc_used() { return pa_total; }
// the game heap arena (the partition block whose size is the KB word 0x08A785D0)
bool psp_heap_arena(uint32_t* a, uint32_t* e) {
    uint32_t want = psp_l32(0x08A785D0u) * 1024u;
    for (int i = 0; i < n_blocks; i++) if (blocks[i].size_b == want) { *a = blocks[i].dir; *e = blocks[i].dir + blocks[i].size_b; return true; }
    return false;
}

