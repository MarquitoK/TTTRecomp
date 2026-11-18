// Animation events: at 0x089B1A58 the game collects the events whose frame falls between the previous
// and current frame; that window is replicated here and each table bit triggers rumble.
// TTT_RUMBLE_EVENTS=bit:percent:ms,... replaces the table. TTT_RUMBLE_LOG=1 events, 2 also animation changes.
struct RumbleEvent { int bit, pct, ms; const char* nombre; };
static RumbleEvent rumble_table[16] = {{45, 90, 80, "vibracion"}, {1, 85, 110, "golpe combo"}, {33, 85, 150, "dano acometida"}};
static int rumble_n = 3;
// TTT_RUMBLE_ANIMS=id:percent,... rumbles while the animation is active; TTT_RUMBLE_SLOTS slot mask (default local player)
// ids follow the extract tool numbering (pak entry minus 51); TTT_RUMBLE_SHIFT corrects the offset
struct RumbleAnim { int id, pct; };
static RumbleAnim rumble_anims[96] = {
    {53, 58}, {54, 90},                                   // ki charge and sparking
    {25, 58}, {26, 76}, {47, 58}, {50, 58},                          // fast flight to the rival, up and down
    {261, 76}, {262, 58}, {268, 76}, {269, 58}, {275, 76}, {276, 58},   // blast 1 charge
    {295, 76}, {296, 58}, {302, 76}, {303, 58}, {309, 76}, {310, 58},   // blast 2 charge
    {329, 76}, {330, 58}, {336, 76}, {337, 58}, {343, 76}, {344, 58},   // blast 3 charge
    {263, 90}, {264, 72}, {270, 90}, {271, 72}, {277, 90}, {278, 72},   // blast 1 launch and rush flight
    {297, 90}, {298, 72}, {304, 90}, {305, 72}, {311, 90}, {312, 72},   // blast 2 launch
    {331, 90}, {332, 72}, {338, 90}, {339, 72}, {345, 90}, {346, 72},   // blast 3 launch
    {364, 76}, {365, 58}, {366, 76}, {367, 58}, {368, 76}, {369, 58}, {370, 76},   // beam clash
    {371, 76}, {372, 58}, {373, 58}, {374, 58},                                     // rush clash
    {375, 76}, {376, 58}, {402, 76}, {412, 76}};                                   // transformation, deflect, ki blast
static int rumble_na = 57, rumble_slots = -1, rumble_shift = 0;

static const char* rumble_name(int bit) {
    for (int k = 0; k < rumble_n; k++) if (rumble_table[k].bit == bit && rumble_table[k].nombre) return rumble_table[k].nombre;
    return "evento";
}

// attack type of the 3 blasts: the character pak is located in memory from its animation 0 pointer
// (obj+0xC0 is the table of 0x089B0278; animation 0 is entry 50) and entry 23, 023_blast_param.dat, is read
static bool blast_types(int slot, uint32_t target, int types[3], int log) {
    static uint32_t key[4] = {0, 0, 0, 0};
    static int cache[4][3];
    static bool ok[4] = {false, false, false, false};
    uint32_t p0 = psp_l32(target + 0xC0);
    if (p0 != key[slot]) {
        key[slot] = p0;
        ok[slot] = false;
        auto be = [](uint32_t a) { uint32_t v = psp_l32(a); return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24); };
        if (p0 >= 0x08800000u && p0 < 0x0A000000u) {
            uint32_t tope = p0 > 0x08800000u + 0x01000000u ? p0 - 0x01000000u : 0x08800000u;
            for (uint32_t b = (p0 & ~3u); b >= tope && b > 0x08800000u; b -= 4) {
                if (psp_l32(b) != 0xF1010000u) continue;
                if (b + be(b + 4 + 4 * 50) != p0) continue;
                uint32_t prm = b + be(b + 4 + 4 * 23);
                for (int k = 0; k < 3; k++) cache[slot][k] = psp_l8(prm + 0x13E + k);
                ok[slot] = true;
                printf("rumble: slot %d pak 0x%08X, tipos de blast %d %d %d\n", slot, b, cache[slot][0], cache[slot][1], cache[slot][2]);
                break;
            }
        }
        if (!ok[slot] && log) printf("rumble: slot %d sin pak localizado (animacion 0 en 0x%08X)\n", slot, p0);
    }
    for (int k = 0; k < 3; k++) types[k] = cache[slot][k];
    return ok[slot];
}

// fighter slot of the local player (0x0885862C), asked once per 8 ms
static int local_slot_cache = 0;
static int rumble_local_slot(const Ctx& c) {
    static uint64_t key = ~0ull;
    int& slot = local_slot_cache;
    uint64_t k = psp_time_us() >> 13;
    if (k == key) return slot;
    key = k;
    Ctx c2 = c;
    c2.r[31] = 0;
    psp_call(c2, 0x0885862Cu);
    slot = (int)c2.r[2];
    return slot;
}

// local multiplayer: the rumble of a fighter goes only to the player who controls it
void psp_window_rumble_player(int k, float strong, float weak, int ms);
int psp_input_local(int k, uint32_t* b, uint8_t* x, uint8_t* y);
static int rumble_humans() {
    uint32_t mode = psp_l32(0x08B48A28u), b; uint8_t x, y;
    return mode >= 11 && mode <= 13 ? 1 : psp_input_local(0, &b, &x, &y);
}
// local multiplayer: fighter slot of each player, set per battle by local_mp.inc (team sizes)
static int LMP_SLOT[4] = {0, 2, 1, 3};
static void rumble_to_slot(int n, int slot, float f, float d, int ms) {
    const int* SL = LMP_SLOT;
    if (n < 2) { psp_window_rumble(f, d, ms); return; }
    if (slot < 0) return;
    static uint32_t seen = 0;
    for (int k = 0; k < n && k < 4; k++) if (SL[k] == slot) {
        if (!(seen >> (slot * 4 + k) & 1)) { seen |= 1u << (slot * 4 + k); printf("rumble: luchador %d -> jugador %d (suyo)\n", slot, k + 1); }
        psp_window_rumble_player(k, f, d, ms);
        return;
    }
}

static void events_rumble(const Ctx& c, uint32_t target) {
    static int modo = -1, log = 0;
    if (modo < 0) {
        const char* e = getenv("TTT_RUMBLE"); modo = e ? atoi(e) : 1;
        e = getenv("TTT_RUMBLE_LOG"); log = e ? atoi(e) : 0;
        e = getenv("TTT_RUMBLE_EVENTS");
        if (e && *e) {
            rumble_n = 0;
            const char* q = e;
            while (*q && rumble_n < 16) {
                int b = 0, pc = 100, ms = 100, used = 0;
                if (sscanf(q, "%d:%d:%d%n", &b, &pc, &ms, &used) < 1) break;
                if (b >= 0 && b < 64) rumble_table[rumble_n++] = {b, pc, ms, nullptr};
                q = strchr(q, ',');
                if (!q) break;
                q++;
            }
            printf("rumble: tabla de %d eventos desde TTT_RUMBLE_EVENTS\n", rumble_n);
        }
        e = getenv("TTT_RUMBLE_ANIMS");
        if (e && *e) rumble_na = 0;
        for (const char* q = e; q && *q && rumble_na < 96;) {
            int id = 0, pc = 85;
            if (sscanf(q, "%d:%d", &id, &pc) < 1) break;
            rumble_anims[rumble_na++] = {id, pc};
            q = strchr(q, ',');
            if (q) q++;
        }
        if (e && *e) printf("rumble: %d animaciones desde TTT_RUMBLE_ANIMS\n", rumble_na);
        e = getenv("TTT_RUMBLE_SLOTS"); if (e) rumble_slots = (int)strtol(e, nullptr, 0);
        e = getenv("TTT_RUMBLE_SHIFT"); if (e) rumble_shift = atoi(e);
    }
    if (!modo) return;
    uint32_t st = target + 0xB60, anim = psp_l32(st);
    if (!anim || anim < 0x08800000u || anim >= 0x0A000000u) return;
    int slot = -1;
    for (int k = 0; k < 4; k++) if (psp_l32(0x08B61178u + k * 4) == target) slot = k;
    int n = psp_l8(anim + 1);
    // animation id per layer: 0x089B115C stores the id at +8 of each track (layer 0 at +0xB60, layer 1 at +0xBF8)
    int id0 = psp_l16(target + 0xB68), id1 = psp_l16(target + 0xC00);
    if (log >= 2 && slot >= 0) {
        static int last0[4] = {-1, -1, -1, -1}, last1[4] = {-1, -1, -1, -1};
        if (last0[slot] != id0 || last1[slot] != id1) {
            last0[slot] = id0; last1[slot] = id1;
            uint64_t all_v = 0;
            for (int i = 0; i < n; i++) all_v |= (uint64_t)psp_l32(anim + 0x94 + i * 16) | ((uint64_t)psp_l32(anim + 0x98 + i * 16) << 32);
            printf("anim: slot %d id %d capa1 %d cuadros %u eventos %d bits", slot, id0, id1, psp_l16(anim + 2), n);
            for (int b = 0; b < 64; b++) if (all_v >> b & 1) printf(" %d", b);
            printf("\n");
        }
    }
    // renewed every frame while the animation is active; lasts at least 300 ms at start
    int mask = rumble_slots, nh = rumble_humans();
    if (mask < 0 && nh >= 2) { mask = 0; for (int k = 0; k < nh && k < 4; k++) mask |= 1 << LMP_SLOT[k]; }
    else if (mask < 0) {
        int l = patch_rumble_local.val ? rumble_local_slot(c) : 0;
        mask = l >= 0 && l < 4 ? 1 << l : 0;
    }
    if (slot >= 0 && (mask >> slot & 1)) {
        int types[3];
        bool types_ok = blast_types(slot, target, types, log);
        static int prev0[4] = {-1, -1, -1, -1}, prev1[4] = {-1, -1, -1, -1};
        bool added_new = prev0[slot] != id0 || prev1[slot] != id1;
        prev0[slot] = id0; prev1[slot] = id1;
        static uint64_t since[4];
        uint64_t now_us = psp_time_us();
        if (added_new) since[slot] = now_us;
        uint32_t ct = psp_l32(0x08A7DBC8u);
        int cst = ct >= 0x08800000u && ct < 0x10000000u ? (int)psp_l32(ct + 0x40) : 0;
        for (int k = 0; k < rumble_na; k++) {
            int id = rumble_anims[k].id + rumble_shift;
            if (id != id0 && id != id1) continue;
            // clash poses can outlast the event (the fighter keeps them until he moves): only while it runs; any other
            // pose but the ki charge stops rumbling after 4 s without changing
            int aid = rumble_anims[k].id;
            bool stale = !cst && aid != 53 && aid != 54 && now_us - since[slot] > 4000000ull;
            if ((aid >= 364 && aid <= 374 && !cst) || stale) {
                static int cut_logs = 0;
                static int cut_last[4] = {-1, -1, -1, -1};
                if (cut_last[slot] != aid && cut_logs < 40) { cut_logs++; cut_last[slot] = aid; printf("rumble: slot %d animacion %d (capas %d %d) sin vibrar, choque %d%s\n", slot, aid, id0, id1, cst, stale ? ", 4 s sin cambio" : ""); }
                break;
            }
            {
                static int on_logs = 0;
                static int on_last[4] = {-1, -1, -1, -1};
                if (on_last[slot] != aid && on_logs < 60) { on_logs++; on_last[slot] = aid; printf("rumble: slot %d animacion %d (capas %d %d) vibra, choque %d\n", slot, aid, id0, id1, cst); }
            }
            // a blast charge only rumbles if the attack is "Energy - Chargeable" (types 2 and 3 of 023_blast_param.dat)
            int rel = rumble_anims[k].id - 261;
            if (types_ok && rel >= 0 && rel < 102) {
                int r = rel % 34, t = types[rel / 34];
                bool load_n = r == 0 || r == 1 || r == 7 || r == 8 || r == 14 || r == 15;
                if (load_n && t != 2 && t != 3) break;
            }
            float p = rumble_anims[k].pct / 100.0f;
            rumble_to_slot(nh, slot, p, p, added_new ? 300 : 80);
            break;
        }
    }
    float cur, prev; uint32_t u;
    u = psp_l32(st + 0x138); memcpy(&cur, &u, 4);
    u = psp_l32(st + 0x13C); memcpy(&prev, &u, 4);
    int s4 = (int)(cur + 1.0f / 60.0f), s2 = (int)(prev + 1.0f / 60.0f);
    if (s4 == s2) s2--;
    for (int i = 0; i < n && i < 255; i++) {
        uint32_t ev = anim + 0x94 + i * 16;
        int fr = psp_l16(ev + 8);
        if (fr <= s2 || fr > s4) continue;
        uint64_t f = (uint64_t)psp_l32(ev) | ((uint64_t)psp_l32(ev + 4) << 32);
        const RumbleEvent* mejor = nullptr;
        for (int k = 0; k < rumble_n; k++)
            if (f >> rumble_table[k].bit & 1 && (!mejor || rumble_table[k].pct > mejor->pct)) mejor = &rumble_table[k];
        if (!mejor) continue;
        if (log) printf("rumble: %s (bit %d) cuadro %d slot %d anm 0x%08X\n", rumble_name(mejor->bit), mejor->bit, fr, slot, anim);
        float p = mejor->pct / 100.0f;
        if (mejor->bit == 1) continue;   // combo hit: rumbles when the rival loses life (events_fx)
        rumble_to_slot(nh, slot, p, p, mejor->ms);
    }
}

// screen effects from animation events (gpu_gl.h fx_frame): bit 12 VFX4 turns the rush blur on, bit 13 VFX5 turns it off
// (BT3 EftFollow_UpdateLight); bit 46 camera shake and bit 33 rush damage are heavy impacts
extern float psp_fx_ev[8];
// the rush ends with the attacker: blur off when the victim left its animations or nobody else is in that skill's attack animations
static void rush_end_check() {
    int c = (int)psp_fx_ev[0] - 1;
    if (c < 0 || c > 3) return;
    uint32_t t = psp_l32(0x08B61178u + c * 4);
    int e = t ? psp_l16(t + 0xB68) - rumble_shift : -1, b = -1;
    for (int k = 0; k < 3; k++) if (e >= 289 + 34 * k && e <= 294 + 34 * k) b = k;
    bool atk = false;
    for (int k = 0; k < 4 && b >= 0; k++) {
        uint32_t o = psp_l32(0x08B61178u + k * 4);
        if (k == c || !o) continue;
        int a = psp_l16(o + 0xB68) - rumble_shift;
        if (a >= 261 + 34 * b && a < 289 + 34 * b) atk = true;
    }
    {   // diagnosis (item 2, session 66): every change of the animations while the blur is on
        static int prev[5] = {-2, -2, -2, -2, -2}, dlogs = 0;
        int cur[5] = {c, -1, -1, -1, -1};
        for (int k = 0; k < 4; k++) { uint32_t o = psp_l32(0x08B61178u + k * 4); cur[k + 1] = o ? psp_l16(o + 0xB68) - rumble_shift : -1; }
        if (memcmp(cur, prev, sizeof(cur)) && dlogs < 120) {
            dlogs++; memcpy(prev, cur, sizeof(cur));
            printf("fx: blur de acometida activo, victima %d: animaciones %d %d %d %d%s\n", c, cur[1], cur[2], cur[3], cur[4], b >= 0 && atk ? "" : " (se apaga)");
        }
    }
    if (b >= 0 && atk) return;
    psp_fx_ev[0] = 0;
    static int logs = 0;
    if (logs < 40) { logs++; printf("fx: fin de la acometida contra el luchador %d (%s): fin del blur\n", c, b < 0 ? "salio de la animacion" : "el atacante termino"); }
}
static void events_fx(const Ctx& c, uint32_t target) {
    rush_end_check();
    uint32_t st = target + 0xB60, anim = psp_l32(st);
    if (!anim || anim < 0x08800000u || anim >= 0x0A000000u) return;
    int slot = -1;
    for (int k = 0; k < 4; k++) if (psp_l32(0x08B61178u + k * 4) == target) slot = k;
    if (slot < 0) return;
    static uint32_t last_anim[4];
    bool fresh = anim != last_anim[slot];
    last_anim[slot] = anim;
    int n = psp_l8(anim + 1), id0 = psp_l16(target + 0xB68), id1 = psp_l16(target + 0xC00);
    static int logs = 0;
    {   // dashes (rumble ids): to the rival, up, down, and the rush flight of each blast; a flight id only counts while the
        // fighter really moves (another character can use that slot for a still pose): per tick at least TTT_FX_SPEEDMIN of the distance to the nearest rival
        static const int DASH[] = {25, 26, 47, 50}, FLIGHT[] = {264, 271, 278, 298, 305, 312, 332, 339, 346};
        static float pos[4][3], minr = -1;
        static bool have[4], was[4];
        if (minr < 0) { const char* ev = getenv("TTT_FX_SPEEDMIN"); minr = ev ? (float)atof(ev) : 0.02f; }
        auto rd = [](uint32_t a) { uint32_t u = psp_l32(a); float f; memcpy(&f, &u, 4); return f; };
        float p[3] = {rd(target + 0x950), rd(target + 0x954), rd(target + 0x958)}, step = 0, dist = 1e30f;
        if (have[slot]) step = std::sqrt((p[0] - pos[slot][0]) * (p[0] - pos[slot][0]) + (p[1] - pos[slot][1]) * (p[1] - pos[slot][1]) + (p[2] - pos[slot][2]) * (p[2] - pos[slot][2]));
        memcpy(pos[slot], p, sizeof(p)); have[slot] = true;
        for (int k = 0; k < 4; k++) {
            uint32_t o = psp_l32(0x08B61178u + k * 4);
            if (k == slot || !o) continue;
            float q[3] = {rd(o + 0x950), rd(o + 0x954), rd(o + 0x958)};
            dist = std::min(dist, std::sqrt((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2])));
        }
        bool d = false, fl = false;
        for (int a : DASH) if (id0 == a + rumble_shift || id1 == a + rumble_shift) d = true;
        for (int a : FLIGHT) if (id0 == a + rumble_shift || id1 == a + rumble_shift) fl = true;
        bool moving = dist < 1e29f && dist > 0 && step >= minr * dist;
        if (fl && moving) d = true;
        {   // only the local players' own dashes and flights blur the screen (TTT_FX_SPEEDALL=1: every fighter, old)
            static int all = -1;
            if (all < 0) { const char* ev = getenv("TTT_FX_SPEEDALL"); all = ev && *ev == '1'; }
            int nh = rumble_humans(); bool own = false;
            if (nh >= 2) { for (int k = 0; k < nh && k < 4; k++) if (LMP_SLOT[k] == slot) own = true; }
            else own = rumble_local_slot(c) == slot;
            static int ologs = 0;
            if (d && !own && !all && fresh && ologs < 20) { ologs++; printf("fx: dash o vuelo del luchador %d (no es del jugador local): sin blur\n", slot); }
            if (!own && !all) d = false;
        }
        static int slogs = 0;
        if (fl && (moving != was[slot] || fresh) && slogs < 40) { slogs++; printf("fx: luchador %d en animacion de vuelo %d, avance %.3f de %.3f (%.3f): %s\n", slot, id0, step, dist, dist > 0 ? step / dist : 0.0f, moving ? "blur" : "quieto, sin blur"); }
        was[slot] = fl && moving;
        int m = (int)psp_fx_ev[4];
        m = d ? m | 1 << slot : m & ~(1 << slot);
        psp_fx_ev[4] = (float)m;
    }
    // the rush victim's animations (skill_003/004/005_enemy_1..5 and _assault): the only ones whose VFX4/VFX5 drive the rush blur
    int e = id0 - rumble_shift;
    bool enemy = (e >= 289 && e <= 294) || (e >= 323 && e <= 328) || (e >= 357 && e <= 362);
    if (enemy && fresh && (int)psp_fx_ev[0] != slot + 1) {   // the rush starts with the victim's animation, VFX4 or not
        psp_fx_ev[0] = (float)(slot + 1); psp_fx_ev[1] += 1.0f;
        if (logs < 40) { logs++; printf("fx: acometida contra el luchador %d (animacion %d): blur de acometida\n", slot, id0); }
    }
    float cur, prev; uint32_t u;
    u = psp_l32(st + 0x138); memcpy(&cur, &u, 4);
    u = psp_l32(st + 0x13C); memcpy(&prev, &u, 4);
    int s4 = (int)(cur + 1.0f / 60.0f), s2 = (int)(prev + 1.0f / 60.0f);
    if (s4 == s2) s2--;
    if (fresh) s2 = -1;   // a new animation: its frame 0 counts
    float shake = 0, hit = 0;
    bool thr = (id0 >= 148 + rumble_shift && id0 <= 157 + rumble_shift) || (id0 >= 390 + rumble_shift && id0 <= 394 + rumble_shift);   // throws
    for (int i = 0; i < n && i < 255; i++) {
        uint32_t ev = anim + 0x94 + i * 16;
        int fr = psp_l16(ev + 8);
        if (fr <= s2 || fr > s4) continue;
        uint64_t f = (uint64_t)psp_l32(ev) | ((uint64_t)psp_l32(ev + 4) << 32);
        if ((f >> 12 & 1) && enemy) { psp_fx_ev[0] = (float)(slot + 1); psp_fx_ev[1] += 1.0f; if (logs < 40) { logs++; printf("fx: VFX4 en el luchador %d, animacion %d cuadro %d: blur de acometida\n", slot, id0, fr); } }
        if ((f >> 13 & 1) && enemy) { psp_fx_ev[0] = 0; if (logs < 40) { logs++; printf("fx: VFX5 en el luchador %d, animacion %d cuadro %d: fin del blur\n", slot, id0, fr); } }
        if (f >> 46 & 1) shake = 0.8f;
        if ((f >> 1 & 1) && !thr) hit = std::max(hit, 0.5f);   // combo hit
        if ((f >> 33 & 1) && !thr) hit = 1.0f;                  // rush hit
    }
    if (shake > 0) { psp_fx_ev[6] += 1.0f; psp_fx_ev[7] = shake; }
    // a hit counts when both happen within 0.4 s, in any order: a hit event in the animation of a human fighter and a life
    // loss of a fighter of the other team (ki blasts, throws and the CPU partner's hits do not count)
    struct Drop { uint64_t t = 0; int vs = -1, team = -1, dmg = 0, was = 0, life = 0; };
    static Drop drops[8];
    static uint64_t hit_t[4]; static float hit_k[4];
    static int last_life[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    static int cur_anim[4][2]; static uint64_t blast_t[8];
    cur_anim[slot][0] = id0 - rumble_shift; cur_anim[slot][1] = id1 - rumble_shift;
    uint32_t tb = psp_l32(0x08A7DBC8u);
    int nc = tb ? (int)psp_l32(tb) : 0;
    uint32_t base = tb ? psp_l32(tb + 4) : 0;
    if (nc < 1 || nc > 8 || !base) return;
    uint64_t now = psp_time_us();
    auto team_of = [&](int es, bool* human) { for (int j = 0; j < nc; j++) { uint32_t a = base + 0x1A90u * j; if ((int)psp_l32(a + 0x2C) == es) { if (human) *human = psp_l32(a + 0x1690) == 0; return (int)psp_l32(a + 4); } } return -1; };
    auto fire = [&](int att, const Drop& d, float k0) {
        float k = k0 >= 1.0f ? 1.0f : std::min(std::max(d.dmg / 900.0f, 0.5f), 1.0f);
        psp_fx_ev[2] += 1.0f; psp_fx_ev[3] = k; psp_fx_ev[5] = (float)(d.vs + 1);
        bool rb = false;
        if (k0 < 1.0f) for (int r = 0; r < rumble_n; r++) if (rumble_table[r].bit == 1) {   // rush hits keep their own rumble (bit 33)
            float p = rumble_table[r].pct / 100.0f;
            rumble_to_slot(rumble_humans(), att, p, p, rumble_table[r].ms); rb = true;
        }
        if (logs < 60) { logs++; printf("fx: golpe del luchador %d al %d (vida %d -> %d, dano %d), impacto %.2f%s\n", att, d.vs, d.was, d.life, d.dmg, k, rb ? ", vibra" : ""); }
    };
    if (hit > 0) {
        bool human = false; int t = team_of(slot, &human);
        if (human) {
            int best = -1;
            for (int i = 0; i < 8; i++) if (drops[i].t && now - drops[i].t < 400000ull && drops[i].team != t && (best < 0 || drops[i].t > drops[best].t)) best = i;
            if (best >= 0) { fire(slot, drops[best], hit); drops[best] = Drop(); }
            else { hit_t[slot] = now; hit_k[slot] = hit; }
        }
    }
    for (int i = 0; i < nc; i++) {
        uint32_t e = base + 0x1A90u * i;
        int life = (int)psp_l32(e + 0x1A34), was = last_life[i];
        last_life[i] = life;
        if (was < 0 || life >= was || life < 0 || was > 1000000) continue;
        Drop d; d.t = now; d.vs = (int)psp_l32(e + 0x2C); d.team = (int)psp_l32(e + 4); d.dmg = was - life; d.was = was; d.life = life;
        int att = -1;
        for (int k = 0; k < 4; k++) {
            bool human = false;
            if (hit_t[k] && now - hit_t[k] < 400000ull && team_of(k, &human) != d.team && human && (att < 0 || hit_t[k] > hit_t[att])) att = k;
        }
        if (att >= 0) { fire(att, d, hit_k[att]); hit_t[att] = 0; continue; }
        // blasts (kamehameha, genkidama...): a human attacker of the other team in one of the 3 blast blocks (261..362,
        // not the rush victim animations); ki volleys (412) are outside; beams hit in ticks: one impact each 250 ms
        for (int k = 0; k < 4 && att < 0; k++) {
            bool human = false;
            if (team_of(k, &human) == d.team || !human) continue;
            for (int l = 0; l < 2; l++) {
                int a = cur_anim[k][l], rel = a - 261;
                bool vict = (a >= 289 && a <= 294) || (a >= 323 && a <= 328) || (a >= 357 && a <= 362);
                if (rel >= 0 && rel < 102 && !vict) att = k;
            }
        }
        if (att >= 0) { if (now - blast_t[i] > 250000ull) { blast_t[i] = now; fire(att, d, 0.0f); } continue; }
        drops[i] = d;
    }
}

// TTT_RUMBLE_SAS=1: every sound effect pulses the pad and logs a trace to locate hits
void psp_sfx_hook(uint32_t dir, uint32_t tam, uint32_t ra) {
    static int modo = -1;
    if (modo < 0) { const char* e = getenv("TTT_RUMBLE_SAS"); modo = e ? atoi(e) : 0; }
    if (!modo) return;
    static int n = 0;
    if (n++ < 400) printf("sfx: vag 0x%08X tam %u desde 0x%08X cuadro %llu\n", dir, tam, ra, (unsigned long long)(psp_time_us() / 16667));
    psp_window_rumble(0.6f, 0.3f, 70);
}

