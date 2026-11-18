// colour selector of the character select (Extra 10 colours): colour row (AGS 37 states 3/4) and damaged costume row (5/6), texts drawn by the port
#include "patches/colour_select.h"

static uint32_t cs_mem = 0;
static int cs_ready = -1;   // the AGS has the COLOR_TEXT slot (asset patch char_select)
static uint64_t cs_shown_t = 0;   // last draw of the colour selector while visible (the 3D of the select follows the colour then)
static int cs_dmg_ready = -1;   // row 2 has the DMG_TEXT slot (char_select with the costume row)
static int cs_focus[4], cs_av[4];   // per player row: focused row (0 colour, 1 costume); costumes of the colour shown (bit 1 level 1, bit 2 level 2)
static bool cs_two[4];
static int cs_menu[4] = {-1, -1, -1, -1};   // character the costume of a row was chosen for
static int s3d_sys_of_menu(int n);

static uint32_t cs_str(const char* s, int k) {
    if (!cs_mem) cs_mem = psp_reserve(0x300);
    uint32_t a = cs_mem + 0x200 + 0x20 * k;
    memcpy(psp_p(a), s, strlen(s) + 1);
    return a;
}

static uint32_t cs_utf16(const char* src, int k = 0) {
    if (!cs_mem) cs_mem = psp_reserve(0x300);
    uint32_t a = cs_mem + 0x100 + 0x80 * k;
    int n = 0;
    for (const unsigned char* p = (const unsigned char*)src; *p && n < 60; n++) {
        uint32_t u = *p++;
        if (u >= 0xC0 && *p) u = ((u & 0x1F) << 6) | (*p++ & 0x3F);
        psp_s16(a + 2 * n, u);
    }
    psp_s16(a + 2 * n, 0);
    return a;
}

static uint32_t cs_text_slot(const Ctx& c, uint32_t obj, int row = 0) {
    uint32_t h = psp_l32(obj + 0x7C + 4 * row), rec = h ? psp_l32(h) : 0, scene = rec ? psp_l32(rec + 0x30) : 0;
    return scene ? mpm_call(c, 0x08840DC4u, scene, cs_str(row ? "DMG_TEXT" : "COLOR_TEXT", row)) : 0;
}
static uint32_t cs_record(const Ctx& c, uint32_t obj) {
    uint32_t o2 = psp_l32(obj + 0xC), e = o2 ? psp_l32(o2 + 0x40) + 0xA0 : 0;
    return e ? mpm_call(c, psp_l32(e + 4), o2 + (uint32_t)(int32_t)(int16_t)psp_l16(e), psp_l32(obj + 0x68)) : 0;
}
static int cs_av_of(const Ctx& c, int n, int col, uint32_t* ent) {
    static std::map<uint32_t, std::pair<int, uint32_t>> memo;
    uint32_t k = (uint32_t)(n & 0xFFFF) << 8 | (uint32_t)(col & 0xFF);
    auto it = memo.find(k);
    if (it == memo.end()) {
        int s = s3d_sys_of_menu(n);
        uint32_t e = s >= 0 && col >= 0 && col < 20 ? mpm_call(c, 0x089C6CA0u, (uint32_t)s, (uint32_t)col) & 0xFFFFu : 0, d = 0;
        int av = e ? (psp_disc_lv1(e) ? 2 : 0) | (psp_disc_dmg(e, &d) ? 4 : 0) : 0;
        it = memo.emplace(k, std::make_pair(av, e)).first;
    }
    *ent = it->second.second;
    return it->second.first;
}
// costumes of the character and colour under the selector; a choice the colour lacks goes back to normal
static void cs_avail(const Ctx& c, uint32_t obj, int p) {
    static int key[4][2] = {{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}};
    static uint32_t ent[4];
    uint32_t rec = cs_record(c, obj);
    if (!rec) return;
    int n = (int)psp_l32(rec + 4), col = (int)psp_l32(rec + 0xC);
    if (key[p][0] != n || key[p][1] != col) {
        key[p][0] = n; key[p][1] = col;
        int s = s3d_sys_of_menu(n);
        uint32_t e = s >= 0 && col >= 0 && col < 20 ? mpm_call(c, 0x089C6CA0u, (uint32_t)s, (uint32_t)col) & 0xFFFFu : 0, d = 0;
        ent[p] = e;
        cs_av[p] = e ? (psp_disc_lv1(e) ? 2 : 0) | (psp_disc_dmg(e, &d) ? 4 : 0) : 0;
        static int logs = 0;
        if (cs_av[p] && logs++ < 20) printf("colores: jugador %d, numero %d color %d: traje danado%s%s\n", p + 1, n, col + 1, cs_av[p] & 2 ? " 1" : "", cs_av[p] & 4 ? " 2" : "");
    }
}
static void cs_state(const Ctx& c, uint32_t h, int st, bool loop, bool force) {
    uint32_t rec = h ? au_record(h, st) : 0, cur = h ? psp_l32(h) : 0;
    if (!rec || (cur == rec && !force)) return;
    if (cur && cur != rec) mpm_wf(cur + 0x2C, 0.0f);
    psp_s32(h, rec);
    if (cur != rec) mpm_wf(rec + 0x2C, 0.0f);
    psp_s8(h + 0x14, loop ? 1 : 0);
    au_restart(c, rec, loop ? 1 : 0);
}
// row states by focus: the costume row only in the normal frame (state 1) and when the colour has a damaged costume
static void cs_apply(const Ctx& c, uint32_t obj, bool force) {
    int p = (int)psp_l32(obj + 0x68);
    if (p < 0 || p > 3) return;
    uint32_t h0 = psp_l32(obj + 0x78), h1 = psp_l32(obj + 0x7C), h2 = psp_l32(obj + 0x80);
    if (!force && (!h0 || !psp_l32(h0) || !h1 || !psp_l32(h1))) return;   // closed: the game dropped the states, the draw must not bring them back
    if (cs_dmg_ready < 0 && h2 && au_record(h2, 5)) {
        uint32_t r5 = au_record(h2, 5), sc = psp_l32(r5 + 0x30);
        cs_dmg_ready = sc && mpm_call(c, 0x08840DC4u, sc, cs_str("DMG_TEXT", 1)) ? 1 : 0;
        printf("colores: fila de traje danado %s\n", cs_dmg_ready ? "disponible (DMG_TEXT)" : "ausente (char_select sin la fila)");
    }
    uint32_t fr = h0 ? psp_l32(h0) : 0, sc = fr ? psp_l32(fr + 0x30) : 0;
    // frame slots of the two-row layout (both rows centred, animated with the frame); without them row 2 only in the normal frame
    uint32_t sa = sc ? mpm_call(c, 0x08840DC4u, sc, cs_str("30_dmg_01_on", 2)) : 0, sb = sa ? mpm_call(c, 0x08840DC4u, sc, cs_str("30_dmg_02_on", 3)) : 0;
    static int lay = -1;
    if (sc && lay != (sb ? 1 : 0)) { lay = sb ? 1 : 0; printf("colores: dos filas %s\n", lay ? "centradas (30_dmg_01_on, 30_dmg_02_on)" : "sin centrar (marco sin 30_dmg_0N_on)"); }
    bool two = cs_dmg_ready == 1 && !net_online_ranked() && (cs_av[p] & 6) && h1 && h2 && (sb ? true : fr == au_record(h0, 1) && psp_l32(h2 + 8));
    if (sb) {
        uint32_t s1 = two ? sa : mpm_call(c, 0x08840DC4u, sc, cs_str("30_color_01_on", 4));
        if (s1) psp_s32(h1 + 8, s1);
        if (two) psp_s32(h2 + 8, sb);
    }
    cs_two[p] = two;
    int f = two ? cs_focus[p] : 0;
    cs_state(c, h1, f ? 3 : 4, !f, force && !f);
    if (h2) { if (two) cs_state(c, h2, f ? 6 : 5, f, force && f); psp_s8(h2 + 0x15, two && psp_l8(h1 + 0x15) ? 1 : 0); }
    for (uint32_t o : {0x84u, 0x88u}) { uint32_t h = psp_l32(obj + o); if (h) psp_s8(h + 0x15, 0); }
}
static void cs_text(const Ctx& c, uint32_t font, uint32_t slot, const char* t, int k, bool dim) {
    uint32_t str = cs_utf16(t, k), col = dim ? 0xFF8C8C8Cu : 0xFFFFFFFFu;
    int w = (int)mpm_call(c, 0x08831F68u, font, str);
    mpm_call(c, 0x08832530u, font, (uint32_t)((int)psp_l32(slot + 0xE4) - w / 2), psp_l32(slot + 0xE8), str, col, col);
}

static bool cs_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&)) {
    if (!patch_colour_select.val) return false;
    if (dir == 0x08A06BB8u) {   // colour input: only left/right (as the game's up/down)
        uint32_t m = c.r[4], p = psp_l32(m + 0x80), r = psp_l32(m + 0x88);
        if (!psp_l32(m + 0x68) || p == 0xFFFFFFFFu || p > 7 || cs_ready == 0) return false;   // not drawn here yet (a rival's row in a net battle) still counts as the port's selector
        uint32_t pad = 0x08B4E990u + 0x58u * p, v = psp_l32(pad + 0x34);
        bool row2 = false;
        uint32_t rec = psp_l32(m + 0x68) + 0x1B4u + 0x18u * r, ent = 0;
        Ctx c0 = c;
        if (r < 4) {   // the same decisions on every machine: the selection record and the pad, never what this machine draws
            int n = (int)(psp_l32(rec + 4) & 0xFF), col = (int)psp_l32(rec + 0xC);
            if (cs_menu[r] != n) { cs_sel[r] = 0; cs_focus[r] = 0; cs_menu[r] = n; }   // another character starts normal (no clocks here: a net battle stalls frames)
            cs_av[r] = cs_av_of(c0, n, col, &ent);
            cs_two[r] = cs_dmg_ready != 0 && !net_online_ranked() && (cs_av[r] & 6);
            if (!cs_two[r]) cs_focus[r] = 0;
        }
        if (r < 4 && cs_two[r]) {   // up/down: row; left/right on the costume row: Normal / Damaged 1 / Damaged 2 of this colour
            if ((v & 8) && cs_focus[r]) { cs_focus[r] = 0; mpm_sfx(c0, 1, 3); }
            else if ((v & 4) && !cs_focus[r]) { cs_focus[r] = 1; mpm_sfx(c0, 1, 3); }
            else if (cs_focus[r] && (v & 3)) {
                int o[3], n = 0, i = 0;
                for (int l = 0; l < 3; l++) if (!l || (cs_av[r] & (1 << l))) { if (l == cs_sel[r]) i = n; o[n++] = l; }
                cs_sel[r] = o[(i + (v & 1 ? n - 1 : 1)) % n];
                mpm_sfx(c0, 1, 3);
                static int logs = 0;
                if (logs++ < 20) printf("colores: jugador %u, traje %d\n", r + 1, cs_sel[r]);
            }
            row2 = cs_focus[r] != 0;
        }
        uint32_t w2 = row2 ? v & ~0xFu : (v & ~0xCu) | (v & 1 ? 8u : 0u) | (v & 2 ? 4u : 0u);
        psp_s32(pad + 0x34, w2);
        orig(c);
        psp_s32(pad + 0x34, v);
        if (r < 4) {   // colour changed: a costume it lacks goes back to normal
            int av = cs_av_of(c0, (int)(psp_l32(rec + 4) & 0xFF), (int)psp_l32(rec + 0xC), &ent);
            if (cs_sel[r] && !(av & (1 << cs_sel[r]))) cs_sel[r] = 0;
            cs_ent[r] = ent;
            // confirm, cancel or customise leave the selector: the next opening starts on the colour row
            if (psp_l32(pad + 0x30) & (psp_l32(0x08A793D4u) | psp_l32(0x08A793D0u) | 0x1000u)) cs_focus[r] = 0;
        }
        return true;
    }
    if (dir == 0x089FC394u) {   // rows by colour: the colour row and the costume row by focus; the real colour stays cached
        uint32_t obj = c.r[4];
        Ctx c0 = c;
        int p = (int)psp_l32(obj + 0x68);
        if (c.r[5] == 0) cs_sel_load();   // the reset on opening is decided by the input of each row (same on every machine)
        if (cs_ready != 1) {
            orig(c);
            int r = cs_text_slot(c0, obj) ? 1 : 0;
            if (r != cs_ready) printf("colores: selector %s\n", r ? "de una fila (COLOR_TEXT)" : "original (char_select sin el parche)");
            cs_ready = r;
            if (!r) return true;
            c = c0;
        }
        uint32_t o2 = psp_l32(obj + 0xC), e = psp_l32(o2 + 0x40) + 0xA0;
        uint32_t rec = mpm_call(c0, psp_l32(e + 4), o2 + (uint32_t)(int32_t)(int16_t)psp_l16(e), psp_l32(obj + 0x68));
        if (!rec) return false;
        uint32_t col = psp_l32(rec + 0xC);
        psp_s32(rec + 0xC, 0);
        orig(c);
        psp_s32(rec + 0xC, col);
        psp_s32(obj + 0x8C, col);
        if (p >= 0 && p < 4) cs_avail(c0, obj, p);
        cs_apply(c0, obj, true);   // the focused row loops its arrows (rows are restarted once by the game)
        return true;
    }
    if (dir == 0x089FCC08u) {   // draw, then "Color N" centred on the slot origin
        uint32_t obj = c.r[4];
        Ctx c0 = c;
        int p = (int)psp_l32(obj + 0x68);
        bool pv = p >= 0 && p < 4;
        if (cs_ready == 1 && pv) { cs_avail(c0, obj, p); cs_apply(c0, obj, false); }
        orig(c);
        if (cs_ready != 1) return true;
        uint32_t h = psp_l32(obj + 0x7C), slot = cs_text_slot(c0, obj), font = psp_l32(0x08A78EECu);
        if (h && psp_l8(h + 0x15)) cs_shown_t = psp_time_us();
        if (!slot || !font || !h || !psp_l8(h + 0x15)) return true;
        // the texts only while the game itself shows the selector: not hidden, not closing or closed (the conditions of its own refresh)
        uint32_t ve = psp_l32(obj + 0x40) + 0x40, h0 = psp_l32(obj + 0x78);
        int a48 = (int)psp_l32(obj + 0x48), a50 = (int)psp_l32(obj + 0x50);
        bool hid = mpm_call(c0, psp_l32(ve + 4), obj + (uint32_t)(int32_t)(int16_t)psp_l16(ve)) != 0;
        bool open = !hid && a48 != 2 && a50 != 2 && a48 != -1 && a50 != -1 && h0 && psp_l32(h0) && psp_l8(h0 + 0x15);
        if (!open) return true;
        int li = mpm_li(mpm_lang(c0));
        bool two = pv && cs_two[p];
        static const char* W[2] = {"Color", "Color"};
        static const char* D[2][4] = {{"Normal", "Damaged 1", "Damaged 2", "Damaged"}, {"Normal", "Da\xC3\xB1" "ado 1", "Da\xC3\xB1" "ado 2", "Da\xC3\xB1" "ado"}};
        char b[32];
        snprintf(b, sizeof b, "%s %u", W[li], psp_l32(obj + 0x8C) + 1);
        mpm_call(c0, 0x08832448u, font);
        cs_text(c0, font, slot, b, 0, two && cs_focus[p]);
        uint32_t s2 = two ? cs_text_slot(c0, obj, 1) : 0;
        int lv = std::min(std::max(cs_sel[p], 0), 2);
        if (s2) cs_text(c0, font, s2, D[li][lv && (cs_av[p] & 6) != 6 ? 3 : lv], 1, !cs_focus[p]);   // one level only: just "Damaged"
        mpm_call(c0, 0x08832500u, font);
        return true;
    }
    return false;
}
