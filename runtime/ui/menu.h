#pragma once
// Game pause, settings file, restart and the game window messages. Included from window.cpp.

#include "ui/theme.h"

// pause: the runtime clock does not advance while a menu or settings window holds the thread
static int pause_n = 0;
static double pause_t0 = 0, pause_total = 0;
static unsigned esc_free_ms = 0;

static double raw_clock() {
    static LARGE_INTEGER freq = {};
    LARGE_INTEGER t;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)freq.QuadPart;
}

static void pause_begin() {
    if (pause_n++ == 0) { pause_t0 = raw_clock(); watchdog_pause.store(1); }
}

static void pause_end() {
    if (pause_n <= 0 || --pause_n > 0) return;
    pause_total += raw_clock() - pause_t0;
    watchdog_pause.store(osk_activo ? 1 : 0);
    esc_free_ms = GetTickCount() + 400;
}

// persistent settings: system language (1 English, 3 Spanish, as on PSP; French is not offered), fullscreen and fit
static int language = 1, saved_fullscreen = 0;
static std::string voice_id;   // voice language chosen in the game's audio page (voice pack mod id, "" = English)
void psp_voice_want(const std::string& id);
extern int snd_3d;

static const char* settings_path() {
    const char* e = getenv("TTT_SETTINGS");
    return e && *e ? e : "settings.cfg";
}

static void settings_save() {
    FILE* f = fopen(settings_path(), "w");
    if (!f) return;
    fprintf(f, "language %d\nfullscreen %d\nfit %d\nsound_3d %d\n", language, completa ? 1 : 0, modo_vista(), snd_3d);
    if (!voice_id.empty()) fprintf(f, "voices %s\n", voice_id.c_str());
    fclose(f);
}

static void settings_load() {
    int fit = -1;
    FILE* f = fopen(settings_path(), "r");
    bool migrated = false;
    if (!f && !getenv("TTT_SETTINGS") && (f = fopen("ajustes.cfg", "r"))) migrated = true;
    if (f) {
        char l[128], vs[100];
        int v;
        while (fgets(l, sizeof(l), f)) {
            if (sscanf(l, "voices %99s", vs) == 1) { voice_id = vs; continue; }
            if ((sscanf(l, "language %d", &v) == 1 || sscanf(l, "idioma %d", &v) == 1) && v >= 1 && v <= 3) language = v;
            else if (sscanf(l, "fullscreen %d", &v) == 1 || sscanf(l, "pantalla_completa %d", &v) == 1) saved_fullscreen = v != 0;
            else if ((sscanf(l, "fit %d", &v) == 1 || sscanf(l, "encaje %d", &v) == 1) && v >= 0 && v <= 2) fit = v;
            else if ((sscanf(l, "sound_3d %d", &v) == 1 || sscanf(l, "sonido_3d %d", &v) == 1) && v >= 0 && v <= 2 && !getenv("TTT_SFX_3D")) snd_3d = v;
        }
        fclose(f);
    }
    const char* e = getenv("TTT_LANGUAGE");
    if (e && *e) {
        if (!strcmp(e, "en")) language = 1;
        else if (!strcmp(e, "es")) language = 3;
        else if (atoi(e) == 1 || atoi(e) == 3) language = atoi(e);
    }
    if (language != 3) language = 1;
    if (fit >= 0 && !getenv("TTT_FIT")) vista_var = fit;
    psp_voice_want(voice_id);
    static const char* label[4] = {"", "ingles", "ingles", "espanol"};
    printf("settings: idioma %s, pantalla completa %d, encaje %d, sonido 3d %d\n", label[language], saved_fullscreen, modo_vista(), snd_3d);
    if (migrated) { settings_save(); remove("ajustes.cfg"); }
}

void psp_voice_store(const std::string& id) { voice_id = id; psp_voice_want(id); settings_save(); }

static void fullscreen_switch() { alternar_completa(); settings_save(); }
static bool restart_requested = false;

static void restart() {
    STARTUPINFOA si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION pi;
    fflush(stdout);
    if (CreateProcessA(nullptr, GetCommandLineA(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        printf("restart: proceso nuevo lanzado\n");
    } else printf("restart: no se pudo lanzar el proceso (error %lu)\n", GetLastError());
    abierta = false;
}

// modern visual style for controls without a manifest in the executable
static void visual_styles(HANDLE* ctx, ULONG_PTR* cookie) {
    *ctx = INVALID_HANDLE_VALUE;
    wchar_t path[MAX_PATH];
    if (!GetTempPathW(MAX_PATH - 40, path)) return;
    wcscat(path, L"tttrecomp_controls.manifest");
    static const char* manifest =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\"><dependency><dependentAssembly>"
        "<assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\" "
        "processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/>"
        "</dependentAssembly></dependency></assembly>";
    FILE* f = _wfopen(path, L"wb");
    if (!f) return;
    fwrite(manifest, 1, strlen(manifest), f);
    fclose(f);
    ACTCTXW ac;
    memset(&ac, 0, sizeof(ac));
    ac.cbSize = sizeof(ac);
    ac.lpSource = path;
    *ctx = CreateActCtxW(&ac);
    if (*ctx == INVALID_HANDLE_VALUE) return;
    if (!ActivateActCtx(*ctx, cookie)) { ReleaseActCtx(*ctx); *ctx = INVALID_HANDLE_VALUE; return; }
    HMODULE cc = LoadLibraryW(L"comctl32.dll");
    auto ini = cc ? (void(WINAPI*)())GetProcAddress(cc, "InitCommonControls") : nullptr;
    if (ini) ini();
}

#include "ui/mods_ui.h"
#include "ui/tpal_editor.h"
#include "ui/graphics.h"
#include "ui/render_editor.h"
#include "ui/map_chooser.h"
#include "ui/links.h"

// game window messages handled by the menu; returns true if it consumes them
static void ov_drop(const std::string& f);
static bool menu_proc(HWND h, UINT m, WPARAM w, LPARAM l, LRESULT* res) {
    *res = 0;
    if (theme_menu_proc(h, m, w, l, res)) return true;
    switch (m) {
        case WM_CREATE: theme_window(h); return false;
        case WM_COPYDATA: if (link_copydata(l)) { *res = TRUE; return true; } return false;
        case WM_DROPFILES: {
            HDROP hd = (HDROP)w;
            UINT n = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; i++) { wchar_t f[4096]; if (DragQueryFileW(hd, i, f, 4096)) ov_drop(mw_u8(f)); }
            DragFinish(hd);
            return true;
        }
        case WM_ENTERSIZEMOVE: pause_begin(); return false;
        case WM_EXITSIZEMOVE: pause_end(); return false;
        case WM_SYSCOMMAND: if ((w & 0xFFF0) == SC_KEYMENU) return true; return false;
        case WM_SYSKEYDOWN: if (w == VK_F10) return true; return false;
        case WM_MOUSEWHEEL: input_wheel(GET_WHEEL_DELTA_WPARAM(w)); return true;
        case WM_DEVICECHANGE: input_devices_changed = true; return false;
    }
    return false;
}
