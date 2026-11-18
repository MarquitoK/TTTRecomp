// TTT_STEP_RULES=1 (games/ttt/fps60_sites.txt) or =<file>: 60 fps store rules of BT3-Recomp, applied only while the
// battle runs at 60. f float accumulator by half, u/d counter every other tick, j integer step by half.
// TTT_STEP_RULES_ONLY=<kinds> keeps only those kinds; TTT_STEP_RULES_FN=0xFUNC,0xLO-0xHI keeps only rules inside them.
// TTT_STEP_PROBE=1 (step_probe.csv) or =<file>: instead of correcting, perturbs one store in 32 per slot and judges
// from the next store whether the site accumulates its own value or recomputes it
static const uint32_t SR_LO = 0x08804040u, SR_HI = 0x08A66B0Cu;
struct SrSlot { uint32_t last_v, last_tick, want, delta; uint8_t phase, carry, probe, pending; };
struct SrStat { uint32_t stores, same, ptr, probes, acc, rec, other; };
static uint8_t* sr_kind = nullptr;
static std::unordered_map<uint64_t, SrSlot> sr_slots;
static std::unordered_map<uint32_t, SrStat> sr_stats;
static bool sr_on = false, sr_probe = false;
static char sr_probe_path[512];
static uint32_t sr_applied = 0, sr_last_tick = 0, sr_saved_tick = 0;
static void sr_dump(bool quiet = false);

static uint32_t sr_owner(uint32_t pc) {
    for (int b = 0; b < BANKS_N; b++) {
        if (BANKS[b].indice != -1) continue;
        const Entry* t = BANKS[b].tab; int lo = 0, hi = BANKS[b].n;
        while (lo < hi) { int m = (lo + hi) / 2; if (t[m].dir <= pc) lo = m + 1; else hi = m; }
        return lo ? t[lo - 1].dir : 0;
    }
    return 0;
}

static bool sr_fn_match(const char* list, uint32_t pc, uint32_t fn) {
    for (const char* p = list; *p;) {
        char* e; uint32_t lo = (uint32_t)strtoul(p, &e, 16);
        if (e == p) { p++; continue; }
        if (*e == '-') { uint32_t hi = (uint32_t)strtoul(e + 1, &e, 16); if (pc >= lo && pc <= hi) return true; }
        else if (fn == lo) return true;
        p = e;
    }
    return false;
}

static void sr_init() {
    const char* e = getenv("TTT_STEP_RULES");
    if (!e || !*e || !strcmp(e, "0")) return;
    std::string path = strcmp(e, "1") ? e : "../../games/ttt/fps60_sites.txt";
    FILE* f = fopen(path.c_str(), "r");
    if (!f) { printf("steprules: cannot read %s\n", path.c_str()); return; }
    const char* only = getenv("TTT_STEP_RULES_ONLY");
    const char* fns = getenv("TTT_STEP_RULES_FN");
    if (fns && !*fns) fns = nullptr;
    sr_kind = (uint8_t*)calloc((SR_HI - SR_LO) / 4, 1);
    char l[256]; int n = 0;
    while (fgets(l, sizeof(l), f)) {
        unsigned pc; char k;
        if (sscanf(l, " 0x%x %c", &pc, &k) != 2 || pc < SR_LO || pc >= SR_HI || !strchr("fudj", k)) continue;
        if (only && *only && !strchr(only, k)) continue;
        if (fns) {
            const char* c = strstr(l, "fn 0x"); unsigned fn = 0;
            if (!c || sscanf(c, "fn 0x%x", &fn) != 1) fn = sr_owner(pc);
            if (!sr_fn_match(fns, pc, fn)) continue;
        }
        sr_kind[(pc - SR_LO) >> 2] = (uint8_t)k;
        n++;
    }
    fclose(f);
    sr_on = n > 0;
    sr_slots.reserve(1 << 16);
    const char* p = getenv("TTT_STEP_PROBE");
    if (sr_on && p && *p && strcmp(p, "0")) { sr_probe = true; snprintf(sr_probe_path, sizeof(sr_probe_path), "%s", strcmp(p, "1") ? p : "step_probe.csv"); }
    printf("steprules: %d rules from %s%s%s%s%s%s\n", n, path.c_str(), only && *only ? ", kinds " : "", only && *only ? only : "",
           fns ? ", functions " : "", fns ? fns : "", sr_probe ? ", probe mode" : "");
}

static float sr_wrap(float d) { if (d > 3.14159265f) d -= 6.2831853f; else if (d < -3.14159265f) d += 6.2831853f; return d; }
static float sr_f(uint32_t b) { float x; memcpy(&x, &b, 4); return x; }
static uint32_t sr_b(float x) { uint32_t b; memcpy(&b, &x, 4); return b; }
static bool sr_ptr(uint32_t v) { return v - 0x08800000u < 0x01800000u || v - 0x04000000u < 0x00200000u; }
static int32_t sr_sext(int size, uint32_t d) { return size == 4 ? (int32_t)d : size == 2 ? (int16_t)(uint16_t)d : (int8_t)(uint8_t)d; }

// the rule's correction of one chained store that changes the value (returns nv when the rule does not apply)
static uint32_t sr_correct(uint8_t k, int size, uint32_t old, uint32_t nv, SrSlot& s) {
    uint32_t mask = size == 4 ? 0xFFFFFFFFu : size == 2 ? 0xFFFFu : 0xFFu;
    if (k == 'f' && size == 4) {
        float o = sr_f(old), n = sr_f(nv);
        if (!std::isfinite(o) || !std::isfinite(n) || nv == 0u) return nv;
        float d = n - o;
        sr_applied++;
        if (std::fabs(d) > 3.14159265f && std::fabs(o) <= 3.3f && std::fabs(n) <= 3.3f) return sr_b(sr_wrap(o + sr_wrap(d) * 0.5f));
        return sr_b(o + d * 0.5f);
    }
    if ((k == 'u' && ((nv - old) & mask) == 1u) || (k == 'd' && ((old - nv) & mask) == 1u)) {
        s.phase ^= 1;
        if (s.phase) { sr_applied++; return old; }
        return nv;
    }
    if (k == 'j' && nv != 0u) {
        int32_t d = sr_sext(size, nv - old), h = d / 2;
        if (d & 1) { s.carry ^= 1; if (s.carry) h += d > 0 ? 1 : -1; }
        sr_applied++;
        return old + (uint32_t)h;
    }
    return nv;
}

// accumulator: the next value continues from what was written; recomputed: it follows the game's own trajectory
static uint32_t sr_probe_store(uint8_t k, int size, uint32_t pc, uint32_t old, uint32_t nv, SrSlot& s, bool chain, bool same) {
    SrStat& t = sr_stats[pc];
    t.stores++;
    if (same) t.same++;
    if (sr_ptr(nv)) t.ptr++;
    uint32_t out = nv;
    if (s.pending) {
        s.pending = 0;
        if (chain && !same) {
            double ea, er, dd;
            if (k == 'f' && size == 4) {
                float w1 = sr_f(nv), o0 = sr_f(s.last_v), w0 = sr_f(s.want), d0 = sr_f(s.delta);
                bool ang = std::fabs(w1) <= 3.3f && std::fabs(o0) <= 3.3f && std::fabs(w0) <= 3.3f;
                ea = std::fabs(ang ? sr_wrap(sr_wrap(w1 - o0) - d0) : w1 - o0 - d0);
                er = std::fabs(ang ? sr_wrap(sr_wrap(w1 - w0) - d0) : w1 - w0 - d0);
                dd = std::fabs(d0);
            } else {
                int32_t d0 = sr_sext(size, s.delta);
                ea = std::fabs((double)(sr_sext(size, nv - s.last_v) - d0));
                er = std::fabs((double)(sr_sext(size, nv - s.want) - d0));
                dd = std::fabs((double)d0);
            }
            if (dd > 0 && ea * 3 < er) {
                t.acc++;
                if (k == 'f' && size == 4) out = sr_b(sr_f(nv) + (sr_f(s.want) - sr_f(s.last_v)));
                else if (k == 'j') out = nv + (s.want - s.last_v);
            } else if (dd > 0 && er * 3 < ea) t.rec++;
            else t.other++;
        }
    } else if (chain && !same && nv != old && !sr_ptr(nv) && !sr_ptr(old) && t.same * 10 <= t.stores && ++s.probe >= 32) {
        s.phase = 0;
        uint32_t c = sr_correct(k, size, old, nv, s);
        if (c != nv) {
            out = c;
            s.probe = 0; s.pending = 1; s.want = nv;
            s.delta = k == 'f' && size == 4 ? sr_b(sr_f(nv) - sr_f(old)) : nv - old;
            t.probes++;
        }
    }
    return out;
}

// continuing its own value from the previous tick or this one is an accumulator step; anything else (and a reset to 0,
// or a counter jump that is not its step) is an assignment and passes through
static uint32_t sr_apply(int size, uint32_t pc, uint32_t a, uint32_t v) {
    uint8_t k = sr_kind[(pc - SR_LO) >> 2];
    if (!k) return v;
    if (battle_tick - sr_last_tick > 8u || sr_slots.size() > (1u << 20)) sr_slots.clear();
    sr_last_tick = battle_tick;
    uint32_t old = size == 4 ? psp_l32(a) : size == 2 ? psp_l16(a) : psp_l8(a);
    uint32_t nv = size == 4 ? v : size == 2 ? (v & 0xFFFFu) : (v & 0xFFu);
    SrSlot& s = sr_slots[((uint64_t)pc << 32) | a];
    bool chain = s.last_tick && battle_tick - s.last_tick <= 2u && s.last_v == old;
    uint32_t out = nv;
    if (sr_probe && battle_tick - sr_saved_tick >= 1800u) { sr_saved_tick = battle_tick; sr_dump(true); }
    if (sr_probe) out = sr_probe_store(k, size, pc, old, nv, s, chain, s.last_tick == battle_tick);
    else if (chain && nv != old) out = sr_correct(k, size, old, nv, s);
    if (size == 2) out &= 0xFFFFu; else if (size == 1) out &= 0xFFu;
    s.last_v = out;
    s.last_tick = battle_tick;
    return out;
}

static void sr_dump(bool quiet) {
    if (!sr_on) return;
    if (!sr_probe) { printf("steprules: %u stores corrected\n", sr_applied); return; }
    FILE* f = fopen(sr_probe_path, "w");
    if (!f) { if (!quiet) printf("stepprobe: cannot write %s\n", sr_probe_path); return; }
    fprintf(f, "pc,kind,stores,same_tick,pointer,probes,accumulates,recomputes,other\n");
    uint32_t n = 0, na = 0, nr = 0;
    for (auto& it : sr_stats) {
        const SrStat& t = it.second;
        fprintf(f, "0x%08X,%c,%u,%u,%u,%u,%u,%u,%u\n", it.first, sr_kind[(it.first - SR_LO) >> 2], t.stores, t.same, t.ptr, t.probes, t.acc, t.rec, t.other);
        n++; if (t.acc > t.rec && t.acc >= 3) na++; else if (t.rec > t.acc && t.rec >= 3) nr++;
    }
    fclose(f);
    if (!quiet) printf("stepprobe: %u sites seen, %u accumulate, %u recompute, %u undecided -> %s\n", n, na, nr, n - na - nr, sr_probe_path);
}
