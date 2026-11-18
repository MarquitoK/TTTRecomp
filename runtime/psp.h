#pragma once
#include <cstdint>
#include <atomic>
#include <cstring>
#include <cmath>

union FReg {
    float f;
    uint32_t u;
    int32_t i;
};

struct Ctx {
    uint32_t r[32];
    uint32_t hi, lo;
    FReg f[32];
    bool fcc;
    uint32_t fcr31;
    FReg v[128];
    uint32_t vctrl[16];
    uint32_t fn;
    uint64_t llamadas;
};

extern uint8_t* psp_mem;
extern uint32_t psp_mem_mask;

static const uint32_t PSP_BANK_VA = 0x08A814A0u;
static const uint32_t PSP_BANK_END = 0x08AEFB70u;

typedef void (*Fn)(Ctx&);

struct Entry { uint32_t dir; Fn fn; };
struct Bank { const char* nombre; int indice; const Entry* tab; int n; };
struct Import { const char* lib; uint32_t nid; const char* nombre; uint32_t stub; };

void psp_mem_init(uint32_t tam);
void psp_mem_free();
void psp_bank(int packfile_index);
int psp_bank_current();
Fn psp_resolve(uint32_t dir);

// per-function call census (TTT_FN_CENSUS=1) and coverage corpus (TTT_COVERAGE): off by default
extern bool psp_fn_census;
extern bool psp_cov_on;
void psp_fn_count_slow(uint32_t dir, uint32_t ra);
static inline void psp_fn_count(uint32_t dir, uint32_t ra) { if (psp_fn_census) psp_fn_count_slow(dir, ra); }
// main-image store sites: step census (TTT_STEPCENSUS) and 60 fps store rules; off = one flag test per store
extern bool psp_st_on;
uint32_t psp_st_slow(int size, uint32_t pc, uint32_t a, uint32_t v);
#define PSP_ST(sz, pc, a, v) (psp_st_on ? psp_st_slow(sz, pc, a, v) : (uint32_t)(v))
void psp_step_census_dump();
enum { COV_FN, COV_HLE, COV_JALR, COV_CALL_UNRES, COV_JUMP_UNRES };
bool psp_coverage_on();
void psp_coverage_note(int kind, uint32_t addr, uint32_t site);
void psp_coverage_save();
void psp_fn_census_report(long cuadros);
void psp_call(Ctx& c, uint32_t dir);
void psp_jump(Ctx& c, uint32_t dir);
void psp_hle(Ctx& c, int id);
void psp_syscall(Ctx& c, uint32_t code);
void psp_break(Ctx& c, uint32_t code);
void psp_vfpu(Ctx& c, uint32_t pc, uint32_t w);
bool psp_vfpu_cc(Ctx& c, int bit);
void psp_vfpu_init(Ctx& c);
void psp_fault(const char* what, uint32_t v);
void psp_hook(uint32_t dir, Ctx& c);
void psp_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&));
void psp_probe(uint32_t pc, Ctx& c);
void psp_after(uint32_t pc, Ctx& c);
void psp_sp_bad(uint32_t dir, uint32_t sp0, Ctx& c);
void psp_ge_list(uint32_t start, uint32_t stop);
void psp_ge_stall(uint32_t stop);
void psp_ge_finish();
void psp_gpu_origin(uint32_t pc);
void psp_gpu_cmd(int c, uint32_t arg);
void psp_gpu_frame(uint32_t dir, int width, int fmt);
bool psp_gpu_to_window(uint32_t dir, int width, int fmt);
void psp_gpu_report();
bool psp_window_init(int scale);
int psp_scale();
bool psp_window_scene_requested();
bool psp_window_lod_toggle();
void psp_window_osk_open(const char* title, const char* initial, int cap);
int psp_window_osk_state(char* out_buf, int cap);
extern std::atomic<int> watchdog_pause;
void psp_gpu_prim_addr(uint32_t dir);
void psp_window_paint();
uint32_t psp_window_buttons();
void psp_window_stick(uint8_t* x, uint8_t* y);
void psp_window_rumble(float strong, float weak, int ms);
void psp_sfx_hook(uint32_t dir, uint32_t tam, uint32_t ra);
void psp_window_frame_ready();
double psp_window_fps();
double psp_window_clock();
int psp_gpu_load();
double psp_window_aspect();
bool psp_window_report_requested();
bool psp_window_capture_requested();
void psp_gpu_capture(uint32_t dir, int width, int fmt);
void psp_scheduler_hook(void (*fn)());
void psp_request_stop();
bool psp_window_open();
uint32_t* psp_window_pixels();
uint32_t* psp_window_canvas(int w, int h);
void psp_window_sleep(int ms);
unsigned psp_window_ms();
void psp_ge_report();
const char* psp_import_name(int id);
int psp_imports_n();
extern const Bank BANKS[];
extern const int BANKS_N;
extern const Import IMPORTS[];
extern const int IMPORTS_N;
uint64_t psp_time_us();
void psp_gp(uint32_t gp);
long psp_switches();
void psp_switch_cap(long n);
void psp_time_cap(double secs);
int psp_thread_prio(int id);
Ctx* psp_thread_ctx(int id);
uint32_t psp_thread_entry(int id);
void psp_yield();
void psp_block(uint64_t until_us);
void psp_wake(int id);
void psp_thread_exit();
int psp_thread_create(uint32_t entry_pc, uint32_t stack_top, int prio);
void psp_thread_start(int id, uint32_t arg_len, uint32_t arg_ptr);
int psp_thread_current();
int psp_threads_n();
int psp_thread_state(int id);
const char* psp_schedule();
void psp_stop_all();
int psp_mbx_create();
void psp_mbx_send(int id, uint32_t msg);
int psp_mbx_receive(int id, uint32_t* msg, uint64_t wait_us);
int psp_sema_create(int initial, int maximum);
int psp_sema_wait(int id, int n, uint64_t wait_us);
void psp_sema_signal(int id, int n);
int psp_flag_create(uint32_t bits);
void psp_flag_set(int id, uint32_t bits);
void psp_flag_clear(int id, uint32_t bits);
int psp_flag_wait(int id, uint32_t pattern, int mode, uint32_t* out_buf, uint64_t wait_us);
extern uint64_t psp_hle_count[4096];
extern uint32_t psp_unresolved;
extern int psp_jalr_trace;

static inline uint8_t* psp_p(uint32_t a) { return psp_mem + (a & psp_mem_mask); }

static inline uint8_t psp_l8(uint32_t a) { return *psp_p(a); }
static inline uint16_t psp_l16(uint32_t a) { uint16_t v; memcpy(&v, psp_p(a), 2); return v; }
static inline uint32_t psp_l32(uint32_t a) { uint32_t v; memcpy(&v, psp_p(a), 4); return v; }
static inline void psp_s8(uint32_t a, uint32_t v) { *psp_p(a) = (uint8_t)v; }
static inline void psp_s16(uint32_t a, uint32_t v) { uint16_t x = (uint16_t)v; memcpy(psp_p(a), &x, 2); }
static inline void psp_s32(uint32_t a, uint32_t v) { memcpy(psp_p(a), &v, 4); }

static inline uint32_t psp_lwl(uint32_t old, uint32_t a) {
    uint32_t s = (a & 3u) * 8;
    uint32_t w = psp_l32(a & ~3u);
    return (old & (0x00FFFFFFu >> s)) | (w << (24 - s));
}

static inline uint32_t psp_lwr(uint32_t old, uint32_t a) {
    uint32_t s = (a & 3u) * 8;
    uint32_t w = psp_l32(a & ~3u);
    return (old & (0xFFFFFF00u << (24 - s))) | (w >> s);
}

static inline void psp_swl(uint32_t a, uint32_t v) {
    uint32_t s = (a & 3u) * 8;
    uint32_t base = a & ~3u;
    psp_s32(base, (v >> (24 - s)) | (psp_l32(base) & (0xFFFFFF00u << s)));
}

static inline void psp_swr(uint32_t a, uint32_t v) {
    uint32_t s = (a & 3u) * 8;
    uint32_t base = a & ~3u;
    psp_s32(base, (v << s) | (psp_l32(base) & (0x00FFFFFFu >> (24 - s))));
}

static inline uint32_t psp_rotr(uint32_t v, uint32_t n) {
    n &= 31;
    return n ? ((v >> n) | (v << (32 - n))) : v;
}

static inline uint32_t psp_clz(uint32_t v) {
    uint32_t n = 0;
    while (n < 32 && !(v & 0x80000000u)) { v <<= 1; n++; }
    return n;
}

static inline uint32_t psp_ext(uint32_t v, uint32_t pos, uint32_t size) {
    if (size >= 32) return v >> pos;
    return (v >> pos) & ((1u << size) - 1u);
}

static inline uint32_t psp_ins(uint32_t dst, uint32_t src, uint32_t pos, uint32_t size) {
    if (size == 0 || size > 32) return dst;
    uint32_t m = (size >= 32) ? 0xFFFFFFFFu : (((1u << size) - 1u) << pos);
    return (dst & ~m) | ((src << pos) & m);
}

static inline uint32_t psp_wsbh(uint32_t v) {
    return ((v & 0x00FF00FFu) << 8) | ((v & 0xFF00FF00u) >> 8);
}

static inline uint32_t psp_wsbw(uint32_t v) {
    return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
}

static inline uint32_t psp_bitrev(uint32_t v) {
    v = ((v & 0x55555555u) << 1) | ((v >> 1) & 0x55555555u);
    v = ((v & 0x33333333u) << 2) | ((v >> 2) & 0x33333333u);
    v = ((v & 0x0F0F0F0Fu) << 4) | ((v >> 4) & 0x0F0F0F0Fu);
    return psp_wsbw(v);
}

static inline void psp_mult(Ctx& c, int32_t a, int32_t b) {
    int64_t p = (int64_t)a * (int64_t)b;
    c.lo = (uint32_t)p; c.hi = (uint32_t)((uint64_t)p >> 32);
}

static inline void psp_multu(Ctx& c, uint32_t a, uint32_t b) {
    uint64_t p = (uint64_t)a * (uint64_t)b;
    c.lo = (uint32_t)p; c.hi = (uint32_t)(p >> 32);
}

static inline void psp_div(Ctx& c, int32_t a, int32_t b) {
    if (b == 0) { c.lo = (a >= 0) ? 0xFFFFFFFFu : 1u; c.hi = (uint32_t)a; return; }
    if (a == (int32_t)0x80000000 && b == -1) { c.lo = 0x80000000u; c.hi = 0; return; }
    c.lo = (uint32_t)(a / b); c.hi = (uint32_t)(a % b);
}

static inline void psp_divu(Ctx& c, uint32_t a, uint32_t b) {
    if (b == 0) { c.lo = 0xFFFFFFFFu; c.hi = a; return; }
    c.lo = a / b; c.hi = a % b;
}

static inline uint64_t psp_thread(Ctx& c) { return ((uint64_t)c.hi << 32) | c.lo; }
static inline void psp_set_thread(Ctx& c, uint64_t v) { c.hi = (uint32_t)(v >> 32); c.lo = (uint32_t)v; }

static inline void psp_madd(Ctx& c, int32_t a, int32_t b) {
    psp_set_thread(c, psp_thread(c) + (uint64_t)((int64_t)a * (int64_t)b));
}
static inline void psp_maddu(Ctx& c, uint32_t a, uint32_t b) {
    psp_set_thread(c, psp_thread(c) + (uint64_t)a * (uint64_t)b);
}
static inline void psp_msub(Ctx& c, int32_t a, int32_t b) {
    psp_set_thread(c, psp_thread(c) - (uint64_t)((int64_t)a * (int64_t)b));
}
static inline void psp_msubu(Ctx& c, uint32_t a, uint32_t b) {
    psp_set_thread(c, psp_thread(c) - (uint64_t)a * (uint64_t)b);
}

static inline float psp_sqrt(float v) { return std::sqrt(v); }

static inline int32_t psp_sat(double v) {
    if (!(v == v)) return 0x7FFFFFFF;
    if (v >= 2147483648.0) return 0x7FFFFFFF;
    if (v < -2147483648.0) return (int32_t)0x80000000;
    return (int32_t)v;
}

static inline int32_t psp_fcvt_T(float v) { return psp_sat(std::trunc((double)v)); }
static inline int32_t psp_fcvt_F(float v) { return psp_sat(std::floor((double)v)); }
static inline int32_t psp_fcvt_C(float v) { return psp_sat(std::ceil((double)v)); }
static inline int32_t psp_fcvt_R(float v) { return psp_sat(std::nearbyint((double)v)); }
static inline int32_t psp_fcvt_N(float v) { return psp_sat(std::nearbyint((double)v)); }

static inline bool psp_nan(float a, float b) { return !(a == a) || !(b == b); }
static inline bool psp_cmp_f(float a, float b) { (void)a; (void)b; return false; }
static inline bool psp_cmp_un(float a, float b) { return psp_nan(a, b); }
static inline bool psp_cmp_eq(float a, float b) { return !psp_nan(a, b) && a == b; }
static inline bool psp_cmp_ueq(float a, float b) { return psp_nan(a, b) || a == b; }
static inline bool psp_cmp_olt(float a, float b) { return !psp_nan(a, b) && a < b; }
static inline bool psp_cmp_ult(float a, float b) { return psp_nan(a, b) || a < b; }
static inline bool psp_cmp_ole(float a, float b) { return !psp_nan(a, b) && a <= b; }
static inline bool psp_cmp_ule(float a, float b) { return psp_nan(a, b) || a <= b; }
static inline bool psp_cmp_sf(float a, float b) { return psp_cmp_f(a, b); }
static inline bool psp_cmp_ngle(float a, float b) { return psp_cmp_un(a, b); }
static inline bool psp_cmp_seq(float a, float b) { return psp_cmp_eq(a, b); }
static inline bool psp_cmp_ngl(float a, float b) { return psp_cmp_ueq(a, b); }
static inline bool psp_cmp_lt(float a, float b) { return psp_cmp_olt(a, b); }
static inline bool psp_cmp_nge(float a, float b) { return psp_cmp_ult(a, b); }
static inline bool psp_cmp_le(float a, float b) { return psp_cmp_ole(a, b); }
static inline bool psp_cmp_ngt(float a, float b) { return psp_cmp_ule(a, b); }

static inline uint32_t psp_cfc1(Ctx& c, uint32_t reg) {
    if (reg == 31) return (c.fcr31 & ~0x00800000u) | (c.fcc ? 0x00800000u : 0u);
    return 0;
}
static inline void psp_ctc1(Ctx& c, uint32_t reg, uint32_t v) {
    if (reg == 31) { c.fcr31 = v; c.fcc = (v & 0x00800000u) != 0; }
}
