static clock_t t_begin = 0;
static double limit_s = 20.0;

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <mmsystem.h>
#include <map>
#include <array>
#include <thread>
unsigned long psp_game_os_tid();
// hang report: where every system thread is (exe offsets, resolved with nm/objdump of the same build)
static void hang_stacks() {
    uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr), top = base + ((const IMAGE_NT_HEADERS*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew))->OptionalHeader.SizeOfImage;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    struct Rec { DWORD tid; uintptr_t rip; int n; uintptr_t hit[14]; };
    static Rec recs[64];
    int nr = 0;
    THREADENTRY32 te; te.dwSize = sizeof(te);
    DWORD me = GetCurrentThreadId(), pid = GetCurrentProcessId(), game = (DWORD)psp_game_os_tid();
    for (BOOL ok = Thread32First(snap, &te); ok && nr < 64; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == me) continue;
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
        if (!h) continue;
        if (SuspendThread(h) == (DWORD)-1) { CloseHandle(h); continue; }
        CONTEXT cx; memset(&cx, 0, sizeof(cx)); cx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (GetThreadContext(h, &cx)) {   // sampled while suspended, printed after resuming (a suspended thread may hold the stdout lock)
            Rec& r = recs[nr++]; r.tid = te.th32ThreadID; r.rip = cx.Rip; r.n = 0;
            const uintptr_t* sp = (const uintptr_t*)(cx.Rsp & ~(uintptr_t)7);
            MEMORY_BASIC_INFORMATION mi;
            size_t lim = 0;
            if (VirtualQuery(sp, &mi, sizeof(mi)) && mi.State == MEM_COMMIT && !(mi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                lim = ((uintptr_t)mi.BaseAddress + mi.RegionSize - (uintptr_t)sp) / sizeof(uintptr_t);
            for (size_t i = 0; i < lim && i < 4096 && r.n < 14; i++) {
                uintptr_t v = sp[i];
                if (v > base && v < top) r.hit[r.n++] = v - base;
            }
        }
        ResumeThread(h);
        CloseHandle(h);
    }
    CloseHandle(snap);
    for (int k = 0; k < nr; k++) {
        const Rec& r = recs[k];
        bool in = r.rip >= base && r.rip < top;
        printf("colgado: hilo del sistema %lu%s rip %s+0x%llX pila:", (unsigned long)r.tid, r.tid == game ? " (juego)" : "", in ? "exe" : "fuera", (unsigned long long)(in ? r.rip - base : r.rip));
        for (int i = 0; i < r.n; i++) printf(" +0x%llX", (unsigned long long)r.hit[i]);
        printf("\n");
    }
    fflush(stdout);
}
// TTT_GEN_PROF=1: samples the game thread ~1000 times per second while it runs generated code; muestras_gen.txt = exe offsets
static void gen_prof_run() {
    uintptr_t base = (uintptr_t)GetModuleHandleA(nullptr);
    const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew);
    uintptr_t t0 = base + nt->OptionalHeader.BaseOfCode, t1 = t0 + nt->OptionalHeader.SizeOfCode;
    while (!psp_game_os_tid()) Sleep(50);
    HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, (DWORD)psp_game_os_tid());
    if (!h) { printf("gen: no pude abrir el hilo del juego\n"); return; }
    timeBeginPeriod(1);
    std::map<std::array<uint32_t, 4>, uint32_t> m;
    uint64_t tot = 0, gen = 0;
    DWORD last = GetTickCount();
    for (;;) {
        Sleep(1);
        tot++;
        if (perf_zone != PZ_GEN) continue;
        if (SuspendThread(h) == (DWORD)-1) continue;
        CONTEXT cx; memset(&cx, 0, sizeof(cx)); cx.ContextFlags = CONTEXT_CONTROL;
        std::array<uint32_t, 4> k{};
        bool ok = perf_zone == PZ_GEN && GetThreadContext(h, &cx) && cx.Rip >= t0 && cx.Rip < t1;
        if (ok) {
            k[0] = (uint32_t)(cx.Rip - base);
            const uintptr_t* sp = (const uintptr_t*)(cx.Rsp & ~(uintptr_t)7);
            MEMORY_BASIC_INFORMATION mi; size_t lim = 0;
            if (VirtualQuery(sp, &mi, sizeof(mi)) && mi.State == MEM_COMMIT && !(mi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
                lim = ((uintptr_t)mi.BaseAddress + mi.RegionSize - (uintptr_t)sp) / sizeof(uintptr_t);
            for (size_t i = 0, n = 1; i < lim && i < 512 && n < 4; i++) { uintptr_t v = sp[i]; if (v >= t0 && v < t1) k[n++] = (uint32_t)(v - base); }
        }
        ResumeThread(h);
        if (ok) { gen++; m[k]++; }
        if (GetTickCount() - last >= 15000) {
            last = GetTickCount();
            if (FILE* f = fopen("muestras_gen.txt", "w")) {
                fprintf(f, "total %llu gen %llu\n", (unsigned long long)tot, (unsigned long long)gen);
                for (auto& e : m) fprintf(f, "%X %X %X %X %u\n", e.first[0], e.first[1], e.first[2], e.first[3], e.second);
                fclose(f);
            }
            printf("gen: %llu muestras en codigo generado de %llu\n", (unsigned long long)gen, (unsigned long long)tot); fflush(stdout);
        }
    }
}
static struct GenProfInit { GenProfInit() {
    const char* e = getenv("TTT_GEN_PROF");
    if (e && atoi(e) > 0) std::thread(gen_prof_run).detach();
} } gen_prof_init;
#else
static void hang_stacks() {}
#endif

static bool with_window = false;
static long cuadros = 0;
bool psp_window_rate_requested();
bool psp_window_pos_requested();
bool psp_stall_on = false;
extern int psp_windows_on;
static struct CensusInit { CensusInit() {
    const char* s = getenv("TTT_STALLS");
    psp_stall_on = s && atoi(s) > 0;
    const char* e = getenv("TTT_FN_CENSUS");
    const char* w = getenv("TTT_WINDOWS");
    psp_windows_on = w && atoi(w) > 0;
    psp_fn_census = (e && atoi(e)) || psp_coverage_on() || psp_stall_on || psp_windows_on;
} } census_init;
void psp_window_repeat(int n);
int psp_window_language();
void psp_window_sample();
void psp_window_lat(int type, int thr);
void psp_census_report();
void psp_jalr_census_report(long cuadros);
void psp_fn_census_report(long cuadros);
static const char* end_reason = "desconocido";
static std::atomic<bool> report_done{false};
static void report();

// virtual disc reads in progress: a slow read (cold cache, antivirus) is not a hang
static std::atomic<int> disc_reading(0);
#define DISC_READING_EXTERN

static void watcher_external(int seconds_n) {
    // system thread: if the game spins without yielding, prints the report and exits.
    // with a window: fires after 8 s without presenting a frame, or on F12
    if (!with_window) {
        std::this_thread::sleep_for(std::chrono::seconds(seconds_n));
    } else {
        long last = cuadros;
        int idle = 0, still = 0;   // still: seconds without frames whatever the reason (a read or a pause that never ends)
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (cuadros == last) still++; else still = 0;
            if (watchdog_pause.load() && !disc_reading.load()) still = 0;   // a port window holding the game is not a hang
            if (psp_window_report_requested()) { psp_census_report(); end_reason = still >= 2 ? "colgado: informe con F12 sin cuadros" : "informe pedido con F12"; break; }
            if (still >= 30 && !(watchdog_pause.load() && !disc_reading.load())) {
                end_reason = disc_reading.load() ? "colgado: 30 s sin cuadros con una lectura de disco en curso" : "colgado: 30 s sin cuadros";
                break;
            }
            if (watchdog_pause.load() || disc_reading.load()) { idle = watchdog_pause.load() ? -22 : 0; last = cuadros; continue; }   // the first frame after a window closes may rebuild textures: 30 s for it
            if (cuadros == last) { if (++idle >= 8) { end_reason = "colgado: 8 s sin cuadros nuevos"; break; } }
            else { idle = 0; last = cuadros; }
        }
    }
    if (!report_done.exchange(true)) {
        printf("\ncortado por el vigilante externo\n");
        if (strncmp(end_reason, "colgado", 7) == 0) hang_stacks();
        report();
        fflush(stdout);
        _Exit(0);
    }
}

static void fatal_error(int next) {
    end_reason = next == 11 ? "FALLO GRAVE: violacion de segmento" : "FALLO GRAVE";
    printf("\nFALLO GRAVE (senal %d) en el hilo %d, ultima importacion %s\n", next, psp_thread_current(),
           last_hle >= 0 ? psp_import_name(last_hle) : "ninguna");
#ifdef __linux__
    void* stack[40];
    int n = backtrace(stack, 40);
    fflush(stdout);
    backtrace_symbols_fd(stack, n, 1);
#endif
    if (!report_done.exchange(true)) report();
    fflush(stdout);
    _Exit(3);
}

static void watch() {
    // the scheduler and the external watcher handle timeouts; here we only yield
    if ((double)(clock() - t_begin) / CLOCKS_PER_SEC > limit_s) psp_yield();
}

