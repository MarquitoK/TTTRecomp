// TTT_STALLS=1 (or =N ms, default 25): one "stall:" block per slow presented frame with the guest threads and
// functions sampled every millisecond, the imports that took the time and the host work done inside it
#include <mutex>
extern uint64_t psp_hle_count[4096];
#include <map>
#include <algorithm>
int psp_fps60();

static double t_start_s = 0;
std::atomic<uint32_t> psp_stall_fn{0}, psp_stall_ra{0};
static const char* STALL_KINDS[] = {"texpng", "texraw", "texmip", "texgl", "target", "disc", "sfx", "at3", "toon"};
enum { STALL_KINDS_N = 9 };
struct StallAcc { std::atomic<uint32_t> n{0}; std::atomic<uint64_t> us{0}; std::atomic<uint32_t> big{0}; };
static StallAcc stall_acc[STALL_KINDS_N];
struct StallSample { int16_t thr, hle; int8_t zone; uint32_t fn, ra; };
static std::mutex stall_mx;
static std::vector<StallSample> stall_samples;
static std::atomic<bool> stall_open{false};
static uint32_t stall_hle_n[4096];
static uint64_t stall_hle_us[4096];
static std::vector<int> stall_hle_list;

static uint64_t stall_us() { return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

void psp_stall_note(int k, uint64_t us, uint32_t size) {
    if (!psp_stall_on || k < 0 || k >= STALL_KINDS_N) return;
    StallAcc& a = stall_acc[k];
    a.n.fetch_add(1, std::memory_order_relaxed);
    a.us.fetch_add(us, std::memory_order_relaxed);
    uint32_t b = a.big.load(std::memory_order_relaxed);
    while (size > b && !a.big.compare_exchange_weak(b, size)) {}
}

static std::string stall_files;
void psp_stall_file(const char* name) {
    if (!psp_stall_on) return;
    std::lock_guard<std::mutex> g(stall_mx);
    if (stall_files.size() < 300) { stall_files += ' '; stall_files += name; }
}

struct StallHle {
    int id; uint64_t t;
    explicit StallHle(int i) : id(i), t(psp_stall_on ? stall_us() : 0) {}
    ~StallHle() {
        if (!t || id < 0 || id >= 4096) return;
        if (!stall_hle_n[id]) stall_hle_list.push_back(id);
        stall_hle_n[id]++;
        stall_hle_us[id] += stall_us() - t;
    }
};

static void stall_sampler() {
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!stall_open.load(std::memory_order_relaxed)) continue;
        StallSample s;
        s.thr = (int16_t)psp_thread_current();
        s.zone = (int8_t)perf_zone;
        s.hle = (int16_t)(s.zone == PZ_HLE ? last_hle : -1);
        s.fn = psp_stall_fn.load(std::memory_order_relaxed);
        s.ra = psp_stall_ra.load(std::memory_order_relaxed);
        std::lock_guard<std::mutex> g(stall_mx);
        if (stall_samples.size() < 8000) stall_samples.push_back(s);
    }
}

static void stall_print(int id, double dt, double usual) {
    static const char* ZN[PZ_N] = {"gen", "resolve", "hle", "ge", "raster", "tex", "present", "sched", "wait", "swap"};
    std::vector<StallSample> v;
    { std::lock_guard<std::mutex> g(stall_mx); v.swap(stall_samples); }
    printf("stall: %d at %.2f s, frame %ld, %.1f ms (usual %.1f), load %d, fps60 %d, samples %zu\n",
           id, clock_s() - t_start_s, cuadros, dt, usual, cg.active ? 1 : 0, psp_fps60() ? 1 : 0, v.size());
    int zc[PZ_N] = {0};
    std::map<int, int> thr_n;
    std::map<std::pair<int, uint64_t>, int> fns;
    std::map<int, int> hles;
    for (auto& s : v) {
        if (s.zone >= 0 && s.zone < PZ_N) zc[s.zone]++;
        thr_n[s.thr]++;
        fns[{s.thr, ((uint64_t)s.fn << 32) | s.ra}]++;
        if (s.hle >= 0) hles[s.hle]++;
    }
    char b[512]; int o = snprintf(b, sizeof(b), "stall: %d zones", id);
    for (int z = 0; z < PZ_N; z++) if (zc[z]) o += snprintf(b + o, sizeof(b) - o, " %s %d", ZN[z], zc[z]);
    printf("%s\n", b);
    for (auto& t : thr_n) {
        std::vector<std::pair<int, uint64_t>> top;
        for (auto& f : fns) if (f.first.first == t.first) top.push_back({f.second, f.first.second});
        std::sort(top.begin(), top.end(), [](const std::pair<int, uint64_t>& a, const std::pair<int, uint64_t>& c) { return a.first > c.first; });
        o = snprintf(b, sizeof(b), "stall: %d thread %d prio %d entry 0x%08X: %d samples |", id, t.first, psp_thread_prio(t.first), psp_thread_entry(t.first), t.second);
        for (size_t i = 0; i < top.size() && i < 5; i++)
            o += snprintf(b + o, sizeof(b) - o, " 0x%08X<0x%08X %d", (uint32_t)(top[i].second >> 32), (uint32_t)top[i].second ? (uint32_t)top[i].second - 8 : 0, top[i].first);
        printf("%s\n", b);
    }
    if (!hles.empty()) {
        o = snprintf(b, sizeof(b), "stall: %d inside imports", id);
        for (auto& h : hles) o += snprintf(b + o, sizeof(b) - o, " %s %d", psp_import_name(h.first), h.second);
        printf("%s\n", b);
    }
    std::sort(stall_hle_list.begin(), stall_hle_list.end(), [](int a, int c) { return stall_hle_us[a] > stall_hle_us[c]; });
    o = snprintf(b, sizeof(b), "stall: %d imports", id);
    for (size_t i = 0; i < stall_hle_list.size() && i < 6; i++) {
        int h = stall_hle_list[i];
        o += snprintf(b + o, sizeof(b) - o, " %s %ux %.1f ms", psp_import_name(h), stall_hle_n[h], stall_hle_us[h] / 1000.0);
    }
    printf("%s\n", b);
    o = snprintf(b, sizeof(b), "stall: %d host", id);
    for (int k = 0; k < STALL_KINDS_N; k++) {
        uint32_t n = stall_acc[k].n.load();
        if (n) o += snprintf(b + o, sizeof(b) - o, " %s %ux %.1f ms max %u", STALL_KINDS[k], n, stall_acc[k].us.load() / 1000.0, stall_acc[k].big.load());
    }
    printf("%s\n", b);
    { std::lock_guard<std::mutex> g(stall_mx); if (!stall_files.empty()) printf("stall: %d disco%s\n", id, stall_files.c_str()); }
    fflush(stdout);
}

static void stall_frame() {
    if (!psp_stall_on) return;
    static double last = 0, usual = 0;
    static int min_ms = -1, shown = 0;
    if (min_ms < 0) {
        const char* e = getenv("TTT_STALLS");
        min_ms = e && atoi(e) > 1 ? atoi(e) : 25;
        t_start_s = clock_s();
        std::thread(stall_sampler).detach();
        printf("stall: activo, umbral %d ms\n", min_ms);
    }
    double now = clock_s() * 1000.0, dt = last > 0 ? now - last : 0;
    last = now;
    bool stall = dt > 0 && usual > 0 && dt >= min_ms && dt > usual * 1.5 + 6.0;
    if (dt > 0 && !stall) usual = usual > 0 ? usual * 0.9 + dt * 0.1 : dt;
    if (stall && shown < 300) stall_print(++shown, dt, usual);
    { std::lock_guard<std::mutex> g(stall_mx); stall_samples.clear(); stall_files.clear(); }
    for (int h : stall_hle_list) { stall_hle_n[h] = 0; stall_hle_us[h] = 0; }
    stall_hle_list.clear();
    for (auto& a : stall_acc) { a.n = 0; a.us = 0; a.big = 0; }
    stall_open = true;
}

// hot imports answered before the name chain: interrupt suspend/resume and memset, tens of thousands per load
static bool hle_fast(Ctx& c, int id) {
    static int s = -2, r = -1, rs = -1, ms = -1;
    if (s == -2) {
        s = -1;
        for (int i = 0; i < IMPORTS_N; i++) {
            const char* n = psp_import_name(i);
            if (!strcmp(n, "sceKernelCpuSuspendIntr")) s = i;
            else if (!strcmp(n, "sceKernelCpuResumeIntr")) r = i;
            else if (!strcmp(n, "sceKernelCpuResumeIntrWithSync")) rs = i;
            else if (!strcmp(n, "sceKernelMemset")) ms = i;
        }
    }
    if (id < 0 || (id != s && id != r && id != rs && id != ms)) return false;
    static bool seen[4] = {false, false, false, false};
    int k = id == s ? 0 : id == r ? 1 : id == rs ? 2 : 3;
    if (!seen[k]) { seen[k] = true; return false; }
    if (id < 4096) psp_hle_count[id]++;
    uint32_t res = 0;
    if (k == 3) {
        uint32_t a0 = c.r[4], a2 = c.r[6];
        if ((uint64_t)(a0 & psp_mem_mask) + a2 > psp_mem_mask) return false;
        memset(psp_p(a0), (int)(c.r[5] & 0xFF), a2);
        res = a0;
    }
    c.r[2] = res;
    c.r[3] = 0;
    return true;
}
