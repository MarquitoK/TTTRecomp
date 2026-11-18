#ifdef _WIN32
#include <windows.h>
#include <vector>
#include <algorithm>
// TTT_WATCH_WRITE=addr  watchpoint: the page becomes read-only, every write raises
// an exception, the host instruction is recorded and mapped to a game function with the
// recompiler table. Only writes touching the watched address are printed.
static const int WATCH_MAX = 4;
static uint32_t watch_addrs[WATCH_MAX];
static uint8_t* watch_pages[WATCH_MAX];
static uint8_t* watch_ptrs[WATCH_MAX];
static int watch_n = 0, watch_hit = -1;
static uintptr_t watch_rip = 0;
static int watch_all = 0;
static uint32_t watch_addr = 0;

// TTT_MEM=0xADDR:N dumps N words from that address on every watchpoint hit
static uint32_t watch_mem_addr = 0;
static int watch_mem_n = 0;
static void watch_dump_mem() {
    if (!watch_mem_addr) return;
    printf("  memoria 0x%08X:", watch_mem_addr);
    for (int i = 0; i < watch_mem_n; i++) printf(" %08X", psp_l32(watch_mem_addr + (uint32_t)i * 4));
    printf("\n");
}
// HLE call in progress per thread (set by psp_hle): names the import when a watched write comes from runtime code
struct HleCur { int id = -1; uint32_t a[4] = {0, 0, 0, 0}; uint32_t ra = 0; };
static HleCur hle_cur[8];
static int watch_fix = -1;
static uint32_t watch_value = 0;
static std::vector<std::pair<uintptr_t, uint32_t>> watch_map;

static void watch_map_prepare() {
    for (int b = 0; b < BANKS_N; b++)
        for (int i = 0; i < BANKS[b].n; i++)
            watch_map.push_back({(uintptr_t)BANKS[b].tab[i].fn, BANKS[b].tab[i].dir});
    std::sort(watch_map.begin(), watch_map.end());
}

static uint32_t watch_func(uintptr_t rip) {
    auto it = std::upper_bound(watch_map.begin(), watch_map.end(), std::make_pair(rip, 0xFFFFFFFFu));
    if (it == watch_map.begin()) return 0;
    --it;
    return it->second;
}

static void watch_protect(DWORD mode) {
    DWORD old;
    for (int i = 0; i < watch_n; i++) if (watch_pages[i]) VirtualProtect(watch_pages[i], 4096, mode, &old);
}

static LONG WINAPI watch_handler(EXCEPTION_POINTERS* ep) {
    EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->ExceptionInformation[0] == 1) {
        uintptr_t f = (uintptr_t)er->ExceptionInformation[1];
        int page = -1;
        for (int i = 0; i < watch_n; i++) if (watch_pages[i] && f >= (uintptr_t)watch_pages[i] && f < (uintptr_t)watch_pages[i] + 4096) page = i;
        if (page >= 0) {
            watch_hit = -1; watch_rip = 0;
            for (int i = 0; i < watch_n; i++) if (watch_ptrs[i] && f + 4 > (uintptr_t)watch_ptrs[i] && f < (uintptr_t)watch_ptrs[i] + 4) { watch_hit = i; watch_rip = (uintptr_t)ep->ContextRecord->Rip; }
            watch_protect(PAGE_READWRITE);
            ep->ContextRecord->EFlags |= 0x100;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    } else if (er->ExceptionCode == EXCEPTION_SINGLE_STEP && watch_n) {
        watch_protect(PAGE_READONLY);
        ep->ContextRecord->EFlags &= ~(DWORD)0x100;
        if (watch_rip && watch_hit >= 0) {
            static uintptr_t seen_list[WATCH_MAX][64]; static int nv[WATCH_MAX];
            int h = watch_hit;
            uint32_t v; memcpy(&v, watch_ptrs[h], 4);
            if (watch_fix > 0 && v != watch_value) {
                memcpy(watch_ptrs[h], &watch_value, 4);
                static int warnings = 0;
                if (warnings++ < 8) { printf("fix: el juego escribio %u en 0x%08X, lo dejo en %u\n", v, watch_addrs[h], watch_value); fflush(stdout); }
            }
            // TTT_WATCH_ALL=1: reports every value change, not only each function's first write
            static uint32_t last_val[WATCH_MAX] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
            bool rep = false;
            for (int i = 0; i < nv[h]; i++) if (seen_list[h][i] == watch_rip) rep = true;
            if (watch_all) rep = v == last_val[h];
            static int zero_on = -1;
            if (zero_on < 0) { const char* z = getenv("TTT_WATCH_ZERO"); zero_on = z ? atoi(z) : 0; }
            if (zero_on && v == 0 && last_val[h] != 0) {
                static int zl = 0;
                if (zl++ < 40) {
                    int th = psp_thread_current();
                    Ctx* cz = psp_thread_ctx(th);
                    uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
                    const HleCur& hc = hle_cur[th & 7];
                    printf("write cero en 0x%08X desde la funcion del juego 0x%08X (exe+0x%llX), hilo %d, cuadro %ld, fn 0x%08X ra 0x%08X sp 0x%08X",
                           watch_addrs[h], watch_func(watch_rip), (unsigned long long)(watch_rip - base), th, cuadros, cz ? cz->fn : 0, cz ? cz->r[31] : 0, cz ? cz->r[29] : 0);
                    if (hc.id >= 0) printf(", dentro de %s(%08X, %08X, %08X, %08X) desde 0x%08X", psp_import_name(hc.id), hc.a[0], hc.a[1], hc.a[2], hc.a[3], hc.ra - 8);
                    printf("\n");
                    watch_dump_mem();
                    fflush(stdout);
                }
            }
            if (zero_on) last_val[h] = v;
            static int total = 0;
            if (!rep && nv[h] < 64 && total < 600) {
                if (!watch_all) seen_list[h][nv[h]++] = watch_rip;
                total++;
                last_val[h] = v;
                float fv; memcpy(&fv, &v, 4);
                Ctx* cx = psp_thread_ctx(psp_thread_current());
                printf("write en 0x%08X = %u (0x%08X, %.3f) desde la funcion del juego 0x%08X, cuadro %ld, fn 0x%08X ra 0x%08X\n",
                       watch_addrs[h], v, v, fv, watch_func(watch_rip), cuadros, cx ? cx->fn : 0, cx ? cx->r[31] : 0);
                watch_dump_mem();
                fflush(stdout);
            }
            watch_rip = 0; watch_hit = -1;
        }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static bool watch_installed = false;

static void watch_set(int i, uint32_t dir) {
    DWORD old;
    if (i >= watch_n) watch_n = i + 1;
    watch_protect(PAGE_READWRITE);
    watch_addrs[i] = dir;
    watch_ptrs[i] = psp_p(dir);
    watch_pages[i] = (uint8_t*)((uintptr_t)watch_ptrs[i] & ~(uintptr_t)4095);
    watch_addr = dir;
    if (!watch_installed) { watch_map_prepare(); AddVectoredExceptionHandler(1, watch_handler); watch_installed = true; }
    watch_protect(PAGE_READONLY);
    (void)old;
    printf("vigilando escrituras en 0x%08X (%zu funciones en la tabla)\n", dir, watch_map.size());
    fflush(stdout);
}
static void watch_arm(uint32_t dir) { watch_set(0, dir); }

// TTT_WATCH_WRITE=L0+0x950: fighter in slot 0 of 0x08B61178; C0+0x40: combatant 0 (table at 0x08A7DBC8, 0x1A90 each)
// accepts up to 4 separated by commas: C0+0xEFA,C2+0xEFA
// N0x088F9C40:0x38+0x10: live task node of that class update, pointer at +0x38, offset 0x10 (:-1 = the node itself)
struct WatchRel { int idx = -1; char type = 'L'; uint32_t off = 0, base = 0; bool abs = false; uint32_t dir = 0, upd = 0, ptr = 0; };
static WatchRel watch_rels[WATCH_MAX];
static int watch_rels_n = 0;
bool psp_watch_nodes = false;
void psp_watch_node(uint32_t upd, uint32_t node) {
    for (int i = 0; i < watch_rels_n; i++) {
        WatchRel& r = watch_rels[i];
        if (r.type != 'N' || r.upd != upd) continue;
        uint32_t b = r.ptr == 0xFFFFFFFFu ? node : psp_l32(node + r.ptr);
        if (b < 0x08800000u || b >= 0x10000000u || b == r.base) continue;
        r.base = b;
        printf("watch: nodo 0x%08X de la clase 0x%08X, base 0x%08X\n", node, upd, b);
        watch_set(i, b + r.off);
    }
}
static void watch_follow() {
    for (int i = 0; i < watch_rels_n; i++) {
        WatchRel& r = watch_rels[i];
        if (r.abs || r.type == 'N') continue;
        uint32_t b;
        if (r.type == 'C') {
            uint32_t t = psp_l32(0x08A7DBC8u);
            if (t < 0x08800000u || t >= 0x10000000u) continue;
            b = psp_l32(t + 4);
            static int warned = 0;
            if (!warned && b) { warned = 1; printf("watch: tabla de combatientes 0x%08X, %u combatientes, base 0x%08X\n", t, psp_l32(t), b); }
            if (!b) continue;
            b += 0x1A90u * (uint32_t)r.idx;
        } else b = psp_l32(0x08B61178u + 4u * (uint32_t)r.idx);
        if (b < 0x08800000u || b >= 0x10000000u || b == r.base) continue;
        r.base = b;
        watch_set(i, b + r.off);
    }
}

static void watch_write() {
    static int hecho = 0;
    if (hecho) { watch_follow(); return; }
    hecho = 1;
    const char* f = getenv("TTT_FIX");
    watch_fix = f ? 1 : 0;
    if (f) watch_value = (uint32_t)strtoul(f, nullptr, 0);
    const char* t = getenv("TTT_WATCH_ALL");
    watch_all = t ? atoi(t) : 0;
    const char* m = getenv("TTT_MEM");
    if (m) {
        watch_mem_addr = (uint32_t)strtoul(m, nullptr, 0);
        const char* c = strchr(m, ':');
        watch_mem_n = c ? atoi(c + 1) : 8;
        if (watch_mem_n < 1) watch_mem_n = 1;
        if (watch_mem_n > 32) watch_mem_n = 32;
    }
    const char* e = getenv("TTT_WATCH_WRITE");
    for (const char* q = e; q && *q && watch_rels_n < WATCH_MAX;) {
        WatchRel& r = watch_rels[watch_rels_n];
        if (q[0] == 'N' || q[0] == 'n') {
            char* e2;
            r.type = 'N';
            r.upd = (uint32_t)strtoul(q + 1, &e2, 0);
            r.ptr = *e2 == ':' ? (uint32_t)strtol(e2 + 1, &e2, 0) : 0xFFFFFFFFu;
            r.off = *e2 == '+' ? (uint32_t)strtoul(e2 + 1, nullptr, 0) : 0;
            psp_watch_nodes = true;
            printf("watch: esperando un nodo de la clase 0x%08X, desplazamiento 0x%X\n", r.upd, r.off);
        } else if (strchr("LlCc", q[0])) {
            r.type = (char)toupper(q[0]);
            r.idx = atoi(q + 1);
            const char* m2 = strchr(q, '+');
            const char* cm = strchr(q, ',');
            r.off = (m2 && (!cm || m2 < cm)) ? (uint32_t)strtoul(m2 + 1, nullptr, 0) : 0;
            printf("watch: esperando al %s %d, desplazamiento 0x%X\n", r.type == 'C' ? "combatiente" : "luchador", r.idx, r.off);
        } else { r.abs = true; r.dir = (uint32_t)strtoul(q, nullptr, 0); watch_set(watch_rels_n, r.dir); }
        watch_rels_n++;
        q = strchr(q, ','); if (q) q++;
    }
}
// any unhandled exception (every thread, also driver threads): code, module + offset, return addresses of the exe on the stack
static LPTOP_LEVEL_EXCEPTION_FILTER crash_prev = nullptr;
static void crash_where(uintptr_t a, char* o, size_t n) {
    HMODULE m = nullptr; char nm[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &m) && m) {
        GetModuleFileNameA(m, nm, MAX_PATH);
        const char* b = strrchr(nm, '\\');
        snprintf(o, n, "%s+0x%llX", b ? b + 1 : nm, (unsigned long long)(a - (uintptr_t)m));
    } else snprintf(o, n, "0x%llX", (unsigned long long)a);
}
static LONG WINAPI crash_filter(EXCEPTION_POINTERS* ep) {
    static std::atomic<bool> once{false};
    if (!once.exchange(true)) {
        char w[300];
        uintptr_t pc = (uintptr_t)ep->ExceptionRecord->ExceptionAddress;
        crash_where(pc, w, sizeof(w));
        printf("\nFALLO GRAVE: excepcion 0x%08lX en %s, hilo del sistema %lu, hilo del juego %d, ultima importacion %s\n", (unsigned long)ep->ExceptionRecord->ExceptionCode, w,
               GetCurrentThreadId(), psp_thread_current(), last_hle >= 0 ? psp_import_name(last_hle) : "ninguna");
        if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) printf("FALLO GRAVE: acceso %s a 0x%llX\n", ep->ExceptionRecord->ExceptionInformation[0] ? "de escritura" : "de lectura", (unsigned long long)ep->ExceptionRecord->ExceptionInformation[1]);
        uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr), top = base + ((const IMAGE_NT_HEADERS*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew))->OptionalHeader.SizeOfImage;
        const uintptr_t* sp = (const uintptr_t*)(ep->ContextRecord->Rsp & ~(uintptr_t)7);
        int n = 0;
        printf("FALLO GRAVE: pila:");
        MEMORY_BASIC_INFORMATION mi;
        size_t lim = 0;
        if (VirtualQuery(sp, &mi, sizeof(mi)) && mi.State == MEM_COMMIT && !(mi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            lim = ((uintptr_t)mi.BaseAddress + mi.RegionSize - (uintptr_t)sp) / sizeof(uintptr_t);
        for (size_t i = 0; i < lim && i < 2048 && n < 16; i++) {
            uintptr_t v = sp[i];
            if (v > base && v < top) { printf(" +0x%llX", (unsigned long long)(v - base)); n++; }
        }
        printf("\n");
        fflush(stdout);
    }
    return crash_prev ? crash_prev(ep) : EXCEPTION_CONTINUE_SEARCH;
}
#else
static void watch_write() {}
static void watch_arm(uint32_t) {}
bool psp_watch_nodes = false;
void psp_watch_node(uint32_t, uint32_t) {}
#endif


