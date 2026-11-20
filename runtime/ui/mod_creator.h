#pragma once
// Create mod window: Packfile and Shared folders or files (dropped or chosen), image, details and tags; writes the .mkpak
#include <shobjidl.h>
#include <shlobj.h>

enum { IDC_NAME = 3000, IDC_VERSION, IDC_AUTHOR, IDC_LINK, IDC_DESC, IDC_IMAGE, IDC_PICK_IMG, IDC_REMOVE_IMG, IDC_LIST, IDC_ADD, IDC_REMOVE,
       IDC_CREATE, IDC_CLOSE, IDC_NOTICE, IDC_TITLE, IDC_SUB, IDC_HINT, IDC_LABEL = 3100, IDC_TAB0 = 3120, IDC_TAB1, IDC_TAB2, IDC_TAB3, IDC_SAVE = 3130 };
struct CmContenido { std::string base, rel, top; bool dir; };
static HWND cm = nullptr, cm_list = nullptr, cm_image = nullptr, cm_notice = nullptr, cm_label[MOD_NTAGS];
static HBITMAP cm_bmp = nullptr;
static std::vector<CmContenido> cm_content;
static std::string cm_img;
static std::vector<std::pair<std::string, std::string>> cm_extra;   // mod.txt keys the creator does not edit (voces, codigo, parches...)
static const int CM_IMG_W = 464, CM_IMG_H = 261;
static const int CM_DY = 40, CM_W[3] = {1280, 1280, 1280}, CM_H[3] = {720, 720, 720}, CM_RAIL = 168;
static const int CM_FIELDS[5][4] = {{180, 30, 408, 24}, {180, 66, 120, 24}, {180, 102, 408, 24}, {180, 138, 408, 24}, {180, 174, 408, 110}};   // x relative to the page (the rail is added); rows end at y 60, 96, 132, 168, 292
static int cm_tab = 0, cm_mk = -1, cm_dx = 0, cm_dy = 0;
static std::vector<HWND> cm_page[3];

static HWND cm_ctl(const wchar_t* cls, const wchar_t* txt, DWORD st, int x, int y, int w, int h, int id, HFONT f = nullptr) {
    HWND c = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | st, E(x + cm_dx), E(y + cm_dy), E(w), E(h), cm, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)(f ? f : theme_f_normal), TRUE);
    if (cm_mk >= 0) cm_page[cm_mk].push_back(c);
    return c;
}

static void cm_notify(const std::wstring& t) { SetWindowTextW(cm_notice, t.c_str()); }

static std::string cm_text(int id) {
    wchar_t t[8192] = L"";
    GetWindowTextW(GetDlgItem(cm, id), t, 8192);
    return mw_u8(t);
}

static void cm_image_set() {
    if (cm_bmp) { DeleteObject(cm_bmp); cm_bmp = nullptr; }
    cm_bmp = mods_png_bitmap(cm_img, E(CM_IMG_W), E(CM_IMG_H));
    SendMessageW(cm_image, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)cm_bmp);
    EnableWindow(GetDlgItem(cm, IDC_REMOVE_IMG), !cm_img.empty());
}

// path inside Packfile or Shared: what follows "/Packfile/" or "/Shared/" in the real path (the later one wins);
// the folder itself is a root; a folder holding Packfile and/or Shared (assets, a mod) gives both roots
static bool cm_relative(const std::string& real, std::string& rel, std::string& base, std::string& top) {
    std::string r = real;
    for (auto& c : r) if (c == '\\') c = '/';
    while (!r.empty() && r.back() == '/') r.pop_back();
    std::string lower = r;
    for (auto& c : lower) c = (char)tolower((unsigned char)c);
    size_t pp = lower.rfind("/packfile/"), ps = lower.rfind("/shared/"), pr = lower.rfind("/roster/");
    if (pr != std::string::npos && (pp == std::string::npos || pr > pp) && (ps == std::string::npos || pr > ps)) { rel = r.substr(pr + 8); base = r; top = "Roster"; return true; }
    if (pp != std::string::npos && (ps == std::string::npos || pp > ps)) { rel = r.substr(pp + 10); base = r; top = "Packfile"; return true; }
    if (ps != std::string::npos) { rel = r.substr(ps + 8); base = r; top = "Shared"; return true; }
    size_t s = r.find_last_of('/');
    std::string last = lower.substr(s == std::string::npos ? 0 : s + 1);
    if (last == "packfile" || last == "shared" || last == "roster") { rel = ""; base = r; top = last == "packfile" ? "Packfile" : last == "shared" ? "Shared" : "Roster"; return true; }
    return false;
}

static void cm_fill() {
    SendMessageW(cm_list, LB_RESETCONTENT, 0, 0);
    for (auto& c : cm_content) SendMessageW(cm_list, LB_ADDSTRING, 0, (LPARAM)mw_w(c.top + (c.rel.empty() ? "" : "/" + c.rel) + (c.dir ? "/" : "")).c_str());
    EnableWindow(GetDlgItem(cm, IDC_REMOVE), !cm_content.empty());
}

static bool cm_is_png(const std::string& fpath) {
    FILE* f = mods_fopen(fpath, "rb");
    if (!f) return false;
    uint8_t b[8]; bool ok = fread(b, 1, 8, f) == 8 && !memcmp(b, "\x89PNG\r\n\x1a\n", 8);
    fclose(f);
    return ok;
}

static void cm_push(const std::string& base, const std::string& rel, const std::string& top, bool dir) {
    for (auto& c : cm_content) if (c.top == top && c.rel == rel) return;
    CmContenido c; c.base = base; c.rel = rel; c.top = top; c.dir = dir;
    cm_content.push_back(c);
}

static void cm_add(const std::string& fpath) {
    std::error_code ec;
    bool dir = std::filesystem::is_directory(std::filesystem::u8path(fpath), ec);
    std::string rel, base, top;
    bool inside = cm_relative(fpath, rel, base, top);
    if (!dir && !inside) {
        if (cm_is_png(fpath)) { cm_img = fpath; cm_image_set(); cm_notify(L""); }
        else cm_notify(L"Only Packfile, Shared or Roster folders or files, and one PNG image");
        return;
    }
    if (inside) cm_push(base, rel, top, dir);
    else {
        std::string r = fpath;
        while (!r.empty() && (r.back() == '/' || r.back() == '\\')) r.pop_back();
        bool any = false;
        for (const char* t : {"Packfile", "Shared", "Roster"})
            if (std::filesystem::is_directory(std::filesystem::u8path(r + "/" + t), ec)) { cm_push(r + "/" + t, "", t, true); any = true; }
        if (!any) { cm_notify(L"The folder must be Packfile, Shared or Roster, contain them, or be inside one"); return; }
    }
    cm_fill();
    cm_notify(L"");
}

static void cm_pick_folder() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void**)&d))) return;
    DWORD op = 0; d->GetOptions(&op);
    d->SetOptions(op | FOS_PICKFOLDERS | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    d->SetTitle(L"Choose folders inside assets\\Packfile or assets\\Shared (or a mod's Packfile/Shared)");
    IShellItemArray* items = nullptr;
    if (SUCCEEDED(d->Show(cm)) && SUCCEEDED(d->GetResults(&items))) {
        DWORD n = 0; items->GetCount(&n);
        for (DWORD i = 0; i < n; i++) {
            IShellItem* it = nullptr;
            if (FAILED(items->GetItemAt(i, &it))) continue;
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { cm_add(mw_u8(p)); CoTaskMemFree(p); }
            it->Release();
        }
        items->Release();
    }
    d->Release();
}

static void cm_pick_image() {
    wchar_t file_name[4096] = L"";
    OPENFILENAMEW of; memset(&of, 0, sizeof(of));
    of.lStructSize = sizeof(of); of.hwndOwner = cm;
    of.lpstrFilter = L"PNG image (*.png)\0*.png\0";
    of.lpstrFile = file_name; of.nMaxFile = 4096; of.lpstrTitle = L"Mod image";
    of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&of)) { std::string r = mw_u8(file_name); if (cm_is_png(r)) { cm_img = r; cm_image_set(); } else cm_notify(L"The file is not a PNG"); }
}

// ---------------- zip writer (stored entries, with CRC32) ----------------
static uint32_t cm_crc(const uint8_t* d, size_t n) {
    static uint32_t table[256];
    if (!table[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = table[(c ^ d[i]) & 255] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

struct CmZip {
    FILE* f = nullptr; std::vector<uint8_t> central; uint32_t n = 0;
    void u16(std::vector<uint8_t>& v, uint32_t x) { v.push_back(x & 255); v.push_back((x >> 8) & 255); }
    void u32(std::vector<uint8_t>& v, uint32_t x) { u16(v, x & 0xFFFF); u16(v, x >> 16); }
    bool entrada(const std::string& nombre, const std::vector<uint8_t>& d) {
        uint32_t off = (uint32_t)ftell(f), crc = cm_crc(d.data(), d.size()), nbytes = (uint32_t)d.size();
        std::vector<uint8_t> h;
        u32(h, 0x04034b50); u16(h, 20); u16(h, 0x800); u16(h, 0); u16(h, 0); u16(h, 0x21); u32(h, crc); u32(h, nbytes); u32(h, nbytes); u16(h, (uint32_t)nombre.size()); u16(h, 0);
        h.insert(h.end(), nombre.begin(), nombre.end());
        if (fwrite(h.data(), 1, h.size(), f) != h.size() || (nbytes && fwrite(d.data(), 1, nbytes, f) != nbytes)) return false;
        u32(central, 0x02014b50); u16(central, 20); u16(central, 20); u16(central, 0x800); u16(central, 0); u16(central, 0); u16(central, 0x21);
        u32(central, crc); u32(central, nbytes); u32(central, nbytes); u16(central, (uint32_t)nombre.size()); u16(central, 0); u16(central, 0); u16(central, 0); u16(central, 0); u32(central, 0); u32(central, off);
        central.insert(central.end(), nombre.begin(), nombre.end());
        n++;
        return true;
    }
    bool finish() {
        uint32_t off = (uint32_t)ftell(f), nbytes = (uint32_t)central.size();
        std::vector<uint8_t> e;
        u32(e, 0x06054b50); u16(e, 0); u16(e, 0); u16(e, n); u16(e, n); u32(e, nbytes); u32(e, off); u16(e, 0);
        bool ok = fwrite(central.data(), 1, nbytes, f) == nbytes && fwrite(e.data(), 1, e.size(), f) == e.size();
        fclose(f); f = nullptr;
        return ok;
    }
};

static bool cm_read(const std::string& fpath, std::vector<uint8_t>& d) {
    FILE* f = mods_fopen(fpath, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    d.resize(n > 0 ? n : 0);
    bool ok = n <= 0 || fread(d.data(), 1, n, f) == (size_t)n;
    fclose(f);
    return ok;
}

static std::string cm_escape(std::string s) {
    std::string r;
    for (char c : s) { if (c == '\r') continue; if (c == '\n') r += "\\n"; else r += c; }
    return r;
}

static void cm_select_tab(int k);
#include "ui/roster_editor.h"
#include "ui/stage_editor.h"

// Mods window: an installed mod folder packed back into a .mkpak (same writer as the mod creator)
// mod folder -> .mkpak (sorted entries, hidden and system files skipped); also used by the library to publish
static bool mw_pack(const std::string& dir, const std::string& dest, size_t* nfiles, uint64_t* nbytes, std::string* err) {
    std::error_code ec;
    std::vector<std::pair<std::string, std::string>> list;
    uint64_t total = 0;
    std::filesystem::path root = std::filesystem::u8path(dir);
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        std::string name = it->path().filename().u8string(), low = name;
        for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
        if (name.empty() || name[0] == '.' || low == "thumbs.db" || low == "desktop.ini") { if (it->is_directory(ec)) it.disable_recursion_pending(); continue; }
        if (!it->is_regular_file(ec)) continue;
        total += (uint64_t)it->file_size(ec);
        list.push_back({fs_rel(it->path(), root), it->path().u8string()});
    }
    if (nfiles) *nfiles = list.size();
    if (nbytes) *nbytes = total;
    if (list.empty()) { if (err) *err = "empty"; return false; }
    if (total >= 0x7FF00000ull || list.size() > 65535) { if (err) *err = "big"; return false; }
    std::sort(list.begin(), list.end());
    CmZip z;
    z.f = mods_fopen(dest, "wb");
    if (!z.f) { if (err) *err = "write"; return false; }
    bool ok = true;
    std::vector<uint8_t> d;
    for (size_t i = 0; i < list.size() && ok; i++) ok = cm_read(list[i].second, d) && z.entrada(list[i].first, d);
    if (ok) ok = z.finish(); else fclose(z.f);
    if (!ok) { std::filesystem::remove(std::filesystem::u8path(dest), ec); if (err) *err = "write"; return false; }
    printf("mods: empaquetado %s, %u archivos, %.1f MB\n", dest.c_str(), (unsigned)list.size(), total / 1048576.0);
    return true;
}

// a checked .mkpak written again with another mod.txt (same writer, same order)
static bool mw_repack(const std::string& src, const std::string& dest, const std::string& modtxt, std::string* err) {
    std::vector<uint8_t> z, d;
    std::vector<ZipEntry> list;
    std::string root;
    if (!mkpak_read(src, z, list, root, err)) return false;
    CmZip w;
    w.f = mods_fopen(dest, "wb");
    if (!w.f) { if (err) *err = "write"; return false; }
    bool ok = true, txt = false;
    for (size_t i = 0; i < list.size() && ok; i++) {
        std::string name = list[i].name.substr(root.size());
        if (name == "mod.txt") { d.assign(modtxt.begin(), modtxt.end()); txt = true; }
        else if (!zip_extract(z, list[i], d)) { ok = false; break; }
        ok = w.entrada(name, d);
    }
    if (ok && !txt) { d.assign(modtxt.begin(), modtxt.end()); ok = w.entrada("mod.txt", d); }
    if (ok) ok = w.finish(); else fclose(w.f);
    std::error_code ec;
    if (!ok) { std::filesystem::remove(std::filesystem::u8path(dest), ec); if (err) *err = "write"; return false; }
    printf("mods: reempaquetado %s, %u archivos\n", dest.c_str(), (unsigned)list.size());
    return true;
}

static void cm_close();
static void cm_create(bool restart = true, bool install = false) {
    std::string nombre = cm_text(IDC_NAME);
    bool roster = re_has_content(), maps = se_has_content();
    if (nombre.empty()) { cm_notify(L"Type the mod name"); if (cm_tab) cm_select_tab(0); SetFocus(GetDlgItem(cm, IDC_NAME)); return; }
    if (cm_content.empty() && !roster && !maps) { cm_notify(L"Add at least one Packfile, Shared or Roster folder or file, edit the roster or add a map"); return; }
    std::wstring err;
    if (roster && !re_validate(err)) { cm_notify(err); cm_select_tab(1); re_rebuild(); return; }
    if (maps && !se_validate(err)) { cm_notify(err); cm_select_tab(2); se_rebuild(); return; }
    bool save = !re_mod_src.empty() || install;   // editing an installed mod, or a new one installed directly: written in its folder
    std::string id = !re_mod_src.empty() ? re_mod_src : mod_clean_id(nombre), dest;
    if (install && id.empty()) { cm_notify(L"Type a valid mod name"); return; }
    if (install && re_mod_src.empty()) {
        std::error_code e0;
        if (std::filesystem::exists(std::filesystem::u8path(mods_root_global + "/" + id), e0) && MessageBoxW(cm, (L"A mod with the id " + mw_w(id) + L" is already installed. Replace it?").c_str(), L"Install mod", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    }
    if (!save) {
        wchar_t file_name[4096];
        wcsncpy(file_name, mw_w(id + ".mkpak").c_str(), 4095); file_name[4095] = 0;
        OPENFILENAMEW of; memset(&of, 0, sizeof(of));
        of.lStructSize = sizeof(of); of.hwndOwner = cm;
        of.lpstrFilter = L"Mod (*.mkpak)\0*.mkpak\0";
        of.lpstrFile = file_name; of.nMaxFile = 4096; of.lpstrTitle = L"Save mod"; of.lpstrDefExt = L"mkpak";
        of.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&of)) return;
        dest = mw_u8(file_name);
    }
    CmZip z;
    if (!save) z.f = mods_fopen(dest, "wb");
    if (!save && !z.f) { cm_notify(L"Cannot write " + mw_w(dest)); return; }
    // saving over an installed mod: only what changed is written in its folder, what the mod no longer has is deleted
    std::string here = mods_root_global + "/" + id;
    std::set<std::string> made, mkd;
    int n_copy = 0, n_write = 0, n_same = 0, n_del = 0;
    bool to_open = false;   // a container was copied or written: only then the opener has work, only in those folders
    std::string changed;
    auto mark = [&](const std::string& n) {
        if (n.compare(0, 9, "Packfile/")) return;
        changed += n + "\n";
        if (mw_closed_ext(n)) to_open = true;
    };
    auto low_of = [](std::string x) { for (auto& ch : x) ch = ch == '\\' ? '/' : (char)tolower((unsigned char)ch); return x; };
    auto norm = [&](const std::string& x) {   // lexical: '/', lowercase, no "." or ".." segments
        std::string l = low_of(x), o = !l.empty() && l[0] == '/' ? "/" : "";
        std::vector<std::string> seg;
        for (size_t i = 0; i <= l.size();) {
            size_t j = l.find('/', i); if (j == std::string::npos) j = l.size();
            std::string t = l.substr(i, j - i);
            if (t == ".." && !seg.empty() && seg.back() != "..") seg.pop_back();
            else if (!t.empty() && t != ".") seg.push_back(t);
            i = j + 1;
        }
        for (size_t k = 0; k < seg.size(); k++) o += (k ? "/" : "") + seg[k];
        return o;
    };
    auto mkparent = [&](const std::string& full) {
        std::string pd = std::filesystem::u8path(full).parent_path().u8string();
        if (mkd.insert(pd).second) { std::error_code e2; std::filesystem::create_directories(std::filesystem::u8path(pd), e2); }
    };
    std::string label;
    for (int i = 0; i < MOD_NTAGS; i++) if (theme_primary.count(cm_label[i]) || (roster && re_has_roster() && !wcscmp(MOD_TAGS[i], L"Roster")) || (maps && !wcscmp(MOD_TAGS[i], L"Maps"))) label += (label.empty() ? "" : ", ") + mw_u8(MOD_TAGS[i]);
    std::string txt = "id=" + id + "\nnombre=" + nombre + "\nversion=" + cm_text(IDC_VERSION) + "\nautor=" + cm_text(IDC_AUTHOR) + "\netiquetas=" + label + "\ndescripcion=" + cm_escape(cm_text(IDC_DESC)) + "\nenlace=" + cm_text(IDC_LINK) + "\n";
    for (auto& e : cm_extra) txt += e.first + "=" + cm_escape(e.second) + "\n";
    std::set<std::string> names;
    ReAdd add = [&](const std::string& n, const std::vector<uint8_t>& d) {
        std::string low = n;
        for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
        if (!names.insert(low).second) return true;   // the roster's own files win over the same path in the contents
        if (!save) return z.entrada(n, d);
        made.insert(low);
        std::string full = here + "/" + n;
        std::vector<uint8_t> old;
        if (cm_read(full, old) && old == d) { n_same++; return true; }
        mkparent(full);
        FILE* f = mods_fopen(full, "wb");
        if (!f) return false;
        bool w = d.empty() || fwrite(d.data(), 1, d.size(), f) == d.size();
        fclose(f);
        n_write++;
        mark(n);
        return w;
    };
    if (save) re_copy = [&](const std::string& n, const std::string& src) {
        std::string low = n;
        for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
        if (!names.insert(low).second) return 1;
        made.insert(low);
        std::string full = here + "/" + n;
        if (norm(src) == norm(full)) { n_same++; return 1; }
        mkparent(full);
        std::error_code e2;
        std::filesystem::copy_file(std::filesystem::u8path(src), std::filesystem::u8path(full), std::filesystem::copy_options::overwrite_existing, e2);
        if (e2) return -1;
        n_copy++;
        mark(n);
        return 1;
    };
    std::vector<uint8_t> d(txt.begin(), txt.end());
    bool ok = add("mod.txt", d);
    if (ok && !cm_img.empty() && cm_read(cm_img, d)) ok = add("imagen.png", d);
    int files = 0;
    auto tq = std::chrono::steady_clock::now();
    double tm[6] = {0, 0, 0, 0, 0, 0};
    auto lap = [&](int k) { auto n = std::chrono::steady_clock::now(); tm[k] += std::chrono::duration<double>(n - tq).count(); tq = n; };
    if (ok && roster) { ok = re_export(add, files, err); if (!ok && err.empty()) err = L"Failed to write the mkpak"; }
    lap(0);
    if (ok && maps) { ok = se_export(add, files, err); if (!ok && err.empty()) err = L"Failed to write the mkpak"; }
    lap(1);
    std::error_code ec;
    for (auto& c : cm_content) {
        if (!ok) break;
        if (!c.dir) { ok = re_add_file(add, c.top + (c.rel.empty() ? "" : "/" + c.rel), c.base, files); continue; }
        ok = re_add_dir(add, c.top + (c.rel.empty() ? "" : "/" + c.rel), c.base, files);
    }
    lap(2);
    re_copy = nullptr;
    if (save) {
        if (!ok) { cm_notify(err.empty() ? L"Failed to save the mod" : err); return; }
        std::vector<std::filesystem::path> gone;
        std::filesystem::path pr = std::filesystem::u8path(here);
        fs_walk(here, [&](const std::string& rel, const std::string& full) { if (!made.count(low_of(rel))) gone.push_back(std::filesystem::u8path(full)); return true; });
        std::set<std::string> up;
        for (auto& g : gone) { std::error_code e2; if (std::filesystem::remove(g, e2)) { n_del++; up.insert(g.parent_path().u8string()); } }
        while (!up.empty()) {   // folders left empty, deepest first, up to the mod folder
            auto it = std::max_element(up.begin(), up.end(), [](const std::string& a, const std::string& b) { return a.size() < b.size(); });
            std::filesystem::path d = std::filesystem::u8path(*it);
            up.erase(it);
            std::error_code e2;
            if (d.generic_u8string().size() > pr.generic_u8string().size() && std::filesystem::is_empty(d, e2) && std::filesystem::remove(d, e2)) up.insert(d.parent_path().u8string());
        }
        printf("mods: guardado en su carpeta %s: %d copiados, %d escritos, %d sin cambios, %d borrados\n", id.c_str(), n_copy, n_write, n_same, n_del);
    } else if (ok) ok = z.finish(); else if (z.f) fclose(z.f);
    lap(3);
    if (!ok) { std::filesystem::remove(std::filesystem::u8path(dest), ec); cm_notify(err.empty() ? L"Failed to write the mkpak" : err); return; }
    if (!save) printf("mods: creado %s, %d archivos%s%s\n", dest.c_str(), files, roster ? " (con roster)" : "", maps ? " (con mapas)" : "");
    if (!save) { cm_notify(L"Created: " + mw_w(dest) + L" (" + std::to_wstring(files) + L" files)"); return; }
    std::string got = id, out;
    lap(4);
    std::string dir = mods_root_global + "/" + got;
    bool same = n_copy + n_write + n_del == 0;
    bool closed = to_open && mw_closed(dir);
    if (closed) {
        cm_notify(L"Opening the mod contents...");
        std::string lst = dir + "/.cambios.txt";
        FILE* fl = mods_fopen(lst, "wb");
        if (fl) { fwrite(changed.data(), 1, changed.size(), fl); fclose(fl); }
        bool pr = pm_run(L"--mod " + pm_quote(dir) + (fl ? L" --solo " + pm_quote(lst) : L""), out);
        std::filesystem::remove(std::filesystem::u8path(lst), ec);
        for (size_t p = 0; p < out.size();) {
            size_t f = out.find('\n', p);
            std::string l = out.substr(p, f == std::string::npos ? std::string::npos : f - p);
            while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
            if (!l.compare(0, 4, "mod:") || !l.compare(0, 8, "tiempos:") || !l.compare(0, 4, "bt3:")) printf("mods: %s: %s\n", got.c_str(), l.c_str());
            p = f == std::string::npos ? out.size() : f + 1;
        }
        if (!pr) { cm_notify(L"Saved, but its .pak files stayed closed: " + pm_error(out)); return; }
    }
    lap(5);
    printf("mods: tiempos al guardar %s: personajes %.1f s, mapas %.1f s, contenido %.1f s, cierre %.1f s, instalar %.1f s, abrir %.1f s%s, %d archivos\n", got.c_str(), tm[0], tm[1], tm[2], tm[3], tm[4], tm[5], closed ? "" : " (nada que abrir)", files);
    if (same) { printf("mods: guardado sobre %s, sin cambios, sin reiniciar\n", got.c_str()); cm_notify(L"No changes to save"); return; }
    if (install && re_mod_src.empty()) printf("mods: instalado desde el creador %s\n", got.c_str());
    if (!restart) { printf("mods: guardado sobre %s, sin reiniciar\n", got.c_str()); cm_notify(L"Saved. The changes apply the next time the game starts"); return; }
    printf("mods: guardado sobre %s, reiniciando\n", got.c_str());
    restart_requested = true;
    cm_close();
}

// an installed mod into both tabs: details, image, contents (its character folders travel through the roster)
static void cm_load_mod(int k) {
    std::string root = mods_root_global;
    SetWindowTextW(GetDlgItem(cm, IDC_CREATE), k > 0 ? L"Save and restart" : L"Create .mkpak...");
    SetWindowTextW(GetDlgItem(cm, IDC_SAVE), k > 0 ? L"Save" : L"Install");
    cm_extra.clear();
    if (k <= 0) { re_reset_all(); se_reset(); re_rebuild(); se_rebuild(); cm_notify(L"New mod: the original roster and maps"); return; }
    wchar_t t[512] = L""; SendMessageW(GetDlgItem(cm, IDR_MOD), CB_GETLBTEXT, k, (LPARAM)t);
    std::string mod = mw_u8(t);
    std::vector<std::string> skip = re_load_mod(root, mod), mskip = se_load_mod(root, mod);
    ModInfo m; m.id = mod; mod_read_txt(root, m);
    for (auto& e : m.data_)
        if (e.first != "id" && e.first != "nombre" && e.first != "version" && e.first != "autor" && e.first != "etiquetas" && e.first != "descripcion" && e.first != "enlace") cm_extra.push_back(e);
    auto field = [&](const char* n) { auto it = m.data_.find(n); return it == m.data_.end() ? std::string() : it->second; };
    SetWindowTextW(GetDlgItem(cm, IDC_NAME), mw_w(field("nombre").empty() ? mod : field("nombre")).c_str());
    SetWindowTextW(GetDlgItem(cm, IDC_VERSION), mw_w(field("version")).c_str());
    SetWindowTextW(GetDlgItem(cm, IDC_AUTHOR), mw_w(field("autor")).c_str());
    SetWindowTextW(GetDlgItem(cm, IDC_LINK), mw_w(field("enlace")).c_str());
    std::string de;
    for (char ch : field("descripcion")) { if (ch == '\n') de += "\r\n"; else de += ch; }
    SetWindowTextW(GetDlgItem(cm, IDC_DESC), mw_w(de).c_str());
    std::string tags = mod_tags_en(field("etiquetas"));
    for (int i = 0; i < MOD_NTAGS; i++) {
        std::string n = mw_u8(MOD_TAGS[i]);
        bool on = (", " + tags + ",").find(", " + n + ",") != std::string::npos;
        if (on) theme_primary.insert(cm_label[i]); else theme_primary.erase(cm_label[i]);
        InvalidateRect(cm_label[i], nullptr, TRUE);
    }
    std::error_code ec;
    cm_img = std::filesystem::is_regular_file(std::filesystem::u8path(root + "/" + mod + "/imagen.png"), ec) ? root + "/" + mod + "/imagen.png" : "";
    cm_image_set();
    cm_content.clear();
    std::string base = root + "/" + mod;
    if (std::filesystem::is_directory(std::filesystem::u8path(base + "/Shared"), ec)) cm_push(base + "/Shared", "", "Shared", true);
    std::string pk = base + "/Packfile";
    for (auto& p : std::filesystem::directory_iterator(std::filesystem::u8path(pk), ec)) {
        std::string n = p.path().filename().u8string();
        if (n == "Maps" && p.is_directory(ec)) {   // its new maps travel through the Maps tab
            for (auto& q : std::filesystem::directory_iterator(p.path(), ec)) {
                std::string qn = q.path().filename().u8string();
                if (qn == "SFX" && q.is_directory(ec)) {
                    for (auto& x : std::filesystem::directory_iterator(q.path(), ec)) {
                        std::string r = "Maps/SFX/" + x.path().filename().u8string();
                        if (std::find(mskip.begin(), mskip.end(), r) == mskip.end()) cm_push(x.path().u8string(), r, "Packfile", x.is_directory(ec));
                    }
                    continue;
                }
                std::string r = "Maps/" + qn;
                if (std::find(mskip.begin(), mskip.end(), r) == mskip.end()) cm_push(q.path().u8string(), r, "Packfile", q.is_directory(ec));
            }
            continue;
        }
        if (n != "Characters" || !p.is_directory(ec)) { cm_push(p.path().u8string(), n, "Packfile", p.is_directory(ec)); continue; }
        for (auto& q : std::filesystem::directory_iterator(p.path(), ec)) {
            std::string r = "Characters/" + q.path().filename().u8string();
            if (std::find(skip.begin(), skip.end(), r) != skip.end()) continue;
            cm_push(q.path().u8string(), r, "Packfile", q.is_directory(ec));
        }
    }
    auto isf = [&](const std::string& p) { return std::filesystem::is_regular_file(std::filesystem::u8path(p), ec); };
    for (auto& p : std::filesystem::directory_iterator(std::filesystem::u8path(base), ec)) {   // everything else the mod has stays in it
        std::string n = p.path().filename().u8string(), low = n;
        for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
        bool dir = p.is_directory(ec);
        if (low.empty() || low[0] == '.' || low == "mod.txt" || low == "imagen.png" || low == "packfile" || low == "shared" || low == "thumbs.db" || low == "desktop.ini") continue;
        if ((low == "roster" && isf(base + "/" + n + "/roster.txt")) || (low == "stages" && isf(base + "/" + n + "/stages.txt"))) continue;
        if (low != "lang" || !dir) { cm_push(p.path().u8string(), "", n, dir); continue; }
        for (auto& q : std::filesystem::directory_iterator(p.path(), ec)) {   // a roster character's voices travel through the roster
            std::string pk = q.path().filename().u8string();
            if (!q.is_directory(ec)) { cm_push(q.path().u8string(), pk, n, false); continue; }
            for (auto& x : std::filesystem::directory_iterator(q.path(), ec)) {
                std::string xn = x.path().filename().u8string(), r = pk + "/" + xn;
                if (xn != "Packfile" || !x.is_directory(ec)) { cm_push(x.path().u8string(), r, n, x.is_directory(ec)); continue; }
                for (auto& y : std::filesystem::directory_iterator(x.path(), ec)) {
                    std::string yn = y.path().filename().u8string();
                    if (yn != "Characters" || !y.is_directory(ec)) { cm_push(y.path().u8string(), r + "/" + yn, n, y.is_directory(ec)); continue; }
                    for (auto& z : std::filesystem::directory_iterator(y.path(), ec)) {
                        std::string c = "Characters/" + z.path().filename().u8string();
                        if (std::find(skip.begin(), skip.end(), c) == skip.end()) cm_push(z.path().u8string(), r + "/" + c, n, z.is_directory(ec));
                    }
                }
            }
        }
    }
    cm_fill();
    re_rebuild();
    se_rebuild();
    cm_notify(L"Editing " + mw_w(mod) + L": Save writes over the installed mod");
}

static void cm_place(int id, int x, int y, int w, int h) { MoveWindow(GetDlgItem(cm, id), E(x), E(y), E(w), E(h), TRUE); }

static void cm_select_tab(int k) {
    cm_tab = k;
    for (int t = 0; t < 3; t++) for (HWND c : cm_page[t]) ShowWindow(c, t == k ? SW_SHOW : SW_HIDE);
    for (int t = 0; t < 4; t++) InvalidateRect(GetDlgItem(cm, IDC_TAB0 + t), nullptr, TRUE);
    int W = CM_W[k], H = CM_H[k];
    RECT r = {0, 0, E(W), E(H)};
    AdjustWindowRectEx(&r, (DWORD)GetWindowLongW(cm, GWL_STYLE), FALSE, (DWORD)GetWindowLongW(cm, GWL_EXSTYLE));
    RECT g; GetWindowRect(cm, &g);
    int w = r.right - r.left, h = r.bottom - r.top, x = g.left, y = g.top;
    MONITORINFO mi; mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromWindow(cm, MONITOR_DEFAULTTONEAREST), &mi)) {
        if (x + w > mi.rcWork.right) x = std::max((int)mi.rcWork.left, (int)mi.rcWork.right - w);
        if (y + h > mi.rcWork.bottom) y = std::max((int)mi.rcWork.top, (int)mi.rcWork.bottom - h);
    }
    SetWindowPos(cm, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    cm_place(IDC_NOTICE, CM_RAIL + 16, H - 40, k >= 1 ? 684 : W - CM_RAIL - 32, 28);
    cm_place(IDC_SAVE, 12, H - 130, 144, 32);
    cm_place(IDC_CREATE, 12, H - 92, 144, 32);
    cm_place(IDC_CLOSE, 12, H - 54, 144, 32);
    if (k == 1) { re_rebuild(); SetFocus(re_view ? re_main.h : rv_list); }
    if (k == 2) { se_rebuild(); SetFocus(se_grid); }
    InvalidateRect(cm, nullptr, TRUE);
}

static void cm_close() {
    if (!cm) return;
    EnableWindow(ventana, TRUE);
    SetForegroundWindow(ventana);
    for (int i = 0; i < MOD_NTAGS; i++) theme_primary.erase(cm_label[i]);
    se_close();
    for (int t = 0; t < 3; t++) { for (HWND c : cm_page[t]) { theme_primary.erase(c); theme_exists.erase(c); theme_switch.erase(c); theme_modded.erase(c); } cm_page[t].clear(); theme_primary.erase(GetDlgItem(cm, IDC_TAB0 + t)); }
    theme_primary.erase(GetDlgItem(cm, IDC_SAVE));
    re_ready = false;
    DestroyWindow(cm);
    cm = nullptr;
    re_main.h = re_forms.h = nullptr;
    re_bitmaps_drop(-100);
    if (cm_bmp) { DeleteObject(cm_bmp); cm_bmp = nullptr; }
}

static void cm_command(int id) {
    if (id >= IDC_LABEL && id < IDC_LABEL + MOD_NTAGS) {
        HWND b = cm_label[id - IDC_LABEL];
        if (!theme_primary.erase(b)) theme_primary.insert(b);
        InvalidateRect(b, nullptr, TRUE);
        return;
    }
    switch (id) {
        case IDC_TAB0: cm_select_tab(0); break;
        case IDC_TAB1: case IDC_TAB3: re_view_set(id == IDC_TAB3); cm_select_tab(1); break;
        case IDC_TAB2: cm_select_tab(2); break;
        case IDC_ADD: cm_pick_folder(); break;
        case IDC_REMOVE: { int k = (int)SendMessageW(cm_list, LB_GETCURSEL, 0, 0); if (k >= 0 && k < (int)cm_content.size()) { cm_content.erase(cm_content.begin() + k); cm_fill(); } break; }
        case IDC_PICK_IMG: cm_pick_image(); break;
        case IDC_REMOVE_IMG: cm_img.clear(); cm_image_set(); break;
        case IDC_CREATE: cm_create(); break;
        case IDC_SAVE: if (re_mod_src.empty()) cm_create(true, true); else cm_create(false); break;
        case IDC_CLOSE: cm_close(); break;
    }
}

static void cm_paint_rows(HDC dc, const std::vector<ReRow>& rows, const wchar_t* added);
// Select screen right column: header (render, name, kind) and the General rows laid out by re_general_layout
static void cm_paint_general(HDC dc, int s, bool nw, bool orig) {
    SetBkMode(dc, TRANSPARENT);
    if (s >= -1) {
        rv_thumb(dc, s, E(884), E(56), E(64), E(36));
        RECT t = {E(960), E(54), E(1264), E(76)}, u = {E(960), E(74), E(1264), E(94)};
        rv_text(dc, mw_w(re_label(s)), t, THEME_TEXT, theme_f_bold);
        rv_text(dc, s == -1 ? L"Random, always the last cell" : nw ? L"New character" : L"Game character", u, THEME_TEXT2, theme_f_normal);
    } else {
        RECT t = {E(884), E(56), E(1264), E(92)};
        rv_text(dc, L"Select a cell", t, THEME_TEXT2, theme_f_normal);
    }
    HPEN line = CreatePen(PS_SOLID, 1, THEME_BORDER);
    HGDIOBJ op = SelectObject(dc, line);
    MoveToEx(dc, E(884), E(140), nullptr); LineTo(dc, E(1264), E(140));
    SelectObject(dc, op); DeleteObject(line);
    if (re_page == 0 && (nw || orig)) cm_paint_rows(dc, re_grows, nw ? L"Added" : L"Replaced");
}
static void cm_paint_rows(HDC dc, const std::vector<ReRow>& rows, const wchar_t* added) {
    SetBkMode(dc, TRANSPARENT);
    HPEN sep = CreatePen(PS_SOLID, 1, RGB(42, 48, 59));
    HGDIOBJ op = SelectObject(dc, sep);
    for (auto& r : rows) {
        if (r.cap) { RECT c = {E(884), E(r.y + 8), E(1264), E(r.y + r.h)}; rv_text(dc, r.label, c, THEME_TEXT2, theme_f_normal); continue; }
        if (!r.label.empty()) { RECT c = {E(884), E(r.y), E(1060), E(r.y + r.h)}; rv_text(dc, r.label, c, THEME_TEXT, theme_f_normal); }
        MoveToEx(dc, E(884), E(r.y + r.h) - 1, nullptr); LineTo(dc, E(1264), E(r.y + r.h) - 1);
        if (r.frame >= 0) theme_frame_e(dc, 1064, r.y + 1, 200, 30);
        auto it = re_st.find(r.btn);
        if (r.btn < 0 || it == re_st.end()) continue;
        int st = it->second;
        static const COLORREF BG[4] = {RGB(42, 48, 59), RGB(29, 53, 80), RGB(58, 43, 85), RGB(74, 50, 21)}, FG[4] = {THEME_TEXT2, RGB(156, 198, 240), RGB(205, 184, 242), RGB(255, 207, 143)};
        std::wstring t = st == 0 ? L"None" : st == 1 ? L"Game" : st == 2 ? L"This mod" : r.btn == IDR_T0 ? L"On" : added;
        HGDIOBJ of = SelectObject(dc, theme_f_normal); SIZE sz; GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &sz); SelectObject(dc, of);
        int w = sz.cx + E(16), x = E(1180) - w, y = E(r.y + 6);
        rv_round(dc, x, y, w, E(20), E(10), BG[st]);
        RECT c = {x, y, x + w, y + E(20)};
        rv_text(dc, t, c, FG[st], theme_f_normal, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(dc, op); DeleteObject(sep);
}
static void cm_paint_maps(HDC dc) {
    SetBkMode(dc, TRANSPARENT);
    if (HBITMAP b = se_bitmap(se_cur)) {
        HDC im = CreateCompatibleDC(dc); HGDIOBJ o = SelectObject(im, b);
        SetStretchBltMode(dc, HALFTONE);
        StretchBlt(dc, E(884), E(56), E(72), E(36), im, 0, 0, E(SE_IW), E(SE_IH), SRCCOPY);
        SelectObject(im, o); DeleteDC(im);
    }
    RECT t = {E(968), E(54), E(1264), E(76)}, u = {E(968), E(74), E(1264), E(94)};
    rv_text(dc, mw_w(se_label(se_cur)), t, THEME_TEXT, theme_f_bold);
    rv_text(dc, se_sel() ? L"New map" : se_cur < stages::OLD ? L"Game map" : L"Random, picks any map", u, THEME_TEXT2, theme_f_normal);
    HPEN line = CreatePen(PS_SOLID, 1, THEME_BORDER);
    HGDIOBJ op = SelectObject(dc, line);
    MoveToEx(dc, E(884), E(104), nullptr); LineTo(dc, E(1264), E(104));
    SelectObject(dc, op); DeleteObject(line);
    cm_paint_rows(dc, se_grows, L"Added");
}

static LRESULT CALLBACK cm_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == 0x02E0) { re_bitmaps_drop(-100); return theme_dpi_change(h, w, l); }
    if (m == WM_COMMAND) {
        int id = LOWORD(w);
        if (id >= IDS_NEW && id < IDS_LAST) { se_command(id, HIWORD(w)); return 0; }
        if (id >= IDR_NEW && id < IDR_LAST) {
            if (id == IDR_MOD && HIWORD(w) == CBN_SELCHANGE) cm_load_mod((int)SendMessageW((HWND)l, CB_GETCURSEL, 0, 0));
            else re_command(id, HIWORD(w));
            return 0;
        }
        if (id == IDV_SEARCH) { if (HIWORD(w) == EN_CHANGE) { wchar_t t[128] = L""; GetWindowTextW((HWND)l, t, 128); rv_filter = t; re_view_apply(); } return 0; }
        if (cm && (HIWORD(w) == BN_CLICKED || id < IDC_NAME || id > IDC_DESC)) cm_command(id);
        return 0;
    }
    if (m == WM_DRAWITEM) { if (!rv_draw((DRAWITEMSTRUCT*)l)) theme_button((DRAWITEMSTRUCT*)l); return TRUE; }
    if (m == WM_CONTEXTMENU) { int id = GetDlgCtrlID((HWND)w); if (id == IDR_F0) re_file_clear(re_col); if (id == IDR_F4) re_file_clear(RF_SFX); if (id == IDR_F5) re_file_clear(RF_VOICES); if (id == IDR_F6) re_file_clear(RF_AURA); if (id == IDR_F7) re_file_clear(RF_CHG1); if (id == IDR_D0) re_dmg_clear(re_col); if (id >= IDR_LANG0 && id <= IDR_LANG3) re_lang_clear(id - IDR_LANG0); if (id == IDS_MAP || id == IDS_SFX) se_file_clear(id == IDS_SFX); return 0; }
    if (m == WM_CTLCOLORSTATIC) {
        int id = GetDlgCtrlID((HWND)l);
        if (id == IDC_SUB || id == IDC_NOTICE || id == IDC_HINT || id == IDR_SUB || id == IDR_INFO || id == IDR_PINFO || id == IDR_SUM || id == IDR_FORMS_L || id == IDR_LHELP || id == IDR_FTN0 || id == IDR_FTN1 || (id >= IDR_LN0 && id < IDR_LN0 + 9) || id == IDS_SUB || id == IDS_INFO || id == IDS_SUM || id == IDS_IMG_L || id == IDS_F_L) {
            SetBkMode((HDC)w, TRANSPARENT); SetBkColor((HDC)w, THEME_BG);
            SetTextColor((HDC)w, id == IDC_NOTICE ? THEME_ACCENT : THEME_TEXT2);
            return (LRESULT)theme_b_bg;
        }
    }
    { LRESULT r; if (theme_color(m, w, l, &r)) return r; }
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        { RECT rr; GetClientRect(h, &rr); rr.right = E(CM_RAIL); HBRUSH b = CreateSolidBrush(RV_RAIL); FillRect(dc, &rr, b); DeleteObject(b); }
        if (cm_tab == 0) {
            for (auto& c : CM_FIELDS) theme_frame_e(dc, c[0] + CM_RAIL - 4, c[1] - 3, c[2] + 8, c[3] + 6);
            theme_frame_e(dc, 799, 23, CM_IMG_W + 2, CM_IMG_H + 2);
            theme_frame_e(dc, 799, 363, 466, 216);
            HPEN sep = CreatePen(PS_SOLID, 1, RGB(42, 48, 59)); HGDIOBJ op = SelectObject(dc, sep);
            for (int y : {60, 96, 132, 168, 292}) { MoveToEx(dc, E(184), E(y), nullptr); LineTo(dc, E(760), E(y)); }
            SelectObject(dc, op); DeleteObject(sep);
        } else if (cm_tab == 2) {
            theme_frame_e(dc, 183, 63, 614, 594);
            cm_paint_maps(dc);
        } else if (re_view == 0) {
            HPEN p = CreatePen(PS_SOLID, 1, THEME_BORDER); HGDIOBJ op = SelectObject(dc, p);
            MoveToEx(dc, E(412), E(124), nullptr); LineTo(dc, E(1264), E(124));
            SelectObject(dc, op); DeleteObject(p);
        } else {
            int s = re_selected();
            bool nw = s >= 0 && re_find(s), orig = s >= 0 && !nw;
            theme_frame_e(dc, 183, 63, 522, 594);
            theme_frame_e(dc, 719, 85, 150, 572);
            cm_paint_general(dc, s, nw, orig);
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_DROPFILES) {
        HDROP hd = (HDROP)w;
        UINT n = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
        if (cm_tab == 1) re_drop_files(hd);
        else if (cm_tab == 2) se_drop_files(hd);
        else for (UINT i = 0; i < n; i++) { wchar_t fpath[4096]; if (DragQueryFileW(hd, i, fpath, 4096)) cm_add(mw_u8(fpath)); }
        DragFinish(hd);
        return 0;
    }
    if (m == WM_CLOSE) { cm_close(); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static void cm_open() {
    theme_init(ventana);
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = cm_proc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"TTTRecompCrearMod";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = theme_b_bg;
        RegisterClassW(&wc);
        registered = true;
    }
    const int CH = CM_H[0];
    HANDLE ctx; ULONG_PTR cookie = 0;
    visual_styles(&ctx, &cookie);
    cm = theme_create(L"TTTRecompCrearMod", L"Create mod", CM_W[0], CH, WS_EX_DLGMODALFRAME | WS_EX_ACCEPTFILES);
    if (cm) {
        cm_page[0].clear(); cm_page[1].clear(); cm_tab = 0;
        cm_mk = -1;
        cm_dx = cm_dy = 0;
        cm_ctl(L"STATIC", L"Create mod", 0, 16, 16, 140, 26, IDC_TITLE, theme_f_title);
        cm_ctl(L"BUTTON", L"Mod", BS_OWNERDRAW | WS_TABSTOP, 8, 112, 152, 36, IDC_TAB0);
        cm_ctl(L"BUTTON", L"Characters", BS_OWNERDRAW | WS_TABSTOP, 8, 192, 152, 36, IDC_TAB1);
        cm_ctl(L"BUTTON", L"Select screen", BS_OWNERDRAW | WS_TABSTOP, 8, 152, 152, 36, IDC_TAB3);
        cm_ctl(L"BUTTON", L"Maps", BS_OWNERDRAW | WS_TABSTOP, 8, 232, 152, 36, IDC_TAB2);
        cm_mk = 0;
        cm_dx = CM_RAIL;
        cm_ctl(L"STATIC", L"Name", 0, 16, 33, 140, 18, -1);
        cm_ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, CM_FIELDS[0][0], CM_FIELDS[0][1], CM_FIELDS[0][2], CM_FIELDS[0][3], IDC_NAME);
        cm_ctl(L"STATIC", L"Version", 0, 16, 69, 140, 18, -1);
        cm_ctl(L"EDIT", L"1.0", ES_AUTOHSCROLL | WS_TABSTOP, CM_FIELDS[1][0], CM_FIELDS[1][1], CM_FIELDS[1][2], CM_FIELDS[1][3], IDC_VERSION);
        cm_ctl(L"STATIC", L"Author", 0, 16, 105, 140, 18, -1);
        cm_ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, CM_FIELDS[2][0], CM_FIELDS[2][1], CM_FIELDS[2][2], CM_FIELDS[2][3], IDC_AUTHOR);
        cm_ctl(L"STATIC", L"Link", 0, 16, 141, 140, 18, -1);
        cm_ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, CM_FIELDS[3][0], CM_FIELDS[3][1], CM_FIELDS[3][2], CM_FIELDS[3][3], IDC_LINK);
        cm_ctl(L"STATIC", L"Description", 0, 16, 177, 140, 18, -1);
        HWND de = cm_ctl(L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP, CM_FIELDS[4][0], CM_FIELDS[4][1], CM_FIELDS[4][2], CM_FIELDS[4][3], IDC_DESC);
        theme_control(de);
        cm_ctl(L"STATIC", L"Tags", 0, 16, 304, 140, 18, IDC_SUB);
        for (int i = 0; i < MOD_NTAGS; i++) cm_label[i] = cm_ctl(L"BUTTON", MOD_TAGS[i], BS_OWNERDRAW | WS_TABSTOP, 16 + (i % 5) * 114, 328 + (i / 5) * 34, 106, 26, IDC_LABEL + i);
        cm_dx = 0;
        cm_image = cm_ctl(L"STATIC", L"", SS_BITMAP | SS_CENTERIMAGE, 800, 24, CM_IMG_W, CM_IMG_H, IDC_IMAGE);
        cm_ctl(L"BUTTON", L"Choose image", BS_OWNERDRAW | WS_TABSTOP, 800, 296, 132, 28, IDC_PICK_IMG);
        cm_ctl(L"BUTTON", L"Remove", BS_OWNERDRAW | WS_TABSTOP, 940, 296, 100, 28, IDC_REMOVE_IMG);
        cm_ctl(L"STATIC", L"Contents", 0, 800, 340, 140, 18, IDC_HINT);
        cm_list = cm_ctl(L"LISTBOX", L"", LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP, 804, 368, 456, 206, IDC_LIST);
        theme_control(cm_list);
        cm_ctl(L"BUTTON", L"Add folder", BS_OWNERDRAW | WS_TABSTOP, 800, 588, 132, 28, IDC_ADD);
        cm_ctl(L"BUTTON", L"Remove", BS_OWNERDRAW | WS_TABSTOP, 940, 588, 100, 28, IDC_REMOVE);

        cm_mk = 1;
        cm_dx = cm_dy = 0;
        // Select screen page: the grid, the cell actions and the forms of the cell
        cm_ctl(L"BUTTON", L"+ New character", BS_OWNERDRAW | WS_TABSTOP, 184, 16, 140, 30, IDR_NEW);
        cm_ctl(L"BUTTON", L"Copy", BS_OWNERDRAW | WS_TABSTOP, 330, 16, 96, 30, IDR_COPY);
        cm_ctl(L"BUTTON", L"Transformation", BS_OWNERDRAW | WS_TABSTOP, 432, 16, 130, 30, IDR_FORM);
        cm_ctl(L"BUTTON", L"General", BS_OWNERDRAW | WS_TABSTOP, 884, 104, 96, 32, IDR_PG0);
        cm_ctl(L"BUTTON", L"Labels", BS_OWNERDRAW | WS_TABSTOP, 984, 104, 88, 32, IDR_PG1);
        // Characters page: list, header with the forms, tabs
        cm_ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 184, 16, 212, 28, IDV_SEARCH);
        SendMessageW(GetDlgItem(cm, IDV_SEARCH), EM_SETCUEBANNER, TRUE, (LPARAM)L"Search");
        rv_create(CH);
        cm_ctl(L"STATIC", L"", SS_ENDELLIPSIS, 0, 0, 0, 0, IDR_TITLE, theme_f_bold);
        cm_ctl(L"STATIC", L"", 0, 412, 136, 700, 52, IDR_PINFO);
        cm_ctl(L"BUTTON", L"<", BS_OWNERDRAW | WS_TABSTOP, 1136, 132, 26, 28, IDR_PCPREV);
        cm_ctl(L"STATIC", L"All colors", SS_CENTER | SS_CENTERIMAGE, 1164, 132, 72, 28, IDR_PCNAME, theme_f_bold);
        cm_ctl(L"BUTTON", L">", BS_OWNERDRAW | WS_TABSTOP, 1238, 132, 26, 28, IDR_PCNEXT);
        cm_ctl(L"BUTTON", L"Undo changes", BS_OWNERDRAW | WS_TABSTOP, 1136, 168, 128, 30, IDR_PUNDO);
        for (int i = 0; i < 16; i++) cm_ctl(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 1136, 220 + 32 * i, 128, 28, IDR_PS0 + i);
        pe_view_create();
        cm_dx = RE_DX; cm_dy = RE_DY;
        cm_ctl(L"STATIC", L"", 0, 16, 156, 300, 34, IDR_SUB);
        cm_ctl(L"STATIC", L"ID", 0, 16, 200, 80, 18, IDR_ID_L);
        cm_ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 104, 196, 208, 24, IDR_ID);
        cm_ctl(L"STATIC", L"BT3 id", 0, 16, 230, 80, 18, IDR_SYS_L);
        theme_control(cm_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 100, 226, 216, 320, IDR_SYS), L"DarkMode_CFD");
        cm_ctl(L"STATIC", L"SFX/Voices", 0, 16, 260, 84, 18, IDR_BASE_L);
        theme_control(cm_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 100, 256, 216, 320, IDR_BASE), L"DarkMode_CFD");
        static const wchar_t* NL[2] = {L"Name EN", L"Name ES"};
        for (int l = 0; l < 2; l++) {
            cm_ctl(L"STATIC", NL[l], 0, 16, 298 + 30 * l, 80, 18, IDR_N_L0 + l);
            cm_ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 104, 294 + 30 * l, 208, 24, IDR_N0 + l);
        }
        re_register();
        cm_ctl(L"STATIC", L"", 0, 16, 388, 1, 1, IDR_IMG_L);
        static const int IB[4][4] = {{16, 388, 70, 84}, {92, 388, 70, 84}, {16, 442, 146, 30}, {168, 388, 146, 84}};
        for (int j = 0; j < 4; j++) cm_ctl(L"TTTRosterImg", L"", 0, IB[j][0], IB[j][1], IB[j][2], IB[j][3], IDR_IMG0 + j);
        cm_ctl(L"STATIC", L"", 0, 16, 480, 1, 1, IDR_F_L);
        cm_ctl(L"BUTTON", L"<", BS_OWNERDRAW | WS_TABSTOP, 16, 480, 28, 26, IDR_CPREV);
        cm_ctl(L"STATIC", L"Color 1", SS_CENTER | SS_CENTERIMAGE, 44, 480, 84, 26, IDR_CNAME, theme_f_bold);
        cm_ctl(L"BUTTON", L">", BS_OWNERDRAW | WS_TABSTOP, 128, 480, 28, 26, IDR_CNEXT);
        cm_ctl(L"BUTTON", L"Color files", BS_OWNERDRAW | WS_TABSTOP, 162, 480, 152, 26, IDR_F0);
        cm_ctl(L"BUTTON", L"Create", BS_OWNERDRAW | WS_TABSTOP, 16, 564, 76, 26, IDR_PCM);
        cm_ctl(L"STATIC", L"", 0, 16, 507, 1, 1, IDR_D_L);
        cm_ctl(L"BUTTON", L"Damaged (model)", BS_OWNERDRAW | WS_TABSTOP, 16, 507, 146, 26, IDR_D0);
        cm_ctl(L"BUTTON", L"Damaged (textures)", BS_OWNERDRAW | WS_TABSTOP, 168, 507, 146, 26, IDR_T0);
        cm_ctl(L"BUTTON", L"SFX", BS_OWNERDRAW | WS_TABSTOP, 16, 534, 146, 26, IDR_F4);
        cm_ctl(L"BUTTON", L"Voices", BS_OWNERDRAW | WS_TABSTOP, 168, 534, 146, 26, IDR_F5);
        cm_ctl(L"BUTTON", L"Charge aura (P2)", BS_OWNERDRAW | WS_TABSTOP, 16, 561, 146, 26, IDR_F6);
        cm_ctl(L"BUTTON", L"Charge anims (P1)", BS_OWNERDRAW | WS_TABSTOP, 168, 561, 146, 26, IDR_F7);
        for (int k = 0; k < 4; k++) cm_ctl(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 16, 561, 70, 26, IDR_LANG0 + k);
        cm_ctl(L"STATIC", L"", SS_ENDELLIPSIS, 16, 591, 300, 18, IDR_INFO);
        static const wchar_t* LN[2] = {L"EN", L"ES"}, *LT[3] = {L"Name", L"Transformation, right", L"Transformation, left"};
        for (int k = 0; k < 3; k++) {
            int y = 166 + 90 * k;
            cm_ctl(L"STATIC", LT[k], 0, 16, y, 300, 18, IDR_LT0 + k, theme_f_bold);
            for (int l = 0; l < 2; l++) {
                cm_ctl(L"STATIC", LN[l], SS_CENTERIMAGE, 16, y + 22 + 34 * l, 28, 30, IDR_LN0 + 3 * k + l);
                cm_ctl(L"TTTRosterLbl", L"", 0, 48, y + 22 + 34 * l, 268, 30, IDR_LB0 + 3 * k + l);
            }
        }
        cm_ctl(L"STATIC", L"Click: choose or import. Right click: default.", 0, 16, 540, 300, 18, IDR_LHELP);
        cm_ctl(L"STATIC", L"Transformation", 0, 16, 256, 300, 18, IDR_FTL, theme_f_bold);
        theme_control(cm_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 16, 278, 300, 360, IDR_FTC), L"DarkMode_CFD");
        for (int l = 0; l < 2; l++) {
            cm_ctl(L"STATIC", LN[l], SS_CENTERIMAGE, 16, 312 + 34 * l, 28, 30, IDR_FTN0 + l);
            cm_ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 48, 315 + 34 * l, 268, 24, IDR_FT0 + l);
        }
        cm_dx = cm_dy = 0;
        re_grid_create(re_main, cm, 184, 64, 520, 592, IDR_GRID);
        cm_page[1].push_back(re_main.h);
        cm_ctl(L"STATIC", L"", SS_ENDELLIPSIS, 184, 662, 520, 18, IDR_SUM);
        cm_ctl(L"STATIC", L"Transformations", SS_ENDELLIPSIS, 720, 64, 148, 18, IDR_FORMS_L);
        re_forms = ReGrid(); re_forms.cols = 1; re_forms.mode = 1;
        re_grid_create(re_forms, cm, 720, 86, 148, 570, IDR_FORMS);
        cm_page[1].push_back(re_forms.h);
        cm_mk = 2;
        cm_dx = CM_RAIL;
        se_controls();
        cm_dx = 0;
        MoveWindow(se_grid, E(184), E(64), E(612), E(592), TRUE);
        cm_mk = -1;

        cm_ctl(L"STATIC", L"Mod to edit", 0, 16, 50, 140, 18, IDR_MOD_L);
        HWND mc = cm_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 12, 70, 144, 300, IDR_MOD);
        theme_control(mc, L"DarkMode_CFD");
        cm_notice = cm_ctl(L"STATIC", L"", SS_CENTERIMAGE | SS_ENDELLIPSIS, 16, CH - 42, 460, 28, IDC_NOTICE);
        cm_ctl(L"BUTTON", L"Create .mkpak...", BS_OWNERDRAW | WS_TABSTOP, 12, CH - 92, 144, 32, IDC_CREATE);
        cm_ctl(L"BUTTON", L"Close", BS_OWNERDRAW | WS_TABSTOP, 12, CH - 54, 144, 32, IDC_CLOSE);
        theme_primary.insert(cm_ctl(L"BUTTON", L"Install", BS_OWNERDRAW | WS_TABSTOP, 12, CH - 130, 144, 32, IDC_SAVE, theme_f_bold));
        DragAcceptFiles(cm, TRUE);
        cm_content.clear(); cm_img.clear(); cm_extra.clear();
        cm_fill(); cm_image_set();
        re_main.mode = 0; re_main.cols = 4;
        re_init();
        re_fill_mods(mods_root_global);
        re_ready = true;
        cm_select_tab(0);
    }
    if (ctx != INVALID_HANDLE_VALUE) { DeactivateActCtx(0, cookie); ReleaseActCtx(ctx); }
    if (!cm) return;
    EnableWindow(ventana, FALSE);
    ShowWindow(cm, SW_SHOW);
    SetForegroundWindow(cm);
    SetFocus(GetDlgItem(cm, IDC_NAME));
}

static void createmod_window() {
    pause_begin();
    cm_open();
    while (cm && abierta) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { abierta = false; break; }
            if (pe_key_msg(msg)) continue;
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && cm) { cm_close(); break; }
            if (!cm || !IsDialogMessageW(cm, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
        if (!cm || !abierta) break;
        theme_poll(cm);
        Sleep(8);
    }
    if (cm) cm_close();
    pause_end();
}

#include "ui/character_editor.h"
