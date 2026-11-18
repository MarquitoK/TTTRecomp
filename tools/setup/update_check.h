#pragma once
// New releases of TTTRecomp on GitHub (setup and launcher): newest tag with a TTTRecomp-Setup-*.exe asset
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <cstdlib>

static const wchar_t* UPD_HOST = L"api.github.com";
static const wchar_t* UPD_PATH = L"/repos/MarquitoK/TTTRecomp/releases?per_page=15";

static bool upd_get(const wchar_t* host, const wchar_t* path, std::string& out, int timeout_ms) {
    HINTERNET s = WinHttpOpen(L"TTTRecomp", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0);
    if (!s) s = WinHttpOpen(L"TTTRecomp", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!s) return false;
    WinHttpSetTimeouts(s, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
    HINTERNET c = WinHttpConnect(s, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE) : nullptr;
    bool ok = r && WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", (DWORD)-1, nullptr, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr);
    DWORD st = 0, sl = sizeof(st);
    if (ok) WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &st, &sl, nullptr);
    ok = ok && st == 200;
    char b[16384]; DWORD n = 0;
    while (ok && WinHttpReadData(r, b, sizeof(b), &n) && n) out.append(b, n);
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return ok && !out.empty();
}

// "key": "value" after position p, before position end
static std::string upd_str(const std::string& j, const char* key, size_t p, size_t end, size_t* at = nullptr) {
    std::string k = std::string("\"") + key + "\"";
    size_t q = j.find(k, p);
    if (q == std::string::npos || q >= end) return "";
    q = j.find(':', q + k.size());
    if (q == std::string::npos) return "";
    q = j.find_first_not_of(" \t\r\n", q + 1);
    if (q == std::string::npos || j[q] != '"') return "";
    size_t e = j.find('"', q + 1);
    if (at) *at = q;
    return e == std::string::npos ? "" : j.substr(q + 1, e - q - 1);
}

// v0.1.0-alpha < v0.1.0-beta < v0.1.0 < v0.1.1-alpha
static int upd_cmp(std::string a, std::string b) {
    auto parse = [](std::string t, int* n, std::string& pre) {
        if (!t.empty() && (t[0] == 'v' || t[0] == 'V')) t.erase(0, 1);
        size_t d = t.find('-');
        pre = d == std::string::npos ? "" : t.substr(d + 1);
        t = t.substr(0, d);
        for (int i = 0; i < 3; i++) { n[i] = atoi(t.c_str()); size_t p = t.find('.'); t = p == std::string::npos ? "" : t.substr(p + 1); }
    };
    int na[3], nb[3]; std::string pa, pb;
    parse(a, na, pa); parse(b, nb, pb);
    for (int i = 0; i < 3; i++) if (na[i] != nb[i]) return na[i] < nb[i] ? -1 : 1;
    if (pa == pb) return 0;
    if (pa.empty()) return 1;
    if (pb.empty()) return -1;
    return pa < pb ? -1 : 1;
}

struct UpdRelease { std::string tag, url, sha; };
// newest release newer than `mine` that carries the setup; false when there is none or GitHub cannot be reached
static bool upd_latest(const std::string& mine, UpdRelease& out, int timeout_ms) {
    std::string j;
    if (!upd_get(UPD_HOST, UPD_PATH, j, timeout_ms)) return false;
    bool found = false;
    for (size_t p = j.find("\"tag_name\""); p != std::string::npos;) {
        size_t next = j.find("\"tag_name\"", p + 10), end = next == std::string::npos ? j.size() : next;
        std::string tag = upd_str(j, "tag_name", p, end);
        bool draft = false;
        for (size_t dr = j.find("\"draft\"", p); dr < end; dr = j.find("\"draft\"", dr + 7)) {
            size_t dv = j.find_first_not_of(" \t\r\n", dr + 7);
            if (dv == std::string::npos || j[dv] != ':') continue;
            dv = j.find_first_not_of(" \t\r\n", dv + 1);
            draft = dv != std::string::npos && j.compare(dv, 4, "true") == 0;
            break;
        }
        for (size_t q = j.find("\"name\"", p); !draft && q < end; q = j.find("\"name\"", q + 6)) {
            std::string name = upd_str(j, "name", q, end);
            if (name.compare(0, 16, "TTTRecomp-Setup-") || name.size() < 4 || name.compare(name.size() - 4, 4, ".exe")) continue;
            size_t nx = j.find("\"name\"", q + 6), lim = nx == std::string::npos || nx > end ? end : nx;
            std::string url = upd_str(j, "browser_download_url", q, end), dg = upd_str(j, "digest", q, lim);
            if (url.empty()) break;
            if (upd_cmp(tag, mine) > 0 && (!found || upd_cmp(tag, out.tag) > 0)) { out.tag = tag; out.url = url; out.sha = dg.compare(0, 7, "sha256:") ? "" : dg.substr(7); found = true; }
            break;
        }
        p = next;
    }
    return found;
}
