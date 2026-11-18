extern bool psp_watch_nodes;
void psp_watch_node(uint32_t upd, uint32_t node);
static bool battle_class(uint32_t upd) { return upd >= 0x08800000u && upd < 0x089C0000u; }
// games/ttt/recomp.txt [conv60] and [free_classes], emitted into generated game_data.cpp
extern const uint32_t GAME_CONV60[]; extern const int GAME_CONV60_N;
extern const char* const GAME_FREE_CLASSES;
static bool class60(uint32_t upd) {
    if (!patch_vfx_conv60.val) return false;
    for (int i = 0; i < GAME_CONV60_N; i++) if (GAME_CONV60[i] == upd) return true;
    return false;
}
static bool slots_on() { return patch_vfx_slots.val; }
// TTT_VFX_FREE=0xA-0xB,...: classes whose update falls in those ranges leave the gate; 0 disables the default list
static bool free_class(uint32_t upd) {
    static std::vector<std::pair<uint32_t, uint32_t>> r; static bool parsed = false;
    if (!parsed) {
        parsed = true;
        const char* e = getenv("TTT_VFX_FREE");
        for (const char* q = e ? e : GAME_FREE_CLASSES; q && *q;) {
            unsigned a = 0, b = 0;
            if (sscanf(q, "%x-%x", &a, &b) == 2) r.push_back({a, b});
            q = strchr(q, ','); if (q) q++;
        }
    }
    for (auto& x : r) if (upd >= x.first && upd <= x.second) return true;
    return false;
}
static bool skip_class(uint32_t upd) { return vfx_mode() != 60 && battle_class(upd) && !class60(upd) && !free_class(upd) && tick_odd; }
static bool half_active = false;
// TTT_HALF_EXCLUDE=0xA-0xB,...: stores in those ranges go back to the full step (to isolate sites)
static bool half_excluded(uint32_t pc) {
    static std::vector<std::pair<uint32_t, uint32_t>> r; static bool parsed = false;
    if (!parsed) {
        parsed = true;
        for (const char* q = getenv("TTT_HALF_EXCLUDE"); q && *q;) {
            unsigned a = 0, b = 0;
            if (sscanf(q, "%x-%x", &a, &b) == 2) r.push_back({a, b});
            q = strchr(q, ','); if (q) q++;
        }
    }
    for (auto& x : r) if (pc >= x.first && pc <= x.second) return true;
    return false;
}
uint32_t psp_half(uint32_t pc, uint32_t dir, uint32_t v) {
    if (!half_active || half_excluded(pc)) return v;
    uint32_t u = psp_l32(dir); float o, n; memcpy(&o, &u, 4); memcpy(&n, &v, 4);
    if (!std::isfinite(o) || !std::isfinite(n)) return v;
    float r = o + (n - o) * 0.5f; memcpy(&u, &r, 4);
    return u;
}

uint32_t psp_battle_tick() { return battle_tick; }

// animation events of each entity (+0xCC0, recomputed by 0x089B1A58 every 60 Hz tick): the 30 Hz tree
// sees in its tick those of the skipped tick plus the current ones, and after the walk the block is restored
// TTT_EVENTS=0 turns it off
static const uint32_t EV_BLOQUE = 0xCC0u, EV_TAM = 0x98u;
struct EvHeld { uint32_t gen = 0, seen = 0, sgen = 0, tick = 0; bool snap_taken = false, merged = false; uint8_t s[EV_TAM], o[EV_TAM]; };
static std::unordered_map<uint32_t, EvHeld> ev_ent;
static bool in_tree = false;
static bool events_on() { return patch_anim_events.val && vfx_mode() != 60; }
static void events_note(uint32_t ent) {
    if (in_tree || ent < 0x08800000u || ent >= 0x10000000u) return;
    EvHeld& r = ev_ent[ent]; r.gen++; r.tick = battle_tick;
}
static bool ev_group(const uint8_t* b, int n, uint32_t mlo, uint32_t mhi) {
    for (int i = 0; i < n; i++) { uint32_t lo, hi; memcpy(&lo, b + 40 + 8 * i, 4); memcpy(&hi, b + 44 + 8 * i, 4); if ((lo & mlo) | (hi & mhi)) return true; }
    return false;
}
static int ev_merge(uint8_t* b, const uint8_t* s, uint64_t* marks) {
    int n = (int8_t)b[30], m = (int8_t)s[30], added_n = 0;
    n = n < 0 ? 0 : n > 8 ? 8 : n; m = m < 0 ? 0 : m > 8 ? 8 : m;
    uint32_t mlo = psp_l32(0x08A6FEA0u), mhi = psp_l32(0x08A6FEA4u);
    bool group_b = ev_group(b, n, mlo, mhi), group_s = ev_group(s, m, mlo, mhi);
    for (int j = 0; j < m; j++) {
        uint64_t f; uint32_t p; memcpy(&f, s + 40 + 8 * j, 8); memcpy(&p, s + 104 + 4 * j, 4);
        int i = 0;
        while (i < n && memcmp(b + 40 + 8 * i, &f, 8)) i++;
        if (i < n) continue;
        if (n >= 8) break;
        memcpy(b + 40 + 8 * n, &f, 8); memcpy(b + 104 + 4 * n, &p, 4); n++; added_n++; *marks |= f;
    }
    b[30] = (uint8_t)n;
    if (!group_b && group_s) memcpy(b + 32, s + 32, 4);
    uint32_t a_lo = psp_l32(0x08A6FE80u), a_hi = psp_l32(0x08A6FE84u);
    for (int i = 0; i < n; i++) { uint32_t lo, hi; memcpy(&lo, b + 40 + 8 * i, 4); memcpy(&hi, b + 44 + 8 * i, 4); a_lo |= lo; a_hi |= hi; }
    memcpy(b + 136, &a_lo, 4); memcpy(b + 140, &a_hi, 4);
    return added_n;
}
static void ev_take() {
    for (auto it = ev_ent.begin(); it != ev_ent.end();) {
        EvHeld& r = it->second;
        if (battle_tick - r.tick > 8u) { it = ev_ent.erase(it); continue; }
        if (r.gen != r.seen) { memcpy(r.s, psp_p(it->first + EV_BLOQUE), EV_TAM); r.sgen = r.gen; r.snap_taken = true; }
        ++it;
    }
}
static bool in_update = false;
static void ev_apply() {
    for (auto& kv : ev_ent) {
        EvHeld& r = kv.second;
        r.merged = false;
        if (!r.snap_taken || r.sgen == r.gen) continue;
        uint8_t* b = psp_p(kv.first + EV_BLOQUE);
        memcpy(r.o, b, EV_TAM);
        uint64_t marks = 0;
        int n = ev_merge(b, r.s, &marks);
        r.merged = true;
    }
}
static void ev_remove() { for (auto& kv : ev_ent) if (kv.second.merged) { memcpy(psp_p(kv.first + EV_BLOQUE), kv.second.o, EV_TAM); kv.second.merged = false; } }
// one-tick combatant flags (+0xDD8..+0x111C: two 0x1A2 tables, e.g. +0xE92/+0x1034 rush camera,
// power clash prompts): gated classes also see those of the skipped tick. TTT_FLAGS=0 keeps only the two old ones
static const uint32_t BAND_INI = 0xDD8u, BAND_TAM = 0x344u;
static uint8_t band_val[4][0x344], band_orig[4][0x344];
static bool band_applied = false;
static uint32_t combatant(int i) {
    uint32_t t = psp_l32(0x08A7DBC8u); if (!t) return 0;
    uint32_t n = psp_l32(t), b = psp_l32(t + 4);
    return (uint32_t)i < n && b ? b + i * 0x1A90u : 0;
}
static bool band_all() { return patch_tick_flags.val; }
static bool band_used(uint32_t k) { return band_all() || k == 0xE92u - BAND_INI || k == 0x1034u - BAND_INI; }
static void band_take() { for (int i = 0; i < 4; i++) { uint32_t cb = combatant(i); if (cb) memcpy(band_val[i], psp_p(cb + BAND_INI), BAND_TAM); else memset(band_val[i], 0, BAND_TAM); } }
static void band_apply() {
    if (band_applied) return;
    for (int i = 0; i < 4; i++) {
        uint32_t cb = combatant(i); if (!cb) continue;
        uint8_t* b = psp_p(cb + BAND_INI);
        memcpy(band_orig[i], b, BAND_TAM);
        for (uint32_t k = 0; k < BAND_TAM; k++) if (!b[k] && band_val[i][k] && band_used(k)) b[k] = band_val[i][k];
    }
    band_applied = true;
}
static void band_remove() {
    if (!band_applied) return;
    for (int i = 0; i < 4; i++) {
        uint32_t cb = combatant(i); if (!cb) continue;
        uint8_t* b = psp_p(cb + BAND_INI);
        for (uint32_t k = 0; k < BAND_TAM; k++) if (!band_orig[i][k] && band_val[i][k] && band_used(k) && b[k] == band_val[i][k]) b[k] = 0;
    }
    band_applied = false;
}
// retention applies only while a gated class runs on an even tick
static bool ev_modo = false, blended = false;
static void blend_events(bool want) {
    if (!ev_modo || tick_odd || want == blended) return;
    if (want) { ev_apply(); band_apply(); } else { ev_remove(); band_remove(); }
    blended = want;
}
static void ev_close() { for (auto& kv : ev_ent) { kv.second.snap_taken = false; kv.second.seen = kv.second.gen; } }

static void walk_update(Ctx& c, uint32_t root) {
    uint32_t o = psp_l32(root + 4);
    while (o) {
        uint32_t cls = psp_l32(o + 0x28), upd = psp_l32(cls);
        if (psp_watch_nodes) psp_watch_node(upd, o);
        if (upd && !skip_class(upd)) {
            bool c60 = class60(upd);
            blend_events(!c60);
            psp_s32(0x08A7D7D4u, o);
            c.r[4] = o;
            half_active = c60;
            psp_call(c, upd);
            half_active = false;
            if (psp_l32(psp_l32(o + 0x28) + 0x14)) psp_s8(o, psp_l8(o) | 2);
        }
        uint32_t sig = psp_l32(o + 0x30);
        if (psp_l8(o) & 1) { c.r[4] = o; psp_call(c, 0x08901D5Cu); o = sig; continue; }
        uint32_t h = psp_l32(o + 0x24);
        if (h) walk_update(c, h);
        o = sig;
    }
}

static void walk_post(Ctx& c, uint32_t root) {
    uint32_t o = psp_l32(root + 4);
    while (o) {
        uint32_t cls = psp_l32(o + 0x28), fn = psp_l32(cls + 0xC);
        uint32_t upd = psp_l32(cls);
        if (fn && !skip_class(upd)) {
            bool c60 = class60(upd);
            blend_events(!c60);
            c.r[4] = o;
            half_active = c60;
            psp_call(c, fn);
            half_active = false;
        }
        uint32_t sig = psp_l32(o + 0x30);
        if (!(psp_l8(o) & 1)) { uint32_t h = psp_l32(o + 0x24); if (h) walk_post(c, h); }
        o = sig;
    }
}

static void dc_battle_frame(const Ctx& c);
static void tasks(Ctx& c, uint32_t dir) {
    uint32_t sp = c.r[29], ra = c.r[31], root = c.r[4];
    c.r[29] = sp - 0x10;
    bool ev = events_on();
    ev_modo = ev;
    if (dir == 0x08901E10u) {
        nt_sample(root);
        if (psp_l32(0x08A78E14u) - battle_frame > 90u) { rand_n = 0; rand_pos = 0; tick_local = 0; }
        else rand_close();
        tick_local++;
        tick_odd = !tick_odd;
        {   // in the WLAN session (phases 13/14) parity comes from the session frame, identical in every instance
            static uint32_t last_v = 0xFFFFFFFFu;
            uint32_t o = psp_l32(0x08A78A28u), f = o ? psp_l32(o + 0x2950) : 0;
            if (o && (f == 13 || f == 14)) { uint32_t q = psp_l32(o + 0x10); if (q != last_v) tick_odd = q & 1; last_v = q; }
        }
        battle_tick++;
        battle_frame = psp_l32(0x08A78E14u);
        st_tick();
        dc_battle_frame(c);
        if (ev && tick_odd) { ev_take(); band_take(); }
        in_update = true;
        in_tree = true;
        walk_update(c, root);
        in_tree = false;
        in_update = false;
        blend_events(false);
    } else {
        in_tree = true;
        walk_post(c, root);
        in_tree = false;
        blend_events(false);
        if (ev && !tick_odd) ev_close();
    }
    c.r[29] = sp; c.r[31] = ra;
}

