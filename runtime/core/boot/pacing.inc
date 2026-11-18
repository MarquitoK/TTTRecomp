// TTT_FPS: vblank rate, 0 removes pacing. The game sets its speed by waiting for
// vblanks, so pacing lives here and not at present time: if it waits for two, it runs at half.
// without TTT_FPS pacing mimics the console: 60 in light scenes, 30 in heavy ones,
// because the game advances one step per drawn frame and reads no clock
static double vb_period_n() {
    static int fixed = -2;
    if (fixed == -2) { const char* e = getenv("TTT_FPS"); fixed = e ? atoi(e) : -1; }
    if (fixed == 0) return 0.0;
    if (fixed > 0) return 1000.0 / fixed;
    static int cap = -1;
    if (cap < 0) { const char* e = getenv("TTT_CAP"); cap = e ? atoi(e) : 600; }
    return psp_gpu_load() > cap ? 1000.0 / 30.0 : 1000.0 / 60.0;
}

static int present_thread = -1;

// TTT_VBLANK=0 restores the old model. The PSP vblank counter runs at 60 Hz wall time,
// not at the game's drawing rate: if the game reads the difference, it self-adjusts.
static int vblank_var = -1;
static bool vblank_real() {
    if (vblank_var < 0) { const char* e = getenv("TTT_VBLANK"); vblank_var = e ? atoi(e) : 1; }
    return vblank_var != 0;
}

static double vb_origin = 0;
static long vb_last = 0;

// TTT_RATE=N: the game advances its logic once every N vblanks but still
// presents on every vblank. Separates the simulation rate from the drawing rate.
static int rate_var = -1;
static int logic_rate() {
    if (rate_var < 0) { const char* e = getenv("TTT_RATE"); rate_var = e ? atoi(e) : 1; if (rate_var < 1) rate_var = 1; }
    return rate_var;
}

static double vb_period() {
    static double p = 0;
    if (p == 0) { const char* e = getenv("TTT_FPS"); int f = e ? atoi(e) : 60; if (f <= 0) f = 60; p = 1.0 / f; }
    return p;
}

static long vb_current() {
    if (vb_origin <= 0) vb_origin = psp_window_clock();
    double d = (psp_window_clock() - vb_origin) / vb_period();
    return d < 0 ? 0 : (long)d;
}

