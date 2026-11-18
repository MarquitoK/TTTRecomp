#pragma once
// Mods: install helpers shared by the settings menu, the library and the mod creator. Included from menu.h
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <functional>
#include "disc/mods.h"

static std::vector<ModInfo> mw_mods;
static std::string mw_root;

// Discord while a tool window is open (the game's own presence waits: psp_ui_activity)
void net_discord_presence(const char* details, const char* state, const char* room, int n, int max);
static int ui_act = 0;
int psp_ui_activity() { return ui_act; }
static void ui_activity(int a) {
    if (a == ui_act) return;
    ui_act = a;
    if (!a) return;
    static const char* S[9][2] = {{"", ""}, {"Exploring the mod library", "Explorando la biblioteca de mods"}, {"Publishing a mod", "Publicando un mod"},
                                  {"Creating a mod", "Creando un mod"}, {"Editing characters", "Editando personajes"}, {"Editing renders", "Editando renders"},
                                  {"Editing palettes", "Editando paletas"}, {"Organizing mods", "Organizando mods"}, {"Editing map renders", "Editando renders de mapas"}};
    int li = language == 3 ? 1 : 0;
    net_discord_presence("Modding", S[a][li], "", 0, 0);
}

static std::wstring mw_w(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    if (n) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static std::string mw_u8(const wchar_t* w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, 0);
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::string mw_field(const ModInfo& m, const char* k) { auto it = m.data_.find(k); return it == m.data_.end() ? "" : it->second; }
static std::string mw_name(const ModInfo& m) { std::string n = mw_field(m, "nombre"); return n.empty() ? m.id : n; }

#include "ui/library_ui.h"

// png scaled into the box (real pixels), alpha composited over the field color
static HBITMAP mods_png_bitmap(const std::string& fpath, int W, int H) {
    std::vector<uint32_t> px; int w = 0, h = 0;
    std::vector<uint8_t> f; std::string err;
    if (FILE* fp = mods_fopen(fpath, "rb")) {
        fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
        f.resize(n > 0 ? n : 0);
        if (n > 0 && fread(f.data(), 1, n, fp) != (size_t)n) f.clear();
        fclose(fp);
    }
    if (!f.empty() && !png_decode(f, &w, &h, px, &err)) { w = h = 0; px.clear(); }
    BITMAPINFO bi; memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) return nullptr;
    uint32_t* d = (uint32_t*)bits;
    uint32_t fr = GetRValue(THEME_FIELD), fg = GetGValue(THEME_FIELD), fb = GetBValue(THEME_FIELD);
    for (int i = 0; i < W * H; i++) d[i] = (fr << 16) | (fg << 8) | fb;
    if (w > 0 && h > 0) {
        double scale = std::min((double)W / w, (double)H / h);
        int dw = std::max(1, (int)(w * scale)), dh = std::max(1, (int)(h * scale)), x0 = (W - dw) / 2, y0 = (H - dh) / 2;
        for (int y = 0; y < dh; y++) {
            int sy0 = y * h / dh, sy1 = std::max(sy0 + 1, (y + 1) * h / dh);
            for (int x = 0; x < dw; x++) {
                int sx0 = x * w / dw, sx1 = std::max(sx0 + 1, (x + 1) * w / dw);
                uint64_t r = 0, g = 0, b = 0, a = 0, n = 0;
                for (int sy = sy0; sy < sy1; sy++) for (int sx = sx0; sx < sx1; sx++) {
                    uint32_t p = px[(size_t)sy * w + sx], pa = p >> 24;
                    r += (p & 255) * pa; g += ((p >> 8) & 255) * pa; b += ((p >> 16) & 255) * pa; a += pa; n++;
                }
                uint32_t A = (uint32_t)(a / n), R = a ? (uint32_t)(r / a) : 0, G = a ? (uint32_t)(g / a) : 0, B = a ? (uint32_t)(b / a) : 0;
                R = (R * A + fr * (255 - A)) / 255; G = (G * A + fg * (255 - A)) / 255; B = (B * A + fb * (255 - A)) / 255;
                d[(y0 + y) * W + x0 + x] = (R << 16) | (G << 8) | B;
            }
        }
    }
    return bmp;
}

static bool pm_run(const std::wstring& args, std::string& output);
static std::wstring pm_error(const std::string& s);
static std::wstring pm_quote(const std::string& s);

static bool mw_closed_ext(const std::string& name) {
    size_t d = name.find_last_of("./\\");
    if (d == std::string::npos || name[d] != '.') return false;
    std::string x = name.substr(d);
    for (auto& c : x) c = (char)tolower((unsigned char)c);
    return x == ".pak" || x == ".txt1" || x == ".vagp" || x == ".atex" || x == ".rhg" || x == ".unk";
}

// .pak, loose containers or AT3 inside the mod: opened as in assets
static bool mw_closed(const std::string& dir) {
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(std::filesystem::u8path(dir + "/Packfile"), ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        if (mw_closed_ext(it->path().filename().u8string())) return true;
    }
    return false;
}

// after the files are in mods/<id>: closed .pak files opened and the list read again
static void mw_after_install(const std::string& id, double t_inst = 0) {
    auto secs = [](std::chrono::steady_clock::time_point a) { return std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count(); };
    std::wstring failed;
    std::string dir = mw_root + "/" + id, out;
    auto t1 = std::chrono::steady_clock::now();
    bool closed = mw_closed(dir);
    double t_scan = secs(t1);
    t1 = std::chrono::steady_clock::now();
    if (closed) {
        if (!pm_run(L"--mod " + pm_quote(dir), out)) failed = L"Installed, but its .pak files stayed closed: " + pm_error(out);
        size_t p = 0;
        while (p < out.size()) {
            size_t f = out.find('\n', p);
            std::string l = out.substr(p, f == std::string::npos ? std::string::npos : f - p);
            while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
            if (!l.empty()) printf("mods: %s: %s\n", id.c_str(), l.c_str());
            p = f == std::string::npos ? out.size() : f + 1;
        }
    }
    printf("mods: tiempos al instalar %s: instalar %.1f s, revisar %.1f s, abrir %.1f s%s\n", id.c_str(), t_inst, t_scan, secs(t1), closed ? "" : " (nada que abrir)");
    mw_mods = mods_list(mw_root);
    if (!failed.empty()) MessageBoxW(ventana, failed.c_str(), L"Mods", MB_OK | MB_ICONWARNING);
}

static std::string mw_install(const std::string& fpath) {
    std::string id, err;
    auto t0 = std::chrono::steady_clock::now();
    if (!mod_install(mw_root, fpath, &id, &err)) {
        MessageBoxW(ventana, (L"Could not install:\n" + mw_w(fpath) + L"\n\n" + mw_w(err)).c_str(), L"Mods", MB_OK | MB_ICONWARNING);
        return "";
    }
    mw_after_install(id, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    return id;
}

#include "ui/mod_creator.h"
