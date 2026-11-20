// item 29.3: character slots from mods (mods/<id>/Roster/roster.txt, disc/roster_def.h). Cells, order, forms and new
// characters are data: the roster, cell, system id, name and voice tables live in port memory and the game reads them
// through psp_after sites; the group, grid and SFX tables (161 rows already) are written in place
#include "disc/roster_def.h"
void psp_mods_info(std::string& root, std::vector<std::string>& ids);
bool psp_disc_pf(uint32_t* count, uint32_t* sectors);
void psp_disc_pf_buf(uint32_t buf, uint32_t bytes);
int psp_col_count(int s);

// keyed characters use ids 161..: the group, grid and SFX tables are read from 256-entry copies (story mode keeps the
// originals: the game uses ids 161.. there for its map objects)
static const int RO_MAX = roster::MENU_N, RO_SYS = 161, RO_ALL = roster::ALL, RO_RANDOM = 0x46;
static const uint32_t RO_ROSTER = 0x08A73F60u, RO_IDTAB = 0x08A73EA0u, RO_GRID = 0x08A743E0u, RO_SYSID = 0x08A70588u,
    RO_GROUP = 0x08A69720u, RO_SFX = 0x08A7F38Eu, RO_NAME_A = 0x08A70E90u, RO_NAME_B = 0x08A72100u, RO_VLINE = 0x08A7941Cu,
    RO_VBASE = 0x08A7B690u;

struct RosterCell { uint8_t ids[5]; int n; };
struct Roster {
    bool ready = false, custom = false, pf = false;
    std::vector<RosterCell> cells;
    uint8_t grid[RO_ALL];
    bool valid[RO_MAX + 1], fresh[RO_MAX + 1], dyn = false, tfs = false;
    uint32_t grid_t = 0, group_t = 0, sfx_t = 0, tab = 0, idtab = 0, sysid = 0, name_a = 0, name_b = 0, vline = 0, vbase = 0, pfbuf = 0, pf_count = 0, pf_sectors = 0;
    int total = 48, start = 44, nnew = 0;
    uint32_t slider = 0x3E2AAAABu;
};
static Roster ro;

static void ro_build() {
    ro.ready = true;
    uint8_t blocks[48][6];
    for (int i = 0; i < 48; i++) for (int j = 0; j < 6; j++) blocks[i][j] = (uint8_t)psp_l32(RO_ROSTER + i * 24 + j * 4);
    uint32_t idt[48];
    for (int i = 0; i < 48; i++) idt[i] = psp_l32(RO_IDTAB + i * 4);
    for (int i = 0; i < RO_ALL; i++) ro.grid[i] = i < RO_SYS ? psp_l8(RO_GRID + i) : 0xFF;
    memset(ro.valid, 0, sizeof(ro.valid));
    memset(ro.fresh, 0, sizeof(ro.fresh));
    for (int i = 0; i < RO_RANDOM; i++) ro.valid[i] = true;
    if (!roster::defs().loaded && patch_roster.val) {
        std::string root; std::vector<std::string> mods;
        psp_mods_info(root, mods);
        roster::load(root, mods);
    }
    const roster::Defs& d = roster::defs();
    int neb = (int)(psp_l32(0x089F7448u) & 0xFFFF);   // menu numbers the EBOOT's select classes hold (patch_roster.py N)
    int over = 0;
    if (patch_roster.val)
        for (auto& c : d.chars) {
            int m = roster::menu_of(c.sysid);
            if (m >= neb) { over++; continue; }
            ro.grid[c.sysid] = (uint8_t)m;
            ro.valid[m] = ro.fresh[m] = true;
            ro.nnew++;
        }

    // an original character with its own BT3 id: the grid, system id, group and SFX tables follow it (his toolkit's sysids)
    std::vector<std::pair<int, int>> remaps;
    for (auto& rn : d.renames) {
        if (!patch_roster.val || rn.newid < 0 || rn.newid == rn.sysid) continue;
        int to = rn.newid;
        if (to >= RO_SYS || roster::vanilla(to) || ro.grid[to] != 0xFF || ro.grid[rn.sysid] == 0xFF) { printf("roster: %s: id BT3 %d para %d ocupado o no valido\n", rn.mod.c_str(), to, rn.sysid); continue; }
        ro.grid[to] = ro.grid[rn.sysid];
        ro.grid[rn.sysid] = 0xFF;
        remaps.push_back({rn.sysid, to});
    }

    std::vector<RosterCell> cells;
    for (int i = 0; i < 48; i++) {
        RosterCell c{}; c.n = 0;
        for (int j = 0; j < 6 && blocks[i][j] != 0xFF && c.n < 5; j++) c.ids[c.n++] = blocks[i][j];
        cells.push_back(c);
    }
    std::string src = patch_roster.val ? d.layout_mod : std::string();
    if (!src.empty()) {
        std::vector<RosterCell> nc;
        int skipped = 0, rnd = -1;
        bool bad = false;
        for (auto& l : d.layout) {
            RosterCell c{}; c.n = 0;
            for (int v : l) {
                int m = v < 0 ? RO_RANDOM : ro.grid[v];
                if (v >= 0 && (m == 0xFF || m > RO_MAX || !ro.valid[m])) { skipped++; continue; }
                if (c.n >= 5) { printf("roster: %s: la casilla de %d tiene mas de 4 formas\n", src.c_str(), l[0]); bad = true; break; }
                c.ids[c.n++] = (uint8_t)m;
            }
            if (bad) break;
            if (!c.n) continue;
            if (c.ids[0] == RO_RANDOM) {
                if (rnd >= 0 || c.n != 1) { printf("roster: %s: Random debe ir una sola vez y sin formas\n", src.c_str()); bad = true; break; }
                rnd = (int)nc.size();
            }
            nc.push_back(c);
        }
        if (!bad && rnd < 0) { printf("roster: %s: falta la casilla Random\n", src.c_str()); bad = true; }
        if (!bad && (nc.size() < 2 || nc.size() > (size_t)RO_MAX)) { printf("roster: %s: %d casillas fuera de rango\n", src.c_str(), (int)nc.size()); bad = true; }
        if (!bad) {
            RosterCell r = nc[rnd];
            nc.erase(nc.begin() + rnd);
            nc.push_back(r);
            cells = nc;
            ro.custom = true;
            if (skipped) printf("roster: %s: %d ids BT3 sin personaje, fuera de la plantilla\n", src.c_str(), skipped);
        } else src.clear();
    }
    // new characters missing from the layout get a cell of their own before Random
    int extra = 0;
    for (auto& c : d.chars) {
        if (!patch_roster.val) break;
        int m = roster::menu_of(c.sysid);
        if (m >= neb) continue;
        bool used = false;
        for (auto& x : cells) for (int j = 0; j < x.n; j++) used = used || x.ids[j] == m;
        if (used || cells.size() >= (size_t)RO_MAX) continue;
        RosterCell nc{}; nc.n = 1; nc.ids[0] = (uint8_t)m;
        cells.insert(cells.end() - 1, nc);
        extra++;
        ro.custom = true;
    }

    // cell ids: a base keeps its original cell, the rest take free ones; Random last, the first cell gets 0 (cursor)
    int total = (int)cells.size();
    std::map<int, uint32_t> slot_of;
    for (int i = 0; i < 48; i++) if (blocks[i][0] != 0xFF && blocks[i][0] != RO_RANDOM) slot_of[blocks[i][0]] = idt[i];
    std::set<uint32_t> keep;
    for (auto& c : cells) if (slot_of.count(c.ids[0])) keep.insert(slot_of[c.ids[0]]);
    std::vector<uint32_t> free_s;
    for (uint32_t s = 0; s < (uint32_t)total + 48; s++) if (!keep.count(s)) free_s.push_back(s);
    std::vector<uint32_t> slots;
    size_t fi = 0;
    for (auto& c : cells) {
        auto it = slot_of.find(c.ids[0]);
        if (it != slot_of.end()) { slots.push_back(it->second); slot_of.erase(it); }
        else slots.push_back(free_s[fi++]);
    }
    slots.back() = (uint32_t)total - 1;
    if (slots[0] != 0)
        for (size_t j = 1; j < slots.size(); j++) if (slots[j] == 0) { std::swap(slots[0], slots[j]); break; }

    ro.cells = cells; ro.total = total;
    ro.start = (total - 1) & ~3;
    int span = (total + 3) / 4 - 6;
    float fs = span > 0 ? 1.0f / span : 0.0f;
    memcpy(&ro.slider, &fs, 4);
    ro.tab = psp_reserve((RO_MAX + 1) * 24);
    ro.idtab = psp_reserve((RO_MAX + 1) * 4);
    for (int i = 0; i < total; i++) {
        for (int j = 0; j < 6; j++) psp_s32(ro.tab + i * 24 + j * 4, j < cells[i].n ? cells[i].ids[j] : j == cells[i].n ? 0xFF : 0);
        psp_s32(ro.idtab + i * 4, slots[i]);
    }
    for (int i = total; i <= RO_MAX; i++) {   // the grid draws whole rows: positions past the last cell stay empty
        psp_s32(ro.tab + i * 24, 0xFF);
        psp_s32(ro.idtab + i * 4, 0xFFFFFFFFu);
    }
    for (int i = 0; i < RO_SYS; i++) psp_s8(RO_GRID + i, ro.grid[i]);
    ro.grid_t = psp_reserve(256);
    memset(psp_p(ro.grid_t), 0xFF, 256);
    for (int i = 0; i < RO_ALL; i++) psp_s8(ro.grid_t + i, ro.grid[i]);

    // per-number and per-group tables: originals copied, new characters added (group k = 70 + rank of the BT3 id)
    ro.sysid = psp_reserve((RO_MAX + 1) * 4);
    for (int m = 0; m < 80; m++) psp_s32(ro.sysid + m * 4, psp_l32(RO_SYSID + m * 4));
    for (auto& r : remaps) {
        psp_s32(ro.sysid + ro.grid[r.second] * 4, (uint32_t)r.second);
        psp_s8(RO_GROUP + r.second, psp_l8(RO_GROUP + r.first));
        psp_s16(RO_SFX + r.second * 2, psp_l16(RO_SFX + r.first * 2));
    }
    if (!remaps.empty()) printf("roster: %d personajes originales con id BT3 propio\n", (int)remaps.size());
    ro.name_a = psp_reserve(RO_ALL * 4);
    ro.name_b = psp_reserve(RO_ALL * 4);
    ro.vline = psp_reserve(RO_ALL * 126);
    ro.vbase = psp_reserve(RO_ALL * 2);
    memset(psp_p(ro.vline), 0xFF, RO_ALL * 126);
    for (int k = 0; k < roster::K_OLD; k++) {
        psp_s32(ro.name_a + k * 4, psp_l32(RO_NAME_A + k * 4));
        psp_s32(ro.name_b + k * 4, psp_l32(RO_NAME_B + k * 4));
        for (int j = 0; j < 126; j++) psp_s8(ro.vline + k * 126 + j, psp_l8(RO_VLINE + k * 126 + j));
        psp_s16(ro.vbase + k * 2, psp_l16(RO_VBASE + k * 2));
    }
    for (int k = 0; k < roster::K_OLD; k++) roster::name_orig()[k] = psp_l32(RO_NAME_A + k * 4);
    for (auto& rn : d.renames) {   // originals that name a transformation or fusion by key
        for (int i = 0; i < 4; i++) ro.tfs = ro.tfs || rn.tf[i] >= -1;
        for (auto& f : rn.fu) ro.tfs = ro.tfs || f[0] >= -1 || f[1] >= -1;
    }
    int renamed = 0;
    for (auto& rn : d.renames) {   // an original character with its own name: one port text id per group
        if (!patch_roster.val) break;
        int k = psp_l8(RO_GROUP + rn.sysid);
        if (k >= roster::K_OLD) k = -1;
        if (k < 0) continue;
        psp_s32(ro.name_a + k * 4, roster::NAME_OLD + (uint32_t)k);
        psp_s32(ro.name_b + k * 4, roster::NAME_OLD + (uint32_t)k);
        renamed++;
    }
    if (renamed) printf("roster: %d personajes originales con nombre propio\n", renamed);
    ro.group_t = psp_reserve(256);
    ro.sfx_t = psp_reserve(512);
    memset(psp_p(ro.group_t), 0xFF, 256);
    memset(psp_p(ro.sfx_t), 0xFF, 512);
    if (over) printf("roster: AVISO: el EBOOT tiene la seleccion para %d numeros (0x%X); %d personajes quedan fuera: ejecuta patchers/patch_roster.py\n", neb, neb, over);
    for (auto& c : d.chars) {
        if (!patch_roster.val) break;
        if (roster::menu_of(c.sysid) >= neb) continue;
        int m = roster::menu_of(c.sysid), k = roster::group_of(c.sysid), r = roster::rank(c.sysid), dk = psp_l8(RO_GROUP + (c.donor >= 0 ? c.donor : 3));
        psp_s32(ro.sysid + m * 4, (uint32_t)c.sysid);
        if (c.sysid < RO_SYS) {
            psp_s8(RO_GROUP + c.sysid, (uint32_t)k);
            psp_s16(RO_SFX + c.sysid * 2, roster::model_entry(r) + 4);
        }
        psp_s8(ro.group_t + c.sysid, (uint32_t)k);
        psp_s16(ro.sfx_t + c.sysid * 2, roster::model_entry(r) + 4);
        psp_s32(ro.name_a + k * 4, roster::name_of_rank(r));
        psp_s32(ro.name_b + k * 4, roster::name_of_rank(r));
        if (roster::keyed(c.sysid)) ro.dyn = true;
        for (int i = 0; i < 4; i++) ro.tfs = ro.tfs || c.tf[i] >= -1;
        for (auto& f : c.fu) ro.tfs = ro.tfs || f[0] >= -1 || f[1] >= -1;
        if (dk < roster::K_OLD) {
            for (int j = 0; j < 126; j++) psp_s8(ro.vline + k * 126 + j, psp_l8(RO_VLINE + dk * 126 + j));
            psp_s16(ro.vbase + k * 2, psp_l16(RO_VBASE + dk * 2));
        }
    }
    for (int s = 0; s < RO_SYS; s++) { psp_s8(ro.group_t + s, psp_l8(RO_GROUP + s)); psp_s16(ro.sfx_t + s * 2, psp_l16(RO_SFX + s * 2)); }
    ro.pf = psp_disc_pf(&ro.pf_count, &ro.pf_sectors);
    if (ro.pf) { ro.pfbuf = psp_reserve(ro.pf_sectors * 2048 + 64); psp_disc_pf_buf(ro.pfbuf, ro.pf_sectors * 2048); }

    if (ro.custom) printf("roster: plantilla de %s, %d casillas, Random en la %d\n", src.empty() ? "los personajes nuevos" : src.c_str(), total, total - 1);
    else printf("roster: original, %d casillas\n", total);
    if (ro.nnew) printf("roster: %d personajes nuevos, %d con casilla propia anadida; PACKFILE %s\n", ro.nnew, extra, ro.pf ? "ampliado" : "sin ampliar (sin disco virtual)");
}

static inline Roster& ro_get() { if (!ro.ready) ro_build(); return ro; }
static uint32_t ro_name_tab_b() { Roster& r = ro_get(); return r.name_b ? r.name_b : RO_NAME_B; }   // bar names: the relocated table (161 groups)

static bool ro_valid(uint32_t id) { return id <= (uint32_t)RO_MAX && ro_get().valid[id]; }
static bool ro_new_ok(uint32_t id) { return id <= (uint32_t)RO_MAX && ro_get().fresh[id]; }

// a new character is unlocked: its bit in the game's character bitmap ([0x08A7F708] + 0x184)
static void ro_unlock(uint32_t id) {
    uint32_t s = psp_l32(0x08A7F708u);
    if (s < 0x08800000u || s >= 0x10000000u) return;
    uint32_t a = s + 0x184 + (id >> 5) * 4;
    psp_s32(a, psp_l32(a) | (1u << (id & 31)));
}

// PACKFILE index of a group: models 1427 + 5k + colour, voices 4441 + 126k + line; new groups after the original entries
static const uint32_t RO_M_DYN = 350u + roster::NEW * 5, RO_V_DYN = 8820u + roster::NEW * 126;   // old-range offsets of the first keyed group
static uint32_t ro_model(uint32_t v) {
    uint32_t r = v - 0x593u;
    if (r >= 350u && r < RO_M_DYN) return roster::MODEL_BASE + r - 350u;
    if (r >= RO_M_DYN && r < RO_M_DYN + (uint32_t)roster::dyn_n() * 5) return roster::DYN_MODEL + r - RO_M_DYN;
    return v;
}
static uint32_t ro_voice(uint32_t v) {
    uint32_t r = v - 0x1159u;
    if (r >= 8820u && r < RO_V_DYN) return roster::VOICE_BASE + r - 8820u;
    if (r >= RO_V_DYN && r < RO_V_DYN + (uint32_t)roster::dyn_n() * 126) return roster::dyn_voice() + r - RO_V_DYN;
    return v;
}
static bool ro_voice_new(uint32_t idx) { return (idx >= roster::VOICE_BASE && idx < roster::PF_COUNT) || (idx >= roster::dyn_voice() && idx < roster::pf_all()); }
static uint32_t ro_voice_old(uint32_t idx) { return idx >= roster::dyn_voice() ? idx - roster::dyn_voice() + RO_V_DYN : idx - roster::VOICE_BASE + 8820; }   // offset from 0x1159

static bool ro_story() { uint32_t m = psp_l32(0x08A7EA50u); return guest_ptr(m) && psp_l32(m + 0x10) == 5; }
static bool ro_dyn_on() { return ro.ready && ro.dyn && !ro_story(); }   // never builds the tables: the PACKFILE sites do that first
static bool ro_live(uint32_t s) { return s >= (uint32_t)RO_SYS && s < (uint32_t)RO_ALL && ro.grid[s] != 0xFF; }

// characters given by key or id in roster.txt: a new character's block or an original's [original N]
struct RoRefs { const int* tf = nullptr; const int (*fu)[2] = nullptr; const std::vector<roster::VsLine>* vs = nullptr; std::string who; };
static RoRefs ro_refs(uint32_t s) {
    RoRefs r;
    if (s >= (uint32_t)RO_ALL) return r;
    if (const roster::Char* c = roster::find((int)s)) { r.tf = c->tf; r.fu = c->fu; r.vs = &c->vs; r.who = c->key.empty() ? std::to_string(s) : c->key; }
    else if (const roster::Rename* n = roster::renamed((int)s)) { r.tf = n->tf; r.fu = n->fu; r.vs = &n->vs; r.who = std::to_string(s); }
    return r;
}
// intro/victory table of a fighter (021_voice_speaker, 161 records of 4 bytes by rival id): a 209-record copy, keyed
// rivals generic unless the character's roster.txt names a line
static uint32_t ro_vs(uint32_t p, uint32_t s) {
    if (!guest_ptr(p)) return p;
    struct T { uint32_t src, own, out; uint8_t sig[16]; };
    static std::vector<T> tabs;
    static size_t next = 0;
    for (auto& t : tabs) if (t.src == p && t.own == s && !memcmp(t.sig, psp_p(p), 16)) return t.out;
    size_t i = 0;
    while (i < tabs.size() && !(tabs[i].src == p && tabs[i].own == s)) i++;
    if (i == tabs.size()) {
        if (tabs.size() < 32) tabs.push_back({0, 0, psp_reserve(RO_ALL * 4), {}});
        else i = next++ % tabs.size();
    }
    T& t = tabs[i];
    t.src = p; t.own = s;
    memcpy(t.sig, psp_p(p), 16);
    uint8_t* o = psp_p(t.out);
    memcpy(o, psp_p(p), RO_SYS * 4);
    for (int r = RO_SYS; r < RO_ALL; r++) { o[r * 4] = 0xFF; o[r * 4 + 1] = 0; o[r * 4 + 2] = 0xFF; o[r * 4 + 3] = 0; }
    RoRefs rf = ro_refs(s);
    if (rf.vs)
        for (auto& x : *rf.vs) {
            if (x.rid < 0 || x.rid >= RO_ALL) continue;
            o[x.rid * 4 + (x.victory ? 2 : 0)] = (uint8_t)x.line;
            static int logs = 0;
            if (logs++ < 12) printf("roster: %s contra %d: linea de %s %d\n", rf.who.c_str(), x.rid, x.victory ? "victoria" : "intro", x.line);
        }
    return t.out;
}

// life and ki per character ([[0x08A7DBC8]+0x30], 16-byte records by id, 161 in the game): a 209-record copy, a keyed
// character takes the record of its base character (base =, Goku when none)
static uint32_t ro_life(uint32_t p) {
    if (!guest_ptr(p)) return p;
    static uint32_t src = 0, out = 0;
    static uint8_t sig[32];
    if (p == src && out && !memcmp(sig, psp_p(p), 32)) return out;
    if (!out) out = psp_reserve(RO_ALL * 16);
    src = p;
    memcpy(sig, psp_p(p), 32);
    uint8_t* o = psp_p(out);
    memcpy(o, psp_p(p), RO_SYS * 16);
    for (int s = RO_SYS; s < RO_ALL; s++) {
        const roster::Char* ch = roster::find(s);
        int from = ch && ch->donor >= 0 && ch->donor < RO_SYS ? ch->donor : 3;
        memcpy(o + s * 16, o + from * 16, 16);
        if (ch) printf("roster: %s: vida y ki del id %d (vida %d)\n", ch->key.c_str(), from, (int)psp_l32(out + s * 16));
    }
    return out;
}

// transformation targets of a fighter given by key in roster.txt: written into its loaded params (025, +0xA2)
static void ro_fighter(uint32_t cb) {
    if (!ro.ready || !ro.tfs || ro_story() || !guest_ptr(cb)) return;
    uint32_t slot = psp_l32(cb + 0x2C);
    if (slot >= 8) return;
    uint32_t x = psp_l32(0x08B61178u + 4 * slot);
    if (!guest_ptr(x)) return;
    uint32_t s = psp_l32(x + 8), p = psp_l32(x + 0x91C);
    if (s >= (uint32_t)RO_ALL || !guest_ptr(p)) return;
    RoRefs rf = ro_refs(s);
    if (!rf.tf) return;
    auto put = [&](uint32_t off, int t, const char* what, int i) {
        if (t < -1) return;
        uint8_t v = t < 0 ? 0xFF : (uint8_t)t;
        if (psp_l8(p + off) == v) return;
        psp_s8(p + off, v);
        static int logs = 0;
        if (logs++ < 24) printf("roster: %s: %s %d -> %d\n", rf.who.c_str(), what, i, (int)v);
    };
    for (int i = 0; i < 4; i++) put(0xA2 + i, rf.tf[i], "transformacion", i);
    for (int i = 0; i < 3; i++) { put(0xBE + i, rf.fu[i][0], "fusion", i); put(0xC4 + 4 * i, rf.fu[i][1], "companero de fusion", i); }
}

// site kinds: the instruction already ran; its result is replaced
enum { RO_TAB = 1, RO_IDT, RO_LIMIT, RO_WRAP_A, RO_WRAP_B, RO_PAGE, RO_SLIDER, RO_RAND, RO_RAND_OV, RO_CPU, RO_VALID, RO_SKIP,
       RO_GHOST, RO_AVAIL, RO_SYSID_T, RO_NAME_AT, RO_NAME_BT, RO_VLINE_T, RO_VBASE_T, RO_MODEL, RO_MINV, RO_VOICE, RO_VINV, RO_VSUB,
       RO_VFLAG, RO_PF_LBN, RO_PF_SEC, RO_PF_CNT, RO_PF_BUF, RO_PF_HDR, RO_GROUP_T, RO_GRID_T, RO_SFX_T, RO_ALL_LIM, RO_MODEL_LIM, RO_VS,
       RO_COLN, RO_LIFE };
struct RoSite { uint32_t pc; uint8_t kind, r1, r2; };
static const RoSite RO_SITES[] = {
    {0x08A0660Cu, RO_TAB}, {0x08A069A8u, RO_TAB}, {0x08A08504u, RO_TAB}, {0x08A08840u, RO_TAB}, {0x08A0889Cu, RO_TAB},
    {0x08A067A4u, RO_IDT}, {0x08A069A4u, RO_IDT}, {0x08A084C0u, RO_IDT}, {0x08A08500u, RO_IDT}, {0x08A08898u, RO_IDT},
    {0x08A069D4u, RO_LIMIT}, {0x08A08524u, RO_LIMIT}, {0x08A08878u, RO_LIMIT}, {0x08A088C0u, RO_LIMIT}, {0x08A084D4u, RO_LIMIT},
    {0x08A09618u, RO_LIMIT}, {0x08A09624u, RO_LIMIT}, {0x08A096F0u, RO_LIMIT}, {0x08A0973Cu, RO_LIMIT},
    {0x08A0959Cu, RO_WRAP_A}, {0x08A09714u, RO_WRAP_A}, {0x08A09760u, RO_WRAP_A},
    {0x08A095A4u, RO_WRAP_B}, {0x08A09718u, RO_WRAP_B}, {0x08A09764u, RO_WRAP_B},
    {0x08A096ECu, RO_PAGE}, {0x089F9314u, RO_SLIDER},
    {0x08A0690Cu, RO_RAND}, {0x08A08980u, RO_RAND}, {0x08A0916Cu, RO_RAND}, {0x08A0936Cu, RO_RAND}, {0x08A094A8u, RO_RAND},
    {0x08A09554u, RO_RAND}, {0x08A09610u, RO_RAND}, {0x08A097A0u, RO_RAND}, {0x08A09828u, RO_RAND},
    {0x08A82A6Cu, RO_RAND_OV}, {0x08A82AE0u, RO_RAND_OV}, {0x08A831D8u, RO_RAND_OV},
    {0x08A8F394u, RO_RAND_OV}, {0x08A91088u, RO_RAND_OV}, {0x08AA45BCu, RO_RAND_OV}, {0x08AA48BCu, RO_RAND_OV},
    {0x08A8DD68u, RO_RAND_OV}, {0x08A8E4D0u, RO_RAND_OV}, {0x08A8E550u, RO_RAND_OV},
    {0x08AA930Cu, RO_RAND_OV}, {0x08AB77A4u, RO_RAND_OV}, {0x08AB7824u, RO_RAND_OV},
    {0x08A08720u, RO_CPU},
    {0x08A33384u, RO_VALID}, {0x08A33654u, RO_VALID}, {0x08A0874Cu, RO_VALID}, {0x089CEE60u, RO_VALID}, {0x089FA294u, RO_VALID},
    {0x089FA768u, RO_VALID}, {0x089F7718u, RO_VALID}, {0x089F7C60u, RO_VALID}, {0x089F8508u, RO_VALID}, {0x089FBBBCu, RO_VALID},
    {0x08A0314Cu, RO_VALID}, {0x08A3E398u, RO_VALID}, {0x08A0662Cu, RO_VALID}, {0x088573A8u, RO_VALID}, {0x08857464u, RO_VALID},
    {0x089FA314u, RO_GHOST}, {0x08A06698u, RO_AVAIL}, {0x08A06700u, RO_AVAIL},
    {0x089F9B6Cu, RO_SKIP, 17, 20}, {0x089FB2ACu, RO_SKIP, 17, 20}, {0x089FF6F0u, RO_SKIP, 17, 20},
    {0x08A01AD8u, RO_SKIP, 16, 17}, {0x08A0294Cu, RO_SKIP, 16, 18},
    {0x089C60D8u, RO_SYSID_T}, {0x089D3664u, RO_NAME_AT}, {0x089E68ACu, RO_NAME_BT}, {0x088531A4u, RO_VLINE_T}, {0x088531C4u, RO_VBASE_T},
    {0x089C6CE4u, RO_MODEL}, {0x089C6D80u, RO_MODEL}, {0x089C6DC0u, RO_MINV}, {0x08852C28u, RO_VOICE}, {0x08852C98u, RO_VOICE},
    {0x0885312Cu, RO_VINV}, {0x08853188u, RO_VSUB}, {0x08853220u, RO_VFLAG},
    {0x0882FBE8u, RO_PF_LBN}, {0x0882FBF8u, RO_PF_SEC}, {0x0882FC94u, RO_PF_SEC}, {0x0882FBFCu, RO_PF_CNT},
    {0x0882FB84u, RO_PF_BUF}, {0x08830074u, RO_PF_BUF}, {0x088301C0u, RO_PF_BUF}, {0x08830298u, RO_PF_BUF}, {0x08830B8Cu, RO_PF_BUF},
    {0x08831010u, RO_PF_BUF}, {0x08831448u, RO_PF_BUF}, {0x088315D4u, RO_PF_BUF}, {0x0883177Cu, RO_PF_BUF}, {0x08831CC4u, RO_PF_BUF},
    {0x0882FCE8u, RO_PF_HDR},
    {0x08852B5Cu, RO_GROUP_T}, {0x08852B74u, RO_GROUP_T}, {0x08852BFCu, RO_GROUP_T}, {0x08852C64u, RO_GROUP_T},
    {0x08A06B84u, RO_GRID_T}, {0x08A088E8u, RO_GRID_T}, {0x089C6D30u, RO_SFX_T},
    {0x08852B8Cu, RO_ALL_LIM}, {0x08A06B9Cu, RO_ALL_LIM}, {0x08A088DCu, RO_ALL_LIM}, {0x089C6D10u, RO_ALL_LIM}, {0x089C6D1Cu, RO_ALL_LIM},
    {0x089C6CA4u, RO_MODEL_LIM},
    {0x089061E8u, RO_VS}, {0x089061F0u, RO_VS}, {0x08906778u, RO_VS}, {0x08906790u, RO_VS}, {0x089068CCu, RO_VS}, {0x089068E4u, RO_VS},
    {0x0890719Cu, RO_VS}, {0x089071A8u, RO_VS},
    {0x08A06DA4u, RO_COLN}, {0x08A08A78u, RO_COLN},
    {0x08926FDCu, RO_LIFE}, {0x08927158u, RO_LIFE}, {0x089278ACu, RO_LIFE}, {0x08927C4Cu, RO_LIFE}, {0x0893C2BCu, RO_LIFE},
};

void psp_after(uint32_t pc, Ctx& c) {
    if (st_after(pc, c) || tf_after(pc, c)) return;
    static std::map<uint32_t, const RoSite*> by_pc;
    if (by_pc.empty()) for (auto& s : RO_SITES) by_pc[s.pc] = &s;
    auto it = by_pc.find(pc);
    if (it == by_pc.end()) return;
    const RoSite& s = *it->second;
    uint32_t w = psp_l32(pc);
    int rt = (w >> 16) & 31, rs = (w >> 21) & 31, imm = (int16_t)(w & 0xFFFF), op = w >> 26;
    if (s.kind == RO_RAND_OV && !(op == 0x0D && (w & 0xFFFF) == 0x63)) return;   // overlay sites: only their cell ori (0x63 = none)
    if (s.kind >= RO_GROUP_T && !ro_dyn_on()) return;
    Roster& r = ro_get();
    switch (s.kind) {
        case RO_TAB: c.r[rt] = r.tab; break;
        case RO_IDT: c.r[rt] = r.idtab; break;
        case RO_LIMIT: c.r[rt] = op == 0x0B ? (uint32_t)(c.r[rs] < (uint32_t)r.total) : (uint32_t)((int32_t)c.r[rs] < r.total); break;
        case RO_WRAP_A: c.r[rt] += (uint32_t)(r.start - imm); break;
        case RO_WRAP_B: {
            int v = (int)c.r[rt] - imm + r.start;
            if ((r.total & 3) && v >= r.total) v -= 4;
            c.r[rt] = (uint32_t)v;
            break;
        }
        case RO_PAGE: c.r[rt] += (uint32_t)(r.start - imm); break;
        case RO_SLIDER: c.r[rt] = r.slider; break;
        case RO_RAND: c.r[rt] = (uint32_t)r.total - 1; break;
        case RO_RAND_OV: case RO_CPU: if (r.custom) c.r[rt] = (uint32_t)r.total - 1; break;
        case RO_VALID:
            c.r[rt] = ro_valid(c.r[rs]);
            if (ro_new_ok(c.r[rs])) ro_unlock(c.r[rs]);
            break;
        case RO_GHOST: if (c.r[16] == 0xFFu) c.r[5] = 0; break;   // grid draw: no locked frame on an empty position
        case RO_AVAIL: if (ro_new_ok(c.r[16])) c.r[2] = 1; break;
        case RO_SKIP:
            if (c.r[s.r1] >= 0x47u && c.r[s.r1] < 0x4Cu) { c.r[s.r1] += 5; c.r[s.r2] += 20; }
            break;
        case RO_SYSID_T: c.r[rt] = r.sysid; break;
        case RO_NAME_AT: c.r[rt] = r.name_a; break;
        case RO_NAME_BT: c.r[rt] = r.name_b; break;
        case RO_VLINE_T: c.r[rt] = r.vline; break;
        case RO_VBASE_T: c.r[rt] = r.vbase; break;
        case RO_MODEL: if (r.pf) c.r[rt] = ro_model(c.r[rt]); break;
        case RO_MINV:   // a0 = index - 1427 for the k = a0 / 5 that follows
            if (r.pf && c.r[5] >= roster::MODEL_BASE && c.r[5] < roster::VOICE_BASE) c.r[rt] = c.r[5] - roster::MODEL_BASE + 350;
            else if (r.pf && c.r[5] >= roster::DYN_MODEL && c.r[5] < roster::dyn_voice()) c.r[rt] = c.r[5] - roster::DYN_MODEL + RO_M_DYN;
            break;
        case RO_VOICE: if (r.pf) c.r[rt] = ro_voice(c.r[rt]); break;
        case RO_VINV: case RO_VFLAG:   // index -> group / line: a new voice becomes the old-range offset the code divides by 126
            if (r.pf && ro_voice_new(c.r[4])) { c.r[4] = ro_voice_old(c.r[4]) + 0x1159; c.r[5] = 1; }
            break;
        case RO_VSUB: if (r.pf && ro_voice_new(c.r[6])) c.r[5] = ro_voice_old(c.r[6]); break;
        case RO_GROUP_T: if (ro_dyn_on()) c.r[rt] = r.group_t; break;
        case RO_GRID_T: if (ro_dyn_on()) c.r[rt] = r.grid_t; break;
        case RO_SFX_T: if (ro_dyn_on()) c.r[rt] = r.sfx_t; break;
        case RO_ALL_LIM: if (ro_dyn_on()) c.r[rt] = c.r[rs] < (uint32_t)RO_ALL; break;
        case RO_MODEL_LIM: if (ro_dyn_on() && ro_live(c.r[rs])) c.r[rt] = 1; break;
        case RO_VS: if (ro_dyn_on()) c.r[rt] = ro_vs(c.r[rt], psp_l32(c.r[rs] + 8)); break;
        case RO_LIFE: if (ro_dyn_on()) c.r[rt] = ro_life(c.r[rt]); break;
        case RO_COLN:
            if (ro_dyn_on() && ro_live(c.r[2])) { int n = psp_col_count((int)c.r[2]); c.r[rt] = (uint32_t)(n > 0 ? n : 1); }
            break;
        case RO_PF_LBN: if (r.pf) c.r[rt] = c.r[rs] + r.pf_sectors; break;
        case RO_PF_SEC: if (r.pf) c.r[rt] = r.pf_sectors; break;
        case RO_PF_CNT: if (r.pf) c.r[rt] = r.pf_count; break;
        case RO_PF_BUF: if (r.pf) c.r[rt] = r.pfbuf; break;
        case RO_PF_HDR: if (r.pf) c.r[rt] = psp_l32(r.pfbuf); break;
    }
}

// select screen and game room: per-number big render bind (0x088404D8, a2 AGS and a3 RHG, 1-based pak entries);
// new characters use their own entries (disc.h roster_assets), numbers without files of their own get none
bool psp_roster_big(int menu, uint32_t* sel, uint32_t* room, uint32_t* rhg);
static bool ro_bind(Ctx& c, void (*orig)(Ctx&)) {
    uint32_t ra = c.r[31];
    if (ra != 0x089F8D08u && ra != 0x08A01AD0u) return false;
    uint32_t id = c.r[16];
    if (id < 0x47u) return false;
    uint32_t sel, room, rhg;
    if (ro_new_ok(id) && psp_roster_big((int)id, &sel, &room, &rhg)) {
        uint32_t a = ra == 0x089F8D08u ? sel : room;
        c.r[6] = a; c.r[7] = rhg;
        orig(c);
        static int logs = 0;
        if (logs++ < 8) printf("roster: render grande del numero 0x%02X: AGS %u, RHG %u\n", id, a, rhg);
        return true;
    }
    c.r[2] = 0xFFFFFFFFu;
    return true;
}

// the original roster for the editor (window thread = game thread): cells as BT3 ids (-1 Random) and each original
// group's name text id
void psp_roster_vanilla(std::vector<std::vector<int>>& cells, std::vector<uint32_t>& name_ids, std::vector<int>& sys_of_menu) {
    cells.clear(); name_ids.clear(); sys_of_menu.clear();
    for (int m = 0; m < RO_RANDOM; m++) sys_of_menu.push_back((int)psp_l32(RO_SYSID + m * 4));
    for (int i = 0; i < 48; i++) {
        std::vector<int> c;
        for (int j = 0; j < 6; j++) {
            uint32_t m = psp_l32(RO_ROSTER + i * 24 + j * 4) & 0xFF;
            if (m == 0xFF) break;
            c.push_back(m == (uint32_t)RO_RANDOM ? -1 : (int)psp_l32(RO_SYSID + m * 4));
        }
        if (!c.empty()) cells.push_back(c);
    }
    for (size_t i = 0; i + 1 < cells.size(); i++)
        if (cells[i][0] == -1) { auto r = cells[i]; cells.erase(cells.begin() + i); cells.push_back(r); break; }
    for (int k = 0; k < roster::K_OLD; k++) name_ids.push_back(psp_l32(RO_NAME_A + k * 4));
}
