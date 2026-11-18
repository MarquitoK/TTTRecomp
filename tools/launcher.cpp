// windres tools/launcher.rc -O coff -o generated/launcher_res.o && g++ -O2 -std=c++17 -municode -mwindows -static -o launcher.exe tools/launcher.cpp generated/launcher_res.o -lwinhttp
#include <windows.h>
#include <string>
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <utility>
#include "setup/update_check.h"

static std::wstring raiz;

static std::wstring ancho(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static std::wstring absoluta(const std::wstring& r) {
    if (r.size() > 1 && (r[1] == L':' || (r[0] == L'\\' && r[1] == L'\\'))) return r;
    return raiz + L"\\" + r;
}

static bool existe(const std::wstring& r) { return GetFileAttributesW(r.c_str()) != INVALID_FILE_ATTRIBUTES; }

static void aviso(const std::wstring& t) { MessageBoxW(nullptr, t.c_str(), L"DBZ TTT", MB_OK | MB_ICONERROR); }

static const char* CFG_BASE =
    "args=--window\n"
    "log=launcher.log\n"
    "timing=0\n";
// the file the first alpha wrote (a test name and test window): its saves folder is kept, the rest goes
static const char* CFG_ALPHA =
    "args=--window --name MARCOS\n"
    "TTT_SAVE_DIR=save_test\n"
    "TTT_GLRAST=1\n"
    "TTT_SCALE=8\n"
    "TTT_WINDOW=1920x1200\n"
    "TTT_SHARPNESS=0.25\n"
    "msys=C:\\msys64\n"
    "log=launcher.log\n"
    "timing=0\n";

static std::string read_all(const std::wstring& p) {
    std::string s;
    if (FILE* f = _wfopen(p.c_str(), L"rb")) { char b[4096]; size_t n; while ((n = fread(b, 1, sizeof(b), f)) > 0) s.append(b, n); fclose(f); }
    return s;
}

// a newer release on GitHub: asked once a day; "Update" runs the setup of the install folder in update mode
static bool update_offer() {
    if (GetEnvironmentVariableW(L"TTT_NO_UPDATE", nullptr, 0)) return false;
    std::wstring setup = raiz + L"\\TTTRecomp-Setup.exe";
    std::string cfg = read_all(raiz + L"\\setup.cfg");
    size_t v = cfg.find("version=");
    if (!existe(setup) || v == std::string::npos) return false;
    std::string mine = cfg.substr(v + 8, cfg.find('\n', v) - v - 8);
    while (!mine.empty() && (mine.back() == '\r' || mine.back() == ' ')) mine.pop_back();
    SYSTEMTIME t; GetLocalTime(&t);
    char today[16]; snprintf(today, sizeof(today), "%04d%02d%02d", t.wYear, t.wMonth, t.wDay);
    std::wstring mark = raiz + L"\\update_check.txt";
    if (read_all(mark) == today) return false;
    UpdRelease r;
    if (!upd_latest(mine, r, 3000)) return false;
    if (FILE* f = _wfopen(mark.c_str(), L"wb")) { fputs(today, f); fclose(f); }
    bool es = read_all(raiz + L"\\generated\\src\\settings.cfg").find("language 3") != std::string::npos;
    std::wstring tag = ancho(r.tag);
    std::wstring m = es ? L"Hay una versi\u00F3n nueva de TTTRecomp (" + tag + L").\n\n\u00BFActualizar ahora? Tarda unos minutos y conserva tus mods y partidas."
                        : L"A new version of TTTRecomp is available (" + tag + L").\n\nUpdate now? It takes a few minutes and keeps your mods and saves.";
    if (MessageBoxW(nullptr, m.c_str(), L"TTTRecomp", MB_YESNO | MB_ICONINFORMATION) != IDYES) return false;
    std::wstring cmd = L"\"" + setup + L"\" --update";
    STARTUPINFOW si = {}; si.cb = sizeof(si); PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, raiz.c_str(), &si, &pi)) return false;
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return true;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t buf[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    raiz = buf;
    raiz = raiz.substr(0, raiz.find_last_of(L"\\/"));

    // command line: *.cfg picks the settings file, --timing stamps times, the rest goes to the game
    std::wstring cfg = L"launcher.cfg", extra;
    bool crono = false;
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        if (a == L"--timing" || a == L"--crono") crono = true;
        else if (a.size() > 4 && _wcsicmp(a.c_str() + a.size() - 4, L".cfg") == 0) cfg = a;
        else extra += L" \"" + a + L"\"";
    }
    // a .mkpak or a tttrecomp:// link while the game is open: handed to that window, no second game
    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        bool mk = a.size() > 6 && _wcsicmp(a.c_str() + a.size() - 6, L".mkpak") == 0, ln = _wcsnicmp(a.c_str(), L"tttrecomp://", 12) == 0;
        if (!mk && !ln) continue;
        HWND w = FindWindowW(L"TTTRecomp", nullptr);
        if (!w) break;
        int n = WideCharToMultiByte(CP_UTF8, 0, a.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string u(n > 0 ? n : 1, 0);
        if (n > 0) WideCharToMultiByte(CP_UTF8, 0, a.c_str(), -1, &u[0], n, nullptr, nullptr);
        COPYDATASTRUCT c = {0x54545452, (DWORD)u.size(), &u[0]};
        AllowSetForegroundWindow(ASFW_ANY);
        DWORD_PTR r = 0;
        SendMessageTimeoutW(w, WM_COPYDATA, 0, (LPARAM)&c, SMTO_ABORTIFHUNG, 5000, &r);
        return 0;
    }
    bool main_cfg = _wcsicmp(cfg.c_str(), L"launcher.cfg") == 0;
    cfg = absoluta(cfg);
    if (existe(raiz + L"\\toolchain")) {
        std::string body = read_all(cfg);
        body.erase(std::remove(body.begin(), body.end(), '\r'), body.end());
        if (body == CFG_ALPHA)
            if (FILE* f = _wfopen(cfg.c_str(), L"wb")) { fputs((std::string(CFG_BASE) + "TTT_SAVE_DIR=save_test\n").c_str(), f); fclose(f); }
    }
    if (main_cfg && update_offer()) return 0;
    if (!existe(cfg)) {
        std::wstring old = cfg;
        size_t q = old.rfind(L"launcher");
        if (q != std::wstring::npos) old.replace(q, 8, L"lanzador");
        std::string body = CFG_BASE;
        if (old != cfg && existe(old)) {   // lanzador*.cfg from before the rename: keys and log names updated, old file removed
            body.clear();
            if (FILE* f = _wfopen(old.c_str(), L"rb")) { char l[2048]; while (fgets(l, sizeof(l), f)) body += l; fclose(f); }
            for (auto r : {std::make_pair("registro=", "log="), std::make_pair("crono=", "timing="), std::make_pair("lanzador", "launcher"),
                           std::make_pair("--ventana", "--window"), std::make_pair("--nombre", "--name")})
                for (size_t k = 0; (k = body.find(r.first, k)) != std::string::npos; k += strlen(r.second)) body.replace(k, strlen(r.first), r.second);
            _wremove(old.c_str());
        }
        if (FILE* f = _wfopen(cfg.c_str(), L"wb")) { fputs(body.c_str(), f); fclose(f); }
    }

    std::wstring args, msys, registro = L"launcher.log";
    std::vector<std::wstring> vars;
    if (FILE* f = _wfopen(cfg.c_str(), L"rb")) {
        char l[2048];
        while (fgets(l, sizeof(l), f)) {
            std::string s = l;
            if (s.size() >= 3 && (uint8_t)s[0] == 0xEF && (uint8_t)s[1] == 0xBB && (uint8_t)s[2] == 0xBF) s = s.substr(3);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
            size_t p = s.find_first_not_of(" \t");
            if (p == std::string::npos || s[p] == '#') continue;
            s = s.substr(p);
            size_t eq = s.find('=');
            if (eq == std::string::npos) continue;
            std::wstring k = ancho(s.substr(0, eq)), v = ancho(s.substr(eq + 1));
            while (!k.empty() && (k.back() == L' ' || k.back() == L'\t')) k.pop_back();
            while (!v.empty() && (v[0] == L' ' || v[0] == L'\t')) v.erase(0, 1);
            if (k == L"args") args = v;
            else if (k == L"msys") msys = v;
            else if (k == L"log" || k == L"registro") registro = v;
            else if (k == L"timing" || k == L"crono") crono = crono || v == L"1";
            else { SetEnvironmentVariableW(k.c_str(), v.c_str()); vars.push_back(k + L"=" + v); }
        }
        fclose(f);
    }

    // MSYS2 DLLs (first in PATH, so older copies are not picked up) and python3
    {
        std::vector<std::wstring> cand;
        if (!msys.empty()) cand.push_back(msys);
        cand.push_back(L"C:\\msys64");
        cand.push_back(L"D:\\msys64");
        cand.push_back(L"C:\\tools\\msys64");
        for (auto& c : cand) {
            if (!existe(c + L"\\ucrt64\\bin\\libstdc++-6.dll")) continue;
            DWORD n = GetEnvironmentVariableW(L"PATH", nullptr, 0);
            std::wstring path(n, 0);
            if (n) { GetEnvironmentVariableW(L"PATH", &path[0], n); path.resize(n - 1); }
            path = c + L"\\ucrt64\\bin;" + c + L"\\usr\\bin;" + path;
            SetEnvironmentVariableW(L"PATH", path.c_str());
            break;
        }
    }

    std::wstring dir = raiz + L"\\generated\\src", exe = dir + L"\\tttrecomp.exe";
    if (!existe(exe)) { aviso(L"Game executable not found:\n" + exe); return 1; }
    std::wstring cmd = L"\"" + exe + L"\" ../../game/EBOOT.OLD " + args + extra;

    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE lee = nullptr, escribe = nullptr;
    CreatePipe(&lee, &escribe, &sa, 0);
    SetHandleInformation(lee, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = escribe;
    si.hStdError = escribe;
    PROCESS_INFORMATION pi = {};
    LARGE_INTEGER frec, t0, t;
    QueryPerformanceFrequency(&frec);
    QueryPerformanceCounter(&t0);
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, dir.c_str(), &si, &pi)) {
        aviso(L"The game could not be started (error " + std::to_wstring(GetLastError()) + L").");
        return 1;
    }
    CloseHandle(escribe);
    CloseHandle(pi.hThread);

    // log: game output (and that of its restarts) with optional time since launch
    std::wstring reg_path = absoluta(registro), reg_prev = reg_path;
    size_t dot = reg_prev.find_last_of(L'.');
    reg_prev.insert(dot == std::wstring::npos || dot < reg_prev.find_last_of(L"\\/") + 1 ? reg_prev.size() : dot, L"_previous");
    MoveFileExW(reg_path.c_str(), reg_prev.c_str(), MOVEFILE_REPLACE_EXISTING);
    FILE* reg = _wfopen(reg_path.c_str(), L"wb");
    if (reg) {
        fprintf(reg, "launcher: %ls\n", cmd.c_str());
        for (auto& v : vars) fprintf(reg, "launcher: %ls\n", v.c_str());
        fflush(reg);
    }
    std::string linea;
    char b[4096];
    DWORD n;
    double t_linea = 0;
    while (ReadFile(lee, b, sizeof(b), &n, nullptr) && n) {
        if (!reg) continue;
        QueryPerformanceCounter(&t);
        double s = (double)(t.QuadPart - t0.QuadPart) / frec.QuadPart;
        for (DWORD i = 0; i < n; i++) {
            if (linea.empty()) t_linea = s;
            if (b[i] == '\r') continue;
            linea += b[i];
            if (b[i] == '\n') {
                if (crono) fprintf(reg, "[%9.3f] ", t_linea);
                fwrite(linea.data(), 1, linea.size(), reg);
                linea.clear();
            }
        }
        fflush(reg);
    }
    if (reg) { if (!linea.empty()) fwrite(linea.data(), 1, linea.size(), reg); fclose(reg); }
    CloseHandle(lee);

    DWORD cod = 0;
    GetExitCodeProcess(pi.hProcess, &cod);
    QueryPerformanceCounter(&t);
    double vida = (double)(t.QuadPart - t0.QuadPart) / frec.QuadPart;
    if (cod == 0xC0000135u) aviso(L"MSYS2 DLLs are missing. Check the msys= line in launcher.cfg.");
    else if (cod != 0 && vida < 5) aviso(L"The game closed at startup. Check " + registro + L".");
    CloseHandle(pi.hProcess);
    return 0;
}
