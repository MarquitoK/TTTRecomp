// item 71: BT3 planet burst (EftBurst) drawn port-side during the game's stage change: model and camera from the mod's transition
// pack, particles read from the game's own pool (TTT runs the schedule, its draw callbacks are empty), game fade on top
#ifndef GL_BLEND_SRC_RGB
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_BLEND_DST_ALPHA 0x80CA
#endif
#ifndef GL_BLEND_EQUATION_RGB
#define GL_BLEND_EQUATION_RGB 0x8009
#endif
#ifndef GL_COLOR_CLEAR_VALUE
#define GL_COLOR_CLEAR_VALUE 0x0C22
#endif
#ifndef GL_DEPTH_CLEAR_VALUE
#define GL_DEPTH_CLEAR_VALUE 0x0B73
#endif

struct PbTex { int w = 0, h = 0; std::vector<uint32_t> px; };
struct PbVtx { float x, y, z, u, v, r, g, b, a; };
struct PbGrp { uint32_t flags = 0; int tex = 0; std::vector<PbVtx> v; std::vector<float> dist; };
struct PbKey { float t, v; bool step; };
struct PbScene { std::string path; PbTex tex[7], mtex[2]; std::vector<PbGrp> grp; std::vector<PbKey> ch[6]; float def[6] = {}; };
static std::mutex pb_mx;
static PbScene pb_cpu;
static std::atomic<int> pb_gen{0};
static float PB_TANV = 0.517f;

// GS memory addressing (block and column layouts are XOR-linear in the coordinate bits)
static int pb_lin(const int* r, int xb, int x, int y) {
    int o = 0;
    for (int b = 0; x >> b && b < xb; b++) if ((x >> b) & 1) o ^= r[b];
    for (int b = 0; y >> b; b++) if ((y >> b) & 1) o ^= r[xb + b];
    return o;
}
static const int PB_C32[] = {1, 4, 8, 2, 16, 32}, PB_B32[] = {1, 4, 16, 2, 8}, PB_C8[] = {4, 16, 32, 2, 8, 33, 96, 128}, PB_B8[] = {1, 4, 16, 2, 8};
static const int PB_C4[] = {8, 32, 64, 2, 4, 16, 65, 192, 256}, PB_B4[] = {2, 8, 1, 4, 16};
static uint32_t pb_a32(int x, int y, int bw) { return (uint32_t)(((y >> 5) * bw + (x >> 6)) * 2048 + pb_lin(PB_B32, 3, (x >> 3) & 7, (y >> 3) & 3) * 64 + pb_lin(PB_C32, 3, x & 7, y & 7)); }
static uint32_t pb_a8(int x, int y, int bw) { return (uint32_t)(((y >> 6) * (bw >> 1) + (x >> 7)) * 8192 + pb_lin(PB_B8, 3, (x >> 4) & 7, (y >> 4) & 3) * 256 + pb_lin(PB_C8, 4, x & 15, y & 15)); }
static uint32_t pb_a4(int x, int y, int bw) { return (uint32_t)(((y >> 7) * (bw >> 1) + (x >> 7)) * 16384 + pb_lin(PB_B4, 2, (x >> 5) & 3, (y >> 4) & 7) * 512 + pb_lin(PB_C4, 5, x & 31, y & 15)); }

// texture record (64 B): pixel / CLUT upload packets (0x60 B header, data uploaded as PSMCT32), DBW at +0x18, TEX0 at +0x30
static bool pb_tex(const std::vector<uint8_t>& d, size_t base, size_t e, size_t unit, PbTex& out, std::vector<uint8_t>& mem) {
    auto rd = [&](size_t o) { uint32_t v = 0; if (o + 4 <= d.size()) memcpy(&v, &d[o], 4); return v; };
    if (e + 64 > d.size()) return false;
    uint64_t t0 = (uint64_t)rd(e + 0x30) | ((uint64_t)rd(e + 0x34) << 32);
    int psm = (int)((t0 >> 20) & 0x3F), tbw = (int)((t0 >> 14) & 0x3F), tw = 1 << ((t0 >> 26) & 0xF), th = 1 << ((t0 >> 30) & 0xF), dbw = (int)(rd(e + 0x18) & 0x3F);
    size_t pix = base + rd(e) * unit, clut = base + rd(e + 4) * unit;
    int rw = (int)rd(pix + 0x30), rh = (int)rd(pix + 0x34), cw = (int)rd(clut + 0x30), chh = (int)rd(clut + 0x34);
    if ((psm != 0x13 && psm != 0x14) || tw > 1024 || th > 1024 || rw <= 0 || rh <= 0 || rw > 1024 || rh > 1024 || pix + 0x60 + (size_t)rw * rh * 4 > d.size() ||
        cw * chh < (psm == 0x13 ? 256 : 16) || clut + 0x60 + (size_t)cw * chh * 4 > d.size()) return false;
    std::fill(mem.begin(), mem.end(), 0);
    const uint8_t* s = &d[pix + 0x60];
    for (int y = 0; y < rh; y++) for (int x = 0; x < rw; x++, s += 4) { uint32_t a = pb_a32(x, y, dbw) * 4; if (a + 4 <= mem.size()) memcpy(&mem[a], s, 4); }
    uint32_t pal[256];
    for (int i = 0; i < (psm == 0x13 ? 256 : 16); i++) {
        int j = psm == 0x13 ? ((i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1)) : i;
        uint32_t c; memcpy(&c, &d[clut + 0x60 + 4 * j], 4);
        uint32_t a = std::min<uint32_t>(255, (c >> 24) * 2);
        pal[i] = (c & 0xFFFFFF) | (a << 24);
    }
    out.w = tw; out.h = th; out.px.assign((size_t)tw * th, 0);
    for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) {
        int i;
        if (psm == 0x13) { uint32_t a = pb_a8(x, y, tbw); i = a < mem.size() ? mem[a] : 0; }
        else { uint32_t n = pb_a4(x, y, tbw); i = (n >> 1) < mem.size() ? (mem[n >> 1] >> ((n & 1) * 4)) & 15 : 0; }
        out.px[(size_t)y * tw + x] = pal[i];
    }
    return true;
}

void psp_burst_load(const std::string& path) {
    {
        std::lock_guard<std::mutex> lk(pb_mx);
        if (pb_cpu.path == path && !pb_cpu.grp.empty()) return;
    }
    auto t0 = std::chrono::steady_clock::now();
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) { printf("planeta: explosion sin paquete %s\n", path.c_str()); return; }
    std::vector<uint8_t> d;
    uint8_t buf[65536]; size_t k;
    while ((k = fread(buf, 1, sizeof(buf), fp)) > 0) d.insert(d.end(), buf, buf + k);
    fclose(fp);
    auto rd = [&](size_t o) { uint32_t v = 0; if (o + 4 <= d.size()) memcpy(&v, &d[o], 4); return v; };
    auto rf = [&](size_t o) { float f; uint32_t v = rd(o); memcpy(&f, &v, 4); return f; };
    auto rs = [&](size_t o) { int16_t v = 0; if (o + 2 <= d.size()) memcpy(&v, &d[o], 2); return v; };
    PbScene s;
    s.path = path;
    size_t e1 = rd(4), e2 = rd(8), e3 = rd(12);
    bool ok = rd(0) == 3 && e1 < e2 && e2 < e3 && e3 < d.size() && !memcmp(&d[e1], "CMAn", 4);
    std::vector<uint8_t> mem(1u << 22);
    int nt = 0, nm = 0;
    if (ok) {
        size_t co = e1 + rd(e1 + 8), dof = e1 + rd(e1 + 0x18);
        for (int i = 0; i < 6; i++) s.def[i] = rf(dof + 4 * i);
        for (uint32_t c = 0; c < rd(e1 + 0xC) && c < 16; c++) {
            uint32_t w = rd(co + 16 * c), ko = rd(co + 16 * c + 4), kn = rd(co + 16 * c + 8);
            if (w >= 6 || kn > 4096) continue;
            for (uint32_t j = 0; j < kn; j++) { size_t kk = e1 + ko + 32 * j; s.ch[w].push_back({rf(kk + 4), rf(kk + 8), (rd(kk) & 0x10) != 0}); }
        }
        uint32_t tn = rd(e3), te = e3 + rd(e3 + 4) * 4;
        for (uint32_t i = 0; i < tn && i < 7; i++) if (pb_tex(d, e3, te + 64 * i, 4, s.tex[i], mem)) nt++;
        uint32_t gn = rd(e2 + 4), go = rd(e2 + 8), mn = rd(e2 + 0xC), mo = rd(e2 + 0x10);
        for (uint32_t i = 0; i < mn && i < 2; i++) if (pb_tex(d, e2, e2 + mo + 64 * i, 1, s.mtex[i], mem)) nm++;
        for (uint32_t g = 0; g < gn && g < 8; g++) {
            size_t o = e2 + go + 24 * g;
            PbGrp G;
            G.flags = rd(o); G.tex = (int)rd(o + 4) & 1;
            size_t vo = e2 + rd(o + 8), to = e2 + rd(o + 12);
            float sc = rf(o + 16);
            int tc = rs(o + 20), vc = rs(o + 22);
            if (tc <= 0 || vc <= 0 || to + 30u * tc > d.size() || vo + 6u * vc > d.size()) continue;
            for (int t = 0; t < tc; t++) {
                size_t r = to + 30u * t;
                for (int j = 0; j < 3; j++) {
                    int vi = rs(r + 2 * j);
                    if (vi < 0 || vi >= vc) vi = 0;
                    PbVtx v;
                    v.x = rs(vo + 6 * vi) / 4096.0f * sc; v.y = rs(vo + 6 * vi + 2) / 4096.0f * sc; v.z = rs(vo + 6 * vi + 4) / 4096.0f * sc;
                    v.u = rs(r + 6 + 4 * j) / 4096.0f; v.v = rs(r + 8 + 4 * j) / 4096.0f;
                    uint16_t c0 = (uint16_t)rs(r + 18 + 4 * j), c1 = (uint16_t)rs(r + 20 + 4 * j);
                    v.r = (c0 & 0xFF) / 128.0f; v.g = (c0 >> 8) / 128.0f; v.b = (c1 & 0xFF) / 128.0f; v.a = (c1 >> 8) / 128.0f;
                    G.v.push_back(v);
                    G.dist.push_back(std::sqrt(v.x * v.x + v.y * v.y + (v.z + 50.0f) * (v.z + 50.0f)));
                }
            }
            s.grp.push_back(std::move(G));
        }
        ok = !s.grp.empty() && nm > 0 && nt >= 6;
    }
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!ok) { printf("planeta: %s no se pudo leer como paquete de explosion de BT3\n", path.c_str()); fflush(stdout); return; }
    size_t tris = 0; for (auto& g : s.grp) tris += g.v.size() / 3;
    printf("planeta: explosion lista (%s): %zu grupos, %zu triangulos, %d texturas del modelo, %d de efectos, %.1f ms\n", path.c_str(), s.grp.size(), tris, nm, nt, ms);
    fflush(stdout);
    std::lock_guard<std::mutex> lk(pb_mx);
    pb_cpu = std::move(s);
    pb_gen++;
}

static GLuint pb_prog = 0, pb_vao = 0, pb_vbo = 0, pb_fbo = 0, pb_ctex = 0, pb_dep = 0, pb_gl[9] = {};
static GLint pb_u_m = -1, pb_u_s = -1, pb_u_at = -1, pb_u_nt = -1;
static int pb_w = 0, pb_h = 0, pb_glgen = 0, pb_logs = 0, pb_seen = 0;
static std::vector<PbGrp> pb_grp;
static std::vector<PbKey> pb_ch[6];
static float pb_def[6];
static std::vector<PbVtx> pb_vb;

static float pb_gf(uint32_t a) { uint32_t v = psp_l32(a); float f; memcpy(&f, &v, 4); return f; }
static bool pb_gp(uint32_t a) { return a >= 0x08800000u && a < 0x10000000u; }

static float pb_eval(int c, float t) {
    const auto& k = pb_ch[c];
    if (k.empty()) return pb_def[c];
    if (t <= k[0].t) return k[0].v;
    for (size_t i = 0; i + 1 < k.size(); i++)
        if (t < k[i + 1].t) return k[i].step || k[i + 1].t <= k[i].t ? k[i].v : k[i].v + (k[i + 1].v - k[i].v) * (t - k[i].t) / (k[i + 1].t - k[i].t);
    return k.back().v;
}

static const char* PB_VS = "#version 330 core\nlayout(location=0) in vec3 p; layout(location=1) in vec2 t; layout(location=2) in vec4 c; uniform mat4 m; out vec2 vt; out vec4 vc;\n"
    "void main(){ gl_Position = m * vec4(p, 1.0); vt = t; vc = c; }\n";
static const char* PB_FS = "#version 330 core\nin vec2 vt; in vec4 vc; uniform sampler2D s; uniform int at, nt; out vec4 o;\n"
    "void main(){ vec4 x = (nt != 0 ? vec4(1.0) : texture(s, vt)) * vc; if (at != 0 && x.a < 0.5 / 128.0) discard; o = clamp(x, 0.0, 1.0); }\n";

static bool pb_build() {
    if (!pb_prog) {
        pb_prog = gl_program(PB_VS, PB_FS);
        if (!pb_prog) { printf("planeta: no pude preparar el programa de la explosion\n"); return false; }
        pb_u_m = glGetUniformLocation(pb_prog, "m"); pb_u_s = glGetUniformLocation(pb_prog, "s");
        pb_u_at = glGetUniformLocation(pb_prog, "at"); pb_u_nt = glGetUniformLocation(pb_prog, "nt");
        glGenVertexArrays(1, &pb_vao); glGenBuffers(1, &pb_vbo);
        glBindVertexArray(pb_vao); glBindBuffer(GL_ARRAY_BUFFER, pb_vbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PbVtx), (const void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PbVtx), (const void*)12);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(PbVtx), (const void*)20);
    }
    int g = pb_gen.load();
    if (g != pb_glgen) {
        std::lock_guard<std::mutex> lk(pb_mx);
        for (int i = 0; i < 9; i++) {
            const PbTex& t = i < 7 ? pb_cpu.tex[i] : pb_cpu.mtex[i - 7];
            if (!pb_gl[i]) glGenTextures(1, &pb_gl[i]);
            glBindTexture(GL_TEXTURE_2D, pb_gl[i]);
            uint32_t one = 0xFFFFFFFFu;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t.w ? t.w : 1, t.h ? t.h : 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.w ? (const void*)t.px.data() : (const void*)&one);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            GLint wr = i >= 6 ? GL_REPEAT : GL_CLAMP_TO_EDGE;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wr); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wr);
        }
        pb_grp = pb_cpu.grp;
        for (int i = 0; i < 6; i++) { pb_ch[i] = pb_cpu.ch[i]; pb_def[i] = pb_cpu.def[i]; }
        pb_glgen = g;
    }
    return !pb_grp.empty();
}

static void pb_quad(const float* c, const float* uv, const float* col) {
    static const int ix[6] = {0, 1, 2, 2, 1, 3};
    for (int j : ix) pb_vb.push_back({c[3 * j], c[3 * j + 1], c[3 * j + 2], uv[2 * j], uv[2 * j + 1], col[0], col[1], col[2], col[3]});
}

// full-screen picture of the burst while the game's transition effect runs (work block [0x08A7BEE0], end request [0x08B40848])
static GLuint pb_present(DestGL* d, int ke) {
    uint32_t W = psp_l32(0x08A7BEE0u);
    if (!pb_gp(W) || psp_l32(0x08B40848u) || !pb_gen.load()) { if (pb_seen) { printf("planeta: explosion terminada tras %d cuadros dibujados\n", pb_seen); fflush(stdout); pb_seen = 0; } return 0; }
    if (!pb_build()) return 0;
    int w = 480 * ke, h = 272 * ke;
    if (pb_w != d->w || pb_h != d->h) {
        if (!pb_fbo) { glGenFramebuffers(1, &pb_fbo); glGenTextures(1, &pb_ctex); glGenRenderbuffers(1, &pb_dep); }
        glBindTexture(GL_TEXTURE_2D, pb_ctex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d->w, d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindRenderbuffer(GL_RENDERBUFFER, pb_dep); glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, d->w, d->h);
        glBindFramebuffer(GL_FRAMEBUFFER, pb_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pb_ctex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, pb_dep);
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
        if (st != GL_FRAMEBUFFER_COMPLETE) { printf("planeta: bufer de la explosion incompleto (0x%X)\n", st); return 0; }
        pb_w = d->w; pb_h = d->h;
    }
    int frame = (int)psp_l32(W + 4);
    float fdist = pb_gf(W + 8);
    if (!(fdist > 0.0f)) fdist = std::min(50.0f, 8.0f + 0.4f * frame);
    float tc = frame * 2.0f;
    float P[3] = {pb_eval(0, tc), pb_eval(1, tc), pb_eval(2, tc)}, rx = pb_eval(3, tc), ry = pb_eval(4, tc), rz = pb_eval(5, tc);
    // camera to world R = RotY(ry) RotX(rx) RotZ(rz); columns = right, down, forward (BT3 world is y down like the screen)
    float cx = cosf(rx), sx = sinf(rx), cy = cosf(ry), sy = sinf(ry), cz = cosf(rz), sz = sinf(rz);
    float A[3][3] = {{cy, sy * sx, sy * cx}, {0, cx, -sx}, {-sy, cy * sx, cy * cx}};
    float Z[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}}, R[3][3];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) R[i][j] = A[i][0] * Z[0][j] + A[i][1] * Z[1][j] + A[i][2] * Z[2][j];
    float tv = PB_TANV, th = tv * (float)w / (float)h, n = 0.3f, f = 3000.0f;
    float pr[3] = {1.0f / th, 1.0f / tv, 1.0f}, M[16];
    for (int c = 0; c < 3; c++) {
        for (int r = 0; r < 2; r++) M[4 * c + r] = pr[r] * R[c][r];
        M[4 * c + 2] = (f + n) / (f - n) * R[c][2];
        M[4 * c + 3] = R[c][2];
    }
    for (int r = 0; r < 4; r++) {
        float s = 0; for (int c = 0; c < 3; c++) s += M[4 * c + r] * P[c];
        M[12 + r] = -s;
    }
    M[14] += -2.0f * f * n / (f - n);
    const int qm = nq_mode(); const uint64_t qt = perf_clock();
    GLint vp[4] = {0, 0, 0, 0}, prog = 0, vao = 0, at = GL_TEXTURE0, df = GL_LESS, bs = GL_ONE, bd = GL_ZERO, bsa = GL_ONE, bda = GL_ZERO, be = GL_FUNC_ADD; GLboolean cm[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE}, dm = GL_FALSE; GLfloat cc[4] = {0, 0, 0, 0}, cd = 1.0f;
    bool sc = false, dt = false, bl = false, st = false, cf = false;
    if (!qm) {
        SubT q_(glq_sid());
        glGetIntegerv(GL_VIEWPORT, vp); glGetIntegerv(GL_CURRENT_PROGRAM, &prog); glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao); glGetIntegerv(GL_ACTIVE_TEXTURE, &at); glGetIntegerv(GL_DEPTH_FUNC, &df);
        glGetIntegerv(GL_BLEND_SRC_RGB, &bs); glGetIntegerv(GL_BLEND_DST_RGB, &bd); glGetIntegerv(GL_BLEND_SRC_ALPHA, &bsa); glGetIntegerv(GL_BLEND_DST_ALPHA, &bda); glGetIntegerv(GL_BLEND_EQUATION_RGB, &be);
        glGetBooleanv(GL_COLOR_WRITEMASK, cm); glGetBooleanv(GL_DEPTH_WRITEMASK, &dm); glGetFloatv(GL_COLOR_CLEAR_VALUE, cc); glGetFloatv(GL_DEPTH_CLEAR_VALUE, &cd);
        sc = glIsEnabled(GL_SCISSOR_TEST); dt = glIsEnabled(GL_DEPTH_TEST); bl = glIsEnabled(GL_BLEND); st = glIsEnabled(GL_STENCIL_TEST); cf = glIsEnabled(GL_CULL_FACE);
        gls_cmp(vp, prog, vao, at, cm, dm, sc, dt, bl, st, -1, cf);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, pb_fbo);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glDepthMask(GL_TRUE);
    glClearColor(0, 0, 0, 1); glClearDepth(1.0); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(pb_prog); glBindVertexArray(pb_vao); glBindBuffer(GL_ARRAY_BUFFER, pb_vbo);
    glUniformMatrix4fv(pb_u_m, 1, GL_FALSE, M);
    glActiveTexture(GL_TEXTURE0 + 13); glUniform1i(pb_u_s, 13); glUniform1i(pb_u_nt, 0);
    struct Dr { int first, count, tex, blend, zt; bool zw, test; };
    std::vector<Dr> dr;
    auto add = [&](int first, int tex, int blend, int zt, bool zw, bool test) {
        int cnt = (int)pb_vb.size() - first;
        if (cnt <= 0) return;
        if (!dr.empty()) { Dr& b = dr.back(); if (b.first + b.count == first && b.tex == tex && b.blend == blend && b.zt == zt && b.zw == zw && b.test == test) { b.count += cnt; return; } }
        dr.push_back({first, cnt, tex, blend, zt, zw, test});
    };
    pb_vb.clear();
    float fade = std::min(1.0f, frame / 60.0f);
    // model: pass 1 every group (lava glow spreading from the pole from frame 11), pass 2 the planet's lava texture (flag 0x800)
    for (auto& g : pb_grp) {
        int first = (int)pb_vb.size();
        bool env = (g.flags & 0x800) != 0;
        for (size_t i = 0; i < g.v.size(); i++) {
            PbVtx v = g.v[i];
            if (env && frame >= 11 && g.dist[i] < fdist) {
                float lit = std::min(255.0f, 255.0f - g.dist[i] / fdist * 255.0f);
                v.r = std::min(255.0f, std::max(0.0f, v.r * 128.0f + lit)) / 128.0f;
                v.g = std::min(255.0f, std::max(0.0f, v.g * 128.0f - 0.2f * lit)) / 128.0f;
                v.b = std::min(255.0f, std::max(0.0f, v.b * 128.0f - 0.8f * lit)) / 128.0f;
            }
            pb_vb.push_back(v);
        }
        add(first, 7 + g.tex, 0, GL_LEQUAL, true, true);
    }
    for (auto& g : pb_grp) {
        if (!(g.flags & 0x800) || frame < 11) continue;
        int first = (int)pb_vb.size();
        for (size_t i = 0; i < g.v.size(); i++) {
            float lit = std::min(255.0f, 255.0f - g.dist[i] / fdist * 255.0f);
            float a = std::min(255.0f, std::max(0.0f, fade * 0.6f * lit));
            PbVtx v = g.v[i];
            v.u *= 8.0f; v.v *= 8.0f; v.r = v.g = v.b = 127.0f / 128.0f; v.a = a / 128.0f;
            pb_vb.push_back(v);
        }
        add(first, 6, 0, GL_LEQUAL, true, true);
    }
    // particles of the game's pool in slot order (draw kinds: 0x08887A30 quad, 0x08887A38 rotated quad, 0x08887A40 sprite)
    uint32_t tb = psp_l32(0x08B40838u), tt = pb_gp(tb) ? psp_l32(tb + 0x10) : 0;
    int np = 0;
    for (int i = 0; i < 350 && pb_gp(tt); i++) {
        uint32_t p = W + 0x240 + 0x70u * i;
        int life = (int)psp_l32(p + 0x40), lm = (int)psp_l32(p + 0x44);
        if (life <= 0 || !psp_l32(p + 0x64)) continue;
        uint32_t tx = psp_l32(p + 0x48), dw = psp_l32(p + 0x58), fl = psp_l32(p + 0x50), col = psp_l32(p + 0x5C);
        int ti = tx >= tt && (tx - tt) % 0x60 == 0 ? (int)((tx - tt) / 0x60) : -1;
        if (ti < 0 || ti > 6 || lm <= 0) continue;
        float pos[3] = {pb_gf(p), pb_gf(p + 4), pb_gf(p + 8)}, s = pb_gf(p + 0x4C);
        int alpha = (int)psp_l32(p + 0x60);
        float c[4], q[12];
        int first = (int)pb_vb.size();
        if (dw == 0x08887A40u) {
            static const float uv[8] = {0, 0, 1, 0, 0, 1, 1, 1};
            bool fc = (fl & 0x40) != 0;
            c[0] = (fc ? (col & 0xFF) : 0x50) / 128.0f; c[1] = (fc ? ((col >> 8) & 0xFF) : 0x50) / 128.0f; c[2] = (fc ? ((col >> 16) & 0xFF) : 0x50) / 128.0f;
            c[3] = (fc ? alpha : life * 80 / lm) / 128.0f;
            for (int j = 0; j < 4; j++) {
                float ox = (j & 1) ? s : -s, oy = (j & 2) ? s : -s;
                for (int k = 0; k < 3; k++) q[3 * j + k] = pos[k] + R[k][0] * ox + R[k][1] * oy;
            }
            pb_quad(q, uv, c);
            add(first, ti, (fl & 0x08) ? 0 : (fl & 0x100) ? 2 : 1, GL_ALWAYS, false, false);
        } else if (dw == 0x08887A38u || dw == 0x08887A30u) {
            static const float uv[8] = {0, 0, 1, 0, 0, 1, 1, 1};
            bool rot = dw == 0x08887A38u;
            float ax = 1, ay = 1, m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            if (rot) {
                bool fc = (fl & 0x40) != 0;
                c[0] = (fc ? (col & 0xFF) : 0x50) / 128.0f; c[1] = (fc ? ((col >> 8) & 0xFF) : 0x50) / 128.0f; c[2] = (fc ? ((col >> 16) & 0xFF) : 0x80) / 128.0f;
                c[3] = (alpha * life / lm) / 128.0f;
                if (fl & 0x10) ax = s;
                if (fl & 0x20) ay = s;
                if (fl & 0x80) {
                    float a1 = pb_gf(p + 0x24) + pb_gf(p + 0x20), a2 = pb_gf(p + 0x28);
                    float c1 = cosf(a1), s1 = sinf(a1), c2 = cosf(a2), s2 = sinf(a2);
                    float yy[3][3] = {{c1, 0, s1}, {0, 1, 0}, {-s1, 0, c1}}, zz[3][3] = {{c2, -s2, 0}, {s2, c2, 0}, {0, 0, 1}};
                    for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) m[a][b] = zz[a][0] * yy[0][b] + zz[a][1] * yy[1][b] + zz[a][2] * yy[2][b];
                }
            } else {
                c[0] = c[1] = c[2] = 1.0f;
                c[3] = (lm < life * 2 ? (lm - life) * 16 / lm : life * 16 / lm) / 128.0f;
                ax = ay = s;
            }
            for (int j = 0; j < 4; j++) {
                float lx = (j & 1) ? ax : -ax, ly = (j & 2) ? ay : -ay;
                for (int k = 0; k < 3; k++) q[3 * j + k] = pos[k] + m[k][0] * lx + m[k][1] * ly;
            }
            pb_quad(q, uv, c);
            add(first, ti, rot && (fl & 0x08) ? 0 : 1, GL_LESS, false, false);
        } else continue;
        np++;
    }
    if (!pb_vb.empty()) {
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pb_vb.size() * sizeof(PbVtx)), pb_vb.data(), GL_STREAM_DRAW);
        glEnable(GL_DEPTH_TEST); glEnable(GL_BLEND);
        for (const Dr& r : dr) {
            glBindTexture(GL_TEXTURE_2D, pb_gl[r.tex]);
            glDepthFunc((GLenum)r.zt); glDepthMask(r.zw ? GL_TRUE : GL_FALSE);
            glBlendEquation(r.blend == 2 ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, r.blend ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
            glUniform1i(pb_u_at, r.test ? 1 : 0);
            glDrawArrays(GL_TRIANGLES, r.first, r.count);
        }
    }
    // the game's white fade (slot 2 of 0x08B40CE0: colour rgb 0..255, alpha 0..128) keeps covering the picture
    uint32_t fs = 0x08B40CE0u + 2 * 0x60;
    float fa = psp_l32(fs + 0x58) ? pb_gf(fs + 12) / 128.0f : 0.0f;
    if (fa > 0.002f) {
        float c[4] = {pb_gf(fs) / 255.0f, pb_gf(fs + 4) / 255.0f, pb_gf(fs + 8) / 255.0f, std::min(1.0f, fa)};
        float q[12] = {-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0}, uv[8] = {0, 0, 1, 0, 0, 1, 1, 1}, I[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        pb_vb.clear(); pb_quad(q, uv, c);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pb_vb.size() * sizeof(PbVtx)), pb_vb.data(), GL_STREAM_DRAW);
        glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUniformMatrix4fv(pb_u_m, 1, GL_FALSE, I); glUniform1i(pb_u_nt, 1); glUniform1i(pb_u_at, 0);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    if (pb_seen == 0 || (pb_logs < 6 && (frame == 12 || frame == 60 || frame == 140))) {
        if (pb_logs < 6) {
            pb_logs++;
            printf("planeta: explosion cuadro %d: %d particulas, camara (%.1f %.1f %.1f) giro (%.3f %.3f), distancia de luz %.1f, fundido %.2f\n", frame, np, P[0], P[1], P[2], rx, ry, fdist, fa);
            fflush(stdout);
        }
    }
    pb_seen++;
    if (qm) { glActiveTexture(GL_TEXTURE0); glUseProgram(glr_prog); glBindVertexArray(glr_vao); glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0); }   // the next flush rebuilds the draw state
    else {
        glActiveTexture((GLenum)at);
        glUseProgram((GLuint)prog); glBindVertexArray((GLuint)vao);
        glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
        glViewport(vp[0], vp[1], vp[2], vp[3]);
        glColorMask(cm[0], cm[1], cm[2], cm[3]); glDepthMask(dm); glDepthFunc((GLenum)df);
        glClearColor(cc[0], cc[1], cc[2], cc[3]); glClearDepth(cd);
        glBlendEquation((GLenum)be); glBlendFuncSeparate((GLenum)bs, (GLenum)bd, (GLenum)bsa, (GLenum)bda);
        if (sc) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        if (dt) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (bl) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (st) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
        if (cf) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    }
    nq_st[qm][10]++; nq_st[qm][11] += perf_clock() - qt;
    return pb_ctex;
}
