#pragma once
// Params page of the roster tab: the character parameter files (017-027) edited by field, as games/ttt/params.txt describes them.
// Changes are byte patches per character, for all colors or one color; on save they go to the 1_p folder of each color inside the mod.
enum { PT_U8, PT_S8, PT_S16, PT_S32, PT_F32, PT_BITS, PT_LIST, PT_LIST16, PT_LIST32, PT_CHAR, PT_HITS, PT_MIN1, PT_CHECK };
static const int PT_SIZE[13] = {1, 1, 2, 4, 4, 1, 1, 2, 4, 1, 1, 1, 1};
static const long long PE_CHARS = -0x7FFFFFFF;
struct PeOpt { std::wstring t; long long v; };
struct PeField { std::string key; int file = 0, type = 0, off[2] = {-1, -1}, step = -1, copy = -1; long long chk = 1; std::string sec, grp; std::wstring label; std::vector<PeOpt> opt; std::vector<std::wstring> bit; };
struct PeFile { int num = 0, step[2] = {0, 0}; bool by_char = false; std::string name; std::wstring title, item; std::vector<std::wstring> items; };
struct PeSrc { int j = 0; std::vector<std::string> dirs; std::string out; };   // folders a color's 1_p is read from (first wins) and its path in the mod
static std::vector<PeField> pe_f;
static std::map<int, PeFile> pe_files;
static std::map<std::string, int> pe_key;
static bool pe_loaded = false;
static std::map<int, std::map<int, std::map<int, std::map<int, uint8_t>>>> pe_edit;   // character -> color (-1 all) -> file -> byte offset -> value
static std::vector<PeSrc> pe_cols_now;
static std::map<int, std::map<int, std::vector<uint8_t>>> pe_base;   // color -> file -> bytes as read
static int pe_s = -100, pe_game = 1, pe_col = -1;

static std::string pe_trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t a = 0; while (a < s.size() && (s[a] == ' ' || s[a] == '\t')) a++;
    return s.substr(a);
}
static std::vector<std::string> pe_split(const std::string& s, char c) {
    std::vector<std::string> v;
    for (size_t a = 0;;) { size_t b = s.find(c, a); v.push_back(pe_trim(s.substr(a, b == std::string::npos ? std::string::npos : b - a))); if (b == std::string::npos) break; a = b + 1; }
    return v;
}
static void pe_load_spec() {
    if (pe_loaded) return;
    pe_loaded = true;
    FILE* f = fopen("../../games/ttt/params.txt", "rb");
    if (!f) return;
    static const char* TN[13] = {"u8", "s8", "s16", "s32", "f32", "bits", "list", "list16", "list32", "char", "hits", "min1", "check"};
    std::vector<std::wstring> moves;
    int cur = -1;
    bool list = false;
    char line[16384];
    while (fgets(line, sizeof(line), f)) {
        std::string s = pe_trim(line);
        if (s.empty() || s[0] == '#') continue;
        if (!s.compare(0, 6, "[file ")) { cur = atoi(s.c_str() + 6); pe_files[cur].num = cur; list = false; continue; }
        if (s[0] == '[') { list = s == "[list moves]"; cur = -1; continue; }
        if (list) { moves.push_back(mw_w(s)); continue; }
        if (cur < 0) continue;
        PeFile& pf = pe_files[cur];
        if (s.find('|') == std::string::npos) {
            size_t e = s.find('=');
            if (e == std::string::npos) continue;
            std::string k = pe_trim(s.substr(0, e)), v = pe_trim(s.substr(e + 1));
            if (k == "name") pf.name = v;
            else if (k == "title") pf.title = mw_w(v);
            else if (k == "item") pf.item = mw_w(v);
            else if (k == "step") { auto t = pe_split(v, ' '); pf.step[0] = (int)strtol(t[0].c_str(), nullptr, 0); pf.step[1] = t.size() > 1 ? (int)strtol(t.back().c_str(), nullptr, 0) : pf.step[0]; }
            else if (k == "items") {
                if (v == "@moves") pf.items.assign(1, L"@moves");
                else if (v == "@characters") pf.by_char = true;
                else if (!v.empty()) for (auto& x : pe_split(v, ',')) pf.items.push_back(mw_w(x));
            }
            continue;
        }
        auto p = pe_split(s, '|');
        if (p.size() < 7) continue;
        PeField x;
        x.file = cur;
        x.type = -1;
        for (int t = 0; t < 13; t++) if (p[1] == TN[t]) x.type = t;
        if (x.type < 0) continue;
        for (int g = 0; g < 2; g++) x.off[g] = p[2 + g] == "-" ? -1 : (int)strtol(p[2 + g].c_str(), nullptr, 0);
        x.key = p[0]; x.sec = p[4]; x.grp = p[5]; x.label = mw_w(p[6]);
        std::string o = p.size() > 7 ? p[7] : "";
        for (;;) {
            if (!o.compare(0, 5, "step=") || !o.compare(0, 5, "copy=")) {
                size_t e = o.find(';');
                int v = (int)strtol(o.c_str() + 5, nullptr, 0);
                (o[0] == 's' ? x.step : x.copy) = v;
                o = e == std::string::npos ? "" : o.substr(e + 1);
            } else break;
        }
        if (!o.compare(0, 5, "list:"))
            for (auto& it : pe_split(o.substr(5), ',')) {
                if (it == "@chars") { x.opt.push_back({L"", PE_CHARS}); continue; }
                size_t e = it.rfind('=');
                if (e != std::string::npos) x.opt.push_back({mw_w(pe_trim(it.substr(0, e))), strtoll(it.c_str() + e + 1, nullptr, 0)});
            }
        else if (!o.compare(0, 5, "bits:")) for (auto& b : pe_split(o.substr(5), ',')) x.bit.push_back(mw_w(b));
        else if (!o.compare(0, 6, "check:")) x.chk = strtoll(o.c_str() + 6, nullptr, 0);
        pe_key[x.key + (x.off[0] < 0 ? "@ttt" : x.off[1] < 0 ? "@bt3" : "")] = (int)pe_f.size();
        pe_f.push_back(x);
    }
    fclose(f);
    for (auto& x : pe_files) {
        if (x.second.items.size() == 1 && x.second.items[0] == L"@moves") x.second.items = moves;
        if (x.second.title.empty()) for (auto& y : pe_f) if (y.file == x.first && y.sec != "-") { x.second.title = mw_w(y.sec); break; }
        if (x.second.item.empty()) x.second.item = L"Entry";
    }
    printf("roster: params.txt: %d campos, %d archivos\n", (int)pe_f.size(), (int)pe_files.size());
}

// ---------------- the character's files ----------------
static bool pe_isd(const std::string& p) { std::error_code ec; return !p.empty() && std::filesystem::is_directory(std::filesystem::u8path(p), ec); }
static std::string pe_find(const std::string& dir, int num) {   // file of that leading number in a 1_p folder ("" when none)
    char pre[8]; snprintf(pre, sizeof(pre), "%03d", num);
    std::string r;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(mw_w(dir + "/" + pre + "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return r;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string n = mw_u8(fd.cFileName);
        if (n.size() > 3 && isdigit((unsigned char)n[3])) continue;
        r = n; break;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return r;
}
static bool pe_read(const PeSrc& c, int num, std::vector<uint8_t>& d, std::string* name = nullptr, std::string* from = nullptr) {
    for (auto& dir : c.dirs) {
        std::string n = pe_find(dir, num);
        if (n.empty() || !cm_read(dir + "/" + n, d)) continue;
        if (name) *name = n;
        if (from) *from = dir + "/" + n;
        return true;
    }
    return false;
}
static std::vector<PeSrc> pe_cols(int s, int* game) {
    std::vector<PeSrc> v;
    ReChar* c = re_find(s);
    int k = roster::group_vanilla(s);
    std::string mod = re_mod_src.empty() ? "" : re_mod_root + "/" + re_mod_src + "/";
    for (int j = 0; j < RE_COLS; j++) {
        PeSrc x;
        x.j = j;
        if (c) {
            if (c->id.empty()) break;
            x.out = "Packfile/Characters/" + c->id + "/color_" + std::to_string(j + 1) + "/1_p";
            if (!mod.empty() && pe_isd(mod + x.out)) x.dirs.push_back(mod + x.out);
            const std::string& f = c->file[j];
            if ((!f.compare(0, 4, "dir:") || !f.compare(0, 4, "mod:")) && pe_isd(f.substr(4) + "/1_p")) x.dirs.push_back(f.substr(4) + "/1_p");
        } else if (k >= 0) {
            if (!psp_colour_exists(k, j)) continue;
            std::string cf = psp_colour_folder(k, j);
            if (cf.empty()) continue;
            x.out = "Packfile/" + cf + "/1_p";
            auto o = re_ochar.find(s);
            std::string f = o != re_ochar.end() ? o->second.file[j] : "";
            if (!mod.empty() && pe_isd(mod + x.out)) x.dirs.push_back(mod + x.out);
            if (!f.empty() && f.compare(0, 4, "mod:")) {   // a color replaced in this session: only an opened folder has a 1_p
                if (!f.compare(0, 4, "dir:") && pe_isd(f.substr(4) + "/1_p")) x.dirs.push_back(f.substr(4) + "/1_p");
                else continue;
            } else {
                if (!f.empty() && pe_isd(f.substr(4) + "/1_p")) x.dirs.push_back(f.substr(4) + "/1_p");
                x.dirs.push_back(psp_assets_dir() + "/Packfile/" + cf + "/1_p");
            }
        }
        if (!x.dirs.empty()) v.push_back(x);
    }
    int g = 1;
    if (c && !v.empty()) {   // a BT3 color: its opened P file has .dbt texture folders, or a model that is not a TTT pMdl
        for (auto& dir : v[0].dirs) {
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileW(mw_w(dir + "/*.dbt").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) { FindClose(h); g = 0; break; }
            std::string m = pe_find(dir, 2);
            if (m.empty()) continue;
            uint8_t hd[4] = {0, 0, 0, 0};
            if (FILE* f = mods_fopen(dir + "/" + m, "rb")) { size_t got = fread(hd, 1, 4, f); fclose(f); if (got == 4 && memcmp(hd, "pMdl", 4)) g = 0; }
            break;
        }
    }
    if (game) *game = g;
    return v;
}

// ---------------- values (the shown color, or color 1 for all; patches for all colors, then the color's own) ----------------
static int pe_fk(const std::string& k) {
    auto it = pe_key.find(k);
    if (it == pe_key.end()) it = pe_key.find(k + (pe_game ? "@ttt" : "@bt3"));
    return it == pe_key.end() || pe_f[it->second].off[pe_game] < 0 ? -1 : it->second;
}
static int pe_step(const PeField& x) { return x.step >= 0 ? x.step : pe_files[x.file].step[pe_game]; }
static int pe_at(const PeField& x, int item) { int o = x.off[pe_game]; return o < 0 ? -1 : o + pe_step(x) * item; }
static int pe_vcol() { return pe_col >= 0 ? pe_col : pe_cols_now.empty() ? 0 : pe_cols_now[0].j; }
static const std::vector<uint8_t>* pe_file(int j, int file) {
    auto c = pe_base.find(j);
    if (c == pe_base.end() || !c->second.count(file)) c = pe_base.find(pe_cols_now.empty() ? 0 : pe_cols_now[0].j);   // a color without the file uses color 1's
    if (c == pe_base.end()) return nullptr;
    auto f = c->second.find(file);
    return f == c->second.end() ? nullptr : &f->second;
}
static bool pe_fits(const PeField& x, int item) { int o = pe_at(x, item); auto* b = pe_file(pe_vcol(), x.file); return o >= 0 && b && o + PT_SIZE[x.type] <= (int)b->size(); }
static int pe_byte(int j, int file, int o, int level) {   // level 0 file as read, 1 + all colors patch, 2 + the color's own patch
    auto* b = pe_file(j, file);
    if (!b || o < 0 || o >= (int)b->size()) return 0;
    auto& ce = pe_edit[pe_s];
    if (level >= 2) { auto c = ce.find(j); if (c != ce.end()) { auto f = c->second.find(file); if (f != c->second.end()) { auto it = f->second.find(o); if (it != f->second.end()) return it->second; } } }
    if (level >= 1) { auto c = ce.find(-1); if (c != ce.end()) { auto f = c->second.find(file); if (f != c->second.end()) { auto it = f->second.find(o); if (it != f->second.end()) return it->second; } } }
    return (*b)[o];
}
static long long pe_get_at(const PeField& x, int item, int j, int level) {
    int o = pe_at(x, item), n = PT_SIZE[x.type];
    uint32_t u = 0;
    for (int i = 0; i < n; i++) u |= (uint32_t)pe_byte(j, x.file, o + i, level) << (8 * i);
    switch (x.type) {
        case PT_S8: return (int8_t)u;
        case PT_S16: return (int16_t)u;
        case PT_S32: case PT_LIST32: return (int32_t)u;
        default: return u;
    }
}
static long long pe_get(const PeField& x, int item) { return pe_get_at(x, item, pe_vcol(), 2); }
static float pe_tof(long long v) { uint32_t u = (uint32_t)v; float f; memcpy(&f, &u, 4); return f; }
static bool pe_changed(const PeField& x, int item, int bit = -1) {
    long long a = pe_get(x, item), b = pe_get_at(x, item, pe_vcol(), 0);
    return bit >= 0 ? ((a ^ b) >> bit) & 1 : a != b;
}
static bool pe_varies(const PeField& x, int item, int bit = -1) {   // the colors hold different values (shown only for all colors)
    if (pe_col >= 0 || pe_cols_now.size() < 2) return false;
    long long a = pe_get_at(x, item, pe_cols_now[0].j, 2);
    for (auto& c : pe_cols_now) {
        if (!pe_base[c.j].count(x.file)) continue;
        long long b = pe_get_at(x, item, c.j, 2);
        if (bit >= 0 ? ((a ^ b) >> bit) & 1 : a != b) return true;
    }
    return false;
}
static void pe_put(const PeField& x, int item, uint32_t u) {
    int o = pe_at(x, item), n = PT_SIZE[x.type];
    auto& ce = pe_edit[pe_s];
    auto set = [&](int at, uint8_t b) {
        if (at < 0) return;
        if (pe_col >= 0) {
            auto& e = ce[pe_col][x.file];
            if (b == pe_byte(pe_col, x.file, at, 1)) e.erase(at); else e[at] = b;
            return;
        }
        bool same = true;
        for (auto& c : pe_cols_now) { ce[c.j][x.file].erase(at); same = same && (!pe_base[c.j].count(x.file) || pe_byte(c.j, x.file, at, 0) == b); }
        auto& e = ce[-1][x.file];
        if (same) e.erase(at); else e[at] = b;
    };
    for (int i = 0; i < n; i++) set(o + i, (uint8_t)(u >> (8 * i)));
    if (x.copy >= 0 && pe_game == 0) set(x.copy + pe_step(x) * item, (uint8_t)u);
}
static bool pe_any() { for (auto& s : pe_edit) for (auto& c : s.second) for (auto& f : c.second) if (!f.second.empty()) return true; return false; }
static int pe_count(int s) { int n = 0; auto it = pe_edit.find(s); if (it != pe_edit.end()) for (auto& c : it->second) for (auto& f : c.second) n += (int)f.second.size(); return n; }
static void pe_reset() { pe_edit.clear(); pe_s = -100; }
static void pe_load_char(int s) {
    pe_s = s;
    pe_base.clear();
    pe_cols_now = pe_cols(s, &pe_game);
    for (auto& c : pe_cols_now) for (auto& x : pe_files) { std::vector<uint8_t> d; if (pe_read(c, x.first, d)) pe_base[c.j][x.first] = d; }
    bool ok = false;
    for (auto& c : pe_cols_now) ok = ok || c.j == pe_col;
    if (!ok) pe_col = -1;
}
static int pe_items(int file) {
    PeFile& pf = pe_files[file];
    auto* b = pe_file(pe_vcol(), file);
    int sz = b ? (int)b->size() : 0, st = pf.step[pe_game];
    if (pf.by_char) return st > 0 ? sz / st : 0;
    if (st > 0) return std::min((int)pf.items.size(), sz / st);
    return (int)pf.items.size();
}
static std::wstring pe_item_name(int file, int i) { PeFile& pf = pe_files[file]; return pf.by_char ? mw_w(re_bt3_name(i)) : i < (int)pf.items.size() ? pf.items[i] : L"Entry " + std::to_wstring(i + 1); }
static std::wstring pe_char_name(long long v) { return v == 255 || v < 0 ? L"None" : v < roster::SYS ? mw_w(re_bt3_name((int)v)) : L"Id " + std::to_wstring(v); }

// ---------------- card view (painted; menus, the character picker and an inline box for numbers) ----------------
enum { EL_CARD, EL_LABEL, EL_NOTE, EL_CHAR, EL_LIST, EL_NUM, EL_TOGGLE, EL_SWATCH, EL_CHIP, EL_SLIDER, EL_EXPAND, EL_COST };
struct PeEl { int kind = 0, x = 0, y = 0, w = 0, h = 0, field = -1, item = 0, bit = -1, tag = 0; double lo = 0, hi = 0; std::wstring t, hint; bool on = false; };
static std::vector<PeEl> pe_els;
static std::vector<int> pe_card_y;   // y of each card (jump buttons)
static HWND pe_view = nullptr, pe_box = nullptr;
static int pe_hot = -1, pe_drag = -1, pe_scroll = 0, pe_content = 0, pe_box_el = -1, pe_attack = 0, pe_rival = 0, pe_adv_file = 17, pe_adv_item = 0;
static bool pe_more_tf = false, pe_more_fu = false, pe_adv_open = false, pe_tracking = false;
static const wchar_t* PE_CARDS[7] = {L"Transformations", L"Fusions", L"Aura", L"Attacks", L"Movement", L"Interactions", L"Advanced"};
static const COLORREF PE_AURA[11] = {RGB(241, 239, 232), RGB(127, 119, 221), RGB(239, 159, 39), RGB(226, 75, 74), RGB(99, 153, 34), RGB(55, 138, 221), RGB(212, 83, 126), RGB(43, 79, 176), RGB(245, 208, 74), RGB(232, 183, 48), RGB(192, 57, 43)};
static const int PE_W = 690;

static std::wstring pe_val_text(const PeField& x, int item, long long v) {
    if (x.type == PT_F32) { wchar_t b[64]; swprintf(b, 64, L"%.6g", pe_tof(v)); return b; }
    if (x.type == PT_HITS) return std::to_wstring(v + 1);
    if (x.type == PT_MIN1) return std::to_wstring(v < 1 ? 1 : v);
    if (x.type == PT_CHAR) return pe_char_name(v);
    if (x.type == PT_LIST || x.type == PT_LIST16 || x.type == PT_LIST32) {
        for (auto& o : x.opt) { if (o.v == PE_CHARS) { if (v >= 0 && v < roster::SYS) return pe_char_name(v); continue; } if (o.v == v) return o.t; }
        return L"Value " + std::to_wstring(v);
    }
    return std::to_wstring(v);
}
static std::wstring pe_cost_text(long long v) { return v == 0 ? L"No cost" : std::to_wstring(v) + (v == 1 ? L" bar" : L" bars"); }

// layout helpers
static int pe_y = 0, pe_card_top = 0;
static PeEl& pe_add(int kind, int x, int y, int w, int h, const std::wstring& t = L"", int field = -1, int item = 0) {
    PeEl e; e.kind = kind; e.x = x; e.y = y; e.w = w; e.h = h; e.t = t; e.field = field; e.item = item;
    pe_els.push_back(e);
    return pe_els.back();
}
static void pe_card_begin(int k, const std::wstring& hint = L"", int expand_tag = 0, bool open = false) {
    pe_card_top = pe_y;
    pe_card_y.push_back(pe_y);
    PeEl& c = pe_add(EL_CARD, 0, pe_y, PE_W, 0, PE_CARDS[k]);
    c.hint = hint;
    if (expand_tag) { PeEl& e = pe_add(EL_EXPAND, PE_W - 150, pe_y + 10, 136, 24, expand_tag == 3 ? (open ? L"Hide" : L"Show") : open ? L"Fewer options" : L"More options"); e.tag = expand_tag; e.on = open; }
    pe_y += 46;
}
static void pe_card_end() {
    for (auto& e : pe_els) if (e.kind == EL_CARD && e.y == pe_card_top) e.h = pe_y - pe_card_top + 6;
    pe_y += 18;
}
// one labelled row: label at the left, control after it
static void pe_row(const std::wstring& label, int fi, int item, int kind, int cw = 260, double lo = 0, double hi = 0) {
    if (fi < 0 || !pe_fits(pe_f[fi], item)) return;
    PeEl& l = pe_add(EL_LABEL, 18, pe_y, 170, 28, label, fi, item);
    l.on = pe_varies(pe_f[fi], item);
    if (kind == EL_SLIDER) {
        PeEl& s = pe_add(EL_SLIDER, 196, pe_y, cw - 96, 28, L"", fi, item); s.lo = lo; s.hi = hi;
        pe_add(EL_NUM, 196 + cw - 88, pe_y + 1, 88, 26, L"", fi, item);
    } else pe_add(kind, 196, pe_y + 1, cw, 26, L"", fi, item);
    pe_y += 34;
}
static void pe_toggles(const std::vector<std::tuple<const char*, int, const wchar_t*>>& v, int item) {
    int x = 18;
    for (auto& t : v) {
        int fi = pe_fk(std::get<0>(t));
        if (fi < 0 || !pe_fits(pe_f[fi], item)) continue;
        int w = 34 + (int)wcslen(std::get<2>(t)) * 7;
        if (x + w > PE_W - 14) { x = 18; pe_y += 30; }
        PeEl& e = pe_add(EL_TOGGLE, x, pe_y, w, 26, std::get<2>(t), fi, item);
        e.bit = std::get<1>(t);
        x += w + 14;
    }
    pe_y += 34;
}
static void pe_layout() {
    pe_els.clear(); pe_card_y.clear();
    pe_y = 0;
    char k[32];
    // transformations
    pe_card_begin(0, L"", 1, pe_more_tf);
    for (int i = 1; i <= 4; i++) {
        snprintf(k, sizeof(k), "tf%d", i);
        int fi = pe_fk(k);
        if (fi < 0) continue;
        pe_add(EL_LABEL, 18, pe_y, 150, 28, L"Transformation " + std::to_wstring(i), fi);
        pe_add(EL_CHAR, 196, pe_y + 1, 300, 26, L"", fi);
        snprintf(k, sizeof(k), "tf%d_cost", i);
        int fc = pe_fk(k);
        if (fc >= 0) pe_add(EL_COST, 506, pe_y + 1, 150, 26, L"", fc);
        pe_y += 34;
        if (pe_more_tf) {
            for (auto& d : {std::make_pair("_kind", L"Kind"), std::make_pair("_aura", L"Aura effect"), std::make_pair("_assist", L"Assist")}) {
                snprintf(k, sizeof(k), "tf%d%s", i, d.first);
                int f2 = pe_fk(k);
                if (f2 < 0) continue;
                PeEl& l = pe_add(EL_NOTE, 40, pe_y, 148, 26, d.second, f2);
                l.on = pe_varies(pe_f[f2], 0);
                pe_add(pe_f[f2].type == PT_CHAR ? EL_CHAR : EL_LIST, 196, pe_y + 1, 300, 24, L"", f2);
                pe_y += 30;
            }
            pe_y += 6;
        }
    }
    pe_row(L"Next form", pe_fk("tf_next"), 0, EL_LIST, 300);
    if (pe_more_tf) { int fb = pe_fk("tf_bonus"); if (fb >= 0) { pe_add(EL_NOTE, 18, pe_y, 300, 22, L"Bonus when transforming or fusing"); pe_y += 26; std::vector<std::tuple<const char*, int, const wchar_t*>> v; for (int b = 0; b < 8 && b < (int)pe_f[fb].bit.size(); b++) v.emplace_back("tf_bonus", b, pe_f[fb].bit[b].c_str()); pe_toggles(v, 0); } }
    pe_card_end();
    // fusions
    pe_card_begin(1, L"", 2, pe_more_fu);
    for (int i = 1; i <= 3; i++) {
        snprintf(k, sizeof(k), "fu%d", i);
        int fr = pe_fk(k);
        if (fr < 0) continue;
        pe_add(EL_LABEL, 18, pe_y, 80, 28, L"Fusion " + std::to_wstring(i), fr);
        pe_add(EL_CHAR, 100, pe_y + 1, 190, 26, L"", fr);
        snprintf(k, sizeof(k), "fu%d_type", i);
        int ft = pe_fk(k);
        if (ft >= 0) pe_add(EL_LIST, 298, pe_y + 1, 112, 26, L"", ft);
        pe_add(EL_NOTE, 418, pe_y, 34, 28, L"with");
        snprintf(k, sizeof(k), pe_game ? "fu%d_t1" : "fu%d_p", i);
        int fp = pe_fk(k);
        if (fp >= 0) pe_add(EL_CHAR, 452, pe_y + 1, 120, 26, L"", fp);
        snprintf(k, sizeof(k), "fu%d_cost", i);
        int fc = pe_fk(k);
        if (fc >= 0) pe_add(EL_COST, 580, pe_y + 1, 96, 26, L"", fc);
        pe_y += 34;
        if (pe_more_fu) {
            const char* more[4][2] = {{"fu%d_t2", "Partner, 2nd form"}, {"fu%d_t3", "Partner, 3rd form"}, {"fu%d_sup", "Partner support"}, {"", ""}};
            const char* more3[4][2] = {{"fu%d_p2", "Partner, 2nd form"}, {"fu%d_p3", "Partner, 3rd form"}, {"fu%d_p4", "Partner, 4th form"}, {"", ""}};
            for (int m = 0; m < 3; m++) {
                snprintf(k, sizeof(k), (pe_game ? more : more3)[m][0], i);
                int f2 = pe_fk(k);
                if (f2 < 0) continue;
                PeEl& l = pe_add(EL_NOTE, 40, pe_y, 150, 26, mw_w((pe_game ? more : more3)[m][1]), f2);
                l.on = pe_varies(pe_f[f2], 0);
                pe_add(pe_f[f2].type == PT_CHAR ? EL_CHAR : EL_LIST, 196, pe_y + 1, 300, 24, L"", f2);
                pe_y += 30;
            }
            pe_y += 6;
        }
    }
    pe_card_end();
    // aura
    pe_card_begin(2);
    {
        int fa = pe_fk("aura");
        if (fa >= 0 && pe_fits(pe_f[fa], 0)) {
            long long v = pe_get(pe_f[fa], 0);
            for (int i = 0; i < 11; i++) { PeEl& e = pe_add(EL_SWATCH, 18 + 36 * i, pe_y, 28, 28, L"", fa); e.tag = i; e.on = v == i; }
            PeEl& n = pe_add(EL_LABEL, 18 + 36 * 11 + 8, pe_y, 220, 28, pe_val_text(pe_f[fa], 0, v), fa);
            n.on = pe_varies(pe_f[fa], 0);
            pe_y += 40;
        }
    }
    pe_card_end();
    // attacks
    if (pe_items(23) > 0) {
        pe_card_begin(3);
        int n = pe_items(23);
        if (pe_attack >= n) pe_attack = 0;
        int x = 18;
        for (int i = 0; i < n; i++) { std::wstring t = pe_item_name(23, i); int w = 26 + (int)t.size() * 7; PeEl& c = pe_add(EL_CHIP, x, pe_y, w, 28, t); c.tag = 23; c.item = i; c.on = i == pe_attack; x += w + 8; }
        pe_y += 42;
        int a = pe_attack;
        pe_row(L"Type", pe_fk("a13e"), a, EL_LIST, 300);
        pe_row(L"Damage", pe_fk("a184"), a, EL_SLIDER, 460, 0, 20000);
        pe_row(L"Damage when blocked", pe_fk("a190"), a, EL_SLIDER, 460, 0, 20000);
        pe_row(L"Ki cost", pe_fk("a19c"), a, EL_SLIDER, 460, 0, 200000);
        pe_row(L"Hits", pe_fk("a09c"), a, EL_NUM, 88);
        pe_toggles({{"af02", 5, L"Destroys planet"}, {"af0e", 1, L"Minigame"}, {"af0e", 3, L"Breaks rival costume"}, {"af0d", 5, L"Breaks own costume"},
                    {"af0d", 6, L"Rival falls to the ground"}, {"af0c", 0, L"Unblockable"}, {"af0d", 7, L"Kaioken effect"}}, a);
        pe_row(L"Rush speed", pe_fk("a1fc"), a, EL_SLIDER, 460, 0, 1000);
        pe_row(L"Rush distance", pe_fk("a024"), a, EL_SLIDER, 460, 0, 100);
        pe_row(L"Partner", pe_fk(pe_game ? "apt208" : "ap208"), a, EL_LIST, 300);
        pe_row(L"Partner, 2nd color", pe_fk(pe_game ? "apt20e" : "ap20e"), a, EL_LIST, 300);
        pe_card_end();
    } else pe_card_y.push_back(pe_y);
    // movement
    pe_card_begin(4);
    pe_row(L"Movement speed", pe_fk("v00"), 0, EL_SLIDER, 460, 0, 100);
    pe_row(L"Dash speed", pe_fk("v10"), 0, EL_SLIDER, 460, 0, 300);
    pe_row(L"Dragon dash speed", pe_fk("v18"), 0, EL_SLIDER, 460, 0, 400);
    pe_row(L"Rush-in distance", pe_fk("v28"), 0, EL_SLIDER, 460, 0, 100);
    pe_row(L"Jump height", pe_fk("v5c"), 0, EL_SLIDER, 460, 0, 20);
    pe_row(L"Z-Burst dash speed", pe_fk("v4c"), 0, EL_SLIDER, 460, 0, 800);
    pe_card_end();
    // interactions
    pe_card_begin(5, L"Lines against a rival in the intro and at victory");
    {
        int fi = pe_fk("vs_intro"), fw = pe_fk("vs_win"), n = pe_items(21);
        if (fi >= 0 && n > 0) {
            if (pe_rival < 0 || pe_rival >= n) {   // the first rival with a line of its own
                pe_rival = 0;
                for (int i = 0; i < n; i++) if (pe_get(pe_f[fi], i) != 255 || (fw >= 0 && pe_get(pe_f[fw], i) != 255)) { pe_rival = i; break; }
            }
            int x = 18, shown = 0;
            for (int i = 0; i < n && shown < 24; i++) {
                if (pe_get(pe_f[fi], i) == 255 && (fw < 0 || pe_get(pe_f[fw], i) == 255) && i != pe_rival) continue;
                std::wstring t = pe_item_name(21, i);
                int w = 26 + (int)t.size() * 7;
                if (x + w > PE_W - 14) { x = 18; pe_y += 34; }
                PeEl& c = pe_add(EL_CHIP, x, pe_y, w, 28, t); c.tag = 21; c.item = i; c.on = i == pe_rival;
                x += w + 8; shown++;
            }
            int w = 120;
            if (x + w > PE_W - 14) { x = 18; pe_y += 34; }
            PeEl& add = pe_add(EL_CHIP, x, pe_y, w, 28, L"+ Other rival"); add.tag = 210;
            pe_y += 42;
            pe_row(L"Intro line", fi, pe_rival, EL_NUM, 88);
            pe_toggles({{"vs_first", 0, L"Rival talks first"}}, pe_rival);
            pe_row(L"Victory line", fw, pe_rival, EL_NUM, 88);
        } else { pe_add(EL_NOTE, 18, pe_y, 600, 26, L"This character has no interactions file."); pe_y += 32; }
    }
    pe_card_end();
    // advanced
    pe_card_begin(6, L"Every parameter, by file", 3, pe_adv_open);
    if (pe_adv_open) {
        if (!pe_file(pe_vcol(), pe_adv_file)) for (auto& x : pe_files) if (pe_file(pe_vcol(), x.first)) { pe_adv_file = x.first; break; }
        PeEl& f = pe_add(EL_LIST, 18, pe_y, 260, 26, L""); f.tag = 300;
        int n = pe_items(pe_adv_file);
        if (pe_adv_item >= n) pe_adv_item = 0;
        if (n > 0) { PeEl& it = pe_add(EL_LIST, 288, pe_y, 380, 26, L""); it.tag = 301; }
        pe_y += 40;
        std::string grp = "\x01";
        for (int fi = 0; fi < (int)pe_f.size(); fi++) {
            const PeField& x = pe_f[fi];
            if (x.file != pe_adv_file || !pe_fits(x, pe_adv_item)) continue;
            if (x.grp != grp) { grp = x.grp; pe_add(EL_NOTE, 18, pe_y + 4, 400, 22, mw_w(grp)).tag = 1; pe_y += 30; }
            if (x.type == PT_BITS) {
                std::vector<std::tuple<const char*, int, const wchar_t*>> v;
                for (int b = 0; b < 8; b++) {
                    const wchar_t* t = b < (int)x.bit.size() && x.bit[b] != L"*" ? x.bit[b].c_str() : nullptr;
                    static std::wstring tmp[8];
                    if (!t) { tmp[b] = x.label + L" " + std::to_wstring(b); t = tmp[b].c_str(); }
                    v.emplace_back(x.key.c_str(), b, t);
                }
                int x0 = 18;
                for (auto& t : v) {
                    int w = 34 + (int)wcslen(std::get<2>(t)) * 7;
                    if (x0 + w > PE_W - 14) { x0 = 18; pe_y += 30; }
                    PeEl& e = pe_add(EL_TOGGLE, x0, pe_y, w, 26, std::get<2>(t), fi, pe_adv_item);
                    e.bit = std::get<1>(t);
                    e.hint = e.t;
                    x0 += w + 14;
                }
                pe_y += 34;
                continue;
            }
            int kind = x.type == PT_LIST || x.type == PT_LIST16 || x.type == PT_LIST32 ? EL_LIST : x.type == PT_CHAR ? EL_CHAR : x.type == PT_CHECK ? EL_TOGGLE : EL_NUM;
            if (kind == EL_TOGGLE) { PeEl& e = pe_add(EL_TOGGLE, 18, pe_y, 600, 26, x.label, fi, pe_adv_item); e.bit = -1; pe_y += 32; continue; }
            PeEl& l = pe_add(EL_LABEL, 18, pe_y, 330, 28, x.label, fi, pe_adv_item);
            l.on = pe_varies(x, pe_adv_item);
            pe_add(kind, 356, pe_y + 1, kind == EL_NUM ? 120 : 312, 26, L"", fi, pe_adv_item);
            pe_y += 32;
        }
    }
    pe_card_end();
    pe_content = pe_y;
}

// painting
static void pe_round(HDC dc, int x, int y, int w, int h, int r, COLORREF fill, COLORREF line) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, line);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, x, y, x + w, y + h, r, r);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(p);
}
static void pe_text(HDC dc, const std::wstring& t, RECT r, COLORREF c, HFONT f, UINT fl = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS) {
    SetTextColor(dc, c);
    HGDIOBJ of = SelectObject(dc, f);
    DrawTextW(dc, t.c_str(), -1, &r, fl);
    SelectObject(dc, of);
}
static const COLORREF PE_CARD = RGB(33, 38, 47), PE_PILL = RGB(42, 48, 60), PE_PILL_HOT = RGB(52, 60, 75);
static void pe_paint(HDC dc, RECT cr) {
    FillRect(dc, &cr, theme_b_bg);
    SetBkMode(dc, TRANSPARENT);
    int sy = pe_scroll;
    for (int i = 0; i < (int)pe_els.size(); i++) {
        const PeEl& e = pe_els[i];
        int x = E(e.x), y = E(e.y) - sy, w = E(e.w), h = E(e.h);
        if (y > cr.bottom || y + h < 0) continue;
        bool hot = i == pe_hot;
        const PeField* f = e.field >= 0 ? &pe_f[e.field] : nullptr;
        RECT r = {x, y, x + w, y + h};
        switch (e.kind) {
            case EL_CARD: {
                pe_round(dc, x, y, w, h, E(14), PE_CARD, THEME_BORDER);
                RECT t = {x + E(18), y + E(10), x + w - E(18), y + E(36)};
                pe_text(dc, e.t, t, THEME_TEXT, theme_f_bold);
                if (!e.hint.empty()) { RECT hr = {x + E(180), y + E(10), x + w - E(170), y + E(36)}; pe_text(dc, e.hint, hr, THEME_TEXT2, theme_f_normal); }
                break;
            }
            case EL_LABEL: case EL_NOTE: {
                bool ch = f && (e.bit >= 0 ? pe_changed(*f, e.item, e.bit) : pe_changed(*f, e.item));
                COLORREF c = e.tag == 1 ? THEME_TEXT : ch ? THEME_ACCENT : e.kind == EL_LABEL ? THEME_TEXT : THEME_TEXT2;
                if (!e.on) { pe_text(dc, e.t, r, c, e.tag == 1 ? theme_f_bold : theme_f_normal); break; }
                RECT a = {r.left, r.top - E(3), r.right, r.top + E(15)}, v = {r.left, r.top + E(13), r.right, r.top + E(31)};
                pe_text(dc, e.t, a, c, theme_f_normal);
                pe_text(dc, L"varies by color", v, THEME_LINK, theme_f_normal);
                break;
            }
            case EL_CHAR: case EL_LIST: case EL_COST: {
                pe_round(dc, x, y, w, h, E(10), hot ? PE_PILL_HOT : PE_PILL, hot ? THEME_ACCENT : THEME_BORDER);
                std::wstring t;
                if (e.tag == 300) t = std::to_wstring(pe_adv_file) + L"  " + pe_files[pe_adv_file].title;
                else if (e.tag == 301) t = pe_files[pe_adv_file].item + L":  " + pe_item_name(pe_adv_file, pe_adv_item);
                else if (f) { long long v = pe_get(*f, e.item); t = e.kind == EL_COST ? pe_cost_text(v) : pe_val_text(*f, e.item, v); }
                RECT tr = {x + E(12), y, x + w - E(26), y + h};
                pe_text(dc, t, tr, f && pe_changed(*f, e.item) ? THEME_ACCENT : THEME_TEXT, theme_f_normal);
                RECT ar = {x + w - E(24), y, x + w - E(8), y + h};
                pe_text(dc, L"\x25BE", ar, THEME_TEXT2, theme_f_normal, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                break;
            }
            case EL_NUM: {
                pe_round(dc, x, y, w, h, E(8), THEME_FIELD, hot ? THEME_ACCENT : THEME_BORDER);
                if (i == pe_box_el) break;
                long long v = pe_get(*f, e.item);
                std::wstring t = (f->key == "vs_intro" || f->key == "vs_win") && v == 255 ? L"None" : pe_val_text(*f, e.item, v);
                RECT tr = {x + E(10), y, x + w - E(8), y + h};
                pe_text(dc, t, tr, pe_changed(*f, e.item) ? THEME_ACCENT : THEME_TEXT, theme_f_normal);
                break;
            }
            case EL_TOGGLE: {
                long long v = pe_get(*f, e.item);
                bool on = e.bit >= 0 ? ((v >> e.bit) & 1) : v != 0;
                int b = E(16), by = y + (h - b) / 2;
                pe_round(dc, x, by, b, b, E(6), on ? THEME_ACCENT : THEME_FIELD, on ? THEME_ACCENT : hot ? THEME_ACCENT : THEME_BORDER);
                if (on) { RECT cr2 = {x, by, x + b, by + b}; pe_text(dc, L"\x2713", cr2, RGB(24, 18, 8), theme_f_bold, DT_CENTER | DT_VCENTER | DT_SINGLELINE); }
                bool ch = e.bit >= 0 ? pe_changed(*f, e.item, e.bit) : pe_changed(*f, e.item);
                RECT tr = {x + b + E(8), y, x + w, y + h};
                pe_text(dc, e.t, tr, ch ? THEME_ACCENT : THEME_TEXT, theme_f_normal);
                break;
            }
            case EL_SWATCH: {
                int d = E(24), cx = x + (w - d) / 2, cy = y + (h - d) / 2;
                if (e.on || hot) { HPEN p = CreatePen(PS_SOLID, E(2), e.on ? THEME_ACCENT : THEME_TEXT2); HGDIOBJ op = SelectObject(dc, p), ob = SelectObject(dc, GetStockObject(NULL_BRUSH)); Ellipse(dc, cx - E(3), cy - E(3), cx + d + E(3), cy + d + E(3)); SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(p); }
                HBRUSH br = CreateSolidBrush(PE_AURA[e.tag]); HPEN p = CreatePen(PS_SOLID, 1, THEME_BORDER);
                HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, p);
                Ellipse(dc, cx, cy, cx + d, cy + d);
                SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(br); DeleteObject(p);
                break;
            }
            case EL_CHIP: {
                pe_round(dc, x, y, w, h, E(14), e.on ? THEME_ACCENT : hot ? PE_PILL_HOT : PE_PILL, e.on ? THEME_ACCENT : THEME_BORDER);
                pe_text(dc, e.t, r, e.on ? RGB(24, 18, 8) : THEME_TEXT, e.on ? theme_f_bold : theme_f_normal, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                break;
            }
            case EL_SLIDER: {
                double v = f->type == PT_F32 ? pe_tof(pe_get(*f, e.item)) : (double)pe_get(*f, e.item);
                double t = e.hi > e.lo ? std::max(0.0, std::min(1.0, (v - e.lo) / (e.hi - e.lo))) : 0;
                int ty = y + h / 2 - E(2), kx = x + E(8) + (int)((w - E(16)) * t);
                pe_round(dc, x + E(8), ty, w - E(16), E(4), E(4), RGB(58, 66, 82), RGB(58, 66, 82));
                pe_round(dc, x + E(8), ty, kx - x - E(8) + 1, E(4), E(4), THEME_ACCENT, THEME_ACCENT);
                int kd = E(hot || pe_drag == i ? 16 : 14);
                HBRUSH br = CreateSolidBrush(THEME_TEXT); HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, GetStockObject(NULL_PEN));
                Ellipse(dc, kx - kd / 2, y + h / 2 - kd / 2, kx + kd / 2, y + h / 2 + kd / 2);
                SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(br);
                break;
            }
            case EL_EXPAND: {
                RECT tr = r;
                pe_text(dc, e.t + (e.on ? L"  \x25B4" : L"  \x25BE"), tr, hot ? THEME_ACCENT : THEME_TEXT2, theme_f_normal, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                break;
            }
        }
    }
}

// interaction
static void pe_side_info();
static void pe_changed_now() { pe_layout(); InvalidateRect(pe_view, nullptr, FALSE); pe_side_info(); }
static void pe_scrollbar() {
    RECT r; GetClientRect(pe_view, &r);
    int total = E(pe_content);
    pe_scroll = std::max(0, std::min(pe_scroll, std::max(0, total - (int)r.bottom)));
    SCROLLINFO si; si.cbSize = sizeof(si); si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0; si.nMax = std::max(0, total - 1); si.nPage = r.bottom; si.nPos = pe_scroll;
    SetScrollInfo(pe_view, SB_VERT, &si, TRUE);
}
static void pe_box_close(bool keep) {
    if (!pe_box) return;
    HWND b = pe_box; pe_box = nullptr;
    int i = pe_box_el; pe_box_el = -1;
    if (keep && i >= 0 && i < (int)pe_els.size() && pe_els[i].field >= 0) {
        const PeField& x = pe_f[pe_els[i].field];
        wchar_t t[64] = L""; GetWindowTextW(b, t, 64);
        std::wstring s = t;
        while (!s.empty() && s.back() == L' ') s.pop_back();
        wchar_t* end = nullptr;
        if ((x.key == "vs_intro" || x.key == "vs_win") && (s.empty() || s == L"None")) pe_put(x, pe_els[i].item, 255);
        else if (x.type == PT_F32) { float v = wcstof(s.c_str(), &end); if (end != s.c_str() && !*end) { uint32_t u; memcpy(&u, &v, 4); pe_put(x, pe_els[i].item, u); } }
        else {
            long long v = wcstoll(s.c_str(), &end, 10);
            if (end != s.c_str() && !*end) {
                if (x.type == PT_HITS) v -= 1;
                static const long long LO[13] = {0, -128, -32768, INT32_MIN, 0, 0, 0, 0, INT32_MIN, 0, 0, 1, 0}, HI[13] = {255, 127, 32767, INT32_MAX, 0, 255, 255, 65535, INT32_MAX, 255, 255, 255, 255};
                pe_put(x, pe_els[i].item, (uint32_t)std::max(LO[x.type], std::min(HI[x.type], v)));
            }
        }
    }
    DestroyWindow(b);
    pe_changed_now();
}
static bool pe_key_msg(const MSG& m) {   // Enter keeps, Esc cancels the number box (before the creator sees them)
    if (!pe_box || m.hwnd != pe_box || m.message != WM_KEYDOWN) return false;
    if (m.wParam == VK_RETURN) { pe_box_close(true); return true; }
    if (m.wParam == VK_ESCAPE) { pe_box_close(false); return true; }
    return false;
}
static void pe_set_slider(int i, int mx) {
    PeEl& e = pe_els[i];
    const PeField& x = pe_f[e.field];
    double t = std::max(0.0, std::min(1.0, (mx - E(e.x + 8)) / (double)std::max(1, E(e.w - 16))));
    double v = e.lo + (e.hi - e.lo) * t;
    if (x.type == PT_F32) { float fv = (float)(std::round(v * 10) / 10); uint32_t u; memcpy(&u, &fv, 4); pe_put(x, e.item, u); }
    else { long long step = e.hi - e.lo >= 20000 ? 10 : 1; pe_put(x, e.item, (uint32_t)(long long)(std::llround(v / step) * step)); }
    InvalidateRect(pe_view, nullptr, FALSE);
}
static void pe_menu_list(int i, POINT p) {
    PeEl& e = pe_els[i];
    HMENU m = CreatePopupMenu();
    std::vector<long long> vals;
    if (e.tag == 300) { for (auto& x : pe_files) if (pe_file(pe_vcol(), x.first)) { vals.push_back(x.first); AppendMenuW(m, MF_STRING | (x.first == pe_adv_file ? MF_CHECKED : 0), vals.size(), (std::to_wstring(x.first) + L"  " + x.second.title).c_str()); } }
    else if (e.tag == 301) { int n = pe_items(pe_adv_file); for (int k = 0; k < n; k++) { vals.push_back(k); AppendMenuW(m, MF_STRING | (k == pe_adv_item ? MF_CHECKED : 0) | (k && k % 30 == 0 ? MF_MENUBARBREAK : 0), vals.size(), pe_item_name(pe_adv_file, k).c_str()); } }
    else {
        const PeField& x = pe_f[e.field];
        long long cur = pe_get(x, e.item);
        std::vector<PeOpt> o;
        if (e.kind == EL_COST) for (int k = 0; k <= 7; k++) o.push_back({pe_cost_text(k), k});
        else for (auto& op : x.opt) { if (op.v != PE_CHARS) o.push_back(op); else for (int k = 0; k < roster::SYS; k++) o.push_back({pe_char_name(k), k}); }
        int col = 0;
        for (auto& op : o) { vals.push_back(op.v); AppendMenuW(m, MF_STRING | (op.v == cur ? MF_CHECKED : 0) | (col && col % 30 == 0 ? MF_MENUBARBREAK : 0), vals.size(), op.t.c_str()); col++; }
    }
    int r = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, pe_view, nullptr);
    DestroyMenu(m);
    if (r <= 0) return;
    long long v = vals[r - 1];
    if (e.tag == 300) { pe_adv_file = (int)v; pe_adv_item = 0; }
    else if (e.tag == 301) pe_adv_item = (int)v;
    else pe_put(pe_f[e.field], e.item, (uint32_t)v);
    pe_changed_now();
}
static int pe_pick_char(const std::wstring& title, long long cur, POINT p) {   // -1 cancelled
    HMENU m = CreatePopupMenu(), by = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 1, L"Choose a character...");
    AppendMenuW(m, MF_STRING | (cur == 255 ? MF_CHECKED : 0), 2, L"None");
    for (int g = 0; g < roster::SYS; g += 20) {
        HMENU sub = CreatePopupMenu();
        for (int k = g; k < std::min(g + 20, roster::SYS); k++) AppendMenuW(sub, MF_STRING | (k == cur ? MF_CHECKED : 0), 100 + k, (std::to_wstring(k) + L"   " + pe_char_name(k)).c_str());
        AppendMenuW(by, MF_POPUP, (UINT_PTR)sub, (std::to_wstring(g) + L" to " + std::to_wstring(std::min(g + 19, roster::SYS - 1))).c_str());
    }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)by, L"By BT3 id");
    int r = TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, pe_view, nullptr);
    DestroyMenu(m);
    if (r == 2) return 255;
    if (r >= 100) return r - 100;
    if (r != 1) return -1;
    std::vector<int> v;
    for (auto& c : re_chars) v.push_back(c.sysid);
    for (int s : roster::VANILLA) v.push_back(s);
    int s = re_picker(title, v);
    return s >= 0 && s < roster::SYS ? s : -1;
}
static void pe_click(int i, POINT scr, int mx) {
    PeEl& e = pe_els[i];
    switch (e.kind) {
        case EL_CHAR: {
            int s = pe_pick_char(L"Character", pe_get(pe_f[e.field], e.item), scr);
            if (s >= 0) { pe_put(pe_f[e.field], e.item, (uint32_t)s); pe_changed_now(); }
            break;
        }
        case EL_LIST: case EL_COST: pe_menu_list(i, scr); break;
        case EL_TOGGLE: {
            const PeField& x = pe_f[e.field];
            uint32_t v = (uint32_t)pe_get(x, e.item);
            if (e.bit >= 0) v ^= 1u << e.bit; else v = v ? 0 : (uint32_t)x.chk;
            pe_put(x, e.item, v);
            pe_changed_now();
            break;
        }
        case EL_SWATCH: pe_put(pe_f[e.field], 0, (uint32_t)e.tag); pe_changed_now(); break;
        case EL_CHIP:
            if (e.tag == 23) pe_attack = e.item;
            else if (e.tag == 21) pe_rival = e.item;
            else if (e.tag == 210) { int s = pe_pick_char(L"Rival", -1, scr); if (s >= 0 && s < 255 && s < pe_items(21)) pe_rival = s; }
            pe_changed_now();
            break;
        case EL_EXPAND:
            if (e.tag == 1) pe_more_tf = !pe_more_tf; else if (e.tag == 2) pe_more_fu = !pe_more_fu; else pe_adv_open = !pe_adv_open;
            pe_changed_now();
            break;
        case EL_SLIDER: pe_drag = i; SetCapture(pe_view); pe_set_slider(i, mx); break;
        case EL_NUM: {
            const PeField& x = pe_f[e.field];
            long long v = pe_get(x, e.item);
            std::wstring t = (x.key == "vs_intro" || x.key == "vs_win") && v == 255 ? L"None" : pe_val_text(x, e.item, v);
            pe_box_el = i;
            pe_box = CreateWindowExW(0, L"EDIT", t.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, E(e.x + 8), E(e.y + 4) - pe_scroll, E(e.w - 14), E(e.h - 8), pe_view, nullptr, GetModuleHandle(nullptr), nullptr);
            SendMessageW(pe_box, WM_SETFONT, (WPARAM)theme_f_normal, TRUE);
            SendMessageW(pe_box, EM_SETSEL, 0, -1);
            SetFocus(pe_box);
            InvalidateRect(pe_view, nullptr, FALSE);
            break;
        }
    }
}
static int pe_hit_at(int mx, int my) {
    int lx = (int)(mx / theme_scale), ly = (int)((my + pe_scroll) / theme_scale);
    for (int i = (int)pe_els.size() - 1; i >= 0; i--) {
        const PeEl& e = pe_els[i];
        if (e.kind == EL_CARD || e.kind == EL_LABEL || e.kind == EL_NOTE) continue;
        if (lx >= e.x && lx < e.x + e.w && ly >= e.y && ly < e.y + e.h) return i;
    }
    return -1;
}
static LRESULT CALLBACK pe_view_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
            RECT r; GetClientRect(h, &r);
            HDC md = CreateCompatibleDC(dc);
            HBITMAP bm = CreateCompatibleBitmap(dc, r.right, r.bottom);
            HGDIOBJ ob = SelectObject(md, bm);
            pe_paint(md, r);
            BitBlt(dc, 0, 0, r.right, r.bottom, md, 0, 0, SRCCOPY);
            SelectObject(md, ob); DeleteObject(bm); DeleteDC(md);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: pe_scrollbar(); return 0;
        case WM_MOUSEWHEEL: {
            pe_box_close(true);
            pe_scroll -= GET_WHEEL_DELTA_WPARAM(w) * E(80) / WHEEL_DELTA;
            pe_scrollbar(); InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_VSCROLL: {
            pe_box_close(true);
            SCROLLINFO si; si.cbSize = sizeof(si); si.fMask = SIF_ALL; GetScrollInfo(h, SB_VERT, &si);
            switch (LOWORD(w)) {
                case SB_LINEUP: pe_scroll -= E(34); break;
                case SB_LINEDOWN: pe_scroll += E(34); break;
                case SB_PAGEUP: pe_scroll -= (int)si.nPage; break;
                case SB_PAGEDOWN: pe_scroll += (int)si.nPage; break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: pe_scroll = si.nTrackPos; break;
            }
            pe_scrollbar(); InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int mx = GET_X_LPARAM(l), my = GET_Y_LPARAM(l);
            if (pe_drag >= 0) { pe_set_slider(pe_drag, mx); return 0; }
            int hit = pe_hit_at(mx, my);
            if (hit != pe_hot) { pe_hot = hit; InvalidateRect(h, nullptr, FALSE); SetCursor(LoadCursor(nullptr, hit >= 0 ? IDC_HAND : IDC_ARROW)); }
            if (!pe_tracking) { TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0}; TrackMouseEvent(&t); pe_tracking = true; }
            return 0;
        }
        case WM_SETCURSOR: if (LOWORD(l) == HTCLIENT) { SetCursor(LoadCursor(nullptr, pe_hot >= 0 ? IDC_HAND : IDC_ARROW)); return TRUE; } break;
        case WM_MOUSELEAVE: pe_tracking = false; if (pe_hot >= 0) { pe_hot = -1; InvalidateRect(h, nullptr, FALSE); } return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(h);
            pe_box_close(true);
            int mx = GET_X_LPARAM(l), my = GET_Y_LPARAM(l), hit = pe_hit_at(mx, my);
            if (hit < 0) return 0;
            const PeEl& e = pe_els[hit];
            POINT p = {E(e.x), E(e.y + e.h) - pe_scroll};
            ClientToScreen(h, &p);
            pe_click(hit, p, mx);
            return 0;
        }
        case WM_LBUTTONUP: if (pe_drag >= 0) { pe_drag = -1; ReleaseCapture(); pe_changed_now(); } return 0;
        case WM_COMMAND: if ((HWND)l == pe_box && HIWORD(w) == EN_KILLFOCUS) pe_box_close(true); return 0;
    }
    { LRESULT r; if (theme_color(m, w, l, &r)) return r; }
    return DefWindowProcW(h, m, w, l);
}
static void pe_view_create() {   // in the creator, where the cell grid is; shown only on the Params page
    WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
    wc.hInstance = GetModuleHandle(nullptr); wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = theme_b_bg;
    wc.lpfnWndProc = pe_view_proc; wc.lpszClassName = L"TTTParamView";
    RegisterClassW(&wc);
    pe_view = CreateWindowExW(0, L"TTTParamView", L"", WS_CHILD | WS_VSCROLL, E(412), E(196), E(PE_W + 20), E(476), cm, (HMENU)(INT_PTR)IDR_PVIEW, GetModuleHandle(nullptr), nullptr);
    theme_control(pe_view);
    cm_page[1].push_back(pe_view);
}

// ---------------- side panel page and saving ----------------
static void pe_side_info() {
    int s = pe_s;
    std::string t;
    if (re_find(s)) t = "Saved in this mod's character folder.";
    else t = "Original character: the changes are saved in your mod, never in the game.";
    int c = pe_count(s);
    if (c) t += "\nChanged: " + std::to_string(c) + (c == 1 ? " value byte." : " value bytes.");
    re_set_text(IDR_PINFO, t);
    re_set_text(IDR_PCNAME, pe_col < 0 ? "All colors" : "Color " + std::to_string(pe_col + 1));
    EnableWindow(GetDlgItem(cm, IDR_PUNDO), c > 0);
}
static void pe_panel(int s) {
    pe_load_spec();
    bool page = re_page == 2 && cm_tab == 1 && re_view == 0;
    bool on = s >= 0 && (re_find(s) || roster::group_vanilla(s) >= 0);
    if (page && on && s != pe_s) { pe_load_char(s); pe_scroll = 0; pe_attack = 0; pe_rival = -1; }
    bool view = page && on && s == pe_s && !pe_base.empty();
    if (view) pe_layout();
    if (pe_view) {
        ShowWindow(pe_view, view ? SW_SHOW : SW_HIDE);
        if (view) { pe_scrollbar(); InvalidateRect(pe_view, nullptr, FALSE); }
    }
    re_show(IDR_PINFO, page && s >= -1 && !view);
    for (int id : {IDR_PCPREV, IDR_PCNAME, IDR_PCNEXT, IDR_PUNDO}) re_show(id, view && (id != IDR_PCPREV && id != IDR_PCNEXT ? true : pe_cols_now.size() > 1));
    re_show(IDR_PCNAME, view);
    for (int i = 0; i < 7; i++) { re_show(IDR_PS0 + i, view); SetWindowTextW(GetDlgItem(cm, IDR_PS0 + i), PE_CARDS[i]); }
    for (int i = 7; i < 16; i++) re_show(IDR_PS0 + i, false);
    if (!page) return;
    if (s < 0) re_set_text(IDR_PINFO, s == -1 ? "Random has no parameters." : "Select a character.");
    else if (!on) re_set_text(IDR_PINFO, "");
    else if (pe_base.empty() || s != pe_s) re_set_text(IDR_PINFO, "Its colors are closed paks. Save the mod once so they are installed as folders, then edit its parameters here.");
    else pe_side_info();
}
static void pe_side_cmd(int id) {
    if (id >= IDR_PS0 && id < IDR_PS0 + 7) {
        int k = id - IDR_PS0;
        if (k == 6 && !pe_adv_open) { pe_adv_open = true; pe_layout(); }
        if (k < (int)pe_card_y.size()) { pe_scroll = E(pe_card_y[k]); pe_scrollbar(); InvalidateRect(pe_view, nullptr, FALSE); }
        return;
    }
    if (id == IDR_PCPREV || id == IDR_PCNEXT) {
        if (pe_cols_now.empty()) return;
        int n = (int)pe_cols_now.size(), cur = -1;
        for (int i = 0; i < n; i++) if (pe_cols_now[i].j == pe_col) cur = i;
        cur += id == IDR_PCNEXT ? 1 : -1;
        if (cur >= n) cur = -1;
        if (cur < -1) cur = n - 1;
        pe_col = cur < 0 ? -1 : pe_cols_now[cur].j;
        pe_changed_now();
        return;
    }
    if (id == IDR_PUNDO) { pe_edit.erase(pe_s); pe_changed_now(); }
}
static bool pe_export(const ReAdd& add, int& files, std::wstring& err) {
    for (auto& ce : pe_edit) {
        int s = ce.first;
        bool any = false;
        for (auto& c : ce.second) for (auto& f : c.second) any = any || !f.second.empty();
        if (!any || (!re_find(s) && roster::group_vanilla(s) < 0)) continue;
        auto cols = pe_cols(s, nullptr);
        std::set<int> nums;
        for (auto& c : ce.second) for (auto& f : c.second) if (!f.second.empty()) nums.insert(f.first);
        int written = 0;
        for (auto& c : cols)
            for (int num : nums) {
                std::vector<uint8_t> d; std::string name;
                if (!pe_read(c, num, d, &name)) {   // a color without its own file takes color 1's when it has changes of its own
                    auto own = ce.second.find(c.j);
                    if (own == ce.second.end() || !own->second.count(num) || own->second[num].empty() || cols.empty() || !pe_read(cols[0], num, d, &name)) continue;
                    std::vector<uint8_t> o = d;
                    for (int lv : {-1, c.j}) { auto a = ce.second.find(lv); if (a == ce.second.end()) continue; auto f = a->second.find(num); if (f != a->second.end()) for (auto& x : f->second) if (x.first < (int)o.size()) o[x.first] = x.second; }
                    files++; written++;
                    if (!add(c.out + "/" + name, o)) { err = L"Cannot write the parameters of " + mw_w(re_label(s)); return false; }
                    continue;
                }
                std::vector<uint8_t> o = d;
                for (int lv : {-1, c.j}) { auto a = ce.second.find(lv); if (a == ce.second.end()) continue; auto f = a->second.find(num); if (f != a->second.end()) for (auto& b : f->second) if (b.first < (int)o.size()) o[b.first] = b.second; }
                if (o == d) continue;
                files++; written++;
                if (!add(c.out + "/" + name, o)) { err = L"Cannot write the parameters of " + mw_w(re_label(s)); return false; }
            }
        printf("roster: parametros de %s: %d archivos escritos en %d colores\n", re_label(s).c_str(), written, (int)cols.size());
    }
    return true;
}
