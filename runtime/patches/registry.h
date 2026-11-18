#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

// Internal patch registry: one file per correction under runtime/patches/, not exposed in the UI.
// val = def unless the TTT_* variable is set (development override).
struct Patch {
    const char* id;
    const char* env;
    int def, val, mod;
};

inline std::vector<Patch*>& patch_all() { static std::vector<Patch*> v; return v; }

inline int patch_add(Patch* p) { patch_all().push_back(p); return 0; }

#define PATCH(sym, env, def) \
    inline Patch patch_##sym = {#sym, env, def, def, 0}; \
    inline const int patch_reg_##sym = patch_add(&patch_##sym);

// a patch that an enabled mod may switch on with parches=<id> in its mod.txt
#define PATCH_MOD(sym, env, def) \
    inline Patch patch_##sym = {#sym, env, def, def, 1}; \
    inline const int patch_reg_##sym = patch_add(&patch_##sym);

inline int patch_mod_on(const std::string& id) {
    for (Patch* p : patch_all()) {
        if (id != p->id) continue;
        if (!p->mod) return -1;
        const char* e = getenv(p->env);
        if (!(e && *e)) p->val = 1;
        return 1;
    }
    return 0;
}

inline void patches_load() {
    int n = 0;
    for (Patch* p : patch_all()) {
        const char* e = getenv(p->env);
        p->val = e && *e ? atoi(e) : p->def;
        if (p->val != p->def) { n++; printf("patch %s = %d (%s)\n", p->id, p->val, p->env); }
    }
    printf("patches: %d registered, %d overridden\n", (int)patch_all().size(), n);
}
