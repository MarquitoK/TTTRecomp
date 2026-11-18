// TTT_STEPCENSUS=1 (step_census.csv) or =<file>: classifies every main-image store site during battle ticks by how it
// advances memory from tick to tick (method of BT3-Recomp ps2_stepcensus); play at 30 fps while it runs
bool psp_st_on = false;
bool psp_st_mark = false;
// guest thread stacks: stores into them through a register copied from $sp are temporaries, not state
static uint32_t st_stack_lo[64], st_stack_hi[64];
static int st_stacks = 0;
void psp_stack_note(uint32_t lo, uint32_t hi) { if (st_stacks < 64) { st_stack_lo[st_stacks] = lo; st_stack_hi[st_stacks++] = hi; } }
static bool st_in_stack(uint32_t a) { for (int i = 0; i < st_stacks; i++) if (a - st_stack_lo[i] < st_stack_hi[i] - st_stack_lo[i]) return true; return false; }
static const uint32_t ST_LO = 0x08804040u, ST_HI = 0x08A66B0Cu;
struct StSite {
    uint32_t count, frames, last_frame, per_cur, per_max, chain, zero, fplaus, addr_lo, addr_hi;
    uint32_t mcount, mzero, mframes, mlast, mchain;
    double fabs_sum, iabs_sum, mabs_sum;
    float fd[4]; uint32_t fn[4]; int32_t id[4]; uint32_t in[4];
    uint32_t ring_a[4], ring_v[4];
    uint8_t ring_pos, size;
};
static StSite* st_sites = nullptr;
static int st_mode = -1;
static char st_path[512];
static uint32_t st_ticks = 0, st_ticks60 = 0, st_mticks = 0;

static void st_init() {
    st_mode = 0;
    sr_init();
    const char* e = getenv("TTT_STEPCENSUS");
    if (!e || !*e || !strcmp(e, "0")) return;
    snprintf(st_path, sizeof(st_path), "%s", strcmp(e, "1") ? e : "step_census.csv");
    st_sites = (StSite*)calloc((ST_HI - ST_LO) / 4, sizeof(StSite));
    if (!st_sites) return;
    st_mode = 1;
    printf("stepcensus: recording store sites into %s\n", st_path);
}


// TTT_AURA_WATCH=1: aura particle pool [[0x08A7C790]] (nodes at [M+0xC], stride 0xD0, next +0xC0, active list M+0x14,
// free list M+0x10): every guest store to links/headers is recorded; lists validated every tick and around every wrapper
static int aw_on = -1;
static bool aw_done = false;
static uint32_t aw_m = 0, aw_b = 0, aw_cap = 0, aw_ctx = 0;
static uint32_t* aw_shadow = nullptr;
static struct AwRec { uint32_t tick, pc, a, o, v, ctx; } aw_ring[16384];
static uint32_t aw_rn = 0;
static int aw_foreign = 0;
static bool aw_pend = false;
static void aw_validate(const char* where, uint32_t dir);
static void aw_init() {
    const char* e = getenv("TTT_AURA_WATCH");
    aw_on = e && atoi(e) > 0;
    if (aw_on) printf("aura: vigilancia de la lista de particulas activa\n");
}
static bool aw_link(uint32_t a, uint32_t* idx) {
    if (a == aw_m + 0x10 || a == aw_m + 0x14) { *idx = aw_cap + (a == aw_m + 0x14); return true; }
    if (a < aw_b || a >= aw_b + aw_cap * 0xD0u || (a - aw_b) % 0xD0u != 0xC0u) return false;
    *idx = (a - aw_b) / 0xD0u; return true;
}
static void aw_bind() {
    uint32_t m = psp_l32(0x08A7C790u);
    if (m == aw_m && (!m || psp_l32(m + 0xC) == aw_b)) return;
    aw_m = m; aw_b = m ? psp_l32(m + 0xC) : 0; aw_cap = m ? psp_l32(m + 4) : 0;
    if (aw_cap > 8192) aw_cap = 0;
    free(aw_shadow); aw_shadow = aw_cap ? (uint32_t*)calloc(aw_cap + 2, 4) : nullptr;
    for (uint32_t i = 0; aw_shadow && i < aw_cap; i++) aw_shadow[i] = psp_l32(aw_b + i * 0xD0u + 0xC0);
    if (aw_shadow) { aw_shadow[aw_cap] = psp_l32(m + 0x10); aw_shadow[aw_cap + 1] = psp_l32(m + 0x14); }
    printf("aura: gestor 0x%08X, nodos 0x%08X, capacidad %u\n", aw_m, aw_b, aw_cap);
}
static void aw_store(int size, uint32_t pc, uint32_t a, uint32_t v) {
    if (aw_done || !aw_m) return;
    if (aw_pend) { aw_pend = false; aw_validate("escritura", pc); if (aw_done) return; }
    if (size != 4) return;
    uint32_t idx;
    bool hdr = a >= aw_m && a < aw_m + 0x30;
    bool lk = aw_shadow && aw_link(a, &idx);
    bool node = a >= aw_b && a < aw_b + aw_cap * 0xD0u && (a - aw_b) % 0xD0u < 0x0Cu;
    if (!hdr && !lk && !node) return;
    aw_ring[aw_rn++ & 16383] = {battle_tick, pc, a, psp_l32(a), v, aw_ctx};
    if (lk) { aw_shadow[idx] = v; aw_pend = true; }
    if ((pc < 0x088B2000u || pc >= 0x088B8000u) && aw_foreign < 30) { aw_foreign++; printf("aura: escritura ajena pc 0x%08X en 0x%08X = 0x%08X (antes 0x%08X), envoltura 0x%08X, tick %u\n", pc, a, v, psp_l32(a), aw_ctx, battle_tick); }
}
static void aw_dump(uint32_t a1, uint32_t node) {
    uint32_t a2 = node + 0xC0;
    printf("aura: historial de 0x%08X y del nodo 0x%08X:\n", a1, node);
    int n = 0;
    for (uint32_t i = aw_rn; i-- > (aw_rn > 16384 ? aw_rn - 16384 : 0) && n < 40;) {
        const AwRec& r = aw_ring[i & 16383];
        if (r.a != a1 && r.a != a2 && r.v != node) continue;
        n++; printf("aura:   tick %u pc 0x%08X [0x%08X] 0x%08X -> 0x%08X envoltura 0x%08X\n", r.tick, r.pc, r.a, r.o, r.v, r.ctx);
    }
    printf("aura: ultimas escrituras:\n");
    for (uint32_t i = aw_rn, k = 0; i-- > 0 && k < 60; k++) {
        const AwRec& r = aw_ring[i & 16383];
        printf("aura:   tick %u pc 0x%08X [0x%08X] 0x%08X -> 0x%08X envoltura 0x%08X\n", r.tick, r.pc, r.a, r.o, r.v, r.ctx);
    }
}
static void aw_check(const char* where, uint32_t dir) {
    if (aw_on < 0) aw_init();
    if (!aw_on || aw_done) return;
    aw_bind();
    if (!aw_m || !aw_shadow) return;
    for (uint32_t i = 0; i < aw_cap + 2; i++) {
        uint32_t a = i < aw_cap ? aw_b + i * 0xD0u + 0xC0 : aw_m + (i == aw_cap ? 0x10 : 0x14), v = psp_l32(a);
        if (v == aw_shadow[i]) continue;
        printf("aura: cambio no vigilado en 0x%08X: 0x%08X -> 0x%08X, %s 0x%08X, tick %u\n", a, aw_shadow[i], v, where, dir, battle_tick);
        aw_shadow[i] = v;
    }
    aw_validate(where, dir);
}
static void aw_validate(const char* where, uint32_t dir) {
    if (!aw_m || !aw_shadow || aw_done) return;
    static uint8_t seen[8192];
    memset(seen, 0, aw_cap);
    for (int l = 0; l < 2; l++) {
        uint32_t prev = aw_m + (l ? 0x14 : 0x10), p = psp_l32(prev);
        for (uint32_t steps = 0; p; steps++) {
            const char* bad = nullptr;
            if (p < aw_b || p >= aw_b + aw_cap * 0xD0u || (p - aw_b) % 0xD0u) bad = "nodo fuera del grupo";
            else if (seen[(p - aw_b) / 0xD0u]) bad = "nodo repetido: ciclo o en ambas listas";
            if (bad) {
                aw_done = true;
                printf("aura: LISTA %s ROTA (%s): enlace 0x%08X -> 0x%08X tras %u nodos, %s 0x%08X, tick %u\n", l ? "ACTIVA" : "LIBRE", bad, prev, p, steps, where, dir, battle_tick);
                aw_dump(prev, p);
                return;
            }
            seen[(p - aw_b) / 0xD0u] = 1;
            prev = p + 0xC0; p = psp_l32(prev);
        }
    }
}
static bool aw_busy(uint32_t d) { return d != 0x089C044Cu && d != 0x0885862Cu && d != 0x08A642D4u; }
struct AwScope {
    uint32_t keep, dir;
    AwScope(uint32_t d) : keep(aw_ctx), dir(d) { if (aw_on > 0 && aw_busy(d)) { aw_check("entrada", d); aw_ctx = d; } }
    ~AwScope() { if (aw_on > 0 && aw_busy(dir)) { aw_ctx = keep; aw_check("salida", dir); } }
};

static void st_tick() {
    if (st_mode < 0) st_init();
    aw_check("tick", 0);
    if (aw_on > 0) psp_st_on = true;
    if (sr_on && battle60()) psp_st_on = true;
    if (st_mode != 1) return;
    st_ticks++;
    if (battle60()) st_ticks60++;
    if (psp_st_mark) st_mticks++;
    psp_st_on = true;
}

// at 30 fps the battle walker runs unwrapped: keep the tick and the battle gate the census needs
// (the unwrapped walker recurses into every subtree: a new tick is a new vblank count)
static void st_walk30() {
    if (st_mode < 0) st_init();
    if (aw_on < 0) aw_init();
    if (st_mode != 1 && aw_on <= 0) return;
    uint32_t vb = psp_l32(0x08A78E14u);
    if (vb == battle_frame) return;
    battle_tick++;
    battle_frame = vb;
    st_tick();
}

static bool st_near(float a, float b) { return std::fabs(a - b) <= 1e-5f * std::max(1.0f, std::fabs(a)); }
static bool st_plausible(uint32_t bits, float& f) {
    memcpy(&f, &bits, 4);
    if (!std::isfinite(f)) return false;
    float a = std::fabs(f);
    return bits == 0u || (a >= 1e-6f && a <= 1e6f);
}

uint32_t psp_st_slow(int size, uint32_t pc, uint32_t a, uint32_t v) {
    if (!battle_active()) { psp_st_on = false; return v; }
    if (pc < ST_LO || pc >= ST_HI) return v;
    if (aw_on > 0) aw_store(size, pc, a, v);
    if (sr_on && battle60()) v = sr_apply(size, pc, a, v);
    if (st_mode != 1 || st_in_stack(a)) return v;
    StSite& s = st_sites[(pc - ST_LO) >> 2];
    uint32_t old = size == 4 ? psp_l32(a) : size == 2 ? psp_l16(a) : psp_l8(a);
    uint32_t nv = size == 4 ? v : size == 2 ? (v & 0xFFFFu) : (v & 0xFFu);
    s.size |= (uint8_t)size;
    s.count++;
    if (s.last_frame != battle_tick || s.frames == 0) { s.frames++; s.last_frame = battle_tick; s.per_cur = 0; }
    if (++s.per_cur > s.per_max) s.per_max = s.per_cur;
    if (s.count == 1) { s.addr_lo = s.addr_hi = a; }
    else { if (a < s.addr_lo) s.addr_lo = a; if (a > s.addr_hi) s.addr_hi = a; }
    int slot = -1; bool chained = false;
    for (int i = 0; i < 4; i++) if (s.ring_a[i] == a && s.count > 1) { slot = i; if (s.ring_v[i] == old) { s.chain++; chained = true; } break; }
    if (slot < 0) { slot = s.ring_pos; s.ring_pos = (uint8_t)((s.ring_pos + 1) & 3); s.ring_a[slot] = a; }
    s.ring_v[slot] = nv;
    if (psp_st_mark) {
        s.mcount++;
        if (s.mlast != battle_tick || s.mframes == 0) { s.mframes++; s.mlast = battle_tick; }
        if (nv == old) s.mzero++;
        else if (chained) {
            s.mchain++;
            float mo, mn;
            s.mabs_sum += size == 4 && st_plausible(old, mo) && st_plausible(nv, mn) ? std::fabs((double)mn - mo)
                        : std::fabs((double)(size == 4 ? (int32_t)(nv - old) : size == 2 ? (int16_t)(uint16_t)(nv - old) : (int8_t)(uint8_t)(nv - old)));
        }
    }
    if (nv == old) { s.zero++; return v; }
    float fo, fn;
    if (size == 4 && st_plausible(old, fo) && st_plausible(nv, fn)) {
        s.fplaus++;
        float d = fn - fo;
        s.fabs_sum += std::fabs((double)d);
        int k = -1, lo = 0;
        for (int i = 0; i < 4; i++) { if (s.fn[i] && st_near(d, s.fd[i])) { k = i; break; } if (s.fn[i] < s.fn[lo]) lo = i; }
        if (k < 0) { k = lo; s.fd[k] = d; s.fn[k] = 0; }
        s.fn[k]++;
    }
    int32_t di = size == 4 ? (int32_t)(nv - old) : size == 2 ? (int32_t)(int16_t)(uint16_t)(nv - old) : (int32_t)(int8_t)(uint8_t)(nv - old);
    s.iabs_sum += std::fabs((double)di);
    int k = -1, lo = 0;
    for (int i = 0; i < 4; i++) { if (s.in[i] && s.id[i] == di) { k = i; break; } if (s.in[i] < s.in[lo]) lo = i; }
    if (k < 0) { k = lo; s.id[k] = di; s.in[k] = 0; }
    s.in[k]++;
    return v;
}

void psp_step_census_dump() {
    if (st_mode < 0) st_init();
    sr_dump();
    if (st_mode != 1) return;
    FILE* f = fopen(st_path, "w");
    if (!f) { printf("stepcensus: cannot write %s\n", st_path); return; }
    fprintf(f, "#ticks,%u,%d,%u\npc,size,count,frames,per_frame_max,chain,zero,fplaus,fmode,fmode_n,fother,imode,imode_n,iother,addr_lo,addr_hi,fabs_mean,iabs_mean,mcount,mzero,mframes,mabs_mean,mchain\n", st_ticks, 2 * st_ticks60 > st_ticks ? 60 : 30, st_mticks);
    uint32_t n = 0;
    for (uint32_t i = 0; i < (ST_HI - ST_LO) / 4; i++) {
        const StSite& s = st_sites[i];
        if (s.frames < 2) continue;
        int bf = 0, bi = 0;
        for (int k = 1; k < 4; k++) { if (s.fn[k] > s.fn[bf]) bf = k; if (s.in[k] > s.in[bi]) bi = k; }
        uint32_t moved = s.count - s.zero;
        fprintf(f, "0x%08X,%u,%u,%u,%u,%u,%u,%u,%g,%u,%u,%d,%u,%u,0x%08X,0x%08X,%g,%g,%u,%u,%u,%g,%u\n", ST_LO + 4 * i, s.size, s.count, s.frames, s.per_max,
                s.chain, s.zero, s.fplaus, s.fd[bf], s.fn[bf], s.fplaus - s.fn[bf], s.id[bi], s.in[bi], moved - s.in[bi], s.addr_lo, s.addr_hi,
                s.fplaus ? s.fabs_sum / s.fplaus : 0.0, moved ? s.iabs_sum / moved : 0.0,
                s.mcount, s.mzero, s.mframes, s.mchain ? s.mabs_sum / s.mchain : 0.0, s.mchain);
        n++;
    }
    fclose(f);
    printf("stepcensus: %u battle ticks at %d fps (%u marked), %u store sites -> %s\n", st_ticks, 2 * st_ticks60 > st_ticks ? 60 : 30, st_mticks, n, st_path);
}

// TTT_NODE_TRACE=<file>: while marked (Ctrl+F7) one row per battle task node and per combatant each tick (time in vblanks)
// TTT_NODE_OFFS=0x4,0x10,...: up to 8 offsets read from each node's data block [node+0x38]
static FILE* nt_f = nullptr;
static int nt_mode = -1;
static uint32_t nt_last_vb = 0xFFFFFFFFu, nt_mark = 0, nt_rows = 0;
static bool nt_was = false;
static int nt_depth = 0, nt_nroots = 0;
static uint32_t nt_roots[8];
static uint32_t nt_off[8] = {0x4, 0x10, 0x14, 0x18, 0x60, 0x64, 0x68, 0x80};

static void nt_init() {
    nt_mode = 0;
    const char* e = getenv("TTT_NODE_TRACE");
    if (!e || !*e || !strcmp(e, "0")) return;
    char path[512]; snprintf(path, sizeof(path), "%s", strcmp(e, "1") ? e : "node_trace.csv");
    int n = 0;
    for (const char* q = getenv("TTT_NODE_OFFS"); q && *q && n < 8;) {
        unsigned v = 0; if (sscanf(q, "%x", &v) == 1) nt_off[n++] = v;
        q = strchr(q, ','); if (q) q++;
    }
    nt_f = fopen(path, "w");
    if (!nt_f) { printf("nodetrace: no se pudo abrir %s\n", path); return; }
    setvbuf(nt_f, nullptr, _IOFBF, 1 << 20);
    fprintf(nt_f, "#offs");
    for (int i = 0; i < 8; i++) fprintf(nt_f, ",%X", nt_off[i]);
    fprintf(nt_f, "\nvb,fps,mark,kind,upd,node,data,w0,w1,w2,w3,w4,w5,w6,w7\n");
    nt_mode = 1;
    printf("nodetrace: grabando en %s\n", path);
}

static bool nt_ram(uint32_t a) { return a >= 0x08800000u && a < 0x0BFFFF00u; }

static void nt_walk(uint32_t o, uint32_t vb, int fps, int depth, int& budget) {
    while (o && nt_ram(o) && budget-- > 0) {
        uint32_t cls = psp_l32(o + 0x28), upd = nt_ram(cls) ? psp_l32(cls) : 0;
        if (!(psp_l8(o) & 1) && upd >= 0x08800000u && upd < 0x089C0000u) {
            uint32_t d = psp_l32(o + 0x38), w[8] = {};
            if (nt_ram(d)) for (int i = 0; i < 8; i++) w[i] = psp_l32(d + nt_off[i]);
            fprintf(nt_f, "%u,%d,%u,N,%08X,%08X,%08X", vb, fps, nt_mark, upd, o, d);
            for (int i = 0; i < 8; i++) fprintf(nt_f, ",%08X", w[i]);
            fputc('\n', nt_f); nt_rows++;
        }
        uint32_t h = psp_l32(o + 0x24);
        if (h && nt_ram(h) && depth < 32 && !(psp_l8(o) & 1)) nt_walk(psp_l32(h + 4), vb, fps, depth + 1, budget);
        o = psp_l32(o + 0x30);
    }
}

static void nt_sample(uint32_t root) {
    if (nt_mode < 0) nt_init();
    if (nt_mode != 1) return;
    if (psp_st_mark != nt_was) {
        nt_was = psp_st_mark;
        if (nt_was) nt_mark++;
        else { fflush(nt_f); printf("nodetrace: ventana %u cerrada, %u filas en total\n", nt_mark, nt_rows); }
    }
    if (!psp_st_mark) return;
    if (nt_depth > 0) return;
    uint32_t vb = psp_l32(0x08A78E14u);
    if (vb != nt_last_vb) { nt_last_vb = vb; nt_nroots = 0; }
    for (int i = 0; i < nt_nroots; i++) if (nt_roots[i] == root) return;
    if (nt_nroots >= 8) return;
    nt_roots[nt_nroots++] = root;
    bool first = nt_nroots == 1;
    int fps = battle60() ? 60 : 30, budget = 4096;
    nt_walk(psp_l32(root + 4), vb, fps, 0, budget);
    uint32_t t = psp_l32(0x08A7DBC8u);
    if (!first || !nt_ram(t)) return;
    uint32_t cnt = psp_l32(t), base = psp_l32(t + 4);
    for (uint32_t i = 0; i < cnt && i < 4 && nt_ram(base); i++) {
        uint32_t cb = base + i * 0x1A90u, slot = psp_l32(cb + 0x2C), ent = slot < 4 ? psp_l32(0x08B61178u + slot * 4) : 0;
        uint32_t w[8] = {};
        if (nt_ram(ent)) {
            w[0] = psp_l32(ent + 0x950); w[1] = psp_l32(ent + 0x954); w[2] = psp_l32(ent + 0x958);
            w[4] = psp_l32(ent + 0xB60 + 0x138);
        }
        w[3] = psp_l32(cb + 0x984); w[5] = psp_l32(cb + 0x780); w[6] = psp_l32(cb + 0x770); w[7] = psp_l32(cb + 0x778);
        fprintf(nt_f, "%u,%d,%u,C,%u,%08X,%08X", vb, fps, nt_mark, i, cb, ent);
        for (int k = 0; k < 8; k++) fprintf(nt_f, ",%08X", w[k]);
        fputc('\n', nt_f); nt_rows++;
    }
}
