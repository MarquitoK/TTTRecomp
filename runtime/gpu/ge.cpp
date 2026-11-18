#include "psp.h"
#include "perf.h"
void psp_gpu_flush();
#include <cstdio>
#include <cstring>

// Walks the GE display lists and takes the command census.
// Format: one 32-bit word per command, 8 bits of opcode and 24 of argument.

static uint64_t count_n[256];
static uint32_t first_arg[256];
static uint64_t lists = 0, commands = 0;
static uint32_t base_dir = 0;
static uint64_t prim_type[8];
static uint64_t vtype_seen[64];
static int n_vtype = 0;
static uint32_t vtypes[64];

// confirmed names; the rest print as numbers
static const char* NOMBRE[256] = {0};
static bool names_set = false;

static void set_names() {
    if (names_set) return;
    names_set = true;
    static const struct { int c; const char* n; } T[] = {
        {0x00,"NOP"},{0x01,"VADDR"},{0x02,"IADDR"},{0x04,"PRIM"},{0x05,"BEZIER"},{0x06,"SPLINE"},
        {0x07,"BBOX"},{0x08,"JUMP"},{0x09,"BJUMP"},{0x0A,"CALL"},{0x0B,"RET"},{0x0C,"END"},
        {0x0E,"SIGNAL"},{0x0F,"FINISH"},{0x10,"BASE"},{0x12,"VTYPE"},{0x13,"OFFSETADDR"},{0x14,"ORIGIN"},
        {0x15,"REGION1"},{0x16,"REGION2"},{0x17,"LIGHTINGEN"},{0x18,"LIGHT0EN"},{0x19,"LIGHT1EN"},
        {0x1A,"LIGHT2EN"},{0x1B,"LIGHT3EN"},{0x1C,"CLIPEN"},{0x1D,"CULLEN"},{0x1E,"TEXTUREEN"},
        {0x1F,"FOGEN"},{0x20,"DITHEREN"},{0x21,"ALPHABLENDEN"},{0x22,"ALPHATESTEN"},{0x23,"ZTESTEN"},
        {0x24,"STENCILEN"},{0x25,"ANTIALIASEN"},{0x26,"PATCHCULLEN"},{0x27,"COLORTESTEN"},{0x28,"LOGICOPEN"},
        {0x2A,"BONEMTXNUM"},{0x2B,"BONEMTXDATA"},
        {0x2C,"MORPHW0"},{0x2D,"MORPHW1"},{0x2E,"MORPHW2"},{0x2F,"MORPHW3"},
        {0x30,"MORPHW4"},{0x31,"MORPHW5"},{0x32,"MORPHW6"},{0x33,"MORPHW7"},
        {0x36,"PATCHDIV"},{0x37,"PATCHPRIM"},{0x38,"PATCHFACING"},
        {0x3A,"WORLDMTXNUM"},{0x3B,"WORLDMTXDATA"},{0x3C,"VIEWMTXNUM"},{0x3D,"VIEWMTXDATA"},
        {0x3E,"PROJMTXNUM"},{0x3F,"PROJMTXDATA"},{0x40,"TGENMTXNUM"},{0x41,"TGENMTXDATA"},
        {0x42,"VIEWPORTXS"},{0x43,"VIEWPORTYS"},{0x44,"VIEWPORTZS"},{0x45,"VIEWPORTXC"},{0x46,"VIEWPORTYC"},
        {0x47,"VIEWPORTZC"},{0x48,"TEXSCALEU"},{0x49,"TEXSCALEV"},{0x4A,"TEXOFFSETU"},{0x4B,"TEXOFFSETV"},
        {0x4C,"OFFSETX"},{0x4D,"OFFSETY"},{0x50,"SHADEMODE"},{0x51,"REVERSENORMAL"},
        {0x53,"MATERIALUPDATE"},{0x54,"MATEMISSIVE"},{0x55,"MATAMBIENT"},{0x56,"MATDIFFUSE"},
        {0x57,"MATSPECULAR"},{0x58,"MATALPHA"},{0x5B,"MATSPECCOEF"},{0x5C,"AMBIENTCOLOR"},{0x5D,"AMBIENTALPHA"},
        {0x5E,"LIGHTMODE"},{0x5F,"LIGHTTYPE0"},{0x60,"LIGHTTYPE1"},{0x61,"LIGHTTYPE2"},{0x62,"LIGHTTYPE3"},
        {0x63,"LX0"},{0x64,"LY0"},{0x65,"LZ0"},{0x66,"LX1"},{0x67,"LY1"},{0x68,"LZ1"},
        {0x69,"LX2"},{0x6A,"LY2"},{0x6B,"LZ2"},{0x6C,"LX3"},{0x6D,"LY3"},{0x6E,"LZ3"},
        {0x6F,"LDX0"},{0x70,"LDY0"},{0x71,"LDZ0"},{0x72,"LDX1"},{0x73,"LDY1"},{0x74,"LDZ1"},
        {0x75,"LDX2"},{0x76,"LDY2"},{0x77,"LDZ2"},{0x78,"LDX3"},{0x79,"LDY3"},{0x7A,"LDZ3"},
        {0x7B,"LKA0"},{0x7C,"LKB0"},{0x7D,"LKC0"},{0x7E,"LKA1"},{0x7F,"LKB1"},{0x80,"LKC1"},
        {0x81,"LKA2"},{0x82,"LKB2"},{0x83,"LKC2"},{0x84,"LKA3"},{0x85,"LKB3"},{0x86,"LKC3"},
        {0x87,"LKS0"},{0x88,"LKS1"},{0x89,"LKS2"},{0x8A,"LKS3"},{0x8B,"LKO0"},{0x8C,"LKO1"},{0x8D,"LKO2"},{0x8E,"LKO3"},
        {0x8F,"LAC0"},{0x90,"LDC0"},{0x91,"LSC0"},{0x92,"LAC1"},{0x93,"LDC1"},{0x94,"LSC1"},
        {0x95,"LAC2"},{0x96,"LDC2"},{0x97,"LSC2"},{0x98,"LAC3"},{0x99,"LDC3"},{0x9A,"LSC3"},
        {0x9B,"CULL"},{0x9C,"FBP"},{0x9D,"FBW"},{0x9E,"ZBP"},{0x9F,"ZBW"},
        {0xA0,"TBP0"},{0xA1,"TBP1"},{0xA2,"TBP2"},{0xA3,"TBP3"},{0xA4,"TBP4"},{0xA5,"TBP5"},{0xA6,"TBP6"},{0xA7,"TBP7"},
        {0xA8,"TBW0"},{0xA9,"TBW1"},{0xAA,"TBW2"},{0xAB,"TBW3"},{0xAC,"TBW4"},{0xAD,"TBW5"},{0xAE,"TBW6"},{0xAF,"TBW7"},
        {0xB0,"CBP"},{0xB1,"CBPH"},{0xB2,"TRXSBP"},{0xB3,"TRXSBW"},{0xB4,"TRXDBP"},{0xB5,"TRXDBW"},
        {0xB8,"TSIZE0"},{0xB9,"TSIZE1"},{0xBA,"TSIZE2"},{0xBB,"TSIZE3"},{0xBC,"TSIZE4"},{0xBD,"TSIZE5"},{0xBE,"TSIZE6"},{0xBF,"TSIZE7"},
        {0xC0,"TMAP"},{0xC1,"TEXSHADELS"},{0xC2,"TMODE"},{0xC3,"TPSM"},{0xC4,"CLOAD"},{0xC5,"CLUTFORMAT"},
        {0xC6,"TFILTER"},{0xC7,"TWRAP"},{0xC8,"TLEVEL"},{0xC9,"TFUNC"},{0xCA,"TEC"},{0xCB,"TFLUSH"},{0xCC,"TSYNC"},
        {0xCD,"FOG1"},{0xCE,"FOG2"},{0xCF,"FOGCOLOR"},{0xD0,"TSLOPE"},{0xD2,"PSM"},{0xD3,"CLEARMODE"},
        {0xD4,"SCISSOR1"},{0xD5,"SCISSOR2"},{0xD6,"MINZ"},{0xD7,"MAXZ"},{0xD8,"COLORTEST"},{0xD9,"COLORREF"},
        {0xDA,"COLORTESTMASK"},{0xDB,"ALPHATEST"},{0xDC,"STENCILTEST"},{0xDD,"STENCILOP"},{0xDE,"ZTEST"},
        {0xDF,"BLEND"},{0xE0,"BLENDFIXA"},{0xE1,"BLENDFIXB"},{0xE2,"DITH0"},{0xE3,"DITH1"},{0xE4,"DITH2"},{0xE5,"DITH3"},
        {0xE6,"LOGICOP"},{0xE7,"ZWRITEDISABLE"},{0xE8,"MASKRGB"},{0xE9,"MASKALPHA"},
        {0xEA,"TRANSFERSTART"},{0xEB,"TRXPOS"},{0xEC,"TRXPOS2"},{0xEE,"TRXSIZE"},
    };
    for (auto& t : T) NOMBRE[t.c] = t.n;
}

static const char* nombre(int c) {
    set_names();
    return NOMBRE[c];
}

// the active list is walked in chunks: on enqueue and whenever the stall address moves
static struct { uint32_t pc, stop, base; uint32_t stack[32]; int sp; bool live; } L;
static uint64_t lists_nonempty = 0;

// split screen (item 20c): the battle draw builds its packets in a per-frame buffer; each pass registers its range.
// A top-level CALL into a pass range sets that pass, into the rest of the buffer (setup, HUD) full screen, and
// anywhere else (the map list in VRAM, shared lists) keeps the current pass; it lasts until the list ends
static struct { uint32_t lo, hi; uint64_t at; } split_rng[2][13];
static int split_cur = 0;

// TTT_SPLIT_LOG=1: after 300 split frames, the next two lists with every CALL, its pass and its primitives
static int split_regs = 0, split_dbg_lists = 0, split_dbg_lines = 0;
static long split_p3 = 0, split_p2 = 0, split_d3 = 0, split_d2 = 0;
static uint32_t split_vt = 0;
static bool split_dbg_on() { static int v = -1; if (v < 0) { const char* e = getenv("TTT_SPLIT_LOG"); v = e && *e == '1'; } return v != 0; }

void psp_ge_split_range(int slot, int pass, uint32_t lo, uint32_t hi) {
    if (slot < 0 || slot > 1 || pass < 0 || pass > 12) return;
    if (split_dbg_on() && pass == 1 && ++split_regs == 300) split_dbg_lists = 3;
    if (split_dbg_lists > 0 && split_dbg_lines < 600) { split_dbg_lines++; printf("GE split: pasada %d rango 0x%08X-0x%08X ranura %d\n", pass, lo, hi, slot); }
    split_rng[slot][pass].lo = lo & 0x0FFFFFFC; split_rng[slot][pass].hi = hi & 0x0FFFFFFC; split_rng[slot][pass].at = lists;
}

static int split_skip[2] = {0, 0}, split_slot = -1;
void psp_ge_split_skip(int slot, int mask) { if (slot >= 0 && slot < 2) split_skip[slot] = mask; }

static int split_of(uint32_t a) {
    for (int p = 12; p >= 5; p--)
        for (int s = 0; s < 2; s++) {
            auto& r = split_rng[s][p];
            if (r.hi > r.lo && a >= r.lo && a < r.hi && lists - r.at <= 6) return p;
        }
    for (int p = 3; p >= 0; p--)
        for (int s = 0; s < 2; s++) {
            auto& r = split_rng[s][p + 1];
            if (r.hi > r.lo && a >= r.lo && a < r.hi && lists - r.at <= 6) { split_slot = s; return p + 1; }
        }
    for (int s = 0; s < 2; s++) {
        auto& r = split_rng[s][0];
        if (r.hi > r.lo && a >= r.lo && a < r.hi && lists - r.at <= 6) return 0;
    }
    return -1;
}

static void split_set(int p) { if (p != split_cur) { split_cur = p; psp_gpu_cmd(0xFB, (uint32_t)p); } }

void psp_gpu_split_time(int on);

// per-player HUD nodes: the list range each piece drew into (view | ring << 3). Entering one, its through-mode
// vertices are measured first and the GPU places the whole piece in the view from that box (0xFD/0xFE/0xFF)
struct HudNode { uint32_t lo, hi; int code; long at; };
static HudNode hud_nodes[2][64];
static int hud_nn[2];
static bool hud_on = false;
static uint32_t hud_lo = 0, hud_hi = 0;
void psp_ge_hud_node(int slot, uint32_t lo, uint32_t hi, int code) {
    if (slot < 0 || slot > 1) return;
    if (code < 0) { hud_nn[slot] = 0; return; }
    if (hi > lo && hud_nn[slot] < 64) hud_nodes[slot][hud_nn[slot]++] = {lo & 0x0FFFFFFC, hi & 0x0FFFFFFC, code, lists};
}

static void hud_scan(uint32_t lo, uint32_t hi, uint32_t base, float* bb) {
    static const int tsz[4] = {0, 1, 2, 4};
    uint32_t pc = lo, va = 0, ia = 0, vt = 0;
    bb[0] = bb[2] = 1e9f; bb[1] = bb[3] = -1e9f;
    for (int g = 0; g < 20000 && pc >= lo && pc < hi; g++) {
        uint32_t w = psp_l32(pc), arg = w & 0xFFFFFF;
        int c = (int)(w >> 24);
        pc += 4;
        if (c == 0x10) base = (arg << 8) & 0xFF000000;
        else if (c == 0x01) va = (base | arg) & 0x0FFFFFFF;
        else if (c == 0x02) ia = (base | arg) & 0x0FFFFFFF;
        else if (c == 0x12) vt = arg;
        else if (c == 0x08) pc = (base | arg) & 0x0FFFFFFC;
        else if (c == 0x0A || c == 0x0B || c == 0x0C) break;
        else if (c == 0x04) {
            int n = (int)(arg & 0xFFFF), it = (vt >> 11) & 3, ps = tsz[(vt >> 7) & 3];
            int off = 0, maxa = 1, po;
            auto add = [&](int k, int sz) { if (!sz) return; off = (off + sz - 1) / sz * sz; off += k * sz; if (sz > maxa) maxa = sz; };
            int wt = (vt >> 9) & 3, col = (vt >> 2) & 7;
            if (wt) add(((vt >> 14) & 7) + 1, tsz[wt]);
            add(2, tsz[vt & 3]);
            add(1, col == 7 ? 4 : col >= 4 ? 2 : 0);
            add(3, tsz[(vt >> 5) & 3]);
            off = ps ? (off + ps - 1) / ps * ps : off;
            po = off;
            add(3, ps);
            int st = (off + maxa - 1) / maxa * maxa;
            if ((vt & 0x800000) && (ps == 2 || ps == 4) && va)
                for (int i = 0; i < n && i < 4096; i++) {
                    int k = it == 1 ? psp_l8(ia + i) : it == 2 ? psp_l16(ia + 2 * i) : i;
                    uint32_t a = va + (uint32_t)(k * st + po);
                    float x, y;
                    if (ps == 2) { x = (int16_t)psp_l16(a); y = (int16_t)psp_l16(a + 2); }
                    else { uint32_t ux = psp_l32(a), uy = psp_l32(a + 4); memcpy(&x, &ux, 4); memcpy(&y, &uy, 4); }
                    if (x < -64.0f || x > 544.0f || y < -64.0f || y > 336.0f) continue;
                    if (x < bb[0]) bb[0] = x; if (x > bb[1]) bb[1] = x; if (y < bb[2]) bb[2] = y; if (y > bb[3]) bb[3] = y;
                }
            if (!it) va += (uint32_t)(n * st);
            else ia += (uint32_t)(n * (it == 1 ? 1 : 2));
        }
    }
}

// list ranges drawn for another player outside his HUD pass (the clash button of player 2): his button icons
void psp_gpu_icon_player(int k);
struct IconRange { uint32_t lo, hi; int k; long at; };
static IconRange ic_rng[8];
static int ic_n = 0;
static bool ic_on = false;
static uint32_t ic_lo = 0, ic_hi = 0;
void psp_ge_icon_range(uint32_t lo, uint32_t hi, int k) {
    lo &= 0x0FFFFFFC; hi &= 0x0FFFFFFC;
    if (hi <= lo) return;
    int j = 0;
    for (int i = 0; i < ic_n; i++) if (lists - ic_rng[i].at <= 6) ic_rng[j++] = ic_rng[i];
    ic_n = j;
    if (ic_n < 8) ic_rng[ic_n++] = {lo, hi, k, lists};
}
static void icon_check() {
    if (ic_on && (L.pc < ic_lo || L.pc >= ic_hi)) { psp_gpu_icon_player(-1); ic_on = false; }
    if (ic_on) return;
    for (int i = 0; i < ic_n; i++) {
        const IconRange& r = ic_rng[i];
        if (L.pc < r.lo || L.pc >= r.hi || lists - r.at > 6) continue;
        psp_gpu_icon_player(r.k); ic_on = true; ic_lo = r.lo; ic_hi = r.hi;
        return;
    }
}

static void hud_check() {
    if (hud_on && (L.pc < hud_lo || L.pc >= hud_hi)) { psp_gpu_cmd(0xFD, 0); hud_on = false; }
    if (hud_on) return;
    for (int s = 0; s < 2; s++)
        for (int i = 0; i < hud_nn[s]; i++) {
            const HudNode& h = hud_nodes[s][i];
            if (L.pc != h.lo || lists - h.at > 6) continue;
            float bb[4];
            hud_scan(h.lo, h.hi, L.base, bb);
            auto pk = [](float a, float b) { int x = (int)a + 1024, y = (int)b + 1024; x = x < 0 ? 0 : x > 4095 ? 4095 : x; y = y < 0 ? 0 : y > 4095 ? 4095 : y; return (uint32_t)(x | (y << 12)); };
            if (bb[0] > bb[1]) { bb[0] = bb[2] = 0; bb[1] = bb[3] = -1; }
            psp_gpu_cmd(0xFE, pk(bb[0], bb[1]));
            psp_gpu_cmd(0xFF, pk(bb[2], bb[3]));
            psp_gpu_cmd(0xFD, (uint32_t)(0x100 | h.code));
            hud_on = true; hud_lo = h.lo; hud_hi = h.hi;
            return;
        }
}

void psp_gpu_nop_pc(uint32_t pc);
#if TTT_PERF
// TTT_PICO_GE=1: the pico lines split the ge zone by command class
static int ge_sub_on() { static int v = -1; if (v < 0) { const char* e = getenv("TTT_PICO_GE"); v = e && *e ? atoi(e) : 0; } return v; }
static void ge_cmd_t(int c, uint32_t arg) {
    static int ids[3] = {perf_sub_id("ge: primitivas"), perf_sub_id("ge: marcas del port"), perf_sub_id("ge: estado")};
    uint64_t t = perf_clock();
    psp_gpu_cmd(c, arg);
    perf_sub_add(ids[c == 0x04 ? 0 : c >= 0xE0 ? 1 : 2], perf_clock() - t);
}
#define GE_CMD(c, a) do { if (ge_sub_on()) ge_cmd_t(c, a); else psp_gpu_cmd(c, a); } while (0)
#else
#define GE_CMD(c, a) psp_gpu_cmd(c, a)
#endif
static void ge_advance() {
    if (!L.live) return;
    PERF(PZ_GE);
    psp_gpu_split_time(1);
    long before = commands;
    for (long i = 0; i < 2000000; i++) {
        if (L.stop && L.pc == (L.stop & 0x0FFFFFFC)) break;
        if (split_cur >= 5 || hud_on) hud_check();
        if (ic_n) icon_check();
        uint32_t w = psp_l32(L.pc);
        int c = w >> 24;
        uint32_t arg = w & 0xFFFFFF;
        count_n[c]++;
        commands++;
        if (count_n[c] == 1) first_arg[c] = arg;
        if (c == 0x14) { psp_gpu_origin(L.pc); L.pc += 4; continue; }   // ORIGIN: addresses relative to this command
        L.pc += 4;
        if (c == 0x10) { L.base = (arg << 8) & 0xFF000000; psp_gpu_cmd(c, arg); continue; }
        if (c == 0x04 && split_dbg_lists > 0) { bool d2 = (split_vt >> 23) & 1; if (L.sp) { if (d2) split_p2++; else split_p3++; } else { if (d2) split_d2++; else split_d3++; } }
        if (c == 0x12) split_vt = arg;
        if (c == 0x04) { prim_type[(arg >> 16) & 7]++; psp_gpu_prim_addr(L.pc - 4); GE_CMD(c, arg); continue; }
        if (c == 0x12) {
            bool created = true;
            for (int k = 0; k < n_vtype; k++) if (vtypes[k] == arg) { vtype_seen[k]++; created = false; break; }
            if (created && n_vtype < 64) { vtypes[n_vtype] = arg; vtype_seen[n_vtype] = 1; n_vtype++; }
            psp_gpu_cmd(c, arg);
            continue;
        }
        if (c == 0x08) { L.pc = (L.base | arg) & 0x0FFFFFFC; continue; }
        if (c == 0x0A) {
            if (L.sp < 32) L.stack[L.sp++] = L.pc;
            L.pc = (L.base | arg) & 0x0FFFFFFC;
            int p = split_of(L.pc);
            if (L.sp == 1 && p > 0 && p <= 4 && p != split_cur) psp_gpu_cmd(0xFC, (uint32_t)split_skip[split_slot]);
            if (L.sp == 1 && p >= 0) split_set(p);
            if (split_dbg_lists > 0 && L.sp <= 2 && split_dbg_lines < 600) { split_dbg_lines++; printf("GE split: %*scall 0x%08X pasada %d\n", 2 * L.sp, "", L.pc, split_cur); }
            continue;
        }
        if (c == 0x0B) {
            if (L.sp > 0) {
                L.pc = L.stack[--L.sp];
                if (split_dbg_lists > 0 && L.sp == 0 && split_dbg_lines < 600) { split_dbg_lines++; printf("GE split:   prims 3d %ld 2d %ld\n", split_p3, split_p2); split_p3 = split_p2 = 0; }
            } else { L.live = false; break; }
            continue;
        }
        if (c == 0x0C) { L.live = false; break; }
        if (c == 0) psp_gpu_nop_pc(L.pc - 4);
        GE_CMD(c, arg);
    }
    psp_gpu_flush();
    psp_gpu_split_time(0);
    if (commands > before) lists_nonempty++;
    PERF_N(PC_CMD, commands - before);
}

void psp_gpu_tex_memo_bump();
void psp_ge_list(uint32_t start, uint32_t stop) {
    if (L.live) { L.stop = 0; ge_advance(); }   // the previous list finishes before another starts
    psp_gpu_tex_memo_bump();
    lists++;
    L.pc = start & 0x0FFFFFFC;
    L.stop = stop;
    L.base = 0;
    L.sp = 0;
    L.live = true;
    psp_gpu_cmd(0x00, 0xE40000u);
    split_set(0);
    if (split_dbg_lists > 0) {
        if (split_dbg_lines < 600) { split_dbg_lines++; printf("GE split: lista 0x%08X, antes directos 3d %ld 2d %ld\n", start, split_d3, split_d2); }
        split_d3 = split_d2 = 0;
        split_dbg_lists--;
    }
    ge_advance();
}

void psp_ge_stall(uint32_t stop) {
    psp_gpu_tex_memo_bump();
    L.stop = stop;
    ge_advance();
}

void psp_ge_finish() {
    if (!L.live) return;
    L.stop = 0;
    ge_advance();
}

void psp_ge_report() {
    set_names();
    printf("\nGE: %llu listas (%llu con contenido), %llu comandos\n", (unsigned long long)lists, (unsigned long long)lists_nonempty, (unsigned long long)commands);
    int used = 0;
    for (int c = 0; c < 256; c++) if (count_n[c]) used++;
    printf("comandos distintos: %d\n", used);
    for (int k = 0; k < 60; k++) {
        int best = -1;
        for (int c = 0; c < 256; c++) if (count_n[c] && (best < 0 || count_n[c] > count_n[best])) best = c;
        if (best < 0) break;
        printf("  0x%02X %-16s x%-10llu primer arg 0x%06X\n", best,
               nombre(best) ? nombre(best) : "?", (unsigned long long)count_n[best], first_arg[best]);
        count_n[best] = 0;
    }
    static const char* PRIM[8] = {"puntos", "lineas", "tira de lineas", "triangulos",
                                  "tira de triangulos", "abanico", "sprites", "?"};
    printf("primitivas:\n");
    for (int i = 0; i < 8; i++) if (prim_type[i]) printf("  %-18s x%llu\n", PRIM[i], (unsigned long long)prim_type[i]);
    printf("formatos de vertice (VTYPE): %d distintos\n", n_vtype);
    for (int k = 0; k < n_vtype && k < 20; k++)
        printf("  0x%06X x%llu\n", vtypes[k], (unsigned long long)vtype_seen[k]);
}
