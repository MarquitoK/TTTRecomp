#pragma once
#include <cstdint>

// Zone profiler. Flat model: one global active zone, every mark
// adds the elapsed time to the previous zone. Guest thread switches
// save and restore their zone, so the zone sum is wall time.

#ifndef TTT_PERF
#define TTT_PERF 1
#endif

enum { PZ_GEN, PZ_RES, PZ_HLE, PZ_GE, PZ_RAST, PZ_TEX, PZ_PRES, PZ_SCHED, PZ_WAIT, PZ_SWAP, PZ_N };
enum { PC_JALR, PC_IMPORT, PC_HLE, PC_CMD, PC_PRIM, PC_PIX, PC_ZFAIL, PC_PIXA, PC_PIXNZ, PC_PIXST, PC_FLUSH, PC_VDEST, PC_VPAL, PC_VTEX, PC_VVRAM, PC_VBLANK, PC_SWITCH, PC_TEXSUB, PC_TEXPX, PC_HOOK, PC_TRIOUT, PC_TRICULL, PC_N };

#if TTT_PERF

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64)
#include <x86intrin.h>
static inline uint64_t perf_clock() { return __rdtsc(); }
#else
static inline uint64_t perf_clock() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
#endif

inline uint64_t perf_cycles[PZ_N];
inline uint64_t perf_count[PC_N];
inline uint64_t perf_last = 0;
inline int perf_zone = PZ_GEN;
inline uint64_t perf_frames = 0;
inline uint64_t perf_frames_base = 0;
inline double perf_t0 = 0;
inline int perf_period = 0;
inline uint64_t perf_every = 0;

// peak detection: the 6 slowest frames of each window are kept with
// that frame's zone breakdown, the only thing that exposes a single stall
struct PerfPeak { double ms; uint64_t frame; uint64_t z[PZ_N]; uint64_t cmd, prim, tex, hle; };
inline PerfPeak perf_peaks[6];
inline int perf_peaks_n = 0;
inline double perf_t_frame = 0;
inline uint64_t perf_z_prev[PZ_N];
inline uint64_t perf_cmd_prev, perf_prim_prev, perf_tex_prev, perf_hle_prev;

static inline int perf_mark(int z) {
    uint64_t t = perf_clock();
    if (perf_last) perf_cycles[perf_zone] += t - perf_last;
    perf_last = t;
    int v = perf_zone;
    perf_zone = z;
    return v;
}

struct PerfZone {
    int prev;
    explicit PerfZone(int z) { prev = perf_mark(z); }
    ~PerfZone() { perf_mark(prev); }
};

// peak probe: named sub-costs of the current frame, printed with the zones when a frame exceeds TTT_PICO_MS (default 24)
struct PerfSub { const char* name; uint64_t cyc, n; };
inline PerfSub perf_sub[64];
inline int perf_sub_n = 0, perf_pico_lines = 0, perf_pico_big = 0;
inline double perf_pico_ms = -1;
inline int perf_sub_id(const char* name) {
    for (int i = 0; i < perf_sub_n; i++) if (perf_sub[i].name == name || !strcmp(perf_sub[i].name, name)) return i;
    if (perf_sub_n >= 64) return 63;
    perf_sub[perf_sub_n] = {name, 0, 0};
    return perf_sub_n++;
}
inline void perf_sub_add(int id, uint64_t c) { perf_sub[id].cyc += c; perf_sub[id].n++; }
struct PerfSubT { int id; uint64_t t0; explicit PerfSubT(int i) : id(i), t0(perf_clock()) {} ~PerfSubT() { perf_sub_add(id, perf_clock() - t0); } };
#define PERF_SUB_CAT2(a, b) a##b
#define PERF_SUB_CAT(a, b) PERF_SUB_CAT2(a, b)
#define PERF_SUB(name) static int PERF_SUB_CAT(_psid_, __LINE__) = perf_sub_id(name); PerfSubT PERF_SUB_CAT(_pst_, __LINE__)(PERF_SUB_CAT(_psid_, __LINE__))

static inline double perf_seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

inline void (*perf_block_hook)(const char*) = nullptr;
inline void perf_report(const char* label) {
    perf_mark(perf_zone);
    static const char* NOM[PZ_N] = {"generado", "resolucion", "hle", "ge", "raster", "texturas", "presenta", "planif", "espera", "intercambio"};
    uint64_t tot = 0;
    for (int i = 0; i < PZ_N; i++) tot += perf_cycles[i];
    if (!tot) return;
    double secs = perf_t0 ? perf_seconds() - perf_t0 : 0;
    uint64_t nc = perf_frames - perf_frames_base;
    if (!nc) nc = 1;
    printf("perf %s: %llu cuadros en %.2f s -> %.2f fps | ciclos/cuadro %.2f M\n",
           label, (unsigned long long)nc, secs, secs > 0 ? nc / secs : 0.0, tot / 1e6 / nc);
    uint64_t util = tot - perf_cycles[PZ_WAIT];
    if (util) printf("perf %s: sin espera -> %.2f fps equivalentes\n", label,
                     secs > 0 ? nc / (secs * (double)util / (double)tot) : 0.0);
    for (int i = 0; i < PZ_N; i++)
        printf("perf %s:   %-11s %6.2f%%  %8.2f M/cuadro\n", label, NOM[i],
               100.0 * perf_cycles[i] / tot, perf_cycles[i] / 1e6 / nc);
    printf("perf %s: por cuadro jalr %.0f (import %.0f) hle %.0f cmd %.0f prim %.0f pix %.0f zfalla %.0f cambios %.0f\n",
           label, (double)perf_count[PC_JALR] / nc, (double)perf_count[PC_IMPORT] / nc,
           (double)perf_count[PC_HLE] / nc, (double)perf_count[PC_CMD] / nc,
           (double)perf_count[PC_PRIM] / nc, (double)perf_count[PC_PIX] / nc,
           (double)perf_count[PC_ZFAIL] / nc, (double)perf_count[PC_SWITCH] / nc);
    if (perf_count[PC_PIX])
        printf("perf %s: %.1f ciclos por pixel cubierto | z temprano rechaza %.1f%% | pixeles con atest %.1f%%, sin ztest %.1f%%, con stencil %.1f%%\n", label,
               (double)perf_cycles[PZ_RAST] / (double)perf_count[PC_PIX],
               100.0 * (double)perf_count[PC_ZFAIL] / (double)perf_count[PC_PIX],
               100.0 * (double)perf_count[PC_PIXA] / (double)perf_count[PC_PIX],
               100.0 * (double)perf_count[PC_PIXNZ] / (double)perf_count[PC_PIX],
               100.0 * (double)perf_count[PC_PIXST] / (double)perf_count[PC_PIX]);
    printf("perf %s: %.0f vaciados por cuadro, %.0f pixeles por vaciado | motivos: destino %.0f paleta %.0f cachetex %.0f vram %.0f\n", label,
           (double)perf_count[PC_FLUSH] / nc,
           perf_count[PC_FLUSH] ? (double)perf_count[PC_PIX] / (double)perf_count[PC_FLUSH] : 0.0,
           (double)perf_count[PC_VDEST] / nc, (double)perf_count[PC_VPAL] / nc,
           (double)perf_count[PC_VTEX] / nc, (double)perf_count[PC_VVRAM] / nc);
    printf("perf %s: %.2f esperas de vblank por cuadro presentado\n", label,
           (double)perf_count[PC_VBLANK] / nc);
    printf("perf %s: por cuadro triangulos 3D descartados: %.0f por plano cercano, %.0f por cara trasera\n", label,
           (double)perf_count[PC_TRIOUT] / nc, (double)perf_count[PC_TRICULL] / nc);
    printf("perf %s: por cuadro %.1f subidas de textura (%.0f K texeles) | %.0f pasadas del gancho\n", label,
           (double)perf_count[PC_TEXSUB] / nc, (double)perf_count[PC_TEXPX] / nc / 1000.0,
           (double)perf_count[PC_HOOK] / nc);
    if (perf_count[PC_JALR])
        printf("perf %s: coste medio resolucion %.0f ciclos por llamada indirecta\n", label,
               (double)perf_cycles[PZ_RES] / (double)perf_count[PC_JALR]);
    for (int i = 0; i < perf_peaks_n; i++) {
        PerfPeak& p = perf_peaks[i];
        uint64_t t = 0;
        int a = 0, b = -1;
        for (int k = 0; k < PZ_N; k++) t += p.z[k];
        for (int k = 1; k < PZ_N; k++) if (p.z[k] > p.z[a]) a = k;
        for (int k = 0; k < PZ_N; k++) if (k != a && (b < 0 || p.z[k] > p.z[b])) b = k;
        if (!t) t = 1;
        printf("perf %s: pico cuadro %llu: %.1f ms | %s %.0f%% %s %.0f%% | cmd %llu prim %llu subtex %llu hle %llu\n",
               label, (unsigned long long)p.frame, p.ms, NOM[a], 100.0 * p.z[a] / t, NOM[b], 100.0 * p.z[b] / t,
               (unsigned long long)p.cmd, (unsigned long long)p.prim, (unsigned long long)p.tex, (unsigned long long)p.hle);
    }
    if (perf_block_hook) perf_block_hook(label);
    fflush(stdout);
}

inline void perf_reset() {
    perf_peaks_n = 0;
    perf_t_frame = 0;
    for (int i = 0; i < PZ_N; i++) perf_z_prev[i] = 0;
    perf_cmd_prev = perf_prim_prev = perf_tex_prev = perf_hle_prev = 0;
    for (int i = 0; i < PZ_N; i++) perf_cycles[i] = 0;
    for (int i = 0; i < PC_N; i++) perf_count[i] = 0;
    perf_frames_base = perf_frames;
    perf_t0 = perf_seconds();
    perf_last = perf_clock();
}

inline void perf_note_peak() {
    perf_mark(perf_zone);
    double now_t = perf_seconds();
    if (perf_t_frame > 0) {
        PerfPeak p;
        p.ms = (now_t - perf_t_frame) * 1000.0;
        p.frame = perf_frames;
        for (int k = 0; k < PZ_N; k++) { p.z[k] = perf_cycles[k] - perf_z_prev[k]; }
        p.cmd = perf_count[PC_CMD] - perf_cmd_prev;
        p.prim = perf_count[PC_PRIM] - perf_prim_prev;
        p.tex = perf_count[PC_TEXSUB] - perf_tex_prev;
        p.hle = perf_count[PC_HLE] - perf_hle_prev;
        if (perf_pico_ms < 0) { const char* e = getenv("TTT_PICO_MS"); perf_pico_ms = e && *e ? atof(e) : 24.0; }
        if (perf_pico_ms > 0 && p.ms >= perf_pico_ms && (perf_pico_lines < 30 || (p.ms >= 100.0 && perf_pico_big < 40))) {   // freezes of 100 ms or more always reach the log
            if (perf_pico_lines < 30) perf_pico_lines++; else perf_pico_big++;
            uint64_t tot = 0; for (int k = 0; k < PZ_N; k++) tot += p.z[k];
            double mpc = tot ? p.ms / (double)tot : 0;
            static const char* NZ[PZ_N] = {"generado", "resolucion", "hle", "ge", "raster", "texturas", "presenta", "planif", "espera", "intercambio"};
            printf("pico: cuadro %llu %.1f ms | zonas:", (unsigned long long)p.frame, p.ms);
            for (int k = 0; k < PZ_N; k++) if (p.z[k] * mpc >= 0.5) printf(" %s %.1f", NZ[k], p.z[k] * mpc);
            printf(" | partes:");
            for (int i = 0; i < perf_sub_n; i++) if (perf_sub[i].cyc * mpc >= 0.3) printf(" %s %.1f (%llu)", perf_sub[i].name, perf_sub[i].cyc * mpc, (unsigned long long)perf_sub[i].n);
            printf(" | cmd %llu prim %llu subtex %llu hle %llu\n", (unsigned long long)p.cmd, (unsigned long long)p.prim, (unsigned long long)p.tex, (unsigned long long)p.hle);
            fflush(stdout);
        }
        for (int i = 0; i < perf_sub_n; i++) { perf_sub[i].cyc = 0; perf_sub[i].n = 0; }
        int n = perf_peaks_n;
        if (n < 6) perf_peaks_n = ++n;
        else if (p.ms <= perf_peaks[5].ms) n = 0;
        if (n) {
            int i = n - 1;
            while (i > 0 && perf_peaks[i - 1].ms < p.ms) { perf_peaks[i] = perf_peaks[i - 1]; i--; }
            perf_peaks[i] = p;
        }
    }
    perf_t_frame = now_t;
    for (int k = 0; k < PZ_N; k++) perf_z_prev[k] = perf_cycles[k];
    perf_cmd_prev = perf_count[PC_CMD];
    perf_prim_prev = perf_count[PC_PRIM];
    perf_tex_prev = perf_count[PC_TEXSUB];
    perf_hle_prev = perf_count[PC_HLE];
}

inline void perf_frame() {
    perf_frames++;
    if (!perf_every) { const char* e = getenv("TTT_PERF_N"); perf_every = e ? (uint64_t)atoll(e) : 300; }
    if (!perf_t0) { perf_reset(); return; }
    perf_note_peak();
    if (perf_frames - perf_frames_base < perf_every) return;
    char et[16];
    snprintf(et, sizeof(et), "%d", ++perf_period);
    perf_report(et);
    perf_reset();
}

#define PERF(z) PerfZone _pz(z)
#define PERF_N(c, k) (perf_count[c] += (uint64_t)(k))

#else
inline void (*perf_block_hook)(const char*) = nullptr;

struct PerfZone { explicit PerfZone(int) {} };
#define PERF_SUB(name) ((void)0)
inline int perf_sub_id(const char*) { return 0; }
inline void perf_sub_add(int, uint64_t) {}
static inline int perf_mark(int z) { (void)z; return 0; }
inline void perf_report(const char*) {}
inline void perf_reset() {}
inline void perf_frame() {}
#define PERF(z) ((void)0)
#define PERF_N(c, k) ((void)0)

#endif
