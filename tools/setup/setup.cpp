// TTTRecomp setup assistant: checks the ISO, downloads the toolchain, runs tools/build_from_iso.py and shows every step.
// windres tools/setup/setup.rc -O coff -o generated/setup_res.o
// --update: run from the install folder by the launcher, rebuilds only what the new version changed
// g++ -O2 -std=c++17 -municode -mwindows -static -o TTTRecomp-Setup.exe tools/setup/setup.cpp generated/setup_res.o -lopengl32 -lglu32 -lgdi32 -lwinhttp -lbcrypt -lole32 -luuid -lshell32 -lcomdlg32
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <commdlg.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "../../runtime/gpu/image.h"
#include "update_check.h"

namespace fs = std::filesystem;
static const char* VERSION = "0.1.1-alpha";   // the release tag is "v" + this
static const wchar_t* TC_URL = L"https://github.com/brechtsanders/winlibs_mingw/releases/download/15.2.0posix-14.0.0-ucrt-r7/winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-14.0.0-r7.zip";
static const char* TC_SHA = "cb2fbad6162540cdf5e1facdce08d4dac359e8cf64f7f696a99274291763b815";
static const wchar_t* PY_URL = L"https://www.python.org/ftp/python/3.13.17/python-3.13.17-embed-amd64.zip";
static const char* PY_SHA = "9e35319bffedab8b36ad44d8f377cb4f1733bcec257a0b6d04d94f01486ddb38";
static const char* EBOOT_SHA = "eb7bffa629a689df";
static const float VW = 960, VH = 600;

// ---- text and paths
static std::string u8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, 0);
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}
static std::wstring wd(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, 0);
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}
static bool es = true;
static const char* T(const char* en, const char* s) { return es ? s : en; }
static fs::path exe_dir() { wchar_t b[MAX_PATH]; GetModuleFileNameW(nullptr, b, MAX_PATH); return fs::path(b).parent_path(); }
static fs::path payload_dir;
static fs::path port_dir() {
    if (!payload_dir.empty()) return payload_dir;
    fs::path e = exe_dir();
    if (fs::exists(e / "port" / "tools" / "build_from_iso.py")) return e / "port";
    return e;
}
static std::string gb(uint64_t b) { char t[32]; snprintf(t, sizeof(t), "%.1f GB", b / 1073741824.0); return t; }
static std::string mb(uint64_t b) { char t[32]; snprintf(t, sizeof(t), "%.0f MB", b / 1048576.0); return t; }
static std::string clock_s(double s) { char t[32]; int m = (int)(s / 60); snprintf(t, sizeof(t), "%d:%02d", m, (int)s % 60); return t; }

// ---- hashing
struct Sha {
    BCRYPT_ALG_HANDLE a = nullptr; BCRYPT_HASH_HANDLE h = nullptr; int len = 32;
    Sha(const wchar_t* alg, int n) : len(n) { BCryptOpenAlgorithmProvider(&a, alg, nullptr, 0); BCryptCreateHash(a, &h, nullptr, 0, nullptr, 0, 0); }
    ~Sha() { if (h) BCryptDestroyHash(h); if (a) BCryptCloseAlgorithmProvider(a, 0); }
    void add(const void* p, size_t n) { BCryptHashData(h, (PUCHAR)p, (ULONG)n, 0); }
    std::string hex() { unsigned char d[64]; BCryptFinishHash(h, d, len, 0); std::string s; char t[3]; for (int i = 0; i < len; i++) { snprintf(t, 3, "%02x", d[i]); s += t; } return s; }
};

// ---- ISO check (ISO 9660, PARAM.SFO, EBOOT fingerprint)
struct IsoInfo { bool read = false, psp = false, game = false, eboot = false; std::string id, title; uint64_t size = 0; };
static IsoInfo iso_check(const std::wstring& path) {
    IsoInfo r;
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return r;
    r.read = true;
    _fseeki64(f, 0, SEEK_END); r.size = (uint64_t)_ftelli64(f);
    std::vector<uint8_t> pvd(2048);
    _fseeki64(f, 16 * 2048, SEEK_SET);
    if (fread(pvd.data(), 1, 2048, f) != 2048 || memcmp(&pvd[1], "CD001", 5)) { fclose(f); return r; }
    auto rd = [&](uint32_t lba, uint32_t n) { std::vector<uint8_t> d(n); _fseeki64(f, (int64_t)lba * 2048, SEEK_SET); d.resize(fread(d.data(), 1, n, f)); return d; };
    auto find = [&](uint32_t lba, uint32_t n, const char* name, uint32_t* olba, uint32_t* osz) {
        std::vector<uint8_t> d = rd(lba, n);
        for (size_t p = 0; p < d.size();) {
            uint8_t len = d[p];
            if (!len) { p = (p / 2048 + 1) * 2048; continue; }
            if (p + 33 > d.size()) break;
            std::string nm((const char*)&d[p + 33], d[p + 32]);
            nm = nm.substr(0, nm.find(';'));
            if (!_stricmp(nm.c_str(), name)) { memcpy(olba, &d[p + 2], 4); memcpy(osz, &d[p + 10], 4); return true; }
            p += len;
        }
        return false;
    };
    uint32_t lba, sz, l2, s2, l3, s3;
    memcpy(&lba, &pvd[156 + 2], 4); memcpy(&sz, &pvd[156 + 10], 4);
    if (find(lba, sz, "PSP_GAME", &l2, &s2)) {
        r.psp = true;
        if (find(l2, s2, "PARAM.SFO", &l3, &s3)) {
            std::vector<uint8_t> d = rd(l3, s3);
            if (d.size() > 20 && !memcmp(d.data(), "\0PSF", 4)) {
                uint32_t kt, dt, n; memcpy(&kt, &d[8], 4); memcpy(&dt, &d[12], 4); memcpy(&n, &d[16], 4);
                for (uint32_t i = 0; i < n && 20 + i * 16 + 16 <= d.size(); i++) {
                    uint16_t ko; uint32_t ln, dofs; memcpy(&ko, &d[20 + i * 16], 2); memcpy(&ln, &d[24 + i * 16], 4); memcpy(&dofs, &d[32 + i * 16], 4);
                    std::string k((const char*)&d[kt + ko]), v((const char*)&d[dt + dofs], strnlen((const char*)&d[dt + dofs], ln));
                    if (k == "DISC_ID") r.id = v; else if (k == "TITLE") r.title = v;
                }
            }
        }
        r.game = r.id == "ULUS10537";
        uint32_t ls, ss;
        if (r.game && find(l2, s2, "SYSDIR", &ls, &ss)) {
            for (const char* e : {"EBOOT.OLD", "EBOOT.BIN"}) {
                if (!find(ls, ss, e, &l3, &s3)) continue;
                std::vector<uint8_t> d = rd(l3, s3);
                if (d.size() < 4 || memcmp(d.data(), "\x7f" "ELF", 4)) continue;
                Sha h(BCRYPT_SHA1_ALGORITHM, 20); h.add(d.data(), d.size());
                if (!h.hex().compare(0, 16, EBOOT_SHA)) { r.eboot = true; break; }
            }
        }
    }
    fclose(f);
    return r;
}

// ---- download with progress and SHA-256
static std::atomic<uint64_t> dl_done(0), dl_total(0);
static std::atomic<bool> cancel_flag(false);
static bool download(const std::wstring& url, const fs::path& dst, const char* sha, std::string* err) {
    URL_COMPONENTS uc; memset(&uc, 0, sizeof(uc)); uc.dwStructSize = sizeof(uc);
    wchar_t host[256], path[2048]; uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) { *err = "url"; return false; }
    HINTERNET s = WinHttpOpen(L"TTTRecomp-Setup", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!s) s = WinHttpOpen(L"TTTRecomp-Setup", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, uc.nPort, 0) : nullptr;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
    bool ok = r && WinHttpSendRequest(r, nullptr, 0, nullptr, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr);
    DWORD st = 0, sl = sizeof(st);
    if (ok) WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &st, &sl, nullptr);
    if (!ok || st != 200) { *err = ok ? "http " + std::to_string(st) : "net"; if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); if (s) WinHttpCloseHandle(s); return false; }
    wchar_t cl[32]; DWORD cls = sizeof(cl);
    dl_total = WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, cl, &cls, nullptr) ? _wtoi64(cl) : 0;
    dl_done = 0;
    FILE* f = _wfopen((dst.wstring() + L".part").c_str(), L"wb");
    Sha h(BCRYPT_SHA256_ALGORITHM, 32);
    std::vector<char> b(1 << 20);
    DWORD got = 0;
    while (f && !cancel_flag && WinHttpReadData(r, b.data(), (DWORD)b.size(), &got) && got) { fwrite(b.data(), 1, got, f); h.add(b.data(), got); dl_done += got; }
    if (f) fclose(f);
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    if (cancel_flag) { *err = "cancel"; return false; }
    if (!f || (dl_total && dl_done != dl_total)) { *err = "net"; return false; }
    if (sha && *sha && h.hex() != sha) { *err = "sha"; return false; }
    fs::remove(dst); fs::rename(dst.wstring() + L".part", dst);
    return true;
}

// ---- child processes in a job (cancel kills the whole tree)
static HANDLE job = nullptr;
static int run_proc(const std::wstring& cmd, const fs::path& cwd, const std::wstring& env_path, std::function<void(const std::string&)> line) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE rd, wr;
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi;
    std::wstring env;
    {
        wchar_t* e = GetEnvironmentStringsW();
        for (wchar_t* p = e; *p; p += wcslen(p) + 1) { std::wstring v = p; if (_wcsnicmp(v.c_str(), L"PATH=", 5) && _wcsnicmp(v.c_str(), L"PYTHON", 6)) env += v + L'\0'; }
        FreeEnvironmentStringsW(e);
        wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
        env += L"PATH=" + env_path + L";" + sys + L";" + fs::path(sys).parent_path().wstring() + L'\0';
        env += std::wstring(L"PYTHONUTF8=1") + L'\0' + L"PYTHONIOENCODING=utf-8" + L'\0' + L"PYTHONUNBUFFERED=1" + L'\0' + L'\0';
    }
    std::vector<wchar_t> c(cmd.begin(), cmd.end()); c.push_back(0);
    if (!CreateProcessW(nullptr, c.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, (void*)env.c_str(), cwd.wstring().c_str(), &si, &pi)) {
        CloseHandle(rd); CloseHandle(wr); line("! no se pudo iniciar: " + u8(cmd)); return -1;
    }
    AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(wr);
    std::string acc; char buf[4096]; DWORD n;
    while (ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n) {
        acc.append(buf, n);
        size_t p;
        while ((p = acc.find('\n')) != std::string::npos) { std::string l = acc.substr(0, p); if (!l.empty() && l.back() == '\r') l.pop_back(); line(l); acc.erase(0, p + 1); }
    }
    if (!acc.empty()) line(acc);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd);
    return (int)code;
}

// ---- port files embedded at the end of the exe: zip + uint64 size + "TTTPORT1"
static bool unpack_payload() {
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    FILE* f = _wfopen(self, L"rb");
    if (!f) return false;
    uint64_t size = 0; char magic[8] = {};
    _fseeki64(f, -16, SEEK_END);
    int64_t end = _ftelli64(f);
    if (fread(&size, 8, 1, f) != 1 || fread(magic, 8, 1, f) != 1 || memcmp(magic, "TTTPORT1", 8) || !size || (int64_t)size > end) { fclose(f); return false; }
    std::vector<uint8_t> z((size_t)size);
    _fseeki64(f, end - (int64_t)size, SEEK_SET);
    bool rd = fread(z.data(), 1, z.size(), f) == z.size();
    fclose(f);
    if (!rd) return false;
    auto u16 = [&](size_t p) { return (uint32_t)z[p] | z[p + 1] << 8; };
    auto u32 = [&](size_t p) { return u16(p) | u16(p + 2) << 16; };
    size_t eo = z.size() >= 22 ? z.size() - 22 : 0;
    while (eo && u32(eo) != 0x06054b50) eo--;
    if (u32(eo) != 0x06054b50) return false;
    wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
    fs::path dir = fs::path(tmp) / (std::string("TTTRecomp-Setup-") + VERSION);
    std::error_code ec;
    fs::remove_all(dir, ec);
    size_t cd = u32(eo + 16);
    for (uint32_t k = 0, n = u16(eo + 10); k < n; k++) {
        if (cd + 46 > z.size() || u32(cd) != 0x02014b50) return false;
        uint32_t method = u16(cd + 10), csz = u32(cd + 20), usz = u32(cd + 24), nl = u16(cd + 28), lo = u32(cd + 42);
        std::string name((const char*)&z[cd + 46], nl);
        cd += 46 + nl + u16(cd + 30) + u16(cd + 32);
        if (name.empty() || name.back() == '/' || name.find("..") != std::string::npos) continue;
        size_t data = lo + 30 + u16(lo + 26) + u16(lo + 28);
        if (data + csz > z.size()) return false;
        std::vector<uint8_t> out;
        if (method == 0) out.assign(z.begin() + data, z.begin() + data + csz);
        else if (method == 8) { out.reserve(usz); Inflater inf(&z[data], csz, out); if (!inf.run(false)) return false; }
        else return false;
        fs::path fp = dir / fs::u8path(name);
        fs::create_directories(fp.parent_path(), ec);
        FILE* o = _wfopen(fp.wstring().c_str(), L"wb");
        if (!o) return false;
        fwrite(out.data(), 1, out.size(), o);
        fclose(o);
    }
    if (!fs::exists(dir / "tools" / "build_from_iso.py")) return false;
    payload_dir = dir;
    return true;
}

// ---- build state shared with the worker thread
enum { S_TOOLS, S_ISO, S_GAME, S_PATCH, S_TRANS, S_COMPILE, S_ASSETS, S_N };
static const char* STEP_N[S_N][2] = {{"Prepare the tools", "Preparar las herramientas"}, {"Read the ISO", "Leer la ISO"}, {"Take out the game files", "Sacar los archivos del juego"},
                                     {"Apply the port patches", "Aplicar los parches del port"}, {"Translate the game to C++", "Traducir el juego a C++"},
                                     {"Compile", "Compilar"}, {"Extract the assets", "Extraer los assets"}};
static const float STEP_W[S_N] = {8, 1, 3, 1, 9, 63, 15};
struct Build {
    std::mutex m;
    int step = -1, a = 0, b = 0, state = 0;   // state 0 running, 1 done, 2 error, 3 cancelled
    double t0[S_N] = {}, t1[S_N] = {};
    std::string sub, err;
    std::vector<std::string> log;
} B;
static double now_s() { return GetTickCount64() / 1000.0; }
static void blog(const std::string& l) {
    std::lock_guard<std::mutex> g(B.m);
    B.log.push_back(l);
    if (B.log.size() > 600) B.log.erase(B.log.begin(), B.log.begin() + 100);
}
static void bstep(int k) {
    std::lock_guard<std::mutex> g(B.m);
    double t = now_s();
    if (B.step >= 0 && B.step < S_N && !B.t1[B.step]) B.t1[B.step] = t;
    B.step = k; B.a = B.b = 0; B.sub.clear();
    if (k < S_N) B.t0[k] = t;
}

// ---- updates: a newer release replaces this setup (and then the game); setup.cfg in the install folder keeps the options
static bool upd_mode = false;
static std::atomic<int> upd_state(0);   // 0 checking, 1 none, 2 available, 3 downloading, 4 failed
static UpdRelease upd_rel;
static fs::path self_path() { wchar_t b[MAX_PATH]; GetModuleFileNameW(nullptr, b, MAX_PATH); return b; }
static void upd_check() { std::thread([] { UpdRelease r; const char* tv = getenv("TTT_SETUP_VERSION"); bool ok = upd_latest(tv && *tv ? tv : VERSION, r, 8000); if (ok) upd_rel = r; upd_state = ok ? 2 : 1; }).detach(); }
static void upd_apply() {   // download the new setup, swap it with this one and start it with the same mode
    upd_state = 3;
    std::thread([] {
        fs::path self = self_path(), nw = self.wstring() + L".new", old = self.wstring() + L".old";
        std::string err;
        if (!download(wd(upd_rel.url), nw, upd_rel.sha.c_str(), &err)) { upd_state = 4; return; }
        DeleteFileW(old.wstring().c_str());
        if (!MoveFileExW(self.wstring().c_str(), old.wstring().c_str(), MOVEFILE_REPLACE_EXISTING) || !MoveFileExW(nw.wstring().c_str(), self.wstring().c_str(), MOVEFILE_REPLACE_EXISTING)) { upd_state = 4; return; }
        std::wstring cmd = L"\"" + self.wstring() + L"\"" + (upd_mode ? L" --update" : L"");
        STARTUPINFOW si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si); PROCESS_INFORMATION pi;
        if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, self.parent_path().wstring().c_str(), &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); ExitProcess(0); }
        upd_state = 4;
    }).detach();
}

struct Options { std::wstring iso; fs::path root; bool all_colors = true, unlock = true, shortcut = true; int lang = 3; } O;

static bool copy_port(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    if (fs::equivalent(from, to, ec)) return true;
    for (const char* d : {"recompiler", "patchers", "games", "runtime", "tools"}) {
        fs::copy(from / d, to / d, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (ec) { blog("! copia de " + std::string(d) + ": " + ec.message()); return false; }
    }
    for (const char* f : {"LICENSE", "README.md"}) if (fs::exists(from / f)) fs::copy_file(from / f, to / f, fs::copy_options::overwrite_existing, ec);
    return true;
}

static void shortcut(const fs::path& root) {
    wchar_t desk[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desk))) return;
    CoInitialize(nullptr);
    IShellLinkW* sl = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl))) {
        sl->SetPath((root / "launcher.exe").wstring().c_str());
        sl->SetWorkingDirectory(root.wstring().c_str());
        sl->SetDescription(L"Dragon Ball Z: Tenkaichi Tag Team (TTTRecomp)");
        IPersistFile* pf = nullptr;
        if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, (void**)&pf))) { pf->Save((fs::path(desk) / L"DBZ Tenkaichi Tag Team.lnk").wstring().c_str(), TRUE); pf->Release(); }
        sl->Release();
    }
}

static void worker() {
    auto fail = [](const std::string& e) { std::lock_guard<std::mutex> g(B.m); if (B.state == 0) { B.state = cancel_flag ? 3 : 2; B.err = e; } };
    fs::path root = O.root, tc = root / "toolchain";
    std::error_code ec;
    fs::create_directories(tc, ec);
    bstep(S_TOOLS);
    blog("instalando en " + u8(root.wstring()));
    if (!copy_port(port_dir(), root)) return fail(T("Could not copy the port files.", "No se pudieron copiar los archivos del port."));
    struct Pkg { const wchar_t* url; const char* sha; const char* name; fs::path test, dir; };
    Pkg pk[2] = {{TC_URL, TC_SHA, "compilador", tc / "mingw64" / "bin" / "g++.exe", tc}, {PY_URL, PY_SHA, "python", tc / "python" / "python.exe", tc / "python"}};
    for (int i = 0; i < 2; i++) {
        if (fs::exists(pk[i].test)) { blog(std::string(pk[i].name) + ": ya instalado"); continue; }
        { std::lock_guard<std::mutex> g(B.m); B.sub = i ? T("Downloading Python", "Descargando Python") : T("Downloading the compiler", "Descargando el compilador"); }
        fs::path z = tc / (std::string(pk[i].name) + ".zip");
        std::string e;
        blog(std::string(pk[i].name) + ": descargando " + u8(pk[i].url));
        bool ok = false;
        for (int k = 0; k < 3 && !ok && !cancel_flag; k++) { ok = download(pk[i].url, z, pk[i].sha, &e); if (!ok) blog(std::string(pk[i].name) + ": intento " + std::to_string(k + 1) + " fallo (" + e + ")"); }
        if (!ok) return fail(e == "sha" ? T("The download does not match the expected file. Try again later.", "La descarga no coincide con el archivo esperado. Inténtalo más tarde.")
                                        : T("Could not download the tools. Check your internet connection.", "No se pudieron descargar las herramientas. Revisa tu conexión a internet."));
        { std::lock_guard<std::mutex> g(B.m); B.sub = T("Unpacking", "Descomprimiendo"); B.a = B.b = 0; }
        fs::create_directories(pk[i].dir, ec);
        wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
        int r = run_proc(L"\"" + std::wstring(sys) + L"\\tar.exe\" -xf \"" + z.wstring() + L"\" -C \"" + pk[i].dir.wstring() + L"\"", root, sys, blog);
        fs::remove(z, ec);
        if (r || !fs::exists(pk[i].test)) return fail(T("Could not unpack the tools.", "No se pudieron descomprimir las herramientas."));
    }
    for (auto& e : fs::directory_iterator(tc / "python", ec)) if (e.path().extension() == "._pth") fs::remove(e.path(), ec);
    if (cancel_flag) return fail("");
    std::wstring path = (tc / "mingw64" / "bin").wstring() + L";" + (tc / "python").wstring();
    unsigned nc = std::thread::hardware_concurrency();
    std::wstring cmd = L"\"" + (tc / "python" / "python.exe").wstring() + L"\" -u \"" + (root / "tools" / "build_from_iso.py").wstring() + L"\" --iso \"" + O.iso + L"\" --root \"" +
                       root.wstring() + L"\" --colors " + (O.all_colors ? L"all" : L"original") + L" --jobs " + std::to_wstring(nc > 2 ? nc - 1 : 1) + L" --resume" + (O.unlock ? L"" : L" --no-unlock");
    int code = run_proc(cmd, root, path, [](const std::string& l) {
        blog(l);
        int k, n, a, b;
        if (sscanf(l.c_str(), "@paso %d %d", &k, &n) == 2) bstep(k);
        else if (sscanf(l.c_str(), "@avance %d %d", &a, &b) == 2) { std::lock_guard<std::mutex> g(B.m); B.a = a; B.b = b; }
        else if (!l.compare(0, 7, "@error ")) { std::lock_guard<std::mutex> g(B.m); B.err = l.substr(7); size_t sp = B.err.find(' '); if (sp != std::string::npos) B.err = B.err.substr(sp + 1); }
    });
    if (code || cancel_flag) return fail(B.err.empty() ? T("The build stopped. Open the details to see why.", "La construcción se detuvo. Abre los detalles para ver por qué.") : B.err);
    fs::create_directories(root / "generated" / "src", ec);
    if (FILE* f = _wfopen((root / "generated" / "src" / "settings.cfg").wstring().c_str(), L"r")) fclose(f);
    else if (FILE* g = _wfopen((root / "generated" / "src" / "settings.cfg").wstring().c_str(), L"w")) { fprintf(g, "language %d\nfit 2\n", O.lang); fclose(g); }
    if (O.shortcut && !upd_mode) shortcut(root);
    {   // the install folder keeps this setup and its options: the launcher runs it to update
        fs::path self = self_path(), dst = root / "TTTRecomp-Setup.exe";
        if (!fs::equivalent(self, dst, ec)) fs::copy_file(self, dst, fs::copy_options::overwrite_existing, ec);
        if (FILE* f = _wfopen((root / "setup.cfg").wstring().c_str(), L"w")) {
            fprintf(f, "version=%s\ncolors=%s\nunlock=%d\niso=%s\n", VERSION, O.all_colors ? "all" : "original", O.unlock ? 1 : 0, u8(O.iso).c_str());
            fclose(f);
        }
    }
    bstep(S_N);
    std::lock_guard<std::mutex> g(B.m);
    B.state = 1;
}

// ---- drawing (OpenGL 1.1, logical 960 x 600)
struct Glyph { float x, y, w, h, xo, yo, xa; };
struct Font { std::map<uint32_t, Glyph> g; std::map<uint64_t, float> kern; float size = 66, base = 64; GLuint fill = 0, outl = 0; float tw = 1024, th = 1024; } F[2];
static GLuint load_png(const fs::path& p, int* w = nullptr, int* h = nullptr) {
    FILE* f = _wfopen(p.wstring().c_str(), L"rb");
    if (!f) return 0;
    std::vector<uint8_t> d; char b[65536]; size_t n;
    while ((n = fread(b, 1, sizeof(b), f)) > 0) d.insert(d.end(), b, b + n);
    fclose(f);
    int W, H; std::vector<uint32_t> px; std::string e;
    if (!png_decode(d, &W, &H, px, &e)) return 0;
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGBA, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    if (w) *w = W;
    if (h) *h = H;
    return t;
}
static void load_font(Font& fo, const fs::path& dir) {
    FILE* f = _wfopen((dir / "font.fnt").wstring().c_str(), L"r");
    if (!f) return;
    char l[512];
    while (fgets(l, sizeof(l), f)) {
        auto v = [&](const char* k) { const char* p = strstr(l, k); return p ? (float)atof(p + strlen(k)) : 0.0f; };
        if (!strncmp(l, "info ", 5)) fo.size = v(" size=");
        else if (!strncmp(l, "common ", 7)) { fo.base = v(" base="); fo.tw = v(" scaleW="); fo.th = v(" scaleH="); }
        else if (!strncmp(l, "char ", 5)) fo.g[(uint32_t)v(" id=")] = {v(" x="), v(" y="), v(" width="), v(" height="), v(" xoffset="), v(" yoffset="), v(" xadvance=")};
        else if (!strncmp(l, "kerning ", 8)) fo.kern[((uint64_t)(uint32_t)v(" first=") << 32) | (uint32_t)v(" second=")] = v(" amount=");
    }
    fclose(f);
    fo.fill = load_png(dir / "fill.png");
    fo.outl = load_png(dir / "outline.png");
}
static std::vector<uint32_t> cps(const std::string& s) {
    std::vector<uint32_t> o;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        uint32_t cp = n == 1 ? c : c & (0x3F >> (n - 1));
        for (int k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | (s[i + k] & 0x3F);
        o.push_back(cp); i += n;
    }
    return o;
}
static float text_w(int st, float px, const std::string& s) {
    Font& fo = F[st];
    float k = px / fo.size, x = 0; uint32_t prev = 0;
    for (uint32_t c : cps(s)) {
        auto it = fo.g.find(c); if (it == fo.g.end()) it = fo.g.find('?'); if (it == fo.g.end()) continue;
        auto kk = fo.kern.find(((uint64_t)prev << 32) | c); if (kk != fo.kern.end()) x += kk->second;
        x += it->second.xa; prev = c;
    }
    return x * k;
}
static void col(uint32_t c, float a) { glColor4f(((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f, a); }
static void text_pass(Font& fo, GLuint tex, float x, float y, float px, const std::vector<uint32_t>& cp) {
    float k = px / fo.size; uint32_t prev = 0;
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    for (uint32_t c : cp) {
        auto it = fo.g.find(c); if (it == fo.g.end()) it = fo.g.find('?'); if (it == fo.g.end()) continue;
        auto kk = fo.kern.find(((uint64_t)prev << 32) | c); if (kk != fo.kern.end()) x += kk->second * k;
        const Glyph& g = it->second;
        float x0 = x + g.xo * k, y0 = y + g.yo * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
        float u0 = g.x / fo.tw, v0 = g.y / fo.th, u1 = (g.x + g.w) / fo.tw, v1 = (g.y + g.h) / fo.th;
        glTexCoord2f(u0, v0); glVertex2f(x0, y0); glTexCoord2f(u1, v0); glVertex2f(x1, y0);
        glTexCoord2f(u1, v1); glVertex2f(x1, y1); glTexCoord2f(u0, v1); glVertex2f(x0, y1);
        x += g.xa * k; prev = c;
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
// align 0 left, 1 center, 2 right; y = top of the line box
static void text(int st, float x, float y, float px, const std::string& s, uint32_t c, float a = 1, int align = 0, bool outline = true) {
    Font& fo = F[st];
    float w = text_w(st, px, s);
    if (align == 1) x -= w / 2; else if (align == 2) x -= w;
    std::vector<uint32_t> cp = cps(s);
    if (outline && fo.outl) { col(0x0B1020, a * 0.9f); text_pass(fo, fo.outl, x, y, px, cp); }
    col(c, a); text_pass(fo, fo.fill, x, y, px, cp);
}
static void rect(float x, float y, float w, float h, uint32_t c, float a) { col(c, a); glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd(); }
static void rrect(float x, float y, float w, float h, float r, uint32_t c, float a) {
    col(c, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + w / 2, y + h / 2);
    const float cx[4] = {x + w - r, x + r, x + r, x + w - r}, cy[4] = {y + r, y + r, y + h - r, y + h - r};
    for (int q = 0; q < 4; q++) for (int i = 0; i <= 6; i++) { float t = (q * 90 + i * 15) * 3.14159f / 180; glVertex2f(cx[q] + cosf(t) * r, cy[q] - sinf(t) * r); }
    glVertex2f(x + w, y + r);
    glEnd();
}
static void rframe(float x, float y, float w, float h, float r, uint32_t c, float a, float t = 1.5f) {
    col(c, a);
    glLineWidth(t);
    glBegin(GL_LINE_LOOP);
    const float cx[4] = {x + w - r, x + r, x + r, x + w - r}, cy[4] = {y + r, y + r, y + h - r, y + h - r};
    for (int q = 0; q < 4; q++) for (int i = 0; i <= 6; i++) { float tt = (q * 90 + i * 15) * 3.14159f / 180; glVertex2f(cx[q] + cosf(tt) * r, cy[q] - sinf(tt) * r); }
    glEnd();
}
static void arc(float cx, float cy, float r, float th, float a0, float a1, uint32_t c, float a) {
    col(c, a);
    glBegin(GL_TRIANGLE_STRIP);
    int n = std::max(2, (int)((a1 - a0) * 40));
    for (int i = 0; i <= n; i++) { float t = a0 + (a1 - a0) * i / n; glVertex2f(cx + cosf(t) * (r - th / 2), cy + sinf(t) * (r - th / 2)); glVertex2f(cx + cosf(t) * (r + th / 2), cy + sinf(t) * (r + th / 2)); }
    glEnd();
}
static void check(float x, float y, float s, uint32_t c, float lw = 2.5f) { col(c, 1); glLineWidth(lw); glBegin(GL_LINE_STRIP); glVertex2f(x, y + s * 0.5f); glVertex2f(x + s * 0.38f, y + s * 0.85f); glVertex2f(x + s, y + s * 0.1f); glEnd(); }
static void cross(float x, float y, float s, uint32_t c) { col(c, 1); glLineWidth(2.5f); glBegin(GL_LINES); glVertex2f(x, y); glVertex2f(x + s, y + s); glVertex2f(x + s, y); glVertex2f(x, y + s); glEnd(); }

// ki aura: soft sprites rising around a ring, additive
static GLuint spark = 0;
static void make_spark() {
    std::vector<uint32_t> p(64 * 64);
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) { float d = hypotf(x - 31.5f, y - 31.5f) / 32; float a = std::max(0.0f, 1 - d); a = a * a; p[y * 64 + x] = 0xFFFFFFu | ((uint32_t)(a * 255) << 24); }
    glGenTextures(1, &spark); glBindTexture(GL_TEXTURE_2D, spark);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, p.data());
}
struct Part { float x, y, vx, vy, life, max, size; uint32_t c; };
static std::vector<Part> parts;
static float frand() { return (float)rand() / RAND_MAX; }
static void sprite(float x, float y, float s, uint32_t c, float a) {
    col(c, a);
    glTexCoord2f(0, 0); glVertex2f(x - s, y - s); glTexCoord2f(1, 0); glVertex2f(x + s, y - s);
    glTexCoord2f(1, 1); glVertex2f(x + s, y + s); glTexCoord2f(0, 1); glVertex2f(x - s, y + s);
}
static void aura(float cx, float cy, float R, float p, float t, float power, bool ring) {
    int spawn = (int)(2 + power * 9);
    for (int i = 0; i < spawn; i++) {
        float a = frand() * 6.2832f, r = R * (0.85f + frand() * 0.25f);
        Part q; q.x = cx + cosf(a) * r; q.y = cy + sinf(a) * r * 0.92f; q.vx = cosf(a) * (8 + frand() * 18); q.vy = -30 - frand() * 70 * (0.5f + power);
        q.max = q.life = 0.6f + frand() * 0.9f; q.size = 6 + frand() * 12 * (0.6f + power); q.c = frand() < 0.25f ? 0xFFF3A0 : frand() < 0.5f ? 0xFFB13A : 0xFF7A1A;
        parts.push_back(q);
    }
    glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, spark);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBegin(GL_QUADS);
    float glow = 0.18f + 0.1f * sinf(t * 3) + power * 0.25f;
    sprite(cx, cy, R * 1.9f, 0xFF8A20, glow * 0.55f);
    sprite(cx, cy, R * 1.15f, 0xFFD060, glow * 0.4f);
    for (auto& q : parts) { float k = q.life / q.max; sprite(q.x, q.y, q.size * (0.5f + k), q.c, k * 0.75f); }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (ring) {
        arc(cx, cy, R, 12, 0, 6.2832f, 0x232A3A, 1);
        if (p > 0.001f) arc(cx, cy, R, 12, -1.5708f, -1.5708f + 6.2832f * std::min(p, 1.0f), 0xF2A33A, 1);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        arc(cx, cy, R + 11 + 2 * sinf(t * 4), 3, 0, 6.2832f, 0xFFF379, 0.25f + 0.2f * sinf(t * 2.5f));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}
static void tick_parts(float dt) {
    for (auto& q : parts) { q.x += q.vx * dt; q.y += q.vy * dt; q.vx *= 0.97f; q.life -= dt; }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Part& q) { return q.life <= 0; }), parts.end());
    if (parts.size() > 1500) parts.erase(parts.begin(), parts.begin() + (parts.size() - 1500));
}

// ---- immediate-mode UI
static float mx = -1, my = -1;
static bool mdown = false, mclick = false, kenter = false, kesc = false;
static bool hover(float x, float y, float w, float h) { return mx >= x && mx < x + w && my >= y && my < y + h; }
static bool button(float x, float y, float w, float h, const std::string& label, bool primary, bool enabled = true) {
    bool hv = enabled && hover(x, y, w, h);
    if (primary) rrect(x, y, w, h, 8, enabled ? (hv ? 0xFFB54E : 0xF2A33A) : 0x5A4A30, 1);
    else { rrect(x, y, w, h, 8, hv ? 0x2A3245 : 0x1C2230, 1); rframe(x, y, w, h, 8, 0x3D4658, 1); }
    text(0, x + w / 2, y + (h - 18) / 2 - 1, 18, label, primary ? 0x3A2200 : 0xE8EDF5, enabled ? 1 : 0.5f, 1, !primary);
    return hv && mclick;
}
static bool checkbox(float x, float y, const std::string& label, bool* v) {
    bool hv = hover(x, y, 340, 24);
    rrect(x, y + 2, 20, 20, 5, *v ? 0xF2A33A : 0x1C2230, 1);
    rframe(x, y + 2, 20, 20, 5, hv ? 0xFFB54E : 0x3D4658, 1);
    if (*v) check(x + 4, y + 7, 12, 0x3A2200);
    text(0, x + 30, y + 1, 17, label, 0xE8EDF5);
    if (hv && mclick) { *v = !*v; return true; }
    return false;
}

// ---- pages
static int page = 0;
static IsoInfo iso;
static bool details = false;
static double build_t0 = 0;
static std::thread th;
static HWND win;

static std::wstring pick_iso() {
    wchar_t fn[4096] = L"";
    OPENFILENAMEW of; memset(&of, 0, sizeof(of));
    of.lStructSize = sizeof(of); of.hwndOwner = win;
    of.lpstrFilter = L"ISO (*.iso)\0*.iso\0";
    of.lpstrFile = fn; of.nMaxFile = 4096;
    of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&of) ? fn : L"";
}
static void pick_folder() {   // the standard Windows folder picker
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void**)&d))) return;
    DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    std::wstring title = wd(T("Install folder", "Carpeta de instalación"));
    d->SetTitle(title.c_str());
    IShellItem* start = nullptr;
    fs::path cur = O.root.parent_path();
    while (!cur.empty() && !fs::exists(cur)) cur = cur.parent_path();
    if (!cur.empty() && SUCCEEDED(SHCreateItemFromParsingName(cur.wstring().c_str(), nullptr, IID_IShellItem, (void**)&start))) { d->SetFolder(start); start->Release(); }
    if (SUCCEEDED(d->Show(win))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(d->GetResult(&it))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                fs::path f = p;
                O.root = f.filename() == "TTTRecomp" ? f : f / "TTTRecomp";
                CoTaskMemFree(p);
            }
            it->Release();
        }
    }
    d->Release();
}
static void set_iso(const std::wstring& p) { O.iso = p; iso = iso_check(p); }
static uint64_t free_space(const fs::path& p) {
    fs::path q = p;
    while (!q.empty() && !fs::exists(q)) q = q.parent_path();
    ULARGE_INTEGER fb; return GetDiskFreeSpaceExW(q.wstring().c_str(), &fb, nullptr, nullptr) ? fb.QuadPart : 0;
}
static const uint64_t NEED = 8ull << 30;

static bool build_started = false;
static void start_build() {
    build_started = true;
    { std::lock_guard<std::mutex> g(B.m); B.step = -1; B.state = 0; B.err.clear(); B.log.clear(); for (int i = 0; i < S_N; i++) B.t0[i] = B.t1[i] = 0; }
    cancel_flag = false;
    O.lang = es ? 3 : 1;
    build_t0 = now_s();
    if (th.joinable()) th.join();
    std::error_code ec; fs::create_directories(O.root, ec);
    th = std::thread(worker);
    page = 3;
}
static void save_log() {
    std::lock_guard<std::mutex> g(B.m);
    if (FILE* f = _wfopen((O.root / "setup_log.txt").wstring().c_str(), L"w")) { for (auto& l : B.log) fprintf(f, "%s\n", l.c_str()); fclose(f); }
}

static void draw_header(float t) {
    rect(0, 0, VW, 64, 0x10141E, 1);
    rect(0, 64, VW, 1, 0x2A3140, 1);
    float bolt = 0.75f + 0.25f * sinf(t * 3);
    col(0xF2A33A, bolt); glBegin(GL_TRIANGLES); glVertex2f(30, 18); glVertex2f(22, 36); glVertex2f(32, 34); glVertex2f(30, 46); glVertex2f(40, 28); glVertex2f(30, 30); glEnd();
    text(1, 50, 18, 26, "TTTRecomp", 0xFFFFFF);
    text(0, 50 + text_w(1, 26, "TTTRecomp") + 10, 17, 14, std::string("v") + VERSION, 0x7F93B4, 1, 0, false);
    text(0, 50 + text_w(1, 26, "TTTRecomp") + 10, 33, 13, "By Los ijue30s", 0xF2A33A, 0.85f, 0, false);
    static const char* P[5][2] = {{"Welcome", "Bienvenida"}, {"ISO", "ISO"}, {"Options", "Opciones"}, {"Build", "Construcción"}, {"Done", "Listo"}};
    float x = VW - 20;
    for (int i = 4; i >= 0; i--) {
        std::string l = T(P[i][0], P[i][1]);
        float w = text_w(0, 14, l) + 26;
        x -= w;
        bool on = i == page, done = i < page;
        rrect(x, 20, w, 26, 13, on ? 0xF2A33A : 0x1C2230, 1);
        if (!on) rframe(x, 20, w, 26, 13, 0x2A3140, 1);
        text(0, x + w / 2, 24, 14, l, on ? 0x3A2200 : done ? 0x4CD486 : 0x7F93B4, 1, 1, false);
        x -= 6;
    }
}
static void footer_line() { rect(0, VH - 70, VW, 1, 0x2A3140, 1); }
// new release notice in the footer of the first pages
static void upd_banner() {
    int st = upd_state;
    if (st < 2) return;
    float cx = VW / 2, y = VH - 52;
    if (st == 2) {
        std::string m = std::string(T("New version available: ", "Versión nueva disponible: ")) + upd_rel.tag;
        float w = text_w(0, 15, m), bw = text_w(0, 16, T("Update", "Actualizar")) + 30, x0 = cx - (w + 12 + bw) / 2;
        text(0, x0, y + 9, 15, m, 0xFFB54E, 1, 0, false);
        if (button(x0 + w + 12, y + 2, bw, 32, T("Update", "Actualizar"), true)) upd_apply();
    } else if (st == 3) {
        char pc[64]; snprintf(pc, sizeof(pc), "%s %d%%", T("Downloading the new version...", "Descargando la versión nueva..."), dl_total ? (int)(dl_done * 100 / dl_total) : 0);
        text(0, cx, y + 9, 15, pc, 0xFFB54E, 1, 1, false);
    } else text(0, cx, y + 9, 15, T("The update could not be downloaded.", "No se pudo descargar la actualización."), 0xFF8A70, 1, 1, false);
}

static void page_welcome(float t) {
    aura(720, 300, 120, 0, t, 0.45f, false);
    text(1, 60, 140, 40, "Dragon Ball Z:", 0xFFFFFF);
    text(1, 60, 186, 40, "Tenkaichi Tag Team", 0xFFFFFF);
    text(1, 60, 236, 30, T("on your PC", "en tu PC"), 0xF2A33A);
    const char* body[3][2] = {{"This assistant turns your copy of the game into a native", "Este asistente convierte tu copia del juego en un port"},
                              {"Windows port. It includes no game files: you need your", "nativo para Windows. No incluye archivos del juego:"},
                              {"own PSP ISO (ULUS10537).", "necesitas tu propia ISO de PSP (ULUS10537)."}};
    for (int i = 0; i < 3; i++) text(0, 60, 296 + i * 26, 18, T(body[i][0], body[i][1]), 0xB8C6DC, 1, 0, false);
    footer_line();
    std::string lg = es ? "Idioma: Español" : "Language: English";
    if (button(30, VH - 52, text_w(0, 18, lg) + 30, 36, lg, false)) es = !es;
    if (button(VW - 190, VH - 52, 160, 36, T("Start", "Empezar"), true) || kenter) page = 1;
}
static void page_iso(float t) {
    text(1, 60, 92, 28, T("Choose your ISO", "Elige tu ISO"), 0xFFFFFF);
    float x = 60, y = 140, w = VW - 120, h = 120;
    bool hv = hover(x, y, w, h);
    rrect(x, y, w, h, 12, hv ? 0x1E2636 : 0x181E2A, 1);
    col(hv ? 0xF2A33A : 0x3D4658, 1);
    glLineWidth(2); glEnable(GL_LINE_STIPPLE); glLineStipple(3, 0xAAAA); rframe(x, y, w, h, 12, hv ? 0xF2A33A : 0x3D4658, 1, 2); glDisable(GL_LINE_STIPPLE);
    arc(x + w / 2, y + 38, 14, 4, 0, 6.2832f, 0xF2A33A, 1); arc(x + w / 2, y + 38, 3, 3, 0, 6.2832f, 0xF2A33A, 1);
    text(0, x + w / 2, y + 66, 18, T("Drop the ISO here or click to browse", "Arrastra la ISO aquí o haz clic para buscarla"), 0xE8EDF5, 1, 1, false);
    if (hv && mclick) { std::wstring p = pick_iso(); if (!p.empty()) set_iso(p); }
    bool ok = !O.iso.empty() && iso.game && iso.eboot;
    if (!O.iso.empty()) {
        float yy = 290;
        rrect(60, yy - 10, VW - 120, 150, 12, 0x181E2A, 1);
        auto row = [&](bool good, const std::string& s) { if (good) check(80, yy + 3, 14, 0x4CD486); else cross(80, yy + 3, 13, 0xFF6A5A); text(0, 106, yy, 17, s, good ? 0xE8EDF5 : 0xFF8A70, 1, 0, false); yy += 32; };
        std::string fn = u8(fs::path(O.iso).filename().wstring());
        row(iso.read && iso.psp, fn + " · " + mb(iso.size) + (iso.psp ? "" : T(" · not a PSP ISO", " · no es una ISO de PSP")));
        if (iso.psp) row(iso.game, iso.game ? "ULUS10537 · Dragon Ball Z: Tenkaichi Tag Team" : std::string(T("This ISO is ", "Esta ISO es ")) + (iso.id.empty() ? "?" : iso.id) + T(", the port needs ULUS10537", ", el port necesita ULUS10537"));
        if (iso.game) row(iso.eboot, iso.eboot ? T("Original EBOOT recognized (version 1.00)", "EBOOT original reconocido (versión 1.00)") : T("The EBOOT is not the original 1.00 one", "El EBOOT no es el original 1.00"));
        if (ok) { std::string pill = T("Compatible", "Compatible"); float pw = text_w(0, 14, pill) + 22; rrect(VW - 80 - pw, 296, pw, 24, 12, 0x1E4A30, 1); text(0, VW - 80 - pw / 2, 300, 14, pill, 0x4CD486, 1, 1, false); }
    }
    footer_line();
    if (button(30, VH - 52, 120, 36, T("Back", "Atrás"), false) || kesc) page = 0;
    if (button(VW - 190, VH - 52, 160, 36, T("Continue", "Continuar"), true, ok) || (kenter && ok)) page = 2;
}
static void page_options(float t) {
    text(1, 60, 92, 28, T("Options", "Opciones"), 0xFFFFFF);
    text(0, 60, 138, 16, T("Character colours", "Colores de los personajes"), 0x9DB4D6, 1, 0, false);
    for (int k = 0; k < 2; k++) {
        float x = 60 + k * 425, y = 162, w = 415, h = 78;
        bool on = O.all_colors == (k == 0), hv = hover(x, y, w, h);
        rrect(x, y, w, h, 10, on ? 0x2A2418 : 0x181E2A, 1);
        rframe(x, y, w, h, 10, on ? 0xF2A33A : hv ? 0x5A6478 : 0x3D4658, 1, on ? 2 : 1.5f);
        text(0, x + 16, y + 14, 19, k ? T("Only the originals", "Solo los originales") : T("All colours", "Todos los colores"), 0xFFFFFF);
        text(0, x + 16, y + 44, 15, k ? T("The original colours of the game", "Los colores originales del juego") : T("The 4 colours per character on the disc", "Los 4 colores por personaje que trae el disco"), 0x9DB4D6, 1, 0, false);
        if (hv && mclick) O.all_colors = k == 0;
    }
    checkbox(60, 262, T("Everything unlocked from the start", "Todo desbloqueado desde el inicio"), &O.unlock);
    checkbox(60, 296, T("Shortcut on the desktop", "Acceso directo en el escritorio"), &O.shortcut);
    rrect(60, 344, VW - 120, 92, 10, 0x181E2A, 1);
    text(0, 78, 356, 15, T("Install folder", "Carpeta de instalación"), 0x9DB4D6, 1, 0, false);
    std::string root = u8(O.root.wstring());
    while (text_w(0, 17, root) > VW - 300 && root.size() > 8) root.erase(0, 1);
    text(0, 78, 380, 17, root, 0xE8EDF5, 1, 0, false);
    uint64_t fr = free_space(O.root);
    bool space = fr >= NEED;
    text(0, 78, 408, 14, std::string(T("Free: ", "Libre: ")) + gb(fr) + T(" (about 8 GB needed)", " (se necesitan unos 8 GB)"), space ? 0x7F93B4 : 0xFF8A70, 1, 0, false);
    if (button(VW - 200, 366, 120, 34, T("Change", "Cambiar"), false)) pick_folder();
    bool again = fs::exists(O.root / "game" / "EBOOT.OLD");
    if (again) text(0, 60, 452, 15, T("A build already exists in this folder: it is made again (mods and saves are kept).", "Ya hay una construcción en esta carpeta: se rehace (los mods y las partidas se conservan)."), 0xFFB54E, 1, 0, false);
    footer_line();
    if (button(30, VH - 52, 120, 36, T("Back", "Atrás"), false) || kesc) page = 1;
    if (button(VW - 190, VH - 52, 160, 36, T("Build", "Construir"), true, space) || (kenter && space)) start_build();
}
static void page_build(float t) {
    int step, a, b, state; std::string sub, err; double t0[S_N], t1[S_N]; std::vector<std::string> tail;
    {
        std::lock_guard<std::mutex> g(B.m);
        step = B.step; a = B.a; b = B.b; state = B.state; sub = B.sub; err = B.err;
        memcpy(t0, B.t0, sizeof(t0)); memcpy(t1, B.t1, sizeof(t1));
        size_t n = B.log.size(); for (size_t i = n > 17 ? n - 17 : 0; i < n; i++) tail.push_back(B.log[i]);
    }
    if (state == 1) { page = 4; save_log(); return; }
    float frac = 0, tot = 0;
    for (int i = 0; i < S_N; i++) tot += STEP_W[i];
    for (int i = 0; i < S_N && i < step; i++) frac += STEP_W[i];
    float in = 0;
    if (step == S_TOOLS && dl_total) in = (float)dl_done / dl_total;
    else if (b) in = (float)a / b;
    if (step >= 0 && step < S_N) frac += STEP_W[step] * std::min(1.0f, in);
    float p = frac / tot;
    float cx = 190, cy = 300;
    aura(cx, cy, 108, p, t, state ? 0.1f : 0.35f + p * 0.65f, true);
    char pc[16]; snprintf(pc, sizeof(pc), "%d%%", (int)(p * 100));
    text(1, cx, cy - 28, 44, pc, 0xFFFFFF, 1, 1);
    double el = now_s() - build_t0;
    std::string rem;
    if (p > 0.12f && state == 0) { double r = el * (1 - p) / p; rem = std::string("~") + std::to_string((int)(r / 60) + 1) + T(" min left", " min restantes"); }
    text(0, cx, cy + 24, 14, state == 2 ? T("Stopped", "Detenido") : state == 3 ? T("Cancelled", "Cancelado") : rem.empty() ? clock_s(el) : rem, 0xE8EDF5, 1, 1, true);
    float x = 360, y = 92;
    text(1, x, y, 26, state == 2 ? std::string(T("Something went wrong", "Algo salió mal")) : (upd_mode ? std::string(T("Updating to v", "Actualizando a la v")) + VERSION : std::string(T("Building your game", "Construyendo tu juego"))), state == 2 ? 0xFF8A70 : 0xFFFFFF);
    if (!build_started) {
        int us = upd_state;
        char m[96];
        if (us == 3) snprintf(m, sizeof(m), "%s %d%%", T("Downloading the new version...", "Descargando la versión nueva..."), dl_total ? (int)(dl_done * 100 / dl_total) : 0);
        else snprintf(m, sizeof(m), "%s", T("Looking for the new version...", "Buscando la versión nueva..."));
        text(0, x, y + 36, 15, m, 0xFFB54E, 1, 0, false);
    } else text(0, x, y + 36, 15, upd_mode ? T("Only what changed is rebuilt; your mods and saves are kept.", "Solo se rehace lo que cambió; tus mods y partidas se conservan.") : T("This takes a while; your PC stays usable.", "Esto tarda un rato; tu PC sigue usable."), 0x7F93B4, 1, 0, false);
    if (details) {
        rrect(x, 150, VW - x - 30, 370, 10, 0x0B0F18, 1);
        for (size_t i = 0; i < tail.size(); i++) {
            std::string l = tail[i];
            while (text_w(0, 13, l) > VW - x - 60 && l.size() > 4) l.pop_back();
            text(0, x + 12, 160 + i * 21, 13, l, l[0] == '@' ? 0xF2A33A : l[0] == '!' ? 0xFF8A70 : 0x9DB4D6, 1, 0, false);
        }
    } else {
        for (int i = 0; i < S_N; i++) {
            float ry = 160 + i * 48;
            bool done = i < step || state == 1, cur = i == step && state == 0, bad = i == step && state >= 2;
            if (i < S_N - 1) rect(x + 11, ry + 26, 2, 22, done ? 0x2E6A48 : 0x2A3140, 1);
            arc(x + 12, ry + 12, 11, 2.5f, 0, 6.2832f, done ? 0x4CD486 : cur ? 0xF2A33A : bad ? 0xFF6A5A : 0x3D4658, 1);
            if (done) check(x + 6, ry + 7, 12, 0x4CD486);
            else if (cur) arc(x + 12, ry + 12, 6, 3, t * 5, t * 5 + 3.5f, 0xF2A33A, 1);
            else if (bad) cross(x + 7, ry + 7, 10, 0xFF6A5A);
            text(0, x + 36, ry + 1, 18, T(STEP_N[i][0], STEP_N[i][1]), cur ? 0xFFF379 : done ? 0xE8EDF5 : bad ? 0xFF8A70 : 0x6A7890, 1, 0, cur);
            std::string info;
            if (cur && i == S_TOOLS) info = sub + (dl_total && dl_done < dl_total ? " · " + mb(dl_done) + " / " + mb(dl_total) : "");
            else if (cur && i == S_COMPILE && b) info = std::to_string(a) + T(" of ", " de ") + std::to_string(b) + T(" files", " archivos");
            else if (cur && b) info = std::to_string(a) + " / " + std::to_string(b);
            if (!info.empty()) text(0, x + 36, ry + 24, 13, info, 0x9DB4D6, 1, 0, false);
            double d = done && t1[i] ? t1[i] - t0[i] : cur && t0[i] ? now_s() - t0[i] : 0;
            if (d > 0) text(0, VW - 40, ry + 3, 14, clock_s(d), cur ? 0xFFFFFF : 0x7F93B4, 1, 2, false);
        }
        if (state == 2 || state == 3) {
            std::string e = state == 3 ? T("You cancelled the build.", "Cancelaste la construcción.") : err;
            while (text_w(0, 15, e) > VW - x - 40 && e.size() > 4) e.pop_back();
            text(0, x, 500, 15, e, 0xFF8A70, 1, 0, false);
        }
    }
    footer_line();
    if (button(30, VH - 52, 190, 36, details ? T("Hide details", "Ocultar detalles") : T("Show details", "Mostrar detalles"), false)) details = !details;
    if (state == 0) {
        if (button(VW - 190, VH - 52, 160, 36, T("Cancel", "Cancelar"), false)) { cancel_flag = true; TerminateJobObject(job, 1); }
    } else {
        if (button(VW - 190, VH - 52, 160, 36, T("Try again", "Reintentar"), true)) start_build();
        if (button(VW - 360, VH - 52, 150, 36, T("Back", "Atrás"), false)) { save_log(); page = 2; }
    }
}
static void page_done(float t) {
    aura(220, 300, 120, 1, t, 1.0f, true);
    check(185, 280, 70, 0x4CD486, 7);
    text(1, 420, 190, 38, T("Ready to fight!", "¡Listo para pelear!"), 0xFFFFFF);
    double el = 0; { std::lock_guard<std::mutex> g(B.m); el = B.t1[S_ASSETS] ? B.t1[S_ASSETS] - build_t0 : now_s() - build_t0; }
    text(0, 420, 246, 17, std::string(T("Built in ", "Construido en ")) + std::to_string((int)(el / 60) + 1) + T(" min. Your ISO was not changed.", " min. Tu ISO no se modificó."), 0xB8C6DC, 1, 0, false);
    text(0, 420, 274, 17, T("Change the settings inside the game with F1.", "Los ajustes se cambian dentro del juego con F1."), 0xB8C6DC, 1, 0, false);
    if (button(420, 330, 200, 50, T("Play", "Jugar"), true) || kenter) {
        STARTUPINFOW si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si); PROCESS_INFORMATION pi;
        std::wstring c = L"\"" + (O.root / "launcher.exe").wstring() + L"\"";
        if (CreateProcessW(nullptr, &c[0], nullptr, nullptr, FALSE, 0, nullptr, O.root.wstring().c_str(), &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); PostQuitMessage(0); }
    }
    footer_line();
    if (button(30, VH - 52, 180, 36, T("Open folder", "Abrir carpeta"), false)) ShellExecuteW(win, L"open", O.root.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (button(VW - 190, VH - 52, 160, 36, T("Close", "Cerrar"), false) || kesc) PostQuitMessage(0);
}

// ---- window
static float scale = 1, offx = 0, offy = 0;
static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_MOUSEMOVE: mx = ((short)LOWORD(l) - offx) / scale; my = ((short)HIWORD(l) - offy) / scale; return 0;
        case WM_LBUTTONDOWN: mdown = true; SetCapture(h); return 0;
        case WM_LBUTTONUP: if (mdown) mclick = true; mdown = false; ReleaseCapture(); return 0;
        case WM_KEYDOWN: if (w == VK_RETURN) kenter = true; if (w == VK_ESCAPE) kesc = true; return 0;
        case WM_DROPFILES: {
            wchar_t f[4096]; HDROP d = (HDROP)w;
            if (DragQueryFileW(d, 0, f, 4096) && (page == 1 || page == 0)) { set_iso(f); page = 1; }
            DragFinish(d); return 0;
        }
        case WM_CLOSE:
            if (page == 3) { std::lock_guard<std::mutex> g(B.m); if (B.state == 0) { if (MessageBoxW(h, wd(T("Cancel the build and close?", "¿Cancelar la construcción y cerrar?")).c_str(), L"TTTRecomp", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0; cancel_flag = true; TerminateJobObject(job, 1); } }
            DestroyWindow(h); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR, int) {
    SetProcessDPIAware();
    es = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_SPANISH;
    wchar_t prof[MAX_PATH];
    O.root = SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, prof)) ? fs::path(prof) / "Games" / "TTTRecomp" : exe_dir() / "TTTRecomp";
    job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li; memset(&li, 0, sizeof(li)); li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof(li));
    unpack_payload();
    DeleteFileW((self_path().wstring() + L".old").c_str());
    {   // --update (from the launcher): the install folder is where this setup lives; its options are in setup.cfg
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; i < argc; i++) if (!wcscmp(argv[i], L"--update")) upd_mode = true;
        LocalFree(argv);
    }
    if (upd_mode) {
        O.root = exe_dir(); O.shortcut = false;
        if (FILE* f = _wfopen((O.root / "setup.cfg").wstring().c_str(), L"r")) {
            char l[1024];
            while (fgets(l, sizeof(l), f)) {
                std::string v = l; while (!v.empty() && (v.back() == '\n' || v.back() == '\r')) v.pop_back();
                if (!v.compare(0, 7, "colors=")) O.all_colors = v.substr(7) != "original";
                else if (!v.compare(0, 7, "unlock=")) O.unlock = v.substr(7) != "0";
                else if (!v.compare(0, 4, "iso=")) O.iso = wd(v.substr(4));
            }
            fclose(f);
        }
        if (fs::exists(O.root / "game" / "game.iso")) O.iso = (O.root / "game" / "game.iso").wstring();
        page = 3;
    }
    upd_check();
    WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC; wc.lpfnWndProc = proc; wc.hInstance = hi; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = L"TTTRecompSetup";
    wc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));
    HICON small = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    RegisterClassW(&wc);
    HDC sdc = GetDC(nullptr); float dpi = GetDeviceCaps(sdc, LOGPIXELSX) / 96.0f; ReleaseDC(nullptr, sdc);
    RECT r = {0, 0, (LONG)(VW * dpi), (LONG)(VH * dpi)};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    win = CreateWindowW(L"TTTRecompSetup", L"TTTRecomp Setup", style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, hi, nullptr);
    DragAcceptFiles(win, TRUE);
    if (small) SendMessageW(win, WM_SETICON, ICON_SMALL, (LPARAM)small);
    HDC dc = GetDC(win);
    PIXELFORMATDESCRIPTOR pfd = {sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32};
    pfd.cAlphaBits = 8; pfd.iLayerType = PFD_MAIN_PLANE;
    SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
    HGLRC gl = wglCreateContext(dc);
    wglMakeCurrent(dc, gl);
    typedef BOOL(WINAPI * SwapFn)(int);
    if (auto sw = (SwapFn)wglGetProcAddress("wglSwapIntervalEXT")) sw(1);
    fs::path fonts = port_dir() / "games" / "ttt" / "fonts";
    load_font(F[0], fonts / "sz_name");
    load_font(F[1], fonts / "map_name");
    if (F[1].g.empty()) F[1] = F[0];
    make_spark();
    if (F[0].g.empty()) MessageBoxW(win, L"No se pudieron preparar los archivos del asistente. Vuelve a descargarlo.", L"TTTRecomp", MB_OK | MB_ICONWARNING);
    ShowWindow(win, SW_SHOW);
    double last = now_s(), t = 0;
    MSG msg;
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { if (msg.message == WM_QUIT) goto out; TranslateMessage(&msg); DispatchMessageW(&msg); }
        double n = now_s(); float dt = (float)std::min(0.05, n - last); last = n; t += dt;
        RECT c; GetClientRect(win, &c);
        int cw = c.right, ch = c.bottom;
        scale = std::min(cw / VW, ch / VH); offx = (cw - VW * scale) / 2; offy = (ch - VH * scale) / 2;
        glViewport(0, 0, cw, ch);
        glClearColor(0.055f, 0.07f, 0.11f, 1); glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(-offx / scale, (cw - offx) / scale, (ch - offy) / scale, -offy / scale, -1, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        tick_parts(dt);
        draw_header((float)t);
        if (upd_mode && !build_started) {   // update: first this setup (newer release), then the game
            if (upd_state == 2) upd_apply();
            else if (upd_state == 1 || upd_state == 4) start_build();
        }
        if (page == 0) page_welcome((float)t);
        else if (page == 1) page_iso((float)t);
        else if (page == 2) page_options((float)t);
        else if (page == 3) page_build((float)t);
        else page_done((float)t);
        if (page <= 2) upd_banner();
        mclick = kenter = kesc = false;
        SwapBuffers(dc);
    }
out:
    cancel_flag = true;
    if (job) TerminateJobObject(job, 1);
    if (th.joinable()) th.join();
    std::error_code ec;
    if (!payload_dir.empty()) fs::remove_all(payload_dir, ec);
    return 0;
}
