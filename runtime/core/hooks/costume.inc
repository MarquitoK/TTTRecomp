// costume break (item 30), BT3 rule (decomp btl_scene.c, btl_act_f.c, btl_char_coll_b.c) on TTT's own leftovers of it.
// Only colours with a 1_p_dmg folder take part: without it nothing differs from TTT. Damaged = gauge variant (+0x1A58)
// - finishing technique: the victim of a technique with attribute 0x80000 (not a throw) loses life -> held flag +0xEA3
//   -> action 0x113 (BT3 0x104), whose first frame gets back BT3's "variant 0: reload as variant 1"
// - throws: 0x0891DF68 (BT3 BtlColl_StartThrow) zeroes +0xBF6/+0xBF7 (BT3 thr.unk48/unk4C); they get BT3's values back
// - the change loader asks the model index with the request's variant (job +0x2C) and gets the colour's damaged pak
bool psp_disc_dmg(uint32_t idx, uint32_t* out);
uint32_t psp_disc_dmg_base(uint32_t idx);
bool psp_col_extra(uint32_t first, uint32_t colour, uint32_t* out);
uint32_t psp_col_extra_base(uint32_t idx);
static uint32_t cs_comb(uint32_t slot) {
    uint32_t t = psp_l32(0x08A7DBC8u);
    if (!guest_ptr(t)) return 0;
    uint32_t n = psp_l32(t), b = psp_l32(t + 4);
    if (!n || n > 4 || !guest_ptr(b)) return 0;
    for (uint32_t k = 0; k < n; k++) { uint32_t f = b + 0x1A90u * k; if (psp_l32(f + 0xC) && psp_l32(f + 0x2C) == slot) return f; }
    return 0;
}
static int cs_class(uint32_t a) { return a == 0x114u ? 4 : (a >= 0x115u && a < 0x14Bu) ? (int)((a - 0x115u) % 3u) + 2 : -1; }
static uint32_t cs_tech(uint32_t f, int cls, bool* thr) {
    uint32_t o = psp_l32(0x08B61178u + 4u * (psp_l32(f + 0x2C) & 3u)), t = guest_ptr(o) ? psp_l32(o + 0x92C) : 0;
    *thr = false;
    if (!guest_ptr(t) || cls < 2) return 0;
    uint32_t s = (uint32_t)cls - 2u;
    for (uint32_t r : {0x141u, 0x144u, 0x147u, 0x14Au}) if (psp_l8(t + r + s) == 0x22) *thr = true;
    return psp_l32(t + 0xC + 4u * s);
}
// PACKFILE index of the fighter's current character and colour (member table +0x9D0, stride 0xA8: +0xC character, +0x10 colour)
static uint32_t cs_index(const Ctx& c, uint32_t f) {
    uint32_t m = psp_l32(f + 0x9D0);
    if (!guest_ptr(m)) return 0;
    m += 0xA8u * psp_l32(f + 8);
    return hud_guest(c, 0x089C6CA0u, psp_l32(m + 0xC), psp_l32(m + 0x10)) & 0xFFFFu;
}
static bool cs_damaged(uint32_t f) { return psp_l8(f + 0x1A58) != 0; }
static bool cs_has(const Ctx& c, uint32_t f) { uint32_t d; return psp_disc_dmg(cs_index(c, f), &d); }

// TTT's finishing-technique scan (0x0887CAB8) does not reach 0x0898EFB4 in normal battles: the rule runs at the hit, in the
// battle logic tick (clash manager, the same frame on every machine)
struct CsSt { int32_t life = -1; bool armed = false, lv1 = false; };
static CsSt cs_f[4];
static uint32_t cs_table = 0, cs_frame = 0;
static int cs_logs = 0;
static bool cs_lv1_slot[8];
static uint32_t cs_thr_atk[4];   // victim in a throw: its attacker, while it stays in a technique (the throw breaks by itself)
// level 1 (textures only, 1_p/011_textures_dmg.*): life at or below TTT_DMG1_LIFE percent of the maximum (+0x1A38), default 50
static int cs_lv1_pct() {
    static int v = 50, n = 0;
    if ((n++ & 1023) == 0) { const char* e = getenv("TTT_DMG1_LIFE"); v = e && *e ? atoi(e) : 50; }
    return v;
}
static int costume_lv1(int slot) {
    if (slot < 0 || slot >= 8 || !cs_lv1_slot[slot]) return 0;
    uint32_t f = cs_comb((uint32_t)slot);
    return f && !cs_damaged(f) ? 1 : 0;
}
// costume chosen in the select per player row (0 normal, 1 level 1, 2 level 2); TTT_DMG_SEL=a,b,c,d for tests
bool psp_disc_lv1(uint32_t idx);
std::string psp_disc_key(uint32_t idx);
static int cs_sel[4];
static uint32_t cs_ent[4];   // entry the select chose it for (0 = any: the test variable)
static bool cs_sel_env = false;
static void cs_sel_load() {
    if (cs_sel_env) return;
    cs_sel_env = true;
    const char* e = getenv("TTT_DMG_SEL");
    for (int i = 0; e && *e && i < 4; i++) { cs_sel[i] = atoi(e) & 3; while (*e && *e != ',') e++; if (*e) e++; }
}
static int cs_row(uint32_t team, uint32_t member) {
    if (team > 1 || member > 1) return -1;
    int r = (int)psp_l32(0x08B48A20u + team * 8u + member * 4u + 0x464u);
    return r >= 0 && r < 4 ? r : -1;
}
bool net_online_ranked();
static int cs_level(int row, uint32_t idx) {
    if (row < 0 || net_online_ranked()) return 0;   // ranked: no damaged costume from the select
    uint32_t d = 0;
    if (cs_ent[row] && cs_ent[row] != idx && !(psp_disc_dmg(cs_ent[row], &d) && d == idx)) return 0;
    return cs_sel[row];
}
// battle start member models (0x08855D4C -> 0x089AE388): v0 still holds the setup member 0x08858698(team, member) at the model index call
static uint32_t cs_start_dmg(uint32_t ra, uint32_t mem, uint32_t e) {
    if (ra != 0x089AE3C8u && ra != 0x089AE3B4u) return e;
    uint32_t off = mem - 0x08B48AE8u, t = off / 0x1D0u, m = (off % 0x1D0u) / 0xD8u;
    if (mem < 0x08B48AE8u || t > 1 || m > 1 || (off % 0x1D0u) % 0xD8u) return e;
    cs_sel_load();
    int row = cs_row(t, m);
    uint32_t d = 0;
    if (cs_level(row, e & 0xFFFFu) != 2 || !psp_disc_dmg(e & 0xFFFFu, &d)) return e;
    static int logs = 0;
    if (logs++ < 20) printf("costume: inicio con traje danado nivel 2, equipo %u miembro %u jugador %d, indice %u -> %u\n", t, m, row + 1, e & 0xFFFFu, d);
    return d;
}
static void cs_flag(uint32_t f) {
    uint8_t now = psp_l8(f + 0xDD4);
    if (now < psp_l8(f + 0x152B)) psp_s8(f + 0x11E7, psp_l8(f + 0xEA3));
    psp_s8(f + 0xEA3, 1);
    psp_s8(f + 0x152B, now);
}
static bool cs_breaking(uint32_t f) { return psp_l8(f + 0xEA3) || psp_l32(f + 0x984) == 0x113u || psp_l32(f + 0x988) == 0x113u; }
// armed = this technique already decided for this victim; it clears when no enemy is in a 0x80000 technique
static void costume_tick(const Ctx& c) {
    uint32_t t = psp_l32(0x08A7DBC8u), vb = psp_l32(0x08A78E14u);
    if (!guest_ptr(t)) return;
    uint32_t n = psp_l32(t), b = psp_l32(t + 4);
    if (!n || n > 4 || !guest_ptr(b)) return;
    static bool start = false;
    if (t != cs_table) { for (auto& x : cs_f) x = CsSt(); for (auto& x : cs_lv1_slot) x = false; for (auto& x : cs_thr_atk) x = 0; cs_table = t; start = true; }
    if (vb - cs_frame > 60) {   // a new battle starts with every fighter at full life (a pause in battle does not)
        bool full = true;
        for (uint32_t k = 0; k < n; k++) { uint32_t f = b + 0x1A90u * k; if (psp_l32(f + 0xC) && psp_l32(f + 0x1A34) != psp_l32(f + 0x1A38)) full = false; }
        if (full) { for (auto& x : cs_f) x.lv1 = false; for (auto& x : cs_lv1_slot) x = false; start = true; }
    }
    if (start) {   // the costume chosen in the select: level 2 is already damaged (no break), level 1 starts with its textures
        cs_sel_load();
        int done = 0;
        uint32_t seen[2] = {0, 0};
        for (uint32_t k = 0; k < n; k++) {
            uint32_t f = b + 0x1A90u * k;
            if (!psp_l32(f + 0xC)) continue;
            done++;
            uint32_t team = psp_l32(f + 4) & 1u;
            int row = cs_row(team, seen[team]++);
            uint32_t idx = cs_index(c, f), d = 0;
            int lv = cs_level(row, idx);
            bool has2 = psp_disc_dmg(idx, &d);
            if (lv == 2 && has2) { psp_s8(f + 0x1A58, 1); psp_s8(f + 0x16FC, 1); }
            if (lv == 1 && !cs_damaged(f)) { cs_f[k].lv1 = true; cs_lv1_slot[psp_l32(f + 0x2C) & 7u] = true; }
            static int logs = 0;
            if (logs++ < 40) printf("costume: inicio luchador %u equipo %u jugador %d indice %u (%s) traje %d%s\n", k, team, row + 1, idx, psp_disc_key(idx).c_str(), lv, lv == 2 && !has2 ? " (sin traje danado, normal)" : "");
        }
        if (done) start = false;
    }
    cs_frame = vb;
    for (uint32_t k = 0; k < n; k++) {
        uint32_t f = b + 0x1A90u * k;
        int32_t life = (int32_t)psp_l32(f + 0x1A34), was = cs_f[k].life;
        cs_f[k].life = life;
        int32_t mx = (int32_t)psp_l32(f + 0x1A38), pct = cs_lv1_pct();
        if (!cs_f[k].lv1 && was >= 0 && life < was && pct > 0 && mx > 0 && psp_l32(f + 0xC) && !cs_damaged(f) && (int64_t)life * 100 <= (int64_t)mx * pct) {
            cs_f[k].lv1 = true;
            cs_lv1_slot[psp_l32(f + 0x2C) & 7u] = true;
            if (cs_logs++ < 60) printf("costume: luchador %u (indice %u) rotura nivel 1, vida %d de %d\n", k, cs_index(c, f), life, mx);
        }
        uint32_t team = psp_l32(f + 4) & 1u, by = 0xFFu, fl = 0;
        for (uint32_t a = 0; a < n && by == 0xFFu; a++) {
            uint32_t fa = b + 0x1A90u * a;
            int cls = cs_class(psp_l32(fa + 0x984));
            bool thr = false;
            if (a == k || (psp_l32(fa + 4) & 1u) == team || cls < 2) continue;
            fl = cs_tech(fa, cls, &thr);
            if ((fl & 0x80000) && !thr) by = a;
        }
        if (cs_thr_atk[k] && cs_class(psp_l32(cs_thr_atk[k] + 0x984)) < 2) cs_thr_atk[k] = 0;
        if (cs_thr_atk[k]) { cs_f[k].armed = true; continue; }
        if (by == 0xFFu) {
            cs_f[k].armed = false;
            continue;
        }
        if (was < 0 || life >= was || cs_f[k].armed || !psp_l32(f + 0xC)) continue;
        cs_f[k].armed = true;
        uint32_t d = 0;
        if (!cs_damaged(f) && !cs_breaking(f) && psp_disc_dmg(cs_index(c, f), &d)) cs_flag(f);
    }
}

// item 64 stage 3: a fighter whose pak was reloaded (Ctrl+R) goes through the change queue like action 0x113 (0x08972808..0x089728DC):
// RequestChara with its current member, IsLoadedFor -> SetReady, next tick SetDone. Waits while it is in a technique or the queue is busy
int psp_disc_reload_take(uint32_t* out, int max);
static int rlf_logs = 0;
struct RlF { int st = 0; uint32_t t0 = 0, idx = 0; };
// the bar portrait comes from the fighter's pak in memory (entries 0x2F/0x30): after a reload it points at the freed copy
static void rlf_portrait(const Ctx& c, uint32_t f) {
    if (!hud_mgr) return;
    for (int k = 0; k < 4; k++) {
        uint32_t o = psp_l32(hud_mgr + 0x74 + 4 * k);
        if (!o || psp_l32(o + 0x6C) != f) continue;
        int s = hud_cpu_slot(o);
        if (s >= 0) hud_unload_portrait(c, s);
        else if (psp_l32(o + 0x9C)) { hud_guest(c, 0x089D248Cu, psp_l32(o + 0x9C), 3); psp_s32(o + 0x9C, 0); }
        hud_portrait(c, o);
        hud_attach_portrait(c, o);
        hud_tries[k] = 0;
        hud_virtual(c, o, 0xC8);
        if (rlf_logs < 40) { rlf_logs++; printf("recarga: retrato de la barra %d recargado\n", k); }
    }
}
static RlF rlf[4];
static std::vector<uint32_t> rlf_pend;
static uint32_t rlf_table = 0;
static void rlf_tick(const Ctx& c) {
    uint32_t t = psp_l32(0x08A7DBC8u), vb = psp_l32(0x08A78E14u);
    uint32_t got[64];
    int m = psp_disc_reload_take(got, 64);
    for (int i = 0; i < m; i++) if (std::find(rlf_pend.begin(), rlf_pend.end(), got[i]) == rlf_pend.end()) rlf_pend.push_back(got[i]);
    if (!guest_ptr(t)) return;
    uint32_t n = psp_l32(t), b = psp_l32(t + 4), q = t + 0x198;
    if (!n || n > 4 || !guest_ptr(b)) return;
    if (t != rlf_table) { for (auto& x : rlf) x = RlF(); rlf_table = t; }
    if (rlf_pend.empty() && !rlf[0].st && !rlf[1].st && !rlf[2].st && !rlf[3].st) return;
    std::vector<uint32_t> keep;
    for (uint32_t k = 0; k < n; k++) {
        uint32_t f = b + 0x1A90u * k;
        if (!psp_l32(f + 0xC)) continue;
        RlF& r = rlf[k];
        uint32_t pl = psp_l32(f);
        if (r.st == 0) {
            if (rlf_pend.empty()) continue;
            uint32_t idx = cs_index(c, f);
            if (std::find(rlf_pend.begin(), rlf_pend.end(), idx) == rlf_pend.end()) continue;
            bool busy = cs_class(psp_l32(f + 0x984)) >= 2 || cs_breaking(f) || psp_l32(q + 0x244) != psp_l32(q + 0x248) || psp_l8(q + 0x24C);
            if (busy) { keep.push_back(idx); continue; }
            uint32_t mb = psp_l32(f + 0x9D0) + 0xA8u * psp_l32(f + 8);
            psp_s32(f + 0x16EC, psp_l32(mb + 0xC)); psp_s32(f + 0x16F0, psp_l32(mb + 0x10)); psp_s8(f + 0x16FC, psp_l8(f + 0x1A58));
            hud_guest(c, 0x08935F08u, f);
            Ctx c2 = c;
            c2.r[4] = pl; c2.r[5] = psp_l32(f + 0x16EC); c2.r[6] = psp_l32(f + 0x16F0); c2.r[7] = psp_l8(f + 0x16FC);
            c2.r[8] = psp_l32(f + 0x1700); c2.r[9] = psp_l32(f + 0x1704); c2.r[10] = psp_l32(f + 0x1708); c2.r[31] = 0;
            psp_call(c2, 0x08933D80u);
            r.st = 1; r.t0 = vb; r.idx = idx;
            if (rlf_logs++ < 40) printf("recarga: luchador %u (indice %u) pedido al cargador, cola %u/%u estado %u\n", k, idx, psp_l32(q + 0x244), psp_l32(q + 0x248), psp_l8(q + 0x24C));
        } else if (r.st == 1) {
            if (hud_guest(c, 0x08933E90u, pl)) {
                hud_guest(c, 0x089360E4u, f);
                hud_guest(c, 0x08933EC4u, pl);
                Ctx c2 = c;
                c2.r[4] = f; c2.r[5] = psp_l32(f + 0x9B0); c2.f[12].f = 0.0f; c2.r[31] = 0;
                psp_call(c2, 0x08915ACCu);
                r.st = 2;
                if (rlf_logs++ < 40) printf("recarga: luchador %u cargado en %u cuadros\n", k, vb - r.t0);
            } else if (vb - r.t0 > 600) {
                hud_guest(c, 0x08933EF4u, pl);
                r.st = 0;
                if (rlf_logs++ < 40) printf("recarga: luchador %u sin respuesta del cargador, cancelado (cola %u/%u estado %u)\n", k, psp_l32(q + 0x244), psp_l32(q + 0x248), psp_l8(q + 0x24C));
            }
        } else if (r.st == 3) {
            rlf_portrait(c, f);
            r.st = 0;
        } else if (r.st == 2) {
            hud_guest(c, 0x08933EF4u, pl);
            r.st = 3;
            if (rlf_logs++ < 40) printf("recarga: luchador %u recargado (indice %u), %u cuadros en total\n", k, r.idx, vb - r.t0);
        }
    }
    rlf_pend.swap(keep);
}

static bool costume_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&)) {
    if (dir == 0x0893BD50u) { rlf_tick(c); costume_tick(c); planet_tick(c); pl_key(c); return false; }
    if (dir == 0x0898EFB4u) {   // a0 attacker object id, a1 target object id
        uint32_t a = cs_comb(c.r[4]), b = cs_comb(c.r[5]);
        c.r[2] = 0;
        if (!a || !b || cs_damaged(b)) return true;
        int cls = cs_class(psp_l32(a + 0x984));
        bool thr = false;
        if ((cs_tech(a, cls, &thr) & 0x80000) && !thr && cs_has(c, b)) c.r[2] = 1;
        return true;
    }
    if (dir == 0x08972760u) {   // action 0x113, a1 = call mode (0 entry)
        uint32_t f = c.r[4], mode = c.r[5];
        orig(c);
        if (mode == 0 && !cs_damaged(f) && cs_has(c, f)) {
            psp_s32(f + 0x400, psp_l32(f + 0x400) | 1u);
            hud_guest(c, 0x08949998u, f);
        }
        return true;
    }
    if (dir == 0x0891DF68u) {   // throw start: a0 attacker, a1 victim, a3 technique attributes; v0 = 1 when it started
        uint32_t atk = c.r[4], def = c.r[5], fl = c.r[7];
        orig(c);
        if (!c.r[2] || !guest_ptr(atk) || !guest_ptr(def)) return true;
        {
            uint32_t t = psp_l32(0x08A7DBC8u), b = guest_ptr(t) ? psp_l32(t + 4) : 0;
            if (b && def >= b && (def - b) % 0x1A90u == 0 && (def - b) / 0x1A90u < 4) cs_thr_atk[(def - b) / 0x1A90u] = atk;
        }
        uint8_t u48 = (fl & 0x2000) && !cs_damaged(atk) && cs_has(c, atk), u4c = (fl & 0x80000) && !cs_damaged(def) && cs_has(c, def);
        psp_s8(atk + 0xBF6, u48); psp_s8(def + 0xBF6, u48);
        psp_s8(atk + 0xBF7, u4c); psp_s8(def + 0xBF7, u4c);
        return true;
    }
    if (dir == 0x089C6CA0u) {   // model index; from the change loader the job (s0) carries the variant at +0x2C
        uint32_t ra = c.r[31], job = c.r[16], col = c.r[5], sid = c.r[4], mem = c.r[2];
        if (col < 16) c.r[5] = 0;   // menu colour k -> the k-th colour that exists (1..4 and the extra 5..10)
        orig(c);
        if (col < 16) {
            uint32_t first = ro_model(c.r[2] & 0xFFFFu), e = first;
            bool ok = psp_col_extra(first, col, &e);
            if (!ok) e = col < 4 ? (c.r[2] & 0xFFFFu) + col : first;
            c.r[2] = e;
            static uint32_t last = 0;
            static int logs = 0;
            if (e != last && logs++ < 40) printf("color: personaje %u color de menu %u -> entrada %u%s\n", sid, col + 1, e, ok ? "" : " (sin lista, original)");
            last = e;
        }
        c.r[2] = cs_start_dmg(ra, mem, c.r[2]);
        uint32_t d = 0;
        if (ra == 0x08855448u && guest_ptr(job) && psp_l8(job + 0x2C) == 1 && psp_disc_dmg(c.r[2] & 0xFFFFu, &d)) {
            static int logs = 0;
            if (logs++ < 40) printf("costume: traje danado cargado, indice %u -> %u\n", c.r[2] & 0xFFFFu, d);
            c.r[2] = d;
        }
        return true;
    }
    if (dir == 0x089C6D60u && c.r[5] < 16) {   // the same model index from the loader's other path
        uint32_t col = c.r[5], sid = c.r[4], ra = c.r[31], mem = c.r[2];
        c.r[5] = 0;
        orig(c);
        uint32_t first = ro_model(c.r[2] & 0xFFFFu), e = first;
        bool ok = psp_col_extra(first, col, &e);
        if (!ok) e = col < 4 ? (c.r[2] & 0xFFFFu) + col : first;
        c.r[2] = e;
        static uint32_t last = 0;
        static int logs = 0;
        if (e != last && logs++ < 40) printf("color: personaje %u color de menu %u -> entrada %u%s (carga)\n", sid, col + 1, e, ok ? "" : " (sin lista, original)");
        last = e;
        c.r[2] = cs_start_dmg(ra, mem, c.r[2]);
        return true;
    }
    if (dir == 0x089C6DB4u) {   // index -> character id: a damaged pak answers as its colour
        uint32_t i = c.r[4] & 0xFFFFu, b = psp_col_extra_base(psp_disc_dmg_base(i));
        if (b != i) c.r[4] = b;
        orig(c);
        return true;
    }
    return false;
}
