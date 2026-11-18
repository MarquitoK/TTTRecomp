#include <atomic>
#include <string>
#include <vector>
static bool guest_ptr_t(uint32_t p) { return p >= 0x08800000u && p < 0x10000000u; }
// TTT_WATCH_CALLS=addr,addr,...  prints who calls those functions through a pointer (once per site)
static void watch_call(uint32_t dir, Ctx& c) {
    static int n = -1;
    static uint32_t goal[8];
    static uint32_t seen_list[64]; static int nv = 0;
    if (n < 0) {
        n = 0;
        const char* e = getenv("TTT_WATCH_CALLS");
        while (e && *e && n < 8) {
            char* fin; unsigned long a = strtoul(e, &fin, 0);
            if (fin == e) break;
            goal[n++] = (uint32_t)a; e = fin;
            while (*e == ',' || *e == ' ') e++;
        }
    }
    for (int i = 0; i < n; i++) {
        if (dir != goal[i]) continue;
        uint32_t k = c.r[31] ^ (c.r[4] << 1);
        bool rep = false;
        for (int j = 0; j < nv; j++) if (seen_list[j] == k) rep = true;
        if (rep || nv >= 64) return;
        seen_list[nv++] = k;
        printf("vigilar 0x%08X: a0 0x%08X a1 0x%08X a2 0x%08X desde 0x%08X\n", dir, c.r[4], c.r[5], c.r[6], c.r[31]);
        fflush(stdout);
    }
}

// TTT_JALR_CENSUS=1: counts indirect calls per target and whether their arguments are constant
struct JalrCensus { uint32_t dir, n, a0, a1; uint8_t a0k, a1k; };
static JalrCensus jalr_cens[8192];
static int jalr_cens_on = -1;

static void jalr_census(uint32_t dir, Ctx& c) {
    if (jalr_cens_on < 0) { const char* e = getenv("TTT_JALR_CENSUS"); jalr_cens_on = (e && atoi(e)) ? 1 : 0; }
    if (!jalr_cens_on) return;
    uint32_t h = (dir >> 2) * 2654435761u;
    for (int i = 0; i < 64; i++) {
        JalrCensus& e = jalr_cens[(h + i) & 8191];
        if (e.n == 0) { e.dir = dir; e.n = 1; e.a0 = c.r[4]; e.a1 = c.r[5]; e.a0k = 1; e.a1k = 1; return; }
        if (e.dir == dir) {
            e.n++;
            if (e.a0 != c.r[4]) e.a0k = 0;
            if (e.a1 != c.r[5]) e.a1k = 0;
            return;
        }
    }
}

void psp_jalr_census_report(long cuadros) {
    if (jalr_cens_on != 1) return;
    int idx[8192], n = 0;
    for (int i = 0; i < 8192; i++) if (jalr_cens[i].n) idx[n++] = i;
    for (int a = 0; a < n; a++)
        for (int b = a + 1; b < n; b++) if (jalr_cens[idx[b]].n > jalr_cens[idx[a]].n) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
    if (cuadros < 1) cuadros = 1;
    printf("censo jalr en %ld cuadros:\n", cuadros);
    for (int i = 0; i < n && i < 60; i++) {
        JalrCensus& e = jalr_cens[idx[i]];
        printf("  jalr 0x%08X %8u  %6.2f/cuadro", e.dir, e.n, (double)e.n / cuadros);
        if (e.a0k) printf("  a0=0x%08X fijo", e.a0);
        if (e.a1k) printf("  a1=0x%08X fijo", e.a1);
        printf("\n");
    }
    fflush(stdout);
    for (int i = 0; i < 8192; i++) jalr_cens[i].n = 0;
}

bool psp_fn_census = false;
static uint32_t cf_dir[32768];
static uint64_t cf_n[32768];

// TTT_TRACE_FN=0xADDR traces every call to that function with its four arguments
uint32_t psp_fn_trace = 0;
static Ctx* psp_ctx_trace = nullptr;
void psp_trace_register(Ctx& c) { psp_ctx_trace = &c; }

static int fn_census_env = -1;
extern bool psp_stall_on;
extern std::atomic<uint32_t> psp_stall_fn, psp_stall_ra;
// TTT_WINDOWS=1: each Ctrl+F7 window writes the functions it ran to ventanas.txt (story menus, item 16b)
extern bool psp_st_mark;
int psp_windows_on = -1;
static uint32_t win_dir[32768], win_n[32768];
static int win_idx = 0;
static bool win_was = false;
static uint32_t win_vb0 = 0;
static void win_fn(uint32_t dir) {
    if (psp_st_mark != win_was) {
        win_was = psp_st_mark;
        uint32_t vb = psp_l32(0x08A78E14u);
        if (win_was) { win_idx++; win_vb0 = vb; memset(win_n, 0, sizeof(win_n)); }
        else if (FILE* f = fopen("ventanas.txt", win_idx == 1 ? "w" : "a")) {
            int b = psp_bank_current();
            fprintf(f, "ventana %d vb %u..%u banco %s\n", win_idx, win_vb0, vb, b >= 0 ? BANKS[b].nombre : "ninguno");
            for (int i = 0; i < 32768; i++) if (win_n[i]) fprintf(f, "fn %08X %u\n", win_dir[i], win_n[i]);
            fclose(f);
            printf("ventanas: ventana %d escrita\n", win_idx); fflush(stdout);
        }
    }
    if (!win_was) return;
    uint32_t h = (dir >> 2) * 2654435761u;
    for (int i = 0; i < 64; i++) {
        uint32_t k = (h + i) & 32767;
        if (win_n[k] == 0) { win_dir[k] = dir; win_n[k] = 1; return; }
        if (win_dir[k] == dir) { win_n[k]++; return; }
    }
}

// TTT_TEXT_LOG=1: strings with button glyphs (U+FF67..U+FF85) given to the font engine, once each
static const char* GLYPH_N[31] = {"+", "START", "SELECT", "cir", "squ", "tri", "cro", "L", "R", "stick", "stick_up", "stick_down",
    "stick_right", "stick_left", "cir2", "squ2", "tri2", "cro2", "squ_hold", "tri_hold", "up", "down", "right", "left",
    "x", "g26", "g27", "g28", "g29", "g30", "g31"};
uint32_t psp_story_field_vb = 0xFFFFFF00u;
uint32_t icons_text(uint32_t s);
uint32_t icons_glyph_table();
static std::string text_show(uint32_t s, uint64_t* hash, bool* glyph) {
    std::string o;
    for (int n = 0; n < 600; n++) {
        uint32_t ch = psp_l16(s + 2 * n);
        if (!ch) break;
        *hash = (*hash ^ ch) * 1099511628211ull;
        if (ch >= 0xFF67u && ch <= 0xFF9Fu) { *glyph = true; o += "{"; o += ch <= 0xFF85u ? GLYPH_N[ch - 0xFF67u] : "s" + std::to_string(ch - 0xFF67u - 31); o += "}"; }
        else if (ch == 10) o += "\\n";
        else if (ch == 13) continue;
        else if (ch < 0x80) o += (char)ch;
        else if (ch < 0x800) { o += (char)(0xC0 | (ch >> 6)); o += (char)(0x80 | (ch & 63)); }
        else { o += (char)(0xE0 | (ch >> 12)); o += (char)(0x80 | ((ch >> 6) & 63)); o += (char)(0x80 | (ch & 63)); }
    }
    return o;
}
static bool text_probe(uint32_t pc, Ctx& c) {
    int reg;
    switch (pc) {
        // story field update (overlay 13933); the address falls mid-function in 56_38
        case 0x08AADCF8u: {
            int b = psp_bank_current();
            if (b >= 0 && BANKS[b].indice == 13932) psp_story_field_vb = psp_l32(0x08A78E14u);
            return true;
        }
        // glyph metric table (lui 0x08A8 + addiu -0x74F8): the port's 57-entry copy
        case 0x08831FCCu: case 0x08832600u: case 0x08832E04u: case 0x08833E70u: case 0x088345BCu: case 0x088356C8u: case 0x08835E70u: case 0x088366C4u: {
            uint32_t t = icons_glyph_table();
            if (t) c.r[pc == 0x08831FCCu ? 30 : (pc == 0x08833E70u || pc == 0x088356C8u || pc == 0x088366C4u) ? 7 : 6] = t + 0x74F8u;
            return true;
        }
        case 0x08831F68u: case 0x088320CCu: reg = 5; break;
        case 0x08832530u: case 0x08832DC4u: case 0x08834574u: reg = 7; break;
        case 0x08835E10u: reg = 2; break;
        default: return false;
    }
    uint32_t s = c.r[reg], r = icons_text(s);
    if (r) c.r[reg] = r;
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_TEXT_LOG"); on = e && atoi(e); }
    if (!on || !guest_ptr_t(s)) return true;
    uint64_t h = 1469598103934665603ull ^ pc;
    bool glyph = false;
    std::string o = text_show(s, &h, &glyph);
    if (r) { o += "\" -> \""; o += text_show(r, &h, &glyph); }
    if (!glyph) return true;
    static std::vector<uint64_t> seen;
    for (uint64_t x : seen) if (x == h) return true;
    if (seen.size() >= 600) return true;
    seen.push_back(h);
    printf("texto: %08X ra %08X s %08X \"%s\"\n", pc, c.r[31], s, o.c_str());
    fflush(stdout);
    return true;
}

static uint32_t fr_dir[1024], fr_ra[1024];
static uint8_t fr_th[1024];
static std::atomic<uint32_t> fr_n{0};

// TTT_STALLS: last guest function entries, printed with the final report (hangs inside port calls to guest code)
void psp_fn_ring_report() {
    if (!psp_stall_on) return;
    uint32_t n = fr_n.load(), m = n < 1024 ? n : 1024;
    printf("\nrastro: ultimas %u entradas a funciones (de %u)\n", m, n);
    std::vector<std::pair<uint64_t, int>> agg;
    for (uint32_t i = 0; i < m; i++) {
        uint32_t k = (n - m + i) & 1023;
        uint64_t key = ((uint64_t)fr_th[k] << 56) | ((uint64_t)fr_dir[k] << 24) | (fr_ra[k] & 0xFFFFFF);
        bool f = false;
        for (auto& x : agg) if (x.first == key) { x.second++; f = true; break; }
        if (!f && agg.size() < 64) agg.push_back({key, 1});
    }
    for (auto& x : agg)
        printf("rastro: hilo %d f_%08X desde 0x%08X x%d\n", (int)(x.first >> 56), (uint32_t)(x.first >> 24) & 0xFFFFFFFF, (uint32_t)(0x08000000 | (x.first & 0xFFFFFF)) - 8, x.second);
    for (uint32_t i = m > 24 ? m - 24 : 0; i < m; i++) {
        uint32_t k = (n - m + i) & 1023;
        printf("rastro: final %u hilo %d f_%08X desde 0x%08X\n", i, fr_th[k], fr_dir[k], fr_ra[k] - 8);
    }
}

void psp_fn_count_slow(uint32_t dir, uint32_t ra) {
    if (psp_windows_on > 0) win_fn(dir);
    if (psp_stall_on) {
        psp_stall_fn.store(dir, std::memory_order_relaxed); psp_stall_ra.store(ra, std::memory_order_relaxed);
        uint32_t k = fr_n.fetch_add(1) & 1023;
        fr_dir[k] = dir; fr_ra[k] = ra; fr_th[k] = (uint8_t)psp_thread_current();
    }
    if (psp_cov_on) psp_coverage_note(COV_FN, dir, ra ? ra - 8 : 0);
    if (fn_census_env < 0) { const char* e = getenv("TTT_FN_CENSUS"); fn_census_env = e && atoi(e); }
    if (!fn_census_env) return;
    uint32_t h = (dir >> 2) * 2654435761u;
    for (int i = 0; i < 32; i++) {
        uint32_t k = (h + i) & 32767;
        if (cf_n[k] == 0) { cf_dir[k] = dir; cf_n[k] = 1; return; }
        if (cf_dir[k] == dir) { cf_n[k]++; return; }
    }
}

void psp_fn_census_report(long cuadros) {
    if (fn_census_env != 1) return;
    int idx[32768], n = 0;
    for (int i = 0; i < 32768; i++) if (cf_n[i]) idx[n++] = i;
    for (int a = 0; a < n; a++)
        for (int b = a + 1; b < n; b++) if (cf_n[idx[b]] > cf_n[idx[a]]) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
    if (cuadros < 1) cuadros = 1;
    printf("censo de funciones en %ld cuadros:\n", cuadros);
    for (int i = 0; i < n && i < 80; i++)
        printf("  fn 0x%08X %10llu  %8.2f/cuadro\n", cf_dir[idx[i]], (unsigned long long)cf_n[idx[i]], (double)cf_n[idx[i]] / cuadros);
    fflush(stdout);
    for (int i = 0; i < 32768; i++) cf_n[i] = 0;
}

