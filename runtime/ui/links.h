#pragma once
// .mkpak files and tttrecomp:// links: Windows association (current user), forwarding from the launcher, actions in the Mods window. Included from menu.h
#include <shlobj.h>

bool net_online_on();
static std::mutex link_m;
static std::vector<std::string> link_q;

void psp_link_queue(const char* s) {
    if (!s || !*s) return;
    std::lock_guard<std::mutex> g(link_m);
    link_q.push_back(s);
    printf("enlaces: recibido %s\n", s);
}

static std::wstring link_root() {
    wchar_t b[MAX_PATH * 2], f[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, b, MAX_PATH * 2);
    std::wstring d = b;
    d = d.substr(0, d.find_last_of(L"\\/")) + L"\\..\\..";
    return GetFullPathNameW(d.c_str(), MAX_PATH * 2, f, nullptr) ? f : d;
}

static bool link_reg_set(const std::wstring& key, const wchar_t* name, const std::wstring& val) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, (L"Software\\Classes\\" + key).c_str(), 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    wchar_t cur[2048]; DWORD sz = sizeof(cur), type = 0;
    bool same = RegQueryValueExW(k, name, nullptr, &type, (BYTE*)cur, &sz) == ERROR_SUCCESS && type == REG_SZ && val == std::wstring(cur, wcsnlen(cur, sz / sizeof(wchar_t)));
    if (!same) RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return !same;
}

// .mkpak -> TTTRecomp.Mod and the tttrecomp: protocol, both opened by launcher.exe; rewritten only when the folder changed
static void link_register() {
    std::wstring root = link_root(), launcher = root + L"\\launcher.exe", cmd = L"\"" + launcher + L"\" \"%1\"";
    std::wstring ico_mod = root + L"\\games\\ttt\\icons\\mkpak.ico", ico_app = root + L"\\games\\ttt\\icons\\tttrecomp.ico";
    if (GetFileAttributesW(ico_mod.c_str()) == INVALID_FILE_ATTRIBUTES) ico_mod = launcher + L",0";
    if (GetFileAttributesW(ico_app.c_str()) == INVALID_FILE_ATTRIBUTES) ico_app = launcher + L",0";
    int n = 0;
    n += link_reg_set(L".mkpak", nullptr, L"TTTRecomp.Mod");
    n += link_reg_set(L"TTTRecomp.Mod", nullptr, L"TTTRecomp mod");
    n += link_reg_set(L"TTTRecomp.Mod\\DefaultIcon", nullptr, ico_mod);
    n += link_reg_set(L"TTTRecomp.Mod\\shell\\open\\command", nullptr, cmd);
    n += link_reg_set(L"tttrecomp", nullptr, L"URL:TTTRecomp");
    n += link_reg_set(L"tttrecomp", L"URL Protocol", L"");
    n += link_reg_set(L"tttrecomp\\DefaultIcon", nullptr, ico_app);
    n += link_reg_set(L"tttrecomp\\shell\\open\\command", nullptr, cmd);
    if (n) SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    printf("enlaces: .mkpak y tttrecomp:// %s (%ls)\n", n ? "registrados" : "ya registrados", launcher.c_str());
}

// window icons from games/ttt/icons (also used by every secondary window through theme_create)
static void link_icons(HWND w) {
    std::wstring f = link_root() + L"\\games\\ttt\\icons\\tttrecomp.ico";
    theme_icon_big = (HICON)LoadImageW(nullptr, f.c_str(), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE);
    theme_icon_small = (HICON)LoadImageW(nullptr, f.c_str(), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
    if (theme_icon_big) SendMessageW(w, WM_SETICON, ICON_BIG, (LPARAM)theme_icon_big);
    if (theme_icon_small) SendMessageW(w, WM_SETICON, ICON_SMALL, (LPARAM)theme_icon_small);
    if (!theme_icon_big) printf("enlaces: sin icono de la app (%ls)\n", f.c_str());
}

static bool link_copydata(LPARAM l) {
    COPYDATASTRUCT* c = (COPYDATASTRUCT*)l;
    if (!c || c->dwData != 0x54545452 || !c->lpData || !c->cbData || c->cbData > 8192) return false;
    psp_link_queue(std::string((const char*)c->lpData, strnlen((const char*)c->lpData, c->cbData)).c_str());
    if (IsIconic(ventana)) ShowWindow(ventana, SW_RESTORE);
    SetForegroundWindow(ventana);
    return true;
}

static std::string link_lib_id(const std::string& s) {
    std::string low = s;
    for (auto& c : low) c = (char)tolower((unsigned char)c);
    if (low.rfind("tttrecomp://mod/", 0)) return "";
    std::string id = low.substr(16);
    while (!id.empty() && (id.back() == '/' || id.back() == ' ')) id.pop_back();
    if (id.size() != 8) return "";
    for (char c : id) if (!isalnum((unsigned char)c)) return "";
    return id;
}

static bool ov_link(const std::string& s);
// one pending action per frame once the game runs (never during an online session): opens the Mods window and acts there
static void link_poll() {
    static bool busy = false;
    if (busy || perf_frames < 90 || net_online_on()) return;
    std::string s;
    {
        std::lock_guard<std::mutex> g(link_m);
        if (link_q.empty()) return;
        s = link_q.front();
        link_q.erase(link_q.begin());
    }
    if (ov_link(s)) return;
    busy = true;
    if (!s.compare(0, 5, "fail:")) MessageBoxW(ventana, mw_w(s.substr(5)).c_str(), L"Mods", MB_OK | MB_ICONWARNING);
    else if (s.size() > 6 && !_stricmp(s.c_str() + s.size() - 6, ".mkpak")) mw_install(s);
    else printf("enlaces: no se atiende %s sin el menu de ajustes\n", s.c_str());
    busy = false;
}
