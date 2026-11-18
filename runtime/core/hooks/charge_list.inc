// Sparking Zero style charge list (item 17): a copy of the technique window class loading AGS 280 / RHG 130
#include <map>
int input_charge_held();
int input_wheel_on();
static void tw_tick(const Ctx& c, uint32_t f);
static void tw_load(const Ctx& c, uint32_t pak);
static void tw_free(const Ctx& c);
static void tw_draw(const Ctx& c, uint32_t ra);
static bool cl_split = false;   // split screen: the instance only carries player 1's transformation wheel, no list
static bool cl_list_on = true;   // the instance also carries the transformation wheel when only the wheel is on
uint32_t icons_text(uint32_t s);
static uint32_t cl_obj = 0, cl_mem = 0, cl_f = 0;
static bool cl_upd = false, cl_block = false, cl_gu_on = false;
static const int CL_SPEED = 2;
static float cl_sx = 1, cl_sy = 1, cl_ax = 0, cl_ay = 0, cl_ox = 0, cl_oy = 0;
static uint32_t cl_vt = 0x80011Eu;
static uint32_t cl_name[6], cl_key[6];
static int cl_rows = 5;
static float cl_h = 120.0f;
static int cl_spk = -1;
static const int CL_NOSPARK = 8;   // states 8..10: open, normal and close without the last row (ultimate)

static uint32_t cl_call(const Ctx& c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1) {
    Ctx c2 = c;
    c2.r[4] = a0; c2.r[5] = a1; c2.r[6] = a2; c2.r[7] = a3; c2.r[8] = t0; c2.r[9] = t1; c2.r[31] = 0;
    psp_call(c2, fn);
    return c2.r[2];
}

static uint32_t cl_str(int k) { return cl_mem + 0x20 * k; }   // 0..5 keys, 6..11 names, 12 style, 13 list slot, 14 list slot 1v1

// item 57: costs from the game's own getters, blocked rows, row pieces of AGS 296 / RHG 297, unit digits of AGS 144 / RHG 145
static const int CP_AGS = 296, CP_RHG = 297, CP_N = 12, CP_KI_BAR = 20000;
static uint32_t cp_hand = 0xFFFFFFFFu, cp_hold = 0;
static int cp_w[CP_N], cp_h[CP_N], cp_ns = 0, cp_dw = 12, cp_dh = 16;
static int cl_rec[6], cl_type[6], cl_cost[6];   // type 0 skill (units), 1 attack (ki), 2 ultimate (no cost)
static bool cl_lock[6];
static float cl_m0 = 0, cl_m1 = 0, cl_kl[6], cl_nl[6], cl_nr[6];   // drawn text extents per row (screen x), measured from the font vertices

// combination without the charge button, which is already held: device glyphs when rewritten, PSP glyphs otherwise
static uint32_t cl_nocharge(uint32_t s, int r) {
    if (!s || psp_l16(s) != 0xFF6E || psp_l16(s + 2) != '+') return s;
    uint32_t w = icons_text(s), src = w ? w : s, o = cl_mem + 0x200 + 0x100 * r;
    int i = 0;
    while (i < 64 && psp_l16(src + 2 * i) && psp_l16(src + 2 * i) != '+') i++;
    if (!psp_l16(src + 2 * i)) return s;
    int k = 0;
    for (i++; k < 126 && psp_l16(src + 2 * i); i++, k++) psp_s16(o + 2 * k, psp_l16(src + 2 * i));
    psp_s16(o + 2 * k, 0);
    return o;
}

static void cl_prepare() {
    if (cl_mem) return;
    cl_mem = psp_reserve(0x900);
    for (int r = 0; r < 6; r++) {
        snprintf((char*)psp_p(cl_str(r)), 32, "TEXT_CHARGE_KEY_%d", r + 1);
        snprintf((char*)psp_p(cl_str(6 + r)), 32, "TEXT_CHARGE_NAME_%d", r + 1);
    }
    strcpy((char*)psp_p(cl_str(13)), "BTL_CHARGE_LIST");
    strcpy((char*)psp_p(cl_str(14)), "BTL_CHARGE_LIST_1V1");
}

// double explosion (L + down + Circle, both fighters of the team to sparking): row of its own when AGS 280 has a sixth row
int psp_window_language();
static void cl_dex(uint32_t* key, uint32_t* name) {
    static const uint16_t k[] = {0xFF6E, '+', 0xFF72, '+', 0xFF6A, 0};
    *key = cl_mem + 0x800; *name = cl_mem + 0x840;
    for (int i = 0; i < 6; i++) psp_s16(*key + 2 * i, k[i]);
    const char* es = "Explosi\xF3n doble";
    const char* en = "Double explosion";
    const unsigned char* t = (const unsigned char*)(psp_window_language() == 3 ? es : en);
    int i = 0;
    for (; t[i] && i < 31; i++) psp_s16(*name + 2 * i, t[i]);
    psp_s16(*name + 2 * i, 0);
}

static uint32_t cl_fighter(const Ctx& c) {
    int idx = (int)hud_guest(c, 0x0885862Cu, 0);
    uint32_t t = psp_l32(0x08A7DBC8u);
    if (!t) return 0;
    int n = (int)psp_l32(t);
    uint32_t e = psp_l32(t + 4);
    for (int i = 0; i < n && i < 8 && e; i++) {
        uint32_t f = e + 0x1A90u * i;
        if (psp_l32(f + 0xC) && (int)psp_l32(f + 0x2C) == idx) return f;
    }
    return 0;
}

// row order from the combination glyphs: skills (Circle) then attacks (Triangle), each plain, up, down
static int cl_kind(uint32_t s) {
    int type = -1, dir = 0;
    for (int i = 0; s && i < 64; i++) {
        uint16_t ch = psp_l16(s + 2 * i);
        if (!ch) break;
        if (ch == 0xFF6A || ch == 0xFF75) type = 0;
        else if (ch == 0xFF6C || ch == 0xFF77 || ch == 0xFF7A) type = 1;
        else if (ch == 0xFF71) dir = 1;
        else if (ch == 0xFF72) dir = 2;
    }
    return type < 0 ? 9 : type * 3 + dir;
}

static void cl_text(uint32_t s, char* o, int n) {
    int k = 0;
    for (int i = 0; s && i < 64 && k < n - 8; i++) {
        uint16_t ch = psp_l16(s + 2 * i);
        if (!ch) break;
        k += ch < 0x80 ? snprintf(o + k, n - k, "%c", (char)ch) : snprintf(o + k, n - k, "{%04X}", ch);
    }
    o[k] = 0;
}

static void cl_fill(const Ctx& c, uint32_t o) {
    memset(cl_name, 0, sizeof(cl_name)); memset(cl_key, 0, sizeof(cl_key));
    for (int r = 0; r < 6; r++) { cl_rec[r] = -1; cl_type[r] = 2; cl_cost[r] = 0; cl_lock[r] = false; cl_kl[r] = cl_nl[r] = cl_nr[r] = 0; }
    bool six = cl_rows == 6;
    static int sl = -1;
    if (sl != (int)six) { sl = six; printf("charge: lista de %d filas%s\n", cl_rows, six ? " (explosion doble)" : ""); }
    cl_f = cl_fighter(c);
    if (!cl_f) return;
    uint32_t p = psp_l32(0x08B61178u + 4 * psp_l32(cl_f + 0x2C)), rec = p ? psp_l32(p + 0xBC) : 0;
    if (!rec) return;
    struct R { int kind, i; uint32_t n, k; } v[6];
    int nv = 0;
    for (int i = 0; i < 5; i++) {
        uint32_t r = rec + 0x20 * i;
        if ((int)psp_l32(r) == -1) break;
        uint32_t sn = hud_guest(c, 0x0883EE74u, psp_l32(r + 4), 0xFFFFFFFFu), sk = hud_guest(c, 0x0883EE74u, psp_l32(r + 8), 0xFFFFFFFFu);
        static uint32_t cut = 0;   // a command over several lines (BT3 condition or note): only the line with the buttons is drawn
        if (!cut) cut = psp_reserve(5 * 256);
        if (sk) {
            int a0 = 0, best = -1, bend = 0, multi = 0;
            for (int k = 0; k < 127; k++) {
                uint16_t ch = psp_l16(sk + 2 * k);
                if (ch == 0x0A || !ch) {
                    if (best < 0) for (int j = a0; j < k; j++) { uint16_t g = psp_l16(sk + 2 * j); if (g >= 0xFF67 && g <= 0xFF7F) { best = a0; bend = k; break; } }
                    if (!ch) break;
                    multi = 1; a0 = k + 1;
                }
            }
            if (multi && best >= 0) {
                uint32_t b = cut + 256 * (uint32_t)nv;
                for (int j = best; j < bend; j++) psp_s16(b + 2 * (j - best), psp_l16(sk + 2 * j));
                psp_s16(b + 2 * (bend - best), 0);
                sk = b;
            }
        }
        v[nv++] = {cl_kind(sk), i, sn, sk};
    }
    if (six && nv < 6) { uint32_t dk, dn; cl_dex(&dk, &dn); v[nv++] = {cl_kind(dk), 99, dn, dk}; }
    std::stable_sort(v, v + nv, [](const R& a, const R& b) { return a.kind < b.kind; });
    static int logs = 0;
    for (int r = 0; r < nv; r++) {
        cl_name[r] = v[r].n; cl_key[r] = v[r].k; cl_rec[r] = v[r].i;
        cl_type[r] = v[r].kind <= 2 ? 0 : v[r].kind <= 5 ? 1 : 2;
        if (logs < 40) {
            char a[256], b[256];
            cl_text(v[r].k, a, sizeof(a)); cl_text(v[r].n, b, sizeof(b));
            printf("charge: fila %d registro %d tipo %d \"%s\" \"%s\"\n", r + 1, v[r].i, v[r].kind, a, b);
            logs++;
        }
    }
}

// sparking: +0x1A54 fills to 30000 with the orange bar, the explosion sets 30000 and it drains to 0 while sparking lasts
static bool cl_spark() {
    if (!cl_f) return false;
    static bool on = false;
    static int logs = 0, last = -1;
    int t = (int)psp_l32(cl_f + 0x1A54);
    if (t >= 30000) on = true;
    else if (t <= 0) on = false;
    if ((int)on != last && logs < 40) { printf("charge: sparking %d (tiempo %d, banderas %d %d, ki %d)\n", (int)on, t, psp_l8(cl_f + 0xDDB), psp_l8(cl_f + 0xF7D), (int)psp_l32(cl_f + 0x1A40)); logs++; }
    last = on;
    return on;
}

// team energy as the game checks it: leader +0x2D8 minus every reservation of the team (100000 = one unit)
static int cl_energy(const Ctx& c, uint32_t f) {
    uint32_t lead = hud_guest(c, 0x08927308u, psp_l32(f + 4));
    if (!guest_ptr(lead)) return 0;
    int e = (int)psp_l32(lead + 0x2D8);
    uint32_t t = psp_l32(0x08A7DBC8u);
    int n = t ? (int)psp_l32(t) : 0;
    uint32_t b = t ? psp_l32(t + 4) : 0;
    for (int i = 0; i < n && i < 8 && b; i++) {
        uint32_t g = b + 0x1A90u * i;
        if (psp_l32(g + 4) == psp_l32(f + 4)) for (int k = 0; k < 3; k++) e -= (int)psp_l32(g + 0x1A48 + 4 * k);
    }
    return e;
}

static void cl_costs(const Ctx& c) {
    if (!cl_f) return;
    int ki = (int)psp_l32(cl_f + 0x1A40), en = cl_energy(c, cl_f);
    static int logs = 0, seen[6] = {-9, -9, -9, -9, -9, -9};
    for (int r = 0; r < cl_rows; r++) {
        int i = cl_rec[r], cost = 0;
        bool lock = false;
        if (cl_type[r] == 0 && i == 99) { cost = 500000; lock = en < cost; }
        else if (cl_type[r] == 0 && i >= 0 && i <= 1) { cost = (int)hud_guest(c, 0x089A42B0u, cl_f, (uint32_t)i); lock = en < cost; }
        else if (cl_type[r] == 1 && i >= 2 && i <= 3) { cost = (int)hud_guest(c, 0x089A25D8u, cl_f, (uint32_t)i); lock = ki < cost; }
        else cl_type[r] = 2;
        cl_cost[r] = cost; cl_lock[r] = lock;
        int key = cost * 2 + lock;
        if (key != seen[r] && logs < 60) {
            seen[r] = key; logs++;
            printf("charge: fila %d registro %d %s costo %d (%s %d) %s\n", r + 1, i, cl_type[r] == 0 ? "habilidad" : cl_type[r] == 1 ? "ataque" : "ultimate",
                   cost, cl_type[r] == 1 ? "ki" : "energia", cl_type[r] == 1 ? ki : en, lock ? "bloqueada" : "disponible");
        }
    }
}

// free states only (12 idle, 60 charging): never during attacks, techniques, guard or movement
static bool cl_free(uint32_t f) {
    if (!f) return false;
    int a = (int)psp_l32(f + 0x984);
    if (a == 12 || a == 60) return true;
    static int logs = 0;
    static std::map<int, int> seen;
    if (input_charge_held() && logs < 30 && !seen[a]++) { printf("charge: accion %d bloquea la lista\n", a); logs++; }
    return false;
}

// through-mode vertices of the list texts scaled around the text anchor (font engine has no scale)
static int cl_vlayout(uint32_t vt, int* posoff, int* possz) {
    static const int tsz[4] = {0, 1, 2, 4};
    int off = 0, maxa = 1;
    auto add = [&](int n, int sz) { if (!sz) return; off = (off + sz - 1) / sz * sz; off += n * sz; if (sz > maxa) maxa = sz; };
    int wt = (vt >> 9) & 3, nw = ((vt >> 14) & 7) + 1, col = (vt >> 2) & 7;
    if (wt) add(nw, tsz[wt]);
    add(2, tsz[vt & 3]);
    add(1, col == 7 ? 4 : col >= 4 ? 2 : 0);
    add(3, tsz[(vt >> 5) & 3]);
    int ps = tsz[(vt >> 7) & 3];
    off = ps ? (off + ps - 1) / ps * ps : off;
    *posoff = off; *possz = ps;
    add(3, ps);
    return (off + maxa - 1) / maxa * maxa;
}

static bool cl_gu(Ctx& c, void (*orig)(Ctx&)) {
    uint32_t n = c.r[6], v = c.r[8], vt = c.r[5] ? c.r[5] : cl_vt;   // font sprites keep the font's own format
    if (!cl_gu_on || !v || c.r[7] || !(vt & 0x800000u)) return false;
    int po, ps, st = cl_vlayout(vt, &po, &ps);
    for (uint32_t i = 0; i < n && i < 4096; i++) {
        uint32_t a = v + i * st + po;
        if (ps == 2) {
            float x = (int16_t)psp_l16(a), y = (int16_t)psp_l16(a + 2);
            x = cl_ax + (x - cl_ax) * cl_sx;
            if (x < cl_m0) cl_m0 = x;
            if (x > cl_m1) cl_m1 = x;
            psp_s16(a, (uint16_t)(int16_t)lroundf(x));
            psp_s16(a + 2, (uint16_t)(int16_t)lroundf(cl_ay + (y - cl_ay) * cl_sy));
        } else if (ps == 4) {
            FReg x, y; x.u = psp_l32(a); y.u = psp_l32(a + 4);
            x.f = cl_ax + (x.f - cl_ax) * cl_sx; y.f = cl_ay + (y.f - cl_ay) * cl_sy;
            if (x.f < cl_m0) cl_m0 = x.f;
            if (x.f > cl_m1) cl_m1 = x.f;
            psp_s32(a, x.u); psp_s32(a + 4, y.u);
        }
    }
    orig(c);
    return true;
}

// battle phase [[0x08A7D880]]: 0..3 map intro, fighter intros, banners, Ready; 4 combat; 5 KO, 6 victory poses, 7 end menu
static bool cl_phase_ok() {
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_CHARGE_PHASE"); on = e && *e ? atoi(e) != 0 : 1; }
    uint32_t m = psp_l32(0x08A7D880u);
    if (!on || !guest_ptr(m)) return true;
    int ph = (int)psp_l32(m);
    static int logs = 0, last = -1;
    if (ph != 4 && ph != last && input_charge_held() && logs < 20) { logs++; last = ph; printf("charge: lista bloqueada en la fase %d\n", ph); }
    return ph == 4;
}
// J1 has a partner in this battle (another active fighter of his team): no technique list, in every bar mode
static bool cl_mate(uint32_t f) {
    uint32_t t = psp_l32(0x08A7DBC8u);
    if (!f || !t) return false;
    int n = (int)psp_l32(t);
    uint32_t b = psp_l32(t + 4);
    for (int i = 0; i < n && i < 8 && b; i++) {
        uint32_t g = b + 0x1A90u * i;
        if (g != f && psp_l32(g + 0xC) && psp_l32(g + 4) == psp_l32(f + 4) && (int)psp_l32(g + 0x1A38) > 0) return true;
    }
    return false;
}
static bool cl_want() {
    if (!input_charge_held() || psp_input_context() != 1 || psp_input_clash() || !cl_phase_ok()) return false;
    int codes[12]; uint32_t serial = 0;
    int n = psp_input_prompt(codes, 12, &serial);
    for (int i = 0; i < n && i < 12; i++) if (codes[i] >= 0x10 && codes[i] <= 0x13) return false;
    return true;
}

int psp_input_local(int k, uint32_t* b, uint8_t* x, uint8_t* y);

// raw AGS of a battle_ui code in guest memory (pak header big endian, AGS little endian); state header i at +0x20 + 128 i, w/h words 18/19
static uint32_t cp_ags(uint32_t pak, int code) {
    if (!guest_ptr(pak) || (int)__builtin_bswap32(psp_l32(pak)) < code) return 0;
    uint32_t a = pak + __builtin_bswap32(psp_l32(pak + 4 + 4 * (code - 1)));
    return psp_l32(a) == 0x00534741u ? a : 0;
}

static void cp_load(const Ctx& c, uint32_t pak) {
    if (uint32_t d = cp_ags(pak, 144)) { cp_dw = (int)psp_l32(d + 0x20 + 18 * 4); cp_dh = (int)psp_l32(d + 0x20 + 19 * 4); }
    if (cp_dw <= 0 || cp_dw > 64) cp_dw = 12;
    if (cp_dh <= 0 || cp_dh > 64) cp_dh = 16;
    uint32_t a = cp_ags(pak, CP_AGS);
    static int logs = 0;
    if (!a) { if (logs++ < 4) printf("charge: sin piezas propias (AGS %d), fondo del AGS 280\n", CP_AGS); return; }
    cp_ns = (int)psp_l32(a + 0xC);
    if (cp_ns > CP_N) cp_ns = CP_N;
    for (int i = 0; i < cp_ns; i++) { cp_w[i] = (int)psp_l32(a + 0x20 + 128 * i + 18 * 4); cp_h[i] = (int)psp_l32(a + 0x20 + 128 * i + 19 * 4); }
    cp_hand = hud_guest(c, 0x088404D8u, 0x08B48200u, pak, CP_AGS, CP_RHG);
    cp_hold = hud_guest(c, 0x089BFDF4u, 0x54);
    if (cp_hold) hud_guest(c, 0x089D1EE8u, cp_hold, cp_hand, 0, (uint32_t)(cp_ns - 1));
    if (logs++ < 4) {
        printf("charge: piezas AGS %d con %d estados, digitos %dx%d:", CP_AGS, cp_ns, cp_dw, cp_dh);
        for (int i = 0; i < cp_ns; i++) printf(" %dx%d", cp_w[i], cp_h[i]);
        printf("\n"); fflush(stdout);
    }
}

static void cp_free(const Ctx& c) {
    if (cp_hold) hud_guest(c, 0x089D248Cu, cp_hold, 3);
    if (cp_hand != 0xFFFFFFFFu) hud_guest(c, 0x08840640u, 0x08B48200u, cp_hand);
    cp_hold = 0; cp_hand = 0xFFFFFFFFu; cp_ns = 0;
}

static void cl_init(const Ctx& c, uint32_t m) {
    cl_obj = 0; cl_f = 0; cl_block = false; cl_spk = -1; cp_hold = 0; cp_hand = 0xFFFFFFFFu; cp_ns = 0;
    const char* e = getenv("TTT_CHARGE_LIST");
    cl_list_on = patch_charge_list.val && !(e && *e && !atoi(e));
    if (!cl_list_on && !input_wheel_on()) return;
    uint32_t pb, md = psp_l32(0x08B48A28u); uint8_t px, py;
    cl_split = patch_split_screen.val && !(md >= 11 && md <= 13) && psp_input_local(0, &pb, &px, &py) >= 2;
    if (cl_split) {
        static int h = 0;
        if (h++ < 5) printf("charge: lista y rueda de transformacion ocultas en pantalla dividida\n");
        return;   // local multiplayer: no list and no wheel (space); the wheel is for one player, LAN and online
    }
    cl_prepare();
    uint32_t a = hud_guest(c, 0x089BFDF4u, 0x94);
    if (!a) return;
    hud_guest(c, 0x089F1824u, a);
    cl_obj = a;
    hud_guest(c, 0x089D0310u, m, a, 0x80, 0);
    static int n = 0;
    if (n++ < 20) printf("charge: lista creada, holder %s\n", psp_l32(a + 0x6C) ? "ok" : "nulo");
}

static void cl_teardown(const Ctx& c, uint32_t m) {
    if (!cl_obj || m != hud_mgr) return;
    uint32_t u = cl_obj; cl_obj = 0; cl_f = 0;
    cp_free(c);
    tw_free(c);
    hud_guest(c, 0x089CFD08u, m, u);
    uint32_t v = psp_l32(u + 0x40) + 8;
    hud_guest(c, psp_l32(v + 4), u + (uint32_t)(int32_t)(int16_t)psp_l16(v), 3);
}

static void cl_style(uint32_t col, uint32_t outline, uint32_t last) {
    uint32_t s = cl_str(12);
    psp_s32(s, 0); psp_s32(s + 4, 0); psp_s32(s + 8, col); psp_s32(s + 0xC, 0);
    psp_s32(s + 0x10, 1); psp_s32(s + 0x14, 1); psp_s32(s + 0x18, outline); psp_s32(s + 0x1C, last);
}

// slot rect of a row text: some slots already carry the list scale, others are in AGS units around the list origin
static bool cl_slot(const Ctx& c, uint32_t root, int k, float base, int* x, int* y, int* w, int* al) {
    uint32_t slot = hud_guest(c, 0x08840DC4u, root, cl_str(k));
    if (!slot) return false;
    *x = (int)psp_l32(slot + 0xE4); *y = (int)psp_l32(slot + 0xE8); *w = (int)psp_l32(slot + 0xEC);
    if (fabsf(*w - base * cl_sx) > fabsf(*w - base)) {
        *x = (int)lroundf(cl_ox + (*x - cl_ox) * cl_sx); *y = (int)lroundf(cl_oy + (*y - cl_oy) * cl_sy);
        *w = (int)lroundf(*w * cl_sx);
    }
    FReg a; a.u = psp_l32(slot + 0x2C);
    int v = (int)(a.f * 255.0f);
    *al = v < 0 ? 0 : v > 255 ? 255 : v;
    return true;
}

static void cl_ge(uint32_t cmd, uint32_t arg) {
    uint32_t ctx = psp_l32(0x08B478D0u), p = ctx && ctx < 0x10000000u ? psp_l32(ctx + 8) : 0;
    if (!p || p >= 0x10000000u) return;
    psp_s32(p, (cmd << 24) | (arg & 0xFFFFFFu));
    psp_s32(ctx + 8, p + 4);
}

// one AGS instance of a holder drawn at a screen position, size and colour; every field it touches is restored
static void cp_inst(const Ctx& c, uint32_t inst, float x, float y, float sx, float sy, float k, float a) {
    if (!inst) return;
    uint8_t keep[0x54];
    memcpy(keep, psp_p(inst), sizeof(keep));
    FReg f;
    psp_s32(inst, (uint32_t)(int)lroundf(x)); psp_s32(inst + 4, (uint32_t)(int)lroundf(y));
    psp_s32(inst + 8, 0); psp_s32(inst + 0xC, 0);
    f.f = k; psp_s32(inst + 0x10, f.u); psp_s32(inst + 0x14, f.u); psp_s32(inst + 0x18, f.u);
    f.f = a; psp_s32(inst + 0x1C, f.u);
    f.f = sx; psp_s32(inst + 0x20, f.u);
    f.f = sy; psp_s32(inst + 0x24, f.u);
    psp_s32(inst + 0x34, 0);
    psp_s8(inst + 0x4C, 1);
    psp_s32(inst + 0x50, 0);
    hud_guest(c, 0x089D1B94u, inst);
    memcpy(psp_p(inst), keep, sizeof(keep));
}

static uint32_t cp_state(uint32_t hold, int st) {
    uint32_t l = hold ? psp_l32(hold + 4) : 0;
    int n = hold ? (int)psp_l32(hold + 0xC) : 0;
    for (int i = 0; l && i < n; i++) if ((int)psp_l32(l + 8 * i) == st) return psp_l32(l + 8 * i + 4);
    return 0;
}

// digit instances of the union counter (AGS 144 / RHG 145, same asset as the energy units next to the ki bar)
static uint32_t cp_digit(int v) {
    uint32_t u = hud_mgr ? psp_l32(hud_mgr + 0xA4) : 0, d = u ? psp_l32(u + 0x78) : 0;
    return d && v >= 0 && v <= 9 ? psp_l32(d + 4 * v) : 0;
}

// row background from the drawn texts (previous frame): left cap just before the button icon, stretched middle, right cap under
// the end of the text; skills put the unit digit (row height) over the right cap, attacks a ki segment per bar under the name
#include "ui/sz_layout.h"
static bool sz_list(const Ctx& c, bool spark, float al, SzLKey* keys);
static void cl_draw_pieces(const Ctx& c, uint32_t root, bool spark, int* tws) {
    float dw;
    if (!cp_hold && !bh_list_row(0, 0, 0, 12, 0, 0, &dw)) return;
    for (int r = 0; r < cl_rows; r++) {
        if (r == cl_rows - 1 && !spark) continue;
        int kx, ky, kw, ka, nx, ny, nw, na;
        if (!cl_slot(c, root, r, 60.0f, &kx, &ky, &kw, &ka) || !cl_slot(c, root, 6 + r, 150.0f, &nx, &ny, &nw, &na) || !na) continue;
        int t = cl_type[r], b = t * 3;
        float a = na / 255.0f, k = cl_lock[r] ? 0.45f : 1.0f, swl = 0;
        bool skin = bh_list_row(0, 0, 0, 20.0f * cl_sy, 0, 0, &swl);   // BT3 bars: the mod's grey row (same for every type)
        static int sl = -1;
        if ((int)skin != sl) { sl = skin; printf("charge: fondo de filas %s\n", skin ? "BT3 del mod (list_l, list_m, list_r)" : "TTT (AGS 296)"); }
        if (!skin && (b + 2 >= cp_ns || cp_w[b] <= 0 || cp_w[b + 1] <= 0)) continue;
        float rh = (skin ? 20 : cp_h[b]) * cl_sy;
        float kl = cl_kl[r] > 0 ? cl_kl[r] : (float)kx + kw, nl = cl_nr[r] > 0 ? cl_nl[r] : (float)nx, nr = cl_nr[r] > 0 ? cl_nr[r] : nx + tws[r] * cl_sx * 0.74f;
        float y0 = ny - (rh / cl_sy - 16) * 0.5f * cl_sy, rs = nr + 3.0f * cl_sx;   // text of 16 units centred in the row
        int digs = 0, dv = 0, segs = 0;
        float ds = rh / cp_dh, dx = nr + 2.0f * cl_sx;
        if (t == 0 && cl_cost[r] > 0) { dv = (cl_cost[r] + 99999) / 100000; digs = dv > 9 ? 2 : 1; rs = dx + digs * cp_dw * ds * 0.4f; }
        float sw = cp_ns > 9 ? cp_w[9] * cl_sx : 0, sg = 1.0f * cl_sx;
        int kis = t == 1 && cl_cost[r] > 0 && bh_on && !bh_sz ? (cl_cost[r] + CP_KI_BAR - 1) / CP_KI_BAR : 0;   // BT3 bars: BT3 ki cells after the name
        float kh = rh * 0.8f, cx = nr + 2.0f * cl_sx;
        if (kis) { float ke = bh_list_ki(cx, 0, kh, kis, 0, -1); if (ke > 0) rs = cx + (ke - cx) * 0.85f; else kis = 0; }
        else if (t == 1 && cl_cost[r] > 0 && sw > 0) {
            segs = (cl_cost[r] + CP_KI_BAR - 1) / CP_KI_BAR;
            float se = nl + segs * (sw + sg) + 2.0f * cl_sx;
            if (se > rs) rs = se;
        }
        float lw = skin ? swl : cp_w[b] * cl_sx;
        int XL = (int)floorf(kl - 2.0f * cl_sx - lw), XM = XL + (int)lroundf(lw) - 1, XR = (int)lroundf(rs);
        if (XR < XM + 1) XR = XM + 1;
        if (skin) bh_list_row((float)XL, (float)XR, y0, rh, k, a, nullptr);
        else {
            cp_inst(c, cp_state(cp_hold, b), (float)XL, y0, cl_sx, cl_sy, k, a);
            cp_inst(c, cp_state(cp_hold, b + 1), (float)XM, y0, (float)(XR + 1 - XM) / cp_w[b + 1], cl_sy, k, a);
            cp_inst(c, cp_state(cp_hold, b + 2), (float)XR, y0, cl_sx, cl_sy, k, a);
        }
        if (kis) bh_list_ki(cx, y0 + (rh - kh) * 0.5f, kh, kis, k, a);
        for (int i = 0; i < segs; i++)
            cp_inst(c, cp_state(cp_hold, 9), nl + i * (sw + sg), y0 + rh - cp_h[9] * cl_sy * 0.5f, cl_sx, cl_sy, k, a);
        if (digs == 1 && bh_list_digit(dx + cp_dw * ds * 0.5f, y0 - rh * 0.15f, rh * 1.3f, dv, k, a)) digs = 0;   // BT3 bars: the BT3 blast digit
        for (int i = digs - 1; i >= 0; i--, dx += cp_dw * ds) cp_inst(c, cp_digit(i ? dv / 10 : dv % 10), dx, y0 + (rh - cp_dh * ds) * 0.5f, ds, ds, k, a);
    }
}

static void cl_draw_texts(const Ctx& c, uint32_t root) {
    bool spark = cl_spark();
    uint32_t font = psp_l32(0x08A78EECu);
    cl_costs(c);
    int tws[6] = {0, 0, 0, 0, 0, 0};
    for (int r = 0; r < cl_rows; r++) if (cl_name[r] && psp_l16(cl_name[r])) tws[r] = (int)hud_guest(c, 0x08831F68u, font, cl_name[r]);
    SzLKey szk[6];
    int sx0, sy0, sw0, sa0 = 255;
    cl_slot(c, root, 6, 150.0f, &sx0, &sy0, &sw0, &sa0);
    bool szl = sz_list(c, spark, sa0 / 255.0f, szk);
    if (!szl) cl_draw_pieces(c, root, spark, tws);
    uint32_t list = hud_guest(c, 0x08838054u, 0);
    hud_guest(c, 0x08832448u, font);
    float kmin = 1e9f, kw[6] = {0, 0, 0, 0, 0, 0};   // keys start where the narrowest ends today (the stairs hold); a wider key pushes only its own name right
    for (int r = 0; r < cl_rows; r++) {
        uint32_t k = cl_nocharge(cl_key[r], r);
        if (k && psp_l16(k) && !(r == cl_rows - 1 && !spark)) { kw[r] = (float)(int)hud_guest(c, 0x08831F68u, font, k) * cl_sx; kmin = std::min(kmin, kw[r]); }
    }
    if (kmin > 1e8f) kmin = 0;
    for (int r = 0; r < cl_rows; r++) {
        if (r == cl_rows - 1 && !spark) continue;
        int kx0 = 0, ky0, kw0 = 0, ka0;
        bool ks = cl_slot(c, root, r, 60.0f, &kx0, &ky0, &kw0, &ka0);
        int push = ks && kw[r] > kmin ? (int)lroundf(kw[r] - kmin) : 0;
        for (int part = 0; part < 2; part++) {
            uint32_t str = part ? cl_name[r] : cl_nocharge(cl_key[r], r);
            if (!str || !psp_l16(str)) continue;
            if (szl) {   // SZ list: only the key glyphs, right-aligned at the row's key place, at the row's depth scale
                if (part || szk[r].a <= 0.01f) continue;
                float sc = 0.36f * szk[r].s, ksx = cl_sx, ksy = cl_sy;
                int aw = (int)(szk[r].a * 255), tw0 = (int)hud_guest(c, 0x08831F68u, font, str);
                cl_style((uint32_t)aw << 24 | (cl_lock[r] ? 0x737373u : 0xFFFFFFu), (uint32_t)aw << 24 | (cl_lock[r] ? 0x1C1C1Cu : 0x404040u), 1);
                hud_guest(c, 0x088321ECu, font, cl_str(12));
                int gx = (int)lroundf(szk[r].x - tw0 * sc), gy = (int)lroundf(szk[r].y - 8 * sc);
                cl_sx = cl_sy = sc; cl_ax = (float)gx; cl_ay = (float)gy; cl_gu_on = true;
                cl_call(c, 0x08832530u, font, (uint32_t)gx, (uint32_t)gy, str, 0xFFFFFFFFu, 0xFFFFFFFFu);
                cl_gu_on = false; cl_sx = ksx; cl_sy = ksy;
                continue;
            }
            int x, y, w, al;
            if (!cl_slot(c, root, part ? 6 + r : r, part ? 150.0f : 60.0f, &x, &y, &w, &al) || !al) continue;
            uint32_t rgb = cl_lock[r] ? 0x737373u : 0xFFFFFFu, ol = cl_lock[r] ? 0x1C1C1Cu : 0x404040u;
            cl_style((uint32_t)al << 24 | rgb, (uint32_t)al << 24 | ol, 1);
            hud_guest(c, 0x088321ECu, font, cl_str(12));
            if (!part) x += w - (int)lroundf(kmin);
            else x += push;
            cl_ax = (float)x; cl_ay = (float)y; cl_gu_on = true; cl_m0 = 1e9f; cl_m1 = -1e9f;
            cl_call(c, 0x08832530u, font, (uint32_t)x, (uint32_t)y, str, 0xFFFFFFFFu, 0xFFFFFFFFu);
            cl_gu_on = false;
            if (cl_m1 > cl_m0) { if (part) { cl_nl[r] = cl_m0; cl_nr[r] = cl_m1; } else cl_kl[r] = cl_m0; }
        }
    }
    cl_style(0xFFFFFFFFu, 0xFF404040u, 0);
    hud_guest(c, 0x088321ECu, font, cl_str(12));
    hud_guest(c, 0x08832500u, font);
    hud_guest(c, 0x08838120u, list);
    hud_guest(c, 0x08837F54u, 0x08B4E690u, list, 0);
}

// a font draw with its vertices scaled around (ax, ay) by the list's GU hook (the font engine has no scale)
static void cl_scaled_call(const Ctx& c, uint32_t fn, uint32_t font, int x, int y, uint32_t str, float s, float ax, float ay) {
    float ksx = cl_sx, ksy = cl_sy, kax = cl_ax, kay = cl_ay;
    cl_sx = cl_sy = s; cl_ax = ax; cl_ay = ay; cl_gu_on = s != 1.0f;
    hud_guest(c, fn, font, (uint32_t)x, (uint32_t)y, str);
    cl_gu_on = false; cl_sx = ksx; cl_sy = ksy; cl_ax = kax; cl_ay = kay;
}

// names on the life bars (BT3 bars only): slot BTL_NAME of the bar body, the bar class's own virtual +0xF8 (0x089E6604)
bool net_online_on();
static uint32_t nm_mem = 0;
static int nm_dl = 0;
static const float NM_SCALE = 1.2f;   // names a little larger than the bar's own scale
static bool nm_want() { const char* e = getenv("TTT_BAR_NAMES"); return !(e && *e) || atoi(e) != 0; }

// online: the room player of the fighter's team slot; otherwise the character's name as the game picks it in 0x089E6604
static uint32_t ro_name_tab_b();
static std::map<uint32_t, uint32_t> nm_seen;   // fighter entry -> its name string as the bars last drew it (Discord reads it)
static uint32_t nm_text(const Ctx& c, uint32_t f, bool* player) {
    *player = false;
    int t = (int)psp_l32(f + 4), m = (int)psp_l32(f + 8);
    if (net_online_on() && t >= 0 && t < 2 && m >= 0 && m < 2) {
        int p = (int)psp_l32(0x08B48E84u + 8 * t + 4 * m);
        uint32_t net = hud_guest(c, 0x08811A58u, 0), ri = net ? hud_guest(c, 0x088118ACu, net) : 0;
        if (ri && p >= 0 && p < (int8_t)psp_l8(ri + 0x1D1) && psp_l16(ri + 0x74 * p)) { *player = true; return ri + 0x74 * p; }
    }
    uint32_t ent = psp_l32(0x08B61178u + 4 * psp_l32(f + 0x2C));
    if (!ent) return 0;
    uint32_t g = hud_guest(c, 0x08852B58u, psp_l32(ent + 8)) & 0xFF;
    if (g == 0xFF) return 0;
    uint32_t id = psp_l32(ro_name_tab_b() + 4 * g), a1 = psp_l8(hud_guest(c, 0x08858698u, (uint32_t)t, (uint32_t)m) + 0xC);
    if (id == 0x43C && a1) id = 0x496;
    if (psp_l8(f + 0x1A5C) || (id == 0x455 && a1)) id = 0x471;
    if (id > 0xFFFF) return 0;
    uint32_t s = hud_guest(c, 0x0883EE74u, id, 0xFFFFFFFFu);
    if (s && psp_l16(s)) nm_seen[f] = s;
    return s && psp_l16(s) ? s : 0;
}

static void nm_draw(const Ctx& c, uint32_t o) {
    int idx = hud_index(o);
    uint32_t f = psp_l32(o + 0x6C), h0 = psp_l32(o + 0x70), inst = h0 ? psp_l32(h0) : 0, t = psp_l32(o + 0x108), font = psp_l32(0x08A78EECu);
    if (idx < 0 || !f || !inst || !psp_l32(inst + 0x30) || !t || !psp_l32(t + 0xF8) || !font) return;
    uint32_t slot = hud_guest(c, 0x08840DC4u, psp_l32(inst + 0x30), psp_l32(t + 0xF8)), body = psp_l32(h0 + 8);
    if (!slot || !body) return;
    int x = (int)psp_l32(slot + 0xE4), y = (int)psp_l32(slot + 0xE8), w = (int)psp_l32(slot + 0xEC), bx = (int)psp_l32(body + 0xE4), bw = (int)psp_l32(body + 0xEC);
    if (idx == 0) nm_dl = x - bx;
    bool player;
    uint32_t str = nm_text(c, f, &player);
    FReg a; a.u = psp_l32(slot + 0x2C);
    int al = (int)(a.f * 255.0f);
    al = al < 0 ? 0 : al > 255 ? 255 : al;
    if (!str || !al || (!player && !nm_want())) return;
    if (!nm_mem) nm_mem = psp_reserve(0x40);
    float sx = hud_sx[idx] * NM_SCALE, sy = hud_sy[idx] * NM_SCALE;
    int tw = (int)hud_guest(c, 0x08831F68u, font, str);
    if (tw <= 0) return;
    if (w > 0 && tw * sx > w) sx = (float)w / tw;
    if (idx >= 2) x = bx + bw - nm_dl - (int)lroundf(tw * sx);
    static int logs = 0;
    if (logs < 8) { logs++; printf("hud: nombre en la barra %d (%s), x %d y %d ancho %d, escala %.2f\n", idx, player ? "jugador" : "personaje", x, y, tw, sx); }
    uint32_t st = nm_mem;
    psp_s32(st, 0); psp_s32(st + 4, 0); psp_s32(st + 8, (uint32_t)al << 24 | 0xFFFFFFu); psp_s32(st + 0xC, 1);
    psp_s32(st + 0x10, 1); psp_s32(st + 0x14, 1); psp_s32(st + 0x18, (uint32_t)al << 24 | 0x404040u); psp_s32(st + 0x1C, 2);
    uint32_t list = hud_guest(c, 0x08838054u, 0);
    hud_guest(c, 0x08832448u, font);
    hud_guest(c, 0x088321ECu, font, st);
    float ksx = cl_sx, ksy = cl_sy, kax = cl_ax, kay = cl_ay;
    cl_sx = sx; cl_sy = sy; cl_ax = (float)x; cl_ay = (float)y; cl_gu_on = sx != 1.0f || sy != 1.0f;
    cl_call(c, 0x08832530u, font, (uint32_t)x, (uint32_t)y, str, 0xFFFFFFFFu, 0xFFFFFFFFu);
    cl_gu_on = false; cl_sx = ksx; cl_sy = ksy; cl_ax = kax; cl_ay = kay;
    psp_s32(st + 8, 0xFFFFFFFFu); psp_s32(st + 0xC, 0); psp_s32(st + 0x18, 0xFF404040u); psp_s32(st + 0x1C, 0);
    hud_guest(c, 0x088321ECu, font, st);
    hud_guest(c, 0x08832500u, font);
    hud_guest(c, 0x08838120u, list);
    hud_guest(c, 0x08837F54u, 0x08B4E690u, list, 0);
}

// partner bar on screen (BT3 bars): a bar of the local fighter's team bound to another fighter and drawn
static bool cl_partner_bar() {
    if (!hud_on || !hud_mgr || !cl_f) return true;
    for (int k = 1; k < 4; k++) {
        uint32_t bar = psp_l32(hud_mgr + 0x74 + 4 * k), fb = bar ? psp_l32(bar + 0x6C) : 0;
        if (fb && fb != cl_f && (int)psp_l32(fb + 0x1A38) > 0 && psp_l32(fb + 4) == psp_l32(cl_f + 4) && psp_l8(bar + 0x12C)) return true;   // also the fused fighter shown in the partner bar
    }
    return false;
}

static bool cl_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&)) {
    if (dir == 0x089E6604u) {                 // bar name: never in the original HUD (it would name the opponents of 0x45 bars)
        if (hud_on && (nm_want() || net_online_on())) nm_draw(c, c.r[4]);
        return true;
    }
    if (dir == 0x08998BE4u) {
        if (!cl_upd) return false;
        c.r[2] = 0xFFFFFFFFu;
        return true;
    }
    uint32_t o = c.r[4];
    if (!cl_obj || o != cl_obj) return false;
    if (dir == 0x089F1E68u) {                 // load: AGS 280 with the technique window's RHG 130, states 0..10
        uint32_t pak = psp_l32(psp_l32(0x08A7F280u) + 0x10);
        uint32_t h = hud_guest(c, 0x088404D8u, 0x08B48200u, pak, 280, 130);
        psp_s32(o + 0x68, h);
        int last = CL_NOSPARK + 2, ns = -1;   // a sixth row lives in state 11, used by states 0..2: load every state of the file
        if (guest_ptr(pak) && __builtin_bswap32(psp_l32(pak)) > 280) {
            uint32_t ags = pak + __builtin_bswap32(psp_l32(pak + 4 + 4 * 279));
            if (psp_l32(ags) == 0x00534741u) ns = (int)psp_l32(ags + 0xC);
        }
        if (ns > last + 1) last = ns - 1 > 15 ? 15 : ns - 1;
        cl_rows = ns >= 12 ? 6 : 5; cl_h = ns >= 12 ? 141.0f : 120.0f;
        static int ll = 0;
        if (ll++ < 4) { printf("charge: AGS 280 con %d estados, se cargan 0..%d\n", ns, last); fflush(stdout); }
        uint32_t in = hud_guest(c, 0x089BFDF4u, 0x54);
        if (in) hud_guest(c, 0x089D1EE8u, in, h, 0, last);
        psp_s32(o + 0x6C, in);
        cp_load(c, pak);
        tw_load(c, pak);
        return true;
    }
    if (dir == 0x089F1F28u) {                 // update: open while charging, close on release or when a technique starts
        uint32_t f = cl_fighter(c);
        tw_tick(c, f);
        int sp = (int)hud_guest(c, 0x08998BE4u, hud_guest(c, 0x0885862Cu, 0));
        if (sp >= 0) cl_block = true;
        if (!input_charge_held()) cl_block = false;
        bool mate = cl_mate(f);
        static int ml = -1, mlogs = 0;
        if ((int)mate != ml && mlogs++ < 10) { ml = mate; printf("charge: lista %s\n", mate ? "oculta (J1 con companero)" : "disponible (J1 sin companero)"); }
        bool want = cl_list_on && !cl_split && !cl_block && !mate && cl_free(f) && cl_want();
        uint32_t hold = psp_l32(o + 0x6C);
        if (hold) { FReg st; st.f = (float)((int)hud_guest(c, 0x08837A0Cu, 0) * CL_SPEED); psp_s32(hold + 0x10, st.u); }
        int st = (int)psp_l32(o + 0x48), rq = (int)psp_l32(o + 0x50), eff = rq != -1 ? rq : st;
        if (want && (eff == -1 || eff == 2)) { cl_fill(c, o); psp_s32(o + 0x70, 0); hud_guest(c, 0x089D1694u, o, 0); }
        else if (!want && (eff == 0 || eff == 1)) hud_guest(c, 0x089D1694u, o, 2);
        cl_upd = true; orig(c); cl_upd = false;
        int spk = cl_spark();
        if ((int)psp_l32(o + 0x48) == 1 && spk != cl_spk && cl_spk >= 0) {
            uint32_t v = psp_l32(o + 0x40) + 0x90;
            hud_guest(c, psp_l32(v + 4), o + (uint32_t)(int32_t)(int16_t)psp_l16(v), 1);
        }
        cl_spk = spk;
        return true;
    }
    if (dir == 0x089F24D0u) {                 // animation state: variants without row 5 outside sparking
        if (c.r[5] <= 2 && !cl_spark()) c.r[5] += CL_NOSPARK;
        return false;
    }
    if (dir == 0x089F1A68u) {                 // normal state: stays open (the technique window closes itself)
        if (hud_guest(c, 0x089D177Cu, o)) {
            psp_s32(o + 0x70, 0);
            uint32_t v = psp_l32(o + 0x40) + 0x90;
            hud_guest(c, psp_l32(v + 4), o + (uint32_t)(int32_t)(int16_t)psp_l16(v), 1);
            uint32_t hold = psp_l32(o + 0x6C);
            if (hold) { FReg st; st.f = (float)((int)hud_guest(c, 0x08837A0Cu, 0) * CL_SPEED); psp_s32(hold + 0x10, st.u); }
        }
        return true;
    }
    if (dir == 0x089F2194u) {                 // draw: row 5 only in sparking, then keys and names in their slots
        uint32_t hold = psp_l32(o + 0x6C), anim = hold ? psp_l32(hold) : 0, root = anim ? psp_l32(anim + 0x30) : 0;
        uint32_t rh = hud_mgr ? psp_l32(hud_mgr + 0x70) : 0, ra = rh ? psp_l32(rh) : 0, ls = ra ? hud_guest(c, 0x08840DC4u, psp_l32(ra + 0x30), cl_str(13)) : 0;
        bool pb = cl_partner_bar();
        uint32_t l1 = ra && !pb ? hud_guest(c, 0x08840DC4u, psp_l32(ra + 0x30), cl_str(14)) : 0;
        static int pl = -1, plogs = 0;
        if ((int)pb * 2 + !!l1 != pl && plogs < 20) { pl = (int)pb * 2 + !!l1; plogs++; printf("charge: barra del companero %s, ranura %s\n", pb ? "visible" : "oculta", l1 ? "BTL_CHARGE_LIST_1V1" : "BTL_CHARGE_LIST"); }
        if (l1) ls = l1;
        else if (ls && !pb) psp_s32(ls + 0xE8, 166);   // root without BTL_CHARGE_LIST_1V1: its 1v1 place (-12, 166)
        if (ls && hold) {                     // attached to BTL_CHARGE_LIST of the loaded root: position from its rect, size = rect / 220x120 (220x141 with six rows)
            int w = (int)psp_l32(ls + 0xEC), h = (int)psp_l32(ls + 0xF0);
            cl_sx = w > 0 ? w / 220.0f : 1.0f; cl_sy = h > 0 ? h / cl_h : 1.0f;
            cl_ox = (float)(int)psp_l32(ls + 0xE4); cl_oy = (float)(int)psp_l32(ls + 0xE8);
            psp_s32(hold + 8, ls);
            hud_apply(hold, cl_sx, cl_sy, false);
            static int ll = 0;
            if (ll < 6) { printf("charge: slot lista %d %d %d %d\n", (int)psp_l32(ls + 0xE4), (int)psp_l32(ls + 0xE8), w, h); ll++; }
        }
        static int bg = -1;   // TTT_LIST_BG=1: the game's own row images of AGS 280 stay under the port's pieces
        if (bg < 0) { const char* e = getenv("TTT_LIST_BG"); bg = e && atoi(e) ? 1 : 0; }
        bool drop = cp_hold && !bg;
        if (drop) cl_ge(0x00, 0xFB0001u);
        orig(c);
        if (drop) cl_ge(0x00, 0xFB0000u);
        if ((int)psp_l32(o + 0x48) >= 0 && root) cl_draw_texts(c, root);
        tw_draw(c, ra);
        return true;
    }
    return false;
}
