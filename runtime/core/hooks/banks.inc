#include "psp.h"
#include "perf.h"
#include "patches/all.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>

uint8_t* psp_mem = nullptr;
uint32_t psp_mem_mask = 0x0FFFFFFFu;
std::atomic<int> watchdog_pause{0};

static int active_bank = -1;
uint64_t psp_hle_count[4096];
uint32_t psp_unresolved = 0;
int psp_jalr_trace = 0;

void psp_mem_init(uint32_t tam) {
    // always 512 MB: render targets from 0x10000000 (2 MB times N*N); heap and volatile zone stay below 0x10000000 for the GE
    if (tam < 0x20000000u) { tam = 0x20000000u; psp_mem_mask = 0x1FFFFFFFu; }
    psp_mem = (uint8_t*)calloc(1, (size_t)tam + 0x10000);   // slack for accesses at the edge
    if (!psp_mem) { fprintf(stderr, "sin memoria\n"); exit(1); }
}

void psp_mem_free() { free(psp_mem); psp_mem = nullptr; }

// address resolution: open hash table. TTT_RESOLVE=0 restores the linear scan
struct Cell { uint32_t dir; int32_t imp; Fn fn; };
static Cell* h_pri = nullptr; static uint32_t m_pri = 0;
static Cell* h_ban = nullptr; static uint32_t m_ban = 0;
static int h_ban_idx = -2;
static int resolve_var = -2;

static bool resolve_hash() {
    if (resolve_var == -2) { const char* e = getenv("TTT_RESOLVE"); resolve_var = e ? atoi(e) : 1; }
    return resolve_var != 0;
}

static inline uint32_t scrambled(uint32_t d) { return (d >> 2) * 2654435761u; }

static Cell* build_table(uint32_t n, uint32_t* mask) {
    uint32_t cap = 16;
    while (cap < n * 2) cap <<= 1;
    *mask = cap - 1;
    return (Cell*)calloc(cap, sizeof(Cell));
}

static void insert_cell(Cell* h, uint32_t mask, uint32_t dir, int32_t imp, Fn fn) {
    uint32_t i = scrambled(dir) & mask;
    while (h[i].dir) { if (h[i].dir == dir) return; i = (i + 1) & mask; }
    h[i].dir = dir; h[i].imp = imp; h[i].fn = fn;
}

static void main_table() {
    if (h_pri) return;
    h_pri = build_table((uint32_t)(BANKS[0].n + IMPORTS_N), &m_pri);
    for (int i = 0; i < BANKS[0].n; i++) insert_cell(h_pri, m_pri, BANKS[0].tab[i].dir, -1, BANKS[0].tab[i].fn);
    for (int i = 0; i < IMPORTS_N; i++) insert_cell(h_pri, m_pri, IMPORTS[i].stub, i, nullptr);
}

static void bank_table() {
    if (h_ban_idx == active_bank) return;
    h_ban_idx = active_bank;
    free(h_ban); h_ban = nullptr; m_ban = 0;
    if (active_bank < 0) return;
    const Bank& b = BANKS[active_bank];
    h_ban = build_table((uint32_t)b.n, &m_ban);
    for (int i = 0; i < b.n; i++) insert_cell(h_ban, m_ban, b.tab[i].dir, -1, b.tab[i].fn);
}

static const Cell* peek(uint32_t dir) {
    Cell* h; uint32_t mask;
    if (dir >= PSP_BANK_VA && dir < PSP_BANK_END) { h = h_ban; mask = m_ban; }
    else { main_table(); h = h_pri; mask = m_pri; }
    if (!h) return nullptr;
    uint32_t i = scrambled(dir) & mask;
    while (h[i].dir) {
        if (h[i].dir == dir) return &h[i];
        i = (i + 1) & mask;
    }
    return nullptr;
}

void psp_bank(int indice_packfile) {
    active_bank = -1;
    for (int i = 1; i < BANKS_N; i++)
        if (BANKS[i].indice == indice_packfile) { active_bank = i; break; }
    bank_table();
}

int psp_bank_current() { return active_bank; }

static Fn lookup(const Bank& b, uint32_t dir) {
    int lo = 0, hi = b.n - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (b.tab[m].dir == dir) return b.tab[m].fn;
        if (b.tab[m].dir < dir) lo = m + 1; else hi = m - 1;
    }
    return nullptr;
}

