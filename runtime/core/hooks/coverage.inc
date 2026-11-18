// TTT_COVERAGE=1 (coverage/corpus.tsv) or =<file>: guest functions run, HLE imports, indirect and unresolved
// calls with their call site, accumulated into a TSV corpus on exit
#include <map>
#include <string>
#include <tuple>
#include <filesystem>

struct CovRow { uint32_t addr, site; int8_t kind, bank; uint64_t n; };
static const uint32_t COV_CAP = 1u << 18;
static const char* COV_KINDS[] = {"fn", "hle", "jalr", "unresolved_call", "unresolved_jump"};
static CovRow* cov_tab = nullptr;
static uint32_t cov_lost = 0;
static char cov_path[512];   // constant-initialized: psp_coverage_on() runs from another TU's static init
bool psp_cov_on = false;

bool psp_coverage_on() {
    static int done = 0;
    if (done) return psp_cov_on;
    done = 1;
    const char* e = getenv("TTT_COVERAGE");
    if (!e || !*e || !strcmp(e, "0")) return false;
    snprintf(cov_path, sizeof(cov_path), "%s", strcmp(e, "1") ? e : "../../coverage/corpus.tsv");
    cov_tab = (CovRow*)calloc(COV_CAP, sizeof(CovRow));
    psp_cov_on = cov_tab != nullptr;
    printf("coverage: capturing into %s\n", cov_path);
    return psp_cov_on;
}

void psp_coverage_note(int kind, uint32_t addr, uint32_t site) {
    bool in_bank = (addr >= PSP_BANK_VA && addr < PSP_BANK_END) || (site >= PSP_BANK_VA && site < PSP_BANK_END);
    int8_t bank = in_bank ? (int8_t)active_bank : (int8_t)-1;
    uint32_t h = (addr * 2654435761u) ^ (site * 0x85EBCA6Bu) ^ (uint32_t)(kind * 0x27D4EB2Fu) ^ (uint32_t)(bank * 0x165667B1u);
    for (uint32_t i = 0; i < 64; i++) {
        CovRow& r = cov_tab[(h + i) & (COV_CAP - 1)];
        if (!r.n) { r.addr = addr; r.site = site; r.kind = (int8_t)kind; r.bank = bank; r.n = 1; return; }
        if (r.addr == addr && r.site == site && r.kind == kind && r.bank == bank) { r.n++; return; }
    }
    cov_lost++;
}

void psp_coverage_save() {
    if (!psp_cov_on) return;
    using Key = std::tuple<std::string, std::string, std::string, std::string>;
    struct Agg { uint64_t n = 0; long runs = 0; std::string name; bool seen = false; };
    std::map<Key, Agg> all;
    long runs = 0;
    if (FILE* f = fopen(cov_path, "r")) {
        char l[512];
        while (fgets(l, sizeof(l), f)) {
            l[strcspn(l, "\r\n")] = 0;
            if (!strncmp(l, "#runs\t", 6)) { runs = atol(l + 6); continue; }
            char k[32], ov[32], a[16], s[16], nm[160] = "";
            unsigned long long n; long rn;
            if (sscanf(l, "%31[^\t]\t%31[^\t]\t%15[^\t]\t%15[^\t]\t%llu\t%ld\t%159[^\n]", k, ov, a, s, &n, &rn, nm) < 6) continue;
            Agg& g = all[Key(k, ov, a, s)];
            g.n += n; g.runs += rn; if (nm[0]) g.name = nm;
        }
        fclose(f);
    }
    runs++;
    size_t here = 0, fresh = 0;
    for (uint32_t i = 0; i < COV_CAP; i++) {
        const CovRow& r = cov_tab[i];
        if (!r.n || r.kind < 0 || r.kind > COV_JUMP_UNRES) continue;
        char a[16], s[16];
        snprintf(a, sizeof(a), "0x%08X", r.addr);
        snprintf(s, sizeof(s), r.site ? "0x%08X" : "-", r.site);
        const char* ov = (r.bank >= 0 && r.bank < BANKS_N) ? BANKS[r.bank].nombre : "-";
        Agg& g = all[Key(COV_KINDS[(int)r.kind], ov, a, s)];
        if (!g.runs && !g.n) fresh++;
        g.n += r.n;
        if (!g.seen) { g.seen = true; g.runs++; here++; }
        if (r.kind == COV_HLE && g.name.empty())
            for (int j = 0; j < IMPORTS_N; j++)
                if (IMPORTS[j].stub == r.addr) { g.name = std::string(IMPORTS[j].lib) + ":" + IMPORTS[j].nombre; break; }
    }
    std::vector<std::pair<const Key*, const Agg*>> rows;
    for (auto& x : all) rows.push_back({&x.first, &x.second});
    std::stable_sort(rows.begin(), rows.end(), [](auto& p, auto& q) { return p.second->n > q.second->n; });
    std::error_code ec;
    std::filesystem::path out(cov_path);
    if (out.has_parent_path()) std::filesystem::create_directories(out.parent_path(), ec);
    std::string tmp = std::string(cov_path) + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) { printf("coverage: cannot write %s\n", tmp.c_str()); return; }
    fprintf(f, "#runs\t%ld\nkind\toverlay\taddr\tsite\tcount\truns\tname\n", runs);
    for (auto& r : rows)
        fprintf(f, "%s\t%s\t%s\t%s\t%llu\t%ld\t%s\n", std::get<0>(*r.first).c_str(), std::get<1>(*r.first).c_str(),
                std::get<2>(*r.first).c_str(), std::get<3>(*r.first).c_str(), (unsigned long long)r.second->n,
                r.second->runs, r.second->name.c_str());
    fclose(f);
    std::filesystem::rename(tmp, out, ec);
    if (ec) { std::filesystem::remove(out, ec); std::filesystem::rename(tmp, out, ec); }
    printf("coverage: run %ld, %zu rows this run, %zu new, %zu in the corpus%s\n", runs, here, fresh, rows.size(),
           cov_lost ? " (table full, notes lost)" : "");
    if (ec) printf("coverage: cannot replace %s\n", cov_path);
}
