// file layer: disc0:/PSP_GAME/USRDIR/X -> game/X ; disc0:/sce_lbnH_sizeS -> game/game.iso
static const char* GAME_DIR = "../../game";
struct IoFile { FILE* f; long long base, size_b, pos; int sectors; int virtual_; };   // 64 bits: long is 32 on Windows and the virtual disc passes 2 GB
static int io_traces = 0;
static IoFile files[32];
static int iso_warned = 0;
static int n_open = 0;
static unsigned t_last_frame = 0;
static unsigned modules_loaded = 0;
// system dialogs (sceUtility*): 0 none, 1 starting, 2 visible, 3 finished, 4 closing
static int util_state = 0;
static uint32_t util_param = 0;

// savedata parameter block dump: strings, pointers and plausible
// integers, to pin the real offsets instead of guessing them
static void dump_param(uint32_t p) {
    static int done_n = 0;
    if (done_n++ >= 6) return;
    const uint8_t* b = psp_p(p);
    printf("param savedata en 0x%08X\n", p);
    for (int i = 0; i < 1792; i++) {
        int n = 0;
        while (i + n < 1792 && b[i + n] >= 0x20 && b[i + n] < 0x7F) n++;
        if (n >= 3) {
            printf("  +%4d cadena \"%.*s\"\n", i, n, (const char*)(b + i));
            i += n;
        }
    }
    for (int i = 0; i + 4 <= 1792; i += 4) {
        uint32_t v = psp_l32(p + (uint32_t)i);
        if ((v >= 0x08800000u && v < 0x0A000000u) || (v >= 0x04000000u && v < 0x04200000u))
            printf("  +%4d puntero 0x%08X\n", i, v);
        else if (v > 0 && v < 0x200000u && (v & 0xFFF) == 0)
            printf("  +%4d tamano %u\n", i, v);
    }
    fflush(stdout);
}
static int util_frames = 0;
static char util_type[16] = "";
static char player_name[32] = "GOKU";
static char osk_name[32] = "";
static int osk_mode = 0;
void psp_account_settings_request();
bool psp_account_name(char* out, int n);

// TTT_OSK=0 restores the old behaviour: accepts the --name name without asking
static int osk_var = -1;
static int osk_native() {
    if (osk_var < 0) { const char* e = getenv("TTT_OSK"); osk_var = e ? atoi(e) : 1; }
    return osk_var;
}
