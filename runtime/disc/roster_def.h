#pragma once
// item 29.3: characters and cell layout declared by mods in mods/<id>/Roster/roster.txt; read by the disc (PACKFILE
// entries of new groups) and by the hooks (roster tables). New BT3 ids are numbered by their rank among the free ones;
// characters declared by key ([character Name]) get a runtime id from 161 on (never a BT3 id), sorted by key
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdint>
#include <map>
#include <array>
#include <algorithm>

namespace roster {

// DYN: keyed characters at once; they get PACKFILE groups only for the ones installed (the game's entry indexes are 16-bit)
static const int SYS = 161, NEW = 91, DYN = 48, ALL = SYS + DYN, K_OLD = 70, FIRST_NEW = 0x4C, MENU_N = FIRST_NEW + NEW + DYN;
static const uint32_t MODEL_BASE = 14097, VOICE_BASE = MODEL_BASE + NEW * 5, PF_COUNT = VOICE_BASE + NEW * 126, DYN_MODEL = PF_COUNT,
    NAME_ID = 9900, NAME_OLD = 6300, NAME_DYN = 6400;
inline int& dyn_n() { static int n = 0; return n; }   // keyed characters installed
inline uint32_t dyn_voice() { return DYN_MODEL + 5 * (uint32_t)dyn_n(); }
inline uint32_t pf_all() { return dyn_voice() + 126 * (uint32_t)dyn_n(); }
// system ids of the 70 original characters (sysid_tab 0..69)
static const uint8_t VANILLA[K_OLD] = {3, 4, 5, 6, 13, 14, 15, 16, 17, 18, 19, 21, 23, 25, 26, 27, 28, 29, 34, 35, 36, 37, 39, 40,
    41, 42, 44, 45, 46, 47, 49, 50, 52, 53, 56, 73, 77, 79, 80, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 99,
    100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 111, 112, 114, 116, 117, 133};

// select screen labels: [kind][lang] kind 0 name, 1 form right, 2 form left; lang 0 en, 1 es, 2 unused, 3 all; -2 unset, -1 empty, >= 0 a BT3 id
struct Labels { int ref[3][4] = {{-2, -2, -2, -2}, {-2, -2, -2, -2}, {-2, -2, -2, -2}}; };
struct VsLine { std::string rival; int rid = -2, line = 0; bool victory = false; };   // intro/victory line against a rival (key or BT3 id)
struct Char { int sysid = -1, donor = 3, parent = -1; std::string mod, id, folder, name[3], form_old[2], key, tf_key[4]; int form = -2, tf[4] = {-2, -2, -2, -2}; Labels lab; std::vector<VsLine> vs; };   // form: transformation text id (names as text), -1 none, -2 unset; tf: transformation targets (-2 as in the file, -1 none)
struct Rename { int sysid = -1, newid = -1, form = -2; std::string mod, name[3], form_old[2]; Labels lab; };   // an original character: own names, own BT3 id
struct Defs {
    bool loaded = false;
    std::vector<Char> chars;
    std::vector<Rename> renames;
    std::vector<std::vector<int>> layout;
    std::string layout_mod, layout_err;
    std::vector<std::string> keyrefs;   // [layout] tokens given as keys: stored as -(2 + index) until load resolves them
    std::map<int, std::array<std::string, 2>> ftext;   // transformation texts: id < 100 = a row of games/ttt/names/forms.txt given another text, >= 100 = new
};
inline Defs& defs() { static Defs d; return d; }
static const int FORM_NEW = 100;
inline const std::vector<std::array<std::string, 2>>& form_rows() {   // the game's transformation texts by atlas row (games/ttt/names/forms.txt: row | en | es)
    static std::vector<std::array<std::string, 2>> v;
    static bool ok = false;
    if (ok) return v;
    ok = true;
    if (FILE* f = fopen("../../games/ttt/names/forms.txt", "rb")) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            std::string s = line;
            if (s.empty() || s[0] == '#') continue;
            size_t a = s.find('|'), b = a == std::string::npos ? a : s.find('|', a + 1);
            if (b == std::string::npos) continue;
            int r = atoi(s.c_str());
            if (r < 0 || r >= FORM_NEW) continue;
            if ((int)v.size() <= r) v.resize(r + 1);
            auto tr = [](std::string x) { size_t p = x.find_first_not_of(" \t\r\n"), q = x.find_last_not_of(" \t\r\n"); return p == std::string::npos ? std::string() : x.substr(p, q - p + 1); };
            v[r] = {tr(s.substr(a + 1, b - a - 1)), tr(s.substr(b + 1))};
        }
        fclose(f);
    }
    return v;
}
inline std::string form_text(int id, int l) {   // the text of a transformation id in a language (the other language when empty)
    l = l ? 1 : 0;
    auto& t = defs().ftext;
    auto it = t.find(id);
    if (it != t.end() && !it->second[l].empty()) return it->second[l];
    auto& r = form_rows();
    if (id >= 0 && id < (int)r.size() && !r[id][l].empty()) return r[id][l];
    if (it != t.end() && !it->second[1 - l].empty()) return it->second[1 - l];
    return id >= 0 && id < (int)r.size() ? r[id][1 - l] : std::string();
}

// asset group k of each BT3 id in the original game (asset_group_tab 0x08A69720; not the order of VANILLA)
static const uint8_t GROUP[SYS] = {255, 255, 255, 0, 1, 2, 3, 255, 255, 255, 255, 255, 255, 4, 5, 6, 7, 8, 9, 10, 255, 11, 255, 12, 255, 13, 14, 15, 16, 17, 255, 255, 255, 255, 18, 19, 20, 21, 255, 24, 25, 26, 27, 255, 22, 23, 29, 30, 255, 31, 32, 255, 34, 33, 255, 255, 28, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 35, 255, 255, 255, 36, 255, 37, 38, 255, 40, 41, 42, 39, 43, 44, 47, 46, 45, 48, 49, 50, 51, 52, 255, 255, 255, 69, 53, 54, 55, 56, 57, 59, 60, 61, 62, 58, 255, 63, 64, 255, 65, 255, 66, 67, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 68, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
inline std::string low(std::string s) { for (auto& ch : s) ch = (char)tolower((unsigned char)ch); return s; }
inline bool keyed(int s) { return s >= SYS && s < ALL; }
inline int group_vanilla(int s) { return s >= 0 && s < SYS && GROUP[s] != 255 ? GROUP[s] : -1; }
inline int sys_of_group(int k) { for (int s = 0; s < SYS; s++) if (GROUP[s] == k) return s; return -1; }
inline bool vanilla(int s) { for (int v : VANILLA) if (v == s) return true; return false; }
inline int rank(int s) {   // 0..90 for a free system id, NEW.. for a keyed one, -1 for an original one
    if (keyed(s)) return NEW + (s - SYS);
    if (s < 0 || s >= SYS || vanilla(s)) return -1;
    int r = 0;
    for (int i = 0; i < s; i++) if (!vanilla(i)) r++;
    return r;
}
inline int rank_vanilla(int s) { for (int i = 0; i < K_OLD; i++) if (VANILLA[i] == s) return i; return -1; }   // index in VANILLA (menu number), not the group
inline int menu_of(int s) { int r = rank(s); return r < 0 ? -1 : FIRST_NEW + r; }
inline int group_of(int s) { int r = rank(s); return r < 0 ? -1 : K_OLD + r; }
inline uint32_t model_entry(int r) { return r < NEW ? MODEL_BASE + 5 * (uint32_t)r : DYN_MODEL + 5 * (uint32_t)(r - NEW); }
inline uint32_t voice_entry(int r) { return r < NEW ? VOICE_BASE + 126 * (uint32_t)r : dyn_voice() + 126 * (uint32_t)(r - NEW); }
inline uint32_t name_of_rank(int r) { return r < NEW ? NAME_ID + (uint32_t)r : NAME_DYN + (uint32_t)(r - NEW); }
inline int rank_of_name(uint32_t id) { return id >= NAME_ID && id < NAME_ID + NEW ? (int)(id - NAME_ID) : id >= NAME_DYN && id < NAME_DYN + DYN ? NEW + (int)(id - NAME_DYN) : -1; }
inline int entry_rank(uint32_t i, int* j, bool* voice) {   // an appended PACKFILE entry: its group rank, colour/sfx or voice line
    *voice = (i >= VOICE_BASE && i < PF_COUNT) || (i >= dyn_voice() && i < pf_all());
    if (i >= MODEL_BASE && i < VOICE_BASE) { *j = (int)(i - MODEL_BASE) % 5; return (int)(i - MODEL_BASE) / 5; }
    if (i >= VOICE_BASE && i < PF_COUNT) { *j = (int)(i - VOICE_BASE) % 126; return (int)(i - VOICE_BASE) / 126; }
    if (i >= DYN_MODEL && i < dyn_voice()) { *j = (int)(i - DYN_MODEL) % 5; return NEW + (int)(i - DYN_MODEL) / 5; }
    if (i >= dyn_voice() && i < pf_all()) { *j = (int)(i - dyn_voice()) % 126; return NEW + (int)(i - dyn_voice()) / 126; }
    return -1;
}
inline const Char* find(int s) { for (auto& c : defs().chars) if (c.sysid == s) return &c; return nullptr; }
inline uint32_t* name_orig() { static uint32_t v[K_OLD]; return v; }   // game text id of each original group's name
inline const Rename* renamed(int s) { for (auto& r : defs().renames) if (r.sysid == s) return &r; return nullptr; }
inline bool valid_id(const std::string& id) {   // a new character's ID is its folder name in Packfile/Characters
    if (id.empty() || id.size() > 48) return false;
    for (char ch : id) if (!isalnum((unsigned char)ch) && ch != '_' && ch != '-') return false;
    return true;
}

// name_image, form_label (both sides), form_right, form_left; optional _en/_es; value: a BT3 id or "none"
inline bool label_key(const std::string& k, const std::string& v, Labels& lab) {
    static const char* K[4] = {"name_image", "form_label", "form_right", "form_left"};
    for (int i = 0; i < 4; i++) {
        size_t n = strlen(K[i]);
        if (k.compare(0, n, K[i])) continue;
        std::string r = k.substr(n);
        int l = r.empty() ? 3 : r == "_en" ? 0 : r == "_es" ? 1 : -1;
        if (l < 0) continue;
        char* e = nullptr;
        long x = v == "none" ? -1 : strtol(v.c_str(), &e, 10);
        if (v != "none" && (v.empty() || *e || x < 0 || x >= SYS)) return true;
        if (i == 0) lab.ref[0][l] = (int)x;
        else if (i == 1) lab.ref[1][l] = lab.ref[2][l] = (int)x;
        else lab.ref[i - 1][l] = (int)x;
        return true;
    }
    return false;
}

inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// [layout]: one cell per line, BT3 ids (base then forms) or "random"; [character N]: id (folder Characters/<id>), name_en/es,
// base (SFX and voices when the mod has none; "none"); [original N] (or [name N]): names and bt3_id of an original character
inline void read_file(const std::string& path, const std::string& mod, bool take_layout, Defs& d) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return;
    char line[2048];
    int sec = 0, ln = 0;
    Char* cur = nullptr;
    Rename* ren = nullptr;
    std::vector<std::vector<int>> lay;
    std::string err;
    bool first = true;
    while (fgets(line, sizeof(line), f)) {
        ln++;
        std::string s = line;
        if (first && s.compare(0, 3, "\xEF\xBB\xBF") == 0) s.erase(0, 3);
        first = false;
        size_t h = s.find('#');
        if (h != std::string::npos) s = s.substr(0, h);
        s = trim(s);
        if (s.empty()) continue;
        if (s[0] == '[') {
            cur = nullptr; ren = nullptr;
            int n = -1;
            if (s == "[layout]") sec = 1;
            else if (s == "[forms]") sec = 4;
            else if (sscanf(s.c_str(), "[name %d]", &n) == 1 || sscanf(s.c_str(), "[original %d]", &n) == 1) {
                sec = 3;
                if (!vanilla(n)) { printf("roster: %s: [original %d] no es un personaje original\n", mod.c_str(), n); sec = 0; continue; }
                bool taken = false;
                for (auto& r : d.renames) taken = taken || r.sysid == n;
                if (taken) { sec = 0; continue; }
                d.renames.push_back(Rename());
                ren = &d.renames.back();
                ren->sysid = n; ren->mod = mod;
            }
            else if (s.compare(0, 11, "[character ") == 0 && s.back() == ']' && !isdigit((unsigned char)trim(s.substr(11, s.size() - 12))[0])) {
                std::string k = trim(s.substr(11, s.size() - 12));
                sec = 0;
                if (!valid_id(k)) { printf("roster: %s: [character %s] clave no valida (solo letras, numeros, _ y -)\n", mod.c_str(), k.c_str()); continue; }
                bool taken = false;
                for (auto& c : d.chars) taken = taken || low(c.key) == low(k);
                if (taken) continue;   // a higher-priority mod already defined it
                sec = 2;
                d.chars.push_back(Char());
                cur = &d.chars.back();
                cur->key = cur->id = k; cur->mod = mod;
            }
            else if (sscanf(s.c_str(), "[character %d]", &n) == 1) {
                sec = 2;
                if (n < 0 || n >= SYS || vanilla(n)) { printf("roster: %s: [character %d] no es un id BT3 libre\n", mod.c_str(), n); sec = 0; continue; }
                bool taken = false;
                for (auto& c : d.chars) taken = taken || c.sysid == n;
                if (taken) { sec = 0; continue; }   // a higher-priority mod already defined it
                d.chars.push_back(Char());
                cur = &d.chars.back();
                cur->sysid = n; cur->mod = mod;
            } else sec = 0;
            continue;
        }
        if (sec == 4) {   // id | en | es
            size_t a = s.find('|'), b = a == std::string::npos ? a : s.find('|', a + 1);
            if (b == std::string::npos) continue;
            int id = atoi(s.c_str());
            if (id < 0 || d.ftext.count(id)) continue;   // a higher-priority mod first
            d.ftext[id] = {trim(s.substr(a + 1, b - a - 1)), trim(s.substr(b + 1))};
            continue;
        }
        if (sec == 1 && take_layout) {
            std::vector<int> cell;
            size_t p = 0;
            while (p < s.size()) {
                size_t q = s.find_first_of(" \t,", p);
                std::string t = s.substr(p, q == std::string::npos ? std::string::npos : q - p);
                p = q == std::string::npos ? s.size() : q + 1;
                if (t.empty()) continue;
                if (t == "random" || t == "Random") { cell.push_back(-1); continue; }
                if (!isdigit((unsigned char)t[0]) && valid_id(t)) { d.keyrefs.push_back(t); cell.push_back(-1 - (int)d.keyrefs.size()); continue; }
                char* e = nullptr;
                long v = strtol(t.c_str(), &e, 0);
                if (*e || v < 0 || v >= SYS) { if (err.empty()) err = "linea " + std::to_string(ln) + ": id BT3 no valido " + t; continue; }
                cell.push_back((int)v);
            }
            if (!cell.empty()) lay.push_back(cell);
        } else if (sec == 3 && ren) {
            size_t eq = s.find('=');
            if (eq == std::string::npos) continue;
            std::string k = trim(s.substr(0, eq)), v = trim(s.substr(eq + 1));
            if (k == "name" || k == "name_en") { ren->name[0] = v; if (k == "name") ren->name[1] = ren->name[2] = v; }
            else if (k == "name_es") ren->name[1] = v;
            else if (k == "bt3_id") ren->newid = atoi(v.c_str());
            else if (k == "form_en" || k == "form_es") ren->form_old[k == "form_es"] = v;
            else if (k == "form") ren->form = v == "none" ? -1 : atoi(v.c_str());
            else label_key(k, v, ren->lab);
        } else if (sec == 2 && cur) {
            size_t eq = s.find('=');
            if (eq == std::string::npos) continue;
            std::string k = trim(s.substr(0, eq)), v = trim(s.substr(eq + 1));
            if (k == "id") cur->id = v;
            else if (k == "folder") { for (auto& ch : v) if (ch == '\\') ch = '/'; while (!v.empty() && v.back() == '/') v.pop_back(); cur->folder = v; }
            else if (k == "name" || k == "name_en") { cur->name[0] = v; if (k == "name") cur->name[1] = cur->name[2] = v; }
            else if (k == "name_es") cur->name[1] = v;
            else if (k == "voice_donor" || k == "base") cur->donor = v == "none" ? -1 : atoi(v.c_str());
            else if (k == "form_en" || k == "form_es") cur->form_old[k == "form_es"] = v;
            else if (k == "form") cur->form = v == "none" ? -1 : atoi(v.c_str());
            else if (k == "transform") {
                size_t p0 = 0;
                for (int i = 0; i < 4 && p0 <= v.size(); i++) {
                    size_t q = v.find(',', p0);
                    cur->tf_key[i] = trim(v.substr(p0, q == std::string::npos ? std::string::npos : q - p0));
                    if (q == std::string::npos) break;
                    p0 = q + 1;
                }
            }
            else if (k.size() == 11 && k.compare(0, 10, "transform_") == 0 && k[10] >= '1' && k[10] <= '4') cur->tf_key[k[10] - '1'] = v;
            else if (k == "intro" || k == "victory") {
                size_t c2 = v.find(':');
                if (c2 == std::string::npos) continue;
                VsLine x; x.rival = trim(v.substr(0, c2)); x.line = atoi(trim(v.substr(c2 + 1)).c_str()); x.victory = k == "victory";
                cur->vs.push_back(x);
            }
            else label_key(k, v, cur->lab);
        }
    }
    fclose(f);
    auto old_text = [&](std::string* fo, int* id) {   // form_en / form_es of the first version: a new text of its own
        if (fo[0].empty() && fo[1].empty()) return;
        for (auto& e : d.ftext) if (e.second[0] == fo[0] && e.second[1] == fo[1]) { *id = e.first; return; }
        int n = FORM_NEW; for (auto& e : d.ftext) n = std::max(n, e.first + 1);
        d.ftext[n] = {fo[0], fo[1]}; *id = n;
    };
    for (auto& c : d.chars) if (c.mod == mod && c.form == -2) old_text(c.form_old, &c.form);
    for (auto& r : d.renames) if (r.mod == mod && r.form == -2) old_text(r.form_old, &r.form);
    if (take_layout && !lay.empty()) {
        if (err.empty()) { d.layout = lay; d.layout_mod = mod; }
        else printf("roster: %s: %s, plantilla ignorada\n", mod.c_str(), err.c_str());
    }
}

inline void load(const std::string& root, const std::vector<std::string>& ids) {
    Defs& d = defs();
    if (d.loaded) return;
    d.loaded = true;
    const char* e = getenv("TTT_ROSTER");
    if (e && *e && !atoi(e)) return;
    for (auto& id : ids) read_file(root + "/" + id + "/Roster/roster.txt", id, d.layout_mod.empty(), d);
    for (size_t i = 0; i < d.chars.size();) {
        Char& c = d.chars[i];
        if (!c.id.empty() && !valid_id(c.id)) { printf("roster: %s: [character %d] id '%s' no valido (solo letras, numeros, _ y -)\n", c.mod.c_str(), c.sysid, c.id.c_str()); c.id.clear(); }
        if (c.folder.empty() && !c.id.empty()) c.folder = "Characters/" + c.id;
        if (c.folder.empty()) { printf("roster: %s: [character %d] sin folder, se ignora\n", c.mod.c_str(), c.sysid); d.chars.erase(d.chars.begin() + i); continue; }
        if (c.donor >= 0 && !vanilla(c.donor)) c.donor = 3;
        i++;
    }
    // keyed characters: runtime ids 161.. in key order (the same on every machine with the same mods)
    std::vector<Char*> ks;
    for (auto& c : d.chars) if (!c.key.empty()) ks.push_back(&c);
    std::sort(ks.begin(), ks.end(), [](const Char* a, const Char* b) { return low(a->key) < low(b->key); });
    for (size_t i = 0; i < ks.size(); i++) {
        if ((int)i < DYN) { ks[i]->sysid = SYS + (int)i; continue; }
        printf("roster: %s: %s sin espacio (maximo %d personajes con clave a la vez)\n", ks[i]->mod.c_str(), ks[i]->key.c_str(), DYN);
        ks[i]->sysid = -1;
    }
    d.chars.erase(std::remove_if(d.chars.begin(), d.chars.end(), [](const Char& c) { return c.sysid < 0; }), d.chars.end());
    dyn_n() = (int)std::min(ks.size(), (size_t)DYN);
    if (!ks.empty()) printf("roster: %d personajes con clave (ids %d a %d)\n", (int)std::min(ks.size(), (size_t)DYN), SYS, SYS + (int)std::min(ks.size(), (size_t)DYN) - 1);
    auto ref = [&](const std::string& t, const std::string& who) -> int {   // -2 as in the file, -1 none
        if (t.empty() || t == "-") return -2;
        if (t == "none") return -1;
        char* e = nullptr;
        long v = strtol(t.c_str(), &e, 10);
        if (!*e) return v >= 0 && v < SYS ? (int)v : -2;
        for (auto& c : d.chars) if (low(c.key) == low(t)) return c.sysid;
        printf("roster: %s: no existe el personaje '%s'\n", who.c_str(), t.c_str());
        return -2;
    };
    for (auto& cell : d.layout) {
        std::vector<int> nc;
        for (int v : cell) {
            if (v >= -1) { nc.push_back(v); continue; }
            int s = ref(d.keyrefs[-v - 2], d.layout_mod + " [layout]");
            if (s >= 0) nc.push_back(s);
        }
        cell = nc;
    }
    d.layout.erase(std::remove_if(d.layout.begin(), d.layout.end(), [](const std::vector<int>& c) { return c.empty(); }), d.layout.end());
    for (auto& c : d.chars) {
        std::string who = c.mod + " " + (c.key.empty() ? std::to_string(c.sysid) : c.key);
        for (int i = 0; i < 4; i++) c.tf[i] = ref(c.tf_key[i], who);
        for (auto& x : c.vs) x.rid = ref(x.rival, who);
    }
    // a transformation without a name of its own takes its base's, per language
    for (auto& cell : d.layout) for (size_t j = 1; j < cell.size(); j++)
        for (auto& c : d.chars) if (c.sysid == cell[j] && cell[0] >= 0) c.parent = cell[0];
    for (int pass = 0; pass < 2; pass++)
        for (auto& c : d.chars) {
            if ((c.parent >= 0) != (pass == 1)) continue;
            const Char* p = c.parent >= 0 ? find(c.parent) : nullptr;
            if (c.parent >= 0 && !p) {
                int o = c.parent;
                for (auto& r : d.renames) if (r.newid == c.parent) o = r.sysid;
                if (vanilla(o)) { c.parent = o; continue; }   // an original base: its name is served by the text hook
                c.parent = -1;
            }
            for (int l = 0; l < 3; l++) if (c.name[l].empty()) c.name[l] = p ? p->name[l] : !c.name[0].empty() ? c.name[0] : c.folder.substr(c.folder.find_last_of('/') + 1);
        }
}

}
