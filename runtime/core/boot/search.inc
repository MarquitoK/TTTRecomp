static void watch_arm(uint32_t dir);

// F2 in two phases: first while moving, it keeps the floats that change every frame;
// then standing still, it keeps those that stop changing. That is character position or state.
static void find_position() {
    static int phase = 0, count_n = 0;
    static const uint32_t base = 0x08800000u, end_pos = 0x0C000000u;
    static const size_t n = (end_pos - base) / 4;
    static uint32_t* prev = nullptr;
    static uint8_t* alive = nullptr;
    bool requested = psp_window_pos_requested();
    const uint32_t* ram = (const uint32_t*)psp_p(base);
    if (!ram) return;
    if (!prev && requested) {
        prev = (uint32_t*)malloc(n * 4);
        alive = (uint8_t*)malloc(n);
        if (!prev || !alive) return;
    }
    if (!prev) return;
    if (requested && phase == 0) {
        memcpy(prev, ram, n * 4);
        memset(alive, 1, n);
        phase = 1; count_n = 0;
        printf("position: fase 1, MUEVETE durante 90 cuadros\n"); fflush(stdout);
        return;
    }
    if (phase == 1) {
        for (size_t i = 0; i < n; i++) {
            if (!alive[i]) continue;
            uint32_t a = prev[i], b = ram[i];
            float fa, fb; memcpy(&fa, &a, 4); memcpy(&fb, &b, 4);
            bool san = fa > -1e6f && fa < 1e6f && fb > -1e6f && fb < 1e6f;
            if (a == b || !san) alive[i] = 0;
        }
        memcpy(prev, ram, n * 4);
        if (++count_n >= 90) { phase = 2; size_t k = 0; for (size_t i = 0; i < n; i++) if (alive[i]) k++;
            printf("position: fase 1 lista, %zu cambian siempre. QUEDATE QUIETO y pulsa F2\n", k); fflush(stdout); }
        return;
    }
    if (phase == 2 && requested) { memcpy(prev, ram, n * 4); phase = 3; count_n = 0; printf("position: fase 2, quieto 90 cuadros\n"); fflush(stdout); return; }
    if (phase == 3) {
        for (size_t i = 0; i < n; i++) if (alive[i] && prev[i] != ram[i]) alive[i] = 0;
        if (++count_n < 90) return;
        phase = 0;
        int k = 0; uint32_t first_one = 0;
        printf("position: cambian al moverte y no al estar quieto\n");
        for (size_t i = 0; i < n && k < 40; i++)
            if (alive[i]) { float v; memcpy(&v, &ram[i], 4); printf("  pos 0x%08X = %.4f\n", (unsigned)(base + i * 4), v); if (!first_one) first_one = (uint32_t)(base + i * 4); k++; }
        printf("position: %d candidatos\n", k);
        fflush(stdout);
        if (first_one) { printf("position: armo la vigilancia sobre 0x%08X\n", first_one); watch_arm(first_one); }
    }
}

// TTT_HALF=1: every frame finds floats that advanced exactly +2.0 and
// rewrites them to +1.0. With the game drawing at 60, animation runs at the right speed.
static void half_step() {
    static int on = -1;
    static uint32_t start = 0, top = 0;
    if (on < 0) {
        const char* e = getenv("TTT_HALF");
        on = (e && atoi(e) != 0) ? 1 : 0;
        const char* a = getenv("TTT_HALF_START");
        const char* b = getenv("TTT_HALF_END");
        start = a ? (uint32_t)strtoul(a, nullptr, 0) : 0x08800000u;
        top = b ? (uint32_t)strtoul(b, nullptr, 0) : 0x0A000000u;
        if (on) printf("medio paso activo entre 0x%08X y 0x%08X\n", start, top);
    }
    if (!on) return;
    size_t n = (top - start) / 4;
    static float* prev = nullptr;
    static bool ready = false;
    float* ram = (float*)psp_p(start);
    if (!ram) return;
    if (!prev) { prev = (float*)malloc(n * 4); if (!prev) { on = 0; return; } }
    if (!ready) { memcpy(prev, ram, n * 4); ready = true; return; }
    int touched = 0;
    for (size_t i = 0; i < n; i++) {
        float a = prev[i], b = ram[i];
        float d = b - a;
        if (d > 1.999f && d < 2.001f && a > -1e7f && a < 1e7f) { ram[i] = a + 1.0f; touched++; }
        prev[i] = ram[i];
    }
    static int warnings = 0;
    if (warnings < 3 && touched) { warnings++; printf("half step: %d relojes ajustados\n", touched); fflush(stdout); }
}

// TTT_PATCH_SUB=addr:sub,addr:sub  subtracts that value from a 32-bit integer once per frame.
// Used to halve the game's global tick counter when running at 60.
static void patch_counters() {
    static int n = -1;
    static uint32_t dir[8];
    static int32_t res[8];
    if (n < 0) {
        n = 0;
        const char* e = getenv("TTT_PATCH_SUB");
        while (e && *e && n < 8) {
            char* end_pos;
            unsigned long a = strtoul(e, &end_pos, 0);
            if (end_pos == e) break;
            long r = 1;
            if (*end_pos == ':') { e = end_pos + 1; r = strtol(e, &end_pos, 0); }
            dir[n] = (uint32_t)a; res[n] = (int32_t)r; n++;
            e = end_pos;
            while (*e == ',' || *e == ' ') e++;
        }
        for (int i = 0; i < n; i++) printf("patch: 0x%08X resta %d por cuadro\n", dir[i], res[i]);
    }
    for (int i = 0; i < n; i++) psp_s32(dir[i], (uint32_t)((int32_t)psp_l32(dir[i]) - res[i]));
}

// Differential search for the pacing step. F4 in battle: records the words equal to 2 or 2.0
// that stay so for 30 frames. F4 later in a menu: which of those now equal 1 or 1.0.
static void find_rate() {
    static int phase = 0, count_n = 0;
    static const uint32_t base = 0x08800000u, end_pos = 0x0A000000u;
    static const size_t n = (end_pos - base) / 4;
    static uint8_t* bits = nullptr;
    bool requested = psp_window_rate_requested();
    const uint32_t* ram = (const uint32_t*)psp_p(base);
    if (!ram) return;
    if (!bits) { bits = (uint8_t*)calloc(n / 8 + 1, 1); if (!bits) return; }
    if (requested && phase == 0) {
        size_t k = 0;
        for (size_t i = 0; i < n; i++) {
            uint32_t v = ram[i];
            bool si = (v == 2u || v == 0x40000000u);
            if (si) { bits[i >> 3] |= (uint8_t)(1u << (i & 7)); k++; } else bits[i >> 3] &= (uint8_t)~(1u << (i & 7));
        }
        printf("pacing: fase 1, %zu palabras valen 2 o 2.0, vigilando 30 cuadros\n", k);
        fflush(stdout);
        phase = 1; count_n = 0;
        return;
    }
    if (phase == 1) {
        size_t k = 0;
        for (size_t i = 0; i < n; i++) {
            if (!(bits[i >> 3] & (1u << (i & 7)))) continue;
            uint32_t v = ram[i];
            if (v == 2u || v == 0x40000000u) k++; else bits[i >> 3] &= (uint8_t)~(1u << (i & 7));
        }
        if (++count_n >= 30) { phase = 2; printf("pacing: fase 1 lista, quedan %zu estables. Ve a un menu y pulsa F4\n", k); fflush(stdout); }
        return;
    }
    if (phase == 2 && requested) {
        int e = 0, f = 0;
        printf("pacing: candidatos que valian 2 en combate y ahora valen 1\n");
        for (size_t i = 0; i < n && e + f < 80; i++) {
            if (!(bits[i >> 3] & (1u << (i & 7)))) continue;
            uint32_t v = ram[i];
            if (v == 1u) { printf("  ritmo entero 0x%08X\n", (unsigned)(base + i * 4)); e++; }
            else if (v == 0x3F800000u) { printf("  ritmo flotante 0x%08X\n", (unsigned)(base + i * 4)); f++; }
        }
        printf("pacing: %d enteros, %d flotantes\n", e, f);
        fflush(stdout);
        phase = 0;
    }
}

// TTT_POKE=addr:value,... writes that 32-bit value once per frame (to pin a step)
static void set_values() {
    static int n = -1;
    static uint32_t dir[64], val[64];
    if (n < 0) {
        n = 0;
        const char* e = getenv("TTT_POKE");
        while (e && *e && n < 64) {
            char* end_pos;
            unsigned long a = strtoul(e, &end_pos, 0);
            if (end_pos == e) break;
            unsigned long v = 0;
            if (*end_pos == ':') { e = end_pos + 1; v = strtoul(e, &end_pos, 0); }
            dir[n] = (uint32_t)a; val[n] = (uint32_t)v; n++;
            e = end_pos;
            while (*e == ',' || *e == ' ') e++;
        }
        for (int i = 0; i < n; i++) printf("poke: 0x%08X = 0x%08X cada cuadro\n", dir[i], val[i]);
    }
    for (int i = 0; i < n; i++) psp_s32(dir[i], val[i]);
}

// TTT_TWO_TO_ONE=start:end  every frame replaces floats equal to exactly 2.0 with 1.0
// within that range. Per-object structures carry their own copy of the step.
static void two_to_one() {
    static int on = -1;
    static uint32_t start = 0, top = 0;
    if (on < 0) {
        const char* e = getenv("TTT_TWO_TO_ONE");
        on = 0;
        if (e && *e) {
            char* end_pos;
            start = (uint32_t)strtoul(e, &end_pos, 0);
            if (*end_pos == ':') top = (uint32_t)strtoul(end_pos + 1, &end_pos, 0);
            if (top > start) { on = 1; printf("dos a uno entre 0x%08X y 0x%08X\n", start, top); }
        }
    }
    if (!on) return;
    uint32_t* ram = (uint32_t*)psp_p(start);
    if (!ram) return;
    size_t n = (top - start) / 4;
    int k = 0;
    for (size_t i = 0; i < n; i++) if (ram[i] == 0x40000000u) { ram[i] = 0x3F800000u; k++; }
    static int warnings = 0;
    if (warnings < 3 && k) { warnings++; printf("two to one: %d valores cambiados\n", k); fflush(stdout); }
}

