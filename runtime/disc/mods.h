#pragma once
// mods/<id>/ = mod.txt, imagen.png, Packfile/..., Shared/..., Roster/ (characters and cells, item 29.3); mods/mods.cfg: "+ id" enabled, "- id" disabled, first line = highest priority
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <thread>
#include <algorithm>
#include "gpu/image.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

struct ModInfo { std::string id; bool enabled = true; std::map<std::string, std::string> data_; };

static std::string mods_root_of(const std::string& assets) {
    const char* e = getenv("TTT_MODS");
    if (e && *e) return e;
    return assets + "/../mods";
}

static void mod_read_txt(const std::string& root_d, ModInfo& m) {
    m.data_.clear();
    FILE* f = fopen((root_d + "/" + m.id + "/mod.txt").c_str(), "rb");
    if (!f) return;
    char line[4096];
    bool first_one = true;
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        if (first_one && (uint8_t)p[0] == 0xEF && (uint8_t)p[1] == 0xBB && (uint8_t)p[2] == 0xBF) p += 3;
        first_one = false;
        size_t n = strlen(p);
        while (n && (p[n - 1] == '\n' || p[n - 1] == '\r')) p[--n] = 0;
        char* igual = strchr(p, '=');
        if (!igual || p[0] == '#') continue;
        *igual = 0;
        std::string v = igual + 1, s;
        for (size_t k = 0; k < v.size(); k++) {
            if (v[k] == '\\' && k + 1 < v.size() && v[k + 1] == 'n') { s += '\n'; k++; }
            else s += v[k];
        }
        m.data_[p] = s;
    }
    fclose(f);
}

static void mods_save(const std::string& root_d, const std::vector<ModInfo>& list) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(root_d), ec);
    FILE* f = fopen((root_d + "/mods.cfg").c_str(), "w");
    if (!f) return;
    for (auto& m : list) fprintf(f, "%c %s\n", m.enabled ? '+' : '-', m.id.c_str());
    fclose(f);
}

// order and state from mods.cfg; new folders are appended enabled; missing ones are removed
static std::vector<ModInfo> mods_list(const std::string& root_d) {
    std::vector<ModInfo> list;
    std::set<std::string> seen, folders;
    std::error_code ec;
    for (auto& p : std::filesystem::directory_iterator(std::filesystem::u8path(root_d), ec))
        if (p.is_directory(ec)) folders.insert(p.path().filename().u8string());
    bool cambio = false;
    if (FILE* f = fopen((root_d + "/mods.cfg").c_str(), "r")) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            size_t n = strlen(line);
            while (n && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ')) line[--n] = 0;
            if (n < 3 || (line[0] != '+' && line[0] != '-') || line[1] != ' ') continue;
            ModInfo m; m.id = line + 2; m.enabled = line[0] == '+';
            if (!folders.count(m.id) || !seen.insert(m.id).second) { cambio = true; continue; }
            list.push_back(m);
        }
        fclose(f);
    } else cambio = !folders.empty();
    for (auto& c : folders) if (!seen.count(c)) { ModInfo m; m.id = c; list.push_back(m); cambio = true; }
    for (auto& m : list) mod_read_txt(root_d, m);
    if (cambio) mods_save(root_d, list);
    return list;
}

static const int MOD_NTAGS = 11;
static const wchar_t* MOD_TAGS[MOD_NTAGS] = {L"Textures", L"Audio", L"Characters", L"Roster", L"Maps", L"Menus", L"Attacks", L"Models", L"Story Mode", L"Logic", L"Others"};
// tags written by older versions of the mod creator, shown in English
static std::string mod_tags_en(const std::string& t) {
    static const char* OLD[10][2] = {{"Texturas", "Textures"}, {"Audios", "Audio"}, {"Personajes", "Characters"}, {"Mapas", "Maps"}, {"Men\xC3\xBAs", "Menus"},
                                     {"Ataques", "Attacks"}, {"Modelos", "Models"}, {"Modo Historia", "Story Mode"}, {"L\xC3\xB3gica", "Logic"}, {"Otros", "Others"}};
    std::string out, w;
    auto flush = [&]() {
        size_t a = w.find_first_not_of(' '), b = w.find_last_not_of(' ');
        std::string x = a == std::string::npos ? "" : w.substr(a, b - a + 1);
        for (auto& o : OLD) if (x == o[0]) x = o[1];
        if (!x.empty()) out += (out.empty() ? "" : ", ") + x;
        w.clear();
    };
    for (char c : t) { if (c == ',') flush(); else w += c; }
    flush();
    return out;
}
inline std::string mods_root_global;

static FILE* mods_fopen(const std::string& fpath, const char* modo) {
#ifdef _WIN32
    int n = MultiByteToWideChar(CP_UTF8, 0, fpath.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n : 1, 0), m(modo, modo + strlen(modo));
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, fpath.c_str(), -1, &w[0], n);
    return _wfopen(w.c_str(), m.c_str());
#else
    return fopen(fpath.c_str(), modo);
#endif
}

static std::string mod_clean_id(const std::string& s) {
    std::string r;
    for (unsigned char c : s) r += (c < 32 || strchr("<>:\"/\\|?*", c)) ? '_' : (char)c;
    while (!r.empty() && (r.back() == ' ' || r.back() == '.')) r.pop_back();
    return r.empty() ? "mod" : r;
}

// ---------------- zip reader (.mkpak) ----------------
struct ZipEntry { std::string name; uint32_t method, comp, sz, off; };

static bool zip_read_index(const std::vector<uint8_t>& z, std::vector<ZipEntry>& list, std::string* error) {
    auto u16 = [&](size_t p) { return (uint32_t)z[p] | ((uint32_t)z[p + 1] << 8); };
    auto u32 = [&](size_t p) { return u16(p) | (u16(p + 2) << 16); };
    size_t n = z.size(), eocd = std::string::npos;
    for (size_t p = n >= 22 ? n - 22 : 0; p + 22 <= n && n - p <= 65557; p--) {
        if (u32(p) == 0x06054b50) { eocd = p; break; }
        if (!p) break;
    }
    if (eocd == std::string::npos) { if (error) *error = "no es un archivo zip"; return false; }
    uint32_t count_n = u16(eocd + 10), cd = u32(eocd + 16);
    size_t p = cd;
    for (uint32_t i = 0; i < count_n; i++) {
        if (p + 46 > n || u32(p) != 0x02014b50) { if (error) *error = "indice zip danado"; return false; }
        ZipEntry e;
        e.method = u16(p + 10); e.comp = u32(p + 20); e.sz = u32(p + 24); e.off = u32(p + 42);
        uint32_t ln = u16(p + 28), le = u16(p + 30), lc = u16(p + 32);
        if (p + 46 + ln > n) return false;
        e.name.assign((const char*)&z[p + 46], ln);
        for (auto& c : e.name) if (c == '\\') c = '/';
        p += 46 + ln + le + lc;
        if (e.name.empty() || e.name.back() == '/') continue;
        if (e.name[0] == '/' || e.name.find("../") != std::string::npos || e.name.find(':') != std::string::npos) continue;
        list.push_back(e);
    }
    return true;
}

static bool zip_extract(const std::vector<uint8_t>& z, const ZipEntry& e, std::vector<uint8_t>& d) {
    auto u16 = [&](size_t p) { return (uint32_t)z[p] | ((uint32_t)z[p + 1] << 8); };
    size_t p = e.off;
    if (p + 30 > z.size() || (u16(p) | (u16(p + 2) << 16)) != 0x04034b50) return false;
    p += 30 + u16(p + 26) + u16(p + 28);
    if (p + e.comp > z.size()) return false;
    d.clear();
    if (e.method == 0) { d.assign(z.begin() + p, z.begin() + p + e.comp); return true; }
    if (e.method != 8) return false;
    d.reserve(e.sz);
    Inflater inf(z.data() + p, e.comp, d);
    return inf.run(false) && d.size() == e.sz;
}

// a mkpak may wrap everything in a root folder: removed when every entry shares it
// content folders a mod can carry at its top: Packfile, Shared, Roster, Lang and any HUD mod folder (BT3Hud, SZHud...)
static bool mod_top_dir(const std::string& d) {
    return d == "Packfile/" || d == "Shared/" || d == "Roster/" || d == "Lang/" || (d.size() > 4 && d.compare(d.size() - 4, 4, "Hud/") == 0);
}

static std::string zip_common_root(const std::vector<ZipEntry>& list) {
    std::string root_d;
    for (auto& e : list) {
        size_t s = e.name.find('/');
        if (s == std::string::npos) return "";
        std::string r = e.name.substr(0, s + 1);
        if (root_d.empty()) root_d = r; else if (r != root_d) return "";
    }
    return mod_top_dir(root_d) ? "" : root_d;
}

static bool mkpak_read(const std::string& fpath, std::vector<uint8_t>& z, std::vector<ZipEntry>& list, std::string& root_d, std::string* error) {
    FILE* f = mods_fopen(fpath, "rb");
    if (!f) { if (error) *error = "no puedo abrir el archivo"; return false; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    z.resize(n > 0 ? n : 0);
    bool ok = n > 0 && fread(z.data(), 1, n, f) == (size_t)n;
    fclose(f);
    if (!ok || !zip_read_index(z, list, error)) return false;
    root_d = zip_common_root(list);
    bool packfile = false;
    for (auto& e : list) {
        size_t s = e.name.find('/', root_d.size());
        if (s != std::string::npos && mod_top_dir(e.name.substr(root_d.size(), s + 1 - root_d.size()))) { packfile = true; break; }
    }
    if (!packfile) { if (error) *error = "el archivo no trae una carpeta Packfile, Shared, Roster, Lang ni de HUD (BT3Hud, SZHud)"; return false; }
    return true;
}

// mod id: the id= field of mod.txt or the file name
static std::string mkpak_id(const std::string& fpath, const std::vector<uint8_t>& z, const std::vector<ZipEntry>& list, const std::string& root_d) {
    for (auto& e : list) {
        if (e.name != root_d + "mod.txt") continue;
        std::vector<uint8_t> d;
        if (!zip_extract(z, e, d)) break;
        std::string t(d.begin(), d.end());
        size_t p = t.find("id=");
        if (p != std::string::npos && (p == 0 || t[p - 1] == '\n')) {
            size_t end_pos = t.find_first_of("\r\n", p);
            std::string id = t.substr(p + 3, end_pos == std::string::npos ? std::string::npos : end_pos - p - 3);
            if (!id.empty()) return mod_clean_id(id);
        }
        break;
    }
    size_t s = fpath.find_last_of("/\\");
    std::string b = s == std::string::npos ? fpath : fpath.substr(s + 1);
    size_t pt = b.find_last_of('.');
    if (pt != std::string::npos && pt) b.resize(pt);
    return mod_clean_id(b);
}

// installs (or updates) the mkpak in root/<id>; keeps state and position if it was there
static bool mod_install(const std::string& root_d, const std::string& mkpak, std::string* out_id, std::string* error) {
    std::vector<uint8_t> z, d;
    std::vector<ZipEntry> list;
    std::string zroot;
    if (!mkpak_read(mkpak, z, list, zroot, error)) return false;
    std::string id = mkpak_id(mkpak, z, list, zroot), target = root_d + "/" + id;
    std::error_code ec;
    std::filesystem::path pd = std::filesystem::u8path(target);
    if (std::filesystem::exists(pd, ec)) std::filesystem::remove_all(pd, ec);
    std::filesystem::create_directories(pd, ec);
    std::set<std::string> dirs;
    for (auto& e : list) {
        std::string rel = e.name.substr(zroot.size());
        if (!rel.empty()) dirs.insert((pd / std::filesystem::u8path(rel)).parent_path().u8string());
    }
    for (auto& x : dirs) std::filesystem::create_directories(std::filesystem::u8path(x), ec);
    // files written by several threads: on Windows each new file costs more than its bytes
    std::atomic<size_t> next(0);
    std::atomic<int> n(0);
    std::atomic<bool> bad(false);
    std::mutex emx;
    std::string why;
    auto work = [&]() {
        std::vector<uint8_t> dd;
        for (size_t k; !bad && (k = next++) < list.size();) {
            std::string rel = list[k].name.substr(zroot.size());
            if (rel.empty()) continue;
            if (!zip_extract(z, list[k], dd)) { std::lock_guard<std::mutex> g(emx); bad = true; why = "no puedo descomprimir " + rel; break; }
            FILE* f = mods_fopen(target + "/" + rel, "wb");
            if (!f) { std::lock_guard<std::mutex> g(emx); bad = true; why = "no puedo escribir " + rel; break; }
            fwrite(dd.data(), 1, dd.size(), f);
            fclose(f);
            n++;
        }
    };
    unsigned nt = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < nt; t++) pool.emplace_back(work);
    for (auto& t : pool) t.join();
    if (bad) { if (error) *error = why; std::filesystem::remove_all(pd, ec); return false; }
    printf("mods: instalado %s, %d archivos\n", id.c_str(), n.load());
    if (out_id) *out_id = id;
    return true;
}

static void mod_uninstall(const std::string& root_d, std::vector<ModInfo>& list, size_t k) {
    if (k >= list.size()) return;
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::u8path(root_d + "/" + list[k].id), ec);
    printf("mods: quitado %s\n", list[k].id.c_str());
    list.erase(list.begin() + k);
    mods_save(root_d, list);
}
