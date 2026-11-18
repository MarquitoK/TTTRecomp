// callbacks: created with sceKernelCreateCallback and delivered when a thread calls a *CB function
struct Callback { uint32_t fn, arg; int id; };
static Callback cbs[32];
static int ncbs = 0;
static struct { int id; uint32_t a1, a2; } cb_queue[64];
static int nqueue = 0;
static int cb_ms = -1;
static bool ms_warned = false;

static void enqueue_cb(int id, uint32_t a1, uint32_t a2) {
    if (id < 0 || nqueue >= 64) return;
    cb_queue[nqueue].id = id; cb_queue[nqueue].a1 = a1; cb_queue[nqueue].a2 = a2; nqueue++;
}

static void deliver_cbs(Ctx& c) {
    while (nqueue > 0) {
        int id = cb_queue[0].id;
        uint32_t a1 = cb_queue[0].a1, a2 = cb_queue[0].a2;
        for (int i = 1; i < nqueue; i++) cb_queue[i - 1] = cb_queue[i];
        nqueue--;
        Callback* cb = nullptr;
        for (int i = 0; i < ncbs; i++) if (cbs[i].id == id) cb = &cbs[i];
        if (!cb) continue;
        Fn f = psp_resolve(cb->fn);
        if (!f) continue;
        printf("callback %d -> 0x%08X(%u, %u, 0x%08X)\n", id, cb->fn, a1, a2, cb->arg);
        Ctx h = c;
        h.r[4] = a1; h.r[5] = a2; h.r[6] = cb->arg;
        h.r[29] = (c.r[29] - 0x400) & ~0xFu;
        h.r[31] = 0;
        { PERF(PZ_GEN); f(h); }
    }
}

