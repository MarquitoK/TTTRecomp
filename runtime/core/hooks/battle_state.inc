// TTT_BATTLE_FPS=60 uses the functions with the patch_60fps.py layers; 30 (default) the originals
// in WLAN battles (mode 11..13) the original 30 Hz logic runs on every machine: the 60 fps half steps are not
// deterministic against the lockstep (one instance lost a ki blast and its clash); TTT_NET_FPS60=1 keeps 60
int psp_fps60() {
    static int v = -1, net = -1;
    if (v < 0) { const char* e = getenv("TTT_BATTLE_FPS"); v = e && atoi(e) == 60; e = getenv("TTT_NET_FPS60"); net = e && atoi(e) == 1; }
    if (!v) return 0;
    uint32_t m = psp_l32(0x08B48A28u);
    if (m >= 11 && m <= 13 && !net) {
        static uint32_t told = 0;
        if (told != m) { told = m; printf("net: combate en red con la logica original a 30 fps (modo %u; la opcion de 60 fps no se aplica en red)\n", m); }
        return 0;
    }
    return v;
}

// battle at 60: with the patch_60fps.py base layer the global step 0x08A78E0C is 1 in battle (2 without it); TTT_60=0 turns it off
static bool battle60() { return patch_battle_step60.val && psp_l32(0x08A78E0Cu) == 1u; }
static float lf(uint32_t d) { uint32_t u = psp_l32(d); float f; memcpy(&f, &u, 4); return f; }
static void ef(uint32_t d, float f) { uint32_t u; memcpy(&u, &f, 4); psp_s32(d, u); }
// combatant entity (table 0x08B61178 indexed by +0x2C) and animation length in 30 Hz ticks (0x089A2E8C)
static uint32_t entity_of(uint32_t cb) {
    uint32_t i = psp_l32(cb + 0x2Cu);
    if (i > 64u) return 0;
    uint32_t e = psp_l32(0x08B61178u + 4u * i);
    return (e >= 0x08800000u && e < 0x10000000u) ? e : 0;
}
static int anim_length(uint32_t cb, uint32_t id) {
    uint32_t e = entity_of(cb);
    if (!e || id < 2u) return -1;
    uint32_t t = psp_l32(e + 0x92Cu);
    if (t < 0x08800000u || t >= 0x10000000u) return -1;
    return (int)(lf(t + 4u * (id - 2u) + 0x1CCu) * 30.0f);
}
static float wrap_angle(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }

// battle task tree (effects): battle engine classes update on even ticks
// TTT_VFX: unset or 30 = 30 Hz gate, 60 = no gate
static int vfx_mode() { return patch_vfx_gate.val; }
static bool tick_odd = false;
static uint32_t battle_tick = 0;
static uint32_t battle_frame = 0xFFFFFF00u;
static bool battle_active() { return psp_l32(0x08A78E14u) - battle_frame <= 2u; }
// input context for the action layer: 1 battle, 2 story field (overlay 13933_366D outside battle), 0 menus
extern uint32_t psp_story_field_vb;
static uint32_t walker_vb = 0xFFFFFF00u, anim_vb = 0xFFFFFF00u;
static bool guest_ptr(uint32_t p) { return p >= 0x08800000u && p < 0x10000000u; }
// clash events (beam, rush and the others of the manager 0x0893BD50): its state word is not 0
int psp_input_clash() {
    uint32_t t = psp_l32(0x08A7DBC8u);
    return guest_ptr(t) && psp_l32(t + 0x40) != 0;
}
// distance from the local fighter to the nearest active fighter of the other team (entity position +0x950)
float psp_input_grab_dist() {
    uint32_t t = psp_l32(0x08A7DBC8u);
    if (!guest_ptr(t)) return -1;
    int n = (int)psp_l32(t);
    uint32_t b = psp_l32(t + 4);
    if (n <= 0 || n > 8 || !guest_ptr(b)) return -1;
    uint32_t me = 0, team = 0;
    for (int i = 0; i < n; i++) if ((int)psp_l32(b + i * 0x1A90u + 0x2C) == local_slot_cache) { me = b + i * 0x1A90u; team = psp_l32(me + 4); }
    uint32_t em = me ? entity_of(me) : 0;
    if (!em) return -1;
    float best = -1;
    for (int i = 0; i < n; i++) {
        uint32_t o = b + i * 0x1A90u, eo;
        if (o == me || !psp_l32(o + 0xC) || psp_l32(o + 4) == team || !(eo = entity_of(o))) continue;
        float dx = lf(eo + 0x950) - lf(em + 0x950), dy = lf(eo + 0x954) - lf(em + 0x954), dz = lf(eo + 0x958) - lf(em + 0x958);
        float dd = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (best < 0 || dd < best) best = dd;
    }
    return best;
}
// TTT_INPUT_LOG=1: command prompt calls (0x089D8148 state, 0x089D82A0 lists) and the pad for 6 s after each change
// the prompt update 0x089D8528 calls 0x089D82A0(obj, n, fighter, list) every tick while a command is shown; from
// ra 0x089D88CC the codes are in the caller's frame at sp+0x9C (directions 0x10..0x13 are followed by 0x20)
static int prompt_log = -1;
static uint32_t prompt_vb = 0, prompt_seen_vb[4] = {0xFFFFFF00u, 0xFFFFFF00u, 0xFFFFFF00u, 0xFFFFFF00u}, prompt_serial[4];
static int prompt_codes[4][12], prompt_n[4];
static void prompt_note(const Ctx& c, uint32_t dir, int k) {
    if (prompt_log < 0) { const char* e = getenv("TTT_INPUT_LOG"); prompt_log = e ? atoi(e) : 0; }
    if (k < 0 || k > 3) k = 0;
    uint32_t vb = psp_l32(0x08A78E14u);
    if (dir == 0x089D82A0u && c.r[31] == 0x089D88CCu && guest_ptr(c.r[29])) {
        int n = (int)c.r[5] > 12 ? 12 : (int)c.r[5], codes[12];
        for (int i = 0; i < n; i++) codes[i] = (int)psp_l32(c.r[29] + 0x9C + 4 * i);
        if (vb - prompt_seen_vb[k] > 4u || n != prompt_n[k] || memcmp(codes, prompt_codes[k], sizeof(int) * n)) { memcpy(prompt_codes[k], codes, sizeof(int) * n); prompt_n[k] = n; prompt_serial[k]++;
            static int pk_logs = 0;
            if (k && pk_logs < 10) { pk_logs++; printf("local: prompt del jugador %d, %d codigos, primero 0x%X\n", k + 1, n, n ? codes[0] : 0); } }
        prompt_seen_vb[k] = vb;
    }
    if (!prompt_log || k) return;
    uint32_t kk[10] = {dir, c.r[5], 0, 0, 0, 0, 0, 0, 0, 0};
    if (dir == 0x089D82A0u && guest_ptr(c.r[29])) for (int i = 0; i < 8; i++) kk[2 + i] = psp_l32(c.r[29] + 0x9C + 4 * i);
    static uint32_t last[10];
    if (!memcmp(kk, last, sizeof(kk))) return;
    memcpy(last, kk, sizeof(kk));
    static int n = 0;
    if (n++ > 400) return;
    prompt_vb = vb;
    printf("input: prompt %08X n %u codigos %X %X %X %X %X %X %X %X ra %08X vb %u\n", dir, kk[1], kk[2], kk[3], kk[4], kk[5], kk[6], kk[7], kk[8], kk[9], c.r[31], vb);
}
int psp_input_prompt_of(int k, int* codes, int max, uint32_t* serial) {
    if (k < 0 || k > 3 || psp_l32(0x08A78E14u) - prompt_seen_vb[k] > 4u) return 0;
    int n = prompt_n[k] < max ? prompt_n[k] : max;
    memcpy(codes, prompt_codes[k], sizeof(int) * n);
    *serial = prompt_serial[k];
    return n;
}
int psp_input_prompt(int* codes, int max, uint32_t* serial) { return psp_input_prompt_of(0, codes, max, serial); }
static void prompt_pad_tick() {
    if (prompt_log <= 0) return;
    uint32_t vb = psp_l32(0x08A78E14u);
    {   // clash manager state and its object, on change; the pad is logged while it lasts
        static uint32_t lt = 1, ls = 1;
        uint32_t t = psp_l32(0x08A7DBC8u), st = guest_ptr(t) ? psp_l32(t + 0x40) : 0xFFFFFFFFu;
        if (t != lt || st != ls) { printf("input: choque objeto %08X estado %d vb %u\n", t, (int)st, vb); lt = t; ls = st; }
        if (st && st != 0xFFFFFFFFu) prompt_vb = vb;
        // words of the clash object that change while it runs (mash counters and power balance)
        static uint32_t prev[80];
        static int lines = 0;
        if (st && st != 0xFFFFFFFFu && lines < 1500) {
            for (int i = 0; i < 80; i++) {
                uint32_t w = psp_l32(t + 0x40 + 4 * i);
                if (w != prev[i] && i) { float f; memcpy(&f, &w, 4); printf("input: choque +0x%X %08X (%g) vb %u\n", 0x40 + 4 * i, w, f, vb); lines++; }
                prev[i] = w;
            }
        }
    }
    if (vb - prompt_vb > 360u) return;
    static uint32_t lb = 0; static uint8_t lx = 128, ly = 128;
    uint32_t b = psp_window_buttons(); uint8_t x, y; psp_window_stick(&x, &y);
    int dx = (int)x - lx, dy = (int)y - ly;
    if (b == lb && dx * dx + dy * dy < 900) return;
    lb = b; lx = x; ly = y;
    printf("input: pad %04X stick %d %d vb %u\n", b, x, y, vb);
}
bool psp_cr_viewer();
// battle pause: the Start check 0x08904F30 returned 1; it runs again only once the battle resumes. While open the controls
// are the menu's even in a clash or the ulti minigame (TTT_PAUSE_CTX=0 restores)
static bool bp_open = false;
static uint32_t bp_vb = 0;
static int bp_on = -1;
static void bp_note(bool opened) {
    if (bp_on < 0) { const char* e = getenv("TTT_PAUSE_CTX"); bp_on = !(e && *e == '0'); }
    if (!bp_on) return;
    uint32_t vb = psp_l32(0x08A78E14u);
    static int logs = 0;
    if (opened) {
        bp_open = true; bp_vb = vb;
        if (logs++ < 40) printf("input: pausa de combate abierta vb %u%s\n", vb, psp_input_clash() ? ", durante un choque o minijuego" : "");
    } else if (bp_open && vb - bp_vb >= 2u) {
        bp_open = false;
        if (logs++ < 40) printf("input: pausa de combate cerrada a los %u vblanks\n", vb - bp_vb);
    }
}
int psp_input_context() {
    if (psp_cr_viewer()) return 0;   // Character Reference 3D viewer: its objects animate, the controls stay the menu's
    uint32_t vb = psp_l32(0x08A78E14u);
    if (bp_open) {
        if (vb - walker_vb <= 8u) return 0;
        bp_open = false;
        printf("input: pausa de combate cerrada, fin del combate\n");
    }
    // battle: the task walker runs and the fighters animate (the pause menu keeps the walker, not the animation)
    if (vb - walker_vb <= 4u && (vb - anim_vb <= 20u || psp_input_clash())) {
        // battle phase [[0x08A7D880]]: 4 combat, 6 victory poses (Start skips), 7 and the end (0x63) the menu after the battle
        static int menu_phase = -1;
        if (menu_phase < 0) { const char* e = getenv("TTT_MENU_PHASE"); menu_phase = e && *e ? atoi(e) : 6; }
        uint32_t m = psp_l32(0x08A7D880u);
        int ph = guest_ptr(m) ? (int)psp_l32(m) : -1;
        static int lph = -2;
        if (ph != lph) { lph = ph; static int logs = 0; if (logs++ < 60) printf("input: fase de combate %d vb %u\n", ph, vb); }
        if (menu_phase > 0 && ph >= menu_phase) return 0;
        return 1;
    }
    int b = psp_bank_current();
    static int last = -2;
    if (b != last) { last = b; printf("input: banco activo %s\n", b >= 0 ? BANKS[b].nombre : "ninguno"); }
    if (b < 0 || BANKS[b].indice != 13932) return 0;
    // story menus (pause, dragon walker, character select): no animation events or no field update
    return patch_story_menus.val && (vb - anim_vb > 20u || vb - psp_story_field_vb > 20u) ? 0 : 2;
}
// WLAN sync: phase 14/13 compares the rand call count between players (0x089C044C, counter at 0x08A7E8B4)
static uint32_t tick_local = 0;
static bool prompt_20 = false;
