#pragma once
// item 29.3 maps: new map slots declared by mods in mods/<id>/Stages/stages.txt, placed after the 10 originals (Random last).
// Map index 10 + j = PACKFILE entries BASE + 2j (map) and BASE + 2j + 1 (SFX), after the roster groups
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdint>

namespace stages {

static const int OLD = 10, CAP = 126;   // maps in the select at most (Random = index of the count, cells = count + 1)
static const char* const NAMES[OLD] = {"Wasteland", "Rocky Area", "Island", "City Ruins", "Namek Land", "Namek Village",
                                       "Namek Destroyed", "Mountain Road", "Cell Games", "Kaioshin World"};

struct Map {
    std::string mod, id;
    bool tour = false;
    int sfx = 0;   // original map whose SFX are used while the mod has none
    std::string name[2];   // en, es (empty = the other language, then the id)
    std::string destroy_to, planet;   // item 71: map that replaces it when the planet is destroyed (id, 1..10 or none); planet = earth|namek marks a destroyed planet map
};
struct Defs { bool loaded = false; std::vector<Map> maps; std::string orig[OLD + 1][2]; };   // orig: names given to the originals and Random ([original] n = 1..11)
inline Defs& defs() { static Defs d; return d; }

inline bool valid_id(const std::string& id) {
    if (id.empty() || id.size() > 48) return false;
    for (char ch : id) if (!isalnum((unsigned char)ch) && ch != '_' && ch != '-') return false;
    return true;
}

inline std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// [stage] blocks in file order: id (folder Packfile/Maps/<id>), tournament = 0/1, sfx = 1..10 (original map, 1-based)
inline void read_file(const std::string& path, const std::string& mod, Defs& d) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return;
    char line[1024];
    Map* cur = nullptr;
    int on = -1;
    std::string oname[OLD + 1][2];
    std::vector<Map> got;
    bool first = true;
    while (fgets(line, sizeof(line), f)) {
        std::string s = line;
        if (first && s.compare(0, 3, "\xEF\xBB\xBF") == 0) s.erase(0, 3);
        first = false;
        size_t h = s.find('#');
        if (h != std::string::npos) s = s.substr(0, h);
        s = trim(s);
        if (s.empty()) continue;
        if (s[0] == '[') {
            cur = nullptr; on = -2;
            if (s == "[stage]" || s == "[map]") { got.push_back(Map()); cur = &got.back(); cur->mod = mod; on = -1; }
            else if (s == "[original]") on = -3;
            continue;
        }
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string k = trim(s.substr(0, eq)), v = trim(s.substr(eq + 1));
        if (!cur && on != -1) {   // [original]: n, name_en, name_es
            if (on == -2) continue;
            if (k == "n") { int x = atoi(v.c_str()); on = x >= 1 && x <= OLD + 1 ? x - 1 : -2; }
            else if (on >= 0 && (k == "name_en" || k == "name_es")) oname[on][k == "name_es"] = v;
            continue;
        }
        if (!cur) continue;
        if (k == "name_en" || k == "name_es") { cur->name[k == "name_es"] = v; continue; }
        if (k == "id") cur->id = v;
        else if (k == "tournament") cur->tour = atoi(v.c_str()) != 0;
        else if (k == "destroy_to") cur->destroy_to = v;
        else if (k == "planet") cur->planet = v;
        else if (k == "sfx") { int x = atoi(v.c_str()); cur->sfx = x >= 1 && x <= OLD ? x - 1 : 0; }
    }
    fclose(f);
    for (int i = 0; i <= OLD; i++) for (int l = 0; l < 2; l++) if (d.orig[i][l].empty()) d.orig[i][l] = oname[i][l];   // a higher-priority mod first
    for (auto& m : got) {
        if (!valid_id(m.id)) { printf("mapas: %s: id '%s' no valido (solo letras, numeros, _ y -)\n", mod.c_str(), m.id.c_str()); continue; }
        bool taken = false;
        for (auto& o : d.maps) if (!taken && o.id == m.id) {
            taken = true;
            bool add = (o.planet.empty() && !m.planet.empty()) || (o.destroy_to.empty() && !m.destroy_to.empty());   // a lower-priority mod adds the destruction keys
            if (o.planet.empty()) o.planet = m.planet;
            if (o.destroy_to.empty()) o.destroy_to = m.destroy_to;
            printf("mapas: %s: id '%s' ya declarado por %s%s\n", mod.c_str(), m.id.c_str(), o.mod.c_str(), add ? ", se toman planet/destroy_to de este mod" : ", ignorado");
        }
        if (taken) continue;   // a higher-priority mod already has it
        if ((int)d.maps.size() >= CAP - OLD) { printf("mapas: %s: limite de %d mapas alcanzado\n", mod.c_str(), CAP); break; }
        d.maps.push_back(m);
    }
}

// the game's names of the originals and Random (games/ttt/names/maps.txt: row | en | es)
inline const std::string& default_name(int i, int l) {
    static std::string n[OLD + 1][2];
    static bool ok = false;
    if (!ok) {
        ok = true;
        if (FILE* f = fopen("../../games/ttt/names/maps.txt", "rb")) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                std::string s = line;
                if (s.empty() || s[0] == '#') continue;
                size_t a = s.find('|'), b = a == std::string::npos ? a : s.find('|', a + 1);
                if (b == std::string::npos) continue;
                int r = atoi(s.c_str());
                if (r < 0 || r > OLD) continue;
                n[r][0] = trim(s.substr(a + 1, b - a - 1)); n[r][1] = trim(s.substr(b + 1));
            }
            fclose(f);
        }
        for (int i = 0; i < OLD; i++) for (int k = 0; k < 2; k++) if (n[i][k].empty()) n[i][k] = NAMES[i];
    }
    return n[i < 0 || i > OLD ? OLD : i][l ? 1 : 0];
}
// the name shown for a map of the select: i < OLD original, i == OLD + new count = Random, else new map i - OLD
inline std::string name_of(int i, int l) {
    Defs& d = defs();
    int nn = (int)d.maps.size(), r = i < OLD ? i : i >= OLD + nn ? OLD : -1;
    l = l ? 1 : 0;
    if (r >= 0) return !d.orig[r][l].empty() ? d.orig[r][l] : default_name(r, l);
    const Map& m = d.maps[(size_t)(i - OLD)];
    return !m.name[l].empty() ? m.name[l] : !m.name[1 - l].empty() ? m.name[1 - l] : m.id;
}

inline void load(const std::string& root, const std::vector<std::string>& ids) {
    Defs& d = defs();
    if (d.loaded) return;
    d.loaded = true;
    const char* e = getenv("TTT_STAGES");
    if (e && *e && !atoi(e)) return;
    for (auto& id : ids) read_file(root + "/" + id + "/Stages/stages.txt", id, d);
}

}
