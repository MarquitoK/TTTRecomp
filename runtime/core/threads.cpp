#include "psp.h"
#include "perf.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <deque>
#include <csetjmp>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <chrono>
#include <atomic>
#ifdef _WIN32
#include <windows.h>
#endif

// Cooperative scheduler: a single guest thread runs at a time.
// Every guest thread is a std::thread waiting for its turn; the scheduler
// hands out the turn. Generated code thus uses the native stack without platform tricks.

struct GuestThread {
    int id = 0;
    uint32_t entry_pc = 0, stack = 0, arg_len = 0, arg_ptr = 0;
    int prio = 32;
    Ctx c{};
    std::thread th;
    int state = 0;                 // 0 new, 1 ready, 2 blocked, 3 finished
    uint64_t despertar = 0;         // 0 = wait forever
    jmp_buf out_buf;
    bool has_out = false;
    void* fiber = nullptr;
};

struct GuestQueue { std::deque<uint32_t> msgs; bool alive = false; };
struct Sema { int count_n = 0, max = 0; bool alive_b = false; };
struct GuestFlag { uint32_t bits = 0; bool alive = false; };

static std::mutex M;
static std::condition_variable CV;
static std::vector<GuestThread*> threads;
static std::vector<GuestQueue> queues;
static std::vector<Sema> semas;
static std::vector<GuestFlag> flags;
static int actual = -1;
static uint64_t t_virtual = 0;
static bool stop_fn = false;
static uint32_t gp_global = 0;
static long switches = 0;
static long switch_cap_v = 40000000;
static clock_t t0_real = 0;
static double cap_s = 0;
static char reason[128] = "";
static void (*sched_hook)() = nullptr;
void psp_scheduler_hook(void (*fn)()) { sched_hook = fn; }
static volatile bool stop_requested = false;
void psp_request_stop() { stop_requested = true; }

uint64_t psp_time_us() { return t_virtual; }
void psp_gp(uint32_t gp) { gp_global = gp; }
long psp_switches() { return switches; }
void psp_switch_cap(long n) { switch_cap_v = n; }
void psp_time_cap(double secs) { t0_real = clock(); cap_s = secs; }
Ctx* psp_thread_ctx(int id) { return (id >= 0 && id < (int)threads.size()) ? &threads[id]->c : nullptr; }
int psp_thread_prio(int id) { return (id >= 0 && id < (int)threads.size()) ? threads[id]->prio : 0; }
uint32_t psp_thread_entry(int id) { return (id >= 0 && id < (int)threads.size()) ? threads[id]->entry_pc : 0; }

// TTT_FIBERS (default 1): every guest thread is a fiber of one OS thread; a turn change is a user mode switch (~100 ns) instead
// of two OS thread wakeups (~70 us measured on a mid PC); the window thread runs the hook only when it has work
#ifdef _WIN32
static bool fb_on = false;
static void* fb_sched = nullptr;
static std::mutex FM;
static std::condition_variable FCV;
static int fb_req = 0;
static const char* fb_end = nullptr;
static double fb_last_hook = 0;
static long fb_hops = 0;
bool psp_window_hook_due();
static double fb_now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static void fb_back() { SwitchToFiber(fb_sched); }
static void body(GuestThread* h);
static void WINAPI fb_proc(void* p) {
    GuestThread* h = (GuestThread*)p;
    body(h);
    for (;;) SwitchToFiber(fb_sched);
}
static void fb_make(GuestThread* h) {
    if (!h->fiber) h->fiber = CreateFiberEx(64 * 1024, 16u << 20, FIBER_FLAG_FLOAT_SWITCH, fb_proc, h);
    if (!h->fiber) printf("hilos: no pude crear la fibra del hilo %d (error %lu)\n", h->id, (unsigned long)GetLastError());
}
static bool fb_wanted() {
    static int v = -1;
    if (v < 0) { const char* e = getenv("TTT_FIBERS"); v = e ? atoi(e) : 1; }
    return v != 0;
}
#else
static const bool fb_on = false;
static void fb_back() {}
static bool fb_wanted() { return false; }
#endif

static GuestThread* mine() { return (actual >= 0 && actual < (int)threads.size()) ? threads[actual] : nullptr; }

static void give_turn(int dst_v) {
    { std::lock_guard<std::mutex> g(M); actual = dst_v; }
    CV.notify_all();
}

static void wait_turn(int id) {
    std::unique_lock<std::mutex> lk(M);
    CV.wait(lk, [id] { return actual == id || stop_fn; });
}

void psp_yield() {
    GuestThread* h = mine();
    if (!h) return;
    int id = h->id;
    switches++;
    t_virtual += 100;
    PERF_N(PC_SWITCH, 1);
    PERF(PZ_SCHED);
    if (fb_on) { fb_back(); return; }
    give_turn(-1);
    wait_turn(id);
}

void psp_block(uint64_t until_us) {
    GuestThread* h = mine();
    if (!h) return;
    h->state = 2;
    h->despertar = until_us;
    psp_yield();
}

void psp_wake(int id) {
    if (id >= 0 && id < (int)threads.size() && threads[id]->state == 2) {
        threads[id]->state = 1;
        threads[id]->despertar = 0;
    }
}

void psp_thread_exit() {
    GuestThread* h = mine();
    if (!h) return;
    h->state = 3;
    if (h->has_out) longjmp(h->out_buf, 2);
}

static void body(GuestThread* h) {
    if (!fb_on) wait_turn(h->id);
    if (!stop_fn) {
        if (setjmp(h->out_buf) == 0) {
            h->has_out = true;
            h->c.r[28] = gp_global;
            h->c.r[29] = h->stack;
            h->c.r[4] = h->arg_len;
            h->c.r[5] = h->arg_ptr;
            h->c.r[31] = 0;
            psp_vfpu_init(h->c);
            Fn f = psp_resolve(h->entry_pc);
            if (f) { PERF(PZ_GEN); f(h->c); }
            else fprintf(stderr, "hilo %d: no hay funcion en 0x%08X\n", h->id, h->entry_pc);
        }
    }
    h->state = 3;
    if (!fb_on) give_turn(-1);
}

int psp_thread_create(uint32_t entry_pc, uint32_t stack_top, int prio) {
    GuestThread* h = new GuestThread();
    h->id = (int)threads.size();
    h->entry_pc = entry_pc;
    h->stack = stack_top;
    h->prio = prio;
    threads.push_back(h);
    return h->id;
}

void psp_thread_start(int id, uint32_t arg_len, uint32_t arg_ptr) {
    if (id < 0 || id >= (int)threads.size()) return;
    GuestThread* h = threads[id];
    if (h->state != 0) return;
    h->arg_len = arg_len;
    h->arg_ptr = arg_ptr;
    if (arg_len && arg_ptr) {
        // the kernel copies the argument block to the new thread's stack at this point
        uint32_t sp = (h->stack - arg_len) & ~0xFu;
        memcpy(psp_p(sp), psp_p(arg_ptr), arg_len);
        h->arg_ptr = sp;
        h->stack = sp - 0x40;
    }
    h->state = 1;
#ifdef _WIN32
    if (fb_wanted()) fb_make(h);
    else
#endif
    h->th = std::thread(body, h);
    GuestThread* yo = mine();
    if (yo && h->prio < yo->prio) psp_yield();   // the PSP switches to the highest priority thread when starting it
}

int psp_thread_current() { return actual; }
int psp_threads_n() { return (int)threads.size(); }
int psp_thread_state(int id) { return (id >= 0 && id < (int)threads.size()) ? threads[id]->state : 3; }

static int elegir() {
    int best = -1;
    for (size_t i = 0; i < threads.size(); i++) {
        GuestThread* h = threads[i];
        if (h->state != 1) continue;
        if (best < 0 || h->prio < threads[best]->prio) best = (int)i;
    }
    return best;
}

static uint64_t next_wake() {
    uint64_t t = 0;
    for (GuestThread* h : threads)
        if (h->state == 2 && h->despertar && (t == 0 || h->despertar < t)) t = h->despertar;
    return t;
}

#ifdef _WIN32
static const char* fb_pick() {
    if (stop_requested) return "ventana cerrada";
    if (switches > switch_cap_v) return "tope de cambios de contexto";
    if (cap_s > 0 && (double)(clock() - t0_real) / CLOCKS_PER_SEC > cap_s) return "corte por tiempo";
    return nullptr;
}
static void fb_hook() {
    std::unique_lock<std::mutex> lk(FM);
    fb_req = 1; fb_hops++;
    FCV.notify_all();
    FCV.wait(lk, [] { return fb_req == 0; });
}
static unsigned long fb_tid = 0;
unsigned long psp_game_os_tid() { return fb_tid; }
static void fb_loop() {
    fb_tid = GetCurrentThreadId();
    fb_sched = ConvertThreadToFiber(nullptr);
    if (!fb_sched) { std::lock_guard<std::mutex> g(FM); fb_end = "no se pudo crear la fibra del planificador"; FCV.notify_all(); return; }
    for (GuestThread* h : threads) if (h->state == 1 && !h->fiber) fb_make(h);
    const char* end = nullptr;
    while (!end) {
        double now = fb_now();
        if (psp_window_hook_due() || now - fb_last_hook >= 0.004) { fb_last_hook = now; fb_hook(); }
        if ((end = fb_pick())) break;
        int sig = elegir();
        if (sig < 0) {
            uint64_t t = next_wake();
            if (t == 0) {
                bool alive_b = false;
                for (GuestThread* h : threads) if (h->state == 2) alive_b = true;
                end = alive_b ? "todos los hilos bloqueados sin plazo" : "no quedan hilos";
                break;
            }
            t_virtual = t;
            for (GuestThread* h : threads)
                if (h->state == 2 && h->despertar && h->despertar <= t_virtual) { h->state = 1; h->despertar = 0; }
            continue;
        }
        GuestThread* g = threads[sig];
        if (!g->fiber) fb_make(g);
        if (!g->fiber) { g->state = 3; continue; }
        actual = sig;
        SwitchToFiber(g->fiber);
        actual = -1;
        if (g->state == 3 && g->fiber) { DeleteFiber(g->fiber); g->fiber = nullptr; }
    }
    std::lock_guard<std::mutex> lk(FM);
    fb_end = end;
    FCV.notify_all();
}
#endif

const char* psp_schedule() {
#ifdef _WIN32
    if (fb_wanted()) {
        fb_on = true;
        printf("hilos: fibras (un hilo del sistema para el juego)\n"); fflush(stdout);
        std::thread(fb_loop).detach();
        for (;;) {
            std::unique_lock<std::mutex> lk(FM);
            FCV.wait(lk, [] { return fb_req == 1 || fb_end; });
            if (fb_end) { printf("hilos: %ld visitas a la ventana, %ld cambios\n", fb_hops, switches); return fb_end; }
            lk.unlock();
            if (sched_hook) sched_hook();
            lk.lock();
            fb_req = 0;
            FCV.notify_all();
        }
    }
#endif
    while (true) {
        if (sched_hook) sched_hook();
        if (stop_requested) return "ventana cerrada";
        if (switches > switch_cap_v) return "tope de cambios de contexto";
        if (cap_s > 0 && (double)(clock() - t0_real) / CLOCKS_PER_SEC > cap_s) return "corte por tiempo";
        int sig = elegir();
        if (sig < 0) {
            uint64_t t = next_wake();
            if (t == 0) {
                bool alive_b = false;
                for (GuestThread* h : threads) if (h->state == 2) alive_b = true;
                return alive_b ? "todos los hilos bloqueados sin plazo" : "no quedan hilos";
            }
            t_virtual = t;
            for (GuestThread* h : threads)
                if (h->state == 2 && h->despertar && h->despertar <= t_virtual) { h->state = 1; h->despertar = 0; }
            continue;
        }
        give_turn(sig);
        std::unique_lock<std::mutex> lk(M);
        CV.wait(lk, [] { return actual == -1; });
    }
}

void psp_stop_all() {
    stop_fn = true;
    give_turn(-1);
    for (GuestThread* h : threads) if (h->th.joinable()) h->th.detach();
}

// synchronization objects

int psp_mbx_create() { queues.push_back(GuestQueue()); queues.back().alive = true; return (int)queues.size() - 1; }

void psp_mbx_send(int id, uint32_t msg) {
    if (id < 0 || id >= (int)queues.size()) return;
    queues[id].msgs.push_back(msg);
    for (GuestThread* h : threads) if (h->state == 2 && h->despertar == 0) { h->state = 1; }
}

// sceKernelPollMbx: no wait, -1 when the mailbox is empty
int psp_mbx_poll(int id, uint32_t* msg) {
    if (id < 0 || id >= (int)queues.size() || queues[id].msgs.empty()) return -1;
    *msg = queues[id].msgs.front();
    queues[id].msgs.pop_front();
    return 0;
}

int psp_mbx_receive(int id, uint32_t* msg, uint64_t wait_us) {
    if (id < 0 || id >= (int)queues.size()) return -1;
    uint64_t deadline = wait_us ? t_virtual + wait_us : 0;
    while (queues[id].msgs.empty()) {
        psp_block(deadline);
        if (deadline && t_virtual >= deadline && queues[id].msgs.empty()) return -1;
    }
    *msg = queues[id].msgs.front();
    queues[id].msgs.pop_front();
    return 0;
}

int psp_sema_create(int initial, int maximum) {
    semas.push_back(Sema());
    semas.back().count_n = initial;
    semas.back().max = maximum;
    semas.back().alive_b = true;
    return (int)semas.size() - 1;
}

int psp_sema_wait(int id, int n, uint64_t wait_us) {
    if (id < 0 || id >= (int)semas.size()) return -1;
    uint64_t deadline = wait_us ? t_virtual + wait_us : 0;
    while (semas[id].count_n < n) {
        psp_block(deadline);
        if (deadline && t_virtual >= deadline && semas[id].count_n < n) return -1;
    }
    semas[id].count_n -= n;
    return 0;
}

void psp_sema_signal(int id, int n) {
    if (id < 0 || id >= (int)semas.size()) return;
    semas[id].count_n += n;
    for (GuestThread* h : threads) if (h->state == 2 && h->despertar == 0) h->state = 1;
}

int psp_flag_create(uint32_t bits) {
    flags.push_back(GuestFlag());
    flags.back().bits = bits;
    flags.back().alive = true;
    return (int)flags.size() - 1;
}

void psp_flag_set(int id, uint32_t bits) {
    if (id < 0 || id >= (int)flags.size()) return;
    flags[id].bits |= bits;
    for (GuestThread* h : threads) if (h->state == 2 && h->despertar == 0) h->state = 1;
}

void psp_flag_clear(int id, uint32_t bits) {
    if (id >= 0 && id < (int)flags.size()) flags[id].bits &= bits;
}

int psp_flag_wait(int id, uint32_t pattern, int mode, uint32_t* out_buf, uint64_t wait_us) {
    if (id < 0 || id >= (int)flags.size()) return -1;
    uint64_t deadline = wait_us ? t_virtual + wait_us : 0;
    while (true) {
        uint32_t b = flags[id].bits;
        bool ok = (mode & 1) ? ((b & pattern) == pattern) : ((b & pattern) != 0);
        if (ok) {
            if (out_buf) *out_buf = b;
            if (mode & 0x10) flags[id].bits &= ~pattern;
            return 0;
        }
        psp_block(deadline);
        if (deadline && t_virtual >= deadline) return -1;
    }
}
