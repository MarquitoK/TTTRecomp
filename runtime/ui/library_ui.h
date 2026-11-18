#pragma once
// Mod library: network, sign in, install and publish; drawn by the settings menu (overlay_lib.h): catalog in the online service (/lib), files in GitHub Releases of each modder. Included from mods_ui.h
#include <winhttp.h>
#include <wincred.h>
#include <bcrypt.h>
#include <deque>
#include <functional>
#include <condition_variable>
#include <ctime>
#include "third_party/stb_image.h"

static const char* LB_CLIENT = "Ov23licPiX4TuhtKr9xK";
static const char* LB_URL = "https://ttt-online.kkmarquito.workers.dev";
static const wchar_t* LB_CRED_LIB = L"TTTRecomp/library";
static const wchar_t* LB_CRED_GH = L"TTTRecomp/github";
static const int LB_NTAGS = 12;
static const char* LB_TAG_KEY[LB_NTAGS] = {"textures", "audio", "chars", "roster", "maps", "menus", "attacks", "models", "story", "logic", "other", "devs"};
static const wchar_t* lb_tag_name(int i) { return i >= 0 && i < MOD_NTAGS ? MOD_TAGS[i] : L"Devs"; }

enum { LA_NONE = 0, LA_SIGNIN, LA_SIGNOUT, LA_TAG, LA_COL, LA_SORT, LA_RANKED, LA_CARD, LA_BACK, LA_SHOT, LA_INSTALL, LA_LIKE, LA_REPORT, LA_WHY,
       LA_CANCEL, LA_COPYOPEN, LA_LINK, LA_AUTHOR, LA_RETRY, LA_HIDE, LA_REMOVE, LA_BAN, LA_MINE_DEL, LA_THUMB,
       LA_PUBLISH, LA_PICK, LA_PICK_FILE, LA_PUB_GO, LA_MENU, LA_MENU_CLOSE, LA_REPO, LA_SHARE, LA_COVER, LA_COVER_DEL, LA_SHOT_ADD, LA_SHOT_DEL, LA_PTAG, LA_PRANKED, LA_EDIT };

// ---- JSON
struct LbJ {
    int t = 0;
    double n = 0;
    std::string s;
    std::vector<LbJ> a;
    std::vector<std::pair<std::string, LbJ>> o;
    const LbJ& operator[](const char* k) const { static LbJ none; if (t == 5) for (auto& p : o) if (p.first == k) return p.second; return none; }
    std::string str() const { return t == 3 ? s : t == 2 ? std::to_string((long long)n) : ""; }
    long long num() const { return t == 2 || t == 1 ? (long long)n : t == 3 ? atoll(s.c_str()) : 0; }
};

static void lbj_ws(const char*& p) { while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++; }

static void lbj_utf8(std::string& o, uint32_t c) {
    if (c < 0x80) o += (char)c;
    else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 63)); }
    else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 63)); o += (char)(0x80 | (c & 63)); }
    else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 63)); o += (char)(0x80 | ((c >> 6) & 63)); o += (char)(0x80 | (c & 63)); }
}

static bool lbj_str(const char*& p, std::string& o) {
    if (*p != '"') return false;
    p++;
    while (*p && *p != '"') {
        if (*p != '\\') { o += *p++; continue; }
        p++;
        char e = *p++;
        if (e == 'n') o += '\n'; else if (e == 't') o += '\t'; else if (e == 'r') o += '\r'; else if (e == 'b') o += '\b'; else if (e == 'f') o += '\f';
        else if (e == 'u') {
            auto hx = [&](uint32_t& v) { v = 0; for (int i = 0; i < 4; i++) { char c = *p; if (!c) return false; p++; v = v * 16 + (c >= '0' && c <= '9' ? c - '0' : (c | 32) - 'a' + 10); } return true; };
            uint32_t c;
            if (!hx(c)) return false;
            if (c >= 0xD800 && c < 0xDC00 && p[0] == '\\' && p[1] == 'u') { p += 2; uint32_t c2; if (!hx(c2)) return false; c = 0x10000 + ((c - 0xD800) << 10) + (c2 - 0xDC00); }
            lbj_utf8(o, c);
        } else if (e) o += e;
        else return false;
    }
    if (*p != '"') return false;
    p++;
    return true;
}

static bool lbj_val(const char*& p, LbJ& v, int depth = 0) {
    if (depth > 32) return false;
    lbj_ws(p);
    if (*p == '{') {
        v.t = 5; p++; lbj_ws(p);
        if (*p == '}') { p++; return true; }
        for (;;) {
            lbj_ws(p);
            std::string k;
            if (!lbj_str(p, k)) return false;
            lbj_ws(p);
            if (*p++ != ':') return false;
            v.o.emplace_back(k, LbJ());
            if (!lbj_val(p, v.o.back().second, depth + 1)) return false;
            lbj_ws(p);
            if (*p == ',') { p++; continue; }
            if (*p == '}') { p++; return true; }
            return false;
        }
    }
    if (*p == '[') {
        v.t = 4; p++; lbj_ws(p);
        if (*p == ']') { p++; return true; }
        for (;;) {
            v.a.emplace_back();
            if (!lbj_val(p, v.a.back(), depth + 1)) return false;
            lbj_ws(p);
            if (*p == ',') { p++; continue; }
            if (*p == ']') { p++; return true; }
            return false;
        }
    }
    if (*p == '"') { v.t = 3; return lbj_str(p, v.s); }
    if (!strncmp(p, "true", 4)) { v.t = 1; v.n = 1; p += 4; return true; }
    if (!strncmp(p, "false", 5)) { v.t = 1; v.n = 0; p += 5; return true; }
    if (!strncmp(p, "null", 4)) { v.t = 0; p += 4; return true; }
    char* e = nullptr;
    v.n = strtod(p, &e);
    if (e == p) return false;
    v.t = 2; p = e;
    return true;
}

static LbJ lbj_parse(const std::string& s) { LbJ v; const char* p = s.c_str(); if (!lbj_val(p, v)) v = LbJ(); return v; }

static std::string lbj_esc(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c == '\n') o += "\\n"; else if (c == '\r') o += "\\r"; else if (c == '\t') o += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
        else o += (char)c;
    }
    return o + "\"";
}

static std::string lb_urlenc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
        else { char b[4]; snprintf(b, sizeof(b), "%%%02X", c); o += b; }
    }
    return o;
}

// ---- HTTP (WinHTTP, follows redirects; body to memory or to a file with progress and SHA-256)
struct LbResp { int st = 0; std::string body; };
typedef std::function<bool(uint64_t, uint64_t)> LbProg;

static HINTERNET lb_ses() {
    static HINTERNET s = nullptr;
    static std::once_flag f;
    std::call_once(f, [] {
        s = WinHttpOpen(L"TTTRecomp", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (s) WinHttpSetTimeouts(s, 10000, 10000, 20000, 30000);
    });
    return s;
}

static LbResp lb_http(const char* method, const std::string& url, const std::string& hdr, const std::string& body, FILE* out = nullptr, const LbProg& prog = nullptr, BCRYPT_HASH_HANDLE hh = nullptr) {
    LbResp r;
    std::wstring wu = mw_w(url);
    URL_COMPONENTS uc; memset(&uc, 0, sizeof(uc)); uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = (DWORD)-1; uc.dwUrlPathLength = (DWORD)-1; uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wu.c_str(), 0, 0, &uc) || !uc.lpszHostName) return r;
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength), path = std::wstring(uc.lpszUrlPath ? uc.lpszUrlPath : L"/", uc.lpszUrlPath ? uc.dwUrlPathLength : 1);
    if (uc.lpszExtraInfo) path += std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    HINTERNET s = lb_ses();
    HINTERNET con = s ? WinHttpConnect(s, host.c_str(), uc.nPort, 0) : nullptr;
    HINTERNET rq = con ? WinHttpOpenRequest(con, mw_w(method).c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    if (rq) {
        std::wstring h = mw_w(hdr);
        bool ok = WinHttpSendRequest(rq, h.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : h.c_str(), h.empty() ? 0 : (DWORD)-1L,
                                     body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) && WinHttpReceiveResponse(rq, nullptr);
        if (ok) {
            DWORD code = 0, sz = sizeof(code);
            WinHttpQueryHeaders(rq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
            r.st = (int)code;
            uint64_t total = 0, done = 0;
            wchar_t cl[32]; DWORD cls = sizeof(cl);
            if (WinHttpQueryHeaders(rq, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, cl, &cls, WINHTTP_NO_HEADER_INDEX)) total = _wcstoui64(cl, nullptr, 10);
            bool to_file = out && code == 200;
            std::vector<char> b(1 << 16);
            for (;;) {
                DWORD n = 0;
                if (!WinHttpReadData(rq, b.data(), (DWORD)b.size(), &n)) { r.st = -1; break; }
                if (!n) break;
                if (to_file) {
                    if (fwrite(b.data(), 1, n, out) != n) { r.st = -2; break; }
                    if (hh) BCryptHashData(hh, (PUCHAR)b.data(), n, 0);
                } else if (r.body.size() < (64u << 20)) r.body.append(b.data(), n);
                done += n;
                if (prog && !prog(done, total)) { r.st = -3; break; }
            }
        } else r.st = 0;
    }
    if (rq) WinHttpCloseHandle(rq);
    if (con) WinHttpCloseHandle(con);
    return r;
}

static std::string lb_hex(const uint8_t* d, size_t n) { std::string o; char b[3]; for (size_t i = 0; i < n; i++) { snprintf(b, 3, "%02x", d[i]); o += b; } return o; }

struct LbSha {
    BCRYPT_ALG_HANDLE a = nullptr; BCRYPT_HASH_HANDLE h = nullptr;
    LbSha() { if (BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) BCryptCreateHash(a, &h, nullptr, 0, nullptr, 0, 0); }
    ~LbSha() { if (h) BCryptDestroyHash(h); if (a) BCryptCloseAlgorithmProvider(a, 0); }
    std::string hex() { uint8_t o[32]; if (!h || BCryptFinishHash(h, o, 32, 0)) return ""; BCryptDestroyHash(h); h = nullptr; return lb_hex(o, 32); }
};

static std::string lb_sha_hex(const std::string& s) { LbSha k; if (k.h) BCryptHashData(k.h, (PUCHAR)s.data(), (ULONG)s.size(), 0); return k.hex(); }

// ---- secrets in the Windows Credential Manager
static bool lb_cred_get(const wchar_t* name, std::string& v) {
    PCREDENTIALW c = nullptr;
    if (!CredReadW(name, CRED_TYPE_GENERIC, 0, &c) || !c) return false;
    v.assign((const char*)c->CredentialBlob, c->CredentialBlobSize);
    CredFree(c);
    return !v.empty();
}

static void lb_cred_set(const wchar_t* name, const std::string& v) {
    CREDENTIALW c; memset(&c, 0, sizeof(c));
    c.Type = CRED_TYPE_GENERIC; c.TargetName = (LPWSTR)name; c.UserName = (LPWSTR)L"TTTRecomp";
    c.CredentialBlobSize = (DWORD)v.size(); c.CredentialBlob = (LPBYTE)v.data(); c.Persist = CRED_PERSIST_LOCAL_MACHINE;
    CredWriteW(&c, 0);
}

static void lb_cred_del(const wchar_t* name) { CredDeleteW(name, CRED_TYPE_GENERIC, 0); }

// ---- async: work on detached threads, results run on the window loop (lb_poll)
static std::mutex lb_qm;
static std::deque<std::function<void()>> lb_queue;
static void lb_post(std::function<void()> f) { std::lock_guard<std::mutex> g(lb_qm); lb_queue.push_back(std::move(f)); }
static void lb_run(std::function<void()> f) { std::thread(std::move(f)).detach(); }

// ---- state
struct LbMod {
    std::string id, name, ver, cover, author, author_name, author_av, descr, link, url, sha;
    std::vector<int> tags;
    std::vector<std::string> shots, reports_list;
    long long size = 0, updated = 0, created = 0;
    int ranked = 0, dl = 0, likes = 0, liked = 0, hidden = 0, reports = 0, mine = 0;
    bool full = false;
};

struct LbUser { bool in = false; std::string id, key, login, name, av; bool admin = false, banned = false; };

static std::vector<LbMod> lb_mods;
static LbMod lb_cur;
static LbUser lb_me;
static int lb_gen = 0, lb_dgen = 0, lb_lgen = 0, lb_total = 0, lb_page = 0, lb_tag = -1, lb_col = 0, lb_sort = 0, lb_view = 0, lb_shot = 0, lb_overlay = 0;
static bool lb_ranked = false, lb_loading = false, lb_err = false, lb_started = false, lb_dloading = false;
static std::string lb_text, lb_code, lb_verify, lb_login_err;
static int lb_login_st = 0;
static std::atomic<bool> lb_login_cancel(false), lb_dl_cancel(false);
static struct { int st = 0; std::string id, name; } lb_dl;
static std::atomic<uint64_t> lb_dl_done(0), lb_dl_total(0), lb_dl_t0(0), lb_up_t0(0);
static std::string lb_base() { const char* e = getenv("TTT_ONLINE_URL"); return e && *e ? e : LB_URL; }
static std::string lb_auth() { return lb_me.in && !lb_me.key.empty() ? "Authorization: Lib " + lb_me.id + ":" + lb_me.key + "\r\n" : ""; }
static std::string lb_cache_dir() { return mw_root + "/../library_cache"; }
static std::function<void(const std::wstring&)> lb_notice_fn;
static std::function<void(const std::string&)> lb_done_fn;
static void lb_notice(const std::wstring& t) { if (lb_notice_fn) lb_notice_fn(t); }
static void lb_load(bool more);
static std::string mw_install(const std::string& fpath);
struct LbPub {
    int mode = 0, ranked = -1, st = 0;
    std::string mod, src, lib, cover, old_ver, login, txt;
    std::vector<std::string> shots;
    std::vector<int> tags;
    size_t files = 0;
    uint64_t bytes = 0;
    std::wstring step;
    bool blocked = false, checking = false;
};
static LbPub lb_pub;
static std::atomic<uint64_t> lb_up_done(0), lb_up_total(0);
static std::string lb_pending;
static bool lb_me_checking = false;
static bool mw_pack(const std::string& dir, const std::string& dest, size_t* nfiles, uint64_t* nbytes, std::string* err);
static bool mw_repack(const std::string& src, const std::string& dest, const std::string& modtxt, std::string* err);
static uint32_t cm_crc(const uint8_t* d, size_t n);
static void lb_pub_open_file(const std::string& path);
static std::string lb_slug(const std::string& s);
static void lb_pub_open(const std::string& folder);
static void lb_edit_open();
static void lb_pub_go();
static std::string lb_pick_image();
static bool lb_has_tag(int t);
static void lb_login_start();

// ---- images: downloaded once (disk cache), decoded and shrunk on worker threads
struct LbImg { int st = 0, w = 0, h = 0; std::vector<uint8_t> px; };
static std::mutex lb_im;
static std::map<std::string, LbImg> lb_imgs;
static std::mutex lb_semm;
static std::condition_variable lb_semc;
static int lb_sem = 0;

static void lb_shrink(std::vector<uint8_t>& px, int& w, int& h, int maxw) {
    int f = 1;
    while (w / (f + 1) >= maxw && h / (f + 1) >= maxw * 9 / 16) f++;
    if (f == 1) return;
    int nw = w / f, nh = h / f;
    std::vector<uint8_t> o((size_t)nw * nh * 4);
    for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) {
        uint32_t s[4] = {0, 0, 0, 0};
        for (int j = 0; j < f; j++) for (int i = 0; i < f; i++) { const uint8_t* p = &px[((size_t)(y * f + j) * w + x * f + i) * 4]; for (int c = 0; c < 4; c++) s[c] += p[c]; }
        for (int c = 0; c < 4; c++) o[((size_t)y * nw + x) * 4 + c] = (uint8_t)(s[c] / (f * f));
    }
    px.swap(o); w = nw; h = nh;
}

static void lb_img_want(const std::string& url) {
    if (url.empty()) return;
    { std::lock_guard<std::mutex> g(lb_im); LbImg& i = lb_imgs[url]; if (i.st) return; i.st = 1; }
    std::string dir = lb_cache_dir();
    lb_run([url, dir]() {
        { std::unique_lock<std::mutex> k(lb_semm); lb_semc.wait(k, [] { return lb_sem < 4; }); lb_sem++; }
        std::string file = dir + "/" + lb_sha_hex(url).substr(0, 24) + ".img";
        std::vector<uint8_t> d;
        bool local = url.rfind("file:", 0) == 0;
        if (local) file = url.substr(5);
        if (FILE* f = mods_fopen(file, "rb")) {
            fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
            if (n > 0) { d.resize(n); if (fread(d.data(), 1, n, f) != (size_t)n) d.clear(); }
            fclose(f);
        }
        if (d.empty() && !local) {
            LbResp r = lb_http("GET", url, "", "");
            if (r.st == 200 && !r.body.empty()) {
                d.assign(r.body.begin(), r.body.end());
                std::error_code ec;
                std::filesystem::create_directories(std::filesystem::u8path(dir), ec);
                if (FILE* f = mods_fopen(file, "wb")) { fwrite(d.data(), 1, d.size(), f); fclose(f); }
            }
        }
        int w = 0, h = 0, n = 0;
        uint8_t* p = d.empty() ? nullptr : stbi_load_from_memory(d.data(), (int)d.size(), &w, &h, &n, 4);
        std::vector<uint8_t> px;
        if (p && (int64_t)w * h <= 8192LL * 8192) { px.assign(p, p + (size_t)w * h * 4); lb_shrink(px, w, h, 640); }
        if (p) stbi_image_free(p);
        { std::lock_guard<std::mutex> g(lb_im); LbImg& i = lb_imgs[url]; if (!px.empty()) { i.st = 2; i.w = w; i.h = h; i.px.swap(px); } else i.st = 3; }
        { std::lock_guard<std::mutex> g(lb_semm); lb_sem--; }
        lb_semc.notify_one();
        lb_post([]() { });
    });
}

static std::wstring lb_size(long long b) {
    wchar_t t[32];
    if (b >= (1LL << 30)) swprintf(t, 32, L"%.1f GB", b / 1073741824.0);
    else if (b >= (1LL << 20)) swprintf(t, 32, L"%.0f MB", b / 1048576.0);
    else swprintf(t, 32, L"%lld KB", std::max(1LL, b / 1024));
    return t;
}

// installed state from mods/*/mod.txt (keys written when installing from the library)
static int lb_installed(const LbMod& m) {
    for (auto& x : mw_mods) if (mw_field(x, "biblioteca") == m.id) return mw_field(x, "biblioteca_version") == m.ver ? 2 : 1;
    return 0;
}

static LbMod lb_mod_of(const LbJ& j) {
    LbMod m;
    m.id = j["id"].str(); m.name = j["name"].str(); m.ver = j["ver"].str(); m.cover = j["cover"].str();
    m.author = j["author"].str(); m.author_name = j["author_name"].str(); m.author_av = j["author_av"].str();
    m.size = j["size"].num(); m.ranked = (int)j["ranked"].num(); m.dl = (int)j["dl"].num(); m.likes = (int)j["likes"].num(); m.liked = (int)j["liked"].num();
    m.hidden = (int)j["hidden"].num(); m.updated = j["updated"].num(); m.reports = (int)j["reports"].num(); m.mine = (int)j["mine"].num();
    for (auto& t : j["tags"].a) for (int i = 0; i < LB_NTAGS; i++) if (t.s == LB_TAG_KEY[i]) m.tags.push_back(i);
    if (j["descr"].t || j["url"].t) {
        m.full = true;
        m.descr = j["descr"].str(); m.link = j["link"].str(); m.url = j["url"].str(); m.sha = j["sha"].str(); m.created = j["created"].num();
        for (auto& s : j["shots"].a) if (s.t == 3) m.shots.push_back(s.s);
        for (auto& r : j["report_list"].a) m.reports_list.push_back(r["why"].str() + " (" + r["login"].str() + ")");
    }
    return m;
}

static std::vector<std::string> lb_gallery() {
    std::vector<std::string> g;
    if (!lb_cur.cover.empty()) g.push_back(lb_cur.cover);
    for (auto& s : lb_cur.shots) g.push_back(s);
    return g;
}

static std::wstring lb_host(const std::string& link) {
    size_t a = link.find("://");
    std::string h = a == std::string::npos ? link : link.substr(a + 3);
    h = h.substr(0, h.find_first_of("/?#"));
    if (h.rfind("www.", 0) == 0) h = h.substr(4);
    return mw_w(h.empty() ? "link" : h);
}

// ---- network actions
static void lb_list_done(int gen, bool more, int page, const LbResp& r, bool review) {
    if (gen != lb_gen) return;
    lb_loading = false;
    LbJ j = r.st == 200 ? lbj_parse(r.body) : LbJ();
    const LbJ& arr = review ? j : j["mods"];
    if (arr.t != 4) { printf("biblioteca: la lista fallo (codigo %d)\n", r.st); if (!more) lb_err = true; return; }
    lb_err = false;
    if (!more) printf("biblioteca: lista con %d mods (total %d)\n", (int)arr.a.size(), review ? (int)arr.a.size() : (int)j["total"].num());
    for (auto& o : arr.a) { lb_mods.push_back(lb_mod_of(o)); lb_img_want(lb_mods.back().cover); }
    lb_total = review ? (int)lb_mods.size() : (int)j["total"].num();
    lb_page = page;
}

static void lb_load(bool more) {
    if (more && (lb_loading || (int)lb_mods.size() >= lb_total || lb_col == 3)) return;
    int gen = more ? lb_gen : ++lb_gen;
    if (!more) { lb_mods.clear(); lb_total = 0; }
    int page = more ? lb_page + 1 : 0;
    bool review = lb_col == 3 && lb_me.admin;
    static const char* SORT[3] = {"new", "dl", "likes"};
    std::string q;
    if (review) q = "admin/reports";
    else {
        q = "list?sort=" + std::string(SORT[lb_sort]) + "&page=" + std::to_string(page);
        if (!lb_text.empty()) q += "&q=" + lb_urlenc(lb_text);
        if (lb_tag >= 0) q += "&tag=" + std::string(LB_TAG_KEY[lb_tag]);
        if (lb_ranked) q += "&ranked=1";
        if (lb_col == 1) q += "&liked=1";
        if (lb_col == 2) q += "&author=" + lb_urlenc(lb_me.login);
    }
    lb_loading = true; lb_err = false;
    std::string url = lb_base() + "/lib/" + q, auth = lb_auth();
    lb_run([=]() { LbResp r = lb_http("GET", url, auth, ""); lb_post([=]() { lb_list_done(gen, more, page, r, review); }); });
}

static void lb_open_detail(const LbMod& m) {
    lb_cur = m; lb_view = 1; lb_shot = 0;
    int gen = ++lb_dgen;
    std::string url = lb_base() + "/lib/mod/" + m.id, auth = lb_auth();
    lb_run([=]() {
        LbResp r = lb_http("GET", url, auth, "");
        lb_post([=]() {
            if (gen != lb_dgen) return;
            if (r.st == 404) { lb_view = 0; lb_notice(L"This mod is no longer in the library."); lb_load(false); return; }
            if (r.st != 200) { lb_notice(L"Can't reach the mod library."); return; }
            lb_cur = lb_mod_of(lbj_parse(r.body));
            for (auto& s : lb_gallery()) lb_img_want(s);
        });
    });
}

static void lb_me_set(const LbJ& j, const std::string& id, const std::string& key) {
    lb_me.in = true; lb_me.id = id; lb_me.key = key;
    lb_me.login = j["login"].str(); lb_me.name = j["name"].str(); lb_me.av = j["av"].str();
    lb_me.admin = j["admin"].num() != 0; lb_me.banned = j["banned"].num() != 0;
}

static void lb_me_check(int tries = 2) {
    std::string s;
    if (!lb_cred_get(LB_CRED_LIB, s)) return;
    size_t c = s.find(':');
    if (c == std::string::npos) { lb_cred_del(LB_CRED_LIB); return; }
    std::string id = s.substr(0, c), key = s.substr(c + 1);
    std::string url = lb_base() + "/lib/me", auth = "Authorization: Lib " + id + ":" + key + "\r\n";
    lb_me_checking = true;
    lb_run([=]() {
        LbResp r = lb_http("GET", url, auth, "");
        if ((r.st == 0 || r.st >= 500) && tries > 1) {
            printf("biblioteca: revision de sesion sin respuesta (codigo %d), reintento\n", r.st);
            Sleep(2000);
            lb_post([=]() { lb_me_check(tries - 1); });
            return;
        }
        lb_post([=]() {
            lb_me_checking = false;
            if (r.st != 200) printf("biblioteca: revision de sesion, codigo %d%s\n", r.st, r.st == 401 ? " (sesion vencida, se borra)" : "");
            if (!lb_pending.empty()) {
                if (r.st == 200) { lb_me_set(lbj_parse(r.body), id, key); std::string f = lb_pending; lb_pending.clear(); lb_pub_open(f); }
                else if (r.st == 401) lb_login_start();
                else { lb_pending.clear(); lb_notice(L"Can't reach the mod library."); }
            }
            if (r.st == 200) { lb_me_set(lbj_parse(r.body), id, key); printf("biblioteca: sesion guardada de %s%s\n", lb_me.login.c_str(), lb_me.admin ? " (admin)" : ""); lb_load(false); }
            else if (r.st == 401) { lb_cred_del(LB_CRED_LIB); lb_cred_del(LB_CRED_GH); }
        });
    });
}

// GitHub device flow: code -> user confirms in the browser -> token -> library session in the service
static void lb_login_start() {
    lb_overlay = 1; lb_login_st = 1; lb_code.clear(); lb_login_err.clear();
    lb_login_cancel = false;
    int gen = ++lb_lgen;
    std::string base = lb_base();
    auto fail = [gen](const std::string& why) { lb_post([=]() { if (gen != lb_lgen) return; lb_login_st = 4; lb_login_err = why; }); };
    lb_run([=]() {
        const std::string H = "Accept: application/json\r\nContent-Type: application/x-www-form-urlencoded\r\n";
        LbResp r = lb_http("POST", "https://github.com/login/device/code", H, "client_id=" + std::string(LB_CLIENT) + "&scope=public_repo");
        LbJ j = lbj_parse(r.body);
        std::string dc = j["device_code"].str(), uc = j["user_code"].str(), vu = j["verification_uri"].str();
        if (r.st != 200 || dc.empty() || uc.empty()) { printf("biblioteca: GitHub no dio el codigo (codigo %d)\n", r.st); fail(r.st ? "GitHub did not answer as expected. Try again in a moment." : "Can't reach GitHub. Check your internet connection."); return; }
        int iv = std::max(5, (int)j["interval"].num());
        long long until = (long long)GetTickCount64() + std::max(60LL, j["expires_in"].num()) * 1000;
        lb_post([=]() { if (gen != lb_lgen) return; lb_login_st = 2; lb_code = uc; lb_verify = vu.rfind("https://github.com/", 0) == 0 ? vu : "https://github.com/login/device"; });
        std::string token;
        while (!lb_login_cancel && (long long)GetTickCount64() < until) {
            for (int k = 0; k < iv * 10 && !lb_login_cancel; k++) Sleep(100);
            if (lb_login_cancel) return;
            LbResp t = lb_http("POST", "https://github.com/login/oauth/access_token", H,
                               "client_id=" + std::string(LB_CLIENT) + "&device_code=" + dc + "&grant_type=urn:ietf:params:oauth:grant-type:device_code");
            LbJ tj = lbj_parse(t.body);
            token = tj["access_token"].str();
            if (!token.empty()) break;
            std::string e = tj["error"].str();
            if (e == "slow_down") iv += 5;
            else if (e == "access_denied") { fail("The sign in was cancelled on GitHub."); return; }
            else if (e == "expired_token") { fail("The code expired. Try again."); return; }
        }
        if (lb_login_cancel) return;
        if (token.empty()) { fail("The code expired. Try again."); return; }
        lb_post([=]() { if (gen == lb_lgen) { lb_login_st = 3; } });
        LbResp s = lb_http("POST", base + "/lib/login", "Authorization: Bearer " + token + "\r\n", "");
        LbJ sj = lbj_parse(s.body);
        std::string id = sj["id"].str(), key = sj["key"].str();
        if (s.st != 200 || id.empty() || key.empty()) { printf("biblioteca: el servicio rechazo la sesion (codigo %d)\n", s.st); fail(s.st ? "The mod library refused the sign in." : "Can't reach the mod library."); return; }
        lb_post([=]() {
            if (gen != lb_lgen) return;
            lb_cred_set(LB_CRED_GH, token);
            lb_cred_set(LB_CRED_LIB, id + ":" + key);
            lb_me_set(sj, id, key);
            printf("biblioteca: sesion iniciada como %s%s\n", lb_me.login.c_str(), lb_me.admin ? " (admin)" : "");
            lb_login_st = 0; lb_overlay = 0;
            lb_notice(L"Signed in as " + mw_w(lb_me.login) + L".");
            if (lb_view == 1) lb_open_detail(lb_cur);
            if (!lb_pending.empty()) { std::string f = lb_pending; lb_pending.clear(); lb_pub_open(f); }
            lb_load(false);
        });
    });
}

static void lb_signout() {
    std::string url = lb_base() + "/lib/logout", auth = lb_auth();
    lb_run([=]() { lb_http("POST", url, auth, ""); });
    lb_cred_del(LB_CRED_LIB); lb_cred_del(LB_CRED_GH);
    lb_me = LbUser();
    if (lb_col) lb_col = 0;
    if (lb_view == 1) lb_cur.liked = 0, lb_cur.mine = 0;
    lb_load(false);
}

// POST to the service and refresh the detail and the list on success
static void lb_action(const std::string& method, const std::string& path, const std::string& body, const std::wstring& ok, bool back) {
    std::string url = lb_base() + "/lib/" + path, auth = lb_auth() + (body.empty() ? "" : "Content-Type: application/json\r\n");
    lb_run([=]() {
        LbResp r = lb_http(method.c_str(), url, auth, body);
        lb_post([=]() {
            if (r.st != 200) { lb_notice(r.st == 401 ? L"Sign in again to do that." : L"The mod library did not accept that."); return; }
            if (!ok.empty()) lb_notice(ok);
            if (back) lb_view = 0; else lb_open_detail(lb_cur);
            lb_load(false);
        });
    });
}

static void lb_like() {
    if (!lb_me.in) { lb_login_start(); return; }
    int on = lb_cur.liked ? 0 : 1;
    std::string id = lb_cur.id, url = lb_base() + "/lib/like/" + id, auth = lb_auth() + "Content-Type: application/json\r\n";
    lb_cur.liked = on; lb_cur.likes += on ? 1 : -1;
    lb_run([=]() {
        LbResp r = lb_http("POST", url, auth, on ? "{\"on\":1}" : "{\"on\":0}");
        lb_post([=]() {
            LbJ j = lbj_parse(r.body);
            if (r.st != 200) { lb_notice(L"Your heart could not be saved."); return; }
            int n = (int)j["likes"].num();
            if (lb_cur.id == id) { lb_cur.likes = n; lb_cur.liked = on; }
            for (auto& m : lb_mods) if (m.id == id) { m.likes = n; m.liked = on; }
        });
    });
}

static void lb_finish_install(const std::string& tmp, const std::string& libid, const std::string& ver, const std::string& name) {
    std::string id = mw_install(tmp);
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(tmp), ec);
    lb_dl.st = 0;
    if (!id.empty()) {
        std::string f = mw_root + "/" + id + "/mod.txt", keep;
        if (FILE* r = mods_fopen(f, "rb")) {
            char line[4096];
            while (fgets(line, sizeof(line), r)) if (strncmp(line, "biblioteca", 10)) keep += line;
            fclose(r);
        }
        if (!keep.empty() && keep.back() != '\n') keep += "\n";
        keep += "biblioteca=" + libid + "\nbiblioteca_version=" + ver + "\n";
        if (FILE* w = mods_fopen(f, "wb")) { fwrite(keep.data(), 1, keep.size(), w); fclose(w); }
        mw_mods = mods_list(mw_root);
        lb_notice(L"Installed \"" + mw_w(name) + L"\". Changes apply after restarting the game.");
        if (lb_done_fn) lb_done_fn(id);
    }
}

static void lb_install() {
    if (lb_dl.st) return;
    LbMod m = lb_cur;
    lb_dl.st = 1; lb_dl.id = m.id; lb_dl.name = m.name;
    lb_dl_done = 0; lb_dl_total = 0; lb_dl_cancel = false;
    std::string base = lb_base(), auth = lb_auth(), dir = lb_cache_dir();
    auto fail = [](const std::wstring& why) { lb_post([=]() { lb_dl.st = 0; lb_notice(why); }); };
    lb_run([=]() {
        LbResp r = lb_http("POST", base + "/lib/dl/" + m.id, auth, "");
        LbJ j = lbj_parse(r.body);
        std::string url = j["url"].str(), sha = j["sha"].str(), ver = j["ver"].str();
        if (r.st != 200 || url.rfind("https://github.com/", 0) != 0) { fail(r.st == 404 ? L"This mod is no longer in the library." : L"Can't reach the mod library."); return; }
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::u8path(dir), ec);
        std::string fname = url.substr(url.find_last_of('/') + 1);
        for (auto& c : fname) if (!isalnum((unsigned char)c) && c != '-' && c != '_' && c != '.') c = '_';
        if (fname.size() < 7 || fname.compare(fname.size() - 6, 6, ".mkpak")) fname = m.id + ".mkpak";
        std::string tmp = dir + "/" + fname;
        FILE* f = mods_fopen(tmp, "wb");
        if (!f) { fail(L"Could not write the download to the disk."); return; }
        LbSha h;
        lb_dl_t0 = GetTickCount64();
        LbResp d = lb_http("GET", url, "", "", f, [](uint64_t done, uint64_t total) { lb_dl_done = done; lb_dl_total = total; return !lb_dl_cancel; }, h.h);
        fclose(f);
        std::string got = h.hex();
        auto drop = [&]() { std::error_code e2; std::filesystem::remove(std::filesystem::u8path(tmp), e2); };
        if (d.st == -3) { drop(); fail(L"Download cancelled."); return; }
        if (d.st == 404) { drop(); lb_http("POST", base + "/lib/broken/" + m.id, "", ""); fail(L"The file of this mod is no longer on GitHub."); return; }
        if (d.st != 200) { drop(); fail(L"The download failed. Try again."); return; }
        if (got != sha) { drop(); printf("biblioteca: %s hash %s, esperado %s\n", m.id.c_str(), got.c_str(), sha.c_str()); fail(L"The downloaded file is not the one that was published. Install cancelled."); return; }
        printf("biblioteca: %s descargado (%llu bytes)\n", m.id.c_str(), (unsigned long long)lb_dl_done.load());
        lb_post([=]() { lb_dl.st = 2; });
        lb_post([=]() { lb_finish_install(tmp, m.id, ver.empty() ? m.ver : ver, m.name); });
    });
}

static void lb_report(int why) {
    static const char* W[4] = {"broken", "stolen", "offensive", "other"};
    lb_overlay = 0;
    lb_action("POST", "report/" + lb_cur.id, std::string("{\"why\":\"") + W[why & 3] + "\"}", L"Thanks, the report was sent.", false);
}

static void lb_act(int act, int arg) {
    switch (act) {
        case LA_SHARE: {
            std::wstring u = mw_w(lb_base() + "/m/" + lb_cur.id);
            if (OpenClipboard(ventana)) {
                HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (u.size() + 1) * sizeof(wchar_t));
                if (g) { memcpy(GlobalLock(g), u.c_str(), (u.size() + 1) * sizeof(wchar_t)); GlobalUnlock(g); EmptyClipboard(); if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g); }
                CloseClipboard();
            }
            lb_notice(L"Link copied: " + u);
            break;
        }
        case LA_REPO: if (!lb_me.login.empty()) ShellExecuteW(ventana, L"open", mw_w("https://github.com/" + lb_me.login + "/tttrecomp-mods/releases").c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
        case LA_SIGNIN: lb_login_start(); break;
        case LA_SIGNOUT: lb_signout(); break;
        case LA_TAG: lb_tag = arg; lb_view = 0; lb_load(false); break;
        case LA_COL: if (lb_col != arg) { lb_col = arg; lb_view = 0; lb_load(false); } break;
        case LA_SORT: if (lb_sort != arg) { lb_sort = arg; lb_load(false); } break;
        case LA_RANKED: lb_ranked = !lb_ranked; lb_load(false); break;
        case LA_CARD: if (arg >= 0 && arg < (int)lb_mods.size()) lb_open_detail(lb_mods[arg]); break;
        case LA_BACK: lb_view = 0; lb_dgen++; break;
        case LA_SHOT: lb_shot = arg; break;
        case LA_INSTALL: lb_install(); break;
        case LA_LIKE: lb_like(); break;
        case LA_REPORT: if (!lb_me.in) lb_login_start(); else lb_overlay = 2; break;
        case LA_WHY: lb_report(arg); break;
        case LA_CANCEL: if (lb_pub.st) break; lb_overlay = 0; lb_login_st = 0; lb_login_cancel = true; lb_lgen++; lb_pending.clear(); break;
        case LA_PUBLISH: if (!lb_me.in) lb_login_start(); else { mw_mods = mods_list(mw_root); lb_overlay = 3; } break;
        case LA_PICK: if (arg >= 0 && arg < (int)mw_mods.size()) lb_pub_open(mw_mods[arg].id); break;
        case LA_PUB_GO: lb_pub_go(); break;
        case LA_PICK_FILE: {
            wchar_t fn[4096] = L"";
            OPENFILENAMEW of; memset(&of, 0, sizeof(of));
            of.lStructSize = sizeof(of); of.hwndOwner = ventana;
            of.lpstrFilter = L"Mods (*.mkpak)\0*.mkpak\0";
            of.lpstrFile = fn; of.nMaxFile = 4096; of.lpstrTitle = L"Publish a .mkpak";
            of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&of)) lb_pub_open_file(mw_u8(fn));
            break;
        }
        case LA_COVER: { std::string p = lb_pick_image(); if (!p.empty()) lb_pub.cover = "file:" + p; break; }
        case LA_COVER_DEL: lb_pub.cover.clear(); break;
        case LA_SHOT_ADD: if (lb_pub.shots.size() < 4) { std::string p = lb_pick_image(); if (!p.empty()) lb_pub.shots.push_back("file:" + p); } break;
        case LA_SHOT_DEL: if (arg >= 0 && arg < (int)lb_pub.shots.size()) lb_pub.shots.erase(lb_pub.shots.begin() + arg); break;
        case LA_PTAG: if (lb_has_tag(arg)) lb_pub.tags.erase(std::find(lb_pub.tags.begin(), lb_pub.tags.end(), arg)); else lb_pub.tags.push_back(arg); std::sort(lb_pub.tags.begin(), lb_pub.tags.end()); break;
        case LA_PRANKED: lb_pub.ranked = lb_pub.ranked ? 0 : 1; break;
        case LA_EDIT: lb_edit_open(); break;
        case LA_COPYOPEN:
            if (!lb_code.empty() && OpenClipboard(ventana)) {
                std::wstring c = mw_w(lb_code);
                HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (c.size() + 1) * sizeof(wchar_t));
                if (g) { memcpy(GlobalLock(g), c.c_str(), (c.size() + 1) * sizeof(wchar_t)); GlobalUnlock(g); EmptyClipboard(); if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g); }
                CloseClipboard();
            }
            ShellExecuteW(ventana, L"open", mw_w(lb_verify).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case LA_LINK: if (lb_cur.link.rfind("http://", 0) == 0 || lb_cur.link.rfind("https://", 0) == 0) ShellExecuteW(ventana, L"open", mw_w(lb_cur.link).c_str(), nullptr, nullptr, SW_SHOWNORMAL); break;
        case LA_AUTHOR: lb_view = 0; lb_tag = -1; lb_col = 0; lb_text = lb_cur.author; lb_load(false); break;
        case LA_RETRY: lb_load(false); break;
        case LA_HIDE: lb_action("POST", "admin/hide/" + lb_cur.id, lb_cur.hidden ? "{\"hidden\":0}" : "{\"hidden\":1}", lb_cur.hidden ? L"The mod is visible again." : L"The mod is hidden.", false); break;
    }
}

// ---- publish (new mod or update, from an installed mod folder) and admin edit

static bool lb_has_tag(int t) { return std::find(lb_pub.tags.begin(), lb_pub.tags.end(), t) != lb_pub.tags.end(); }

// form fields: name, version, link, description
static std::string lb_ftxt[4];
static void lb_fset(int i, const std::string& v) { lb_ftxt[i] = v; }
static std::string lb_fget(int i) {
    std::string o;
    for (char c : lb_ftxt[i]) if (c != '\r') o += c;
    size_t a = o.find_first_not_of(" \n\t"), b = o.find_last_not_of(" \n\t");
    return a == std::string::npos ? "" : o.substr(a, b - a + 1);
}

// same rule as the ranked signature in net.h: maps, or non-texture files of character colours (damaged textures count)
static bool lb_rank_blocks(std::string k) {
    for (auto& c : k) c = (char)tolower((unsigned char)c);
    if (!k.compare(0, 9, "packfile/")) k = k.substr(9);
    bool map = !k.compare(0, 5, "maps/");
    if (!map && (k.compare(0, 11, "characters/") || k.find("/color_") == std::string::npos)) return false;
    size_t d = k.find_last_of('.');
    std::string ext = d == std::string::npos ? "" : k.substr(d + 1);
    bool tex = ext == "png" || ext == "atex" || ext == "ctex" || ext == "dbt" || ext == "rhg" || ext == "dds" || ext == "tga" || ext == "bmp";
    return map || !tex || k.find("_dmg") != std::string::npos;
}

static int lb_folder_ranked(const std::string& dir, size_t* n, uint64_t* bytes) {
    std::error_code ec;
    std::filesystem::path root = std::filesystem::u8path(dir);
    int ok = 1;
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        (*n)++; *bytes += (uint64_t)it->file_size(ec);
        if (lb_rank_blocks(std::filesystem::relative(it->path(), root, ec).generic_u8string())) ok = 0;
    }
    return ok;
}

// mod.txt text: the given keys replaced or added, the rest kept
typedef std::vector<std::pair<std::string, std::string>> LbKv;
static std::string lb_txt_merge(std::string in, const LbKv& kv) {
    if (in.size() >= 3 && (uint8_t)in[0] == 0xEF && (uint8_t)in[1] == 0xBB && (uint8_t)in[2] == 0xBF) in.erase(0, 3);
    std::string out;
    std::set<std::string> done;
    for (size_t a = 0; a < in.size();) {
        size_t b = in.find('\n', a);
        std::string l = in.substr(a, b == std::string::npos ? std::string::npos : b - a);
        a = b == std::string::npos ? in.size() : b + 1;
        while (!l.empty() && l.back() == '\r') l.pop_back();
        size_t eq = l.find('=');
        std::string k = eq == std::string::npos ? "" : l.substr(0, eq);
        bool rep = false;
        for (auto& x : kv) if (x.first == k) { if (!done.count(k)) out += k + "=" + x.second + "\n"; done.insert(k); rep = true; }
        if (!rep) out += l + "\n";
    }
    for (auto& x : kv) if (!done.count(x.first)) out += x.first + "=" + x.second + "\n";
    return out;
}

static std::map<std::string, std::string> lb_txt_map(const std::string& in) {
    std::map<std::string, std::string> m;
    for (size_t a = 0; a < in.size();) {
        size_t b = in.find('\n', a);
        std::string l = in.substr(a, b == std::string::npos ? std::string::npos : b - a);
        a = b == std::string::npos ? in.size() : b + 1;
        if (m.empty() && l.size() >= 3 && (uint8_t)l[0] == 0xEF && (uint8_t)l[1] == 0xBB && (uint8_t)l[2] == 0xBF) l.erase(0, 3);
        while (!l.empty() && l.back() == '\r') l.pop_back();
        size_t eq = l.find('=');
        if (eq == std::string::npos || l[0] == '#') continue;
        std::string v = l.substr(eq + 1), o;
        for (size_t k = 0; k < v.size(); k++) { if (v[k] == '\\' && k + 1 < v.size() && v[k + 1] == 'n') { o += '\n'; k++; } else o += v[k]; }
        m[l.substr(0, eq)] = o;
    }
    return m;
}

static void lb_mod_txt(const std::string& folder, const LbKv& kv) {
    std::string f = mw_root + "/" + folder + "/mod.txt", in;
    std::vector<uint8_t> d;
    if (FILE* r = mods_fopen(f, "rb")) {
        fseek(r, 0, SEEK_END); long n = ftell(r); fseek(r, 0, SEEK_SET);
        in.resize(n > 0 ? n : 0);
        if (n > 0 && fread(&in[0], 1, n, r) != (size_t)n) in.clear();
        fclose(r);
    }
    std::string out = lb_txt_merge(in, kv);
    if (FILE* w = mods_fopen(f, "wb")) { fwrite(out.data(), 1, out.size(), w); fclose(w); }
}

// a .mkpak is accepted if the port's installer reads it, every file decompresses with its CRC and none is a program;
// whatever its origin (the port, bt3_hud.py, char_ref.py...), it is written again by the port before uploading
struct LbSrc { bool ok = false; std::wstring err; std::string txt, id; std::vector<uint8_t> img; int ranked = 1; size_t files = 0; uint64_t bytes = 0; };
static LbSrc lb_mkpak_check(const std::string& path) {
    LbSrc r;
    std::string low = path;
    for (auto& c : low) c = (char)tolower((unsigned char)c);
    if (low.size() < 7 || low.compare(low.size() - 6, 6, ".mkpak")) { r.err = L"Only .mkpak files can be published."; return r; }
    std::vector<uint8_t> z, d;
    std::vector<ZipEntry> list;
    std::string root, e;
    auto bad = [&](const std::wstring& msg, const std::string& why) { printf("biblioteca: mkpak rechazado (%s): %s\n", path.c_str(), why.c_str()); r.err = msg; return r; };
    if (!mkpak_read(path, z, list, root, &e)) return bad(L"This file is not a valid .mkpak.", e);
    auto u16 = [&](size_t p) { return (uint32_t)z[p] | ((uint32_t)z[p + 1] << 8); };
    auto u32 = [&](size_t p) { return u16(p) | (u16(p + 2) << 16); };
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> cen;   // local offset -> crc, flags
    size_t n = z.size(), eocd = std::string::npos;
    for (size_t p = n - 22; p + 22 <= n && n - p <= 65557; p--) { if (u32(p) == 0x06054b50) { eocd = p; break; } if (!p) break; }
    if (eocd == std::string::npos) return bad(L"This file is not a valid .mkpak.", "sin final de zip");
    for (size_t p = u32(eocd + 16), i = 0; i < u16(eocd + 10) && p + 46 <= n && u32(p) == 0x02014b50; i++) {
        cen[u32(p + 42)] = {u32(p + 16), u16(p + 8)};
        p += 46 + u16(p + 28) + u16(p + 30) + u16(p + 32);
    }
    static const char* BLOCK[] = {".exe", ".dll", ".bat", ".cmd", ".com", ".scr", ".msi", ".ps1", ".vbs", ".vbe", ".js", ".jse", ".wsf", ".hta", ".lnk", ".jar", ".sys", ".cpl", ".reg", ".py", ".sh"};
    if (list.size() > 65535) return bad(L"This .mkpak has too many files.", "demasiados archivos");
    bool txt = false;
    for (auto& x : list) {
        std::string name = x.name.substr(root.size()), lname = name;
        for (auto& c : lname) c = (char)tolower((unsigned char)c);
        for (const char* b : BLOCK) { size_t bl = strlen(b); if (lname.size() > bl && !lname.compare(lname.size() - bl, bl, b)) return bad(L"This .mkpak contains program files and can't be published.", "programa " + name); }
        auto it = cen.find(x.off);
        if (it == cen.end() || (it->second.second & 1) || (x.method != 0 && x.method != 8)) return bad(L"This .mkpak is damaged or uses an unsupported format.", "entrada " + name);
        if (!zip_extract(z, x, d) || cm_crc(d.data(), d.size()) != it->second.first) return bad(L"This .mkpak is damaged.", "datos de " + name);
        r.bytes += d.size();
        if (r.bytes >= 0x7FF00000ull) return bad(L"The mod is too big (2 GB at most).", "demasiado grande");
        if (lname == "mod.txt") { r.txt.assign(d.begin(), d.end()); txt = true; }
        else if (lname == "imagen.png") r.img = d;
        if (lb_rank_blocks(name)) r.ranked = 0;
    }
    if (!txt) return bad(L"This .mkpak has no mod.txt.", "sin mod.txt");
    r.files = list.size();
    r.id = mkpak_id(path, z, list, root);
    r.ok = true;
    return r;
}

static std::string lb_esc_nl(const std::string& s) { std::string o; for (char c : s) { if (c == '\n') o += "\\n"; else if (c != '\r') o += c; } return o; }

static void lb_pub_fill(const std::map<std::string, std::string>& d, const std::string& fallback) {
    auto f = [&](const char* k) { auto it = d.find(k); return it == d.end() ? std::string() : it->second; };
    std::string n = f("nombre"), v = f("version"), link = f("enlace");
    lb_fset(0, n.empty() ? fallback : n);
    lb_fset(1, v.empty() ? "1.0" : v);
    size_t sp = link.find(' ');
    lb_fset(2, sp == std::string::npos ? link : link.substr(0, sp));
    lb_fset(3, f("descripcion"));
    std::wstring tags = mw_w(mod_tags_en(f("etiquetas"))) + L",";
    for (size_t a = 0, b; (b = tags.find(L',', a)) != std::wstring::npos; a = b + 1) {
        std::wstring t = tags.substr(a, b - a);
        while (!t.empty() && t[0] == L' ') t.erase(0, 1);
        for (int i = 0; i < MOD_NTAGS; i++) if (t == MOD_TAGS[i] && !lb_has_tag(i)) lb_pub.tags.push_back(i);
    }
    lb_pub.lib = f("biblioteca");
}

static void lb_pub_check_lib(const std::string& key);

static void lb_pub_open(const std::string& folder) {
    mw_mods = mods_list(mw_root);
    const ModInfo* m = nullptr;
    for (auto& x : mw_mods) if (x.id == folder) m = &x;
    if (!m) return;
    lb_pub = LbPub();
    lb_pub.mod = folder; lb_pub.login = lb_me.login;
    std::string dir = mw_root + "/" + folder;
    lb_pub_fill(m->data_, mw_name(*m));
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::u8path(dir + "/imagen.png"), ec)) lb_pub.cover = "file:" + dir + "/imagen.png";
    lb_overlay = 4;
    lb_run([folder, dir]() {
        size_t n = 0; uint64_t b = 0;
        int ok = lb_folder_ranked(dir, &n, &b);
        lb_post([=]() { if (lb_pub.mod != folder) return; lb_pub.ranked = ok; lb_pub.files = n; lb_pub.bytes = b; });
    });
    lb_pub_check_lib(folder);
}

// a .mkpak from any folder: checked on a worker thread, then the form is filled from its mod.txt and imagen.png
static void lb_pub_open_file(const std::string& path) {
    if (!lb_me.in) { lb_login_start(); return; }
    lb_pub = LbPub();
    lb_pub.src = path; lb_pub.login = lb_me.login; lb_pub.checking = true;
    for (int i = 0; i < 4; i++) lb_fset(i, "");
    lb_overlay = 4;
    std::string cache = lb_cache_dir() + "/publish";
    lb_run([path, cache]() {
        LbSrc r = lb_mkpak_check(path);
        std::string cover;
        if (r.ok && !r.img.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::u8path(cache), ec);
            cover = cache + "/" + lb_slug(r.id) + "_cover.png";
            if (FILE* f = mods_fopen(cover, "wb")) { fwrite(r.img.data(), 1, r.img.size(), f); fclose(f); } else cover.clear();
            std::lock_guard<std::mutex> g(lb_im);
            lb_imgs.erase("file:" + cover);
        }
        lb_post([=]() {
            if (lb_pub.src != path || lb_overlay != 4) return;
            if (!r.ok) { lb_notice(r.err); lb_pub = LbPub(); lb_overlay = 3; return; }
            printf("biblioteca: mkpak aceptado %s (%u archivos)\n", path.c_str(), (unsigned)r.files);
            lb_pub.txt = r.txt; lb_pub.mod = r.id; lb_pub.ranked = r.ranked; lb_pub.files = r.files; lb_pub.bytes = r.bytes;
            lb_pub.checking = false;
            lb_pub_fill(lb_txt_map(r.txt), r.id);
            if (!cover.empty()) lb_pub.cover = "file:" + cover;
            lb_pub_check_lib(path);
           
        });
    });
   
}

static void lb_pub_check_lib(const std::string& key) {
    if (!lb_pub.lib.empty()) {
        std::string folder = key;
        lb_pub.checking = true;
        std::string lib = lb_pub.lib, url = lb_base() + "/lib/mod/" + lib, auth = lb_auth();
        lb_run([=]() {
            LbResp r = lb_http("GET", url, auth, "");
            lb_post([=]() {
                if ((lb_pub.src.empty() ? lb_pub.mod : lb_pub.src) != folder || lb_pub.lib != lib) return;
                lb_pub.checking = false;
                LbJ j = lbj_parse(r.body);
                if (r.st == 200 && j["mine"].num()) { lb_pub.mode = 1; lb_pub.old_ver = j["ver"].str(); }
                else if (r.st == 200) { lb_pub.blocked = true; lb_notice(L"This mod was installed from the library and belongs to " + mw_w(j["author"].str()) + L"."); }
                else if (r.st == 404) lb_pub.lib.clear();
                else { lb_pub.checking = true; lb_notice(L"Can't reach the mod library. Close this form and try again."); }
            });
        });
    }
}

static void lb_edit_open() {
    lb_pub = LbPub();
    lb_pub.mode = 2; lb_pub.lib = lb_cur.id; lb_pub.cover = lb_cur.cover; lb_pub.shots = lb_cur.shots; lb_pub.tags = lb_cur.tags; lb_pub.ranked = lb_cur.ranked;
    lb_fset(0, lb_cur.name);
    lb_fset(3, lb_cur.descr);
    lb_overlay = 4;
}

static void lb_publish_from(const std::string& folder) {
    if (lb_me.in) { lb_pub_open(folder); return; }
    lb_pending = folder;
    if (!lb_me_checking) { lb_notice(L"Sign in with GitHub to publish."); lb_login_start(); }
}

static std::string lb_pick_image() {
    wchar_t fn[4096] = L"";
    OPENFILENAMEW of; memset(&of, 0, sizeof(of));
    of.lStructSize = sizeof(of); of.hwndOwner = ventana;
    of.lpstrFilter = L"Images (*.png, *.jpg)\0*.png;*.jpg;*.jpeg\0";
    of.lpstrFile = fn; of.nMaxFile = 4096; of.lpstrTitle = L"Choose an image";
    of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&of)) return "";
    std::string p = mw_u8(fn);
    std::error_code ec;
    if (std::filesystem::file_size(std::filesystem::u8path(p), ec) > (10u << 20)) { lb_notice(L"Images can't be bigger than 10 MB."); return ""; }
    return p;
}

// streamed upload (GitHub release assets) with progress
static LbResp lb_upload(const std::string& url, const std::string& hdr, const std::string& path, bool prog) {
    LbResp r;
    FILE* f = mods_fopen(path, "rb");
    if (!f) return r;
    _fseeki64(f, 0, SEEK_END); uint64_t size = (uint64_t)_ftelli64(f); _fseeki64(f, 0, SEEK_SET);
    std::wstring wu = mw_w(url);
    URL_COMPONENTS uc; memset(&uc, 0, sizeof(uc)); uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = (DWORD)-1; uc.dwUrlPathLength = (DWORD)-1; uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(wu.c_str(), 0, 0, &uc) || !uc.lpszHostName) { fclose(f); return r; }
    std::wstring host(uc.lpszHostName, uc.dwHostNameLength), p(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.lpszExtraInfo) p += std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    HINTERNET s = lb_ses();
    HINTERNET con = s ? WinHttpConnect(s, host.c_str(), uc.nPort, 0) : nullptr;
    HINTERNET rq = con ? WinHttpOpenRequest(con, L"POST", p.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (prog) { lb_up_done = 0; lb_up_total = size; lb_up_t0 = GetTickCount64(); }
    if (rq) {
        std::wstring h = mw_w(hdr);
        bool ok = WinHttpSendRequest(rq, h.c_str(), (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, (DWORD)size, 0) != 0;
        std::vector<char> b(1 << 18);
        uint64_t done = 0;
        while (ok && done < size) {
            size_t n = fread(b.data(), 1, b.size(), f);
            if (!n) { ok = false; break; }
            DWORD w = 0;
            if (!WinHttpWriteData(rq, b.data(), (DWORD)n, &w) || w != n) { ok = false; break; }
            done += n;
            if (prog) lb_up_done = done;
        }
        if (ok && WinHttpReceiveResponse(rq, nullptr)) {
            DWORD code = 0, sz = sizeof(code);
            WinHttpQueryHeaders(rq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
            r.st = (int)code;
            for (;;) { DWORD n = 0; if (!WinHttpReadData(rq, b.data(), (DWORD)b.size(), &n) || !n) break; r.body.append(b.data(), n); }
        }
    }
    if (rq) WinHttpCloseHandle(rq);
    if (con) WinHttpCloseHandle(con);
    fclose(f);
    return r;
}

static std::string lb_slug(const std::string& s) {
    std::string o;
    for (unsigned char c : s) o += isalnum(c) || c == '-' || c == '_' || c == '.' ? (char)c : '_';
    return o.empty() ? "mod" : o;
}

static std::string lb_tags_json(const std::vector<int>& t) {
    std::string o = "[";
    for (size_t i = 0; i < t.size(); i++) o += (i ? "," : "") + lbj_esc(LB_TAG_KEY[t[i]]);
    return o + "]";
}

static void lb_pub_go() {
    if (lb_pub.st) return;
    std::string name = lb_fget(0), descr = lb_fget(3);
    if (name.empty()) { lb_notice(L"Type the mod name."); return; }
    std::vector<int> tags;
    for (int t : lb_pub.tags) if (t != 11 || lb_me.admin) tags.push_back(t);
    if (lb_pub.mode == 2) {
        lb_overlay = 0;
        lb_action("POST", "admin/edit/" + lb_pub.lib, "{\"name\":" + lbj_esc(name) + ",\"descr\":" + lbj_esc(descr) + ",\"tags\":" + lb_tags_json(tags) + ",\"ranked\":" + (lb_pub.ranked ? "1" : "0") + "}", L"Saved.", false);
        return;
    }
    if (lb_pub.blocked) { lb_notice(L"Only the author can publish this mod."); return; }
    std::string ver = lb_fget(1), link = lb_fget(2);
    bool vok = !ver.empty() && ver.size() <= 16;
    for (char c : ver) if (!isalnum((unsigned char)c) && c != '.' && c != '-' && c != '_' && c != ' ') vok = false;
    if (!vok) { lb_notice(L"Type a version with letters, numbers and dots (for example 1.0)."); return; }
    if (lb_pub.mode == 1 && ver == lb_pub.old_ver) { lb_notice(L"Change the version to publish an update (now v" + mw_w(ver) + L")."); return; }
    if (!link.empty() && link.rfind("http://", 0) && link.rfind("https://", 0)) { lb_notice(L"The link must start with http:// or https://"); return; }
    if (lb_pub.ranked < 0 || lb_pub.checking) { lb_notice(L"Still checking the mod, try again in a moment."); return; }
    std::string token;
    if (!lb_cred_get(LB_CRED_GH, token)) { lb_notice(L"Sign in with GitHub again to publish."); lb_pending = lb_pub.mod; lb_login_start(); return; }
    std::string etq;
    for (int t : tags) if (t < MOD_NTAGS) etq += (etq.empty() ? "" : ", ") + mw_u8(MOD_TAGS[t]);
    LbKv kv = {{"id", lb_pub.mod}, {"nombre", name}, {"version", ver}, {"enlace", link}, {"etiquetas", etq}, {"descripcion", lb_esc_nl(descr)}};
    std::string src = lb_pub.src, modtxt;
    if (src.empty()) lb_mod_txt(lb_pub.mod, kv);
    else modtxt = lb_txt_merge(lb_pub.txt, kv);
    lb_pub.st = 1; lb_pub.step = L"Packing the mod...";
    lb_up_done = 0; lb_up_total = 0;
    std::string folder = lb_pub.mod, dir = mw_root + "/" + folder, cache = lb_cache_dir() + "/publish", base = lb_base(), auth = lb_auth(), lib = lb_pub.mode == 1 ? lb_pub.lib : "";
    std::string cover = lb_pub.cover;
    std::vector<std::string> shots = lb_pub.shots;
    int ranked = lb_pub.ranked;
    printf("biblioteca: publicando %s v%s (%s)\n", folder.c_str(), ver.c_str(), lib.empty() ? "nuevo" : "actualizacion");
    lb_run([=]() {
        auto step = [](const std::wstring& t) { lb_post([=]() { lb_pub.step = t; }); };
        auto fail = [](const std::wstring& why, const std::string& log) {
            printf("biblioteca: publicar fallo: %s\n", log.c_str());
            lb_post([=]() { lb_pub.st = 0; lb_notice(why); });
        };
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::u8path(cache), ec);
        std::string tmp = cache + "/" + folder + ".mkpak", perr;
        size_t nf = 0; uint64_t nb = 0;
        if (src.empty() ? !mw_pack(dir, tmp, &nf, &nb, &perr) : !mw_repack(src, tmp, modtxt, &perr)) { fail(perr == "big" ? L"The mod is too big (2 GB or 65535 files at most)." : perr == "empty" ? L"The mod folder is empty." : L"Could not pack the mod.", "empaquetar " + perr); return; }
        LbSha h;
        uint64_t size = 0;
        if (FILE* f = mods_fopen(tmp, "rb")) {
            std::vector<char> b(1 << 20);
            for (size_t n; (n = fread(b.data(), 1, b.size(), f)) > 0;) { BCryptHashData(h.h, (PUCHAR)b.data(), (ULONG)n, 0); size += n; }
            fclose(f);
        }
        std::string sha = h.hex();
        const std::string GH = "Authorization: Bearer " + token + "\r\nAccept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        auto gh = [&](const char* m, const std::string& path, const std::string& body) { return lb_http(m, "https://api.github.com" + path, GH + (body.empty() ? "" : "Content-Type: application/json\r\n"), body); };
        auto drop = [&]() { std::error_code e2; std::filesystem::remove(std::filesystem::u8path(tmp), e2); };
        step(L"Preparing your GitHub repository...");
        LbResp me = gh("GET", "/user", "");
        std::string login = lbj_parse(me.body)["login"].str();
        if (me.st == 401) { drop(); lb_post([]() { lb_cred_del(LB_CRED_GH); }); fail(L"GitHub closed the session. Sign out and sign in again.", "token 401"); return; }
        if (me.st != 200 || login.empty()) { drop(); fail(L"Can't reach GitHub.", "user " + std::to_string(me.st)); return; }
        const std::string repo = "/repos/" + login + "/tttrecomp-mods";
        LbResp rp = gh("GET", repo, "");
        if (rp.st == 404) {
            LbResp c = gh("POST", "/user/repos", "{\"name\":\"tttrecomp-mods\",\"description\":\"Mods for TTTRecomp\",\"auto_init\":true,\"has_issues\":false,\"has_wiki\":false,\"has_projects\":false}");
            if (c.st != 201) { drop(); fail(L"Could not create the repository tttrecomp-mods on your GitHub.", "repo " + std::to_string(c.st)); return; }
            printf("biblioteca: repositorio creado %s/tttrecomp-mods\n", login.c_str());
            Sleep(2000);
        } else if (rp.st != 200) { drop(); fail(L"Can't reach GitHub.", "repo " + std::to_string(rp.st)); return; }
        step(L"Creating the release...");
        std::string tag = lb_slug(folder) + "-v" + lb_slug(ver) + "-" + std::to_string((long long)time(nullptr));
        LbResp rel;
        for (int k = 0; k < 4; k++) {
            rel = gh("POST", repo + "/releases", "{\"tag_name\":" + lbj_esc(tag) + ",\"name\":" + lbj_esc(name + " v" + ver) + ",\"body\":\"Published with TTTRecomp.\"}");
            if (rel.st == 201) break;
            Sleep(2000);
        }
        LbJ rj = lbj_parse(rel.body);
        std::string rid = rj["id"].str(), up = rj["upload_url"].str();
        if (rel.st != 201 || rid.empty() || up.empty()) { drop(); fail(L"Could not create the release on GitHub.", "release " + std::to_string(rel.st)); return; }
        up = up.substr(0, up.find('{'));
        auto del = [&]() { gh("DELETE", repo + "/releases/" + rid, ""); };
        auto upload = [&](const std::string& path, const std::string& fname, const char* ctype, bool prog) {
            LbResp u = lb_upload(up + "?name=" + lb_urlenc(fname), GH + "Content-Type: " + ctype + "\r\n", path, prog);
            std::string got = u.st == 201 ? lbj_parse(u.body)["browser_download_url"].str() : "";
            if (got.empty()) printf("biblioteca: subida de %s fallo (codigo %d)\n", fname.c_str(), u.st);
            return got;
        };
        step(L"Uploading the mod...");
        std::string url = upload(tmp, lb_slug(folder) + ".mkpak", "application/octet-stream", true);
        drop();
        if (url.empty()) { del(); fail(L"The upload to GitHub failed. Try again.", "subida"); return; }
        auto ext = [](const std::string& p) { std::string e = p.substr(p.find_last_of('.') + 1); for (auto& c : e) c = (char)tolower((unsigned char)c); return e == "jpeg" ? std::string("jpg") : e; };
        std::string cover_url = cover.rfind("https://", 0) == 0 ? cover : "";
        if (cover.rfind("file:", 0) == 0) {
            step(L"Uploading the images...");
            std::string e = ext(cover);
            cover_url = upload(cover.substr(5), "cover." + e, e == "png" ? "image/png" : "image/jpeg", false);
            if (cover_url.empty()) { del(); fail(L"The upload to GitHub failed. Try again.", "portada"); return; }
        }
        std::vector<std::string> shot_urls;
        for (size_t k = 0; k < shots.size(); k++) {
            if (shots[k].rfind("file:", 0) != 0) continue;
            std::string e = ext(shots[k]);
            std::string u = upload(shots[k].substr(5), "shot" + std::to_string(k + 1) + "." + e, e == "png" ? "image/png" : "image/jpeg", false);
            if (u.empty()) { del(); fail(L"The upload to GitHub failed. Try again.", "captura"); return; }
            shot_urls.push_back(u);
        }
        step(L"Publishing in the library...");
        std::string sj = "[";
        for (size_t k = 0; k < shot_urls.size(); k++) sj += (k ? "," : "") + lbj_esc(shot_urls[k]);
        sj += "]";
        std::string body = "{" + std::string(lib.empty() ? "" : "\"id\":" + lbj_esc(lib) + ",") + "\"name\":" + lbj_esc(name) + ",\"ver\":" + lbj_esc(ver) + ",\"tags\":" + lb_tags_json(tags) +
                           ",\"descr\":" + lbj_esc(descr) + ",\"url\":" + lbj_esc(url) + ",\"size\":" + std::to_string(size) + ",\"sha\":" + lbj_esc(sha) + ",\"cover\":" + lbj_esc(cover_url) +
                           ",\"shots\":" + sj + ",\"link\":" + lbj_esc(link) + ",\"ranked\":" + (ranked ? "1" : "0") + "}";
        LbResp p;
        for (int k = 0; k < 3; k++) {
            p = lb_http("POST", base + "/lib/publish", auth + "Content-Type: application/json\r\n", body);
            if (p.st != 400 || p.body.find("remote") == std::string::npos) break;
            Sleep(3000);
        }
        std::string id = lbj_parse(p.body)["id"].str();
        if (p.st != 200 || id.empty()) {
            del();
            fail(p.st == 403 && p.body.find("banned") != std::string::npos ? L"Your account can't publish in the library." : p.st == 429 ? L"Too many publications today. Try again tomorrow." : L"The mod library did not accept the mod.", "servicio " + std::to_string(p.st) + " " + p.body.substr(0, 120));
            return;
        }
        printf("biblioteca: publicado %s como %s (%llu bytes)\n", folder.c_str(), id.c_str(), (unsigned long long)size);
        lb_post([=]() {
            std::error_code e3;
            if (std::filesystem::is_directory(std::filesystem::u8path(mw_root + "/" + folder), e3)) lb_mod_txt(folder, {{"biblioteca", id}, {"biblioteca_version", ver}});
            mw_mods = mods_list(mw_root);
            lb_pub.st = 0; lb_overlay = 0;
            lb_notice(L"Published \"" + mw_w(name) + L"\" v" + mw_w(ver) + L".");
            LbMod m; m.id = id; m.name = name;
            lb_load(false);
            lb_open_detail(m);
        });
    });
}

static void lb_poll() {
    std::deque<std::function<void()>> q;
    { std::lock_guard<std::mutex> g(lb_qm); q.swap(lb_queue); }
    for (auto& f : q) f();
}

