// item 29.3 maps: new map slots from mods (mods/<id>/Stages/stages.txt, disc/stages_def.h), as Los ijue30s' patch_stage.
// The select screen's three per-cell arrays live in the object's tail (patchers/patch_roster.py); counts, Random, the
// index -> AGS state maps, PACKFILE entries, availability and the battle's tournament checks are answered here
#include "disc/stages_def.h"
int psp_stage_base();

struct StageState { bool ready = false; int n = 10, base = -1; std::vector<uint8_t> tour; };
static StageState stg;
static int st_shown = -1;   // map under the cursor of the stage select (item 33), Random keeps the previous one
static int st_cur = -1;     // cell under the cursor, Random included (map select 3D)
static void m3d_ctor(uint32_t o);
static void m3d_dtor(uint32_t o);
static const uint32_t ST_OBJ = 0x13Cu, ST_CELLS = stages::CAP + 1;

static StageState& st_get() {
    if (stg.ready) return stg;
    stg.ready = true;
    int b = psp_stage_base();
    auto& d = stages::defs();
    if (b >= 0 && !d.maps.empty()) {
        stg.base = b;
        stg.n = stages::OLD + (int)d.maps.size();
        stg.tour.assign(ST_CELLS, 0);
        int nt = 0;
        for (size_t j = 0; j < d.maps.size(); j++) if (d.maps[j].tour) { stg.tour[stages::OLD + j] = 1; nt++; }
        printf("mapas: %d mapas en la seleccion (%d nuevos, %d con reglas de torneo), Random en %d\n", stg.n, stg.n - stages::OLD, nt, stg.n);
    }
    return stg;
}

// logical index -> AGS state: originals base + i, Random fixed, new maps after every original state
static uint32_t st_state(uint32_t i, uint32_t base, uint32_t rnd, uint32_t first) {
    int n = st_get().n;
    if (i < (uint32_t)stages::OLD) return base + i;
    if (i == (uint32_t)n) return rnd;
    return first + i - stages::OLD;
}

enum { ST_RANDOM = 1, ST_PICK, ST_CELLS_LT, ST_LT_N, ST_BIG, ST_THUMB, ST_NAME, ST_NEXT, ST_PF, ST_OV_N, ST_OV_PICK };
struct StgSite { uint32_t pc; uint8_t kind, reg; };
static const StgSite ST_SITES[] = {
    {0x08A04808u, ST_RANDOM, 18}, {0x08A04A08u, ST_RANDOM, 17}, {0x08A053DCu, ST_RANDOM, 22}, {0x08A07300u, ST_RANDOM, 5},
    {0x08A0752Cu, ST_RANDOM, 18}, {0x08A07108u, ST_RANDOM, 5}, {0x08A04A10u, ST_PICK, 4},
    {0x08A048C0u, ST_CELLS_LT, 4}, {0x08A04170u, ST_CELLS_LT, 6}, {0x08A05164u, ST_CELLS_LT, 4}, {0x08A053F4u, ST_CELLS_LT, 4},
    {0x08A05930u, ST_CELLS_LT, 4}, {0x08A0753Cu, ST_CELLS_LT, 4}, {0x08A07118u, ST_LT_N, 5},
    {0x08A05098u, ST_BIG, 6}, {0x08A041BCu, ST_BIG, 20}, {0x08A043DCu, ST_BIG, 20},
    {0x08A05054u, ST_THUMB, 20}, {0x08A0505Cu, ST_NAME, 22}, {0x08A042CCu, ST_NAME, 20}, {0x08A044E8u, ST_NAME, 19},
    {0x08A05160u, ST_NEXT, 18},
    {0x089C6E68u, ST_PF, 4}, {0x089C6E7Cu, ST_PF, 4}, {0x08A07124u, ST_PF, 4}, {0x08A0714Cu, ST_PF, 5},
    {0x08A81B88u, ST_OV_N, 5}, {0x08A81BACu, ST_OV_PICK, 4}, {0x08A81934u, ST_OV_N, 5}, {0x08A81954u, ST_OV_PICK, 4},
    {0x08AB9FA0u, ST_OV_N, 4}, {0x08AB9FA8u, ST_OV_N, 4}, {0x08AB9FB8u, ST_OV_PICK, 4},
};

static bool st_after(uint32_t pc, Ctx& c) {
    if (pc < 0x089C6E68u || pc > 0x08AB9FB8u) return false;
    static std::map<uint32_t, const StgSite*> by_pc;
    if (by_pc.empty()) for (auto& s : ST_SITES) by_pc[s.pc] = &s;
    auto it = by_pc.find(pc);
    if (it == by_pc.end()) return false;
    const StgSite& s = *it->second;
    StageState& g = st_get();
    if (s.kind == ST_BIG) {   // the page builds every map image at once when it loads: several indices within 5 ms are not a choice
        static uint64_t t_last = 0;
        static int i_last = -1, keep = -1;
        uint32_t i = c.r[s.reg] - (psp_l32(pc) & 0xFFFF);
        uint64_t t = psp_time_us();
        bool burst = t - t_last < 5000 && (int)i != i_last;
        t_last = t; i_last = (int)i;
        int was = st_shown;
        if (burst) st_shown = keep;
        else if (i < (uint32_t)g.n) { keep = st_shown; st_shown = (int)i; }
        if (!burst && i <= (uint32_t)g.n) st_cur = (int)i;
        if (st_shown != was) { printf("select3d: mapa de la seleccion de escenario %d%s\n", st_shown, burst ? " (carga de la pagina, ignorado)" : ""); fflush(stdout); }
    }
    if (g.base < 0 && s.kind != ST_OV_N && s.kind != ST_OV_PICK) return true;   // overlays may hold another count: always the real one
    uint32_t w = psp_l32(pc), imm = w & 0xFFFF, n = (uint32_t)g.n;
    uint32_t& r = c.r[s.reg];
    switch (s.kind) {
        case ST_RANDOM: r = n; break;
        case ST_PICK: r = n - 1; break;
        case ST_CELLS_LT: {   // 0x08A0753C compares a0 with itself: the index is also in s3 (the step loop never wrapped past Random: hang)
            uint32_t rs = (w >> 21) & 31, v = rs == s.reg ? c.r[19] : c.r[rs];
            r = (uint32_t)((int32_t)v < (int32_t)n + 1);
            break;
        }
        case ST_LT_N: r = (uint32_t)((int32_t)c.r[(w >> 21) & 31] < (int32_t)n); break;
        case ST_BIG: r = st_state(r - imm, 5, 15, 16); break;
        case ST_THUMB: r = st_state(r - imm, 6, 16, 18); break;
        case ST_NAME: r = st_state(r - imm, 1, 11, 12); break;
        case ST_NEXT: {   // s6 was both the name state and i + 1: back to the next index
            uint32_t v = c.r[22];
            r = v <= 10u ? v : v == 11u ? n + 1 : v - 1;
            break;
        }
        case ST_PF: {   // 0x495 + 2i map, 0x496 + 2i SFX: new maps' entries after the roster groups
            uint32_t i = (r - imm) / 2;
            if (i >= (uint32_t)stages::OLD && i < n) r = (uint32_t)g.base + 2 * (i - stages::OLD) + (imm - 0x495u);
            break;
        }
        case ST_OV_N: case ST_OV_PICK:   // battle build in 24_18, 48_30, 56_38: "map == Random -> draw 0..N-1"
            if ((w >> 26) != 0x0D || ((w >> 21) & 31) || ((w >> 16) & 31) != s.reg || imm < 9) break;
            r = s.kind == ST_OV_N ? n : n - 1;
            break;
    }
    return true;
}

// battle: the 50 callers of the map index getter that test "< 11 and not < 10" (tournament ring) get 10 or 0, never a
// new raw index; the two switches inherited from BT3 send new maps to their default case
static const uint32_t ST_TOUR_RA[] = {
    0x0886213C, 0x088622A0, 0x08863F40, 0x08866EB0, 0x088676A8, 0x08868864, 0x088690D4, 0x0886D41C, 0x0886FA68, 0x08872C84,
    0x089208D8, 0x08924814, 0x08924884, 0x08926414, 0x0892684C, 0x089318B0, 0x08946B3C, 0x0894A724, 0x08958F04, 0x089593EC,
    0x08959848, 0x08959E24, 0x0895A1E4, 0x0895A2FC, 0x0895A4C4, 0x0895AF58, 0x0895B208, 0x0895B820, 0x0895C8B8, 0x0895CA10,
    0x08965BC8, 0x08969E34, 0x0896A34C, 0x0896A588, 0x0896D0CC, 0x08971808, 0x08972BB4, 0x089785E0, 0x089789C4, 0x0897B874,
    0x0897C418, 0x0897CEBC, 0x0897E430, 0x0897F14C, 0x0897FD5C, 0x08980484, 0x08980B58, 0x0898112C, 0x08981944, 0x089943D4};

static bool st_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&)) {
    if (dir == 0x08A03C0Cu) {   // constructor: the tail arrays start empty
        orig(c);
        uint32_t o = c.r[2];
        if (o >= 0x08800000u && o < 0x10000000u) { for (uint32_t k = 0; k < 3 * ST_CELLS; k++) psp_s32(o + ST_OBJ + 4 * k, 0); m3d_ctor(o); }
        return true;
    }
    if (dir == 0x08A03C9Cu) {   // destructor: the tail arrays' elements go as the loose ones do
        uint32_t o = c.r[4];
        m3d_dtor(o);
        if (o >= 0x08800000u && o < 0x10000000u)
            for (uint32_t k = 0; k < 3 * ST_CELLS; k++) {
                uint32_t e = psp_l32(o + ST_OBJ + 4 * k);
                if (!e) continue;
                hud_guest(c, 0x089D248Cu, e, 3);
                psp_s32(o + ST_OBJ + 4 * k, 0);
            }
        orig(c);
        return true;
    }
    StageState& g = st_get();
    if (g.base < 0) return false;
    if (dir == 0x08A07170u) {   // availability: new maps are always offered
        uint32_t i = c.r[5];
        if (i >= (uint32_t)stages::OLD && i < (uint32_t)g.n) { c.r[2] = 1; return true; }
        return false;
    }
    if (dir == 0x08858764u) {
        uint32_t ra = c.r[31];
        orig(c);
        uint32_t v = c.r[2];
        if (ra == 0x0887BF9Cu || ra == 0x089149ACu) { if (v >= (uint32_t)stages::OLD) c.r[2] = 0; return true; }
        static std::set<uint32_t> tour_ra(std::begin(ST_TOUR_RA), std::end(ST_TOUR_RA));
        if (tour_ra.count(ra)) c.r[2] = v < ST_CELLS && g.tour[v] ? 10u : 0u;
        return true;
    }
    return false;
}

// the battle map's name for Discord (the game's getter, names as in the map select)
static std::string dc_map_name(const Ctx& c, int li) {
    uint32_t v = hud_guest(c, 0x08858764u, 0);
    if (v > 4096) return "";
    std::string n = stages::name_of((int)v, li);
    return n.size() > 40 ? n.substr(0, 37) + "..." : n;
}
