// clash event manager 0x0893BD50 (state at [0x08A7DBC8]+0x40): 30 Hz logic. At 60 fps, with the event active,
// it runs one tick in two; in the skipped tick the writes of the executed tick are replayed (camera, scene marks).
// TTT_CLASH60: 1 replays everything (default), 2 everything but combatants, 3 no replay, 0 off
struct Region { uint32_t base, tam; bool comb; };
static std::vector<Region> gc_regs;
static std::vector<uint32_t> gc_foto;
static std::vector<std::pair<uint32_t, uint32_t>> gc_dif;
static int gc_modo() { return patch_clash_gate.val; }
static void gc_regions() {
    gc_regs.clear();
    gc_regs.push_back({0x08A60000u, 0x08B67000u - 0x08A60000u, false});
    uint32_t t = psp_l32(0x08A7DBC8u);
    if (t >= 0x08800000u && t < 0x10000000u) {
        gc_regs.push_back({t, 0x1000u, false});
        uint32_t b = psp_l32(t + 4), n = psp_l32(t);
        if (b >= 0x08800000u && b < 0x10000000u && n && n <= 4u) gc_regs.push_back({b, 0x1A90u * n, true});
    }
    uint32_t k = psp_l32(0x08A7BBE8u);
    if (k >= 0x08800000u && k < 0x10000000u) gc_regs.push_back({k, 0x1000u, false});
    for (uint32_t i = 0; i < 4; i++) { uint32_t e = psp_l32(0x08B61178u + 4u * i); if (e >= 0x08800000u && e < 0x10000000u) gc_regs.push_back({e, 0x1800u, true}); }
}
static void clash_manager(Ctx& c, void (*orig)(Ctx&)) {
    int modo = gc_modo();
    uint32_t t = psp_l32(0x08A7DBC8u);
    bool ok = t >= 0x08800000u && t < 0x10000000u;
    uint32_t st = ok ? psp_l32(t + 0x40) : 0;
    if (!modo || !battle_active() || !ok || st == 0) { orig(c); gc_dif.clear(); }
    else if (tick_odd) {
        if (modo != 3) for (auto& d : gc_dif) psp_s32(d.first, d.second);
        return;
    } else {
        gc_regions();
        size_t tot = 0; for (auto& r : gc_regs) tot += r.tam / 4;
        gc_foto.resize(tot);
        size_t k = 0; for (auto& r : gc_regs) for (uint32_t i = 0; i < r.tam; i += 4) gc_foto[k++] = psp_l32(r.base + i);
        orig(c);
        gc_dif.clear();
        k = 0;
        for (auto& r : gc_regs) for (uint32_t i = 0; i < r.tam; i += 4, k++) {
            uint32_t v = psp_l32(r.base + i);
            if (v != gc_foto[k] && !(modo == 2 && r.comb) && gc_dif.size() < 16384) gc_dif.push_back({r.base + i, v});
        }
        if (gc_dif.size() >= 16384) gc_dif.clear();
    }
}

struct RandTick { uint32_t tick, total, pair_v, n, ra[6], cnt[6]; };
static RandTick rand_ring[256];
static uint32_t rand_pos = 0, rand_n = 0, rand_ra[6], rand_cnt[6];
static void rand_note(uint32_t ra) {
    for (uint32_t i = 0; i < rand_n; i++) if (rand_ra[i] == ra) { rand_cnt[i]++; return; }
    if (rand_n < 6) { rand_ra[rand_n] = ra; rand_cnt[rand_n++] = 1; }
}
static void rand_close() {
    RandTick& r = rand_ring[rand_pos++ & 255];
    r.tick = tick_local; r.total = psp_l32(0x08A7E8B4u); r.pair_v = tick_odd ? 1 : 0; r.n = rand_n;
    memcpy(r.ra, rand_ra, sizeof r.ra); memcpy(r.cnt, rand_cnt, sizeof r.cnt);
    rand_n = 0;
}
// the shake (0x089AD480, camera and fighters) differs between instances: with its own
// Mersenne Twister (state 0x08B40E00, index 0x08A7E8B0, count 0x08A7E8B4) it does not desync the shared rand
static bool rand_private = false;
static void shake_private(Ctx& c, void (*orig)(Ctx&)) {
    static int on = -1; if (on < 0) { const char* e = getenv("TTT_RUMBLE_PRIVATE"); on = e ? atoi(e) : 1; }
    if (!on) { orig(c); return; }
    const uint32_t EST = 0x08B40E00u, TAM = 0x9C0u;
    static uint8_t mt[0x9C0]; static uint32_t idx = 0, cnt = 0; static bool ready = false;
    static uint8_t g[0x9C0];
    uint8_t* m = psp_p(EST);
    if (!ready) { memcpy(mt, m, TAM); idx = psp_l32(0x08A7E8B0u); ready = true; }
    memcpy(g, m, TAM);
    uint32_t gi = psp_l32(0x08A7E8B0u), gc = psp_l32(0x08A7E8B4u);
    memcpy(m, mt, TAM); psp_s32(0x08A7E8B0u, idx); psp_s32(0x08A7E8B4u, cnt);
    rand_private = true;
    orig(c);
    rand_private = false;
    memcpy(mt, m, TAM); idx = psp_l32(0x08A7E8B0u); cnt = psp_l32(0x08A7E8B4u);
    memcpy(m, g, TAM); psp_s32(0x08A7E8B0u, gi); psp_s32(0x08A7E8B4u, gc);
}
static void rand_dump(uint32_t phase) {
    static int times = 0;
    if (times++ >= 4) return;
    uint32_t n = rand_pos < 256 ? rand_pos : 256;
    printf("net: rand volcado (fase %u), tick %u, cuenta %u\n", phase, tick_local, psp_l32(0x08A7E8B4u));
    for (uint32_t i = 0; i < n; i++) {
        RandTick& r = rand_ring[(rand_pos - n + i) & 255];
        if (!r.n) continue;
        printf("net: rand tick %u cuenta %u %s:", r.tick, r.total, r.pair_v ? "impar" : "par");
        for (uint32_t j = 0; j < r.n; j++) printf(" 0x%08X x%u", r.ra[j], r.cnt[j]);
        printf("\n");
    }
}
