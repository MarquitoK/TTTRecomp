#pragma once
#include <algorithm>
// SZ battle HUD layout (mod sz_hud, SZHud/layout.txt from tools/sz_hud.py): widget trees evaluated into textured quads
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <deque>
#include <tuple>
#include <string_view>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
// interned string: copying a widget copies pointers (the per-frame node copy allocated every name and child list)
struct SzS {
    const std::string* p;
    static const std::string* in(std::string_view s) {   // no allocation for a name already known
        static std::unordered_multimap<size_t, const std::string*> idx; static std::deque<std::string> store;
        size_t h = std::hash<std::string_view>()(s);
        auto r = idx.equal_range(h);
        for (auto i = r.first; i != r.second; ++i) if (*i->second == s) return i->second;
        store.emplace_back(s); idx.emplace(h, &store.back());
        return &store.back();
    }
    SzS() : p(nil()) {}
    static const std::string* nil() { static const std::string* e = in(std::string()); return e; }
    SzS(const std::string& s) : p(in(std::string_view(s))) {}
    SzS(const char* s) : p(in(std::string_view(s))) {}
    operator const std::string&() const { return *p; }
    const std::string& str() const { return *p; }
    const char* c_str() const { return p->c_str(); }
    size_t size() const { return p->size(); }
    bool empty() const { return p->empty(); }
    void clear() { p = nil(); }
    char operator[](size_t i) const { return (*p)[i]; }
    size_t find(const char* s, size_t pos = 0) const { return p->find(s, pos); }
    size_t find(const std::string& s, size_t pos = 0) const { return p->find(s, pos); }
    std::string substr(size_t a, size_t n = std::string::npos) const { return p->substr(a, n); }
    int cmp_p(size_t a, size_t n, const char* s) const { size_t m = strlen(s), k = std::min(n, p->size() > a ? p->size() - a : 0); int r = memcmp(p->data() + a, s, std::min(k, m)); return r ? r : k < m ? -1 : k > m ? 1 : 0; }
    int compare(size_t a, size_t n, const std::string& s) const { return p->compare(a, n, s); }
    template <size_t N> bool operator==(const char (&s)[N]) const { return p->size() == N - 1 && !memcmp(p->data(), s, N - 1); }
    template <size_t N> bool operator!=(const char (&s)[N]) const { return !(*this == s); }
    template <class T, typename std::enable_if<std::is_same<T, const char*>::value || std::is_same<T, char*>::value, int>::type = 0> bool operator==(T s) const { size_t m = strlen(s); return p->size() == m && !memcmp(p->data(), s, m); }
    template <class T, typename std::enable_if<std::is_same<T, const char*>::value || std::is_same<T, char*>::value, int>::type = 0> bool operator!=(T s) const { return !(*this == s); }
    template <class T, typename std::enable_if<std::is_same<T, const char*>::value || std::is_same<T, char*>::value, int>::type = 0> int compare(size_t a, size_t n, T s) const { return cmp_p(a, n, s); }
    template <size_t N> int compare(size_t a, size_t n, const char (&s)[N]) const { return n == N - 1 && p->size() >= a + n ? memcmp(p->data() + a, s, n) : cmp_p(a, n, s); }
    bool operator==(const SzS& o) const { return p == o.p; }
    bool operator!=(const SzS& o) const { return p != o.p; }
    bool operator==(const std::string& s) const { return *p == s; }
    bool operator!=(const std::string& s) const { return *p != s; }
};
inline bool operator==(const std::string& a, const SzS& b) { return b == a; }
inline bool operator<(const std::string& a, const SzS& b) { return a < b.str(); }
inline bool operator<(const SzS& a, const std::string& b) { return a.str() < b; }
inline bool operator!=(const std::string& a, const SzS& b) { return b != a; }
inline std::string operator+(const std::string& a, const SzS& b) { return a + b.str(); }
inline std::string operator+(const char* a, const SzS& b) { return a + b.str(); }
inline std::string operator+(const SzS& a, const char* b) { return a.str() + b; }
inline std::string operator+(const SzS& a, const std::string& b) { return a.str() + b; }
// short list on the stack (the per-frame lists of a node never reach the heap)
template <class T, int N> struct SzV {
    T d[N]; int n = 0;
    T* begin() { return d; } T* end() { return d + n; }
    const T* begin() const { return d; } const T* end() const { return d + n; }
    size_t size() const { return (size_t)n; } bool empty() const { return !n; }
    T& operator[](size_t i) { return d[i]; } const T& operator[](size_t i) const { return d[i]; }
    T& back() { return d[n - 1]; }
    void push_back(const T& v) { if (n < N) d[n++] = v; }
    void clear() { n = 0; }
    void assign(int k, const T& v) { n = 0; for (int i = 0; i < k; i++) push_back(v); }
    template <class It> void append(It a, It b) { for (; a != b; ++a) push_back(*a); }
};
template <class It, class C> static void sz_ssort(It a, It b, C c) {   // stable, no buffer
    for (It i = a; i != b; ++i) for (It j = i; j != a && c(*j, *(j - 1)); --j) std::swap(*j, *(j - 1));
}
// "a/b/c" keys of gauges, made once per combination
static SzS sz_key3(const SzS& a, const SzS& b, const SzS& c) {
    struct H { size_t operator()(const std::tuple<const void*, const void*, const void*>& k) const { return (size_t)std::get<0>(k) * 31 + (size_t)std::get<1>(k) * 17 + (size_t)std::get<2>(k); } };
    static std::unordered_map<std::tuple<const void*, const void*, const void*>, SzS, H> m;
    auto k = std::make_tuple((const void*)a.p, (const void*)b.p, (const void*)c.p);
    auto f = m.find(k);
    if (f != m.end()) return f->second;
    SzS r(a.str() + "/" + b.str() + "/" + c.str());
    m.emplace(k, r);
    return r;
}
static SzS sz_key2(const SzS& a, const SzS& c) {
    static std::unordered_map<const void*, std::unordered_map<const void*, SzS>> m;
    auto& mm = m[a.p];
    auto f = mm.find(c.p);
    if (f != mm.end()) return f->second;
    SzS r(a.str() + "/" + c.str());
    mm.emplace(c.p, r);
    return r;
}
static SzS sz_numbered(const char* fmt, int v) {   // "T_UI_TimeNum_%02d" and the like
    static std::map<std::pair<const char*, int>, SzS> m;
    auto f = m.find({fmt, v});
    if (f != m.end()) return f->second;
    char b[48]; snprintf(b, sizeof b, fmt, v);
    return m.emplace(std::make_pair(fmt, v), SzS(b)).first->second;
}
// child list shared by every copy of a widget
struct SzKids {
    std::vector<int>* v = nullptr;
    static std::vector<int>* none() { static std::vector<int> e; return &e; }
    const std::vector<int>& get() const { return v ? *v : *none(); }
    void push_back(int i) { static std::deque<std::vector<int>> pool; if (!v) { pool.emplace_back(); v = &pool.back(); } v->push_back(i); }
    bool empty() const { return get().empty(); }
    size_t size() const { return get().size(); }
    int operator[](size_t i) const { return get()[i]; }
    std::vector<int>::const_iterator begin() const { return get().begin(); }
    std::vector<int>::const_iterator end() const { return get().end(); }
};
struct SzW {
    int par = -1;
    SzS type, name, img, fx, ucls;   // ucls: the class of a user widget (type "U:<class>")
    char slot = 0;
    float l = 0, t = 0, w = 100, h = 30, anc[4] = {0, 0, 0, 0}, al[2] = {0, 0}, pad[4] = {0, 0, 0, 0}, tr[2] = {0, 0}, sc[2] = {1, 1}, sh[2] = {0, 0};
    float rot = 0, piv[2] = {0.5f, 0.5f}, op = 1, col[4] = {1, 1, 1, 1}, tint[4] = {1, 1, 1, 1}, sz[2] = {0, 0}, ptime = 0, addr = 0, mg[4] = {0, 0, 0, 0}, bal = -1;
    bool hid = false, autosz = false, rev = false;
    char ha = 'F', va = 'F';
    SzKids kids;
};
struct SzMat { std::string parent; std::map<std::string, std::string, std::less<>> tex; std::map<std::string, float, std::less<>> val; };
struct SzX { float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0; };
static SzX sz_mul(const SzX& p, const SzX& q) {
    SzX r;
    r.a = p.a * q.a + p.b * q.c; r.b = p.a * q.b + p.b * q.d;
    r.c = p.c * q.a + p.d * q.c; r.d = p.c * q.b + p.d * q.d;
    r.e = p.a * q.e + p.b * q.f + p.e; r.f = p.c * q.e + p.d * q.f + p.f;
    return r;
}
// one quad: corners (screen), texture and its uv box (0..1), colour; gauge = base masked and cut at value (M_UI_Gauge)
struct SzItem {
    SzS tex, mask, filter, key;
    float x[4], y[4], u0 = 0, v0 = 0, u1 = 1, v1 = 1, r = 1, g = 1, b = 1, a = 1, value = -1;
    bool add = false, vert = false, addq = false;   // addq: additive when the texture has no alpha (plain materials)
};
// values of the panel being drawn; t_* = seconds since the event (negative: never)
// subtree cache (SzLayout::draw): every SzData read inside a draw is recorded with its value (or, for times, with the band it fell in)
struct SzDep { uint16_t off; uint8_t sz, band; uint32_t v; float lo, hi; };
struct SzRec {
    const char* base = nullptr;
    bool bad = false;
    uint64_t seen[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    std::vector<SzDep>* deps = nullptr;
};
inline SzRec* sz_rec = nullptr;
template <class T> static inline void sz_dep(const T* p, int band, float lo, float hi) {
    SzRec* r = sz_rec;
    if (!r) return;
    size_t off = (size_t)((const char*)p - r->base);
    if (off >= 512) { r->bad = true; return; }
    uint64_t& m = r->seen[off >> 6]; uint64_t bit = 1ull << (off & 63);
    if (!band) { if (m & bit) return; m |= bit; }
    SzDep d; d.off = (uint16_t)off; d.sz = sizeof(T); d.band = (uint8_t)band; d.lo = lo; d.hi = hi; d.v = 0;
    if (band) d.v = (uint32_t)band; else memcpy(&d.v, p, sizeof(T));
    r->deps->push_back(d);
}
template <class T> struct SzT {
    T v{};
    SzT() = default;
    SzT(T x) : v(x) {}
    SzT& operator=(T x) { v = x; return *this; }
    SzT& operator++() { ++v; return *this; }
    T operator++(int) { return v++; }
    SzT& operator+=(T x) { v += x; return *this; }
    operator T() const { sz_dep(&v, 0, 0, 0); return v; }
    T raw() const { return v; }
};
// a time whose exact value matters only inside [lo, hi): outside it only the side is recorded
static inline float sz_tq(const SzT<float>& x, float lo, float hi) {
    float v = x.raw();
    if (v >= lo && v < hi) sz_dep(&x.v, 0, 0, 0); else sz_dep(&x.v, v < lo ? 1 : 2, lo, hi);
    return v;
}
// values of the panel being drawn; t_* = seconds since the event (negative: never)
struct SzData {
    SzT<int> hp = 0, trail = 0, ki = 0, kitrail = 0, energy = 0, senzu = -1, orange = 0;
    SzT<bool> spark = false, charging = false;
    SzT<float> now = 0, t_show = 0, t_hit = -1, t_spark = -1, t_unspark = -1, t_charge = -1, t_uncharge = -1, t_unit = -1, t_max = -1, t_break = -1, t_hide = -1;
    SzT<float> cell[5] = {-1, -1, -1, -1, -1}, t_egain = -1, t_estart = -1;
    SzT<float> nw = 0, nh = 0;   // name text size in canvas units (0: no name, no name base)
    SzT<bool> copy = false;   // bottom copy (partner, other opponent): no team pieces (energy, senzu)
    SzT<int> tm = -2, tprev[3] = {0, 0, 0};   // timer: seconds, -1 infinite, -2 none; previous digit of each place (units, tens, hundreds)
    SzT<float> tchg[3] = {-1, -1, -1};   // seconds since each digit changed
    SzT<int> hits = 0, dmg = 0;   // combo counters: hits, damage as displayed
    SzT<float> t_cshow = -1, t_chide = -1, t_dshow = -1, t_dhide = -1;
    SzT<int> nbuf = 0, buf[6] = {0, 0, 0, 0, 0, 0};   // status icons left to right: stat * 2 + (1 when lowered); stat 0 attack, 1 defence, 2 ki, 3 speed
    SzT<float> bshow[6] = {-1, -1, -1, -1, -1, -1}, bout[6] = {-1, -1, -1, -1, -1, -1};
};
static_assert(sizeof(SzData) < 512, "SzData offsets index a 512 bit set");
static std::string sz_field(size_t off) {   // name of the SzData field at a byte offset (cache miss report)
#define SZF(f, n) { offsetof(SzData, f), sizeof(SzData::f), n }
    static const struct { size_t o, s; const char* n; } F[] = {SZF(hp, "hp"), SZF(trail, "trail"), SZF(ki, "ki"), SZF(kitrail, "kitrail"), SZF(energy, "energy"), SZF(senzu, "senzu"), SZF(orange, "orange"),
        SZF(spark, "spark"), SZF(charging, "charging"), SZF(now, "now"), SZF(t_show, "t_show"), SZF(t_hit, "t_hit"), SZF(t_spark, "t_spark"), SZF(t_unspark, "t_unspark"), SZF(t_charge, "t_charge"),
        SZF(t_uncharge, "t_uncharge"), SZF(t_unit, "t_unit"), SZF(t_max, "t_max"), SZF(t_break, "t_break"), SZF(t_hide, "t_hide"), SZF(cell, "cell"), SZF(t_egain, "t_egain"), SZF(t_estart, "t_estart"),
        SZF(nw, "nw"), SZF(nh, "nh"), SZF(copy, "copy"), SZF(tm, "tm"), SZF(tprev, "tprev"), SZF(tchg, "tchg"), SZF(hits, "hits"), SZF(dmg, "dmg"), SZF(t_cshow, "t_cshow"), SZF(t_chide, "t_chide"),
        SZF(t_dshow, "t_dshow"), SZF(t_dhide, "t_dhide"), SZF(nbuf, "nbuf"), SZF(buf, "buf"), SZF(bshow, "bshow"), SZF(bout, "bout")};
#undef SZF
    for (auto& f : F) if (off >= f.o && off < f.o + f.s) return f.n;
    return "?";
}
static inline bool sz_dep_ok(const std::vector<SzDep>& v, const char* base) {
    for (auto& d : v) {
        const char* p = base + d.off;
        if (d.band) { float x; memcpy(&x, p, 4); if ((x < d.lo ? 1u : x >= d.hi ? 2u : 0u) != d.v) return false; }
        else { uint32_t x = 0; memcpy(&x, p, d.sz); if (x != d.v) return false; }
    }
    return true;
}
// state animations (A/K lines): keys t:v:interp:arrive:leave, interp 0 linear, 1 constant, 2 cubic
struct SzKey { float t, v, ar, lv; int m; };
struct SzTrack { SzS who; int prop = 0, ch = 0; std::vector<SzKey> k; };
struct SzAnim {
    float len = 0; std::vector<SzTrack> tr;
    mutable std::unordered_map<const std::string*, std::vector<int>> by_m;   // tracks of a widget, built at first use
    mutable bool by_ok = false;
    const std::vector<int>* by(const SzS& who) const {
        if (!by_ok) { by_ok = true; for (int i = 0; i < (int)tr.size(); i++) if (!tr[i].k.empty()) by_m[tr[i].who.p].push_back(i); }
        auto f = by_m.find(who.p);
        return f == by_m.end() ? nullptr : &f->second;
    }
};

struct SzLayout {
    std::map<std::string, std::vector<SzW>, std::less<>> cls;
    std::map<std::string, SzMat, std::less<>> mat;
    std::map<std::string, std::map<std::string, SzAnim, std::less<>>, std::less<>> anim;
    bool ok = false;

    static bool track(const std::string& pp, SzTrack& t) {
        static const char* RT[] = {"tr.x", "tr.y", "sc.x", "sc.y", "sh.x", "sh.y", "rot"};
        size_t d = pp.find('.');
        if (d == std::string::npos) return false;
        std::string a = pp.substr(0, d), b = pp.substr(d + 1);
        if (a == "RenderOpacity") { t.prop = 0; return true; }
        if (a == "ColorAndOpacity" || a == "ContentColorAndOpacity") { t.prop = 1; t.ch = (int)std::string("rgba").find(b[0]); return b.size() == 1 && t.ch >= 0; }
        if (a == "RenderTransform") { for (int i = 0; i < 7; i++) if (b == RT[i]) { t.prop = 2; t.ch = i; return true; } return false; }
        if (a == "Visibility") { t.prop = 3; return true; }
        if (a == "Brush" && b == "s.Time") { t.prop = 4; return true; }
        if (a == "Brush" && b == "s.AddRate") { t.prop = 5; return true; }
        if (a == "Brush" && b == "s.Alpha") { t.prop = 6; return true; }
        return false;
    }
    static float ev(const std::vector<SzKey>& k, float t) {
        if (t <= k[0].t) return k[0].v;
        for (size_t i = 0; i + 1 < k.size(); i++) if (t < k[i + 1].t) {
            const SzKey &a = k[i], &b = k[i + 1];
            float d = b.t - a.t;
            if (a.m == 1 || d <= 0) return a.v;
            float s = (t - a.t) / d;
            if (a.m == 0) return a.v + (b.v - a.v) * s;
            float s2 = s * s, s3 = s2 * s;
            return (2 * s3 - 3 * s2 + 1) * a.v + (s3 - 2 * s2 + s) * d * a.lv + (-2 * s3 + 3 * s2) * b.v + (s3 - s2) * d * b.ar;
        }
        return k.back().v;
    }
    static void apply(const SzAnim& an, float t, SzW& w) {
        const std::vector<int>* ix = an.by(w.name);
        if (!ix) return;
        for (int ti : *ix) {
            const SzTrack& tr = an.tr[ti];
            float v = ev(tr.k, t);
            if (tr.prop == 0) w.op = v;
            else if (tr.prop == 1) w.col[tr.ch] = v;
            else if (tr.prop == 2) {
                float* f[7] = {&w.tr[0], &w.tr[1], &w.sc[0], &w.sc[1], &w.sh[0], &w.sh[1], &w.rot};
                *f[tr.ch] = v;
            } else if (tr.prop == 3) w.hid = (int)v == 1 || (int)v == 2;
            else if (tr.prop == 4) w.ptime = v;
            else if (tr.prop == 5) w.addr = v;
            else if (tr.prop == 6) w.bal = v;
        }
    }

    static void nums(const std::string& v, float* o, int n) {
        const char* p = v.c_str();
        for (int i = 0; i < n && *p; i++) { o[i] = strtof(p, (char**)&p); if (*p == ',') p++; }
    }
    bool parse(const std::string& text) {
        cache.clear(); ci_p.clear(); ci_s.clear();
        std::istringstream in(text);
        std::string line, cur;
        SzAnim* an = nullptr;
        while (std::getline(in, line)) {
            if (line.size() < 3) continue;
            std::istringstream ls(line);
            std::string k;
            ls >> k;
            if (k == "C") { ls >> cur; cls[cur]; an = nullptr; }
            else if (k == "A" && !cur.empty()) {
                std::string n, l;
                ls >> n >> l;
                an = &anim[cur][n];
                if (!l.compare(0, 4, "len=")) an->len = strtof(l.c_str() + 4, nullptr);
            } else if (k == "K" && an) {
                SzTrack t; std::string pp, ks;
                { std::string wn; ls >> wn >> pp >> ks; t.who = wn; }
                if (!track(pp, t)) continue;
                const char* q = ks.c_str();
                while (*q) {
                    SzKey y; float f[5] = {0, 0, 0, 0, 0};
                    for (int j = 0; j < 5; j++) { f[j] = strtof(q, (char**)&q); if (*q == ':') q++; }
                    y.t = f[0]; y.v = f[1]; y.m = (int)f[2]; y.ar = f[3]; y.lv = f[4];
                    t.k.push_back(y);
                    if (*q == ',') q++; else break;
                }
                if (!t.k.empty()) an->tr.push_back(t);
            }
            else if (k == "W" && !cur.empty()) {
                SzW w; int i;
                std::string ty, nm;
                ls >> i >> w.par >> ty >> nm;
                w.type = ty; w.name = nm; if (!ty.compare(0, 2, "U:")) w.ucls = ty.substr(2);
                std::string tok;
                while (ls >> tok) {
                    size_t e = tok.find('=');
                    if (e == std::string::npos) continue;
                    std::string a = tok.substr(0, e), v = tok.substr(e + 1);
                    if (a == "s") w.slot = v[0];
                    else if (a == "l") w.l = strtof(v.c_str(), nullptr);
                    else if (a == "t") w.t = strtof(v.c_str(), nullptr);
                    else if (a == "w") w.w = strtof(v.c_str(), nullptr);
                    else if (a == "h") w.h = strtof(v.c_str(), nullptr);
                    else if (a == "anc") nums(v, w.anc, 4);
                    else if (a == "al") nums(v, w.al, 2);
                    else if (a == "pad") nums(v, w.pad, 4);
                    else if (a == "tr") nums(v, w.tr, 2);
                    else if (a == "sc") nums(v, w.sc, 2);
                    else if (a == "sh") nums(v, w.sh, 2);
                    else if (a == "rot") w.rot = strtof(v.c_str(), nullptr);
                    else if (a == "piv") nums(v, w.piv, 2);
                    else if (a == "op") w.op = strtof(v.c_str(), nullptr);
                    else if (a == "hid") w.hid = v == "1";
                    else if (a == "auto") w.autosz = v == "1";
                    else if (a == "col") nums(v, w.col, 4);
                    else if (a == "tint") nums(v, w.tint, 4);
                    else if (a == "img") w.img = v;
                    else if (a == "sz") nums(v, w.sz, 2);
                    else if (a == "mg") nums(v, w.mg, 4);
                    else if (a == "rev") w.rev = v == "1";
                    else if (a == "fx") w.fx = v;
                    else if (a == "ha") w.ha = v[0];
                    else if (a == "va") w.va = v[0];
                }
                auto& L = cls[cur];
                if ((int)L.size() != i) return false;
                if (w.par >= 0 && w.par < i) L[w.par].kids.push_back(i);
                L.push_back(w);
            } else if (k == "M") {
                std::string n, tok;
                ls >> n;
                SzMat& m = mat[n];
                while (ls >> tok) {
                    size_t e = tok.find('=');
                    if (e == std::string::npos) continue;
                    std::string a = tok.substr(0, e), v = tok.substr(e + 1);
                    if (a == "parent") m.parent = v;
                    else if (!a.compare(0, 4, "tex.")) m.tex[a.substr(4)] = v;
                    else if (!a.compare(0, 2, "s.")) m.val[a.substr(2)] = strtof(v.c_str(), nullptr);
                }
            }
        }
        ok = cls.count("WBP_GRP_MainGauge_P1") && cls.count("WBP_GRP_MainGauge_P2");
        return ok;
    }

    // texture of a brush: a texture, or the texture of a material instance (none for runtime-set bases)
    std::string brush(const std::string& img) const {
        if (!img.compare(0, 2, "T_")) return img;
        auto m = mat.find(img);
        if (m == mat.end()) return "";
        for (const char* p : {"Texture", "BaseTexture"}) {
            auto t = m->second.tex.find(p);
            if (t != m->second.tex.end() && t->second != "DefaultTexture") return t->second;
        }
        return "";
    }
    // pieces drawn later (name, partner, status icons) or never (particles, texts, layout points)
    static bool skip(const SzW& w) {
        if (w.hid || w.type == "Spacer" || w.type == "SpUIParticle" || w.type == "TextBlock") return true;
        return w.name == "IMG_NameBase" || w.name == "GRP_NameBox" || w.name.find("TeamGauge") != std::string::npos;
    }

    struct Ctx {
        const SzData* d;
        std::vector<SzItem>* out;
        SzS gmask, gfilter, gbase, gkey;
        float gval = -1, col[4] = {1, 1, 1, 1}, at = 0;
        bool gvert = false;
        const SzAnim* an = nullptr;
        int bar = 0, rem = 0, tbar = 0, trem = 0, panel = 0;
        int* senzu_at = nullptr;
        const SzW* ov = nullptr;   // one widget drawn with other fields (senzu digit over its icon)
        int ovi = -1;
        int dig = -1;   // digit place of a combo/damage number
        float dsc = 0;   // scale of a damage digit (measured)
    };
    static int bar_split(int v, int* rem) { int b = v / 10000; *rem = v % 10000; if (*rem == 0 && b) { *rem = 10000; b--; } return b; }

    // desired size of a widget class: extent of the children of its root canvas (docked children add their offset)
    mutable std::unordered_map<std::string, std::vector<std::pair<float, float>>> des_m;   // desired sizes per class and depth (the tree never changes)
    void desired(const std::string& cn, float* W, float* H, int depth = 0) const {
        auto& m = des_m[cn];
        if (m.size() <= (size_t)depth) m.resize(depth + 1, {-1e30f, 0});
        if (m[depth].first > -1e29f) { *W = m[depth].first; *H = m[depth].second; return; }
        desired_calc(cn, W, H, depth);
        des_m[cn][depth] = {*W, *H};
    }
    void desired_calc(const std::string& cn, float* W, float* H, int depth) const {
        *W = *H = 0;
        auto it = cls.find(cn);
        if (it == cls.end() || depth > 6) return;
        const auto& L = it->second;
        for (auto& w : L) {
            if (w.par < 0 || L[w.par].par >= 0 || w.slot != 'C') continue;
            float cw = w.w, ch = w.h;
            if (w.autosz) {
                float dw = 0, dh = 0;
                if (!w.type.compare(0, 2, "U:")) desired(w.ucls, &dw, &dh, depth + 1);
                else kids_size(L, w, &dw, &dh, depth);
                if (dw > 0) cw = dw;
                if (dh > 0) ch = dh;
            }
            if (w.anc[0] == w.anc[2]) *W = std::max(*W, cw + (w.anc[0] == 0 || w.anc[0] == 1 ? fabsf(w.l) : 0));
            if (w.anc[1] == w.anc[3]) *H = std::max(*H, ch + (w.anc[1] == 0 || w.anc[1] == 1 ? fabsf(w.t) : 0));
        }
    }
    void kids_size(const std::vector<SzW>& L, const SzW& w, float* W, float* H, int depth = 0) const {
        *W = w.sz[0]; *H = w.sz[1];
        if (w.type == "HorizontalBox" && !w.kids.empty() && L[w.kids[0]].slot == 'H') {   // a row: children side by side
            *W = *H = 0;
            for (int k : w.kids) {
                const SzW& q = L[k];
                if (q.hid) continue;
                float cw = q.sz[0], ch = q.sz[1];
                if (!q.type.compare(0, 2, "U:")) desired(q.ucls, &cw, &ch, depth + 1);
                *W += cw + q.pad[0] + q.pad[2]; *H = std::max(*H, ch + q.pad[1] + q.pad[3]);
            }
            return;
        }
        for (int k : w.kids) {
            const SzW& q = L[k];
            if (q.slot != 'C') continue;
            float cw = q.w, ch = q.h;
            if (q.autosz && !q.type.compare(0, 2, "U:")) { float dw = 0, dh = 0; desired(q.ucls, &dw, &dh, depth + 1); if (dw > 0) cw = dw; if (dh > 0) ch = dh; }
            if (q.anc[0] == q.anc[2]) *W = std::max(*W, cw + (q.anc[0] == 0 || q.anc[0] == 1 ? fabsf(q.l) : 0));
            if (q.anc[1] == q.anc[3]) *H = std::max(*H, ch + (q.anc[1] == 0 || q.anc[1] == 1 ? fabsf(q.t) : 0));
        }
    }
    // slot rectangle of a widget inside its parent (pw x ph)
    void place(const std::vector<SzW>& L, const SzW& w, float pw, float ph, float* X0, float* Y0, float* Wd, float* Ht) const {
        float x0, y0, W, H;
        if (w.slot == 'C') {
            x0 = w.anc[0] * pw + w.l; y0 = w.anc[1] * ph + w.t;
            W = w.anc[2] != w.anc[0] ? w.anc[2] * pw - w.w - x0 : w.w;
            H = w.anc[3] != w.anc[1] ? w.anc[3] * ph - w.h - y0 : w.h;
            if (w.autosz) {
                float aw = 0, ah = 0;
                if (!w.type.compare(0, 2, "U:")) desired(w.ucls, &aw, &ah);
                else kids_size(L, w, &aw, &ah);
                if (aw > 0) W = aw;
                if (ah > 0) H = ah;
            }
            bool flame = w.type == "U:WBP_OBJ_BuGauge_Ef";   // flipped in place by its animation on the P2 side: placed from its left edge
            if (w.anc[2] == w.anc[0] && !flame) x0 -= w.al[0] * W;
            if (w.anc[3] == w.anc[1]) y0 -= w.al[1] * H;
        } else if (w.slot) {
            x0 = w.pad[0]; y0 = w.pad[1]; W = pw - w.pad[0] - w.pad[2]; H = ph - w.pad[1] - w.pad[3];
            float dw = w.sz[0], dh = w.sz[1];
            if (!w.type.compare(0, 2, "U:") && (w.ha != 'F' || w.va != 'F')) desired(w.ucls, &dw, &dh);   // aligned user widgets take their desired size
            if (w.ha != 'F' && dw > 0) { float s = W - dw; W = dw; x0 += w.ha == 'C' ? s * 0.5f : w.ha == 'R' ? s : 0; }
            if (w.va != 'F' && dh > 0) { float s = H - dh; H = dh; y0 += w.va == 'C' ? s * 0.5f : w.va == 'B' ? s : 0; }
        } else { x0 = y0 = 0; W = pw; H = ph; }
        *X0 = x0; *Y0 = y0; *Wd = W; *Ht = H;
    }
    // tracking points of a gauge (StartObject, Route_NN, EndObject among the siblings): position and angle at v 0..1
    bool route(const std::vector<SzW>& L, int par, const char* pre, float v, float pw, float ph, float* x, float* y, float* rot, float ylev = -1) const {
        if (par < 0) return false;
        SzV<const SzW*, 64> P;
        char a[48], r[48], e[48]; snprintf(a, sizeof a, "%sStartObject", pre); snprintf(r, sizeof r, "%sRoute_", pre); snprintf(e, sizeof e, "%sEndObject", pre); size_t rn = strlen(r);
        for (int k : L[par].kids) if (L[k].name == (const char*)a) P.push_back(&L[k]);
        SzV<const SzW*, 64> mid;
        for (int k : L[par].kids) if (L[k].name.size() >= rn && !memcmp(L[k].name.c_str(), r, rn)) mid.push_back(&L[k]);
        std::sort(mid.begin(), mid.end(), [](const SzW* p, const SzW* q) { return p->name.str() < q->name.str(); });
        P.append(mid.begin(), mid.end());
        for (int k : L[par].kids) if (L[k].name == (const char*)e) P.push_back(&L[k]);
        if (P.size() < 2) return false;
        SzV<float, 64> px, py, len; len.push_back(0.0f);
        for (auto* q : P) { float x0, y0, W, H; place(L, *q, pw, ph, &x0, &y0, &W, &H); px.push_back(x0); py.push_back(y0); }
        for (size_t i = 1; i < P.size(); i++) len.push_back(len.back() + hypotf(px[i] - px[i - 1], py[i] - py[i - 1]));
        if (ylev >= 0) {   // the point where the route crosses a fill line (vertical gauges)
            for (size_t k = 1; k < P.size(); k++) {
                float a0 = py[k - 1], a1 = py[k];
                if ((ylev - a0) * (ylev - a1) > 0 || a0 == a1) continue;
                float s = (ylev - a0) / (a1 - a0);
                *x = px[k - 1] + (px[k] - px[k - 1]) * s; *y = ylev;
                *rot = P[k - 1]->rot + (P[k]->rot - P[k - 1]->rot) * s;
                return true;
            }
        }
        float d = std::max(0.0f, std::min(1.0f, v)) * len.back();
        size_t i = 1;
        while (i + 1 < P.size() && len[i] < d) i++;
        float s = len[i] > len[i - 1] ? (d - len[i - 1]) / (len[i] - len[i - 1]) : 0;
        *x = px[i - 1] + (px[i] - px[i - 1]) * s; *y = py[i - 1] + (py[i] - py[i - 1]) * s;
        *rot = P[i - 1]->rot + (P[i]->rot - P[i - 1]->rot) * s;
        return true;
    }

    struct ClsF { bool combo13, main, sp, cset, r4, hud, rtime, otimer, rep, hp, chg, king, style, bu, num, combo, tcn, otn, cnum, dnum, buff; };
    mutable std::unordered_map<const void*, ClsF> clsf_m;
    const ClsF& clsf(const std::string& cn, const std::vector<SzW>& L) const {
        auto it = clsf_m.find(&L);
        if (it != clsf_m.end()) return it->second;
        auto has = [&](const char* s) { return cn.find(s) != std::string::npos; };
        ClsF f;
        f.combo13 = !cn.compare(0, 13, "WBP_OBJ_Combo"); f.main = has("MainGauge"); f.sp = cn == "WBP_GRP_SpGauge";
        f.cset = !cn.compare(0, 18, "WBP_Rep_ComboSet_P") || !cn.compare(0, 18, "WBP_GRP_ComboSet_P"); f.r4 = cn.size() > 4 && cn[4] == 'R';
        f.hud = cn == "WBP_MainGame_HUD"; f.rtime = cn == "WBP_Rep_TimeCount"; f.otimer = cn == "WBP_OBJ_Timer"; f.rep = !cn.compare(0, 17, "WBP_Rep_MainGauge");
        f.hp = has("HpGauge"); f.chg = has("ChargeOverEf"); f.king = cn == "WBP_OBJ_KingGauge"; f.style = has("StyleIcon_Timer"); f.bu = has("BuGauge_P");
        f.num = has("OBJ_Combo_P") || has("OBJ_Damage_P"); f.combo = has("Combo");
        f.tcn = cn == "WBP_Rep_TimeCountNum"; f.otn = cn == "WBP_OBJ_TimerNum"; f.cnum = cn == "WBP_OBJ_ComboNum"; f.dnum = cn == "WBP_OBJ_DamageNum"; f.buff = cn == "WBP_GRP_BuffIcon";
        return clsf_m.emplace(&L, f).first->second;
    }
    void node(const std::string& cn, const std::vector<SzW>& L, int i, SzX xf, float pw, float ph, float op, const std::string& inst, Ctx& c) const {
        if (crec && L[i].kids.empty() && !(c.ov && i == c.ovi) && node_rec(cn, L, i, xf, pw, ph, op, inst, c)) return;
        node_body(cn, L, i, xf, pw, ph, op, inst, c);
    }
    void node_body(const std::string& cn, const std::vector<SzW>& L, int i, SzX xf, float pw, float ph, float op, const std::string& inst, Ctx& c) const {
        const ClsF& F = clsf(cn, L);
        SzW w = c.ov && i == c.ovi ? *c.ov : L[i];
        if (c.an) apply(*c.an, c.at, w);
        if (w.name == "IMG_CombText_R" && F.combo13) w.hid = false;
        if (w.name == "CharaNum" && c.d->senzu >= 0) { if (c.senzu_at) *c.senzu_at = i; return; }   // senzu count: drawn over the icon frame (as in SZ)
        if (w.name == "ON_Eff" || w.name == "IMG_EF_0") { if (c.d->senzu <= 0) return; }   // senzu left: the lit disc under the arrow (IMG_ON_01)
        else if (w.type == "SpUIParticle" && !w.hid && !w.name.compare(0, 3, "PR_")) {}   // energy particles: rebuilt by hand (emit)
        else if (skip(w) && !(c.d->nw > 0 && !w.hid && (w.name == "IMG_NameBase" || w.name == "GRP_NameBox" || w.name == "UserName"))) return;
        const SzData& dd = *c.d;
        bool main = F.main;
        // measured on SZ captures (not in the exported data): stocks and ki row 60 units toward the portrait, stocks tilted 6, each ki cell along the row
        if (main && (w.name == "WBP_HpStockSet" || w.name == "WBP_SpecialGauge")) w.l += c.panel ? 60 : -60;
        if (main && w.name == "WBP_HpStockSet") w.rot += c.panel ? -6 : 6;
        if (F.sp && !w.name.compare(0, 8, "SpGauge_")) {
            float sl = 0, st = 0;
            for (auto& q : L) if (!q.name.compare(0, 8, "SpGauge_") && &q != &L[i]) { float dl = q.l - L[i].l, dt = q.t - L[i].t; if (dl < 0) { dl = -dl; dt = -dt; } sl += dl; st += dt; }
            if (sl > 0) w.rot += atan2f(st, sl) * 180 / 3.14159265f;
            w.piv[0] = 0.8f; w.piv[1] = 0.867f;
        }
        if (main && w.name == "GRP_SparkingBase") {   // P1 has no track for it: shown while sparking, short fades
            if (dd.spark) { float ts = sz_tq(dd.t_spark, 0, 0.2f); w.op = ts >= 0 ? std::min(1.0f, ts / 0.2f) : 1; }
            else { float tu = sz_tq(dd.t_unspark, 0, 0.2f); if (tu >= 0 && tu < 0.2f) w.op = 1 - tu / 0.2f; }
        }
        if (dd.copy && (w.name == "WBP_BuGauge" || w.name == "WBP_OBJ_BuOverEf" || !w.name.compare(0, 23, "WBP_OBJ_StyleIcon_Timer"))) return;
        if (F.cset) {
            if ((w.name == "WBP_OBJ_Combo" && sz_tq(dd.t_cshow, 0, 0) < 0) || (w.name == "WBP_OBJ_Damage" && sz_tq(dd.t_dshow, 0, 0) < 0)) return;
            if (w.name == "WBP_OBJ_Damage" && F.r4) { w.tr[0] += c.panel ? -SZ_DX : SZ_DX; w.tr[1] += SZ_DY; }
        }
        if (F.hud && w.name == "WBP_OBJ_Timer") { w.sc[0] *= 0.45f; w.sc[1] *= 0.45f; w.t = 148; }   // measured on SZ footage (number height and centre)
        if (F.rtime) {
            if (!w.name.compare(0, 17, "WBP_Rep_TimerNum_") && w.name.size() > 18) { int k = w.name[18] - '0'; if ((k == 2 && dd.tm < 100) || (k == 1 && dd.tm < 10)) return; }
            if ((w.name == "IMG_Infinite" && dd.tm != -1) || ((w.name == "Time" || w.name == "Time_01") && dd.tm < 0)) return;
        }
        if (F.otimer && ((w.name == "IMG_Infinite" && dd.tm != -1) || (w.name == "GRP_TimeNum" && dd.tm < 0))) return;
        bool rep = F.rep;   // SZ's in-battle panel: only its start lightning is taken (fx)
        if (rep && w.par >= 0 && L[w.par].name == "GRP_All" && w.name != "ShowEffectBox") return;
        if (rep && w.slot == 'O' && w.ha == 'F') w.ha = 'L';   // overlay slots default to left
        if (main && !rep && !w.name.compare(0, 11, "IMG_Thunder")) return;   // the HUD class's copy (short flashes): SZ plays the Rep one
        if (w.name == "WBP_Active_Thunder") w.l = -40;   // over the gauge (its exported offset leaves it outside)
        if (w.name == "WBP_OBJ_HpOverEf" && F.hp) return;   // P2 leftover: the panel has its own (GRP_HitEF)
        if (F.chg && w.name == "GRP_Thunder") w.sc[0] *= std::min(1.0f, dd.orange / 30000.0f * 656 / 540);   // never longer than the filled part
        float x0, y0, W, H;
        place(L, w, pw, ph, &x0, &y0, &W, &H);
        if (F.rtime && (w.name == "Time" || w.name == "Time_01")) {   // auto size = the digits shown (pitch 64, digit 68, right margin 36)
            int n = dd.tm >= 100 ? 3 : dd.tm >= 10 ? 2 : 1;
            W = 72 + 64.0f * (n - 1); x0 = w.anc[0] * pw + w.l - w.al[0] * W;
        }
        if (w.name == "GRP_NameBox") { W = dd.nw + 12; H = dd.nh; x0 = w.anc[0] * pw + w.l - w.al[0] * W; y0 = w.anc[1] * ph + w.t - w.al[1] * H; }   // auto size = the text (UserName pads 6 each side)
        if (F.king && w.par < 0 && W > 0 && H > 0) {   // charge and sparking gauges lie along the ki row (measured on SZ), turned about the gauge centre
            auto g = cls.find("WBP_GRP_SpGauge");
            float sl = 0, st = 0;
            if (g != cls.end()) {
                const SzW *a = nullptr, *b = nullptr;
                for (auto& q : g->second) if (!q.name.compare(0, 8, "SpGauge_")) { if (!a || q.l < a->l) a = &q; if (!b || q.l > b->l) b = &q; }
                if (a && b && b->l > a->l) { sl = b->l - a->l; st = b->t - a->t; }
            }
            if (sl > 0) { w.rot += atan2f(st, sl) * 180 / 3.14159265f; w.piv[0] = 390 / W; w.piv[1] = 70 / H; }
        }
        {   // effects that follow the end of a gauge
            const char* pre = nullptr; float v = 0;
            if (w.name == "GRP_HitEF") { pre = ""; v = dd.hp > 0 ? c.rem / 10000.0f : 0; if (c.panel) w.sc[0] = -w.sc[0]; }   // mirrored for P2 (as its Rep twin)
            else if (w.name == "WBP_OBJ_BuOverEf") { pre = ""; v = (dd.energy % 100000) / 100000.0f; if (dd.energy >= 500000) return; float tg = sz_tq(dd.t_egain, 0, 0.35f); if (tg < 0 || tg > 0.35f) return; }
            else if (w.name == "WBP_ChargeOverEF") { pre = "Charge_"; v = dd.orange / 30000.0f; if (dd.spark || dd.orange <= 0) return; }
            else if (w.name == "WBP_OBJ_ActiveOverEF") { pre = "Active_"; v = dd.orange / 30000.0f; if (!dd.spark) return; }
            float rx, ry, rr2, yl = -1;
            if (w.name == "WBP_OBJ_BuOverEf") for (auto& q : L) if (q.name == "GRP_BuGauge") { float q0, q1, qw, qh; place(L, q, pw, ph, &q0, &q1, &qw, &qh); yl = q1 + (1 - (0.088f + v * (0.919f - 0.088f))) * qh; }   // fill line of the energy gauge (span measured in game: 0.088..0.919)
            if (pre && route(L, w.par, pre, v, pw, ph, &rx, &ry, &rr2, yl)) { x0 = rx; y0 = ry; w.rot += rr2; }
        }
        float px = w.piv[0] * W, py = w.piv[1] * H, rr = w.rot * 3.14159265f / 180.0f, cs = cosf(rr), sn = sinf(rr);
        float kx = tanf(w.sh[0] * 3.14159265f / 180.0f), ky = tanf(w.sh[1] * 3.14159265f / 180.0f);
        // p' = R * Sh * S * (p - piv) + piv + offset
        float m00 = w.sc[0], m01 = kx * w.sc[1], m10 = ky * w.sc[0], m11 = w.sc[1];
        SzX L2;
        L2.a = cs * m00 - sn * m10; L2.b = cs * m01 - sn * m11;
        L2.c = sn * m00 + cs * m10; L2.d = sn * m01 + cs * m11;
        L2.e = x0 + w.tr[0] + px - (L2.a * px + L2.b * py);
        L2.f = y0 + w.tr[1] + py - (L2.c * px + L2.d * py);
        SzX X = sz_mul(xf, L2);
        float o = op * w.op;
        if (o <= 0) return;
        Ctx cc = c;
        if (w.type != "Image") for (int k = 0; k < 4; k++) cc.col[k] *= w.col[k];
        if (cc.col[3] <= 0) return;
        if (w.type == "SpUIParticle") { emit(w, X, W, H, o, cc); return; }
        if (w.name == "UserName") {   // the name texture, made by sz_hud.inc
            SzItem it;
            it.tex = "@name";
            float px4[4] = {0, W, W, 0}, py4[4] = {0, 0, H, H};
            for (int k = 0; k < 4; k++) { it.x[k] = X.a * px4[k] + X.b * py4[k] + X.e; it.y[k] = X.c * px4[k] + X.d * py4[k] + X.f; }
            it.r = cc.col[0]; it.g = cc.col[1]; it.b = cc.col[2]; it.a = o * cc.col[3];
            if (it.a > 0.003f) c.out->push_back(it);
            return;
        }
        if (w.type == "RetainerBox" && !w.fx.empty()) {
            auto m = mat.find(w.fx);
            if (m != mat.end() && !m->second.parent.compare(0, 10, "M_UI_Gauge")) {
                auto get = [&](const char* k) { auto t = m->second.tex.find(k); return t == m->second.tex.end() || t->second == "DefaultTexture" ? std::string() : t->second; };
                cc.gmask = get("MaskTexture"); cc.gfilter = get("FilterH"); cc.gbase = get("BaseTexture");
                auto v = m->second.val.find("Value");
                cc.gval = v == m->second.val.end() ? 1 : v->second;
                cc.gkey = sz_key3(SzS(cn), SzS(inst), w.name);
                if (!bind_gauge(cn, inst, w.name, cc)) return;
            }
        }
        if (w.type == "Image") item(cn, w, inst, X, W, H, o, cc, F);
        else if (c.dsc > 0 && w.type == "U:WBP_OBJ_DamageNum") {   // damage digit at its measured size, bottom on the row's
            SzX t; t.a = t.d = c.dsc; t.e = -x0; t.f = ph - 96 * c.dsc - y0;
            cc.an = nullptr; cc.dsc = 0;
            draw("WBP_OBJ_DamageNum", sz_mul(X, t), 88, 96, o, w.name, cc);
            return;
        } else if (!w.type.compare(0, 2, "U:")) {
            SzX M = X;
            if (c.panel && (w.type == "U:WBP_GRP_SpGauge" || w.type == "U:WBP_OBJ_KingGauge")) { SzX f; f.a = -1; f.e = W; M = sz_mul(X, f); }   // shared classes (ki row, sparking gauge) are mirrored for P2 at runtime
            cc.an = nullptr; cc.gval = -1; cc.gkey.clear(); cc.gmask.clear(); cc.gfilter.clear(); cc.gbase.clear(); cc.gvert = false;
            draw(w.ucls, M, W, H, o, w.name, cc);
            return;
        }
        SzV<int, 96> ks; ks.append(w.kids.begin(), w.kids.end());
        if (F.hp && w.name == "CanvasPanel_45") {
            static const char* O[] = {"IMG_Base", "GRP_NextBar", "GRP_DamageGauge", "GRP_CurrentGauge"};
            ks.clear();
            for (auto* n : O) for (int k : w.kids) if (L[k].name == n) ks.push_back(k);
        }
        if (F.style && w.par < 0) {   // the lit disc goes under the arrow and the frame
            auto on = std::find_if(ks.begin(), ks.end(), [&](int k) { return L[k].name == "ON_Eff"; });
            auto off = std::find_if(ks.begin(), ks.end(), [&](int k) { return L[k].name == "IMG_OFF"; });
            if (on != ks.end() && off != ks.end() && on > off) std::rotate(off + 1, on, on + 1);
        }
        if (w.name == "GRP_BaseSet") {   // name base under the name on both sides (P1 lists it after the text, P2 before)
            auto nb = std::find_if(ks.begin(), ks.end(), [&](int k) { return L[k].name == "IMG_NameBase"; });
            auto bx = std::find_if(ks.begin(), ks.end(), [&](int k) { return L[k].name == "GRP_NameBox"; });
            if (nb != ks.end() && bx != ks.end() && bx < nb) std::rotate(bx, nb, nb + 1);
        }
        if (w.name == "GRP_All") {   // the panel base, then the portrait, under every gauge on both sides (P2 lists the base last, P1 the portrait late)
            auto rk = [&](int k) { return L[k].name == "GRP_BaseSet" ? 0 : L[k].name == "IMG_Face" ? 1 : 2; };
            sz_ssort(ks.begin(), ks.end(), [&](int a, int b) { return rk(a) < rk(b); });
            int hx = -1, hg = -1;   // life-end shadow and hit flash over the gauge (as on SZ footage)
            for (int i = 0; i < (int)ks.size(); i++) { if (L[ks[i]].name == "GRP_HitEF") hx = i; else if (L[ks[i]].name.find("HpGauge") != std::string::npos) hg = i; }
            if (hx >= 0 && hx < hg) std::rotate(ks.begin() + hx, ks.begin() + hx + 1, ks.begin() + hg + 1);
        }
        if (w.par < 0 && F.bu)   // the empty arc under the energy fill (P2 lists it after the fill)
            sz_ssort(ks.begin(), ks.end(), [&](int a, int b) { return (L[a].name == "GRP_Base") > (L[b].name == "GRP_Base"); });
        if (F.rtime && w.par < 0)   // the start glow under the lightning (as on SZ footage)
            sz_ssort(ks.begin(), ks.end(), [&](int a, int b) { return (L[a].name == "IMG_Start_Eff_Base") > (L[b].name == "IMG_Start_Eff_Base"); });
        if (w.type == "WidgetSwitcher" && !ks.empty()) {   // one page shown: the Rep timer's Alart page from 10 s down, else Wait (the first)
            const char* want = F.rtime && dd.tm >= 0 && dd.tm <= 10 ? "Alart" : nullptr;
            int pick1 = ks[0];
            if (want) for (int k : ks) if (L[k].name == want) pick1 = k;
            ks.assign(1, pick1);
        }
        if (w.type == "HorizontalBox") {   // children one after the other at their desired widths (collapsed ones out), centred
            SzV<std::pair<int, float>, 96> hb;
            SzV<int, 96> dg;
            float tot = 0;
            // combo/damage rows: digit NN = place NN (leading zeros collapsed); the text after the number (R) as SZ shows it in es/en
            bool num = F.num;
            int nd = 1;
            if (num) for (int v = F.combo ? dd.hits : dd.dmg; v >= 10; v /= 10) nd++;
            if (w.rev) std::reverse(ks.begin(), ks.end());   // bReverse: laid from the right edge
            for (int k : ks) {
                const SzW& q = L[k];
                int p = -1;
                if (num && (!q.type.compare(0, 14, "U:WBP_OBJ_Comb") || !q.type.compare(0, 14, "U:WBP_OBJ_Dama"))) { p = atoi(q.name.c_str() + q.name.size() - 2); if (p >= nd) continue; }
                if (num && q.name == "IMG_CombText_L") continue;
                if (q.type == "U:WBP_GRP_BuffIcon" && atoi(q.name.c_str() + q.name.size() - 1) >= dd.nbuf) continue;
                if ((q.hid && !(num && q.name == "IMG_CombText_R")) || (q.name == "WBP_OBJ_TimerNum_02" && dd.tm < 100) || (q.name == "WBP_OBJ_TimerNum_01" && dd.tm < 10)) continue;
                dg.push_back(p);
                float dw = q.sz[0], dh = 0;
                if (!q.type.compare(0, 2, "U:")) desired(q.ucls, &dw, &dh);
                else if (q.name == "UserName") dw = dd.nw;
                dw += q.pad[0] + q.pad[2];
                if (p >= 0 && q.type == "U:WBP_OBJ_DamageNum") dw = p < 3 ? SZ_DPS : SZ_DPB;
                if (q.type == "U:WBP_GRP_BuffIcon") dw = 92;   // pitch of SZ's in-battle list (WBP_Rep_BuffIcon slots 0, 92, 184...)
                hb.push_back({k, dw}); tot += dw;
            }
            float x = w.rev ? W - tot : w.ha == 'C' || F.otimer ? (W - tot) * 0.5f : 0;
            for (size_t e = 0; e < hb.size(); e++) { SzX t; t.e = x; cc.dig = dg[e]; cc.dsc = dg[e] < 0 ? 0 : dg[e] < 3 ? SZ_DKS : SZ_DKB; node(cn, L, hb[e].first, sz_mul(X, t), hb[e].second, H, o, inst, cc); x += hb[e].second; }
            return;
        }
        for (int k : ks) node(cn, L, k, X, W, H, o, inst, cc);
    }

    // particles of the energy number (not exportable; rebuilt from its sprites and SZ footage): stateless, every particle is a
    // function of its spawn index and the time. PR_MaxFire / PR_MaxLoop: rising flames (their opacity comes from ANI_Max/MaxWait),
    // PR_SingleOn: a burst of sparks on each unit gained
    static float rnd(uint32_t h) { h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16; return (h & 0xFFFFFF) / 16777216.0f; }
    void emit(const SzW& w, const SzX& X, float W, float H, float o, Ctx& c) const {
        const SzData& d = *c.d;
        static const char* FL[6] = {"T_UI_NumEF_10", "T_UI_NumEF_11", "T_UI_NumEF_12", "T_UI_NumEF_13", "T_UI_NumEF_14", "T_UI_NumEF_15"};
        static const float FS[6][2] = {{52, 116}, {48, 164}, {40, 144}, {84, 196}, {32, 116}, {112, 196}};
        int kind = w.name == "PR_MaxFire" ? 0 : w.name == "PR_MaxLoop" ? 1 : w.name == "PR_SingleOn" ? 2 : -1;
        if (kind < 0 || o <= 0.003f) return;
        auto quad = [&](const char* tex, float cx, float by, float sw, float sh, float a) {
            SzItem it;
            it.tex = tex; it.add = true;
            float px[4] = {cx - sw * 0.5f, cx + sw * 0.5f, cx + sw * 0.5f, cx - sw * 0.5f}, py[4] = {by - sh, by - sh, by, by};
            for (int k = 0; k < 4; k++) { it.x[k] = X.a * px[k] + X.b * py[k] + X.e; it.y[k] = X.c * px[k] + X.d * py[k] + X.f; }
            it.r = c.col[0]; it.g = c.col[1]; it.b = c.col[2]; it.a = a * c.col[3];
            if (it.a > 0.003f) c.out->push_back(it);
        };
        uint32_t seed = (uint32_t)kind * 7919u + (uint32_t)c.panel * 104729u;
        if (kind == 2) {
            float tu = sz_tq(d.t_unit, 0, 0.4f);
            if (tu < 0 || tu >= 0.4f) return;
            float s = d.t_unit / 0.4f, e = 1 - (1 - s) * (1 - s);
            uint32_t b = seed + (uint32_t)lroundf(d.now - d.t_unit) * 31u;
            for (int i = 0; i < 12; i++) {
                float an = (i + rnd(b + i * 3)) * 6.2831853f / 12, dist = (40 + 50 * rnd(b + i * 3 + 1)) * e, sz = 18 + 16 * rnd(b + i * 3 + 2);
                quad("T_UI_NumEF_00", W * 0.5f + cosf(an) * dist, H * 0.5f + sinf(an) * dist + sz * 0.5f, sz, sz, o * (1 - s));
            }
            return;
        }
        float rate = kind ? 9.0f : 14.0f, life = kind ? 0.9f : 0.6f, rise = kind ? 70.0f : 34.0f, hs = kind ? 1.0f : 0.5f, amax = kind ? 0.55f : 0.85f;
        long i1 = (long)floorf(d.now * rate), i0 = (long)floorf((d.now - life) * rate) + 1;
        for (long i = i0; i <= i1; i++) {
            float t = d.now - i / rate, s = t / life;
            if (s < 0 || s >= 1) continue;
            uint32_t h = seed + (uint32_t)i * 2654435761u;
            int f = (int)(rnd(h) * 6) % 6;
            float k = hs * (0.65f + 0.5f * rnd(h + 1)), x = W * (0.1f + 0.8f * rnd(h + 2)) + (rnd(h + 3) - 0.5f) * 24 * s;
            float y = H - rise * s, a = o * amax * sinf(3.14159265f * std::min(1.0f, s * 1.25f)) * (0.6f + 0.4f * rnd(h + 4));
            quad(FL[f], x, y, FS[f][0] * k, FS[f][1] * k * (0.85f + 0.3f * s), a);
        }
    }

    // values of the gauges: HP top bar, trail, next bar, ki cells, energy fill
    bool bind_gauge(const std::string& cn, const std::string& inst, const std::string& n, Ctx& c) const {
        const SzData& d = *c.d;
        if (cn.find("HpGauge") != std::string::npos) {
            static const char* B[] = {"T_UI_HpGauge_Bar_01", "T_UI_HpGauge_Bar_02", "T_UI_HpGauge_Bar_03", "T_UI_HpGauge_Bar_04", "T_UI_HpGauge_Bar_05",
                                      "T_UI_HpGauge_Bar_06", "T_UI_HpGauge_Bar_07", "T_UI_HpGauge_Bar_08", "T_UI_HpGauge_Bar_09"};
            int k = std::min(c.bar, 8);
            if (n == "GRP_CurrentGauge") { c.gbase = B[k]; c.gval = d.hp > 0 ? c.rem / 10000.0f : 0; }
            else if (n == "GRP_NextBar") { if (c.bar < 1) return false; c.gbase = B[std::min(c.bar - 1, 8)]; c.gval = 1; }
            else if (n == "GRP_DamageGauge") { c.gbase = "T_UI_HpGauge_Bar_Damage"; c.gval = c.tbar > c.bar ? 1 : c.trem / 10000.0f; if (d.trail <= d.hp) return false; }
        } else if (n == "GRP_BuGauge") {   // energy fills bottom to top
            c.gval = d.energy >= 500000 ? 1 : (d.energy % 100000) / 100000.0f;
            if (c.gfilter == "T_Dev_UI_GaugeFilterH") { c.gfilter.clear(); c.gvert = true; }
        } else if (n == "GRP_ChargeMain" || n == "GRP_ChargeEF") c.gval = d.spark ? 0 : std::min(1.0f, d.orange / 30000.0f);
        else if (n == "GRP_ActiveMain") c.gval = d.spark ? std::min(1.0f, d.orange / 30000.0f) : 0;
        else if (n == "GRP_ActiveDamage") return false;
        return c.gval > 0;
    }

    // what item() reads from a widget's material, worked out once per image name
    struct MatC { bool has = false, cadd = false, add = false, noalpha = false, plain = false, ymaj = false, autof = false, brink = false; float alpha = 1, alpha_v = 1, tx = 0, ty = 0, pn = 0, speed = 1; SzS brush; };
    mutable std::unordered_map<const std::string*, MatC> matc_m;
    const MatC& matc(const SzS& img) const {
        auto f = matc_m.find(img.p);
        if (f != matc_m.end()) return f->second;
        MatC r;
        r.brush = brush(img);
        auto m = mat.find(img);
        if (m != mat.end()) {
            const std::string& pa = m->second.parent;
            auto V = [&](const char* k, float dv) { auto x = m->second.val.find(k); return x == m->second.val.end() ? dv : x->second; };
            r.has = true;
            r.cadd = pa.find("ColorAdd") != std::string::npos;
            r.add = !r.cadd && (pa.find("Add") != std::string::npos || pa.find("Glow") != std::string::npos || pa.find("Pattern") != std::string::npos);
            r.noalpha = r.cadd && pa.find("NoAlpha") != std::string::npos;
            auto x = m->second.val.find("Alpha");
            r.alpha = x == m->second.val.end() ? 1 : x->second;
            r.alpha_v = V("Alpha", 1);
            r.plain = m->second.val.count("Plain") > 0;
            r.tx = V("TilingX", 0); r.ty = V("TilingY", 0); r.pn = V("PatternNum", 0); r.speed = V("Speed", 1);
            r.ymaj = pa.size() > 2 && (!pa.compare(pa.size() - 2, 2, "_Y") || pa.find("_Y_") != std::string::npos);
            r.autof = pa.find("Auto") != std::string::npos;
            r.brink = pa == "M_UI_Brink_Add";
        }
        return matc_m.emplace(img.p, r).first->second;
    }
    void item(const std::string& cn, const SzW& w, const std::string& inst, const SzX& X, float W, float H, float o, Ctx& c, const ClsF& F) const {
        SzItem it;
        const MatC& mc = matc(w.img);
        it.tex = w.name == "IMG_Face" ? SzS("@face") : mc.brush;   // the fighter's portrait, made by sz_hud.inc
        const SzData& d = *c.d;
        if (F.tcn && !w.img.compare(0, 12, "T_UI_TimeNum") && inst.size() > 18) {
            int k = std::max(0, std::min(2, inst[18] - '0')), p = k == 2 ? 100 : k == 1 ? 10 : 1;
            it.tex = sz_numbered("T_UI_TimeNum_%02d", std::max(0, (int)d.tm) / p % 10);
        }
        bool radial = !w.img.compare(0, 20, "M_UI_Start_RadioGrad");   // radial gradient (shader only, made by sz_hud.inc)
        if (radial) it.tex = w.img.find("_Add") != std::string::npos ? "@radial2" : "@radial";
        if (F.otn && inst.size() > 2 && (w.name == "IMG_Num_A" || w.name == "IMG_Num_B")) {
            int k = std::max(0, std::min(2, inst.back() - '0')), p = k == 2 ? 100 : k == 1 ? 10 : 1;
            it.tex = sz_numbered("T_UI_TimeNum_%02d", w.name == "IMG_Num_A" ? std::max(0, (int)d.tm) / p % 10 : (int)d.tprev[k]);
        }
        if ((F.cnum || F.dnum) && (w.name == "IMG_Num_A" || w.name == "IMG_Num_B")) {
            if (w.name == "IMG_Num_B" || c.dig < 0) return;
            bool cb = F.cnum;
            int v = cb ? d.hits : d.dmg;
            for (int k = 0; k < c.dig; k++) v /= 10;
            it.tex = sz_numbered(cb ? "T_UI_ComboNum_%02d" : "T_UI_DamageoNum_%02d", v % 10);
        }
        if (w.name == "IMG_Icon" && F.buff) {
            int k = std::min(5, atoi(inst.c_str() + inst.size() - 1)), b = d.buf[k];
            it.tex = sz_numbered(b & 1 ? "T_UI_HUD_Debuff_%02d" : "T_UI_HUD_Buff_%02d", b / 2 + 1);
        }
        if (w.name == "IMG_BuNum") { it.tex = sz_numbered("T_UI_BuNum_%02d", std::max(0, std::min(10, d.energy / 100000))); }
        if ((w.name == "SpGauge" || w.name == "SpDamageGauge" || w.name == "SpMinusGauge") && !inst.compare(0, 8, "SpGauge_")) {
            int i = atoi(inst.c_str() + 8) - 1;
            if (w.name == "SpMinusGauge") return;
            float v = ((w.name == "SpGauge" ? d.ki : d.kitrail) - 20000.0f * i) / 20000.0f;
            if (v <= 0) return;
            auto m = mat.find(w.img);
            if (m != mat.end()) {
                auto g = [&](const char* k) { auto t = m->second.tex.find(k); return t == m->second.tex.end() ? std::string() : t->second; };
                it.mask = g("MaskTexture"); it.filter = g("FilterH"); it.tex = g("BaseTexture");
            }
            it.value = std::min(1.0f, v); it.key = sz_key3(SzS(cn), SzS(inst), w.name);
        }
        if (w.name == "IMG_CurrentStock") {
            int j = atoi(inst.c_str() + inst.size() - 2);
            if (j < 1 || j > c.bar) return;
            it.tex = sz_numbered("T_UI_HpStock_%02d", std::min(j, 8));
        }
        if (F.style && d.senzu < 0) return;
        if (c.gval > 0 && !c.gkey.empty()) {
            if (!c.gbase.empty()) it.tex = c.gbase;
            it.mask = c.gmask; it.filter = c.gfilter; it.value = c.gval; it.key = sz_key2(c.gkey, w.name); it.vert = c.gvert;
        }
        if (it.tex.empty()) return;
        float blink = -1, sal = 1;
        if (mc.has) {
            it.add = mc.add;
            if (mc.noalpha) { sal = w.bal >= 0 ? w.bal : mc.alpha; it.add = true; }   // additive, intensity = Alpha
            it.addq = mc.plain;
            float tx = mc.tx, ty = mc.ty, pn = mc.pn, sp = std::max(0.1f, mc.speed);
            int f = std::max(0, (int)w.ptime);
            bool ymaj = mc.ymaj;
            if (it.tex == "T_UI_ChargeOverEF_Set") pn = 11;   // rows counted on the sheet
            if (pn > 0 && ymaj) { tx = 1; ty = 1 / pn; f = (int)(d.now * sp * pn); pn = 0; }
            else if (pn <= 0 && mc.autof && w.sz[0] > 0 && w.sz[1] > 0 && W > 0 && H > 0) {   // frames from the sheet and widget shapes
                float q = (w.sz[1] / w.sz[0]) / (H / W);
                pn = ymaj ? lroundf(q) : lroundf(1 / q);
                if (pn > 1) { if (ymaj) { tx = 1; ty = 1 / pn; } else { tx = 1 / pn; ty = 1; } f = (int)(d.now * sp * pn); pn = 0; }
            }
            if (pn > 0) { tx = 1 / pn; if (ty <= 0) ty = 1; f = (int)(d.now * sp * pn); }   // auto flipbook: PatternNum frames, Speed loops per second
            if (tx > 0 && ty > 0) {   // flipbook sheet
                int cols = std::max(1, (int)lroundf(1 / tx)), rows = std::max(1, (int)lroundf(1 / ty)), n = cols * rows;
                f %= n;
                int cx = ymaj ? f / rows : f % cols, cy = ymaj ? f % rows : f / cols;
                it.u0 = cx * tx; it.u1 = it.u0 + tx; it.v0 = cy * ty; it.v1 = it.v0 + ty;
            }
            if (radial) it.add = w.img.find("_Add") != std::string::npos;
            if (mc.brink) blink = mc.alpha_v * std::max(0.0f, sinf(3.14159265f * d.now));   // lit 1 s, dark 1 s (measured on SZ footage)
        }
        if (it.tex == "T_UI_Combo_Particle") { it.tex = "@cpart"; it.add = true; it.addq = false; }   // channel-packed sparkles (R, G) under a gradient (B): made by sz_hud.inc
        float px[4] = {0, W, W, 0}, py[4] = {0, 0, H, H};
        for (int k = 0; k < 4; k++) { it.x[k] = X.a * px[k] + X.b * py[k] + X.e; it.y[k] = X.c * px[k] + X.d * py[k] + X.f; }
        it.r = w.col[0] * w.tint[0] * c.col[0]; it.g = w.col[1] * w.tint[1] * c.col[1]; it.b = w.col[2] * w.tint[2] * c.col[2]; it.a = o * w.col[3] * w.tint[3] * c.col[3];
        if (blink >= 0) { it.r *= blink; it.g *= blink; it.b *= blink; }
        if (radial) { it.r = 1; it.g = it.add ? 0.2f : 0.8f; it.b = it.add ? 0.6f : 1; it.a = it.add ? o * o : o * 0.9f; }   // colours measured on SZ footage (the shader is not exported)
        it.a *= sal;
        if (it.a <= 0.003f) return;
        if ((w.mg[0] > 0 || w.mg[1] > 0 || w.mg[2] > 0 || w.mg[3] > 0) && w.sz[0] > 0 && w.sz[1] > 0) {   // Box brush: margins drawn at image size, centre stretched
            float ml = w.mg[0] * w.sz[0], mr = w.mg[2] * w.sz[0], mt = w.mg[1] * w.sz[1], mb = w.mg[3] * w.sz[1];
            if (ml + mr > W) { float k = W / (ml + mr); ml *= k; mr *= k; }
            if (mt + mb > H) { float k = H / (mt + mb); mt *= k; mb *= k; }
            float xs[4] = {0, ml, W - mr, W}, ys[4] = {0, mt, H - mb, H}, du = it.u1 - it.u0, dv = it.v1 - it.v0;
            float us[4] = {it.u0, it.u0 + w.mg[0] * du, it.u1 - w.mg[2] * du, it.u1}, vs[4] = {it.v0, it.v0 + w.mg[1] * dv, it.v1 - w.mg[3] * dv, it.v1};
            for (int r = 0; r < 3; r++)
                for (int q = 0; q < 3; q++) {
                    if (xs[q + 1] - xs[q] <= 0.01f || ys[r + 1] - ys[r] <= 0.01f) continue;
                    SzItem s9 = it;
                    float qx[4] = {xs[q], xs[q + 1], xs[q + 1], xs[q]}, qy[4] = {ys[r], ys[r], ys[r + 1], ys[r + 1]};
                    for (int k = 0; k < 4; k++) { s9.x[k] = X.a * qx[k] + X.b * qy[k] + X.e; s9.y[k] = X.c * qx[k] + X.d * qy[k] + X.f; }
                    s9.u0 = us[q]; s9.u1 = us[q + 1]; s9.v0 = vs[r]; s9.v1 = vs[r + 1];
                    c.out->push_back(s9);
                }
            return;
        }
        c.out->push_back(it);
        if (w.addr > 0.003f && it.tex == "@face") { it.tex = "@facew"; it.add = true; it.a *= std::min(1.0f, w.addr); c.out->push_back(it); }   // AddRate: white flash
    }

    // the state animation of one widget instance, chosen from the TTT data
    // per class facts used by every draw: its widgets, its animations and which state rule (pick) it follows
    struct CInfo { mutable std::deque<std::pair<const SzAnim*, std::vector<int>>> km; std::string name; const std::vector<SzW>* L = nullptr; const std::map<std::string, SzAnim, std::less<>>* am = nullptr; int pk = 0; const SzAnim* rep = nullptr; };
    mutable std::unordered_map<const std::string*, CInfo*> ci_p;
    mutable std::unordered_map<std::string, CInfo> ci_s;
    const CInfo& cinfo(const std::string& cn) const {
        auto f = ci_p.find(&cn);
        if (f != ci_p.end() && f->second->name == cn) return *f->second;
        auto g = ci_s.find(cn);
        if (g == ci_s.end()) {
            CInfo& ci = ci_s[cn];
            ci.name = cn;
            auto it = cls.find(cn); if (it != cls.end()) ci.L = &it->second;
            auto a = anim.find(cn); if (a != anim.end()) ci.am = &a->second;
            auto has = [&](const char* k) { return cn.find(k) != std::string::npos; };
            int k = 0;
            if (!cn.compare(0, 17, "WBP_Rep_MainGauge")) k = 1;
            else if (has("OBJ_Combo_P") || has("OBJ_Damage_P")) k = has("Combo") ? 2 : 3;
            else if (cn == "WBP_OBJ_DamageNum") k = 4;
            else if (has("MainGauge")) k = 5;
            else if (has("SpecialGaugeSet")) k = 6;
            else if (has("KingGauge")) k = 7;
            else if (has("SpGaugeParts")) k = 8;
            else if (cn == "WBP_Rep_TimeCount") k = 9;
            else if (cn == "WBP_OBJ_Timer") k = 10;
            else if (cn == "WBP_OBJ_TimerNum") k = 11;
            else if (has("BuGauge_P")) k = 12;
            else if (has("HpStock") && !has("Set")) k = 13;
            else if (cn == "WBP_GRP_BuffIcon") k = 14;
            else if (has("BuGauge_Ef")) k = 15;
            else if (has("ChargeOverEf")) k = 16;
            else if (has("ThunderEF")) k = 17;
            else if (has("ActiveOverEF")) k = 18;
            else if (has("HpOverEf")) k = 19;
            else if (has("BuOverEf")) k = 20;
            ci.pk = k;
            auto r = anim.find(cn.find("_P2") != std::string::npos ? "WBP_Rep_MainGauge_P2" : "WBP_Rep_MainGauge_P1");
            if (r != anim.end()) { auto q = r->second.find("ANI_FirstIn"); if (q != r->second.end()) ci.rep = &q->second; }
            g = ci_s.find(cn);
        }
        ci_p[&cn] = &g->second;
        return g->second;
    }
    void pick(const std::string& cn, const std::string& inst, Ctx& c) const { pick(cinfo(cn), inst, c); }
    void pick(const CInfo& ci, const std::string& inst, Ctx& c) const {
        c.an = nullptr;
        if (!ci.am) return;
        const auto& am = *ci.am;
        const std::string& cn = ci.name;
        const SzData& d = *c.d;
        auto A = [&](const char* n) -> const SzAnim* { auto b = am.find(n); return b == am.end() ? nullptr : &b->second; };
        auto in = [&](const char* n, float t) { const SzAnim* x = A(n); return x && t >= 0 && t < x->len; };
        auto set = [&](const char* n, float t, bool loop) {
            const SzAnim* x = A(n);
            if (!x) return;
            c.an = x;
            c.at = loop && x->len > 0 ? fmodf(std::max(0.0f, t), x->len) : std::min(std::max(0.0f, t), x->len);
        };
        auto repin = [&](float t) { return ci.rep && t >= 0 && t < ci.rep->len ? ci.rep : nullptr; };
        const int k = ci.pk;
        (void)cn;
        if (k == 1) set("ANI_FirstIn", d.t_show, false);
        else if (k == 2 || k == 3) {
            bool cb = k == 2;
            float hd = cb ? d.t_chide : d.t_dhide;
            if (hd >= 0) set("ANI_Hide", hd, false); else set("ANI_Show", cb ? d.t_cshow : d.t_dshow, false);
        } else if (k == 4) set("ANI_Change", 1e3f, false);   // SZ swaps the digits in battle: the rest pose of the change (Num_A shown)
        else if (k == 5) {
            const char* sd = A("ANI_Sparking_Damage") ? "ANI_Sparking_Damage" : "ANI_Sprking_Damage";
            if (d.t_hide >= 0) set("AN_Hide", d.t_hide, false);
            else if (repin(d.t_show)) { c.an = repin(d.t_show); c.at = d.t_show; }   // SZ's in-battle in animation (Rep class), when the kit has it
            else if (!repin(0) && in("ANI_Show", d.t_show)) set("ANI_Show", d.t_show, false);
            else if (d.spark) { if (in(sd, d.t_hit)) set(sd, d.t_hit, false); else set("ANI_Sparking", d.now, true); }
            else if (in("ANI_Damage", d.t_hit)) set("ANI_Damage", d.t_hit, false);
            else set("ANI_Wait", d.now, true);
        } else if (k == 6) {
            if (d.spark || d.orange > 0) set("ANI_Wait_King", d.now, true);   // ki cells hidden under the charge and sparking gauges (as in SZ)
            else if (in("ANI_Change_Sp", d.t_unspark)) set("ANI_Change_Sp", d.t_unspark, false);
            else if (in("ANI_Change_Sp", d.t_uncharge)) set("ANI_Change_Sp", d.t_uncharge, false);
            else set("ANI_Wait_Sp", d.now, true);
        } else if (k == 7) {
            if (d.spark) { if (in("ANI_King_Start", d.t_spark)) set("ANI_King_Start", d.t_spark, false); else set("ANI_King_Active", d.now, true); }
            else if (in("ANI_King_Finish", d.t_unspark)) set("ANI_King_Finish", d.t_unspark, false);
            else if (d.orange > 0) {
                if (!d.charging) set("ANI_Charge_Pause", d.now, true);
                else if (in("ANI_Charge_Start", d.t_charge)) set("ANI_Charge_Start", d.t_charge, false);
                else set("ANI_Charge", d.now, true);
            } else if (in("ANI_Charge_End", d.t_uncharge)) set("ANI_Charge_End", d.t_uncharge, false);
            else set("ANI_Blank", 0, false);
        } else if (k == 8) {
            int i = !inst.compare(0, 8, "SpGauge_") ? atoi(inst.c_str() + 8) - 1 : -1;
            float ct = i >= 0 && i < 5 ? (float)d.cell[i] : -1.0f;
            if (d.ki >= 100000) set("ANI_MaxWait", d.now, true);
            else if (ct >= 0) { if (in("ANI_SingleOn", ct)) set("ANI_SingleOn", ct, false); else set("ANI_SingleMax", 0, false); }
            else set("ANI_Normal", 0, false);
        } else if (k == 9) {
            const SzAnim* ae = A("ANI_Alart_Effect");
            if (in("ANI_FitstIn", d.t_show)) set("ANI_FitstIn", d.t_show, false);
            else if (ae && d.tm >= 0 && d.tm <= 10 && d.tchg[0] >= 0 && d.tchg[0] < ae->len) set("ANI_Alart_Effect", d.tchg[0], false);
        } else if (k == 10) {
            float al = A("ANI_Alart_Effect") ? A("ANI_Alart_Effect")->len : 0;
            if (d.t_hide >= 0) set("ANI_Hide", d.t_hide, false);
            else if (in("ANI_Show", d.t_show)) set("ANI_Show", d.t_show, false);
            else if (d.tm >= 0 && d.tm <= 10) { if (d.tchg[0] >= 0 && d.tchg[0] < al) set("ANI_Alart_Effect", d.tchg[0], false); else set("ANI_Alart", d.now, true); }
            else set("ANI_Wait", d.now, true);
        } else if (k == 11) {
            set("ANI_Normal", d.now, true);   // SZ swaps the digit (ANI_Change is not played in battle)
        } else if (k == 12) {
            if (d.energy >= 500000) { if (in("ANI_Max", d.t_max)) set("ANI_Max", d.t_max, false); else set("ANI_MaxWait", d.now, true); }
            else if (in("ANI_ChargeOn", d.t_unit)) set("ANI_ChargeOn", d.t_unit, false);
            else set("ANI_Normal", 0, false);
        } else if (k == 13) {
            int j = inst.size() >= 2 ? atoi(inst.c_str() + inst.size() - 2) : 0;
            if (j >= 1 && j <= c.bar) set("ANI_Normal", 0, false);
            else if (j == c.bar + 1 && in("ANI_Break", d.t_break)) set("ANI_Break", d.t_break, false);
            else set("ANI_Blank", 0, false);
        } else if (k == 14) {
            int k = std::min(5, atoi(inst.c_str() + inst.size() - 1));
            if (d.bout[k] >= 0) set("ANM_Out", d.bout[k], false);
            else if (in("ANM_ShowIn", d.bshow[k])) set("ANM_ShowIn", d.bshow[k], false);
            else set("ANM_Normal_Loop", 0, false);
        } else if (k == 15) set("ANI_Loop2", d.now, true);
        else if (k == 16) set(d.charging ? "ANI_Charge" : "ANI_Pause", d.now, true);
        else if (k == 17) set("ANI_Thunder", d.now, true);
        else if (k == 18) set("ANI_Active", d.now, true);
        else if (k == 19) { if (in("ANI_Show", d.t_hit)) set("ANI_Show", d.t_hit, false); else set("ANI_Wait", d.now, true); }
        else if (k == 20) {
            if (d.t_egain > 0.2f) set("ANI_Hide", d.t_egain - 0.2f, false);
            else if (in("ANI_Show", d.t_estart)) set("ANI_Show", d.t_estart, false);
            else set("ANI_Wait", d.now, true);
        }
    }

    void draw_eval(const std::vector<SzW>& L, const std::string& cn, const SzX& xf, float pw, float ph, float op, const std::string& inst, Ctx& c2, int& at) const {
        for (int i = 0; i < (int)L.size(); i++) if (L[i].par < 0) node(cn, L, i, xf, pw, ph, op, inst, c2);
        if (at >= 0) {
            SzW t = L[at]; t.type = "Image"; t.name = "Senzu"; t.img = sz_numbered("T_UI_TimeNum_%02d", std::min(9, (int)c2.d->senzu));
            int i = at, p = t.par;
            t.par = -1;
            c2.senzu_at = nullptr; c2.ov = &t; c2.ovi = i;
            if (p >= 0 && L[p].par < 0 && L[p].slot == 0) node(cn, L, i, xf, pw, ph, op, inst, c2);
        }
    }
    // subtree cache: a draw replays its last output when its arguments, the pose of its animated containers and every SzData value
    // it read are the same. Inside it, user widgets are calls replayed through draw (their own cache), and childless widgets that
    // an animation moves or that read the clock are node calls: evaluated again alone, each with a few remembered poses
    struct CKey {
        const void *gm, *gf, *gb, *gk; float gval, col[4]; int bar, rem, tbar, trem, panel, dig; float dsc; bool gvert;
        float xf[6], pw, ph, op;
        bool operator==(const CKey& o) const { return !memcmp(this, &o, sizeof *this); }
    };
    static CKey ckey(const Ctx& c, const SzX& xf, float pw, float ph, float op) {
        CKey k; memset(&k, 0, sizeof k);
        k.gm = c.gmask.p; k.gf = c.gfilter.p; k.gb = c.gbase.p; k.gk = c.gkey.p; k.gval = c.gval; memcpy(k.col, c.col, sizeof k.col);
        k.bar = c.bar; k.rem = c.rem; k.tbar = c.tbar; k.trem = c.trem; k.panel = c.panel; k.dig = c.dig; k.dsc = c.dsc; k.gvert = c.gvert;
        k.xf[0] = xf.a; k.xf[1] = xf.b; k.xf[2] = xf.c; k.xf[3] = xf.d; k.xf[4] = xf.e; k.xf[5] = xf.f; k.pw = pw; k.ph = ph; k.op = op;
        return k;
    }
    struct CSlot;
    struct CCall { const CInfo* ci; std::string inst; SzX xf; float pw, ph, op; Ctx c; CSlot* slot; };
    struct NVar { std::vector<float> pose; std::vector<SzDep> deps; std::vector<SzItem> items; };
    struct NCall { int i; SzX xf; float pw, ph, op; Ctx c; bool vol; std::vector<NVar> var; int rr, streak; };
    struct CEnt { const CInfo* ci = nullptr; std::string inst; CKey k; const SzAnim* an = nullptr; std::vector<float> pose; std::vector<SzDep> deps; std::vector<SzItem> items; std::vector<CCall> calls; std::vector<NCall> ncalls; std::vector<std::pair<int, int>> seq; uint32_t use = 0; bool busy = false; };
    struct CRec { SzRec r; CEnt* e; std::vector<SzItem>* out; size_t mark; };
    struct CSlot { std::deque<CEnt> v; int streak = 0; };
    mutable std::unordered_map<size_t, CSlot> cache;
    mutable CRec* crec = nullptr;
    mutable uint32_t cuse = 0;
    mutable int scope = 0;   // who is being drawn (bar 0..3, combo, timer): each keeps its own entries of the shared classes
    mutable long c_hit = 0, c_miss = 0;
    static int memo_on() { static int on = -1; if (on < 0) { const char* e = getenv("TTT_SZ_MEMO"); on = e && *e ? atoi(e) : 1; } return on; }
    // tracks of an animation that belong to the key of a class: the ones that move a widget with children
    const std::vector<int>& keytr(const CInfo& ci, const SzAnim* an) const {
        for (auto& p : ci.km) if (p.first == an) return p.second;
        std::vector<int> v;
        if (an) for (int i = 0; i < (int)an->tr.size(); i++) {
            const SzTrack& t = an->tr[i];
            if (t.k.empty()) continue;
            bool any = false, leaf = true;
            for (auto& w : *ci.L) if (w.name == t.who) { any = true; if (!w.kids.empty() || w.name == "CharaNum") leaf = false; }
            if (any && !leaf) v.push_back(i);
        }
        ci.km.emplace_back(an, std::move(v));
        return ci.km.back().second;
    }
    static void posev(const SzAnim* an, float at, const std::vector<int>& kt, std::vector<float>& o) { o.clear(); for (int i : kt) o.push_back(ev(an->tr[i].k, at)); }
    static bool pose_ok(const SzAnim* an, float at, const std::vector<int>& kt, const std::vector<float>& o) {
        if (o.size() != kt.size()) return false;
        for (size_t i = 0; i < o.size(); i++) { float v = ev(an->tr[kt[i]].k, at); if (memcmp(&v, &o[i], 4)) return false; }
        return true;
    }
    static bool has_now(const std::vector<SzDep>& v) { for (auto& d : v) if (!d.band && d.off == offsetof(SzData, now)) return true; return false; }
    mutable std::map<std::string, int> why;
    void miss_why(const CSlot& S, const CInfo& ci, const std::string& inst, const CKey& k, const Ctx& c2, const char* base) const {
        std::string r = "sin entrada";
        const std::vector<int>& kt = keytr(ci, c2.an);
        for (auto& e : S.v) {
            if (e.ci != &ci || e.inst != inst) continue;
            if (!(e.k == k)) {
                r = "argumentos";
                if (memcmp(e.k.xf, k.xf, sizeof k.xf)) r = "argumentos: transformacion";
                else if (e.k.op != k.op) r = "argumentos: opacidad";
                else if (memcmp(e.k.col, k.col, sizeof k.col)) r = "argumentos: color";
                else if (e.k.bar != k.bar || e.k.rem != k.rem || e.k.tbar != k.tbar || e.k.trem != k.trem) r = "argumentos: vida";
                else if (memcmp(&e.k.gval, &k.gval, 4)) r = "argumentos: valor de medidor";
                continue;
            }
            if (e.an != c2.an) { r = "animacion elegida"; continue; }
            if (!pose_ok(c2.an, c2.at, kt, e.pose)) {
                r = "pose de animacion";
                for (size_t i = 0; i < e.pose.size() && i < kt.size(); i++) { float v = ev(c2.an->tr[kt[i]].k, c2.at); if (memcmp(&v, &e.pose[i], 4)) { r += " " + c2.an->tr[kt[i]].who.str(); break; } }
                continue;
            }
            r = "dato";
            for (auto& d : e.deps) { std::vector<SzDep> one(1, d); if (!sz_dep_ok(one, base)) { r += " " + sz_field(d.off); break; } }
            break;
        }
        why[ci.name + ": " + r]++;
    }
    void why_print() const {
        std::vector<std::pair<int, std::string>> v;
        for (auto& w : why) v.push_back({w.second, w.first});
        std::sort(v.rbegin(), v.rend());
        for (size_t i = 0; i < v.size() && i < 12; i++) printf("hud sz: fallo de cache %d veces, %s\n", v[i].first, v[i].second.c_str());
        why.clear();
    }
    void flush(CRec* p, size_t n) const {
        if (n > p->mark) { p->e->items.insert(p->e->items.end(), p->out->begin() + p->mark, p->out->begin() + n); p->e->seq.push_back({0, (int)(n - p->mark)}); }
        p->mark = n;
    }
    static int npose(const SzAnim* an, const SzS& name, float at, float* pv) {   // values of the tracks of one widget; -1 = too many
        const std::vector<int>* ix = an ? an->by(name) : nullptr;
        int n = 0;
        if (ix) for (int ti : *ix) { if (n >= 16) return -1; pv[n++] = ev(an->tr[ti].k, at); }
        return n;
    }
    // while a draw is recorded: a childless widget is evaluated alone; kept as a node call when an animation has tracks for it or
    // it read the clock, else its quads stay in the entry with what it read
    bool node_rec(const std::string& cn, const std::vector<SzW>& L, int i, const SzX& xf, float pw, float ph, float op, const std::string& inst, Ctx& c) const {
        const SzW& q = L[i];
        if (q.name == "CharaNum") return false;
        bool tracks = c.an && c.an->by(q.name), user = !q.type.compare(0, 2, "U:");
        if (user && !tracks) return false;
        CRec* r = crec; SzRec* s = sz_rec;
        size_t n0 = c.out->size();
        Ctx sc = c; sc.senzu_at = nullptr; sc.ov = nullptr; sc.ovi = -1;
        if (user) {
            flush(r, n0);
            r->e->ncalls.push_back(NCall{i, xf, pw, ph, op, sc, true, {}, 0, 0}); r->e->seq.push_back({2, (int)r->e->ncalls.size() - 1});
            crec = nullptr; sz_rec = nullptr;
            node_body(cn, L, i, xf, pw, ph, op, inst, c);
            crec = r; sz_rec = s; r->mark = c.out->size();
            return true;
        }
        SzRec tmp; NVar nv; tmp.base = (const char*)c.d; tmp.deps = &nv.deps;
        crec = nullptr; sz_rec = &tmp;
        node_body(cn, L, i, xf, pw, ph, op, inst, c);
        crec = r; sz_rec = s;
        bool vol = tmp.bad || has_now(nv.deps);
        if (!tracks && !vol) { if (s) s->deps->insert(s->deps->end(), nv.deps.begin(), nv.deps.end()); return true; }
        float pv[16];
        int np = npose(c.an, q.name, c.at, pv);
        if (np < 0) vol = true;
        flush(r, n0);
        NCall nc{i, xf, pw, ph, op, sc, vol, {}, 0, 0};
        if (!vol) { nv.pose.assign(pv, pv + np); nv.items.assign(c.out->begin() + n0, c.out->end()); nc.var.push_back(std::move(nv)); }
        r->e->ncalls.push_back(std::move(nc)); r->e->seq.push_back({2, (int)r->e->ncalls.size() - 1});
        r->mark = c.out->size();
        return true;
    }
    void ncall_run(const CInfo& ci, NCall& nc, const std::string& inst, const SzAnim* an, float at, Ctx& c) const {
        const std::vector<SzW>& L = *ci.L;
        Ctx& x = nc.c; x.d = c.d; x.out = c.out; x.an = an; x.at = at;
        float pv[16];
        int np = nc.vol ? -1 : npose(an, L[nc.i].name, at, pv);
        if (np < 0) { node_body(ci.name, L, nc.i, nc.xf, nc.pw, nc.ph, nc.op, inst, x); return; }
        for (auto& v : nc.var)
            if ((int)v.pose.size() == np && (!np || !memcmp(v.pose.data(), pv, 4 * (size_t)np)) && sz_dep_ok(v.deps, (const char*)c.d)) { nc.streak = 0; c.out->insert(c.out->end(), v.items.begin(), v.items.end()); return; }
        if (++nc.streak >= 6 && nc.streak % 64) { node_body(ci.name, L, nc.i, nc.xf, nc.pw, nc.ph, nc.op, inst, x); return; }   // moves every frame: no record
        size_t n0 = c.out->size();
        bool full = nc.var.size() >= 4;
        if (!full) nc.var.emplace_back();
        NVar& nv = full ? nc.var[nc.rr++ & 3] : nc.var.back();   // the vectors of the replaced pose are reused
        nv.deps.clear(); nv.items.clear(); nv.pose.clear();
        SzRec tmp; tmp.base = (const char*)c.d; tmp.deps = &nv.deps;
        SzRec* s = sz_rec; sz_rec = &tmp;
        node_body(ci.name, L, nc.i, nc.xf, nc.pw, nc.ph, nc.op, inst, x);
        sz_rec = s;
        if (tmp.bad || has_now(nv.deps)) { nv.pose.assign(1, 0.0f); nv.pose.resize(17); return; }   // 17 values: never matches
        nv.pose.assign(pv, pv + np); nv.items.assign(c.out->begin() + n0, c.out->end());
    }

    void draw(const std::string& cn, const SzX& xf, float pw, float ph, float op, const std::string& inst, Ctx& c, CSlot** hint = nullptr) const {
        const CInfo& ci = cinfo(cn);
        if (!ci.L) return;
        const auto& L = *ci.L;
        int at = -1;
        Ctx c2 = c; c2.senzu_at = &at; c2.ov = nullptr;
        CRec* par = crec;
        if (!memo_on()) { crec = nullptr; pick(ci, inst, c2); crec = par; draw_eval(L, cn, xf, pw, ph, op, inst, c2, at); return; }
        if (par) { flush(par, c.out->size()); par->e->calls.push_back(CCall{&ci, inst, xf, pw, ph, op, c, nullptr}); par->e->seq.push_back({1, (int)par->e->calls.size() - 1}); }
        SzRec* rr = sz_rec;
        crec = nullptr; sz_rec = nullptr;
        pick(ci, inst, c2);
        CKey k = ckey(c, xf, pw, ph, op);
        CSlot* sp = hint ? *hint : nullptr;
        if (!sp) { sp = &cache[(size_t)&ci * 31 + std::hash<std::string>()(inst) + (size_t)(scope * 2 + c.panel + 1) * (size_t)0x9E3779B97F4A7C15ull]; if (hint) *hint = sp; }
        CSlot& S = *sp;
        const std::vector<int>& kt = keytr(ci, c2.an);
        cuse++;
        for (auto& e : S.v) {
            if (!(e.k == k) || e.an != c2.an || e.ci != &ci || e.inst != inst || !pose_ok(c2.an, c2.at, kt, e.pose) || !sz_dep_ok(e.deps, (const char*)c.d)) continue;
            e.use = cuse; c_hit++; S.streak = 0;
            bool was = e.busy; e.busy = true;
            size_t ii = 0;
            for (auto& q : e.seq) {
                if (q.first == 0) { c.out->insert(c.out->end(), e.items.begin() + ii, e.items.begin() + ii + q.second); ii += q.second; }
                else if (q.first == 1) {
                    CCall& cc = e.calls[q.second];
                    Ctx x = cc.c; x.d = c.d; x.out = c.out; x.senzu_at = c.senzu_at;
                    draw(cc.ci->name, cc.xf, cc.pw, cc.ph, cc.op, cc.inst, x, &cc.slot);
                } else ncall_run(ci, e.ncalls[q.second], inst, c2.an, c2.at, c);
            }
            e.busy = was;
            crec = par; sz_rec = rr;
            if (par) par->mark = c.out->size();
            return;
        }
        c_miss++;
        if (memo_on() == 2) miss_why(S, ci, inst, k, c2, (const char*)c.d);
        S.streak++;
        if (S.streak >= 8 && S.streak % 32) {   // changes every frame: plain evaluation, no record
            draw_eval(L, cn, xf, pw, ph, op, inst, c2, at);
            crec = par; sz_rec = rr;
            if (par) par->mark = c.out->size();
            return;
        }
        CEnt ne; ne.ci = &ci; ne.inst = inst; ne.k = k; ne.an = c2.an; posev(c2.an, c2.at, kt, ne.pose);
        CRec R; R.e = &ne; R.out = c.out; R.mark = c.out->size(); R.r.base = (const char*)c.d; R.r.deps = &ne.deps;
        crec = &R; sz_rec = &R.r;
        draw_eval(L, cn, xf, pw, ph, op, inst, c2, at);
        flush(&R, c.out->size());
        crec = par; sz_rec = rr;
        if (par) par->mark = c.out->size();
        if (R.r.bad) return;
        ne.use = cuse;
        if (S.v.size() < 4) { S.v.push_back(std::move(ne)); return; }
        CEnt* o = nullptr;
        for (auto& e : S.v) if (!e.busy && (!o || e.use < o->use)) o = &e;
        if (o) *o = std::move(ne);
    }

    // start lightning of a panel (WBP_Rep_MainGauge_P1/P2 ShowEffectBox, ANI_FirstIn): its GRP_All placed on the panel's.
    static constexpr float FX_LAG = 0;
    const SzAnim* repin(const std::string& cn, float t) const {   // ANI_FirstIn of the Rep twin of a panel class while it runs
        auto a = anim.find(cn.find("_P2") != std::string::npos ? "WBP_Rep_MainGauge_P2" : "WBP_Rep_MainGauge_P1");
        if (a == anim.end()) return nullptr;
        auto f = a->second.find("ANI_FirstIn");
        return f != a->second.end() && t >= 0 && t < f->second.len ? &f->second : nullptr;
    }
    void fx(int side, const SzData& d, std::vector<SzItem>& out) const {
        const char* cn = side ? "WBP_Rep_MainGauge_P2" : "WBP_Rep_MainGauge_P1";
        auto a = anim.find(cn);
        if (a == anim.end()) return;
        auto f = a->second.find("ANI_FirstIn");
        SzData d2 = d;
        d2.t_show = d.t_show - FX_LAG;
        if (f == a->second.end() || d2.t_show < 0 || d2.t_show >= f->second.len) return;
        SzX s; s.a = 480.0f / 3840; s.d = 272.0f / 2160;
        SzX o; o.e = side ? -24 : 24; o.f = 46;
        Ctx c; c.d = &d2; c.out = &out; c.panel = side;
        draw(cn, sz_mul(s, o), 3840, 2160, 1, "", c);
    }
    // a whole widget class on the 3840x2160 canvas (widgets outside the HUD root, e.g. WBP_Rep_TimeCount)
    void whole(const char* cn, const SzData& d, std::vector<SzItem>& out, float op = 1) const {
        if (!cls.count(cn)) return;
        SzX s; s.a = 480.0f / 3840; s.d = 272.0f / 2160;
        Ctx c; c.d = &d; c.out = &out;
        draw(cn, s, 3840, 2160, op, "", c);
    }
    // a child of the HUD root (WBP_MainGame_HUD) by name, on the 480x272 screen
    void root(const char* name, const SzData& d, std::vector<SzItem>& out) const {
        auto it = cls.find("WBP_MainGame_HUD");
        if (it == cls.end()) return;
        SzX s; s.a = 480.0f / 3840; s.d = 272.0f / 2160;
        Ctx c; c.d = &d; c.out = &out;
        for (int i = 0; i < (int)it->second.size(); i++) if (it->second[i].name == name) { node("WBP_MainGame_HUD", it->second, i, s, 3840, 2160, 1, "", c); return; }
    }

    // combo hits and damage of a side (SZ's in-battle WBP_Rep_ComboSet_P1/P2 when the kit has it, else the HUD's own set)
    // measured on SZ footage (set at runtime, not in the export): the set at 0.93 and placed; damage digits 0-2 at 0.775, the
    // higher ones at 0.935, pitch 57 / 75 canvas units, bottoms aligned
    static constexpr float SZ_CK = 0.93f, SZ_CX = -3.3f, SZ_CY = 12.45f, SZ_DX = 13.9f, SZ_DY = -26.1f, SZ_DKS = 0.775f, SZ_DKB = 0.935f, SZ_DPS = 57, SZ_DPB = 75;
    void combo(int side, const SzData& d, std::vector<SzItem>& out) const {
        const char* rep = side ? "WBP_Rep_ComboSet_P2" : "WBP_Rep_ComboSet_P1";
        if (!cls.count(rep)) { root(side ? "WBP_GRP_CommboSet_P2" : "WBP_GRP_CommboSet_P1", d, out); return; }
        SzX s; s.a = 480.0f / 3840; s.d = 272.0f / 2160;
        SzX m; m.a = m.d = SZ_CK; m.e = side ? 480 * (1 - SZ_CK) - SZ_CX : SZ_CX; m.f = SZ_CY;
        Ctx c; c.d = &d; c.out = &out; c.panel = side;
        draw(rep, sz_mul(m, s), 3840, 2160, 1, "", c);
    }

    // power-up notice (WBP_Rep_BBoost_1P): only the background disc and the text, text centre at (cx, cy) on the 480x272 screen;
    // ANM_ShowIn, ANM_Wait while shown, ANM_Out from tout (its 1.05 text scale follows ANM_Normal_ON, not played here: kept at 1)
    bool boost(float cx, float cy, float t, float tout, std::vector<SzItem>& out) const {
        auto c = cls.find("WBP_Rep_BBoost_1P");
        if (c == cls.end()) return false;
        const SzW *pn = nullptr, *bk = nullptr, *tx = nullptr;
        for (auto& w : c->second) {
            if (w.name == "CanvasPanel_0") pn = &w;
            else if (w.name == "Image_Black") bk = &w;
            else if (w.name == "Image_BoostText") tx = &w;
        }
        if (!pn || !bk || !tx) return false;
        SzW P = *pn, B = *bk, T = *tx;
        const SzAnim *in = nullptr, *wt = nullptr, *ot = nullptr;
        auto am = anim.find(c->first);
        if (am != anim.end()) {
            auto f = [&](const char* n) { auto i = am->second.find(n); return i == am->second.end() ? (const SzAnim*)nullptr : &i->second; };
            in = f("ANM_ShowIn"); wt = f("ANM_Wait"); ot = f("ANM_Out");
        }
        const SzAnim* a = tout >= 0 ? ot : in && t < in->len ? in : wt;
        float at = tout >= 0 ? tout : a == in ? t : 0;
        if (a) for (SzW* w : {&P, &B, &T}) apply(*a, at, *w);
        if (tout >= 0) T.sc[0] = T.sc[1] = 1;
        auto ctr = [](const SzW& w, float* o) { o[0] = w.l + w.w * (0.5f - w.al[0]) + w.tr[0]; o[1] = w.t + w.h * (0.5f - w.al[1]) + w.tr[1]; };
        float t0[2] = {tx->l + tx->w * (0.5f - tx->al[0]), tx->t + tx->h * (0.5f - tx->al[1])}, kx = P.sc[0] * 480.0f / 3840, ky = P.sc[1] * 272.0f / 2160;
        auto quad = [&](const SzW& w, const std::string& tex, float al, bool mid) {
            if (tex.empty() || al <= 0.002f) return;
            float m[2]; ctr(w, m);
            if (mid) { m[0] = t0[0]; m[1] = t0[1]; }   // disc centred on the text (his choice)
            float x = cx + (m[0] - t0[0]) * kx, y = cy + (m[1] - t0[1]) * ky, hw = w.w * w.sc[0] * kx * 0.5f, hh = w.h * w.sc[1] * ky * 0.5f;
            SzItem it;
            it.tex = tex;
            it.x[0] = it.x[3] = x - hw; it.x[1] = it.x[2] = x + hw; it.y[0] = it.y[1] = y - hh; it.y[2] = it.y[3] = y + hh;
            it.r = w.col[0]; it.g = w.col[1]; it.b = w.col[2]; it.a = al;
            out.push_back(it);
        };
        if (!B.hid) quad(B, brush(B.img), B.col[3] * B.op * P.op, true);
        if (!T.hid) quad(T, brush(T.img), T.col[3] * T.op * P.op, false);
        return true;
    }

    // panel 0 = P1 (top left), 1 = P2 (top right); screen 480x272 from the 3840x2160 canvas
    void panel(int side, const SzData& d, std::vector<SzItem>& out) const {
        const char* cn = side ? "WBP_GRP_MainGauge_P2" : "WBP_GRP_MainGauge_P1";
        auto it = cls.find(cn);
        if (it == cls.end()) return;
        float cw = 1422;
        for (auto& w : it->second) if (w.name == "GRP_All") cw = w.l + w.w;
        SzX s; s.a = 480.0f / 3840; s.d = 272.0f / 2160;
        SzX o; o.e = side ? 3816 - cw : 24; o.f = 46;
        Ctx c; c.d = &d; c.out = &out; c.panel = side;
        c.bar = bar_split(std::max(0, (int)d.hp), &c.rem);
        c.tbar = bar_split(std::max(0, (int)d.trail), &c.trem);
        draw(cn, sz_mul(s, o), cw, 600, 1, "", c);
    }
};

// technique list (WBP_Rep_SkillGuideSet_P1): rows built flat in canvas units from the Rep row classes, then SZ's perspective
// (vanishing point, horizon and row heights measured on SZ footage; the flame materials are shader graphs, rebuilt from their textures)
struct SzRow { int type = 0; float tw = 0, th = 0; int cost = 0; bool lock = false; float t_on = -1; int text = 0; };
struct SzLKey { float x = 0, y = 0, s = 0, a = 0; };   // key glyphs: right edge, centre, scale, alpha (screen units)
struct SzList {
    static constexpr float XC = 441, YH = 1398, D = 1863, AS = 28, CS = 45, CU = 22, TS = 1.04f, TC = 1.42f, FK = 1.5f;   // content shift of the rows and tilt of the ultimate row (footage)
    static void proj(float X, float Y, float* x, float* y) { float s = 1 / (1 + (X - XC) / D); *x = (XC + (X - XC) * s) / 8; *y = (YH + (Y - YH) * s) / 8; }
    static float wob(float i, float t, float sd) { return sinf(i * 1.7f + t * 9.0f + sd) * 0.6f + sinf(i * 3.3f - t * 14.0f + sd * 2.1f) * 0.4f; }
    struct Q { float x0, y0, x1, y1, u0 = 0, v0 = 0, u1 = 1, v1 = 1, rot = 0, shx = 0, sx = 1, sy = 1, dx = 0, dy = 0, r = 1, g = 1, b = 1, a = 1; bool add = false; float wt = 0, wb = 0, wsd = 0, pvx = 0, rr = 0, rpx = 0, rpy = 0; int wi = 0; bool pv = false; };
    static void quad(std::vector<SzItem>& out, const char* tex, const Q& q, float t, int n = 1, const float* prof = nullptr) {
        float rc = cosf(q.rr * 0.0174533f), rs = sinf(q.rr * 0.0174533f);
        float cx = q.pv ? q.pvx : (q.x0 + q.x1) * 0.5f, cy = (q.y0 + q.y1) * 0.5f, cs = cosf(q.rot * 0.0174533f), sn = sinf(q.rot * 0.0174533f), kx = tanf(q.shx * 0.0174533f);
        auto pt = [&](float X, float Y, float* x, float* y) {
            float lx = (X - cx) * q.sx, ly = (Y - cy) * q.sy;
            lx += kx * ly;
            float X2 = cx + lx * cs - ly * sn - q.rpx, Y2 = cy + lx * sn + ly * cs - q.rpy;
            proj(q.rpx + X2 * rc - Y2 * rs + q.dx, q.rpy + X2 * rs + Y2 * rc + q.dy, x, y);
        };
        for (int i = 0; i < n; i++) {
            float f0 = (float)i / n, f1 = (float)(i + 1) / n, a = q.a;
            if (prof) { float m = (f0 + f1) * 0.5f; a *= m < prof[0] ? 0 : m < prof[1] ? (m - prof[0]) / (prof[1] - prof[0]) : m < prof[2] ? 1 : m < prof[3] ? (prof[3] - m) / (prof[3] - prof[2]) : 0; }
            if (a <= 0.004f) continue;
            float X0 = q.x0 + (q.x1 - q.x0) * f0, X1 = q.x0 + (q.x1 - q.x0) * f1;
            float t0 = q.wt ? q.wt * wob((float)(i + q.wi), t, q.wsd) : 0, t1 = q.wt ? q.wt * wob((float)(i + q.wi) + 1, t, q.wsd) : 0;
            float b0 = q.wb ? q.wb * wob((float)(i + q.wi), t, q.wsd + 5) : 0, b1 = q.wb ? q.wb * wob((float)(i + q.wi) + 1, t, q.wsd + 5) : 0;
            SzItem it;
            it.tex = tex; it.add = q.add;
            pt(X0, q.y0 + t0, &it.x[0], &it.y[0]); pt(X1, q.y0 + t1, &it.x[1], &it.y[1]); pt(X1, q.y1 + b1, &it.x[2], &it.y[2]); pt(X0, q.y1 + b0, &it.x[3], &it.y[3]);
            it.u0 = q.u0 + (q.u1 - q.u0) * f0; it.u1 = q.u0 + (q.u1 - q.u0) * f1; it.v0 = q.v0; it.v1 = q.v1;
            it.r = q.r; it.g = q.g; it.b = q.b; it.a = a;
            if (q.add) { it.r *= a; it.g *= a; it.b *= a; it.a = 1; }
            out.push_back(it);
        }
    }
    // Box brush: side margins of the texture at their own size, centre stretched
    static void box(std::vector<SzItem>& out, const char* tex, Q q, float mg, float tw, float t, int n) {
        float m = mg * tw, x0 = q.x0, x1 = q.x1;
        if (2 * m > x1 - x0) m = (x1 - x0) * 0.5f;
        float xs[4] = {x0, x0 + m, x1 - m, x1}, us[4] = {0, mg, 1 - mg, 1};
        int ns[3] = {std::max(1, n / 6), n, std::max(1, n / 6)}, wi = 0;
        for (int k = 0; k < 3; k++) {
            Q p = q;
            p.x0 = xs[k]; p.x1 = xs[k + 1]; p.u0 = us[k]; p.u1 = us[k + 1]; p.pv = true; p.pvx = (x0 + x1) * 0.5f; p.wi = wi;
            if (p.x1 > p.x0) quad(out, tex, p, t, ns[k]);
            wi += ns[k];
        }
    }
    static float tri(float t) { float f = t - floorf(t); return f < 0.5f ? f * 2 : 2 - f * 2; }
    // rows in list order (skills, attacks, ultimate last); t = seconds, tshow = seconds since the list opened, al = list opacity
    static void build(const SzRow* R, int n, float t, float tshow, float al, std::vector<SzItem>& out, SzLKey* keys) {
        static const float LX[6] = {20, 56, 100, 152, 204, 256};
        static const float IN[5][4] = {{0.0667f, 0.1333f, -100, 0}, {0.1f, 0.1667f, -70, -5}, {0.0333f, 0.1f, -110, 15}, {0, 0.0667f, -130, 15}, {0.0667f, 0.1667f, -200, 40}};
        static const float PA[4] = {0.06f, 0.19f, 0.81f, 0.94f}, PC[4] = {0, 0.12f, 0.62f, 1};
        float cys[6], cy = 1352;
        int cnt[3] = {0, 0, 0}, kind[6];
        for (int i = 0; i < n; i++) {
            int ty = R[i].type;
            if (ty == 2) cy += 49 - CU;
            cys[i] = cy; cy += ty == 0 ? 85 : 100;
            kind[i] = ty == 2 ? 4 : ty == 1 ? 2 + std::min(1, cnt[1]) : std::min(1, cnt[0]);
            cnt[ty]++;
        }
        for (int pass = 0; pass < 3; pass++)
            for (int i = n - 1; i >= 0; i--) {
                const SzRow& r = R[i];
                if ((pass == 0) != (r.type == 2) && pass < 2) continue;   // SZ order: ultimate, attacks (last first), skills
                if (pass == 1 && r.type != 1) continue;
                if (pass == 2 && r.type != 0) continue;
                if (pass == 0 && r.type != 2) continue;
                const float* A = IN[kind[i]];
                float k = tshow < 0 ? 1 : A[1] > A[0] ? std::max(0.0f, std::min(1.0f, (tshow - A[0]) / (A[1] - A[0]))) : 1;
                float op = al * k, dx = A[2] * (1 - k) - 80 * (1 - al), dy = A[3] * (1 - k), c0 = cys[i];
                if (!r.lock && r.t_on >= 0 && r.t_on < 0.1333f) { float u = r.t_on / 0.1333f; dx += (r.type == 2 ? -260 : -100) * (1 - u); op *= 0.3f + 0.7f * u; }
                if (keys) keys[i] = SzLKey();
                if (op <= 0.004f) continue;
                float l = r.type == 2 ? -32 : LX[std::min(5, i)], ph = tri(t);
                auto base = [&]() { Q q{}; q.dx = dx; q.dy = dy; q.a = op; return q; };
                if (r.type == 2) {
                    float X0 = l + 10, GL = X0 + 216 + CS, tw = r.tw * TC, th = r.th * TC, GR = GL + std::max(tw + 16, 650.0f);
                    float dk = r.lock ? 0.3f : 1;
                    if (!r.lock) {
                        Q e = base(); e.x0 = GL - 50; e.x1 = GR + 100; e.y0 = c0 - 80; e.y1 = c0 + 80; e.sx = 1.06f; e.sy = 1.2f - 0.06f * ph; e.add = true; e.a = op * (0.3f - 0.06f * ph); e.wt = 5; e.wb = 5; e.wsd = 1;
                        quad(out, "T_UI_SkillGuide_Base_TypeC_EF", e, t, 12);
                        Q m = base(); m.x0 = GL - 80; m.x1 = GR + 40; m.y0 = c0 + 28; m.y1 = c0 + 72; m.rot = -0.5f; m.add = true; m.a = op * (ph < 0.6667f ? 1 - 0.4f * (t - floorf(t)) / 0.6667f : 0.6f + 0.4f * ((t - floorf(t)) - 0.6667f) / 0.3333f); m.wt = 4; m.wb = 4; m.wsd = 2;
                        quad(out, "T_UI_SkillGuide_Ef_TypeA_22", m, t, 16, PA);
                        Q m1 = base(); m1.x0 = GL - 80; m1.x1 = GR + 60; m1.y0 = c0 - 72; m1.y1 = c0 + 80; m1.rot = 4.6f; m1.add = true; m1.a = op * 0.5f; m1.wt = 6; m1.wb = 6; m1.wsd = 3;
                        quad(out, "T_UI_SkillGuide_Ef_TypeA_23", m1, t, 16, PA);
                    }
                    Q b = base(); b.x0 = GL - 120; b.x1 = GR + 100; b.y0 = c0 - 80; b.y1 = c0 + 80; b.sy = 0.88f; b.r = b.g = b.b = dk; b.a = op * 0.9f; if (!r.lock) { b.wt = 7; b.wb = 7; b.wsd = 4; }
                    box(out, "T_UI_SkillGuide_Base_TypeC_01", b, 0.1f, 728, t, 18);
                    if (!r.lock) {
                        Q w = base(); w.x0 = GL - 74; w.x1 = GR; w.y0 = c0 - 50; w.y1 = c0 + 50; w.rot = -1; w.add = true; w.a = op * (0.08f - 0.04f * ph); w.wt = 5; w.wb = 5; w.wsd = 6;
                        quad(out, "T_UI_SkillGuide_Ef_TypeA_24", w, t, 12, PC);
                    }
                    if (r.tw > 0) {
                        Q x = base(); x.x0 = GL + 16; x.x1 = x.x0 + tw; x.y0 = c0 - th * 0.5f; x.y1 = c0 + th * 0.5f;
                        if (r.lock) { x.r = 0.2f; x.g = 0.1f; x.b = 0.2f; }
                        char nm[16]; snprintf(nm, sizeof nm, "@ltext%d", r.text); quad(out, nm, x, t);
                    }
                    if (keys) { proj(l + 208 + dx, c0 + CU + dy, &keys[i].x, &keys[i].y); keys[i].s = 1 / (1 + (l + 208 + dx - XC) / D); keys[i].a = op; }
                    continue;
                }
                bool B = r.type == 1;
                float tw = r.tw * TS, th = r.th * TS, GL = l + 200 + AS, GR = GL + std::max(tw + 40, 436.0f);
                Q b = base(); b.x0 = GL - 44; b.x1 = GR + 100; b.y0 = c0 - 70; b.y1 = c0 + 70; b.rot = B ? 0.6f : 0;
                if (r.lock) { b.r = b.g = b.b = 0.1f; b.a = op * 0.9f; }
                box(out, B ? "T_UI_SkillGuide_Base_TypeB_01" : "T_UI_SkillGuide_Base_TypeA_01", b, 0.2f, 580, t, 1);
                if (!r.lock) {
                    Q m = base(); m.x0 = GL - 120; m.x1 = GR + 140; m.y0 = c0 + 4; m.y1 = c0 + 42; m.rot = -2; m.sx = -1; m.add = true; m.a = op * std::min(1.0f, FK * (0.5f + 0.3f * ph)); m.wt = 3; m.wb = 3; m.wsd = (float)i;
                    quad(out, B ? "T_UI_SkillGuide_Ef_TypeA_21" : "T_UI_SkillGuide_Ef_TypeA_20", m, t, 16, PA);
                }
                if (B && r.cost > 0) {
                    int nc = std::min(5, r.cost);
                    float W = 128 + 112.0f * (nc - 1), bx = GL - 64, bcx = bx + W * 0.5f, bcy = c0 + 39, cs = cosf(-1.7f * 0.0174533f), sn = sinf(-1.7f * 0.0174533f);
                    for (int j = 0; j < nc; j++) {
                        Q q = base(); float ox = (bx + 64 + 112.0f * j - bcx) * 0.8f;
                        q.x0 = bcx + ox * cs - 64; q.x1 = q.x0 + 128; q.y0 = bcy + ox * sn - 20; q.y1 = q.y0 + 40; q.sx = 0.8f; q.sy = 0.64f; q.rot = -1.7f;
                        if (r.lock) { q.r = q.g = q.b = 0.3f; q.a = op * 0.7f; } else q.a = op * 0.9f;
                        quad(out, "T_UI_SpIcon", q, t);
                    }
                }
                if (r.tw > 0) {
                    Q x = base(); x.x0 = GL; x.x1 = GL + tw; x.y0 = c0 - 2.5f - th * 0.5f; x.y1 = c0 - 2.5f + th * 0.5f;
                    if (r.lock) { if (B) { x.r = 0.3f; x.g = 0.2f; x.b = 0.1f; } else { x.r = 0.4f; x.g = 0.5f; x.b = 0.6f; } x.a = op * 0.8f; }
                    char nm[16]; snprintf(nm, sizeof nm, "@ltext%d", r.text); quad(out, nm, x, t);
                }
                if (!B && r.cost > 0) {
                    Q d = base(); d.x1 = GR + 60; d.x0 = d.x1 - 128; d.y0 = c0 - 5 - 57; d.y1 = c0 - 5 + 57; d.sx = d.sy = 0.56f; d.shx = 26; d.a = op * (r.lock ? 0.55f : 0.75f);
                    char nm[32]; snprintf(nm, sizeof nm, "T_UI_SkillGuide_Num_%02d", std::min(9, r.cost)); quad(out, nm, d, t);
                }
                if (keys) { proj(l + 170 + dx, c0 + dy, &keys[i].x, &keys[i].y); keys[i].s = 1 / (1 + (l + 170 + dx - XC) / D); keys[i].a = op; }
            }
    }
};
