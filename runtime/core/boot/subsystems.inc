#include "audio/sound.h"
#include "net/net.h"
#include "save/savedata.h"

// pad read delay (like RetroArch frame delay): the game reads the pad right after
// SetFrameBuf, computes and draws in a few ms and then sleeps until vblank. We sleep before reading, until
// vblank minus what that span takes (maximum of the last 32 frames) plus a margin.
static int nvb_last = 1;
int psp_present_nvb() { return nvb_last; }
double psp_window_mid_due();

// smooth motion: the window's second picture falls inside the game's sleeps; the window paints it on its own thread (scheduler hook)
static bool mid_step(double until) {
    static double last = 0;
    double due = psp_window_mid_due();
    if (due <= 0 || due > until || due == last) return false;
    last = due;
    for (;;) {
        double missing = due - psp_window_clock();
        if (missing <= 0) break;
        if (missing > 0.0015) psp_window_sleep((int)((missing - 0.001) * 1000.0));
    }
    psp_yield();
    return true;
}
static double fd_span[32];
static int fd_i = 0;
static double fd_read = -1;

static double read_delay() {
    static int mode = -1;
    static double margin = 3.0;
    if (mode < 0) {
        const char* e = getenv("TTT_INPUT_DELAY"); mode = e ? atoi(e) : 1;
        e = getenv("TTT_INPUT_DELAY_MARGIN"); if (e) margin = atof(e);
        for (int i = 0; i < 32; i++) fd_span[i] = 16.0;
        printf("input: retraso de lectura %s, margen %.1f ms\n", mode ? "activo" : "desactivado", margin);
    }
    if (!mode || !with_window || !vblank_real() || vb_origin <= 0 || psp_thread_current() != present_thread) return 0;
    double span = 0;
    for (int i = 0; i < 32; i++) if (fd_span[i] > span) span = fd_span[i];
    double goal = vb_origin + (double)(vb_last + nvb_last) * vb_period() - (span + margin) / 1000.0;
    double t0 = psp_window_clock();
    if (goal - t0 > 0.1) goal = t0 + 0.1;
    PERF(PZ_WAIT);
    for (;;) {
        if (mid_step(goal)) continue;
        double missing = goal - psp_window_clock();
        if (missing <= 0) break;
        if (missing > 0.002) psp_window_sleep((int)((missing - 0.0015) * 1000.0));
    }
    return (psp_window_clock() - t0) * 1000.0;
}

static void vblank_rate_multi(int nvb) {
    if (!with_window) return;
    if (psp_thread_current() != present_thread) return;
    if (nvb < 1) nvb = 1;
    if (vb_origin <= 0) { vb_origin = psp_window_clock(); vb_last = 0; return; }
    long goal = vb_last + nvb;
    if (vb_current() >= goal) { vb_last = vb_current(); return; }   // late: do not accumulate delay
    PERF(PZ_WAIT);
    for (;;) {
        if (vb_current() >= goal) break;
        if (mid_step(vb_origin + (double)goal * vb_period())) continue;
        double missing = vb_origin + (double)goal * vb_period() - psp_window_clock();
        if (missing > 0.002) psp_window_sleep((int)((missing - 0.0015) * 1000.0));
    }
    vb_last = goal;
}


static void vblank_rate() {
    if (!with_window || vb_period_n() <= 0) return;
    if (psp_thread_current() != present_thread) return;
    static double t_vb = 0;
    double per = vb_period_n() / 1000.0;
    double now_t = psp_window_clock();
    if (t_vb <= 0) { t_vb = now_t + per; return; }
    t_vb += per;
    if (t_vb < now_t - 0.1) { t_vb = now_t + per; return; }   // far behind: resync
    if (t_vb <= now_t) return;
    PERF(PZ_WAIT);
    for (;;) {
        double missing = t_vb - psp_window_clock();
        if (missing <= 0) break;
        if (missing > 0.002) psp_window_sleep((int)((missing - 0.0015) * 1000.0));
    }
}

