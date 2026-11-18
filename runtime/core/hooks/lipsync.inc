// lip sync (item 29.2) around the fighter draw 0x0880CAF4: the voice that plays moves the mouth.
// Faces: [ent+0x16A8]+0x1C = shown face + 1 (0 = model face), face models at [ent+0x54 + 4 i] (pak entries 3 + i; 1..3 = talk 1..3);
// an animation face (value != 0) always wins. Without talk faces the mouth bone 0x31 turns (bone matrices [[ent+0x16B0] + 4 [ent+0x16B4] + 0xC],
// 128 bytes per bone id, first 4x3 = the bone's own). Both only for this draw: the game state is restored right after.
int psp_lip_level(uint32_t pak, int* known);
static int cv_lip(uint32_t ent);
struct LipSave { uint32_t ctrl = 0, face = 0, mtx = 0; float m[16], o[16]; bool face_set = false, bone_set = false; };
static int lip_on() {
    static int v = 1, n = 0;
    if ((n++ & 255) == 0) { const char* e = getenv("TTT_LIPSYNC"); v = e && *e ? atoi(e) : 1; }
    return v;
}
static float lip_f(uint32_t a) { FReg f; f.u = psp_l32(a); return f.f; }
static void lip_w(uint32_t a, float v) { FReg f; f.f = v; psp_s32(a, f.u); }
static uint32_t lip_pak(uint32_t hdr) {   // character pak that holds the model: big-endian count, entry 2 = the model
    static uint32_t ch[8], cp[8];
    for (int i = 0; i < 8; i++) if (ch[i] == hdr) return cp[i];
    uint32_t pak = 0;
    for (uint32_t b = hdr - 4; b >= 0x08000000u && b + 0x80000u > hdr && !pak; b -= 4) {
        uint32_t cnt = __builtin_bswap32(psp_l32(b));
        if (cnt >= 3 && cnt < 1024 && (__builtin_bswap32(psp_l32(b + 12)) & ~3u) == hdr - b) pak = b;
    }
    static int k = 0;
    ch[k] = hdr; cp[k] = pak; k = (k + 1) & 7;
    return pak;
}
static void lip_begin(Ctx& c, LipSave& s) {
    if (lip_on() <= 0) return;
    uint32_t ent = c.r[4];
    if (!ent || ent >= 0x10000000u) return;
    uint32_t hdr = psp_l32(ent + 0x14);
    if (!hdr || hdr >= 0x10000000u || (psp_l32(hdr) != 0x6C644D70u)) return;
    int known = 0, lv = psp_lip_level(lip_pak(hdr), &known), ov = cv_lip(ent);
    if (ov > -2) lv = ov;   // Character Reference viewer: its sample voice
    static int logs = 0; static uint32_t seen[8];
    bool newone = true;
    for (int i = 0; i < 8; i++) if (seen[i] == hdr) newone = false;
    if (newone && logs < 12) {
        seen[logs & 7] = hdr; logs++;
        int talk = 0;
        for (int i = 1; i <= 3; i++) if (psp_l32(ent + 0x54 + 4 * i)) talk |= 1 << (i - 1);
        printf("lip: luchador 0x%08X modelo 0x%08X pak 0x%08X %s, caras de hablar %d%d%d, control de caras %s\n", ent, hdr, lip_pak(hdr),
               known ? "con voces" : "sin voces conocidas", talk & 1, (talk >> 1) & 1, (talk >> 2) & 1, psp_l32(ent + 0x16A8) ? "si" : "no");
    }
    uint32_t ctrl = psp_l32(ent + 0x16A8);
    if (lv > 0 && ctrl && ctrl < 0x10000000u && psp_l32(ctrl + 0x1C)) return;   // a face from the animation wins
    int best = 0;
    for (int i = 1; i <= 3; i++) if (psp_l32(ent + 0x54 + 4 * i) && (i <= lv || !best)) best = i;
    if (lv > 0 && best && ctrl && ctrl < 0x10000000u) {
        s.ctrl = ctrl; s.face = psp_l32(ctrl + 0x1C); s.face_set = true;
        psp_s32(ctrl + 0x1C, (uint32_t)best + 1);
        return;
    }
    // mouth bone 0x31: bind records (global position at +0x10) give its parent (indent chain) and the head top 0x32
    uint32_t bt = psp_l32(hdr + 0x50), nb = psp_l32(hdr + 8);
    if (bt < 0x08000000u) bt += hdr;
    if (nb > 256) return;
    float P[256][3]; int id[256], par[256], st[64], sp = 0, ij = -1, it = -1;
    for (uint32_t i = 0; i < nb; i++) {
        uint32_t r = bt + 0xA0 * i;
        id[i] = psp_l16(r + 0xA) & 0xFF;
        for (int k = 0; k < 3; k++) P[i][k] = lip_f(r + 0x10 + 4 * k);
        par[i] = sp > 0 ? st[sp - 1] : -1;
        if (sp < 64) st[sp] = (int)i;
        int ind = (int)psp_l32(r + 4) & 0xFFFF;
        if (ind > 0) { sp -= ind; if (sp < -1) sp = i == 0 ? -1 : 0; }
        sp++;
        if (sp < 0) sp = 0;
        if (id[i] == 0x31) ij = (int)i;
        if (id[i] == 0x32) it = (int)i;
    }
    uint32_t mb = psp_l32(ent + 0x16B0);
    if (ij < 0 || !mb || mb >= 0x10000000u) return;
    uint32_t tbl = psp_l32(mb + 4 * psp_l32(ent + 0x16B4) + 0xC);
    if (!tbl || tbl >= 0x10000000u) return;
    uint32_t m = tbl + 0x31 * 128;
    for (int i = 0; i < 16; i++) s.o[i] = s.m[i] = lip_f(m + 4 * i);
    int pj = par[ij];
    auto dist = [&](int a2, int b2) { float d = 0; for (int k = 0; k < 3; k++) d += (P[a2][k] - P[b2][k]) * (P[a2][k] - P[b2][k]); return std::sqrt(d); };
    float head = it >= 0 && pj >= 0 ? dist(it, pj) : 1.8f;
    if (head < 0.1f) head = 1.8f;
    // rows carry the model's per-axis scale (pMdl +0x40, 1 to 512): compare and move in unit directions
    float S[3], U[3][3];
    for (int i = 0; i < 3; i++) {
        S[i] = std::sqrt(s.m[4 * i] * s.m[4 * i] + s.m[4 * i + 1] * s.m[4 * i + 1] + s.m[4 * i + 2] * s.m[4 * i + 2]);
        if (S[i] < 1e-6f) return;
        for (int k = 0; k < 3; k++) U[i][k] = s.m[4 * i + k] / S[i];
    }
    // an animation that moves the mouth wins: the jaw no longer follows its parent rigidly
    if (pj >= 0) {
        uint32_t pm = tbl + (uint32_t)id[pj] * 128;
        float dr = 0, dt = 0;
        for (int i = 0; i < 3; i++) for (int k = 0; k < 3; k++) dr = std::max(dr, std::fabs(lip_f(pm + 4 * (4 * i + k)) / S[i] - U[i][k]));
        for (int i = 12; i < 15; i++) dt = std::max(dt, std::fabs(lip_f(pm + 4 * i) - s.m[i]));
        if (dr > 0.02f || dt > 0.02f * head) return;
    }
    if (lv <= 0) return;
    s.mtx = m; s.bone_set = true;
    bool hinge = pj >= 0 && dist(ij, pj) > 0.05f * head;   // a jaw with its own hinge turns; a jaw sitting on the head pivot only drops
    float R[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, t[3] = {0, 0, 0};
    if (hinge) {
        const char* e = getenv("TTT_LIP_BONE_DEG");
        float a2 = (e && *e ? (float)atof(e) : 20.0f) * (float)lv / 3.0f * 3.14159265f / 180.0f, co = std::cos(a2), si = std::sin(a2);
        R[1][1] = co; R[1][2] = si; R[2][1] = -si; R[2][2] = co;
        const float* p = P[ij];
        for (int k = 0; k < 3; k++) t[k] = p[k] - (p[0] * R[0][k] + p[1] * R[1][k] + p[2] * R[2][k]);
    } else {
        const char* e = getenv("TTT_LIP_DROP");
        t[1] = (e && *e ? (float)atof(e) : 0.05f) * head * (float)lv / 3.0f;   // model +Y is down
    }
    static int lg = 0;
    if (lg < 8 && (lg == 0 || lv == 3)) { lg++; printf("lip: boca por hueso, %s, padre 0x%02X, cabeza %.2f, escala %.0f %.0f %.0f\n", hinge ? "bisagra (giro)" : "sobre la cabeza (baja)", pj >= 0 ? id[pj] : 0xFF, head, S[0], S[1], S[2]); }
    // model space x' = x R + t, vertices x_i = v_i S_i: row_i = S_i sum_k R[i][k] U_k, translation += sum_k t_k U_k
    for (int col = 0; col < 3; col++) {
        float tr = s.m[12 + col];
        for (int k = 0; k < 3; k++) tr += t[k] * U[k][col];
        for (int i = 0; i < 3; i++) lip_w(m + 4 * (4 * i + col), S[i] * (R[i][0] * U[0][col] + R[i][1] * U[1][col] + R[i][2] * U[2][col]));
        lip_w(m + 4 * (12 + col), tr);
    }
}
static void lip_end(LipSave& s) {
    if (s.face_set) psp_s32(s.ctrl + 0x1C, s.face);
    if (s.bone_set) for (int i = 0; i < 16; i++) lip_w(s.mtx + 4 * i, s.o[i]);
}

// item 48: name of the fighter whose voice started last (within ms), for the centred subtitle prefix; 0 = unknown
int psp_lip_started_ms(uint32_t pak);
static uint32_t nm_text(const Ctx& c, uint32_t f, bool* player);
int psp_lip_started_char_ms(uint32_t pak, std::string* last);
int psp_lip_same_char(uint32_t pak, uint32_t idx, std::string* seen);
static uint32_t sub_speaker(const Ctx& c, int ms) {
    uint32_t t = psp_l32(0x08A7DBC8u);
    int n = t ? (int)psp_l32(t) : 0, best = 1 << 30;
    if (n < 1 || n > 8) return 0;
    uint32_t base = psp_l32(t + 4), who = 0;
    std::string seen;
    int same[8] = {-9, -9, -9, -9, -9, -9, -9, -9};
    for (int i = 0; i < n && sub_snd > 0; i++) {   // the sound id is the PACKFILE index of the voice entry: same character folder as the fighter's pak
        uint32_t f = base + 0x1A90u * i, ent = entity_of(f), hdr = ent ? psp_l32(ent + 0x14) : 0;
        if (!hdr || hdr >= 0x10000000u || psp_l32(hdr) != 0x6C644D70u) continue;
        same[i] = psp_lip_same_char(lip_pak(hdr), (uint32_t)sub_snd, &seen);
        if (same[i] == 1) { who = f; break; }
    }
    if (who) { bool pl; return nm_text(c, who, &pl); }
    for (int i = 0; i < n; i++) {
        uint32_t f = base + 0x1A90u * i, ent = entity_of(f), hdr = ent ? psp_l32(ent + 0x14) : 0;
        if (!hdr || hdr >= 0x10000000u || psp_l32(hdr) != 0x6C644D70u) continue;
        int d = psp_lip_started_ms(lip_pak(hdr));
        if (d >= 0 && d <= ms && d < best) { best = d; who = f; }
    }
    if (!who) {   // voices outside the pak's own folder (another form, a voice pack): match by character folder
        std::string last;
        int got[8];
        for (int i = 0; i < n; i++) {
            uint32_t f = base + 0x1A90u * i, ent = entity_of(f), hdr = ent ? psp_l32(ent + 0x14) : 0;
            got[i] = -3;
            if (!hdr || hdr >= 0x10000000u || psp_l32(hdr) != 0x6C644D70u) continue;
            int d = got[i] = psp_lip_started_char_ms(lip_pak(hdr), &last);
            if (d >= 0 && d <= ms && d < best) { best = d; who = f; }
        }
        static int logs = 0;
        if (!who && logs++ < 10) printf("hud: subtitulo sin hablante, sonido %d (%s), carpeta %d %d %d %d, ultima voz %s, luchadores %d %d %d %d\n", sub_snd, seen.c_str(), same[0], same[1], same[2], same[3], last.c_str(), got[0], n > 1 ? got[1] : -9, n > 2 ? got[2] : -9, n > 3 ? got[3] : -9);
    }
    if (!who) return 0;
    bool player;
    return nm_text(c, who, &player);
}
