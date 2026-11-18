// item 64, stage 1: Ctrl+R rereads changed files from assets/Packfile and mods/*/Packfile without a restart
// a watcher thread per root collects changed paths (ReadDirectoryChangesW, no polling); the game thread applies them between frames:
// mod listing updated, BT3 sources dropped, containers listed again, held/built copies dropped, sizes, sectors and the game's index
// (0x08AF3240 or the roster copy) updated. New entries, damaged (#dmg) entries and audio containers still need a restart
static std::mutex rl_mx;
static std::set<std::string> rl_paths;
static std::atomic<bool> rl_over(false), rl_req(false);

#ifdef _WIN32
static void rl_watch(std::string root) {
    std::wstring w = to_utf16(root);
    HANDLE h = CreateFileW(w.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (h == INVALID_HANDLE_VALUE) { printf("recarga: no se puede vigilar %s\n", root.c_str()); return; }
    std::vector<DWORD> buf(16384);
    for (;;) {
        DWORD got = 0;
        if (!ReadDirectoryChangesW(h, buf.data(), (DWORD)(buf.size() * 4), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                   FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE, &got, nullptr, nullptr)) break;
        if (!got) { rl_over = true; continue; }
        const uint8_t* p = (const uint8_t*)buf.data();
        for (;;) {
            const FILE_NOTIFY_INFORMATION* fi = (const FILE_NOTIFY_INFORMATION*)p;
            std::wstring nm(fi->FileName, fi->FileNameLength / 2);
            std::string s = root + "/" + to_utf8(nm.c_str());
            for (auto& ch : s) if (ch == '\\') ch = '/';
            std::string l = lower(s);
            if (l.find("/packfile/") != std::string::npos && l.find("/lang/") == std::string::npos && l.find("/cache/") == std::string::npos) {
                std::lock_guard<std::mutex> g(rl_mx);
                if (rl_paths.size() < 20000) rl_paths.insert(s); else rl_over = true;
            }
            if (!fi->NextEntryOffset) break;
            p += fi->NextEntryOffset;
        }
    }
    CloseHandle(h);
}
#endif

static void rl_start() {
    tex_reg.reserve(tex_reg.size() + 32768);
    sfx_reg.reserve(sfx_reg.size() + 4096);
    at3_reg.reserve(at3_reg.size() + 4096);
#ifdef _WIN32
    std::thread(rl_watch, disc_dir + "/Packfile").detach();
    if (mod_enabled && !mods_root_global.empty()) std::thread(rl_watch, mods_root_global).detach();
#endif
}

void psp_disc_reload_request() { rl_req = true; }
static uint32_t rl_pf_buf = 0x08AF3240u, rl_pf_cap = 0x38000;
static std::vector<uint32_t> rl_ids0;
static std::set<int> rl_pend;   // stage 3: reloaded entries for the fighters in battle
// entries reloaded and not read by the game since (a battle loaded after the reload already has them)
int psp_disc_reload_take(uint32_t* out, int max) {
    std::lock_guard<std::mutex> g(rl_watch_mx);
    int n = 0;
    for (int i : rl_pend) if (n < max && rl_watch_idx.count(i)) out[n++] = (uint32_t)i;
    rl_pend.clear();
    return n;
}
static std::unordered_map<uint32_t, uint32_t> rl_swap;   // stage 2: texture id in the game's memory -> its new registration

// changed images whose game size, bpp and padded size match: the copy already in memory can point at the new image
static void rl_tex_pairs(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b, const std::set<std::string>& changed, std::vector<std::pair<uint32_t, uint32_t>>& out) {
    std::map<std::pair<std::string, std::string>, uint32_t> nb;
    for (uint32_t id : b) if (id && id <= tex_reg.size() && !tex_reg[id - 1].mat_tex) nb[{lower(tex_reg[id - 1].pak), tex_reg[id - 1].name}] = id;
    int logs = 0;
    for (uint32_t id : a) {
        if (!id || id > tex_reg.size()) continue;
        const TexPng& o = tex_reg[id - 1];
        if (o.mat_tex) continue;
        auto it = nb.find({lower(o.pak), o.name});
        if (it == nb.end()) continue;
        const TexPng& n = tex_reg[it->second - 1];
        bool dirty = false;
        std::vector<uint32_t> look{it->second};
        for (auto& m : n.mats) look.push_back(m.second);
        if (n.dmg1) look.push_back(n.dmg1);
        for (uint32_t k : look) {
            if (!k || k > tex_reg.size()) continue;
            std::string r = lower(mod_path(tex_reg[k - 1].fpath));
            for (auto& ch : r) if (ch == '\\') ch = '/';
            if (changed.count(r)) dirty = true;
        }
        if (!dirty) continue;
        if (o.width0 != n.width0 || o.height0 != n.height0 || o.dw != n.dw || o.dh != n.dh || o.bpp != n.bpp) {
            if (logs++ < 8) printf("recarga: %s :: %s cambio de tamano (%dx%d a %dx%d), se ve al volver a cargarse\n", o.pak.c_str(), o.name.c_str(), o.width0, o.height0, n.width0, n.height0);
            continue;
        }
        out.push_back({id, it->second});
    }
    static int miss = 0;
    bool png = false;
    for (auto& x : changed) if (ends_with(x, ".png")) png = true;
    if (png && out.empty() && !b.empty() && miss++ < 2) {
        std::string r = lower(mod_path(tex_reg[b[0] - 1].fpath));
        printf("recarga: ninguna imagen cambiada en este contenedor (ejemplo %s)\n", r.c_str());
    }
}

// rewrites the texture markers of the copies already in memory (user RAM, VRAM, port heap) to the new ids
static void rl_tex_swap() {
    if (rl_swap.empty()) return;
    std::vector<uint32_t> ids;
    for (auto& pr : rl_swap) ids.push_back(pr.second);
    tex_preload(ids);
    uint32_t top = std::max(brk_free, 0x0A000000u);
    const uint32_t ranges[2][2] = {{0x04000000u, 0x04200000u}, {0x08800000u, top}};
    uint32_t tag; memcpy(&tag, TEX_MARKER, 4);
    int n = 0;
    for (auto& r : ranges)
        for (uint32_t a = r[0]; a + 16 <= r[1]; a += 4) {
            uint8_t* p = psp_p(a);
            uint32_t w0; memcpy(&w0, p, 4);
            if (w0 != tag || memcmp(p, TEX_MARKER, 8)) continue;
            uint32_t id, chk; memcpy(&id, p + 8, 4); memcpy(&chk, p + 12, 4);
            if (chk != ~id) continue;
            auto it = rl_swap.find(id);
            if (it == rl_swap.end()) continue;
            uint32_t v[2] = {it->second, ~it->second};
            memcpy(p + 8, v, 8);
            n++;
        }
    printf("recarga: %d texturas cambiadas al instante, %d copias en memoria\n", (int)rl_swap.size(), n);
    rl_swap.clear();
}
void psp_disc_pf_buf(uint32_t buf, uint32_t bytes) { if (buf) { rl_pf_buf = buf; rl_pf_cap = bytes; } }

static bool rl_prefix(const std::string& s, const std::string& p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }

// a mod file changed, appeared or went away: its folder listing in the overlay follows it; false = the folder is not in the overlay
static bool rl_mod_file(const std::string& real, const std::string& rel) {
    size_t s = rel.find_last_of('/');
    std::string pd = s == std::string::npos ? std::string() : rel.substr(0, s), nm = real.substr(real.find_last_of('/') + 1);
    std::lock_guard<std::mutex> g(mod_mutex);
    auto it = mod_dirs.find(pd);
    if (it == mod_dirs.end()) return false;
    std::error_code ec;
    auto p = std::filesystem::u8path(real);
    bool here = std::filesystem::is_regular_file(p, ec);
    auto mf = mod_files.find(rel);
    if (mf != mod_files.end() && lower(mf->second) != lower(real)) return true;   // another mod serves it first
    auto& v = it->second;
    auto e = std::find_if(v.begin(), v.end(), [&](const DirEntry& d) { return !d.dir && lower(d.name) == lower(nm); });
    if (!here) { if (e != v.end()) v.erase(e); mod_files.erase(rel); return true; }
    DirEntry d; d.name = nm; d.dir = false;
    d.sz = (uint64_t)std::filesystem::file_size(p, ec);
    d.date = (int64_t)std::filesystem::last_write_time(p, ec).time_since_epoch().count();
    if (e != v.end()) *e = d; else { v.push_back(d); mod_files[rel] = real; }
    return true;
}

static void rl_spans() {
    std::vector<DiscSpan> s;
    for (auto& t : disc_spans) if (t.type == 0) s.push_back(t);
    s.push_back({disc_pk_lbn, disc_pk_data, (uint32_t)disc_index.size(), 1, 0});
    for (size_t k = 0; k < disc_pk.size(); k++) s.push_back({disc_pk_lbn + disc_pk_data + disc_pk[k].sector, sectors_of(disc_pk[k].sz), disc_pk[k].sz, 2, (int)k});
    std::stable_sort(s.begin(), s.end(), [](const DiscSpan& a, const DiscSpan& b) { return a.lbn != b.lbn ? a.lbn < b.lbn : a.sectors < b.sectors; });
    disc_spans.swap(s);
}

static void rl_apply() {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::string> paths;
    { std::lock_guard<std::mutex> g(rl_mx); paths.assign(rl_paths.begin(), rl_paths.end()); rl_paths.clear(); }
    bool over = rl_over.exchange(false);
    if (over) printf("recarga: demasiados cambios a la vez, algunos pueden faltar (reinicia si algo no cambia)\n");
    if (paths.empty()) { printf("recarga: sin cambios desde la ultima recarga\n"); return; }
    if (tex_reg.capacity() - tex_reg.size() < 4096 || sfx_reg.capacity() - sfx_reg.size() < 256) { printf("recarga: limite de recargas de esta sesion alcanzado, reinicia el juego\n"); return; }
    rl_gen++;
    std::unordered_map<std::string, std::vector<int>> by_key;
    for (size_t k = 0; k < disc_pk.size(); k++) {
        const PkEntry& x = disc_pk[k];
        if (x.folder) by_key[lower(dmg_strip(x.key))].push_back((int)k);
        else by_key[lower(x.fpath)].push_back((int)k);
    }
    std::string pk_l = lower(disc_dir + "/Packfile/"), md_l = lower(mods_root_global) + "/";
    std::set<int> hit;
    std::vector<std::string> lost;
    int files = 0;
    for (auto& real : paths) {
        std::string l = lower(real), rel;
        bool mod = false;
        if (rl_prefix(l, pk_l)) rel = l.substr(pk_l.size());
        else if (mod_enabled && rl_prefix(l, md_l)) { size_t q = l.find("/packfile/", md_l.size()); if (q == std::string::npos) continue; rel = l.substr(q + 10); mod = true; }
        else continue;
        std::error_code ec;
        if (std::filesystem::is_directory(std::filesystem::u8path(real), ec)) continue;
        files++;
        { std::lock_guard<std::mutex> g(bt3_mx);
          for (auto it = bt3_cache.begin(); it != bt3_cache.end();) { std::string k = lower(it->first); for (auto& ch : k) if (ch == '\\') ch = '/'; if (k == l || rl_prefix(l, k + "/")) it = bt3_cache.erase(it); else ++it; } }
        std::vector<std::string> rels;
        if (rl_prefix(rel, "shared/")) { std::vector<std::string> v; mod_shared_targets(real.substr(real.size() - rel.size() + 7), 0, v); for (auto& t : v) rels.push_back(lower(t)); }
        else rels.push_back(rel);
        bool found = false, gone = false;
        for (auto& r : rels) {
            if (mod && !rl_mod_file(real, r)) { if (lost.size() < 64) lost.push_back(r); gone = true; continue; }
            for (size_t pos = r.size(); pos != std::string::npos && pos > 0; pos = r.find_last_of('/', pos - 1)) {
                std::string pre = r.substr(0, pos);
                auto it = by_key.find(pre);
                if (it == by_key.end() && ends_with(pre, ".pak")) it = by_key.find(pre.substr(0, pre.size() - 4));
                if (it != by_key.end()) { for (int i : it->second) hit.insert(i); found = true; break; }
                if (pos == 0) break;
            }
        }
        if (!found && !gone && !rels.empty() && lost.size() < 64) lost.push_back(rels[0]);
    }
    std::set<std::string> changed;
    for (auto& real : paths) changed.insert(lower(real));
    rl_swap.clear();
    bool moved = false, sized = false;
    int done = 0, skipped = 0, mlogs = 0;
    std::string shown;
    std::map<std::string, std::vector<int>> groups;   // entries sharing one container (a colour served by colour 1) go together
    for (int i : hit) groups[disc_pk[i].folder ? disc_pk[i].key : "\x01" + disc_pk[i].fpath].push_back(i);
    for (auto& gk : groups) {
        PkEntry& x0 = disc_pk[gk.second[0]];
        std::vector<std::pair<std::string, Container>> saved;
        std::string kp = x0.key + "/";
        if (x0.folder) {
            const Container* oc = container_of(x0.key);
            int type = oc ? oc->type : 1;
            if (dmg_key(x0.key) || type == 3 || type == 5 || !oc) { skipped += (int)gk.second.size(); continue; }
            for (auto it = disc_containers.lower_bound(x0.key); it != disc_containers.end() && (it->first == x0.key || rl_prefix(it->first, kp)); ++it) saved.push_back(*it);
            tex_ids(*oc, x0.key, rl_ids0);
            Container c;
            bool rs = disc_rescan; disc_rescan = true;
            bool ok = list_container(c, x0.key, disc_dir + "/Packfile/" + x0.key, type);
            disc_rescan = rs;
            if (!ok) { skipped += (int)gk.second.size(); continue; }
            resolve_txt1(disc_dir + "/Packfile/");
            for (auto it = disc_containers.lower_bound(x0.key); it != disc_containers.end() && (it->first == x0.key || rl_prefix(it->first, kp)); ++it) if (it->second.type != 3 && it->second.type != 4 && it->second.type != 5) it->second.sz = 0;
        }
        uint32_t now = 0;
        if (x0.folder) now = compute_size(x0.key);
        std::vector<std::pair<uint32_t, uint32_t>> gsw;
        if (x0.folder) {
            std::vector<uint32_t> ids1;
            if (const Container* nc = container_of(x0.key)) tex_ids(*nc, x0.key, ids1);
            rl_tex_pairs(rl_ids0, ids1, changed, gsw);
            rl_ids0.clear();
        }
        else { uint64_t t = 0; if (!is_file(disc_dir + "/Packfile/" + x0.fpath, &t)) { skipped += (int)gk.second.size(); continue; } now = (uint32_t)t; }
        // decoded index (0x0882FCF8): record i+1 = absolute sector ([0x08A78AC0] = LBN + index sectors), size
        bool fits = true;
        uint32_t gbase = psp_l32(0x08A78AC0u);
        for (int i : gk.second) {
            const PkEntry& x = disc_pk[i];
            if (x.sz == now) continue;
            if ((size_t)(i + 2) * 16 > rl_pf_cap) { fits = false; continue; }
            const uint32_t* g = (const uint32_t*)psp_p(rl_pf_buf + (uint32_t)(i + 1) * 16);
            uint32_t t = x.sz == x.size0 ? x.real : x.sz;
            if (g[0] != x.sector + gbase || g[1] != t) {
                fits = false;
                if (mlogs++ < 4) printf("recarga: registro %d del indice distinto: juego sector %u tamano %u, port sector %u tamano %u (indice 0x%08X)\n", i, g[0] - gbase, g[1], x.sector, t, rl_pf_buf);
            }
        }
        if (!fits) {
            if (x0.folder) {
                for (auto it = disc_containers.lower_bound(x0.key); it != disc_containers.end() && (it->first == x0.key || rl_prefix(it->first, kp));) it = disc_containers.erase(it);
                for (auto& sv : saved) disc_containers[sv.first] = sv.second;
            }
            printf("recarga: %s cambio de tamano y no se pudo aplicar, se usa al reiniciar\n", x0.key.empty() ? x0.fpath.c_str() : x0.key.c_str());
            skipped += (int)gk.second.size();
            continue;
        }
        for (auto& pr : gsw) rl_swap[pr.first] = pr.second;
        for (int i : gk.second) {
            PkEntry& x = disc_pk[i];
            { std::lock_guard<std::mutex> g(hold_mx); auto h = hold_map.find(i); if (h != hold_map.end()) { hold_bytes -= h->second.d.size(); hold_map.erase(h); } }
            if (x.sz != now) {
                if (sectors_of(now) > sectors_of(x.sz)) { x.sector = disc_pk_sectors - disc_pk_data; disc_pk_sectors += sectors_of(now); moved = true; }
                x.sz = now;
                uint32_t t = x.sz == x.size0 ? x.real : x.sz;
                uint32_t v[4] = {x.sector ^ KEY_SECTOR, t ^ (uint32_t)i, x.alt == 0xFFFFFFFFu ? (KEY_SECTOR ^ KEY_ALT) : (x.alt ^ KEY_ALT), x.cola};
                memcpy(disc_index.data() + (size_t)(i + 1) * 16, v, 16);
                uint32_t* g = (uint32_t*)psp_p(rl_pf_buf + (uint32_t)(i + 1) * 16);
                g[0] = x.sector + gbase; g[1] = t;
                sized = true;
            }
            done++;
            { std::lock_guard<std::mutex> g(rl_watch_mx); rl_watch_idx.insert(i); rl_pend.insert(i); }
            { std::lock_guard<std::mutex> g(hold_mx); rl_fresh.insert(i); }
            if (shown.size() < 400) shown += (shown.empty() ? "" : ", ") + (x.key.empty() ? x.fpath : x.key);
        }
    }
    rl_tex_swap();
    for (int k = 0; k < disc_cache_n; k++) fclose(disc_cache[k]);
    disc_cache_n = 0;
    if (sized) {
        if (moved) { disc_relocated = true; disc_pk_size = disc_pk_sectors * DISC_SECTOR; }
        rl_spans();
        printf("recarga: indice del PACKFILE actualizado en el juego\n");
    }
    disc_cache_write();
    printf("recarga: %d archivos cambiados, %d entradas recargadas%s%s, %d sin recargar, %.0f ms\n", files, done, shown.empty() ? "" : ": ", shown.c_str(), skipped,
           std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    for (size_t k = 0; k < lost.size() && k < 8; k++) printf("recarga: %s no pertenece a ninguna entrada actual (necesita reiniciar)\n", lost[k].c_str());
}

void psp_disc_reload_poll(bool blocked) {
    if (!rl_req || !disc_ready) return;
    if (blocked) { rl_req = false; printf("recarga: no disponible en combates en red\n"); return; }
    if (disc_reading > 0 || hold_busy >= 0) return;
    rl_req = false;
    rl_apply();
}
