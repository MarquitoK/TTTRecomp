#pragma once

// GPU rasterization. Reuses the primitive queue and the TexState structure of the software
// rasterizer: every Job already comes in screen coordinates with 1/w, so here the GE
// state is only translated to OpenGL state and drawn in batches.
// Enabled with TTT_GLRAST=1. With 0 the software path is untouched.

#ifdef _WIN32

// port-side copy of the GL state the passes save and restore (a glGet stalls the driver thread)
struct GlShadow { uint32_t en = 0; bool vpk = false; GLint vp[4] = {0, 0, 0, 0}; GLboolean cm[4] = {1, 1, 1, 1}; GLboolean dm = 1; GLuint sm = 0xFFFFFFFFu, prog = 0, vao = 0; GLenum at = 0x84C0, df = 0x0201; };
static thread_local GlShadow gls;
static inline int gls_bit(GLenum c) { return c == GL_SCISSOR_TEST ? 1 : c == GL_DEPTH_TEST ? 2 : c == GL_BLEND ? 4 : c == GL_STENCIL_TEST ? 8 : c == GL_CULL_FACE ? 16 : 0; }
static inline void gls_enable(GLenum c) { gls.en |= gls_bit(c); glEnable(c); }
static inline void gls_disable(GLenum c) { gls.en &= ~gls_bit(c); glDisable(c); }
static inline void gls_viewport(GLint x, GLint y, GLsizei w, GLsizei h) { gls.vpk = true; gls.vp[0] = x; gls.vp[1] = y; gls.vp[2] = w; gls.vp[3] = h; glViewport(x, y, w, h); }
static inline void gls_cmask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { gls.cm[0] = r; gls.cm[1] = g; gls.cm[2] = b; gls.cm[3] = a; glColorMask(r, g, b, a); }
static inline void gls_dmask(GLboolean m) { gls.dm = m; glDepthMask(m); }
static inline void gls_smask(GLuint m) { gls.sm = m; glStencilMask(m); }
static inline void gls_prog(GLuint p) { gls.prog = p; glUseProgram(p); }
static inline void gls_vao(GLuint v) { gls.vao = v; glBindVertexArray(v); }
static inline void gls_unit(GLenum u) { gls.at = u; glActiveTexture(u); }
static inline void gls_dfunc(GLenum f) { gls.df = f; glDepthFunc(f); }
#define glEnable(c) gls_enable(c)
#define glDisable(c) gls_disable(c)
#define glViewport(x, y, w, h) gls_viewport(x, y, w, h)
#define glColorMask(r, g, b, a) gls_cmask(r, g, b, a)
#define glDepthMask(m) gls_dmask(m)
#define glStencilMask(m) gls_smask(m)
#define glUseProgram(p) gls_prog(p)
#define glBindVertexArray(v) gls_vao(v)
#define glActiveTexture(u) gls_unit(u)
#define glDepthFunc(f) gls_dfunc(f)
static uint64_t gls_bad[12];   // shadow vs GL, counted where the old mode queries: viewport, program, vao, unit, stencil mask, colour mask, depth mask, scissor, depth, blend, stencil, cull
static void gls_cmp(const GLint* vp, GLint prog, GLint vao, GLint at, const GLboolean* cm, GLboolean dm, bool sc, bool dt, bool bl, bool st, int sm = -1, int cf = -1) {
    if (gls.vpk && memcmp(vp, gls.vp, sizeof(gls.vp))) gls_bad[0]++;
    if ((GLuint)prog != gls.prog) gls_bad[1]++;
    if ((GLuint)vao != gls.vao) gls_bad[2]++;
    if ((GLenum)at != gls.at) gls_bad[3]++;
    if (sm >= 0 && (GLuint)sm != gls.sm) gls_bad[4]++;
    if (memcmp(cm, gls.cm, 4)) gls_bad[5]++;
    if (dm != gls.dm) gls_bad[6]++;
    if (sc != !!(gls.en & 1)) gls_bad[7]++;
    if (dt != !!(gls.en & 2)) gls_bad[8]++;
    if (bl != !!(gls.en & 4)) gls_bad[9]++;
    if (st != !!(gls.en & 8)) gls_bad[10]++;
    if (cf >= 0 && (cf != 0) != !!(gls.en & 16)) gls_bad[11]++;
}

static int glr_var = -1;
// if anything fails it turns off once: never retry per primitive
static bool glr_enabled() {
    if (glr_var == 0) return false;
    if (glr_var < 0) {
        const char* e = getenv("TTT_GLRAST");
        glr_var = (e && atoi(e) != 0) ? 1 : 0;
        if (!glr_var) return false;
        printf("rasterizado por OpenGL (TTT_GLRAST=1)\n");
        if (!gl_ready()) { glr_var = 0; printf("gl: sin contexto, sigo por software\n"); return false; }
    }
    static thread_local int here = -1;
    if (here < 0) {
        here = gl_here() ? 1 : 0;
        if (!here) { glr_var = 0; printf("gl: este hilo no obtuvo contexto, sigo por software\n"); return false; }
    }
    return glr_var != 0;
}

// TTT_MIPS: 0 never, 1 like software (only when the game asks), 2 always except with alpha test
static int glr_mips_var = -1;
static int glr_mips() {
    if (glr_mips_var < 0) { const char* e = getenv("TTT_MIPS"); glr_mips_var = e ? atoi(e) : 1; }
    return glr_mips_var;
}

// TTT_SHOW_LOD=1 or F8: paints the level of detail instead of the texture
static int glr_verlod_var = -1;
static int glr_verlod() {
    if (glr_verlod_var < 0) { const char* e = getenv("TTT_SHOW_LOD"); glr_verlod_var = e ? atoi(e) : 0; }
    return glr_verlod_var || psp_window_debug();
}

// TTT_UV=1: one line per texture with the coordinate range and texel density
static int glr_uv_var = -1;
static bool glr_uv() {
    if (glr_uv_var < 0) { const char* e = getenv("TTT_UV"); glr_uv_var = e ? atoi(e) : 0; }
    return glr_uv_var != 0;
}

// TTT_TRACE_UV=<texture address>: dumps per-vertex coordinates of its first triangles
static uint32_t glr_trace_addr = 0xFFFFFFFFu;
static int glr_trace_n = 0;
static uint32_t glr_traceuv() {
    if (glr_trace_addr == 0xFFFFFFFFu) {
        const char* e = getenv("TTT_TRACE_UV");
        glr_trace_addr = e ? (uint32_t)strtoul(e, nullptr, 0) : 0;
    }
    return glr_trace_addr;
}

static float glr_aniso_max = 0.0f;

// F7 cycles 1x, 4x and the card maximum; F6 cycles the LOD bias
static float glr_aniso() {
    if (glr_aniso_max == 0.0f) {
        glr_aniso_max = 1.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glr_aniso_max);
        if (glr_aniso_max < 1.0f) glr_aniso_max = 1.0f;
        printf("gl: mipmaps modo %d, anisotropia maxima de la tarjeta %.0fx\n", glr_mips(), glr_aniso_max);
    }
    int i = psp_window_aniso();
    float v = i == 0 ? 1.0f : (i == 1 ? 4.0f : glr_aniso_max);
    return v > glr_aniso_max ? glr_aniso_max : v;
}

static float glr_bias() {
    static const float t[4] = {0.0f, -0.5f, -1.0f, -1.5f};
    return t[psp_window_bias() & 3];
}

static int glr_warnings = 0;
static void glr_warning(const char* q, int v) {
    if (glr_warnings++ < 30) printf("gl aviso: %s %d\n", q, v);
}

// render targets: one RGBA8 texture and one 16-bit depth per game buffer
struct DestGL { uint32_t dir; int width, fmt, w, h; GLuint fbo, tex, depth; GLuint mfbo, mcol, mdep; int mh; bool dirty; GLuint dtex, cfbo; };
#define GLR_DEST 12
static DestGL glr_dest[GLR_DEST];
static int glr_dest_n = 0;
static DestGL* glr_current = nullptr;

// MSAA (Graphics "Antialiasing", TTT_MSAA samples, read at startup): wide 8888 targets draw into a multisampled twin,
// resolved into the texture only when something reads it (texture, copy, outline, readback, present)
static int glr_msaa() {
    static int v = -1;
    if (v < 0) { const char* e = getenv("TTT_MSAA"); v = e && *e ? atoi(e) : 0; v = v >= 8 ? 8 : v >= 4 ? 4 : v >= 2 ? 2 : 0; }
    return v;
}
static inline GLuint dfbo(const DestGL* d) { return d->mfbo ? d->mfbo : d->fbo; }
static int glr_dtex_var = -1;
static bool glr_dtex_on() { if (glr_dtex_var < 0) { const char* e = getenv("TTT_DEPTH_TEX"); glr_dtex_var = e && *e ? atoi(e) : 0; } return glr_dtex_var > 0; }
static inline bool ddirect(const DestGL* d) { return d->dtex && d->cfbo && !d->mfbo; }   // the passes read the target depth directly
static long dd_n[2];   // depth blits skipped, done
// GPU time per zone (GL_TIME_ELAPSED, read 3 frames later; optional functions): TTT_GPU_PROF=0 turns it off
enum { GQ_DRAW, GQ_FX, GQ_CDK, GQ_OL, GQ_COPY, GQ_RES, GQ_FXAA, GQ_SM, GQ_VIEW, GQ_FIG, GQ_MAP, GQ_FX3, GQ_2D, GQ_ATM, GQ_N };
static const char* GQ_NOM[GQ_N] = {"dibujos", "efectos de pantalla", "oscuridad de carga", "borde", "copias", "resolucion msaa", "fxaa", "suave", "vistas", "luchadores", "mapa", "efectos 3D", "2D", "atmosfera y carga"};
static int gq_cat = GQ_DRAW, gq_next = GQ_DRAW;   // category of the draws being batched (set per job in the flush loop)
typedef void(__stdcall* T_gqGen)(GLsizei, GLuint*);
typedef void(__stdcall* T_gqBegin)(GLenum, GLuint);
typedef void(__stdcall* T_gqEnd)(GLenum);
typedef void(__stdcall* T_gqGetiv)(GLuint, GLenum, GLint*);
typedef void(__stdcall* T_gqGet64)(GLuint, GLenum, unsigned long long*);
static T_gqGen gq_gen = nullptr; static T_gqBegin gq_beg = nullptr; static T_gqEnd gq_endf = nullptr; static T_gqGetiv gq_getiv = nullptr; static T_gqGet64 gq_get64 = nullptr;
static int gq_ok = -1, gq_slot = 0, gq_active = -1;
static int gq_every = -1; static bool gq_samp = true; static long gq_rf = 0, gq_rf0 = 0;   // GPU timing in 1 frame of TTT_GPU_PROF_N (4): queries and areas cost CPU in the driver
static GLuint gq_ids[16][768]; static uint8_t gq_z[16][768]; static int gq_n[16]; static uint8_t gq_ab[16];
static double gq_ms[GQ_N], gq_max = 0; static long gq_frames = 0, gq_lost = 0; static long long gq_cp0 = 0, gq_px0 = 0;
static std::thread::id gq_tid; static bool gq_tid_set = false;   // queries belong to one GL context: the one that presents (gq_frame)
static bool gq_ready() {
    if (!gq_tid_set || std::this_thread::get_id() != gq_tid) return false;
    if (gq_ok < 0) {
        const char* e = getenv("TTT_GPU_PROF");
        gq_ok = 0;
        if (e && *e == '0') return false;
        gq_gen = (T_gqGen)gl_proc("glGenQueries"); gq_beg = (T_gqBegin)gl_proc("glBeginQuery"); gq_endf = (T_gqEnd)gl_proc("glEndQuery");
        gq_getiv = (T_gqGetiv)gl_proc("glGetQueryObjectiv"); gq_get64 = (T_gqGet64)gl_proc("glGetQueryObjectui64v");
        if (gq_gen && gq_beg && gq_endf && gq_getiv && gq_get64) {
            while (glGetError()) {}
            for (int k = 0; k < 16; k++) gq_gen(768, gq_ids[k]);
            gq_beg(0x88BF, gq_ids[0][0]); gq_endf(0x88BF);
            GLenum er = glGetError();
            gq_ok = er ? 0 : 1;
            if (er) printf("gpu: la medicion de tiempo fallo al probarla (error GL 0x%X)\n", er);
        }
        printf("gpu: medicion de tiempo por zonas %s\n", gq_ok ? "activa" : "no disponible");
    }
    return gq_ok > 0;
}
struct SubT { int id; uint64_t t; SubT(int i) : id(i), t(perf_clock()) {} void end() { if (t) { perf_sub_add(id, perf_clock() - t); t = 0; } } ~SubT() { end(); } };
static int glq_sid() { static int id = perf_sub_id("consultas GL"); return id; }
static GLboolean glr_cm[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};   // colour mask set by the last glr_state
static uint64_t nq_st[2][14];   // per mode: box, darkness, atm (calls/entry/total), smooth motion, explosion, msaa resolve (calls/cycles)
static int nq_mode() {
    static int v = -1;
    if (v < 0) { const char* e = getenv("TTT_GL_NOQ"); v = e && *e ? atoi(e) : 1; }
    if (v != 2) return v ? 1 : 0;
    static uint32_t n = 0; static int m = 0; static uint64_t t0 = 0, rep = 0;
    if ((n++ & 15) == 0) {
        uint64_t t = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (!t0) t0 = rep = t;
        m = (int)(((t - t0) / 10000) & 1);
        if (t >= rep + 20000) {
            rep = t;
            for (int k = 0; k < 2; k++)
                printf("consultas: %s | ciclos por llamada: caja del delineado %.0f (%llu), oscuridad %.0f (%llu), entrada atm %.0f, pase atm %.0f (%llu)\n", k ? "sin consultas" : "anterior",
                       nq_st[k][0] ? (double)nq_st[k][1] / nq_st[k][0] : 0.0, (unsigned long long)nq_st[k][0], nq_st[k][2] ? (double)nq_st[k][3] / nq_st[k][2] : 0.0, (unsigned long long)nq_st[k][2],
                       nq_st[k][4] ? (double)nq_st[k][5] / nq_st[k][4] : 0.0, nq_st[k][4] ? (double)nq_st[k][6] / nq_st[k][4] : 0.0, (unsigned long long)nq_st[k][4]);
            for (int k = 0; k < 2; k++)
                printf("consultas: %s | ciclos por llamada: movimiento suave %.0f (%llu), explosion %.0f (%llu), resolucion msaa %.0f (%llu)\n", k ? "sin consultas" : "anterior",
                       nq_st[k][8] ? (double)nq_st[k][9] / nq_st[k][8] : 0.0, (unsigned long long)nq_st[k][8], nq_st[k][10] ? (double)nq_st[k][11] / nq_st[k][10] : 0.0, (unsigned long long)nq_st[k][10],
                       nq_st[k][12] ? (double)nq_st[k][13] / nq_st[k][12] : 0.0, (unsigned long long)nq_st[k][12]);
            printf("consultas: copia del estado distinta de GL: vista %llu, programa %llu, vao %llu, unidad %llu, stencil %llu, color %llu, profundidad %llu, recorte %llu, prueba z %llu, mezcla %llu, prueba stencil %llu, caras %llu\n",
                   (unsigned long long)gls_bad[0], (unsigned long long)gls_bad[1], (unsigned long long)gls_bad[2], (unsigned long long)gls_bad[3], (unsigned long long)gls_bad[4], (unsigned long long)gls_bad[5],
                   (unsigned long long)gls_bad[6], (unsigned long long)gls_bad[7], (unsigned long long)gls_bad[8], (unsigned long long)gls_bad[9], (unsigned long long)gls_bad[10], (unsigned long long)gls_bad[11]);
            fflush(stdout);
        }
    }
    return m;
}
struct GqZone {
    bool own = false;
    explicit GqZone(int z) {
        if (gq_active >= 0 || !gq_samp || !gq_ready()) return;
        int s = gq_slot; if (gq_n[s] >= 768) return;
        gq_z[s][gq_n[s]] = (uint8_t)z; gq_beg(0x88BF, gq_ids[s][gq_n[s]]); gq_active = gq_n[s]++; own = true;
    }
    ~GqZone() { if (own) { gq_endf(0x88BF); gq_active = -1; glFlush(); } }   // flushed at once: a later flush would add the emulation time in between to the zone
};
// glTextureBarrier: a full-screen pass that reads only its own pixel samples the target directly, no copy (TTT_TBAR=0 = always copy)
typedef void(__stdcall* T_tbar)();
static T_tbar gl_tbar_fn = nullptr; static int gl_tbar_ok = -1, gl_tbar_n = 0;
static bool gl_tbar() {
    if (gl_tbar_ok < 0) {
        gl_tbar_ok = 0;
        const char* e = getenv("TTT_TBAR");
        typedef const unsigned char*(__stdcall* T_gsi)(GLenum, GLuint);
        T_gsi gsi = (T_gsi)gl_proc("glGetStringi");
        bool arb = false, nv = false; GLint ne = 0;
        if (gsi) { glGetIntegerv(0x821D, &ne); for (GLint i = 0; i < ne; i++) { const char* x = (const char*)gsi(0x1F03, (GLuint)i); if (!x) continue; if (!strcmp(x, "GL_ARB_texture_barrier")) arb = true; if (!strcmp(x, "GL_NV_texture_barrier")) nv = true; } }
        if (arb) gl_tbar_fn = (T_tbar)gl_proc("glTextureBarrier");
        if (!gl_tbar_fn && nv) gl_tbar_fn = (T_tbar)gl_proc("glTextureBarrierNV");
        gl_tbar_ok = gl_tbar_fn && !(e && *e == '0') ? 1 : 0;
        printf("gl: barrera de textura %s (ARB %d, NV %d)\n", gl_tbar_ok ? "en uso" : gl_tbar_fn ? "apagada" : "no disponible", (int)arb, (int)nv);
    }
    return gl_tbar_ok > 0;
}
// resolve: colour averaged over the samples, alpha (the PSP stencil: outline layers) from one sample, exact as without MSAA
static GLuint glr_prog = 0, glr_vao = 0, glr_vbo = 0;
static GLuint rs_prog = 0, rs_vao = 0; static GLint rs_u_src = -1, rs_u_n = -1;
// TTT_RES_BOX: a box copy of an MSAA target resolves only its box (1 default; 0 the whole target every time; 2 alternates every 10 s)
static long rb_st[2][2]; static double rb_mpx[2], rb_t0 = 0;
static bool rb_on() {
    static int m = -1;
    if (m < 0) { const char* e = getenv("TTT_RES_BOX"); m = e && *e ? atoi(e) : 1; }
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!rb_t0) rb_t0 = now;
    if (now - rb_t0 >= 10.0) {
        rb_t0 = now;
        static int logs = 0;
        if (m == 2 && logs++ < 60 && (rb_st[0][0] + rb_st[0][1] + rb_st[1][0] + rb_st[1][1]))
            for (int k = 1; k >= 0; k--) if (rb_st[k][0] + rb_st[k][1]) printf("resolucion: %s, %ld completas, %ld por caja, %.1f Mpx\n", k ? "por caja" : "completa", rb_st[k][0], rb_st[k][1], rb_mpx[k]);
        for (int k = 0; k < 2; k++) { rb_st[k][0] = rb_st[k][1] = 0; rb_mpx[k] = 0; }
    }
    return m == 2 ? ((long long)(now / 10.0) & 1) == 0 : m != 0;
}
static void glr_resolve(DestGL* d, const int* box = nullptr) {
    GqZone gqz_(GQ_RES);
    if (!d || !d->mfbo || !d->dirty) return;
    if (box && (box[2] <= box[0] || box[3] <= box[1])) return;
    { int k = rb_on() ? 1 : 0; if (box) { rb_st[1][1]++; rb_mpx[1] += (box[2] - box[0]) * (double)(box[3] - box[1]) / 1e6; } else { rb_st[k][0]++; rb_mpx[k] += d->w * (double)d->mh / 1e6; } }
    if (!box) d->dirty = false;
    if (!rs_prog) {
        rs_prog = gl_program("#version 330 core\nvoid main(){ vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID >> 1) * 4.0 - 1.0); gl_Position = vec4(p, 0.0, 1.0); }\n",
                             "#version 330 core\nuniform sampler2DMS src; uniform int n; out vec4 o;\n"
                             "void main(){ ivec2 p = ivec2(gl_FragCoord.xy); vec4 a = texelFetch(src, p, 0); vec3 c = a.rgb; for(int i = 1; i < n; i++) c += texelFetch(src, p, i).rgb; o = vec4(c / float(n), a.a); }\n");
        glGenVertexArrays(1, &rs_vao);
        rs_u_src = glGetUniformLocation(rs_prog, "src"); rs_u_n = glGetUniformLocation(rs_prog, "n");
    }
    const int rqm = nq_mode(); const uint64_t rqt = perf_clock();
    struct RqC { int m; uint64_t t; ~RqC() { nq_st[m][12]++; nq_st[m][13] += perf_clock() - t; } } rqc_{rqm, rqt};
    GLint vp[4], prog = 0, vao = 0, at = 0, sm = 0; GLboolean cm[4], dm;   // the caller may be in the middle of its own state
    bool sc, dt, bl, st;
    if (rqm && gls.vpk) {
        memcpy(vp, gls.vp, sizeof(vp)); prog = (GLint)gls.prog; vao = (GLint)gls.vao; at = (GLint)gls.at; sm = (GLint)gls.sm; memcpy(cm, gls.cm, 4); dm = gls.dm;
        sc = gls.en & 1; dt = gls.en & 2; bl = gls.en & 4; st = gls.en & 8;
    } else {
        SubT q_(glq_sid());
        glGetIntegerv(GL_VIEWPORT, vp); glGetIntegerv(GL_CURRENT_PROGRAM, &prog); glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao); glGetIntegerv(GL_ACTIVE_TEXTURE, &at);
        glGetIntegerv(GL_STENCIL_WRITEMASK, &sm); glGetBooleanv(GL_COLOR_WRITEMASK, cm); glGetBooleanv(GL_DEPTH_WRITEMASK, &dm);
        sc = glIsEnabled(GL_SCISSOR_TEST); dt = glIsEnabled(GL_DEPTH_TEST); bl = glIsEnabled(GL_BLEND); st = glIsEnabled(GL_STENCIL_TEST);
        gls_cmp(vp, prog, vao, at, cm, dm, sc, dt, bl, st, sm);
    }
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, d->mfbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, d->fbo);
    if (!box) glBlitFramebuffer(0, 0, d->w, d->mh, 0, 0, d->w, d->mh, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
    if (box) glViewport(box[0], box[1], box[2] - box[0], box[3] - box[1]);   // the fullscreen triangle covers only the box; texelFetch uses window coordinates
    else glViewport(0, 0, d->w, d->mh);
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST); glStencilMask(0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(rs_prog); glBindVertexArray(rs_vao);
    glActiveTexture(GL_TEXTURE0 + 7); glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, d->mcol);
    glUniform1i(rs_u_src, 7); glUniform1i(rs_u_n, glr_msaa());
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture((GLenum)at);
    glUseProgram((GLuint)prog); glBindVertexArray((GLuint)vao);
    glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glColorMask(cm[0], cm[1], cm[2], cm[3]); glDepthMask(dm); glStencilMask((GLuint)sm);
    if (sc) glEnable(GL_SCISSOR_TEST); if (dt) glEnable(GL_DEPTH_TEST); if (bl) glEnable(GL_BLEND); if (st) glEnable(GL_STENCIL_TEST);
}
static void glr_resolve_tex(GLuint t) { for (int i = 0; i < glr_dest_n; i++) if (glr_dest[i].tex == t && glr_dest[i].mfbo) { glr_resolve(&glr_dest[i]); return; } }

static DestGL* glr_find(uint32_t dir, int width, int fmt) {
    for (int i = 0; i < glr_dest_n; i++)
        if (glr_dest[i].dir == dir && glr_dest[i].width == width) return &glr_dest[i];
    if (glr_dest_n >= GLR_DEST) return &glr_dest[0];
    uint64_t t0 = st_us();
    DestGL& d = glr_dest[glr_dest_n++];
    d.dir = dir; d.width = width; d.fmt = fmt;
    d.w = width; d.h = 512 * scale_int();
    glGenTextures(1, &d.tex);
    glBindTexture(GL_TEXTURE_2D, d.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d.w, d.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    d.depth = 0; d.dtex = 0; d.cfbo = 0;
    if (glr_dtex_on()) {   // depth as a texture: the passes sample it instead of blitting it (cfbo = colour only, no feedback loop)
        glGenTextures(1, &d.dtex);
        glBindTexture(GL_TEXTURE_2D, d.dtex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, d.w, d.h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, d.tex);
    } else {
        glGenRenderbuffers(1, &d.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, d.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, d.w, d.h);
    }
    glGenFramebuffers(1, &d.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, d.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, d.tex, 0);
    if (d.dtex) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, d.dtex, 0);
    else glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, d.depth);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (st != GL_FRAMEBUFFER_COMPLETE) printf("gl: destino 0x%08X incompleto (0x%X)\n", dir, st);
    else printf("gl: destino 0x%08X %dx%d fmt %d%s\n", dir, d.w, d.h, fmt, d.dtex ? ", profundidad en textura" : "");
    if (d.dtex) {
        glGenFramebuffers(1, &d.cfbo);
        glBindFramebuffer(GL_FRAMEBUFFER, d.cfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, d.tex, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { printf("gl: destino 0x%08X sin destino de solo color\n", dir); glDeleteFramebuffers(1, &d.cfbo); d.cfbo = 0; }
    }
    d.mfbo = 0; d.dirty = false; d.mh = d.h;
    int ms = glr_msaa();
    if (ms && fmt == 3 && width >= 480 * scale_int()) {
        d.mh = std::min(d.h, 280 * scale_int());
        glGenTextures(1, &d.mcol); glGenRenderbuffers(1, &d.mdep);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, d.mcol);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, ms, GL_RGBA8, d.w, d.mh, GL_TRUE);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, d.mdep);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, ms, GL_DEPTH24_STENCIL8, d.w, d.mh);
        glGenFramebuffers(1, &d.mfbo);
        glBindFramebuffer(GL_FRAMEBUFFER, d.mfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, d.mcol, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, d.mdep);
        GLenum ms_st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        printf("gl: antialiasing %dx en destino 0x%08X %dx%d %s\n", ms, dir, d.w, d.mh, ms_st == GL_FRAMEBUFFER_COMPLETE ? "listo" : "incompleto, sin antialiasing");
        if (ms_st != GL_FRAMEBUFFER_COMPLETE) { d.mfbo = 0; d.mh = d.h; }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : d.fbo);
    if (t0) psp_stall_note(4, st_us() - t0, (uint32_t)d.w * d.h);
    return &d;
}

static long long glr_reads = 0, glr_no_dest = 0, glr_copies = 0, glr_copy_px = 0;

static uint32_t glr_seen[48];
static int glr_seen_n = 0;

static bool glr_new(uint32_t h) {
    for (int i = 0; i < glr_seen_n; i++) if (glr_seen[i] == h) return false;
    if (glr_seen_n >= 48) return false;   // full: stay quiet, do not repeat per primitive
    glr_seen[glr_seen_n++] = h;
    return true;
}

// when reading a render target, the one with the matching declared width wins
static unsigned glr_target_tex(uint32_t dir, int width, int tw, int th, int tfmt, int* fmt, int* rw, int* rh) {
    int chosen = -1, loose = -1;
    for (int i = 0; i < glr_dest_n; i++) {
        if (glr_dest[i].dir != dir) continue;
        if (glr_dest[i].width == width) chosen = i;
        else if (loose < 0) loose = i;
    }
    if (chosen < 0) chosen = loose;
    glr_reads++;
    if (chosen < 0) glr_no_dest++;
    uint32_t h = dir ^ ((uint32_t)width << 3) ^ ((uint32_t)tw << 11) ^ ((uint32_t)th << 19) ^ ((uint32_t)tfmt << 27);
    if (glr_new(h))
        printf("gl: lee destino 0x%08X: tex %dx%d stride %d fmt %d -> %s (ancho %d fmt %d)\n",
               dir, tw, th, width, tfmt,
               chosen < 0 ? "SIN DESTINO" : (glr_dest[chosen].width == width ? "exacto" : "ANCHO DISTINTO"),
               chosen < 0 ? 0 : glr_dest[chosen].width, chosen < 0 ? -1 : glr_dest[chosen].fmt);
    if (chosen < 0) return 0;
    *fmt = glr_dest[chosen].fmt;
    *rw = glr_dest[chosen].w; *rh = glr_dest[chosen].h;
    return glr_dest[chosen].tex;
}

// textures: the decoded chain is already RGBA8, only level 0 is uploaded here
// texture filter (Graphics "Texture filter", TTT_TEXFILTER 0 off, 1 = 2x, 2 = 4x, read at startup): xBR-Z style scale of the
// decoded texture as extra mip levels above the original; stair steps of low-res art become smooth edges
static int glr_texfilter() {
    static int v = -1;
    if (v < 0) { const char* e = getenv("TTT_TEXFILTER"); v = e && *e ? atoi(e) : 0; v = v < 0 ? 0 : v > 2 ? 2 : v; }
    return v;
}
static int glr_up_of(int w, int h) {
    int up = glr_texfilter();
    if (up == 2 && w * h > 512 * 512) up = 1;
    if (w * h > 1024 * 1024 || w < 4 || h < 4) up = 0;
    return up;
}
// xBR-Z style scaler (own implementation of the published idea): corner blend decisions taken once on the source
// (diagonal edge weights over 4x4 blocks, dominant/steep/shallow tests in YCbCr), then each output pixel of the NxN
// block is blended by the area its sub-square has under the edge shape; one pass for 2x or 4x
struct XzCol { float y, cb, cr, a; };
static inline XzCol xz_col(uint32_t c) {
    float r = (float)(c & 255), g = (float)((c >> 8) & 255), b = (float)((c >> 16) & 255);
    float y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    return {y, (b - y) * 0.5389f, (r - y) * 0.635f, (float)(c >> 24)};
}
static inline float xz_d(const XzCol& p, const XzCol& q) {
    float y = p.y - q.y, b = p.cb - q.cb, r = p.cr - q.cr, a = p.a - q.a;
    return std::sqrt(y * y + b * b + r * r + a * a);
}
static inline uint32_t xz_mix(uint32_t a, uint32_t b, float t) {
    uint32_t o = 0;
    for (int s = 0; s < 32; s += 8) { float x = (float)((a >> s) & 255), y = (float)((b >> s) & 255); o |= (uint32_t)(int)(x + (y - x) * t + 0.5f) << s; }
    return o;
}
struct XzTab { float a[4][5][16]; };   // rotation, shape (diagonal, shallow, steep, both, corner), sub-pixel
static const XzTab& xz_tab(int n) {
    static XzTab t[5]; static bool ok[5] = {false, false, false, false, false};
    if (!ok[n]) {
        ok[n] = true;
        static const int RX[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
        for (int r = 0; r < 4; r++)
            for (int sy = 0; sy < n; sy++)
                for (int sx = 0; sx < n; sx++) {
                    int cnt[5] = {0, 0, 0, 0, 0};
                    for (int qy = 0; qy < 16; qy++)
                        for (int qx = 0; qx < 16; qx++) {
                            float ax = (sx + (qx + 0.5f) / 16.0f) / n - 0.5f, ay = (sy + (qy + 0.5f) / 16.0f) / n - 0.5f;
                            int ca = RX[r][0], sa = RX[r][1];
                            float x = ax * ca + ay * sa + 0.5f, y = -ax * sa + ay * ca + 0.5f;   // canonical: corner at bottom right
                            bool dg = x + y > 1.5f, sh = x + 2.0f * y > 2.0f, st = 2.0f * x + y > 2.0f;
                            bool co = x > 0.5f && y > 0.5f && (x - 0.5f) * (x - 0.5f) + (y - 0.5f) * (y - 0.5f) > 0.25f;
                            cnt[0] += dg; cnt[1] += sh; cnt[2] += st; cnt[3] += sh || st; cnt[4] += co;
                        }
                    for (int k = 0; k < 5; k++) t[n].a[r][k][sy * n + sx] = cnt[k] / 256.0f;
                }
    }
    return t[n];
}
static void xbrz_scale(const std::vector<uint32_t>& s, int w, int h, int n, std::vector<uint32_t>& o, int nt_max = 8) {
    const float EQ = 30.0f, DOM = 3.6f, STEEP = 2.2f;
    static thread_local std::vector<XzCol> cv_tl; static thread_local std::vector<uint8_t> bl_tl;   // per pixel corner blend: 2 bits each, TL TR BL BR
    std::vector<XzCol>& cv = cv_tl; std::vector<uint8_t>& bl = bl_tl;   // the row workers must see the caller's buffers, not their own thread_local ones
    cv.resize((size_t)w * h); bl.assign((size_t)w * h, 0);
    for (size_t i = 0; i < cv.size(); i++) cv[i] = xz_col(s[i]);
    auto ix = [&](int x, int y) -> size_t { x = x < 0 ? 0 : x >= w ? w - 1 : x; y = y < 0 ? 0 : y >= h ? h - 1 : y; return (size_t)y * w + x; };
    auto D = [&](size_t a, size_t b) { return xz_d(cv[a], cv[b]); };
    for (int y = -1; y < h; y++)
        for (int x = -1; x < w; x++) {
            size_t f = ix(x, y), g = ix(x + 1, y), j = ix(x, y + 1), k = ix(x + 1, y + 1);
            if ((s[f] == s[g] && s[j] == s[k]) || (s[f] == s[j] && s[g] == s[k])) continue;
            size_t b = ix(x, y - 1), c = ix(x + 1, y - 1), e = ix(x - 1, y), hh = ix(x + 2, y), i = ix(x - 1, y + 1), l = ix(x + 2, y + 1), nn = ix(x, y + 2), oo = ix(x + 1, y + 2);
            float jg = D(i, f) + D(f, c) + D(nn, k) + D(k, hh) + 4.0f * D(j, g);
            float fk = D(e, j) + D(j, oo) + D(b, g) + D(g, l) + 4.0f * D(f, k);
            auto set = [&](int px, int py, int corner, int v) { if (px < 0 || py < 0 || px >= w || py >= h) return; uint8_t& q = bl[(size_t)py * w + px]; int sh = corner * 2; if (((q >> sh) & 3) < v) q = (uint8_t)((q & ~(3 << sh)) | (v << sh)); };
            if (jg < fk) {
                int v = DOM * jg < fk ? 2 : 1;
                if (s[f] != s[g] && s[f] != s[j]) set(x, y, 3, v);
                if (s[k] != s[j] && s[k] != s[g]) set(x + 1, y + 1, 0, v);
            } else if (fk < jg) {
                int v = DOM * fk < jg ? 2 : 1;
                if (s[j] != s[f] && s[j] != s[k]) set(x, y + 1, 1, v);
                if (s[g] != s[f] && s[g] != s[k]) set(x + 1, y, 2, v);
            }
        }
    const XzTab& T = xz_tab(n);
    o.resize((size_t)w * h * n * n);
    int W = w * n;
    static const int RX[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    auto cidx = [](int cx, int cy) { return (cy > 0 ? 2 : 0) + (cx > 0 ? 1 : 0); };   // corner sign -> TL 0, TR 1, BL 2, BR 3
    auto rows = [&](int y0, int y1) {
        uint32_t out[16];
        for (int y = y0; y < y1; y++)
            for (int x = 0; x < w; x++) {
                size_t E = (size_t)y * w + x;
                for (int q = 0; q < n * n; q++) out[q] = s[E];
                uint8_t bb = bl[E];
                if (bb)
                    for (int r = 0; r < 4; r++) {
                        int ca = RX[r][0], sa = RX[r][1];
                        auto at = [&](int dx, int dy) { return ix(x + dx * ca - dy * sa, y + dx * sa + dy * ca); };
                        auto corner = [&](int dx, int dy) { int c = cidx(dx * ca - dy * sa, dx * sa + dy * ca); return (bb >> (c * 2)) & 3; };
                        int t = corner(1, 1);
                        if (!t) continue;
                        size_t B = at(0, -1), C = at(1, -1), Dd = at(-1, 0), F = at(1, 0), G = at(-1, 1), H = at(0, 1), I = at(1, 1);
                        auto eq = [&](size_t a, size_t b2) { return D(a, b2) < EQ; };
                        bool line = true;
                        if (t < 2) {
                            if (corner(1, -1) && !eq(E, G)) line = false;
                            if (corner(-1, 1) && !eq(E, C)) line = false;
                            if (line && !eq(E, I) && eq(G, H) && eq(H, I) && eq(I, F) && eq(F, C)) line = false;
                        }
                        uint32_t px = D(E, F) <= D(E, H) ? s[F] : s[H];
                        int shape = 4;
                        if (line) {
                            float fg = D(F, G), hc = D(H, C);
                            bool shallow = STEEP * fg <= hc && s[E] != s[G] && s[Dd] != s[G];
                            bool steep = STEEP * hc <= fg && s[E] != s[C] && s[B] != s[C];
                            shape = shallow && steep ? 3 : shallow ? 1 : steep ? 2 : 0;
                        }
                        const float* al = T.a[r][shape];
                        for (int q = 0; q < n * n; q++) if (al[q] > 0.0f) out[q] = xz_mix(out[q], px, al[q]);
                    }
                for (int sy = 0; sy < n; sy++)
                    for (int sx = 0; sx < n; sx++) o[(size_t)(y * n + sy) * W + x * n + sx] = out[sy * n + sx];
            }
    };
    int nt = (int)std::min((unsigned)std::max(1, nt_max), std::max(1u, std::thread::hardware_concurrency()));
    if (w * h < 128 * 128 || nt < 2) { rows(0, h); return; }
    std::vector<std::thread> th;
    for (int k = 0; k < nt; k++) th.emplace_back(rows, h * k / nt, h * (k + 1) / nt);
    for (auto& t : th) t.join();
}
static void box_half(const std::vector<uint32_t>& s, int w, int h, std::vector<uint32_t>& o) {
    int W = w / 2, H = h / 2;
    o.resize((size_t)W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const uint32_t* p = &s[(size_t)(y * 2) * w + x * 2];
            uint32_t q[4] = {p[0], p[1], p[w], p[w + 1]}, r = 0;
            for (int c = 0; c < 32; c += 8) r |= (uint32_t)((((q[0] >> c) & 255) + ((q[1] >> c) & 255) + ((q[2] >> c) & 255) + ((q[3] >> c) & 255) + 2) / 4) << c;
            o[(size_t)y * W + x] = r;
        }
}
static GLuint glr_upload_chain(const Chain& c, GLuint reuse, int* aw, int* ah) {
    PERF(PZ_TEX);
    PERF_N(PC_TEXSUB, 1);
    GLuint t = reuse;
    if (!t) glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    int up = glr_up_of(c.aw[0], c.ah[0]), px0 = c.aw[0] * c.ah[0];
    static std::vector<uint32_t> s1, s2;
    if (up == 2) { xbrz_scale(c.px[0], c.aw[0], c.ah[0], 4, s2); box_half(s2, c.aw[0] * 4, c.ah[0] * 4, s1); }
    else if (up == 1) xbrz_scale(c.px[0], c.aw[0], c.ah[0], 2, s1);
    int lv = 0;
    if (up == 2) { glTexImage2D(GL_TEXTURE_2D, lv++, GL_RGBA8, c.aw[0] * 4, c.ah[0] * 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, s2.data()); PERF_N(PC_TEXPX, (uint64_t)px0 * 16); }
    if (up >= 1) { glTexImage2D(GL_TEXTURE_2D, lv++, GL_RGBA8, c.aw[0] * 2, c.ah[0] * 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, s1.data()); PERF_N(PC_TEXPX, (uint64_t)px0 * 4); }
    for (int i = 0; i < c.nn; i++) {
        glTexImage2D(GL_TEXTURE_2D, lv + i, GL_RGBA8, c.aw[i], c.ah[i], 0, GL_RGBA, GL_UNSIGNED_BYTE, c.px[i].data());
        PERF_N(PC_TEXPX, (uint64_t)c.aw[i] * c.ah[i]);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, lv + c.nn - 1);
    { static int logs = 0; if (up && logs < 3) { logs++; printf("gl: filtro de texturas %dx en %dx%d\n", 1 << up, c.aw[0], c.ah[0]); } }
    *aw = c.aw[0]; *ah = c.ah[0];
    return t;
}

static void glr_tex_free(unsigned t) { GLuint g = t; if (g) glDeleteTextures(1, &g); }

// first use of a texture: the original level goes up at once, mipmaps and the filtered copies are made on a worker
// and swapped in by the GL thread at the next flush (same texture name, so queued states keep working)
struct TexJob { int slot; uint32_t gen; int up, mm, nn; int aw[12], ah[12]; std::vector<uint32_t> px[12], s1, s2; double ms; };
static std::mutex& tj_mx = *(new std::mutex());
static std::condition_variable& tj_cv = *(new std::condition_variable());
static std::deque<TexJob*>& tj_in = *(new std::deque<TexJob*>());
static std::vector<TexJob*>& tj_out = *(new std::vector<TexJob*>());
static std::atomic<int> tj_pending{0};
static int tj_batch_n = 0, tj_batch_max = 0; static double tj_batch_ms = 0, tj_batch_t0 = 0;
static double tj_now() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static void tj_worker() {
    int nt = (int)std::max(1u, std::thread::hardware_concurrency());
    nt = nt > 3 ? nt - 2 : 1;
    for (;;) {
        TexJob* j;
        { std::unique_lock<std::mutex> lk(tj_mx); tj_cv.wait(lk, [] { return !tj_in.empty(); }); j = tj_in.front(); tj_in.pop_front(); }
        double t0 = tj_now();
        if (cache_gen[j->slot].load() == j->gen) {
            j->nn = j->mm ? chain_mips(j->px, j->aw, j->ah, j->mm) : 1;
            if (j->up == 2) { xbrz_scale(j->px[0], j->aw[0], j->ah[0], 4, j->s2, nt); box_half(j->s2, j->aw[0] * 4, j->ah[0] * 4, j->s1); }
            else if (j->up == 1) xbrz_scale(j->px[0], j->aw[0], j->ah[0], 2, j->s1, nt);
        }
        j->ms = tj_now() - t0;
        std::lock_guard<std::mutex> g(tj_mx);
        tj_out.push_back(j);
    }
}
static void glr_upload_async(Chain& c, int slot) {
    PERF(PZ_TEX);
    PERF_N(PC_TEXSUB, 1);
    GLuint t = c.gl;
    if (!t) glGenTextures(1, &t);
    c.gl = t;
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, c.aw[0], c.ah[0], 0, GL_RGBA, GL_UNSIGNED_BYTE, c.px[0].data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    PERF_N(PC_TEXPX, (uint64_t)c.aw[0] * c.ah[0]);
    c.glw = c.aw[0]; c.glh = c.ah[0];
    int up = glr_up_of(c.aw[0], c.ah[0]), mm = glr_mips();
    c.bytes = (size_t)c.aw[0] * c.ah[0] * 4 * (mm ? 4 : 3) / 3 + (size_t)c.aw[0] * c.ah[0] * 4 * (up == 2 ? 20 : up == 1 ? 4 : 0);
    if (!up && !mm) return;
    static bool started = false;
    if (!started) { started = true; std::thread(tj_worker).detach(); printf("tex: mipmaps y filtro en segundo plano activos\n"); }
    TexJob* j = new TexJob();
    j->slot = slot; j->gen = cache_gen[slot].load(); j->up = up; j->mm = mm; j->nn = 1;
    j->aw[0] = c.aw[0]; j->ah[0] = c.ah[0]; j->px[0] = c.px[0];
    if (!tj_batch_n && !tj_pending.load()) tj_batch_t0 = tj_now();
    tj_pending.fetch_add(1);
    { std::lock_guard<std::mutex> g(tj_mx); tj_in.push_back(j); tj_batch_max = std::max(tj_batch_max, (int)tj_in.size()); }
    tj_cv.notify_one();
}
static void glr_tex_apply() {
    if (!tj_pending.load(std::memory_order_relaxed)) return;
    std::vector<TexJob*> v;
    { std::lock_guard<std::mutex> g(tj_mx); v.swap(tj_out); }
    uint64_t budget = 6u << 20, done_px = 0;
    size_t k = 0;
    for (; k < v.size() && done_px < budget; k++) {
        TexJob* j = v[k];
        tj_pending.fetch_sub(1);
        tj_batch_n++; tj_batch_ms += j->ms;
        Chain& c = cache[j->slot];
        if (cache_gen[j->slot].load() == j->gen && c.nn && c.gl && c.aw[0] == j->aw[0] && c.ah[0] == j->ah[0]) {
            for (int i = 1; i < j->nn; i++) { c.px[i].swap(j->px[i]); c.aw[i] = j->aw[i]; c.ah[i] = j->ah[i]; }
            c.nn = j->nn;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, c.gl);
            int lv = 0, w0 = c.aw[0], h0 = c.ah[0];
            if (j->up == 2) { glTexImage2D(GL_TEXTURE_2D, lv++, GL_RGBA8, w0 * 4, h0 * 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, j->s2.data()); done_px += (uint64_t)w0 * h0 * 16; }
            if (j->up >= 1) { glTexImage2D(GL_TEXTURE_2D, lv++, GL_RGBA8, w0 * 2, h0 * 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, j->s1.data()); done_px += (uint64_t)w0 * h0 * 4; }
            for (int i = 0; i < c.nn; i++) { glTexImage2D(GL_TEXTURE_2D, lv + i, GL_RGBA8, c.aw[i], c.ah[i], 0, GL_RGBA, GL_UNSIGNED_BYTE, c.px[i].data()); done_px += (uint64_t)c.aw[i] * c.ah[i]; }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, lv + c.nn - 1);
            { static int logs = 0; if (j->up && logs < 3) { logs++; printf("gl: filtro de texturas %dx en %dx%d\n", 1 << j->up, w0, h0); } }
        }
        delete j;
    }
    if (k < v.size()) { std::lock_guard<std::mutex> g(tj_mx); tj_out.insert(tj_out.begin(), v.begin() + k, v.end()); }
    if (!tj_pending.load() && tj_batch_n) {
        static int logs = 0;
        if (logs < 60) { logs++; printf("tex: segundo plano listo, %d texturas en %.0f ms (trabajo %.0f ms, cola max %d), cache %d entradas %.0f MB\n", tj_batch_n, tj_now() - tj_batch_t0, tj_batch_ms, tj_batch_max, (int)cache_map.size(), cache_bytes / 1048576.0); }
        tj_batch_n = 0; tj_batch_ms = 0; tj_batch_max = 0;
    }
}

// live palette as a 256 by 1 texture for targets read with CLUT
static GLuint glr_pal_tex = 0;
static uint32_t glr_pal_seq = 0xFFFFFFFFu;

static GLuint glr_palette() {
    uint32_t px[256];
    for (int i = 0; i < 256; i++) px[i] = clut_entry(i);
    uint32_t h = 2166136261u;
    for (int i = 0; i < 256; i++) h = (h ^ px[i]) * 16777619u;
    if (glr_pal_tex && h == glr_pal_seq) return glr_pal_tex;
    glr_pal_seq = h;
    PERF(PZ_TEX);
    PERF_N(PC_TEXSUB, 1);
    PERF_N(PC_TEXPX, 256);
    if (!glr_pal_tex) {
        glGenTextures(1, &glr_pal_tex);
        glBindTexture(GL_TEXTURE_2D, glr_pal_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, glr_pal_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    return glr_pal_tex;
}

static GLuint glr_copy = 0;
static int glr_copy_w = 0, glr_copy_h = 0;

// OpenGL forbids sampling the texture bound to the current target: it is copied first
static int glr_copy_part_v = -1;
static int glr_copy_part() {   // TTT_COPY_PART=0: whole-target copies as before (Delete toggles live)
    if (glr_copy_part_v < 0) { const char* e = getenv("TTT_COPY_PART"); glr_copy_part_v = e && *e ? atoi(e) : 1; }
    return glr_copy_part_v;
}
static GLuint glr_copy_fb(DestGL* d) {
    GqZone gqz_(GQ_COPY);
    PERF_SUB("copia de pantalla");
    if (!glr_copy) {
        glGenTextures(1, &glr_copy);
        glBindTexture(GL_TEXTURE_2D, glr_copy);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    if (d->mfbo) { glr_resolve(d); glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo); }
    glBindTexture(GL_TEXTURE_2D, glr_copy);
    if (glr_copy_w != d->w || glr_copy_h != d->h) {
        uint64_t t0 = st_us();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d->w, d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glr_copy_w = d->w; glr_copy_h = d->h;
        if (t0) psp_stall_note(4, st_us() - t0, (uint32_t)d->w * d->h);
    }
    int cw = d->w, ch = d->h, ke = scale_int();
    if (glr_copy_part() && d->w >= 480 * ke) { cw = std::min(cw, 480 * ke); ch = std::min(ch, 272 * ke); }   // the displayed part: rows below 272 are never shown
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, cw, ch);
    if (d->mfbo) glBindFramebuffer(GL_READ_FRAMEBUFFER, glr_current ? dfbo(glr_current) : d->mfbo);
    glr_copies++;
    glr_copy_px += (long long)cw * ch;
    return glr_copy;
}
// ABSDIFF reads the destination only under its own pixels: the copy waits for the batch and covers its box
static bool glr_pend_abs = false, glr_pend_self = false;   // self: the texture is the target itself, copied over the batch uv box
static void glr_copy_box(DestGL* d, float x0, float y0, float x1, float y1) {
    GqZone gqz_(GQ_COPY);
    int ke = scale_int(), vw = d->w >= 480 * ke ? std::min(d->w, 480 * ke) : d->w, vh = d->w >= 480 * ke ? std::min(d->h, 272 * ke) : d->h;
    int ix0 = std::max(0, (int)std::floor(x0) - 2), iy0 = std::max(0, (int)std::floor(y0) - 2), ix1 = std::min(vw, (int)std::ceil(x1) + 2), iy1 = std::min(vh, (int)std::ceil(y1) + 2);
    if (ix1 <= ix0 || iy1 <= iy0) return;
    if (!glr_copy || glr_copy_w != d->w || glr_copy_h != d->h) { glActiveTexture((GL_TEXTURE0 + 7)); glr_copy_fb(d); glActiveTexture(GL_TEXTURE0); return; }
    if (d->mfbo) {
        int bx[4] = {ix0, iy0, ix1, std::min(iy1, d->mh)};
        if (rb_on()) glr_resolve(d, bx); else glr_resolve(d);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo);
    }
    glActiveTexture((GL_TEXTURE0 + 7));
    glBindTexture(GL_TEXTURE_2D, glr_copy);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, ix0, iy0, ix0, iy0, ix1 - ix0, iy1 - iy0);
    glActiveTexture(GL_TEXTURE0);
    if (d->mfbo) glBindFramebuffer(GL_READ_FRAMEBUFFER, glr_current ? dfbo(glr_current) : d->mfbo);
    glr_copies++;
    glr_copy_px += (long long)(ix1 - ix0) * (iy1 - iy0);
}

static const char* GLR_VS =
    "#version 330 core\n"
    "layout(location=0) in vec4 pos;\n"
    "layout(location=1) in vec2 uv;\n"
    "layout(location=2) in vec4 col;\n"
    "layout(location=3) in vec2 toon;\n"
    "layout(location=4) in vec4 tint;\n"
    "layout(location=5) in vec4 ext;\n"
    "layout(location=6) in vec4 tp;\n"
    "layout(location=7) in vec4 wat;\n"
    "layout(location=8) in vec4 nrm;\n"
    "layout(location=9) in vec4 lgt;\n"
    "layout(location=10) in vec4 vps;\n"
    "out vec2 vuv; out vec4 vcol; out vec2 vtoon; out vec4 vtint; out vec4 vext; out vec4 vtp; out vec4 vwat; out vec4 vn; out vec4 vl; out vec4 vpo;\n"
    "void main(){ gl_Position = pos; vuv = uv; vcol = col; vtoon = toon; vtint = tint; vext = ext; vtp = tp; vwat = wat; vn = nrm; vl = lgt; vpo = vps; }\n";

static const char* GLR_FS =
    "#version 330 core\n"
    "in vec2 vuv; in vec4 vcol; in vec2 vtoon; in vec4 vtint; in vec4 vext; in vec4 vtp; in vec4 vwat; in vec4 vn; in vec4 vl; in vec4 vpo;\n"
    "uniform vec4 toon_px; uniform vec4 fx_lp[4]; uniform vec4 fx_lc[4];\n"
    "uniform vec4 water_c; uniform vec4 water_w; uniform vec4 wet_k; uniform vec4 chg_k;\n"
    "float wnz(vec2 q){ vec2 i = floor(q), f = fract(q); f = f*f*(3.0 - 2.0*f); vec4 h = fract(sin(vec4(dot(i, vec2(127.1, 311.7)), dot(i + vec2(1.0, 0.0), vec2(127.1, 311.7)), dot(i + vec2(0.0, 1.0), vec2(127.1, 311.7)), dot(i + vec2(1.0, 1.0), vec2(127.1, 311.7))))*43758.5453); return mix(mix(h.x, h.y, f.x), mix(h.z, h.w, f.x), f.y); }\n"
    "uniform int toon_mode; uniform float toon_edge; uniform vec4 toon_k; uniform vec4 toon_k2; uniform vec4 toon_k3; uniform vec4 toon_k4; uniform float toon_tphl; uniform int toon_tpx; uniform vec4 toon_bolt;\n"
    "uniform sampler2D tex0;\n"
    "uniform sampler2D pal0;\n"
    "uniform sampler2D ramp0;\n"
    "uniform int usar_tex;\n"
    "uniform int tf_modo, tf_alfa, tf_dbl;\n"
    "uniform int at_on, at_fn, at_ref, at_msk;\n"
    "uniform int st_on; uniform float st_ref;\n"
    "uniform int alfa_doble;\n"
    "uniform int rt_fmt, clut_desp, clut_masc;\n"
    "uniform sampler2D dst0;\n"
    "uniform int mezcla_prog;\n"
    "uniform int ver_lod;\n"
    "uniform int pm_on, pm_rgb;\n"
    "uniform float soft_r;\n"
    "layout(location=0, index=0) out vec4 salida;\n"
    "layout(location=0, index=1) out vec4 salida_mezcla;\n"
    "uint empaquetar(vec4 c){\n"
    "  uint r=uint(c.r*255.0+0.5), g=uint(c.g*255.0+0.5), b=uint(c.b*255.0+0.5), a=uint(c.a*255.0+0.5);\n"
    "  if(rt_fmt==3) return r|(g<<8)|(b<<16)|(a<<24);\n"
    "  if(rt_fmt==0) return (r>>3)|((g>>2)<<5)|((b>>3)<<11);\n"
    "  if(rt_fmt==1) return (r>>3)|((g>>3)<<5)|((b>>3)<<10)|((a>=128u)?32768u:0u);\n"
    "  return (r>>4)|((g>>4)<<4)|((b>>4)<<8)|((a>>4)<<12);\n"
    "}\n"
    "const vec2 SOFT[12] = vec2[12](vec2(-0.326,-0.406),vec2(-0.840,-0.074),vec2(-0.696,0.457),vec2(-0.203,0.621),vec2(0.962,-0.195),vec2(0.473,-0.480),\n"
    "  vec2(0.519,0.767),vec2(0.185,-0.893),vec2(0.507,0.064),vec2(0.896,0.412),vec2(-0.322,-0.933),vec2(-0.792,-0.598));\n"
    "uniform int soft_bl;\n"
    "vec4 clut_at(ivec2 p, ivec2 tam);\n"
    "vec4 clut_bl(vec2 q, ivec2 tam){ vec2 f = q - 0.5; ivec2 i = ivec2(floor(f)); vec2 t = f - floor(f);\n"
    "  return mix(mix(clut_at(i, tam), clut_at(i + ivec2(1,0), tam), t.x), mix(clut_at(i + ivec2(0,1), tam), clut_at(i + ivec2(1,1), tam), t.x), t.y); }\n"
    "vec4 clut_at(ivec2 p, ivec2 tam){ uint w = empaquetar(texelFetch(tex0,clamp(p,ivec2(0),tam-1),0)); return texelFetch(pal0, ivec2(int(((w >> uint(clut_desp)) & uint(clut_masc)) & 255u),0), 0); }\n"
    "void main(){\n"
    "  if(ver_lod == 1){\n"
    "    if(usar_tex == 0){ salida = vec4(0.1,0.1,0.1,1.0); salida_mezcla = vec4(1.0); return; }\n"
    "    vec2 ts = vec2(textureSize(tex0,0));\n"
    "    vec2 ax = dFdx(vuv*ts), ay = dFdy(vuv*ts);\n"
    "    float lod = log2(max(max(length(ax), length(ay)), 1e-6));\n"
    "    vec3 k;\n"
    "    if(lod < 0.0) k = vec3(0.0,0.0,1.0);\n"
    "    else if(lod < 1.0) k = vec3(0.0,1.0,1.0);\n"
    "    else if(lod < 2.0) k = vec3(0.0,1.0,0.0);\n"
    "    else if(lod < 3.0) k = vec3(1.0,1.0,0.0);\n"
    "    else if(lod < 4.0) k = vec3(1.0,0.5,0.0);\n"
    "    else if(lod < 6.0) k = vec3(1.0,0.0,0.0);\n"
    "    else k = vec3(1.0,1.0,1.0);\n"
    "    salida = vec4(k, 1.0); salida_mezcla = vec4(1.0); return;\n"
    "  }\n"
    "  vec4 col = vcol;\n"
    "  if(usar_tex != 0){\n"
    "    vec4 tc;\n"
    "    if(usar_tex == 2){\n"
    "      ivec2 tam = textureSize(tex0,0);\n"
    "      vec2 q = vuv*vec2(tam);\n"
    "      if(soft_r > 0.0 && soft_bl == 1){ tc = clut_bl(q, tam); for(int i=0;i<12;i++) tc += clut_bl(q + SOFT[i]*soft_r, tam); tc /= 13.0; }\n"
    "      else { tc = clut_at(ivec2(q), tam); if(soft_r > 0.0){ for(int i=0;i<12;i++) tc += clut_at(ivec2(q + SOFT[i]*soft_r), tam); tc /= 13.0; } }\n"
    "    } else if(soft_r > 0.0){ vec2 ts = vec2(textureSize(tex0,0)); tc = texture(tex0, vuv); for(int i=0;i<12;i++) tc += texture(tex0, vuv + SOFT[i]*soft_r/ts); tc /= 13.0; }\n"
    "    else tc = texture(tex0, vuv);\n"
    "    if(tf_modo == 0) { col.rgb = vcol.rgb*tc.rgb; col.a = (tf_alfa==1)? vcol.a*tc.a : vcol.a; }\n"
    "    else if(tf_modo == 3) { col.rgb = tc.rgb; col.a = (tf_alfa==1)? tc.a : vcol.a; }\n"
    "    else if(tf_modo == 4) { col.rgb = vcol.rgb+tc.rgb; col.a = (tf_alfa==1)? vcol.a*tc.a : vcol.a; }\n"
    "    else { col.rgb = mix(vcol.rgb, tc.rgb, tc.a); col.a = vcol.a; }\n"
    "    if(tf_dbl == 1) col.rgb *= 2.0;\n"
    "  }\n"
    "  if(vtoon.x < -2.0 && vtoon.y > 0.5) col.rgb += col.rgb * (vec3(0.85,0.93,1.0) * toon_bolt.w + vext.rgb * 1.5);\n"
    "  if(vpo.w > 0.5 && toon_px.z > 0.5){\n"
    "    vec3 fn = cross(dFdx(vpo.xyz), dFdy(vpo.xyz)); float fl = dot(fn,fn); fn = fl > 1e-20 ? fn*inversesqrt(fl) : vec3(0.0); if(dot(fn, vpo.xyz) > 0.0) fn = -fn;\n"
    "    vec3 kl = vec3(0.0);\n"
    "    for(int i=0;i<4;i++){ if(fx_lc[i].w <= 0.0) continue; vec3 d = fx_lp[i].xyz - vpo.xyz; float r2 = dot(d,d), rr = fx_lp[i].w*fx_lp[i].w;\n"
    "      float wr = 0.35 + 0.65*max(dot(fn, d*inversesqrt(max(r2,1e-12))), 0.0); kl += fx_lc[i].rgb * (fx_lc[i].w * wr * rr/(rr + r2)); }\n"
    "    vec3 kc = col.rgb + min(kl*toon_px.w, vec3(1.0))*(0.35 + 0.65*col.rgb);\n"
    "    vec3 ov = max(kc - 0.75, 0.0); col.rgb = min(kc, vec3(0.75)) + ov/(1.0 + ov*4.0);\n"
    "  }\n"
    "  if(toon_mode > 0 && vtoon.x > -2.0){\n"
    "    vec3 b = col.rgb;\n"
    "    float mx = max(max(b.r,b.g),b.b), sat = mx > 0.001 ? (mx - min(min(b.r,b.g),b.b))/mx : 0.0;\n"
    "    float skin = (b.r > 0.45 && b.r >= b.g && b.g >= b.b && b.r - b.b > 0.08 && b.r - b.b < 0.55 && sat < 0.6) ? toon_k.w : 0.0;\n"
    "    bool face = vtoon.y > 2.0; float fc = face ? vtoon.y - 4.0 : vtoon.y;\n"
    "    float nl = vtoon.x, nb = vtoon.x;\n"
    "    if(toon_px.x > 0.5 && dot(vn.xyz,vn.xyz) > 0.01){ vec3 N = normalize(vn.xyz), V = normalize(-vpo.xyz), L = normalize(vl.xyz + toon_px.y*V); nl = dot(N,L); fc = dot(N,V); nb = dot(N, normalize(vl.xyz)); }\n"
    "    float fw = max(fwidth(nl), toon_k2.y) + skin*0.12;\n"
    "    float lit = smoothstep(toon_edge - fw, toon_edge + fw, nl), hi = smoothstep(toon_k2.z - fw, toon_k2.z + fw, nl);\n"
    "    if(face) lit = 1.0 - (1.0 - lit)*toon_k2.x;\n"
    "    vec3 sh = b * mix(vec3(1.0), b, toon_k.x) * 0.85 * vec3(0.95,0.93,1.0);\n"
    "    sh = mix(sh, b, skin*0.35);\n"
    "    vec3 tc = vtint.rgb; float tmx = max(max(tc.r,tc.g),tc.b);\n"
    "    vec3 tn = tmx > 0.02 ? tc/tmx : vec3(1.0);\n"
    "    float a8 = floor(vtint.a*255.0+0.5), phase = mod(a8, 2.0), ai = a8/255.0;\n"
    "    float ph = clamp(toon_k.y*phase + toon_k.z*ai, 0.0, 1.0);\n"
    "    vec3 tw = mix(tn, tn*vec3(1.0,0.68,0.32), toon_k3.x);\n"
    "    sh = mix(sh, sh*mix(vec3(1.0), tw, 0.6) + tw*ph*0.06, ph);\n"
    "    vec3 li = mix(b, b*tw*1.12, ph*0.55*(1.0 - 0.8*sat));\n"
    "    vec3 hl = mix(b*0.3*(1.0 - 0.4*sat), tn*0.3, ph) * toon_k2.w;\n"
    "    float tpr = floor(vtp.x*255.0 + 0.5), tpf = floor(vtp.y*255.0 + 0.5);\n"
    "    float rimk = 1.0, glowk = 1.0;\n"
    "    if(toon_mode == 2){\n"
    "      float u = clamp(0.5 + 0.5*nb, 0.0, 1.0);\n"
    "      float tpm = floor(vtp.z*255.0 + 0.5);\n"
    "      if(tpr > 0.5){ vec3 rp = texture(ramp0, vec2(u, (tpr + 0.5)/256.0)).rgb; col.rgb = tpm > 2.5 ? b - rp : tpm > 1.5 ? b*rp*2.0 : b + rp; }\n"
    "      else col.rgb = mix(b, b*(0.55 + 0.4*b)*(1.0 - 0.15*step(u, 0.125)), step(u, 0.3125));\n"
    "      col.rgb += vext.rgb*(0.35 + 0.65*b);\n"
    "      rimk = 0.0; glowk = 0.0;\n"
    "    } else if(tpr > 0.5){\n"
    "      vec3 rp = texture(ramp0, vec2(clamp(0.5 + 0.5*nl, 0.0, 1.0), (tpr + 0.5)/256.0)).rgb;\n"
    "      vec3 tb = floor(vtp.z*255.0 + 0.5) > 1.5 ? b*rp*2.0 : b + rp;\n"
    "      if((toon_tpx & 1) == 0) tb = mix(tb, tb*tw*1.12, ph*0.55*(1.0 - 0.8*sat));\n"
    "      bool nolight = mod(floor(tpf/2.0), 2.0) > 0.5;\n"
    "      vec3 ml = (toon_tpx & 4) != 0 ? vec3(1.0) : mix(toon_k4.rgb, toon_k3.yzw, lit);\n"
    "      vec3 ex = (toon_tpx & 2) != 0 ? vec3(0.0) : vext.rgb*mix(vec3(1.0), b*2.0, vext.a);\n"
    "      col.rgb = nolight ? tb : tb*ml + hl*hi*toon_k4.w*toon_k3.yzw*toon_tphl + ex;\n"
    "      rimk = (mod(tpf, 2.0) > 0.5 || (toon_tpx & 8) != 0) ? 0.0 : 1.0;\n"
    "      if((toon_tpx & 8) != 0) glowk = 0.0;\n"
    "    } else\n"
    "    col.rgb = mix(sh*toon_k4.rgb, li*toon_k3.yzw, lit) + hl*hi*toon_k4.w*toon_k3.yzw + vext.rgb*mix(vec3(1.0), b*2.0, vext.a);\n"
    "    col.rgb += b * toon_bolt.rgb * mix(0.45, 1.0, lit);\n"
    "    float rim = smoothstep(0.72, 0.9, 1.0 - clamp(fc, 0.0, 1.0)) * rimk;\n"
    "    col.rgb += rim * (0.12*lit + 0.18*ph) * mix(vec3(1.0,0.97,0.92), tn, clamp(ph*1.5, 0.0, 1.0));\n"
    "    col.rgb += tn * ai * toon_k.z * 0.12 * glowk;\n"
    "    if(vwat.z > 0.0 && wet_k.x > 0.5){ float w = vwat.z; vec3 dk = col.rgb*mix(vec3(1.0), col.rgb*1.6, 0.35);   // Wet surfaces: darker, richer cloth and skin, glossy film, sharp highlight, water running down\n"
    "      col.rgb = mix(col.rgb, dk*0.82, 0.55*w);\n"
    "      col.rgb += vec3(0.72, 0.8, 0.9)*pow(1.0 - clamp(fc, 0.0, 1.0), 4.0)*0.32*w;\n"
    "      if(dot(vn.xyz, vn.xyz) > 0.01){ vec3 N = normalize(vn.xyz), V = normalize(-vpo.xyz), H = normalize(normalize(vl.xyz) + V); float sp = smoothstep(0.86, 0.93, pow(max(dot(N, H), 0.0), 40.0)); col.rgb += vec3(0.95, 0.97, 1.0)*sp*0.45*w*(0.4 + 0.6*lit); }\n"
    "      float rv = smoothstep(0.74, 0.92, wnz(vec2(vpo.x*2.2, vpo.y*0.45 + wet_k.y*2.5)))*smoothstep(0.3, 0.8, wnz(vpo.xy*0.7 + 9.0)); col.rgb += vec3(0.85, 0.9, 1.0)*rv*0.3*w*wet_k.z; }\n"
    "    else if(vwat.z > 0.0){ col.rgb *= 1.0 - water_w.x*vwat.z; col.rgb += vec3(0.85,0.92,1.0) * max(hi, rim) * water_w.y * vwat.z; }\n"
    "  }\n"
    "  if(vwat.x > -9000.0){ float sub = smoothstep(-water_w.z, water_w.z, vwat.x); col.rgb = mix(col.rgb, col.rgb*water_c.rgb, max(vwat.y, sub*water_c.w)); }\n"
    "  if(chg_k.x > 0.0){ float l = dot(col.rgb, vec3(0.3, 0.59, 0.11));   // SZ charge: night sky, ground a little dimmer and warmer, fighters lit\n"
    "    if(vpo.w > 0.2) col.rgb *= 1.0 + 0.08*chg_k.x;\n"
    "    else if(toon_mode != 0 && vtoon.x > -2.0) col.rgb *= 1.0 + 0.12*chg_k.x; }\n"
    "  col.rgb = clamp(col.rgb, 0.0, 1.0);\n"
    "  if(at_on == 1){\n"
    "    int a = int(col.a*255.0+0.5) & at_msk; int rf = at_ref & at_msk; bool p;\n"
    "    if(at_fn==0) p=false; else if(at_fn==1) p=true; else if(at_fn==2) p=(a==rf);\n"
    "    else if(at_fn==3) p=(a!=rf); else if(at_fn==4) p=(a<rf); else if(at_fn==5) p=(a<=rf);\n"
    "    else if(at_fn==6) p=(a>rf); else p=(a>=rf);\n"
    "    if(!p) discard;\n"
    "  }\n"
    "  if(mezcla_prog == 1){\n"
    "    vec3 d = texelFetch(dst0, ivec2(gl_FragCoord.xy), 0).rgb;\n"
    "    col.rgb = abs(col.rgb - d);\n"
    "  }\n"
    "  if(pm_on == 1){\n"
    "    uint m = uint(pm_rgb);\n"
    "    uvec3 mm = uvec3(m & 255u, (m >> 8) & 255u, (m >> 16) & 255u);\n"
    "    uvec3 s = uvec3(col.rgb*255.0+0.5);\n"
    "    uvec3 d = uvec3(texelFetch(dst0, ivec2(gl_FragCoord.xy), 0).rgb*255.0+0.5);\n"
    "    col.rgb = vec3((s & ~mm) | (d & mm)) / 255.0;\n"
    "  }\n"
    "  float oa = col.a;\n"
    "  if(alfa_doble == 1) oa = min(col.a*2.0, 1.0);\n"
    "  salida_mezcla = vec4(oa);\n"
    "  if(st_on == 1) oa = st_ref;\n"
    "  salida = vec4(col.rgb, oa);\n"
    "}\n";

// light program for plain draws (texture * colour, alpha test, stencil alpha): no toon, ki light, water, CLUT target, ABSDIFF or pixel mask
static const char* GLR_FS_LITE =
    "#version 330 core\n"
    "in vec2 vuv; in vec4 vcol;\n"
    "uniform sampler2D tex0; uniform int usar_tex; uniform int tf_modo, tf_alfa, tf_dbl; uniform int at_on, at_fn, at_ref, at_msk; uniform int st_on; uniform float st_ref; uniform int alfa_doble;\n"
    "layout(location=0, index=0) out vec4 salida;\n"
    "layout(location=0, index=1) out vec4 salida_mezcla;\n"
    "void main(){\n"
    "  vec4 col = vcol;\n"
    "  if(usar_tex != 0){ vec4 tc = texture(tex0, vuv);\n"
    "    if(tf_modo == 0) { col.rgb = vcol.rgb*tc.rgb; col.a = (tf_alfa==1)? vcol.a*tc.a : vcol.a; }\n"
    "    else if(tf_modo == 3) { col.rgb = tc.rgb; col.a = (tf_alfa==1)? tc.a : vcol.a; }\n"
    "    else if(tf_modo == 4) { col.rgb = vcol.rgb+tc.rgb; col.a = (tf_alfa==1)? vcol.a*tc.a : vcol.a; }\n"
    "    else { col.rgb = mix(vcol.rgb, tc.rgb, tc.a); col.a = vcol.a; }\n"
    "    if(tf_dbl == 1) col.rgb *= 2.0; }\n"
    "  col.rgb = clamp(col.rgb, 0.0, 1.0);\n"
    "  if(at_on == 1){\n"
    "    int a = int(col.a*255.0+0.5) & at_msk; int rf = at_ref & at_msk; bool p;\n"
    "    if(at_fn==0) p=false; else if(at_fn==1) p=true; else if(at_fn==2) p=(a==rf);\n"
    "    else if(at_fn==3) p=(a!=rf); else if(at_fn==4) p=(a<rf); else if(at_fn==5) p=(a<=rf);\n"
    "    else if(at_fn==6) p=(a>rf); else p=(a>=rf);\n"
    "    if(!p) discard;\n"
    "  }\n"
    "  float oa = col.a;\n"
    "  if(alfa_doble == 1) oa = min(col.a*2.0, 1.0);\n"
    "  salida_mezcla = vec4(oa);\n"
    "  if(st_on == 1) oa = st_ref;\n"
    "  salida = vec4(col.rgb, oa);\n"
    "}\n";
typedef void(__stdcall* T_pu1i)(GLuint, GLint, GLint);
typedef void(__stdcall* T_pu1f)(GLuint, GLint, GLfloat);
static T_pu1i gl_pu1i = nullptr; static T_pu1f gl_pu1f = nullptr;
static GLuint glr_lite = 0;
static GLint gll_use, gll_mode, gll_alpha, gll_dbl, gll_aton, gll_atfn, gll_atref, gll_atmsk, gll_ston, gll_stref, gll_adouble;
static bool glr_st_lite = false;   // the current state has no CLUT target, soft taps, ABSDIFF, pixel mask or LOD view
static float glr_bolt_w = 0.0f, glr_chg_x = 0.0f;
static long glr_lite_n = 0, glr_draw_n = 0, glr_clr_n = 0; static double glr_a0_px = 0;
static int glr_clr_var = -1, glr_a0_var = -1;
static bool glr_clr_on() { if (glr_clr_var < 0) { const char* e = getenv("TTT_GL_CLEAR"); glr_clr_var = e && *e ? atoi(e) : 1; } return glr_clr_var > 0; }   // clear-mode sprites covering the scissor become glClear
static bool glr_a0_on() { if (glr_a0_var < 0) { const char* e = getenv("TTT_SKIP_A0"); glr_a0_var = e && *e ? atoi(e) : 1; } return glr_a0_var > 0; }   // alpha-blended 2D draws with alpha 0 everywhere are not drawn
static double gq_px[16]; static long gq_dn[16];   // fragments (Mpx, before depth/alpha test) and draws per zone
static uint32_t glr_st_tdir = 0; static int glr_st_tw = 0, glr_st_th = 0, glr_st_bl = 0;
struct BigDraw { uint32_t dir; int w, h, bl; double px; long n; };
static BigDraw gq_big[64]; static int gq_big_n = 0;
static int glr_lite_var = -1;
static bool glr_lite_on() {
    if (glr_lite_var < 0) { const char* e = getenv("TTT_GPU_LITE"); glr_lite_var = e && *e ? atoi(e) : 1; }
    return glr_lite_var > 0 && glr_lite != 0;
}
static inline void gll_1i(GLint loc, int v) { if (glr_lite && loc >= 0) gl_pu1i(glr_lite, loc, v); }
static inline void gll_1f(GLint loc, float v) { if (glr_lite && loc >= 0) gl_pu1f(glr_lite, loc, v); }

struct VtxGL { float x, y, z, w, u, v; uint32_t col; float tl, tf; uint32_t tint, ext, tp; float wd, wc, wet, wpad; uint32_t nrm, lgt; float px, py, pz, pf; };

static void gq_area(int cat, const VtxGL* v, const uint32_t* idx, size_t n) {
    if (!glr_current || gq_ok <= 0 || !gq_samp) return;
    double W = glr_current->w * 0.5, H = (glr_current->mfbo ? std::min(glr_current->h, glr_current->mh) : glr_current->h) * 0.5, a = 0;
    for (size_t i = 0; i + 2 < n; i += 3) {
        const VtxGL* t[3] = {&v[idx ? idx[i] : i], &v[idx ? idx[i + 1] : i + 1], &v[idx ? idx[i + 2] : i + 2]};
        if (!(t[0]->w > 1e-4f && t[1]->w > 1e-4f && t[2]->w > 1e-4f)) continue;
        auto cx = [&](const VtxGL* q) { return std::max(-W, std::min(W, (double)q->x / q->w * W)); };
        auto cy = [&](const VtxGL* q) { return std::max(-H, std::min(H, (double)q->y / q->w * H)); };
        double x0 = cx(t[0]), y0 = cy(t[0]), x1 = cx(t[1]), y1 = cy(t[1]), x2 = cx(t[2]), y2 = cy(t[2]);
        a += std::fabs((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)) * 0.5;
    }
    gq_px[cat] += a; gq_dn[cat]++;
    if (cat != GQ_2D) return;
    for (int i = 0; i < gq_big_n; i++) if (gq_big[i].dir == glr_st_tdir && gq_big[i].w == glr_st_tw && gq_big[i].h == glr_st_th && gq_big[i].bl == glr_st_bl) { gq_big[i].px += a; gq_big[i].n++; return; }
    if (gq_big_n < 64) gq_big[gq_big_n++] = {glr_st_tdir, glr_st_tw, glr_st_th, glr_st_bl, a, 1};
}
static bool glr_vtx_plain(const VtxGL* v, size_t n) {
    for (size_t i = 0; i < n; i++) {
        const VtxGL& g = v[i];
        if (g.tl > -2.0f || g.pf > 0.5f || (g.pf > 0.2f && glr_chg_x > 0.0f) || g.wd > -9000.0f || (g.tf > 0.5f && (glr_bolt_w != 0.0f || g.ext != 0u))) return false;
    }
    return true;
}
static GLint glu_tex0, glu_pal0, glu_ramp0, glu_use, glu_mode, glu_alpha, glu_dbl;
static GLuint glr_ramp = 0;
static GLint glu_aton, glu_atfn, glu_atref, glu_atmsk, glu_ston, glu_stref, glu_adouble;
static GLint glu_rtfmt, glu_cshift, glu_cmask, glu_dst0, glu_mprog, glu_verlod, glu_pmon, glu_pmrgb, glu_soft, glu_toon, glu_tedge, glu_tk, glu_tk2, glu_tk3, glu_tk4, glu_tphl, glu_tpx, glu_bolt, glu_wc, glu_ww, glu_wetk, glu_chg, glu_tpix, glu_fxlp, glu_fxlc, glu_softbl;

// soft fighter shadows: the 64x64 silhouette target read one bit per fighter (CLUT mask 1) is sampled 13 times (Graphics option)
static bool glr_soft() {
    static int v = 1, n = 0;
    if ((n++ & 255) == 0) { const char* e = getenv("TTT_SOFT_SHADOWS"); v = !(e && *e) || atoi(e) != 0; }
    return v != 0;
}
static std::vector<VtxGL> glr_vtx;
static void glr_pend_copy(const VtxGL* v, const uint32_t* idx, size_t n) {
    if ((!glr_pend_abs && !glr_pend_self) || !glr_current) { glr_pend_abs = glr_pend_self = false; return; }
    float W = (float)glr_current->w, Hh = (float)glr_current->h;
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f; bool all = false;
    for (size_t i = 0; i < n && !all; i++) {
        const VtxGL& g = idx ? v[idx[i]] : v[i];
        if (glr_pend_abs) {
            if (!(g.w > 1e-6f)) { all = true; break; }
            float x = (g.x / g.w + 1.0f) * 0.5f * W, y = (g.y / g.w + 1.0f) * 0.5f * Hh;
            x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
        }
        if (glr_pend_self) {
            if (!(g.u >= -0.001f && g.u <= 1.001f && g.v >= -0.001f && g.v <= 1.001f)) { all = true; break; }
            float x = g.u * W, y = g.v * Hh;
            x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y);
        }
    }
    glr_pend_abs = glr_pend_self = false;
    if (all) { x0 = y0 = 0; x1 = W; y1 = Hh; }
    glr_copy_box(glr_current, x0, y0, x1, y1);
}
// GPU vertex path (TTT_GPU_VTX): one indexed draw per primitive, vertices written once (gpu.cpp gv_prim); Job cls 2 = x0 vertex base, x1 count, y0 index base, y1 count
static bool gv_noinit = false;   // resize without zeroing: the vertex workers write every field
template <class T> struct GvAlloc : std::allocator<T> {
    template <class U> struct rebind { using other = GvAlloc<U>; };
    GvAlloc() = default; template <class U> GvAlloc(const GvAlloc<U>&) {}
    template <class U> void construct(U* p) { if (gv_noinit) ::new ((void*)p) U; else ::new ((void*)p) U(); }
    template <class U, class... A> void construct(U* p, A&&... a) { ::new ((void*)p) U(std::forward<A>(a)...); }
};
static std::vector<VtxGL, GvAlloc<VtxGL>> gv_vtx;
static std::vector<uint32_t> gv_idx;
struct GvAg { float b[4]; bool big, tn; float z0, z1; double s[5]; };   // screen box, |view z| range, index sums (n, x, y, x2, y2)
static std::vector<GvAg> gv_agv;
static float* gv_tn = nullptr; static size_t gv_tn_cap = 0;   // per triangle of a cel prim: face normal and corner weights (workers)
static void gv_tn_fit() {
    size_t need = gv_idx.capacity() / 3 + 1;
    if (gv_tn_cap >= need) return;
    float* n = new float[need * 6];
    if (gv_tn) { memcpy(n, gv_tn, std::min(gv_tn_cap, gv_idx.size() / 3 + 1) * 6 * sizeof(float)); delete[] gv_tn; }
    gv_tn = n; gv_tn_cap = need;
}
struct GvX { uint32_t key, fn; };   // cel: vertex identity hash and the model normal (BT3), per gv vertex
static std::vector<GvX, GvAlloc<GvX>> gv_x;
static GLuint gv_vao = 0, gv_vbo = 0, gv_ibo = 0;
static uint64_t gv_draws = 0;
static void glr_layout() {
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(VtxGL), (const void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VtxGL), (const void*)16);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VtxGL), (const void*)24);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(VtxGL), (const void*)28);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VtxGL), (const void*)36);
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VtxGL), (const void*)40);
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VtxGL), (const void*)44);
    glEnableVertexAttribArray(7);
    glVertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(VtxGL), (const void*)48);
    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 4, GL_BYTE, GL_TRUE, sizeof(VtxGL), (const void*)64);
    glEnableVertexAttribArray(9);
    glVertexAttribPointer(9, 4, GL_BYTE, GL_TRUE, sizeof(VtxGL), (const void*)68);
    glEnableVertexAttribArray(10);
    glVertexAttribPointer(10, 4, GL_FLOAT, GL_FALSE, sizeof(VtxGL), (const void*)72);
}
static float SH_R = 2.4f; static int SH_BL = 1; static float TF_BIAS = -0.5f;   // fighter shadow blur radius (native texels) and bilinear CLUT taps

static bool glr_init() {
    if (glr_prog) return true;
    glr_prog = gl_program(GLR_VS, GLR_FS);
    if (!glr_prog) return false;
    gl_pu1i = (T_pu1i)gl_proc("glProgramUniform1i"); gl_pu1f = (T_pu1f)gl_proc("glProgramUniform1f");
    if (gl_pu1i && gl_pu1f) glr_lite = gl_program(GLR_VS, GLR_FS_LITE);
    if (glr_lite) {
        gll_use = glGetUniformLocation(glr_lite, "usar_tex"); gll_mode = glGetUniformLocation(glr_lite, "tf_modo"); gll_alpha = glGetUniformLocation(glr_lite, "tf_alfa"); gll_dbl = glGetUniformLocation(glr_lite, "tf_dbl");
        gll_aton = glGetUniformLocation(glr_lite, "at_on"); gll_atfn = glGetUniformLocation(glr_lite, "at_fn"); gll_atref = glGetUniformLocation(glr_lite, "at_ref"); gll_atmsk = glGetUniformLocation(glr_lite, "at_msk");
        gll_ston = glGetUniformLocation(glr_lite, "st_on"); gll_stref = glGetUniformLocation(glr_lite, "st_ref"); gll_adouble = glGetUniformLocation(glr_lite, "alfa_doble");
        gl_pu1i(glr_lite, glGetUniformLocation(glr_lite, "tex0"), 0);
    }
    printf("gl: programa ligero %s\n", glr_lite ? "listo" : "no disponible (sin glProgramUniform)");
    glGenVertexArrays(1, &glr_vao);
    glGenBuffers(1, &glr_vbo);
    glBindVertexArray(glr_vao);
    glBindBuffer(GL_ARRAY_BUFFER, glr_vbo);
    glr_layout();
    glGenVertexArrays(1, &gv_vao);
    glGenBuffers(1, &gv_vbo);
    glGenBuffers(1, &gv_ibo);
    glBindVertexArray(gv_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gv_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gv_ibo);
    glr_layout();
    glBindVertexArray(glr_vao);
    glBindBuffer(GL_ARRAY_BUFFER, glr_vbo);
    glGenTextures(1, &glr_ramp);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, glr_ramp);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(glr_prog);
    glu_tex0 = glGetUniformLocation(glr_prog, "tex0");
    glu_pal0 = glGetUniformLocation(glr_prog, "pal0");
    glu_ramp0 = glGetUniformLocation(glr_prog, "ramp0");
    glu_use = glGetUniformLocation(glr_prog, "usar_tex");
    glu_mode = glGetUniformLocation(glr_prog, "tf_modo");
    glu_alpha = glGetUniformLocation(glr_prog, "tf_alfa");
    glu_dbl = glGetUniformLocation(glr_prog, "tf_dbl");
    glu_aton = glGetUniformLocation(glr_prog, "at_on");
    glu_atfn = glGetUniformLocation(glr_prog, "at_fn");
    glu_atref = glGetUniformLocation(glr_prog, "at_ref");
    glu_atmsk = glGetUniformLocation(glr_prog, "at_msk");
    glu_ston = glGetUniformLocation(glr_prog, "st_on");
    glu_stref = glGetUniformLocation(glr_prog, "st_ref");
    glu_adouble = glGetUniformLocation(glr_prog, "alfa_doble");
    glu_rtfmt = glGetUniformLocation(glr_prog, "rt_fmt");
    glu_cshift = glGetUniformLocation(glr_prog, "clut_desp");
    glu_cmask = glGetUniformLocation(glr_prog, "clut_masc");
    glu_dst0 = glGetUniformLocation(glr_prog, "dst0");
    glu_mprog = glGetUniformLocation(glr_prog, "mezcla_prog");
    glu_verlod = glGetUniformLocation(glr_prog, "ver_lod");
    glu_pmon = glGetUniformLocation(glr_prog, "pm_on");
    glu_pmrgb = glGetUniformLocation(glr_prog, "pm_rgb");
    glu_soft = glGetUniformLocation(glr_prog, "soft_r");
    glu_toon = glGetUniformLocation(glr_prog, "toon_mode");
    glu_tedge = glGetUniformLocation(glr_prog, "toon_edge");
    glu_tk = glGetUniformLocation(glr_prog, "toon_k");
    glu_tk2 = glGetUniformLocation(glr_prog, "toon_k2");
    glu_tk3 = glGetUniformLocation(glr_prog, "toon_k3");
    glu_tk4 = glGetUniformLocation(glr_prog, "toon_k4");
    glu_tphl = glGetUniformLocation(glr_prog, "toon_tphl");
    glu_tpx = glGetUniformLocation(glr_prog, "toon_tpx");
    glu_bolt = glGetUniformLocation(glr_prog, "toon_bolt");
    glu_tpix = glGetUniformLocation(glr_prog, "toon_px");
    glu_softbl = glGetUniformLocation(glr_prog, "soft_bl");
    glu_fxlp = glGetUniformLocation(glr_prog, "fx_lp");
    glu_fxlc = glGetUniformLocation(glr_prog, "fx_lc");
    glu_wc = glGetUniformLocation(glr_prog, "water_c");
    glu_ww = glGetUniformLocation(glr_prog, "water_w");
    glu_wetk = glGetUniformLocation(glr_prog, "wet_k");
    glu_chg = glGetUniformLocation(glr_prog, "chg_k");
    glUniform1i(glu_tex0, 0);
    glUniform1i(glu_pal0, 1);
    glUniform1i(glu_dst0, 2);
    glUniform1i(glu_ramp0, 3);
    return true;
}

static const GLenum GLR_CMP[8] = {GL_NEVER, GL_ALWAYS, GL_EQUAL, GL_NOTEQUAL, GL_LESS, GL_LEQUAL, GL_GREATER, GL_GEQUAL};

// blend factors: 6 to 9 double alpha and have no direct equivalent,
// case 6/7 is solved by outputting min(2a,1) from the shader
static GLenum glr_factor(int f, bool source, bool* doubled) {
    switch (f) {
    case 0: return source ? GL_DST_COLOR : GL_SRC_COLOR;
    case 1: return source ? GL_ONE_MINUS_DST_COLOR : GL_ONE_MINUS_SRC_COLOR;
    case 2: return GL_SRC_ALPHA;
    case 3: return GL_ONE_MINUS_SRC_ALPHA;
    case 4: return GL_DST_ALPHA;
    case 5: return GL_ONE_MINUS_DST_ALPHA;
    case 6: *doubled = true; return GL_SRC_ALPHA;
    case 7: *doubled = true; return GL_ONE_MINUS_SRC_ALPHA;
    case 8: glr_warning("factor alfa destino doble sin equivalente", f); return GL_DST_ALPHA;
    case 9: glr_warning("factor alfa destino doble sin equivalente", f); return GL_ONE_MINUS_DST_ALPHA;
    default: return GL_CONSTANT_COLOR;
    }
}

static const GLenum GLR_EQ[8] = {GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX,
                                 GL_FUNC_SUBTRACT, GL_FUNC_ADD, GL_FUNC_ADD};

// destination copy for per-bit masks: only the scissor rect, one texture per destination size
static GLuint glr_mc_tex[4];
static int glr_mc_w[4], glr_mc_h[4], glr_mc_next = 0;

static void glr_copy_rect(DestGL* d, int x, int y, int w, int h) {
    GqZone gqz_(GQ_COPY);
    int k = 0;
    while (k < 4 && !(glr_mc_tex[k] && glr_mc_w[k] == d->w && glr_mc_h[k] == d->h)) k++;
    if (k == 4) {
        k = glr_mc_next; glr_mc_next = (glr_mc_next + 1) & 3;
        if (!glr_mc_tex[k]) {
            glGenTextures(1, &glr_mc_tex[k]);
            glBindTexture(GL_TEXTURE_2D, glr_mc_tex[k]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glBindTexture(GL_TEXTURE_2D, glr_mc_tex[k]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d->w, d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glr_mc_w[k] = d->w; glr_mc_h[k] = d->h;
    }
    glBindTexture(GL_TEXTURE_2D, glr_mc_tex[k]);
    if (x + w > d->w) w = d->w - x;
    if (y + h > d->h) h = d->h - y;
    if (w <= 0 || h <= 0) return;
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, x, y, x, y, w, h);
    glr_copies++;
    glr_copy_px += (long long)w * h;
}

static bool glr_mask_partial(uint32_t m) {
    for (int i = 0; i < 3; i++) { uint32_t b = (m >> (8 * i)) & 0xFF; if (b != 0 && b != 0xFF) return true; }
    return false;
}

static bool kk_rec = false; static GLuint kk_fbo = 0; static DestGL* kk_d = nullptr;   // bars behind: their draws again on the layer target
static void glr_draw_batch() {
    if (glr_vtx.empty()) return;
    const bool cpe = cp_on();
    uint64_t c0 = cpe ? perf_clock() : 0;
    glr_pend_copy(glr_vtx.data(), nullptr, glr_vtx.size());
    GqZone gqz_(gq_cat);
    bool lite = glr_st_lite && glr_lite_on() && glr_vtx_plain(glr_vtx.data(), glr_vtx.size());
    glr_draw_n++; if (lite) { glr_lite_n++; glUseProgram(glr_lite); }
    gq_area(gq_cat, glr_vtx.data(), nullptr, glr_vtx.size());
    uint64_t c1 = cpe ? perf_clock() : 0;
    glBindBuffer(GL_ARRAY_BUFFER, glr_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(glr_vtx.size() * sizeof(VtxGL)), glr_vtx.data(), GL_STREAM_DRAW);
    uint64_t c2 = cpe ? perf_clock() : 0;
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)glr_vtx.size());
    if (kk_rec && glr_current == kk_d) { glBindFramebuffer(GL_FRAMEBUFFER, kk_fbo); glDrawArrays(GL_TRIANGLES, 0, (GLsizei)glr_vtx.size()); glBindFramebuffer(GL_FRAMEBUFFER, dfbo(glr_current)); }
    if (cpe) { uint64_t c3 = perf_clock(); cp_db[0]++; cp_db[1] += glr_vtx.size(); cp_db[2] += c1 - c0; cp_db[3] += c2 - c1; cp_db[4] += c3 - c2; }
    if (lite) glUseProgram(glr_prog);
    glr_vtx.clear();
    if (glr_current && glr_current->mfbo) glr_current->dirty = true;
}

static DestGL* cap_main = nullptr;   // the screen of the capture under way (gpu.cpp cap_*)

static const GLenum GLR_SRC1_ALPHA = 0x8589, GLR_ONE_MINUS_SRC1_ALPHA = 0x88FB;
static void glr_state(const TexState& e) {
    glr_pend_self = false;
    DestGL* d = glr_find(e.f.dir, e.f.width, e.f.fmt);
    bool capd = cap_on && e.f.dir == cap_dir;   // only the screen: the outline mask and other targets stay as they are
    if (capd) cap_main = d;
    if (e.texture && !e.cleared && e.gltex && e.gltex != d->tex) glr_resolve_tex(e.gltex);   // before any state: the resolve pass changes it
    if (d != glr_current) { glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d)); glr_current = d; }
    if (capd && (cap_fresh || (!cap_zdone && !e.cleared && e.ztest))) {   // transparent; depth cleared to the far end of the first depth test
        glDisable(GL_SCISSOR_TEST);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glDepthMask(GL_TRUE); glStencilMask(0xFF);
        GLbitfield bits = 0;
        if (cap_fresh) { if (cap_map) glClearColor(0, 0, 0, 1); else glClearColor(1.0f / 255.0f, 0, 1.0f / 255.0f, 0);   // a key colour: black outline pixels count as drawn
            glClearStencil(0); bits |= GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT; cap_fresh = false; }
        if (!cap_zdone && !e.cleared && e.ztest) {
            int f = e.zfun & 7;
            glClearDepth(f == 6 || f == 7 ? 0.0 : 1.0); bits |= GL_DEPTH_BUFFER_BIT; cap_zdone = true;
            static int logs = 0;
            if (logs++ < 2) printf("render: prueba de profundidad %d, fondo a %s\n", f, f == 6 || f == 7 ? "0" : "1");
        }
        glClear(bits);
    }
    glViewport(0, 0, d->w, d->h);
    glEnable(GL_SCISSOR_TEST);
    int sx0 = e.sx0 < 0 ? 0 : e.sx0, sy0 = e.sy0 < 0 ? 0 : e.sy0;
    int sw = e.sx1 - sx0 + 1, sh = e.sy1 - sy0 + 1;
    glScissor(sx0, sy0, sw < 0 ? 0 : sw, sh < 0 ? 0 : sh);

    // framebuffer alpha is the PSP stencil: written only when the operation changes it
    bool cleared = e.cleared;
    bool st_reemp = !cleared && e.stest && e.szpass == 2;
    if (!cleared && e.stest && (e.sfail != 0 || e.szfail != 0 || (e.szpass != 0 && e.szpass != 2)))
        glr_warning("operacion de stencil sin reflejo en alfa", (int)(e.sfail | (e.szfail << 4) | (e.szpass << 8)));
    bool mr, mg, mb, ma;
    if (cleared) {
        mr = mg = mb = ((e.modo >> 8) & 1) != 0;
        ma = ((e.modo >> 9) & 1) != 0;
    } else {
        mr = (e.mrgb & 0xFF) != 0xFF;
        mg = ((e.mrgb >> 8) & 0xFF) != 0xFF;
        mb = ((e.mrgb >> 16) & 0xFF) != 0xFF;
        if (e.stest) ma = st_reemp;
        else ma = !(e.sbits && keep_alpha());
        if (e.ma == 0xFF) ma = false;
    }
    glColorMask(mr, mg, mb, ma); glr_cm[0] = mr; glr_cm[1] = mg; glr_cm[2] = mb; glr_cm[3] = ma;
    bool pm = !cleared && patch_pixel_mask.val && glr_mask_partial(e.mrgb);
    if (pm && e.blend) { pm = false; glr_warning("mascara de bits junto a mezcla", (int)e.mrgb); }
    glUniform1i(glu_pmon, pm ? 1 : 0);
    if (pm) {
        glUniform1i(glu_pmrgb, (GLint)e.mrgb);
        glActiveTexture(GL_TEXTURE2);
        glr_copy_rect(d, sx0, sy0, sw, sh);
        glActiveTexture(GL_TEXTURE0);
        if (glr_new(0x3A5C0000u ^ d->dir ^ e.mrgb))
            printf("gl: mascara de bits en destino 0x%08X, mascara 0x%06X\n", d->dir, e.mrgb);
    }

    if (cleared) {
        glDisable(GL_BLEND);
        glDepthFunc(GL_ALWAYS);
        glDepthMask(((e.modo >> 10) & 1) != 0);
        glEnable(GL_DEPTH_TEST);
    } else {
        if (e.ztest) { glEnable(GL_DEPTH_TEST); glDepthFunc(GLR_CMP[e.zfun & 7]); }
        else { glDisable(GL_DEPTH_TEST); }
        glDepthMask(e.zwrite);
    }

    bool doubled = false;
    int eqp = (e.bmode >> 8) & 7;
    bool prog = !cleared && e.blend && eqp == 5;   // ABSDIFF: absent in GL, computed in the shader
    if (prog) {
        if (glr_copy_part() && glr_copy && glr_copy_w == glr_current->w && glr_copy_h == glr_current->h) {
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, glr_copy); glActiveTexture(GL_TEXTURE0);
            glr_pend_abs = true;
        } else {
            glActiveTexture(GL_TEXTURE2);
            glr_copy_fb(glr_current);
            glActiveTexture(GL_TEXTURE0);
        }
    } else glr_pend_abs = false;
    glUniform1i(glu_mprog, prog ? 1 : 0);
    if (!cleared && e.blend && !prog) {
        int fs = e.bmode & 15, fd = (e.bmode >> 4) & 15, eq = eqp;
        GLenum gs = glr_factor(fs, true, &doubled), gd = glr_factor(fd, false, &doubled);
        if (fs == 10 || fd == 10) {
            uint32_t fixed = fs == 10 ? e.fixed_a : e.fixed_b;
            glBlendColor((fixed & 0xFF) / 255.0f, ((fixed >> 8) & 0xFF) / 255.0f, ((fixed >> 16) & 0xFF) / 255.0f, 1.0f);
            if (fs == 10 && fd == 10 && e.fixed_a != e.fixed_b) glr_warning("dos constantes de mezcla distintas", 10);
        }
        glEnable(GL_BLEND);
        glBlendEquation(GLR_EQ[eq]);
        if (st_reemp && !doubled) {   // GE blends with the fragment alpha; the framebuffer alpha takes the stencil reference
            auto s1 = [](GLenum f) -> GLenum { return f == GL_SRC_ALPHA ? GLR_SRC1_ALPHA : f == GL_ONE_MINUS_SRC_ALPHA ? GLR_ONE_MINUS_SRC1_ALPHA : f; };
            glBlendFuncSeparate(s1(gs), s1(gd), GL_ONE, GL_ZERO);
        } else glBlendFunc(gs, gd);
    } else glDisable(GL_BLEND);

    glUniform1i(glu_aton, (!cleared && e.atest) ? 1 : 0); gll_1i(gll_aton, (!cleared && e.atest) ? 1 : 0);
    glUniform1i(glu_atfn, e.atst & 7); gll_1i(gll_atfn, e.atst & 7);
    glUniform1i(glu_atref, (e.atst >> 8) & 0xFF); gll_1i(gll_atref, (e.atst >> 8) & 0xFF);
    glUniform1i(glu_atmsk, (e.atst >> 16) & 0xFF); gll_1i(gll_atmsk, (e.atst >> 16) & 0xFF);

    if (st_reemp && doubled) { st_reemp = false; glr_warning("stencil junto a mezcla de alfa doble", 1); }
    glUniform1i(glu_ston, st_reemp ? 1 : 0); gll_1i(gll_ston, st_reemp ? 1 : 0);
    // the GL stencil tracks the same count as alpha: one serves the test, the other the palette
    static const GLenum OPS[8] = {GL_KEEP, GL_ZERO, GL_REPLACE, GL_INVERT, GL_INCR, GL_DECR, GL_KEEP, GL_KEEP};
    if (cleared) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, ma ? GL_REPLACE : GL_KEEP);
        glStencilMask(ma ? 0xFF : 0);
    } else if (e.stest) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GLR_CMP[e.stst & 7], (GLint)((e.stst >> 8) & 0xFF), (GLuint)((e.stst >> 16) & 0xFF));
        glStencilOp(OPS[e.sfail & 7], OPS[e.szfail & 7], OPS[e.szpass & 7]);
        glStencilMask((GLuint)(~e.ma & 0xFF));
    } else {
        glDisable(GL_STENCIL_TEST);
        glStencilMask(0);
    }
    glUniform1f(glu_stref, ((e.stst >> 8) & 0xFF) / 255.0f); gll_1f(gll_stref, ((e.stst >> 8) & 0xFF) / 255.0f);
    glUniform1i(glu_adouble, doubled ? 1 : 0); gll_1i(gll_adouble, doubled ? 1 : 0);

    int use = 0; bool lite_ok = !pm && !prog && !glr_verlod();
    if (e.texture && !cleared) {
        use = e.gltex_clut ? 2 : 1;
        GLuint tx = e.gltex;
        if (tx && glr_current && tx == glr_current->tex) {
            if (glr_copy_part() && glr_copy && glr_copy_w == glr_current->w && glr_copy_h == glr_current->h) { tx = glr_copy; glr_pend_self = true; }
            else tx = glr_copy_fb(glr_current);
            if (glr_new(0xC0DE0000u ^ glr_current->dir))
                printf("gl: lectura del propio destino 0x%08X, uso copia\n", glr_current->dir);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tx);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (e.wrap & 1) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (e.wrap & 0x100) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        int mm = glr_mips();
        bool mips = mm != 0 && use == 1 && e.chain != nullptr && e.chain->nn > 1 && (mm == 2 ? true : e.mip);
        int cm = (e.clutf >> 8) & 0xFF;
        bool sh_rt = use == 2 && e.gltex_rtfmt == 2 && cm && !(cm & (cm - 1)) && e.t.fmt >= 4;   // fighter shadow: one silhouette bit of the 4444 shadow target
        bool soft = ((cm == 1 && e.t.fmt >= 4 && e.t.w == 64 && e.t.h == 64) || sh_rt) && glr_soft();
        if (soft || use == 2) lite_ok = false;
        if (sh_rt) { static int sl = 0; static uint32_t seen = 0; if (sl < 8 && !(seen & (1u << (cm & 31)))) { sl++; seen |= 1u << (cm & 31); printf("sombra: silueta leida del destino 0x%08X, mascara %d, desplazamiento %d, textura %dx%d, suave %s\n", e.t.dir, cm, (int)((e.clutf >> 2) & 0x1F), e.gltex_w, e.gltex_h, soft ? "si" : "no"); } }
        GLint mag = e.bilinear || (soft && use == 1) ? GL_LINEAR : GL_NEAREST;
        GLint min = mips ? (e.bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR) : mag;
        bool upc = use == 1 && e.chain && glr_up_of(e.chain->aw[0], e.chain->ah[0]);
        if (!mips && upc) min = e.bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;   // filtered copies: never sample the big level when minified
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
        if (upc && !mips) { glr_aniso(); glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(glr_aniso_max, 16.0f)); glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, TF_BIAS); }   // sharp at grazing angles
        else {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, mips ? glr_aniso() : 1.0f);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, mips ? glr_bias() : 0.0f);
        }
        if (use == 2) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, e.glpal);
            glActiveTexture(GL_TEXTURE0);
            glUniform1i(glu_rtfmt, e.gltex_rtfmt);
            glUniform1i(glu_cshift, (e.clutf >> 2) & 0x1F);
            glUniform1i(glu_cmask, (e.clutf >> 8) & 0xFF);
        }
        glUniform1f(glu_soft, soft ? (use == 2 ? SH_R * (float)scale_int() : SH_R) : 0.0f);
        glUniform1i(glu_softbl, SH_BL);
        if (((e.clutf >> 8) & 0xFF) == 1 && e.t.fmt >= 4 && e.t.w == 64 && glr_new(0x5AD00000u ^ (uint32_t)use))
            printf("sombra: textura 0x%08X leida por %s, %dx%d, suave %s\n", e.t.dir, use == 2 ? "el destino de GL con CLUT" : "la cadena decodificada", e.gltex_w, e.gltex_h, soft ? "si" : "no");
    }
    glUniform1i(glu_use, use); gll_1i(gll_use, use);
    glUniform1i(glu_verlod, glr_verlod());
    glUniform1i(glu_mode, e.tf & 7); gll_1i(gll_mode, e.tf & 7);
    glUniform1i(glu_alpha, (e.tf >> 8) & 1); gll_1i(gll_alpha, (e.tf >> 8) & 1);
    glUniform1i(glu_dbl, (e.tf >> 16) & 1); gll_1i(gll_dbl, (e.tf >> 16) & 1);
    glr_st_lite = lite_ok;
    glr_st_tdir = use ? e.t.dir : 0; glr_st_tw = use ? e.t.w : 0; glr_st_th = use ? e.t.h : 0; glr_st_bl = (!cleared && e.blend) ? (int)(e.bmode & 0xFFF) : -1;
}

static inline void glr_push(std::vector<VtxGL>& v, float x, float y, float z, float iw,
                            float u, float vv, uint32_t col, float sw, float sh, float tw, float th, float tl = -9.0f, float tf = 0.0f, uint32_t tint = 0, uint32_t ext = 0, uint32_t tp = 0,
                            float wd = -1e4f, float wc = 0.0f, float wet = 0.0f, uint32_t nrm = 0, uint32_t lgt = 0, const float* vp = nullptr, float pf = 0.0f) {
    VtxGL o;
    float w = (iw > 1e-20f || iw < -1e-20f) ? 1.0f / iw : 1.0f;
    float nz = 2.0f * (z / 65535.0f) - 1.0f;
    o.x = (2.0f * x / sw - 1.0f) * w;
    o.y = (2.0f * y / sh - 1.0f) * w;
    o.z = nz * w;
    o.w = w;
    o.u = (u * w) / tw;
    o.v = (vv * w) / th;
    o.col = col;
    o.tl = tl; o.tf = tf; o.tint = tint; o.ext = ext; o.tp = tp;
    o.wd = wd; o.wc = wc; o.wet = wet; o.wpad = 0.0f;
    o.nrm = nrm; o.lgt = lgt; o.pf = pf;
    if (vp) { o.px = vp[0]; o.py = vp[1]; o.pz = vp[2]; } else o.px = o.py = o.pz = 0.0f;
    v.push_back(o);
}

// F5 saves the decoded texture of every state of the current flush as PNG
static int glr_dumped = 0;

static void glr_dump(const TexState& e) {
    if (!e.chain || glr_dumped >= 24) return;
    char nm[160];
    snprintf(nm, sizeof(nm), "tex_%02d_%08X_%dx%d_fmt%d_swz%d_stride%d.png",
             glr_dumped, e.t.dir, e.t.w, e.t.h, e.t.fmt, e.t.swz, e.t.stride);
    save_png(nm, e.chain->px[0].data(), e.chain->aw[0], e.chain->ah[0]);
    printf("volcada %s (niveles %d)\n", nm, e.chain->nn);
    glr_dumped++;
}

static GLuint ms_prog = 0, ms_vao = 0; static GLint ms_u_src = -1;
static void ms_paste(DestGL* d, GLuint src, int x0, int y0, int x1, int y1) {
    if (!ms_prog) {
        ms_prog = gl_program("#version 330 core\nvoid main(){ vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID >> 1) * 4.0 - 1.0); gl_Position = vec4(p, 0.0, 1.0); }\n",
                             "#version 330 core\nuniform sampler2D src; out vec4 o; void main(){ o = texelFetch(src, ivec2(gl_FragCoord.xy), 0); }\n");
        glGenVertexArrays(1, &ms_vao);
        ms_u_src = glGetUniformLocation(ms_prog, "src");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, d->mfbo);
    glViewport(0, 0, d->w, d->mh);
    glEnable(GL_SCISSOR_TEST); glScissor(x0, y0, x1 - x0, y1 - y0);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
    glUseProgram(ms_prog); glBindVertexArray(ms_vao);
    glActiveTexture(GL_TEXTURE0 + 6); glBindTexture(GL_TEXTURE_2D, src); glUniform1i(ms_u_src, 6);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(glr_prog); glBindVertexArray(glr_vao);
    d->dirty = true;
}

// bars behind fighters (marker 0xEF): the display alpha (PSP stencil: outline layers) saved before the bars and written back after
static GLuint ak_fbo = 0, ak_tex = 0; static int ak_w = 0, ak_h = 0; static bool ak_saved = false;
static void glr_alpha_keep(bool save) {
    DestGL* d = glr_current;
    if (!d) { ak_saved = false; return; }
    if (save) {
        if (!ak_fbo || ak_w != d->w || ak_h != d->h) {
            if (!ak_tex) glGenTextures(1, &ak_tex);
            glBindTexture(GL_TEXTURE_2D, ak_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d->w, d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            if (!ak_fbo) glGenFramebuffers(1, &ak_fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, ak_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ak_tex, 0);
            ak_w = d->w; ak_h = d->h;
        }
        glDisable(GL_SCISSOR_TEST);
        glr_resolve(d);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ak_fbo);
        glBlitFramebuffer(0, 0, d->w, d->h, 0, 0, d->w, d->h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
        glEnable(GL_SCISSOR_TEST);
        ak_saved = true;
        return;
    }
    if (!ak_saved || ak_w != d->w || ak_h != d->h) return;
    ak_saved = false;
    if (!ms_prog) {
        ms_prog = gl_program("#version 330 core\nvoid main(){ vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID >> 1) * 4.0 - 1.0); gl_Position = vec4(p, 0.0, 1.0); }\n",
                             "#version 330 core\nuniform sampler2D src; out vec4 o; void main(){ o = texelFetch(src, ivec2(gl_FragCoord.xy), 0); }\n");
        glGenVertexArrays(1, &ms_vao);
        ms_u_src = glGetUniformLocation(ms_prog, "src");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    glViewport(0, 0, d->w, d->mfbo ? d->mh : d->h);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glUseProgram(ms_prog); glBindVertexArray(ms_vao);
    glActiveTexture(GL_TEXTURE0 + 6); glBindTexture(GL_TEXTURE_2D, ak_tex); glUniform1i(ms_u_src, 6);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_SCISSOR_TEST);
    glUseProgram(glr_prog); glBindVertexArray(glr_vao);
    if (d->mfbo) d->dirty = true;
    static int logs = 0;
    if (logs++ < 2) printf("gl: alfa del cuadro devuelto tras las barras (%dx%d)\n", d->w, d->h);
}

// alternate views (patch split_alt): a view's pixels saved to a same-size copy and pasted back on frames it is not drawn
static GLuint glr_sv_fbo = 0, glr_sv_tex = 0;
static int glr_sv_w = 0, glr_sv_h = 0;

static bool glr_view_copy(uint32_t dir, int width, int x0, int y0, int x1, int y1, bool save) {
    GqZone gqz_(GQ_VIEW);
    DestGL* d = nullptr;
    for (int i = 0; i < glr_dest_n; i++) if (glr_dest[i].dir == dir && glr_dest[i].width == width) d = &glr_dest[i];
    if (!d) return false;
    if (!glr_sv_fbo || glr_sv_w != d->w || glr_sv_h != d->h) {
        if (!save) return false;
        if (!glr_sv_tex) glGenTextures(1, &glr_sv_tex);
        glBindTexture(GL_TEXTURE_2D, glr_sv_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d->w, d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (!glr_sv_fbo) glGenFramebuffers(1, &glr_sv_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, glr_sv_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glr_sv_tex, 0);
        glr_sv_w = d->w; glr_sv_h = d->h;
        printf("gl: copia de vistas %dx%d\n", d->w, d->h);
    }
    if (x1 > d->w) x1 = d->w;
    if (y1 > d->h) y1 = d->h;
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (save) glr_resolve(d);
    if (!save && d->mfbo) ms_paste(d, glr_sv_tex, x0, y0, x1, y1);   // a multisampled target cannot be a blit destination
    else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, save ? d->fbo : glr_sv_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, save ? glr_sv_fbo : d->fbo);
        glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
    glEnable(GL_SCISSOR_TEST);
    return true;
}

// map select 3D: two same-size copies of the target, 0 = the screen before the panel's 3D, 1 = the 3D (rect copies and pastes)
static GLuint m3_fbo[2] = {0, 0}, m3_tex[2] = {0, 0};
static int m3_w = 0, m3_h = 0;
static bool glr_m3d_ready(DestGL* d) {
    if (m3_fbo[1] && m3_w == d->w && m3_h == d->h) return true;
    for (int k = 0; k < 2; k++) {
        if (!m3_tex[k]) glGenTextures(1, &m3_tex[k]);
        glBindTexture(GL_TEXTURE_2D, m3_tex[k]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d->w, d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (!m3_fbo[k]) glGenFramebuffers(1, &m3_fbo[k]);
        glBindFramebuffer(GL_FRAMEBUFFER, m3_fbo[k]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m3_tex[k], 0);
    }
    m3_w = d->w; m3_h = d->h;
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    printf("gl: copias del mapa 3D %dx%d\n", d->w, d->h);
    return true;
}
static void glr_m3d_copy(int k, int x0, int y0, int x1, int y1) {
    DestGL* d = glr_current;
    if (!d || !glr_m3d_ready(d)) return;
    x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(d->w, x1); y1 = std::min(d->h, y1);
    if (x1 <= x0 || y1 <= y0) return;
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glr_resolve(d);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m3_fbo[k]);
    glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    glEnable(GL_SCISSOR_TEST);
}
static bool glr_m3d_paste(int k, int x0, int y0, int x1, int y1) {
    DestGL* d = glr_current;
    if (!d || !m3_fbo[k] || m3_w != d->w || m3_h != d->h) return false;
    x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(d->w, x1); y1 = std::min(d->h, y1);
    if (x1 <= x0 || y1 <= y0) return false;
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (d->mfbo) ms_paste(d, m3_tex[k], x0, y0, x1, y1);   // a multisampled target cannot be a blit destination
    else {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m3_fbo[k]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, d->fbo);
        glBlitFramebuffer(x0, y0, x1, y1, x0, y0, x1, y1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    glEnable(GL_SCISSOR_TEST);
    return true;
}

// tunables TTT_CEL_*: fill, edge, shade, tint, aura, skin, merge, sphere, face, soft, highlight, warm, bounce, vfx, map
static float TOON_FILL = 0.4f, TOON_EDGE = 0.0f, TOON_SHADE = 0.5f, TOON_TINT = 0.6f, TOON_AURA = 0.6f, TOON_SKIN = 0.5f, TOON_MERGE = 1.0f, TOON_SPHERE = 0.0f, TOON_FACE = 0.4f, TOON_SOFT = 0.015f, TOON_HI = 0.55f, TOON_HL = 1.0f, TOON_WARM = 0.5f, TOON_BOUNCE = 0.2f, TOON_VFX = 0.9f, TOON_MAP = 1.0f, TOON_MAPREF = 0.45f, TOON_MAPMIN = 0.45f, TOON_WRAP = 0.0f, TOON_SKY = 0.9f, TOON_HUE = 0.0f, TOON_WARMMIN = 0.65f, TOON_WARMHUE = 0.12f, TOON_TPHL = 0.0f, TOON_TPX = 0.0f, TOON_BOLT = 0.35f, TOON_BOLTUP = 0.8f, TOON_BOLTAMB = 0.12f, TOON_BOLTMAP = 0.6f, TOON_BOLTSIGN = -1.0f, TOON_BOLTGMIN = 0.05f, TOON_BOLTGMAX = 0.35f, TOON_PIXEL = 1.0f, TOON_SMOOTH = 0.25f;
static float toon_env(const char* n, float d) { const char* e = getenv(n); return e && *e ? (float)atof(e) : d; }
static void toon_params() {
    static int n = 0;
    if ((n++ & 63) == 0) {
        TOON_FILL = toon_env("TTT_CEL_FILL", 0.4f); TOON_EDGE = toon_env("TTT_CEL_EDGE", 0.0f);
        TOON_SHADE = toon_env("TTT_CEL_SHADE", 0.5f); TOON_TINT = toon_env("TTT_CEL_TINT", 0.6f);
        TOON_AURA = toon_env("TTT_CEL_AURA", 0.6f); TOON_SKIN = toon_env("TTT_CEL_SKIN", 0.5f);
        TOON_MERGE = toon_env("TTT_CEL_MERGE", 1.0f); TOON_SPHERE = toon_env("TTT_CEL_SPHERE", 0.0f);
        TOON_FACE = toon_env("TTT_CEL_FACE", 0.4f); TOON_SOFT = toon_env("TTT_CEL_SOFT", 0.015f);
        TOON_HI = toon_env("TTT_CEL_HI", 0.55f); TOON_HL = toon_env("TTT_CEL_HL", 1.0f); TOON_WARM = toon_env("TTT_CEL_WARM", 0.5f);
        TOON_BOUNCE = toon_env("TTT_CEL_BOUNCE", 0.2f); TOON_VFX = toon_env("TTT_CEL_VFX", 0.9f); TOON_MAP = toon_env("TTT_CEL_MAP", 1.0f);
        TOON_MAPREF = toon_env("TTT_CEL_MAPREF", 0.45f); TOON_MAPMIN = toon_env("TTT_CEL_MAPMIN", 0.45f); TOON_WRAP = toon_env("TTT_CEL_WRAP", 0.0f); TOON_SKY = toon_env("TTT_CEL_SKY", 0.9f); TOON_HUE = toon_env("TTT_CEL_HUE", 0.0f); TOON_WARMMIN = toon_env("TTT_CEL_WARMMIN", 0.65f); TOON_WARMHUE = toon_env("TTT_CEL_WARMHUE", 0.12f); TOON_TPHL = toon_env("TTT_CEL_TPHL", 0.0f); TOON_TPX = toon_env("TTT_CEL_TPX", 0.0f);
        TOON_BOLT = toon_env("TTT_CEL_BOLT", 0.35f); TOON_BOLTUP = toon_env("TTT_CEL_BOLTUP", 0.8f); TOON_BOLTAMB = toon_env("TTT_CEL_BOLTAMB", 0.12f); TOON_BOLTMAP = toon_env("TTT_CEL_BOLTMAP", 0.6f); TOON_BOLTSIGN = toon_env("TTT_CEL_BOLTSIGN", -1.0f); TOON_BOLTGMIN = toon_env("TTT_CEL_BOLTGMIN", 0.05f); TOON_BOLTGMAX = toon_env("TTT_CEL_BOLTGMAX", 0.35f);
        TOON_PIXEL = toon_env("TTT_CEL_PIXEL", 1.0f); TOON_SMOOTH = toon_env("TTT_CEL_SMOOTH", 0.25f);
    }
}
// light from the scene: additive effects of the previous frame (one weighted light), the ground under each fighter, the map
struct ToonVfx { double c[3], p[3], w; };
static ToonVfx vfx_cur = {}, vfx_prev = {}, amb_cur = {}, sky_cur = {};
struct VfxCl { double c[3], p[3], w; uint32_t vh; };
static VfxCl vcl_cur[6]; static int vcl_n = 0;
struct MapL { float p[3], c[3], e, t; uint32_t vh; };
static MapL map_l[4] = {};
static float fz_cur = 0, fz_prev = 0;
static uint16_t ol_ids[8], ol_bg[8]; static float ol_zr[128][2];   // outline layers of this frame's fighters (depth range each) and of the scenery
static uint8_t g3_lut[128]; static bool g3_any = false; static int g3_n[4];   // diagnosis: BT3 fighter jobs, with glow, with stencil id, frames   // Cel BT3 glow: strength per outline layer id of this frame's BT3 fighters
static uint8_t ol_who[128];   // outline id -> fighter slot + 1 (0 none), this frame
static double fg_bb[8][5]; static bool fg_bbok[8];   // fighter screen vertex sums this frame (n, x, y, x2, y2), complete at the ATM pass
static void ol_reset2() { memset(fg_bbok, 0, sizeof(fg_bbok)); memset(fg_bb, 0, sizeof(fg_bb));
    memset(ol_who, 0, sizeof(ol_who)); memset(ol_ids, 0, sizeof(ol_ids)); memset(ol_bg, 0, sizeof(ol_bg)); for (int i = 0; i < 128; i++) { ol_zr[i][0] = 1e30f; ol_zr[i][1] = 0; } }
static uint32_t toon_fno = 0;
// Enhanced water: the screen alpha (water mask 126, fighter ids) saved right after the mask is drawn and written back before the atmosphere reads it
static GLuint wk_fbo = 0, wk_tex = 0, wk_prog = 0, wk_vao = 0; static int wk_w = 0, wk_h = 0; static uint32_t wk_f = 0xFFFFFFF0u;
static void wk_save(DestGL* d) {
    if (!wk_fbo || wk_w != d->w || wk_h != d->h) {
        if (!wk_tex) glGenTextures(1, &wk_tex);
        glBindTexture(GL_TEXTURE_2D, wk_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d->w, d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (!wk_fbo) glGenFramebuffers(1, &wk_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, wk_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, wk_tex, 0);
        wk_w = d->w; wk_h = d->h;
    }
    GLboolean sc = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    glr_resolve(d);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, wk_fbo);
    glBlitFramebuffer(0, 0, d->w, d->h, 0, 0, d->w, d->h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    if (sc) glEnable(GL_SCISSOR_TEST);
    wk_f = toon_fno;
}
static bool wk_put(DestGL* d) {
    if (!patch_water_mask_keep.val || !wk_fbo || wk_w != d->w || wk_h != d->h || toon_fno - wk_f > 1u) return false;
    if (!wk_prog) {
        wk_prog = gl_program("#version 330 core\nvoid main(){ vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID >> 1) * 4.0 - 1.0); gl_Position = vec4(p, 0.0, 1.0); }\n",
                             "#version 330 core\nuniform sampler2D src; out vec4 o; void main(){ o = texelFetch(src, ivec2(gl_FragCoord.xy), 0); }\n");
        if (!wk_prog) { wk_prog = 0xFFFFFFFFu; return false; }
        glGenVertexArrays(1, &wk_vao);
    }
    if (wk_prog == 0xFFFFFFFFu) return false;
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    glViewport(0, 0, d->w, d->mfbo ? d->mh : d->h);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glUseProgram(wk_prog); glBindVertexArray(wk_vao);
    glActiveTexture(GL_TEXTURE0 + 6); glBindTexture(GL_TEXTURE_2D, wk_tex); glUniform1i(glGetUniformLocation(wk_prog, "src"), 6);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (d->mfbo) d->dirty = true;
    return true;
}

// the game's outline passes (A: display alpha -> edge mask, B: mask over the screen) limited to the box of this frame's fighters
// (TTT_OL_BOX=0 = full-screen passes as before; Pause toggles). The mask target is cleared once per frame: outside the box = no edge
static int olb_var = -1; static float olb_x0, olb_y0, olb_x1, olb_y1; static bool olb_any = false; static long olb_n = 0, olb_full = 0; static uint32_t olb_clr_f = ~0u;
static bool olb_on() { if (olb_var < 0) { const char* e = getenv("TTT_OL_BOX"); olb_var = e && *e ? atoi(e) : 1; } return olb_var > 0; }
static int olb_mv = -1;
static bool olb_merge() { if (olb_mv < 0) { const char* e = getenv("TTT_OL_BOX_MERGE"); olb_mv = e && *e ? atoi(e) : 1; } return olb_mv > 0; }
static void olb_reset() { olb_any = false; olb_x0 = olb_y0 = 1e9f; olb_x1 = olb_y1 = -1e9f; }
static inline void olb_add(float x, float y) { olb_any = true; olb_x0 = std::min(olb_x0, x); olb_y0 = std::min(olb_y0, y); olb_x1 = std::max(olb_x1, x); olb_y1 = std::max(olb_y1, y); }
static int olb_pass(const TexState& e, int mainw) {
    if (!e.texture || e.cleared || !e.gltex_clut || e.f.width < mainw) return 0;
    int sh = (e.clutf >> 2) & 0x1F, mk = (e.clutf >> 8) & 0xFF;
    if (e.t.fmt == 7 && sh == 24 && mk == 0x7F) return 1;
    if (e.t.fmt == 6 && sh == 5 && mk == 0xFF) return 2;
    return 0;
}
static void olb_apply(const TexState& e, int mainw, bool battle) {
    int p = olb_on() && battle ? olb_pass(e, mainw) : 0;
    if (!p) return;
    if (!olb_any) { olb_full++; return; }
    int m = 8 * scale_int() + 4;
    int sx0 = e.sx0 < 0 ? 0 : e.sx0, sy0 = e.sy0 < 0 ? 0 : e.sy0;
    int bx0 = std::max(sx0, (int)std::floor(olb_x0) - m), by0 = std::max(sy0, (int)std::floor(olb_y0) - m);
    int bx1 = std::min(e.sx1, (int)std::ceil(olb_x1) + m), by1 = std::min(e.sy1, (int)std::ceil(olb_y1) + m);
    if (p == 1 && olb_clr_f != toon_fno) {
        olb_clr_f = toon_fno;
        int qm = nq_mode(); uint64_t qt = perf_clock();
        GLboolean cm[4]; if (qm) memcpy(cm, glr_cm, sizeof(cm)); else { SubT q_(glq_sid()); glGetBooleanv(GL_COLOR_WRITEMASK, cm); }
        glDisable(GL_SCISSOR_TEST); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
        glColorMask(cm[0], cm[1], cm[2], cm[3]); glEnable(GL_SCISSOR_TEST);
        nq_st[qm][0]++; nq_st[qm][1] += perf_clock() - qt;
    }
    glScissor(bx0, by0, std::max(0, bx1 - bx0 + 1), std::max(0, by1 - by0 + 1));
    olb_n++;
}
static void ol_reset() {
    static int logs = 0;
    if (g3_n[0] && logs < 6) { logs++; printf("cel: brillo BT3 diagnostico, cuadro: %d dibujos de luchador, %d con brillo, %d con brillo y capa, pasada %s\n", g3_n[0], g3_n[1], g3_n[2], g3_n[3] ? "si" : "no"); }
    memset(g3_n, 0, sizeof(g3_n)); memset(g3_lut, 0, sizeof(g3_lut)); g3_any = false; ol_reset2();
}
static void vcl_add(float cx, float cy, float cz, const float* tc, const float* vc, double w, uint32_t vh) {
    float az = std::fabs(cz);
    if (az < 4.0f || (fz_prev > 0 && az > 2.0f * fz_prev + 20.0f)) return;   // camera overlays and far scenery
    float R = 0.5f * az + 1e-3f, bd = 1e30f; int b = -1;
    for (int j = 0; j < vcl_n; j++) {
        const VfxCl& q = vcl_cur[j];
        if (q.vh != vh || q.w <= 0) continue;
        float dx = (float)(q.p[0] / q.w) - cx, dy = (float)(q.p[1] / q.w) - cy, dz = (float)(q.p[2] / q.w) - cz, d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < bd) { bd = d2; b = j; }
    }
    if ((b < 0 || bd > R * R) && vcl_n < 6) { b = vcl_n++; vcl_cur[b] = {}; vcl_cur[b].vh = vh; }
    if (b >= 0) { VfxCl& q = vcl_cur[b]; for (int j = 0; j < 3; j++) q.c[j] += tc[j] * vc[j] * w; q.p[0] += cx * w; q.p[1] += cy * w; q.p[2] += cz * w; q.w += w; }
}
static bool scn_held = false;   // stage light held: map draws no longer feed it
static float amb_rgb[3] = {1, 1, 1}, amb_lum = 0.3f, map_t[3] = {1, 1, 1}, map_s[3] = {1, 1, 1}, map_hl = 1.0f;
static double sky_bin[64][4]; static bool amb_ok = false;
struct ToonGround { float p[3]; float c[3]; uint32_t vh; };
static std::vector<ToonGround> toon_ground;
static std::vector<uint32_t> toon_e;
// lightning: the map's lightning sprites light every fighter for a moment
extern int psp_map_bolts;
static float bolt_rgb[3] = {0.85f, 0.93f, 1.0f}, bolt_k = 0, toon_up[3] = {0, 1, 0}, bolt_dir[3] = {0, 1, 0};
static int bolts_on() {
    static int v = 1, n = 0;
    if ((n++ & 255) == 0) { const char* e = getenv("TTT_BOLTS"); v = e && *e ? atoi(e) : 1; }
    return v;
}
// water (disc.h reads MAPD slots 0 and 16): camera under the level tints the whole 3D view and wobbles it (BT3 GfxWater),
// submerged parts seen from above get part of the tint, the surface stops hiding the other side, fighters stay wet a while
extern int psp_map_water, psp_map_gfxw, psp_map_wimm; extern float psp_map_wlevel; extern uint32_t psp_map_wcol, psp_map_wsplash;
static float WAT_SAT = 0.7f, WAT_GAIN = 1.2f, WAT_BLUE = 1.0f, WAT_HIDE = 0.0f, WAT_MESH = 0.0f, WAT_LSIGN = -1.0f;
static int wat_bg = -1, wat_f9 = 0, wat_mesh_hold = 0;
static uint32_t wat_mesh_tex = 0, wat_mt[8]; static int wat_mn[8], wat_mk = 0;
static float WAT_SUB = 0.6f, WAT_SOFT = 1.5f, WAT_WAVE = 5.0f, WAT_SURF = 1.0f, WAT_TOL = 1.0f, WAT_WET = 6.0f, WAT_DARK = 0.25f, WAT_SHINE = 0.35f, WAT_SIGN = 1.0f;
static int wat_stale = 1000, wat_mode = 1, wat_enh = 0;   // wat_enh: Water effects = Enhanced (BT3 + the water shader)
static std::vector<VtxGL> glr_wvtx;
static int wsf_frame = 0;
static float wat_scol[4] = {0, 0, 0, 0};   // BT3 surface colours (EftSurfRt colorA + colorB, 0..1), w = set
static uint32_t wat_enh_ok = 0;   // toon_fno of the last frame the atmosphere drew the Enhanced water
static int wat_on() {
    static int v = 1, n = 0;
    if ((n++ & 63) == 0) {
        const char* e = getenv("TTT_WATER"); v = e && *e ? atoi(e) : 1; wat_enh = v == 3; if (v == 3) v = 2; wat_mode = v;
        WAT_SUB = toon_env("TTT_WATER_SUB", 0.6f); WAT_SOFT = toon_env("TTT_WATER_SOFT", 1.5f); WAT_WAVE = toon_env("TTT_WATER_WAVE", 5.0f);
        WAT_BLUE = toon_env("TTT_WATER_BLUE", 1.0f); WAT_SAT = toon_env("TTT_WATER_SAT", 0.7f); WAT_GAIN = toon_env("TTT_WATER_GAIN", 1.2f); WAT_HIDE = toon_env("TTT_WATER_HIDE", 0.0f); WAT_MESH = toon_env("TTT_WATER_MESH", 0.0f);
        WAT_LSIGN = toon_env("TTT_WATER_LSIGN", -1.0f) < 0 ? -1.0f : 1.0f;
        WAT_SURF = toon_env("TTT_WATER_SURF", 1.0f); WAT_TOL = toon_env("TTT_WATER_TOL", 1.0f); WAT_WET = toon_env("TTT_WATER_WET", 6.0f);
        WAT_DARK = toon_env("TTT_WATER_DARK", 0.25f); WAT_SHINE = toon_env("TTT_WATER_SHINE", 0.35f); WAT_SIGN = toon_env("TTT_WATER_SIGN", 1.0f) < 0 ? -1.0f : 1.0f;
    }
    return v > 0 && psp_map_water && wat_stale < 30;   // only while the battle camera updates
}
// maps with water on the GPU vertex path: water depth per vertex in the flush (TTT_GPU_WATER: 1 on, 0 the old path for the whole scene)
static bool gv_water_on() { static int v = -1; if (v < 0) { const char* e = getenv("TTT_GPU_WATER"); v = e && *e ? atoi(e) : 1; } return v > 0; }
static std::vector<uint8_t> gw_fl;   // per job of the queue: 1 surface, 2 surface depth mask, 4 mesh texture, 8 counted blend texture
// BT3 animated surfaces on the GPU vertex path (TTT_GPU_SURF: 1 on, 0 one job per triangle as before); the water mask redraws their index ranges
static bool gv_surf_on() { static int v = -1; if (v < 0) { const char* e = getenv("TTT_GPU_SURF"); v = e && *e ? atoi(e) : 1; } return gv_water_on() && v > 0; }
struct GwMask { size_t ib, ni; int st; };
static std::vector<GwMask> gw_mask;
static bool wat_cam[8], wat_cam_ok[8];
static float wat_camy[8];
static double wat_sub[9][2];
static float wat_wet[9];
static double wat_last = 0, wat_pu = 0, wat_pv = 0;
extern float psp_wat_cam[8];
static int wat_passes = 0;
static bool wat_done = false, wat_fseen = false, wat_3d = false, wat_split = false, wat_surfseen = false;
struct WatStat { long surf, surf_bl, surf_zw, surf_zt, before, after, fighter, fsub; float fy0, fy1; };
static WatStat wat_st = {0, 0, 0, 0, 0, 0, 0, 0, 1e9f, -1e9f};
// screen post-process before the HUD (after the outline): underwater wobble, rush radial blur (animation VFX4/VFX5, BT3
// EftFollow_UpdateLight), speed blur from the camera motion, ki bloom, impact frames and chromatic aberration on heavy hits
extern float psp_fx_ev[8], psp_cam_m[20];
static float FX_RUSH = 1, FX_BLOOM = 1, FX_IMPACT = 1, FX_SPEED = 1, FX_SHAKE = 1;
static float FX_VFXK = 1.0f, FX_VFXR = 0.9f, FX_VFXGAIN = 1.5f, FX_VFXMAP = 0.6f, FX_RUSHK = 1.0f, FX_BLOOMTH = 0.78f, FX_BLOOMK = 0.13f, FX_BLOOMVFX = 15.0f, FX_B2TH = 0.70f, FX_B2KNEE = 0.12f, FX_B2K = 0.6f, FX_B2VFX = 0.5f, FX_SPEEDK = 0.5f, FX_SPEEDZ = 0.6f, FX_SHAKEK = 1.0f, FX_IMPACTMS = 90.0f;
static void fx_params() {
    static int n = 0;
    if ((n++ & 63) != 0) return;
    FX_RUSH = toon_env("TTT_FX_RUSH", 1); FX_BLOOM = toon_env("TTT_FX_BLOOM", 2); FX_IMPACT = toon_env("TTT_FX_IMPACT", 1);
    FX_SPEED = toon_env("TTT_FX_SPEED", 1); FX_SHAKE = toon_env("TTT_FX_SHAKE", 1);
    FX_RUSHK = toon_env("TTT_FX_RUSHK", 1.0f); FX_BLOOMTH = toon_env("TTT_FX_BLOOMTH", 0.78f); FX_BLOOMK = toon_env("TTT_FX_BLOOMK", 0.13f); FX_VFXMAP = toon_env("TTT_FX_VFXMAP", 0.6f);
    FX_VFXR = toon_env("TTT_FX_VFXR", 0.9f); FX_VFXK = toon_env("TTT_FX_VFXK", 1.0f); FX_VFXGAIN = toon_env("TTT_FX_VFXGAIN", 1.5f);
    {   // Graphics "Motion blur": Off, Low (the old values), Medium, High; explicit TTT_FX_SPEEDK/SPEEDZ/RUSHK still win
        static const float SK[4] = {0.0f, 0.5f, 0.8f, 1.0f}, SZ[4] = {0.0f, 0.6f, 1.0f, 1.6f}, RK[4] = {0.0f, 1.0f, 1.4f, 1.8f};
        int mb = std::max(0, std::min(3, (int)toon_env("TTT_FX_MBLUR", 2)));
        if (!mb) { FX_SPEED = 0; FX_RUSH = 0; }
        if (!getenv("TTT_FX_RUSHK")) FX_RUSHK = RK[mb];
        FX_SPEEDK = getenv("TTT_FX_SPEEDK") ? toon_env("TTT_FX_SPEEDK", 0.5f) : SK[mb];
        FX_SPEEDZ = getenv("TTT_FX_SPEEDZ") ? toon_env("TTT_FX_SPEEDZ", 0.6f) : SZ[mb];
    } FX_BLOOMVFX = toon_env("TTT_FX_BLOOMVFX", 15.0f); FX_SHAKEK = toon_env("TTT_FX_SHAKEK", 1.0f);
    FX_IMPACTMS = toon_env("TTT_FX_IMPACTMS", 90.0f);
    FX_B2TH = toon_env("TTT_FX_BLOOM2TH", 0.70f); FX_B2KNEE = toon_env("TTT_FX_BLOOM2KNEE", 0.12f); FX_B2K = toon_env("TTT_FX_BLOOM2K", 0.6f); FX_B2VFX = toon_env("TTT_FX_BLOOM2VFX", 0.5f);
}
struct FxState { float rush = 0, dash = 0, rcx = 0, rcy = 0, dcx = 240, dcy = 136, ab = 0, bloom = 0, lines = 0, seed = 0, shx = 0, shy = 0, lcx = 240, lcy = 136; int imode = 0; bool rcok = false; };
static FxState fx;
static double fx_fx[9], fx_fy[9], fx_fn[9], fx_px[9], fx_py[9], fx_pn[9];
static GLuint wat_prog = 0, wat_vao = 0; static GLint gwt_u_t = -1, gwt_u_uv = -1, wat_u_src = -1, wat_u_r = -1, wat_u_a = -1, fx_u_b = -1, fx_u_rad = -1, fx_u_dir = -1, fx_u_glow = -1, fx_u_bk = -1, fx_u_imp = -1;
static GLuint fx_bprog = 0, fx_bfbo[2] = {0, 0}, fx_btex[2] = {0, 0}; static int fx_bw = 0, fx_bh = 0; static GLint fx_bu_src = -1, fx_bu_m = -1, fx_bu_lim = -1, fx_bu_tg = -1, fx_bu_glut = -1;
static GLuint fx_pfbo[8] = {0}, fx_ptex[8] = {0}; static int fx_pw[8], fx_ph[8], fx_plv = 0, fx_pvw = 0, fx_pvh = 0;
static const char* WAT_VS = "#version 330 core\nvoid main(){ vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID >> 1) * 4.0 - 1.0); gl_Position = vec4(p, 0.0, 1.0); }\n";
static const char* WAT_FS =
    "#version 330 core\n"
    "uniform sampler2D src; uniform sampler2D glow; uniform vec4 wr; uniform vec4 wa; uniform vec4 fb; uniform vec4 rad; uniform vec4 dirb; uniform vec4 imp; uniform float bk; uniform vec4 gwt; uniform vec4 guv[30]; out vec4 o;\n"
    "vec2 gv(int i){ vec4 v = guv[i >> 1]; return (i & 1) == 0 ? v.xy : v.zw; }\n"
    "float h(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }\n"
    "float n(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0 - 2.0*f); return mix(mix(h(i), h(i + vec2(1.0, 0.0)), f.x), mix(h(i + vec2(0.0, 1.0)), h(i + vec2(1.0, 1.0)), f.x), f.y); }\n"
    "float fbm(vec2 p){ float s = 0.0, a = 0.5; for(int i = 0; i < 3; i++){ s += a*n(p); p = p*2.03 + vec2(17.0, 9.0); a *= 0.5; } return s; }\n"
    "vec3 at(vec2 s){ s = clamp(s, wa.zw + 0.5, wa.zw + wr.xy - 0.5); return texture(src, s / vec2(textureSize(src, 0))).rgb; }\n"
    "void main(){\n"
    "  vec2 f = gl_FragCoord.xy, s = f + imp.zw * wr.w; float sc = wr.w;\n"
    "  if(wa.x > 0.0){ vec2 p = (f - wa.zw) / sc / 70.0; float t = wr.z;\n"
    "    vec2 w = vec2(fbm(p + vec2(0.0, t*0.21)), fbm(p + vec2(5.2, 1.3) - vec2(t*0.17, 0.0)));\n"
    "    vec2 off = vec2(fbm(p + 1.7*w + vec2(t*0.11, 2.0)), fbm(p + 1.7*w + vec2(8.3, -t*0.13))) / 0.875 * 2.0 - 1.0;\n"
    "    s += off * wa.x * sc; }\n"
    "  if(gwt.a > 0.5){ vec2 q = clamp((s - wa.zw) / wr.xy, 0.0, 0.99999); int c = 0, r = 0;\n"
    "    for(int i = 1; i < 9; i++) if(q.x >= floor(float(i) * 512.0 / 9.0) / 512.0) c = i;\n"
    "    for(int i = 1; i < 5; i++) if(q.y >= floor(float(i) * 448.0 / 5.0) / 448.0) r = i;\n"
    "    float x0 = floor(float(c) * 512.0 / 9.0) / 512.0, x1 = floor(float(c + 1) * 512.0 / 9.0) / 512.0, y0 = floor(float(r) * 448.0 / 5.0) / 448.0, y1 = floor(float(r + 1) * 448.0 / 5.0) / 448.0;\n"
    "    float a = (q.x - x0) / (x1 - x0), b = (q.y - y0) / (y1 - y0);\n"
    "    vec2 t0 = gv(r * 10 + c), t1 = gv(r * 10 + c + 1), b0 = gv(r * 10 + 10 + c), b1 = gv(r * 10 + 11 + c);\n"
    "    vec2 uv = a + b <= 1.0 ? t0 + a * (t1 - t0) + b * (b0 - t0) : b1 + (1.0 - a) * (b0 - b1) + (1.0 - b) * (t1 - b1);\n"
    "    s = wa.zw + uv * wr.xy; }\n"
    "  vec3 c;\n"
    "  if(fb.y > 0.0){ vec2 d = normalize(s - (wa.zw + wr.xy*0.5) + 0.001) * fb.y * sc; c = vec3(at(s + d).r, at(s).g, at(s - d).b); } else c = at(s);\n"
    "  if(gwt.a > 0.5) c = min(c * gwt.rgb * (255.0 / 128.0), vec3(1.0));\n"
    "  if(rad.z > 0.0){ vec2 d = s - rad.xy; vec3 acc = c; for(int i = 1; i < 12; i++) acc += at(s - d * (float(i) / 11.0) * 0.09 * rad.w); acc /= 12.0;\n"
    "    float r = clamp(length(d) / (0.6 * length(wr.xy)), 0.0, 1.0);\n"
    "    float a = dirb.x > 0.5 ? smoothstep(0.06, 0.9, r) : (r < 0.5 ? mix(0.25, 0.5, r*2.0) : mix(0.5, 1.0, r*2.0 - 1.0));\n"
    "    c = mix(c, acc, rad.z * a); }\n"
    "  if(bk > 0.0) c += texture(glow, (f - wa.zw) / wr.xy).rgb * bk;\n"
    "  else if(bk < 0.0){ vec3 g = min(texture(glow, (f - wa.zw) / wr.xy).rgb * -bk, vec3(1.0)); c = 1.0 - (1.0 - c) * (1.0 - g); }\n"
    "  if(fb.y > 0.0){ vec2 q = (f - wa.zw) / wr.xy - 0.5; c *= 1.0 - fb.w * smoothstep(0.25, 0.75, length(q)); }\n"
    "  if(imp.x > 0.0){ vec2 q = (f - wa.zw) / sc - dirb.zw; float an = (atan(q.y, q.x) + 3.14159265) / 6.2831853 * 180.0, cell = floor(an);\n"
    "    float hv = h(vec2(cell, imp.y)), r = length(q) / 272.0, st = 0.18 + 0.25*h(vec2(cell, imp.y + 7.0));\n"
    "    float wd = mix(0.05, 0.18, hv) * smoothstep(st, st + 0.6, r), sf = 0.06 + wd * 0.5, dd = abs(fract(an) - 0.5);\n"
    "    float ln = (1.0 - smoothstep(wd * 0.25, wd * 0.25 + sf, dd)) * step(0.72, hv);\n"
    "    c = mix(c, vec3(1.0), ln * smoothstep(st, st + 0.2, r) * (1.0 - smoothstep(0.9, 1.4, r)) * imp.x * 0.45); }\n"
    "  float l = dot(c, vec3(0.3, 0.59, 0.11));\n"
    "  if(fb.x > 0.5 && fb.x < 1.5) c = vec3(1.0 - smoothstep(0.38, 0.46, l));\n"
    "  else if(fb.x > 1.5) c = mix(vec3(0.05, 0.0, 0.0), vec3(1.0, 0.97, 0.93), smoothstep(0.42, 0.5, l));\n"
    "  o = vec4(c, 1.0);\n"
    "}\n";
static const char* FX_BFS =   // bloom: bright pass (m 0) and separable 9-tap blur (m 1 horizontal, m 2 vertical)
    "#version 330 core\n"
    "uniform sampler2D src; uniform vec4 m; uniform vec2 lim; uniform vec2 tg; uniform vec4 glut[32]; out vec4 o;\n"
    "void main(){\n"
    "  vec2 ts = vec2(textureSize(src, 0));\n"
    "  if(m.x > 5.5){ ivec2 b = ivec2(gl_FragCoord.xy * m.z), lm = ivec2(lim * ts); vec3 c = vec3(0.0);\n"
    "    for(int i = 0; i < 4; i++){ vec4 s = texelFetch(src, min(b + ivec2(i & 1, i >> 1), lm), 0); int id = int(s.a*255.0 + 0.5) & 127; c += s.rgb * glut[id >> 2][id & 3]; }\n"
    "    o = vec4(c * 0.25, 1.0); return; }\n"
    "  if(m.x > 2.5){ vec2 uv = gl_FragCoord.xy / tg, hp = 0.5 / ts; vec3 c;\n"
    "    if(m.x < 3.5){ uv = min(gl_FragCoord.xy * m.z / ts, lim); c = vec3(0.0); for(int i = 0; i < 4; i++) c += min(texture(src, min(uv + (vec2(float(i & 1), float(i >> 1)) * 2.0 - 1.0) / ts, lim)).rgb, vec3(1.0)); c *= 0.25;\n"
    "      float l = mix(dot(c, vec3(0.2126, 0.7152, 0.0722)), max(max(c.r, c.g), c.b), 0.5), s = clamp(l - m.y + m.w, 0.0, 2.0 * m.w); s = s * s / (4.0 * m.w + 1e-4);\n"
    "      o = vec4(c * (max(s, l - m.y) / max(l, 1e-4)), 1.0); return; }\n"
    "    if(m.x < 4.5){ c = texture(src, uv).rgb * 4.0 + texture(src, uv - hp).rgb + texture(src, uv + hp).rgb + texture(src, uv + vec2(hp.x, -hp.y)).rgb + texture(src, uv - vec2(hp.x, -hp.y)).rgb; o = vec4(c / 8.0, 1.0); return; }\n"
    "    c = texture(src, uv + vec2(-2.0 * hp.x, 0.0)).rgb + texture(src, uv + vec2(2.0 * hp.x, 0.0)).rgb + texture(src, uv + vec2(0.0, 2.0 * hp.y)).rgb + texture(src, uv + vec2(0.0, -2.0 * hp.y)).rgb;\n"
    "    c += (texture(src, uv + hp).rgb + texture(src, uv - hp).rgb + texture(src, uv + vec2(hp.x, -hp.y)).rgb + texture(src, uv - vec2(hp.x, -hp.y)).rgb) * 2.0;\n"
    "    o = vec4(c / 12.0 * m.y, 1.0); return; }\n"
    "  if(m.x < 0.5){ vec2 uv = (gl_FragCoord.xy * m.z) / ts; vec3 c = vec3(0.0); for(int i = 0; i < 4; i++){ vec2 q = uv + (vec2(float(i & 1), float(i >> 1)) - 0.5) * m.z / ts; c += texture(src, min(q, lim)).rgb; } c *= 0.25;\n"
    "    float l = max(max(c.r, c.g), c.b); o = vec4(c * smoothstep(m.y, m.y + 0.2, l), 1.0); return; }\n"
    "  vec2 d = (m.x < 1.5 ? vec2(1.0, 0.0) : vec2(0.0, 1.0)) / ts; vec2 uv = gl_FragCoord.xy / ts;\n"
    "  const float w[5] = float[5](0.227, 0.195, 0.122, 0.054, 0.016);\n"
    "  vec3 c = texture(src, uv).rgb * w[0]; for(int i = 1; i < 5; i++) c += (texture(src, uv + d * float(i) * 1.5).rgb + texture(src, uv - d * float(i) * 1.5).rgb) * w[i];\n"
    "  o = vec4(c, 1.0);\n"
    "}\n";
static GLuint glr_copy_fb(DestGL* d);
static bool fx_bloom_make(int w, int h) {
    if (fx_bprog == 0xFFFFFFFFu) return false;
    if (!fx_bprog) {
        fx_bprog = gl_program(WAT_VS, FX_BFS);
        if (!fx_bprog) { printf("fx: no pude preparar el programa de bloom\n"); fx_bprog = 0xFFFFFFFFu; return false; }
        fx_bu_src = glGetUniformLocation(fx_bprog, "src"); fx_bu_m = glGetUniformLocation(fx_bprog, "m"); fx_bu_lim = glGetUniformLocation(fx_bprog, "lim"); fx_bu_tg = glGetUniformLocation(fx_bprog, "tg"); fx_bu_glut = glGetUniformLocation(fx_bprog, "glut");
        glGenFramebuffers(2, fx_bfbo); glGenTextures(2, fx_btex);
    }
    if (fx_bw != w || fx_bh != h) {
        for (int k = 0; k < 2; k++) {
            glBindTexture(GL_TEXTURE_2D, fx_btex[k]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, fx_bfbo[k]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fx_btex[k], 0);
        }
        fx_bw = w; fx_bh = h;
    }
    return true;
}
#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#define GL_HALF_FLOAT 0x140B
#endif
static bool fx_pyr_make(int w, int h) {   // soft bloom: half size bright pass and a dual filter pyramid down to ~16 px (16-bit float)
    if (!fx_bloom_make(std::max(w / 4, 1), std::max(h / 4, 1))) return false;
    if (fx_pvw == w && fx_pvh == h) return fx_plv > 0;
    if (fx_plv) { glDeleteFramebuffers(fx_plv, fx_pfbo); glDeleteTextures(fx_plv, fx_ptex); }
    fx_plv = 0; fx_pvw = w; fx_pvh = h;
    int lw = w / 2, lh = h / 2;
    while (fx_plv < 8 && lw >= 16 && lh >= 8) { fx_pw[fx_plv] = lw; fx_ph[fx_plv] = lh; fx_plv++; lw /= 2; lh /= 2; }
    glGenFramebuffers(fx_plv, fx_pfbo); glGenTextures(fx_plv, fx_ptex);
    for (int k = 0; k < fx_plv; k++) {
        glBindTexture(GL_TEXTURE_2D, fx_ptex[k]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, fx_pw[k], fx_ph[k], 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, fx_pfbo[k]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fx_ptex[k], 0);
    }
    printf("fx: bloom suave, %d niveles desde %dx%d\n", fx_plv, fx_plv ? fx_pw[0] : 0, fx_plv ? fx_ph[0] : 0);
    return fx_plv > 0;
}
// Cel BT3: BT3's object glow (ObjGlow_Draw / GfxPost_DrawGlow): the fighter's pixels weighted by their material's glow strength
// (alpha id -> g3_lut), blurred at about 1/8 of the view and added back to the frame (TTT_CEL_GLOW=0 off, TTT_CEL_GLOWK strength)
static void g3_pass(DestGL* d, int w, int h) {
    for (int j = 0; j < 8; j++) for (int b = 0; b < 16; b++) if ((ol_bg[j] >> b) & 1) g3_lut[j * 16 + b] = 0;   // ids also used by the scenery
    if (g3_lut[127] && toon_env("TTT_CEL_GLOW127", 1.0f) > 0.5f) {   // id 0x7F = alpha 0xFF, the plain scenery (far LOD fighters use it)
        static int gl127 = 0; if (gl127 < 4) { gl127++; printf("cel: brillo del id 0x7F ignorado (fuerza %d), es el escenario\n", g3_lut[127]); }
        g3_lut[127] = 0;
    }
    float k = toon_env("TTT_CEL_GLOWK", 0.5f);
    g3_n[3]++;
    if (!fx_pyr_make(w, h) || fx_plv < 4 || k <= 0) { static int el = 0; if (el < 3) { el++; printf("cel: brillo BT3 sin pasada (niveles %d, fuerza %.2f)\n", fx_plv, k); } return; }
    float lut[128];
    for (int i = 0; i < 128; i++) lut[i] = g3_lut[i] / 128.0f;
    glActiveTexture(GL_TEXTURE0 + 4);
    GLuint t = glr_copy_fb(d);
    glBindTexture(GL_TEXTURE_2D, t);
    glUseProgram(fx_bprog);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_STENCIL_TEST); glStencilMask(0); glDisable(GL_BLEND);
    glUniform4fv(fx_bu_glut, 32, lut);
    glUniform2f(fx_bu_lim, (w - 0.5f) / d->w, (h - 0.5f) / d->h);
    glUniform1i(fx_bu_src, 4);
    glViewport(0, 0, fx_pw[0], fx_ph[0]);
    glBindFramebuffer(GL_FRAMEBUFFER, fx_pfbo[0]);
    glUniform2f(fx_bu_tg, (float)fx_pw[0], (float)fx_ph[0]);
    glUniform4f(fx_bu_m, 6.0f, 0, (float)w / fx_pw[0], 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0 + 5);
    glUniform1i(fx_bu_src, 5);
    int top = std::min(4, fx_plv - 1);
    for (int l = 1; l <= top; l++) {
        glViewport(0, 0, fx_pw[l], fx_ph[l]);
        glBindFramebuffer(GL_FRAMEBUFFER, fx_pfbo[l]);
        glBindTexture(GL_TEXTURE_2D, fx_ptex[l - 1]);
        glUniform2f(fx_bu_tg, (float)fx_pw[l], (float)fx_ph[l]);
        glUniform4f(fx_bu_m, 4.0f, 0, 0, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_ONE, GL_ONE);
    for (int l = top - 1; l >= 2; l--) {
        glViewport(0, 0, fx_pw[l], fx_ph[l]);
        glBindFramebuffer(GL_FRAMEBUFFER, fx_pfbo[l]);
        glBindTexture(GL_TEXTURE_2D, fx_ptex[l + 1]);
        glUniform2f(fx_bu_tg, (float)fx_pw[l], (float)fx_ph[l]);
        glUniform4f(fx_bu_m, 5.0f, 1.0f, 0, 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    glViewport(0, 0, w, h);
    glBindTexture(GL_TEXTURE_2D, fx_ptex[2]);
    glUniform2f(fx_bu_tg, (float)w, (float)h);
    glUniform4f(fx_bu_m, 5.0f, k / (float)(top - 1), 0, 0);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glActiveTexture(GL_TEXTURE0);
    static int logs = 0;
    if (logs < 3) { logs++; int n = 0; for (int i = 0; i < 128; i++) n += g3_lut[i] != 0; printf("cel: brillo BT3 de los luchadores, %d capas\n", n); }
}
// Extra 2, BT3 1:1 (decomp gfxm_a.c): StgDepthTint (additive tint by the alpha byte: 0xFF = scenery, fighter ids get curve 0) and
// StgGlare (frame weighted 1.0 scenery / alpha fighters, halved to 64x64, 10 blur passes, added at level/128; level from the sun on screen)
extern float psp_map_sun[4]; extern int psp_map_tint[2][6], psp_map_glare[7], psp_map_fog[5], psp_map_water; extern float psp_map_wlevel;
extern int psp_map_haze[10]; extern float psp_map_haze_follow, psp_map_top;
static float at_sun[4] = {0, 0, 0, 0}, at_zk[6] = {0, 1, 0, 0, 1, 1}, at_cam_y = 0;
static float aw_q[17 * 4]; static bool aw_ok = false;
struct WRing { double sx, sz, n, x0, x1, z0, z1; float y0, y1; uint32_t f; };
static WRing wr_f[9];
static bool ws_on() { static int v = 1, n = 0; if ((n++ & 63) == 0) v = toon_env("TTT_FX_WSURF", 0) > 0.5f; return v && psp_map_water; }   // water surface: parked for phase H (BT3 maps read directly), off
static bool at_ok = false, at_full = true; static void atm_pick(uint32_t dir); bool psp_sel3d_open(); static double at_dt = 1.0 / 60.0;
static int at_level() { static int v = 1, n = 0; if ((n++ & 63) == 0) v = std::max(0, std::min(2, (int)toon_env("TTT_FX_ATM", 1))); return v; }
static int at_glare() { static int v = 1, n = 0; if ((n++ & 63) == 0) v = std::max(0, std::min(2, (int)toon_env("TTT_FX_SUN", 1))); return v; }
static bool at_on_flag(int f) { return (f & 1) || (f & 2); }   // his converter keeps bit 1 but drops bit 0 (BT3 3 -> 2)
static int rn_enh = 1; static uint32_t rn_fno = 0, rn_drawn = 0; static float rn_flash = 0.0f, rn_wind[2] = {0.0f, 0.0f};   // Enhanced rain: frame (+1) the storm asked for it, frame (+1) the atmosphere drew it
static const int WSR_N = 24; static float wsr_q[4 * WSR_N]; static uint32_t wsr_f = 0;   // Enhanced splash ripples for the water shader: x, z, radius, strength
int psp_charge_dark = 0; static float chg_k = 0.0f;
static uint32_t cdk_scn_f = 0xFFFFFFFFu, cdk_done_f = 0xFFFFFFFFu;   // charge darkness drawn before the effects batch: frame of the scenery batch, frame it was drawn   // SZ charge phase 2 / sparking: scene darkening, eased
static int wt_enh = 1; static float rn_wet = 0.0f;   // Wet surfaces: new wet model on fighters and wet ground while it rains; rain wetness 0..1
int psp_spark_n = 0; float psp_spark[8][10];   // sparking fighters from the game thread: feet xyz, body centre xyz, aura colour rgb, slot
static float spk_f[8], spk_c[8][3], spk_p[8][6], spk_a[8][4]; static bool spk_any = false;

struct SpkFlame { float ox, oy, t, v, age, life, sw; int slot, id; }; static SpkFlame spk_fl[48]; static int spk_fn = 0; static float spk_gpu[48][4];   // flame emitter: world particles, screen rows for the shader
   // flame fade per slot, last aura colour, feet/centre world, screen centre/half height/radius
static bool rn_live() { return rn_enh && rn_fno && toon_fno + 1 - rn_fno <= 2u; }
static bool atm_on() { return (at_level() > 0 || ws_on() || (wat_enh && psp_map_wimm) || rn_live() || psp_spark_n > 0 || spk_any || spk_fn > 0 || psp_charge_dark || chg_k > 0.0f || (at_glare() > 0 && psp_map_glare[6] && at_on_flag(psp_map_glare[0]))); }
static const char* ATM_FS =
    "#version 330 core\n"
    "uniform sampler2D src; uniform sampler2D dep; uniform sampler2D aux; uniform sampler2D ray; uniform sampler2D st; uniform sampler2D hal;\n"
    "uniform int cutm; uniform vec4 fbb; bool C(int b){ return (cutm & b) != 0; }\n"
    "uniform vec4 am; uniform vec4 vw; uniform vec4 sun; uniform vec4 zk; uniform vec4 zk2; uniform vec4 ap; uniform vec4 gp; uniform vec4 bz; uniform vec4 tn; uniform vec4 tb; uniform vec4 fid[2]; uniform vec4 tl[64]; uniform vec4 pb; uniform vec4 wq[17]; uniform sampler2D fgt; uniform vec4 fq; uniform vec4 tl2[64]; uniform vec4 hzq; uniform vec4 hzc; uniform sampler2D hzj; uniform vec4 tl3[64]; uniform vec4 wm; uniform vec4 wc; uniform vec4 rq; uniform vec4 rw; uniform vec4 rx; uniform vec4 wr[24]; uniform vec4 ck; uniform vec4 skc[8]; uniform vec4 idw[32]; uniform vec4 fpt[48]; uniform float cka; out vec4 o;\n"
    "float dist(vec2 p){ float d = texelFetch(dep, ivec2(clamp(p, vec2(0.0), vw.xy - 1.0)), 0).r; float k = (d*65535.0 - zk.x)/zk.y; float v = (zk.w - k*zk2.y)/(k*zk2.x - zk.z); return (v != v || d >= 1.0) ? 1e9 : abs(v); }\n"
    "bool sky(vec2 q){ float d = texelFetch(dep, ivec2(clamp(q, vec2(0.0), vw.xy - 1.0)), 0).r; return d >= 1.0 || d <= 0.0; }\n"
    "vec3 w2v(vec3 w, out float dz){ vec3 e = w.x*wq[0].xyz + w.y*wq[1].xyz + w.z*wq[2].xyz + wq[3].xyz; vec3 v = e.x*wq[4].xyz + e.y*wq[5].xyz + e.z*wq[6].xyz + vec3(wq[4].w, wq[5].w, wq[6].w); dz = -v.z; return v; }\n"
    "vec2 w2s(vec3 w, out float dz){ vec3 v = w2v(w, dz); vec2 n = vec2((v.x*wq[7].x + v.z*wq[7].z + wq[8].x)/dz, (v.y*wq[7].y + v.z*wq[7].w + wq[8].y)/dz)*wq[8].zw; return (wq[9].zw + wq[9].xy*n)*wq[3].w; }\n"
    "vec3 eray(vec2 p){ vec2 n = ((p/wq[3].w) - wq[9].zw)/wq[9].xy/wq[8].zw; vec3 v = vec3((n.x + wq[7].z - wq[8].x)/wq[7].x, (n.y + wq[7].w - wq[8].y)/wq[7].y, -1.0);\n"
    "  vec3 e = vec3(dot(v, wq[4].xyz), dot(v, wq[5].xyz), dot(v, wq[6].xyz)); return normalize(vec3(dot(e, wq[0].xyz), dot(e, wq[1].xyz), dot(e, wq[2].xyz))); }\n"
    "float hn(vec2 q){ return fract(sin(dot(q, vec2(127.1, 311.7)))*43758.5453); }\n"
    "float vn(vec2 q){ vec2 i = floor(q), f = fract(q); f = f*f*(3.0 - 2.0*f); return mix(mix(hn(i), hn(i + vec2(1.0, 0.0)), f.x), mix(hn(i + vec2(0.0, 1.0)), hn(i + vec2(1.0, 1.0)), f.x), f.y); }\n"
    "vec2 wgrad(vec2 x, float tm, float fd){ vec2 g = vec2(0.0), w = x + vec2(vn(x*0.021 + tm*0.05), vn(x*0.021 - tm*0.04 + 17.0))*18.0;   // swells 62..4 units, warped by noise, random phases\n"
    "  for(int i = 0; i < 6; i++){ float a = float(i)*2.39996 + 0.7 + (hn(vec2(float(i), 3.1)) - 0.5)*0.9, L = 62.0*pow(0.58, float(i)), k = 6.2832/L; vec2 d = vec2(cos(a), sin(a));\n"
    "    g += d*k*cos(dot(d, w)*k + tm*sqrt(9.8*k)*1.1 + hn(vec2(float(i), 7.7))*6.2832)*L*0.010*(i > 2 ? fd : 1.0); }\n"
    "  vec2 u = w*0.35 + tm*0.3; float n0 = vn(u); return g + vec2(n0 - vn(u + vec2(0.6, 0.0)), n0 - vn(u + vec2(0.0, 0.6)))*0.25*fd; }\n"
    "bool wmask(vec2 q){ return int(texelFetch(src, ivec2(clamp(q, vec2(0.0), vw.xy - 1.0)), 0).a*255.0 + 0.5) == 126; }\n"
    "vec3 ssr(vec3 H, vec3 r, float s, float k, bool nw, out bool hit){ vec3 rc = wq[11].rgb; float sp = 0.0; hit = false;   // screen-space reflection: march, then a binary refinement of the hit (no stepped copies)\n"
    "  for(int i = 0; i < 22; i++){ float qd; vec2 q = w2s(H + r*s, qd); if(qd <= 0.0 || q.x < 0.0 || q.y < 0.0 || q.x >= vw.x || q.y >= vw.y) break;\n"
    "    float sd = dist(q); if(qd > sd && qd - sd < max(8.0, (s - sp)*2.0) && !(nw && wmask(q))){ float a = sp, b = s;\n"
    "      for(int j = 0; j < 6; j++){ float m = 0.5*(a + b), md; vec2 mq = w2s(H + r*m, md); if(md > dist(mq)) b = m; else a = m; }\n"
    "      float bd; vec2 bq = w2s(H + r*b, bd); rc = texelFetch(src, ivec2(clamp(bq, vec2(0.0), vw.xy - 1.0)), 0).rgb; hit = true; break; }\n"
    "    sp = s; s *= k; }\n"
    "  return rc; }\n"
    "vec3 water(vec2 p, vec3 c, bool msk){\n"
    "  vec3 E = vec3(wq[0].w, wq[1].w, wq[2].w), d = eray(p); float wl = wq[10].x;\n"
    "  if(msk && E.y > wl + 0.2 && d.y < -1e-4){ float t = (wl - E.y)/d.y, tm = wq[10].z; vec3 H = E + d*t; float fd = 1.0/(1.0 + t/900.0); vec2 g = wgrad(H.xz, tm, fd)*(0.5 + 0.5*fd);   // seen from below: Snell window, total reflection outside it\n"
    "    vec3 n = normalize(vec3(-g.x, 1.0, -g.y)); float cs = clamp(dot(-d, n), 0.0, 1.0), win = smoothstep(0.6, 0.72, cs);\n"
    "    vec2 q = p + vec2(g.x, -g.y)*40.0*wq[3].w; vec3 ab = texelFetch(src, ivec2(clamp(q, vec2(0.0), vw.xy - 1.0)), 0).rgb*mix(vec3(1.0), wm.yzw*1.5, 0.35);\n"
    "    vec3 tr = wc.rgb*(1.1 + 0.5*vn(H.xz*0.08 + tm*0.2)); float e = exp(-pow((cs - 0.66)/0.03, 2.0));\n"
    "    vec3 r = mix(tr, ab*1.15, win) + vec3(0.85, 0.95, 1.0)*(e*0.35 + pow(max(vn(H.xz*0.5 + tm*0.6) - 0.55, 0.0)*2.2, 2.0)*0.3*win);\n"
    "    return mix(r, wc.rgb, 1.0 - exp(-t/600.0)); }\n"
    "  if(E.y >= wl - 0.2 || d.y < 1e-4) return c;\n"
    "  float t = (wl - E.y)/d.y, hd; vec3 H = E + d*t; w2v(H, hd);\n"
    "  if(!msk && (!(hd < dist(p)*1.002 + 1.0) || t > 30000.0)) return c;\n"
    "  float tm = wq[10].z, fade = 1.0/(1.0 + t/900.0); vec2 g = wgrad(H.xz, tm, fade)*(0.5 + 0.5*fade), foam = vec2(0.0); float th = 1e4;\n"
    "  for(int k = 0; k < 4; k++){ vec4 R = wq[13 + k]; if(R.w <= 0.0) continue; vec2 dv = H.xz - R.xy; float dd = length(dv); vec2 rd = dv/max(dd, 1e-3);\n"
    "    for(int i = 0; i < 3; i++){ float ph = fract(tm*0.45 + float(i)/3.0), rr = R.z + ph*7.0, w = exp(-pow((dd - rr)/0.45, 2.0))*(1.0 - ph)*R.w;\n"
    "      g += rd*w*0.55; foam.x += w; } }\n"
    "  for(int k = 0; k < 24; k++){ vec4 R = wr[k]; if(R.w <= 0.0) continue; vec2 dv = H.xz - R.xy; float dd = length(dv); if(dd > R.z*1.2 + 4.0) continue; vec2 rd = dv/max(dd, 1e-3);   // splash ripples\n"
    "    for(int i = 0; i < 3; i++){ float rr = R.z*(1.0 - 0.28*float(i)), w = exp(-pow((dd - rr)/max(0.3, R.z*0.07), 2.0))*R.w*(1.0 - 0.3*float(i)); g += rd*w*0.9*cos(float(i)*3.1416); foam.x += w*0.5; } foam.x += R.w*0.35*exp(-dd*dd/max(0.5, R.z*R.z*0.25))*(0.6 + 0.4*vn(H.xz*1.7 + tm)); }\n"
    "  if(rq.x > 0.5) for(int k = 0; k < 3; k++){ vec2 gg = H.xz/3.5 + float(k)*vec2(0.5, 0.37), ci = floor(gg); float h = hn(ci + float(k)*19.0), ph = fract(tm*1.4 + h);   // rain ripples\n"
    "    vec2 dv = (fract(gg) - 0.3 - 0.4*vec2(h, fract(h*13.1)))*3.5; float dd = length(dv), w = exp(-pow((dd - ph*1.3)/0.16, 2.0))*(1.0 - ph)*fade; g += dv/max(dd, 1e-3)*w*0.5; foam.x += w*0.25; }\n"
    "  vec3 n = normalize(vec3(-g.x, -1.0, -g.y)), r = reflect(d, n);\n"
    "  if(msk){ th = max(dist(p) - hd, 0.0); vec2 q = p + vec2(g.x, -g.y)*min(th, 60.0)*0.6*wq[3].w;   // refraction of the bed (only where the offset pixel is water too), stage tint, absorption by depth\n"
    "    vec3 rf = wmask(q) ? texelFetch(src, ivec2(clamp(q, vec2(0.0), vw.xy - 1.0)), 0).rgb : c;\n"
    "    rf *= mix(vec3(1.0), wm.yzw*1.6, 0.35);\n"
    "    c = mix(rf, wc.rgb, (1.0 - exp(-th/max(wc.w, 1.0)))*0.64); }\n"
    "  bool hit; vec3 rc = ssr(H, r, 2.0 + 2.0*hn(p), 1.3, true, hit);\n"
    "  if(!hit){ float qd; vec2 q = w2s(H + r*20000.0, qd); if(qd > 0.0 && q.x >= 0.0 && q.y >= 0.0 && q.x < vw.x && q.y < vw.y && sky(q)) rc = texelFetch(src, ivec2(q), 0).rgb; }\n"
    "  float cs = clamp(dot(-d, n), 0.0, 1.0), F = clamp(0.02 + 0.98*pow(1.0 - cs, 5.0) + 0.1, 0.0, 1.0)*wq[10].w*(1.0 - smoothstep(6000.0, 15000.0, t));\n"
    "  c = mix(c, rc, F);\n"
    "  if(wq[11].w > 0.5){ vec3 L = normalize(wq[12].xyz - H); float rl = max(dot(r, L), 0.0); c += vec3(1.0, 0.95, 0.85)*(pow(rl, 350.0)*2.2 + pow(rl, 40.0)*0.12)*wq[12].w; }\n"
    "  c += vec3(0.9, 0.95, 1.0)*min(foam.x, 1.0)*0.28;\n"
    "  if(msk){ float fo = (1.0 - smoothstep(0.0, 5.0, th))*(0.55 + 0.45*vn(H.xz*0.6 + tm*0.8));   // waterline foam: shores and bodies just under the surface\n"
    "    c = mix(c, vec3(0.92, 0.96, 1.0), fo*0.6); }\n"
    "  return c; }\n"
    "float lum(vec3 c){ return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    "int byt(float a){ return int(a*255.0 + 0.5); }\n"
    "float curve(int b){ return tl[b >> 2][b & 3]; }\n"
    "float curve2(int b){ return tl2[b >> 2][b & 3]; }\n"
    "float curve3(int b){ return tl3[b >> 2][b & 3]; }\n"
    "vec2 hzj_at(int i){ int n = int(hzq.z + 0.5); i = min(i, n*int(hzq.w + 0.5) - 1); return texelFetch(hzj, ivec2(i % n, i / n), 0).rg; }\n"
    "float kz(){ float fz = texelFetch(st, ivec2(0, 0), 0).y; return fq.y > 0.0 ? fq.y : fz > 0.5 ? -fq.y/fz : 0.0; }\n"
    "int dby(vec2 p){ float k = kz(); if(k <= 0.0) return 0;\n"
    "  if(sky(p)) return 255; float d = max(dist(p)*k, 0.3); return int(clamp(floor(255.0 - (fq.z/d + fq.w)/256.0), 0.0, 255.0)); }\n"
    "int slot_of(float a){ int id = int(a*255.0 + 0.5) & 127; return int(idw[id >> 2][id & 3] + 0.5); }\n"
    "bool fighter(float a){ int id = int(a*255.0 + 0.5) & 127; int w = int(fid[id >> 6][(id >> 4) & 3] + 0.5); return ((w >> (id & 15)) & 1) != 0; }\n"
    "float rnh(vec3 q){ return fract(sin(dot(q, vec3(127.1, 311.7, 74.7)))*43758.5453); }\n"
    "float rsplash(vec2 xz, float gs, float tm, float pw){ float s = 0.0;   // drops hitting a flat surface: crown flash then a thin ring\n"
    "  for(int k = 0; k < 2; k++){ vec2 g = xz/gs + float(k)*vec2(0.5, 0.37), ci = floor(g); float h = rnh(vec3(ci, 5.0 + float(k))), ph = fract(tm*1.7 + h);\n"
    "    float dd = length(fract(g) - 0.3 - 0.4*vec2(h, fract(h*13.1)))*gs, r = gs*(0.03 + ph*0.22), f = (1.0 - ph)*(1.0 - ph);\n"
    "    s += (exp(-pow((dd - r)/max(0.06, pw), 2.0))*0.7 + exp(-dd*dd/max(0.02, pw*pw))*(1.0 - smoothstep(0.0, 0.2, ph))*1.3)*f; }\n"
    "  return s; }\n"
    "vec3 rain(vec2 p, vec3 c, bool fg, float ts, vec3 E, vec3 d, vec3 P, vec3 N, float apx){\n"
    "  float tm = wq[10].z; vec3 rc = mix(c, wq[11].rgb, 0.4)*0.6 + vec3(0.035)*(1.0 + rq.y*0.6); bool sk = sky(p);   // drops take the colour of what lies behind them\n"
    "  if(rx.x > 0.0 && !sk && !fg && byt(texelFetch(src, ivec2(p), 0).a) != 126 && ts < 4000.0){ float fl = smoothstep(0.6, 0.9, abs(N.y));   // wet ground: darker, mirror-like where flat, puddles\n"
    "    float pd = smoothstep(0.25, 0.85, 0.65*vn(P.xz*0.006) + 0.35*vn(P.xz*0.017 + 3.0))*fl, k = rx.x*(1.0 - smoothstep(1500.0, 4000.0, ts));   // the whole floor wet, broad soft puddles\n"
    "    c *= 1.0 - 0.28*k*(0.4 + 0.6*fl);\n"
    "    vec3 Nf = dot(N, d) > 0.0 ? -N : N; float ny = abs(N.y), fr = smoothstep(0.35, 0.9, ny);   // reflection on flat and inclined surfaces, weaker as they get steep\n"
    "    if(ny < 0.97){ float ac = abs(Nf.x) > abs(Nf.z) ? P.z : P.x, nf = 1.0 - smoothstep(0.2, 0.6, ts*apx);   // slopes and walls: soft moving sheen and water running down, fixed in the world\n"
    "      float sh = vn(vec2(ac*0.04, P.y*0.025 - tm*0.35)), rv = smoothstep(0.8, 0.95, vn(vec2(ac*0.5, P.y*0.06 - tm*1.4)))*smoothstep(0.45, 0.8, vn(vec2(ac*0.08, P.y*0.02 - tm*0.3) + 5.0));\n"
    "      c = mix(c, c*0.75 + mix(wq[11].rgb, vec3(0.8, 0.85, 0.9), 0.3)*0.4, k*(1.0 - fl)*(0.1 + 0.16*sh));\n"
    "      c += mix(c, wq[11].rgb, 0.6)*rv*k*(1.0 - fl)*nf*(ny > 0.3 ? 0.35 : 0.22); }\n"
    "    if(fr > 0.0){ vec2 g = vec2(vn(P.xz*0.9 + tm*1.3) - 0.5, vn(P.xz*0.9 - tm*1.1 + 7.0) - 0.5)*mix(0.08, 0.02, pd);\n"
    "      vec3 n = normalize(mix(Nf, vec3(0.0, -1.0, 0.0), fl) + vec3(g.x, 0.0, g.y)), r = reflect(d, n); bool hit; vec3 rc2 = ssr(P + n*0.5, r, 1.0 + 1.5*hn(p), 1.33, false, hit);\n"
    "      if(!hit){ float qd; vec2 q = w2s(P + r*20000.0, qd); if(qd > 0.0 && q.x >= 0.0 && q.y >= 0.0 && q.x < vw.x && q.y < vw.y && sky(q)) rc2 = texelFetch(src, ivec2(q), 0).rgb; }\n"
    "      float F = 0.03 + 0.97*pow(1.0 - clamp(dot(-d, n), 0.0, 1.0), 5.0); c = mix(c, rc2, clamp(F*(0.6 + 0.4*pd) + 0.1 + pd*0.15, 0.0, 0.75)*k*fr*(0.6 + 0.4*fl)*rx.y); } }\n"
    "  float a = atan(d.z, d.x), hl = length(d.xz), wt = dot(rw.xy, vec2(-sin(a), cos(a))), acc = 0.0;\n"
    "  for(int i = 0; i < 6; i++){ float D = 26.0*pow(1.7, float(i)), tr = D/max(hl, 0.05); if(tr > ts) break;   // streaks on 6 cylinders around the camera, hidden behind the scene\n"
    "    float n = floor(6.2832*D/2.2), yw = E.y + d.y*tr, v = yw - mod(tm, 300.0)*(540.0 + 40.0*float(i)), u = (a/6.2832 + 0.5)*n - yw*wt/2.2, cv = floor(v/34.0);\n"
    "    float cu = mod(floor(u), n), h1 = rnh(vec3(cu, cv, float(i))), h2 = fract(h1*31.7), h3 = fract(h1*71.3); if(h3 > rq.z*(1.0 - 0.11*float(i))) continue;\n"
    "    float L = 6.0 + 6.0*h2, dv = (v/34.0 - cv - h1*0.5)*34.0; if(dv < 0.0 || dv > L) continue;\n"
    "    float px = D*apx, dx = abs(fract(u) - 0.15 - 0.7*h2)*2.2, w = max(0.04, 0.9*px);\n"
    "    acc += (1.0 - smoothstep(0.0, w, dx))*sin(3.1416*dv/L)*clamp(0.07/px, 0.1, 1.0)*(0.55 + 0.45*fract(h1*7.3)); }\n"
    "  c += rc*min(acc, 1.0)*rw.z;\n"
    "  if(!sk && ts < 1800.0){ float fl = smoothstep(0.72, 0.93, abs(N.y)); if(fl > 0.0) c += rc*rsplash(P.xz, fg ? 1.6 : 3.2, tm, 0.6*ts*apx)*fl*(1.0 - smoothstep(150.0, 800.0, ts))*(1.0 - smoothstep(0.06, 0.2, ts*apx/(fg ? 1.6 : 3.2)))*(fg ? 1.1 : 0.9)*rw.w; }\n"
    "  float fz = texelFetch(st, ivec2(0, 0), 0).y;\n"
    "  if(rw.z > 0.0 && fz > 0.0 && fz < 1500.0){ float rp = clamp(0.9/max(fz*apx, 1e-4), 1.5, 10.0); int m = 0;   // rain aura: spray along the fighters' outline, drifting up\n"
    "    for(int k = 0; k < 8; k++){ float an = float(k)*0.7854; if(fighter(texelFetch(src, ivec2(clamp(p + vec2(cos(an), sin(an))*rp, vec2(0.0), vw.xy - 1.0)), 0).a)) m++; }\n"
    "    float e = fg ? 1.0 - float(m)/8.0 : float(m)/8.0;\n"
    "    if(e > 0.0){ float nz = vn((p - vec2(0.0, tm*rp*9.0))/(rp*1.3)), fk = vn(p/(rp*0.6) + tm*3.0); c += rc*e*smoothstep(0.35, 0.85, nz)*(0.5 + 0.5*fk)*(fg ? 0.7 : 0.9)*rq.w; } }\n"
    "  return c; }\n"
    "void main(){\n"
    "  int md = int(am.x + 0.5); vec2 p = gl_FragCoord.xy;\n"
    "  if(md == 5){ if(!wmask(p)) discard; o = vec4(0.0, 0.0, 0.0, 1.0); return; }\n"
    "  if(md == 0){\n"
    "    if(p.x > 1.0){ vec4 ps = texelFetch(st, ivec2(1, 0), 0); vec3 a = vec3(0.0); float n = 0.0;   // average sky colour, smoothed\n"
    "      if(ck.x > 0.0 && ps.a > 0.5){ o = ps; return; }   // frozen while the charge darkens the sky: additive auras have no depth and would pass for sky\n"
    "      for(int i = 0; i < 24; i++) for(int j = 0; j < 14; j++){ vec2 q = (vec2(i, j) + 0.5)*vw.xy/vec2(24.0, 14.0); if(sky(q)){ a += texelFetch(src, ivec2(q), 0).rgb; n += 1.0; } }\n"
    "      o = n > 0.0 ? vec4(ps.a > 0.5 ? mix(ps.rgb, a/n, 1.0 - exp(-gp.y*1.5)) : a/n, 1.0) : ps; return; }\n"
    "    float vis = 0.0;\n"
    "    if(sun.z > 0.5) for(int i = -2; i <= 2; i++) for(int j = -2; j <= 2; j++){ vec2 q = sun.xy + vec2(i, j)*sun.w; if(q.x < 0.0 || q.y < 0.0 || q.x >= vw.x || q.y >= vw.y || sky(q) || dist(q) > 4000.0) vis += 1.0/25.0; }\n"
    "    vec4 pv = texelFetch(st, ivec2(0, 0), 0);\n"
    "    float m = 1e9; for(int i = 0; i < 24; i++) for(int j = 0; j < 14; j++){ vec2 q = (vec2(i, j) + 0.5)*vw.xy/vec2(24.0, 14.0); if(fighter(texelFetch(src, ivec2(q), 0).a) && !sky(q)) m = min(m, dist(q)); }\n"
    "    float fz = m < 1e8 ? (pv.y > 0.0 ? mix(pv.y, m, 1.0 - exp(-gp.y*0.7)) : m) : pv.y;   // camera to the nearest fighter, smoothed\n"
    "    o = vec4(mix(pv.x, vis, 1.0 - exp(-gp.y*10.0)), fz, 0.0, 1.0); return;\n"
    "  }\n"
    "  if(md == 5){ vec2 ts = vec2(textureSize(aux, 0)), c = p*am.yz; vec3 acc = vec3(0.0);\n"
    "    for(int i = 0; i < 4; i++){ vec2 q = c + vec2(i & 1, i >> 1)*2.0 - 1.0; vec4 s = texture(aux, q/ts); acc += s.rgb*(sky(q) ? 1.0 : gp.z); }\n"
    "    o = vec4(acc*0.25, 1.0); return; }\n"
    "  if(md == 1){ vec2 ts = vec2(textureSize(aux, 0)), c = p*am.yz;\n"
    "    o = vec4(0.25*(texture(aux, (c + vec2(-1.0, -1.0))/ts).rgb + texture(aux, (c + vec2(1.0, -1.0))/ts).rgb + texture(aux, (c + vec2(-1.0, 1.0))/ts).rgb + texture(aux, (c + vec2(1.0, 1.0))/ts).rgb), 1.0); return; }\n"
    "  if(md == 4){ vec2 ts = vec2(textureSize(aux, 0)), uv = p/ts; o = vec4(0.5*texture(aux, uv).rgb + 0.5*texture(aux, uv + am.y*0.5/ts).rgb, 1.0); return; }\n"
    "  if(md == 2){ vec2 q = p*am.yz, dl = (sun.xy - q)/40.0*0.9; vec3 acc = vec3(0.0); float wt = 1.0;\n"
    "    for(int i = 0; i < 40; i++){ if(q.x >= 0.0 && q.y >= 0.0 && q.x < vw.x && q.y < vw.y && dist(q) > zk2.z){ vec3 c = texelFetch(src, ivec2(q), 0).rgb; acc += c*max(lum(c) - 0.35, 0.0)*2.5*wt; } wt *= 0.955; q += dl; }\n"
    "    o = vec4(acc/40.0, 1.0); return; }\n"
    "  vec4 s0 = texelFetch(src, ivec2(p), 0); vec3 c = s0.rgb; bool fg = fighter(s0.a); vec2 uv = p/vw.xy;\n"
    "  if(wq[10].y > 0.5 && !C(1)){ if(wm.x > 0.5){ if(byt(s0.a) == 126) c = water(p, c, true); } else if(!fg) c = water(p, c, false); }\n"
    "  if(wq[10].y > 0.5 && wm.x > 0.5 && wq[1].w > wq[10].x + 0.2){ vec3 E = vec3(wq[0].w, wq[1].w, wq[2].w), d = eray(p); float k, tm = wq[10].z; w2v(E + d, k);   // Enhanced, camera under water: caustics, light shafts, haze by distance\n"
    "    float ts = k > 1e-4 ? min(dist(p)/k, 3000.0) : 3000.0; bool sf = byt(s0.a) == 126; vec3 P = E + d*ts;\n"
    "    if(!sf && !sky(p)){ float dp = max(P.y - wq[10].x, 0.0); vec2 x = P.xz*0.11;\n"
    "      float c1 = pow(1.0 - abs(2.0*vn(x + vec2(tm*0.21, tm*0.13)) - 1.0), 7.0), c2 = pow(1.0 - abs(2.0*vn(x*1.37 - vec2(tm*0.17, -tm*0.19) + 4.0) - 1.0), 7.0);\n"
    "      c += vec3(0.75, 0.95, 1.0)*(c1 + c2)*0.5*exp(-dp/70.0)*(1.0 - smoothstep(150.0, 900.0, ts))*(fg ? 0.5 : 1.0); }\n"
    "    float up = clamp(-d.y, 0.0, 1.0), sh = vn(vec2(atan(d.z, d.x)*9.0 + tm*0.05, tm*0.15))*vn(vec2(atan(d.z, d.x)*23.0 - tm*0.08, 3.0 + tm*0.1));\n"
    "    c += mix(wm.yzw, vec3(1.0), 0.5)*smoothstep(0.25, 0.75, sh)*smoothstep(0.05, 0.6, up)*0.12*(1.0 - exp(-ts/120.0));\n"
    "    c = mix(c, wc.rgb*1.3, (1.0 - exp(-ts/420.0))*0.85); }\n"
    "  if(pb.y > 0.0 && !fg && !C(32) && !(wm.x > 0.5 && byt(s0.a) == 126)){ float wv = sky(p) ? 1.0 : smoothstep(600.0, 4500.0, dist(p));   // camera pan blur (BT3 StgPanBlur strength), only far scenery and sky\n"
    "    if(wv > 0.001){ vec2 tsz = vec2(textureSize(src, 0)), lo = vec2(0.5), hi = vw.xy - 0.5; vec3 acc = c; float n = 1.0;\n"
    "      for(int i = 1; i <= 6; i++){ vec2 q = clamp(p + vec2(pb.z*float(i)/6.0, 0.0), lo, hi); vec4 t = texture(src, q/tsz); if(!fighter(texelFetch(src, ivec2(q), 0).a)){ acc += t.rgb; n += 1.0; } }\n"
    "      c = mix(c, acc/n, (1.0 - (1.0 - pb.y)*(1.0 - pb.w))*wv); } }\n"
    "  int by = fq.x > 0.5 ? (fg ? -1 : dby(p)) : fg ? byt(s0.a) : sky(p) ? 255 : 128;   // BT3 GfxDepthFog byte: the stage only, fighters are drawn after\n"
    "  if(fq.x < 0.5 && ap.w > 0.0 && !fg && !C(4)) c = mix(c, texture(hal, uv).rgb, ap.w*smoothstep(bz.x, bz.y, dist(p)));\n"
    "  if(fq.x > 1.5 && by >= 0 && !C(4)) c = mix(c, texture(fgt, uv).rgb, curve2(by)*ap.w);\n"
    "  float cv = by < 0 ? 0.0 : curve(by)*bz.z*(1.0 - 0.9*cka*ck.x);\n"
    "  if(C(4)){} else if(tn.w > 1.5) c = mix(c, tb.rgb, cv*tb.w); else if(tn.w > 0.5) c += floor(cv*tn.rgb)/255.0;\n"
    "  if(hzq.x > 0.5 && by >= 0 && !C(2)){ float hw = floor(curve3(by)*128.0*hzq.y)/128.0;   // BT3 StgHaze: strip grid with eased random jitter (1/16 texel of a 512x224 copy), weight = depth curve\n"
    "    if(hw > 0.0){ float nc = hzq.z, nr = hzq.w, fc = p.x/vw.x*(nc - 1.0), fr = (1.0 - p.y/vw.y)*(nr - 1.0); int k = int(clamp(floor(fc), 0.0, nc - 2.0)), r = int(clamp(floor(fr), 0.0, nr - 2.0)), n = int(nc + 0.5);\n"
    "      float fx = fc - float(k), fy = fr - float(r); int i0 = r*n + k; vec2 T0 = hzj_at(i0), T1 = hzj_at(i0 + 1), B1 = hzj_at(i0 + 2);\n"
    "      vec2 jo = fx + fy <= 1.0 ? T0 + (T1 - T0)*fx + (T1 - T0)*fy : B1 + (T1 - B1)*(1.0 - fx) + (T1 - B1)*(1.0 - fy);\n"
    "      vec2 tsz = vec2(textureSize(src, 0)), q = clamp(p + vec2(jo.x/16.0/512.0*vw.x, -jo.y/16.0/224.0*vw.y), vec2(0.5), vw.xy - 0.5), e = vec2(0.0, 0.25*vw.y/448.0);\n"
    "      if(!fighter(texelFetch(src, ivec2(q), 0).a)){ vec3 h = 0.5*(texture(src, (q + e)/tsz).rgb + texture(src, (q - e)/tsz).rgb);\n"
    "        if(fq.x > 1.5) h = mix(h, texture(fgt, q/vw.xy).rgb, curve2(by)*ap.w);\n"
    "        if(tn.w > 1.5) h = mix(h, tb.rgb, cv*tb.w); else if(tn.w > 0.5) h += floor(cv*tn.rgb)/255.0;\n"
    "        c = mix(c, min(h*hzc.rgb, vec3(1.0)), hw); } } }\n"
    "  if(ap.x > 0.0 && !sky(p) && !C(4)){ float k = kz(); if(k > 0.0){ float f = (1.0 - exp(-max(dist(p)*k - ap.y, 0.0)/ap.z))*ap.x*(1.0 - 0.9*cka*ck.x); vec4 hz = texelFetch(st, ivec2(1, 0), 0); c = mix(mix(c, vec3(lum(c)), f*0.3), hz.a > 0.5 ? hz.rgb : wq[11].rgb, f); } }   // Enhanced: aerial perspective toward the sky colour\n"
    "  if(rq.x > 0.5){ vec3 E = vec3(wq[0].w, wq[1].w, wq[2].w), d = eray(p); float k; w2v(E + d, k); float ts = k > 1e-4 ? dist(p)/k : 1e9; vec3 P = E + d*min(ts, 30000.0);   // Enhanced rain\n"
    "    vec3 N = normalize(cross(dFdx(P), dFdy(P)) + vec3(0.0, 1e-6, 0.0)); c = rain(p, c, fg, ts, E, d, P, N, length(eray(p + vec2(1.0, 0.0)) - d)); }\n"
    "  if(ck.x > 0.0 && cka > 0.5){}\n  else if(ck.x > 0.0 && sky(p)){ vec3 sa = texelFetch(st, ivec2(1, 0), 0).rgb, ex = max(c - sa, 0.0), dk = mix(c, vec3(lum(c))*vec3(0.5, 0.55, 0.8), 0.5)*0.165;   // SZ charge: night sky; what was added over it (auras, effects) stays\n"
    "    c = mix(c, dk + ex, ck.x); }\n"
    "  else if(ck.x > 0.0){ float dd = dist(p), fd = fg && dd < ck.w*3.0 ? 0.0 : smoothstep(ck.w, ck.w*5.0, dd);   // distance layer: scenery fades to the night from a radius around the camera; fighter-tagged pixels far away are effects (sparking lightning) over the scenery\n"
    "    c = mix(c, mix(c, vec3(lum(c))*vec3(0.5, 0.55, 0.8), 0.5)*0.16, fd*ck.x*0.9); }\n"
    "  if(ck.z > 0.0 && !fg && !C(8) && p.x >= fbb.x && p.x <= fbb.z && p.y >= fbb.y && p.y <= fbb.w){ float fa = 0.0; vec3 fc = vec3(0.0);   // sparking: flames emitted by the fighter, each one rises and dies on its own\n"
    "    for(int i = 0; i < 48; i++){ vec4 f = fpt[i]; if(f.z <= 0.0) continue;\n"
    "      vec2 Q = vec2(p.x - f.x, f.y - p.y)/f.z; if(abs(Q.x) > 0.6 || Q.y < -0.35 || Q.y > 2.0) continue;\n"
    "      float id = floor(f.w), ph = fract(f.w), sd = mod(id, 1024.0); int sl = int(id/1024.0); if(skc[sl].w <= 0.0) continue;\n"
    "      float h = 1.1 + 0.8*ph, ty = Q.y/h, ln = (hn(vec2(sd, 1.3)) - 0.5)*0.35, x = Q.x - ln*ty*ty - 0.06*sin(ty*4.0 + ck.y*5.0 + sd)*ty;\n"
    "      float hw = 0.2*smoothstep(0.0, 0.18, ph)*sqrt(clamp((ty + 0.3)/0.5, 0.0, 1.0))*pow(clamp(1.0 - ty*0.85, 0.0, 1.0), 0.55);\n"
    "      float env = smoothstep(-0.1, 0.06, hw - abs(x))*(1.0 - smoothstep(0.55, 1.0, ty))*(1.0 - smoothstep(0.55, 1.0, ph));\n"
    "      if(env <= 0.0) continue;\n"
    "      float n1 = vn(vec2(x*4.0 + sd*3.1, ty*2.2 - ck.y*2.4 + sd)), n2 = vn(vec2(x*11.0 + sd*1.7, ty*5.0 - ck.y*3.6 + sd*0.5));   // TTT-style ragged texture inside each tongue\n"
    "      float fl = (n1*0.65 + n2*0.45)*(0.6 + 0.6*env), a = smoothstep(0.42, 0.8, fl)*env*skc[sl].w;\n"
    "      fa += a; fc += mix(mix(skc[sl].rgb, vec3(1.0), 0.3), vec3(1.0), 0.75*smoothstep(0.6, 1.0, fl))*a; }   // softer hue, white cores\n"
    "    if(fa > 0.0){ fc /= fa; float k = min(fa, 1.0)*ck.z; c = mix(c, fc, k*0.85) + fc*k*0.3; } }\n"
    "  float sv = texelFetch(st, ivec2(0, 0), 0).x, gk = gp.x + am.y*sv;\n"
    "  if(gk > 0.0 && !C(16)) c += texture(aux, uv).rgb*gk;\n"
    "  float L = sv*gp.w*am.z;\n"
    "  if(L > 0.001 && !C(16)){   // lens flare: core, star, streak on the sun-centre axis, ghosts past the centre\n"
    "    vec2 cen = vw.xy*0.5, dv = (p - sun.xy)/vw.y, ax = cen - sun.xy; float r = length(dv), r2 = r*r;\n"
    "    float out_ = max(max(-sun.x, sun.x - vw.x), max(-sun.y, sun.y - vw.y))/vw.y; L *= 1.0 - smoothstep(0.0, 0.15, out_);\n"
    "    vec3 sc = vec3(1.0, 0.9, 0.75);\n"
    "    c += texture(ray, uv).rgb*0.8*L;\n"
    "    c += sc*L*(1.4*exp(-r2/0.0004) + 0.45*exp(-r/0.05) + 0.12*exp(-r/0.25));\n"
    "    float an = atan(dv.y, dv.x);\n"
    "    c += sc*L*(0.9*pow(abs(cos(an*4.0)), 80.0)*exp(-r/0.12) + 0.5*pow(abs(cos(an*4.0 + 0.39)), 120.0)*exp(-r/0.07));\n"
    "    vec2 sd = normalize(ax + vec2(1e-3)); float al = abs(dot(dv, vec2(-sd.y, sd.x))), ac = dot(dv, sd);\n"
    "    c += vec3(1.0, 0.85, 0.6)*L*(0.7*exp(-al*al/0.0000025)*exp(-abs(ac)/0.55) + 0.12*exp(-al*al/0.00012)*exp(-abs(ac)/0.35));\n"
    "    for(int k = 0; k < 7; k++){\n"
    "      float t = k == 0 ? 0.3 : k == 1 ? 0.55 : k == 2 ? 0.8 : k == 3 ? 1.2 : k == 4 ? 1.45 : k == 5 ? 1.75 : 2.1;\n"
    "      float rr = k == 0 ? 0.035 : k == 1 ? 0.09 : k == 2 ? 0.03 : k == 3 ? 0.15 : k == 4 ? 0.055 : k == 5 ? 0.11 : 0.22;\n"
    "      vec3 tc = k == 0 ? vec3(1.0, 0.8, 0.55) : k == 1 ? vec3(0.8, 0.7, 0.55) : k == 2 ? vec3(0.6, 0.8, 1.0) : k == 3 ? vec3(0.75, 0.7, 0.6) : k == 4 ? vec3(1.0, 0.75, 0.5) : k == 5 ? vec3(0.65, 0.7, 0.75) : vec3(0.7, 0.62, 0.55);\n"
    "      float d = length(p - (sun.xy + ax*t))/vw.y, disk = 1.0 - smoothstep(rr*0.75, rr, d), ring = exp(-pow((d - rr*0.96)/(rr*0.05), 2.0));\n"
    "      c += tc*L*(0.07*disk + 0.12*ring); }\n"
    "  }\n"
    "  o = vec4(min(c, vec3(1.0)), 1.0);\n"
    "}\n";
static GLuint at_prog = 0, at_vao = 0, at_dfbo = 0, at_dtex = 0, at_fbo[8] = {}, at_tex[8] = {}, at_sfbo[2] = {0, 0}, at_stex[2] = {0, 0};
static int at_dw = 0, at_dh = 0, at_w[8] = {}, at_h[8] = {}, at_cur = 0;
static GLint at_u[38]; static float at_pan[4] = {0, 0, 0, 0};
static int gl_lv = -1, gl_st = 0;
static double gl_acc = 0;
static void at_target(int i, int w, int h) {
    if (!at_tex[i]) { glGenTextures(1, &at_tex[i]); glGenFramebuffers(1, &at_fbo[i]); }
    if (at_w[i] == w && at_h[i] == h) return;
    glBindTexture(GL_TEXTURE_2D, at_tex[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, at_fbo[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, at_tex[i], 0);
    at_w[i] = w; at_h[i] = h;
}
static void at_draw(int i, int w, int h, float mode, float a, float b) {
    glBindFramebuffer(GL_FRAMEBUFFER, at_fbo[i]); glViewport(0, 0, w, h);
    glUniform4f(at_u[5], mode, a, b, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
static void at_curve(const int* ky, float* o) {   // StgCurve_Build: zeros up to key 0, two Hermite segments, then the last value
    float kx[3], kz[3];
    for (int k = 0; k < 3; k++) { kx[k] = (float)(ky[k] & 255); kz[k] = (float)(ky[k] >> 8); }
    auto nrm = [](float x, float z, float len) { float l = std::sqrt(x * x + z * z); return l > 1e-9f ? std::array<float, 2>{x / l * len, z / l * len} : std::array<float, 2>{0, 0}; };
    float d1x = kx[2] - kx[1], d1z = kz[2] - kz[1], d0x = kx[0] - kx[1], d0z = kz[0] - kz[1];
    auto tin = nrm(kx[2] - kx[0], kz[2] - kz[0], std::sqrt(d1x * d1x + d1z * d1z)), tout = nrm(kx[0] - kx[2], kz[0] - kz[2], std::sqrt(d0x * d0x + d0z * d0z));
    int cnt = 0; float last = 0;
    for (int i = 0; i < (int)kx[0] && cnt < 256; i++) o[cnt++] = 0.0f;
    for (int sg = 0; sg < 2; sg++) {
        int steps = (int)kx[sg + 1] - (int)kx[sg];
        float p0x = kx[sg], p0z = kz[sg], p1x = kx[sg + 1], p1z = kz[sg + 1];
        float t0x = sg == 0 ? 0.0f : -tout[0], t0z = sg == 0 ? 0.0f : -tout[1], t1x = sg == 0 ? tin[0] : 0.0f, t1z = sg == 0 ? tin[1] : 0.0f;
        for (int i = 0; i <= steps; i++) {
            float t = steps > 0 ? (float)i / steps : 0.0f, h0 = (t - 1) * (t - 1) * (2 * t + 1), h1 = t * t * (3 - 2 * t), h2 = (1 - t) * (1 - t) * t, h3 = (t - 1) * t * t;
            float oz = h0 * p0z + h1 * p1z + h2 * t0z + h3 * t1z; (void)p0x; (void)p1x; (void)t0x; (void)t1x;
            oz = std::min(std::max(oz, 0.0f), 255.0f);
            if (cnt < 256) o[cnt] = oz / 255.0f;
            last = oz;
            if (cnt < 255) cnt++;
        }
    }
    for (int i = cnt; i < 256; i++) o[i] = last / 255.0f;
}
static void glare_step(bool on_scr, int gm) {   // StgGlare_Update, one step per BT3 frame (60 Hz)
    const int* g = psp_map_glare;
    int step = g[1], mx = g[2], hold = gm == 2 ? std::max(g[4], g[3] - 20) : g[3], mn = g[4];   // Full rests 20 below the BT3 hold
    if (gl_lv < 0) { gl_lv = step; gl_st = 0; }
    if (!(g[0] & 2)) { gl_lv = mx; return; }
    if (on_scr) {
        if ((unsigned)gl_st < 2) { gl_st = 1; gl_lv += step; if (gl_lv > mx) { gl_lv = mx; gl_st = 2; } }
        else { gl_lv -= step; if (gl_lv < hold) gl_lv = hold; }
    } else { gl_lv--; gl_st = 0; if (gl_lv < mn) gl_lv = mn; }
}
// BT3 StgHaze (stg_c.c): per 30 Hz battle frame, new random targets every `period` frames, eased by `follow`; cols x rows vertices
static int8_t hz_cur[50][20][2], hz_tgt[50][20][2];
static int hz_frame = 0; static double hz_acc = 0; static GLuint hz_tex = 0; static float hz_lut[256]; static int hz_key[3] = {-1, -1, -1};
static uint32_t hz_rng = 1;
static int hz_rand() { hz_rng = hz_rng * 1103515245u + 12345u; return (int)((hz_rng >> 16) & 0x7FFF); }
static void hz_step(int cols, int rows, int ax, int ay, int period, float follow) {
    if (period > 0 && hz_frame % period == 0)
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++) { hz_tgt[r][c][0] = (int8_t)(-ax + hz_rand() % (2 * ax + 1)); hz_tgt[r][c][1] = (int8_t)(-ay + hz_rand() % (2 * ay + 1)); }
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++)
            for (int k = 0; k < 2; k++) hz_cur[r][c][k] = (int8_t)(hz_cur[r][c][k] + (int)((hz_tgt[r][c][k] - hz_cur[r][c][k]) * follow));
    hz_frame++;
}
// Enhanced splashes (Water effects = Enhanced): a port particle system of its own. Events from the BT3 water logic (fighter enters or
// leaves the water, skimming, ki and technique hits) spawn crowns of droplets, a central jet and spray mist; drawn after the
// atmosphere pass as lit droplet impostors and soft mist, faded against the scene depth
struct WseP { float p[3], v[3], age, life, size, grow, seed, col[3]; int kind; };   // kind 0 droplet, 1 mist, 2 sparking aura cloud
struct WseEv { float p[3], s, dx, dz; int kind; };                    // kind 0 burst, 1 wake
static std::vector<WseP> wse_p; static std::vector<WseEv> wse_ev; static std::mutex wse_mx;
static uint32_t wse_rng = 0x9E3779B9u; static double wse_last = 0;
static GLuint wse_prog = 0, wse_vao = 0, wse_vbo = 0; static GLint wse_u[6];
static float wse_r() { wse_rng ^= wse_rng << 13; wse_rng ^= wse_rng >> 17; wse_rng ^= wse_rng << 5; return (wse_rng & 0xFFFFFF) / 16777216.0f; }
static bool wse_on() { return wat_enh && psp_map_wimm && !psp_gpu_split2() && toon_fno - wat_enh_ok <= 2u; }
static void wse_event(const float* p, float s, int kind, float dx = 0.0f, float dz = 0.0f) {
    if (!wse_on() || !std::isfinite(p[0]) || !std::isfinite(p[2])) return;
    std::lock_guard<std::mutex> g(wse_mx);
    if (wse_ev.size() < 64) wse_ev.push_back({{p[0], p[1], p[2]}, s, dx, dz, kind});
}
static void wse_add(const float* p, const float* v, float size, float life, float grow, int kind, const float* col = nullptr) {
    if (wse_p.size() >= 3000) return;
    WseP q; memcpy(q.p, p, 12); memcpy(q.v, v, 12); q.age = 0; q.life = life; q.size = size; q.grow = grow; q.kind = kind; q.seed = wse_r() * 0.9f;
    for (int j = 0; j < 3; j++) q.col[j] = col ? col[j] : 1.0f; wse_p.push_back(q);
}
static void wse_spawn(const WseEv& e, float lv) {
    float s = std::max(0.3f, std::min(e.s, 3.0f)), rs = std::sqrt(s), c[3] = {e.p[0], lv, e.p[2]};
    if (e.kind == 0) {
        int nd = (int)(90 * s), nj = (int)(24 * s), nm = (int)(14 * s);
        for (int i = 0; i < nd; i++) {   // crown: droplets thrown out and up from a ring
            float a = wse_r() * 6.2832f, r0 = (0.8f + wse_r() * 1.6f) * rs, hs = (5.0f + wse_r() * 10.0f) * rs, vs = -(16.0f + wse_r() * 26.0f) * rs;
            float p[3] = {c[0] + std::cos(a) * r0, lv - 0.2f, c[2] + std::sin(a) * r0}, v[3] = {std::cos(a) * hs, vs, std::sin(a) * hs};
            wse_add(p, v, 0.04f + wse_r() * 0.07f, 3.0f, 0.0f, 0);
        }
        for (int i = 0; i < nj; i++) {   // central jet
            float a = wse_r() * 6.2832f, hs = wse_r() * 2.5f, vs = -(32.0f + wse_r() * 30.0f) * rs;
            float p[3] = {c[0], lv - 0.3f, c[2]}, v[3] = {std::cos(a) * hs, vs, std::sin(a) * hs};
            wse_add(p, v, 0.05f + wse_r() * 0.08f, 3.0f, 0.0f, 0);
        }
        for (int i = 0; i < nm; i++) {   // spray mist
            float a = wse_r() * 6.2832f, r0 = wse_r() * 2.5f * rs, hs = 2.0f + wse_r() * 4.0f;
            float p[3] = {c[0] + std::cos(a) * r0, lv - 1.0f - wse_r() * 2.0f, c[2] + std::sin(a) * r0}, v[3] = {std::cos(a) * hs, -(3.0f + wse_r() * 6.0f), std::sin(a) * hs};
            wse_add(p, v, (1.8f + wse_r() * 2.5f) * rs, 1.0f + wse_r() * 1.0f, 5.0f, 1);
        }
    } else {   // wake: spray thrown to both sides and back from the fighter's feet, plus mist
        float dl = std::sqrt(e.dx * e.dx + e.dz * e.dz), dx = dl > 1e-4f ? e.dx / dl : 1.0f, dz = dl > 1e-4f ? e.dz / dl : 0.0f;
        for (int i = 0; i < 10; i++) {
            float sd = wse_r() < 0.5f ? -1.0f : 1.0f, hs = (6.0f + wse_r() * 10.0f) * rs, bk = 3.0f + wse_r() * 6.0f;
            float p[3] = {c[0] - dx * wse_r() * 2.0f, lv - 0.2f, c[2] - dz * wse_r() * 2.0f}, v[3] = {-dz * sd * hs - dx * bk, -(8.0f + wse_r() * 16.0f) * rs, dx * sd * hs - dz * bk};
            wse_add(p, v, 0.035f + wse_r() * 0.06f, 3.0f, 0.0f, 0);
        }
        for (int i = 0; i < 2; i++) {   // mist plume behind
            float b = 1.0f + wse_r() * 2.5f, sd = (wse_r() - 0.5f) * 2.0f;
            float p[3] = {c[0] - dx * b - dz * sd, lv - 0.6f - wse_r(), c[2] - dz * b + dx * sd}, v[3] = {-dx * (3.0f + wse_r() * 3.0f), -(2.0f + wse_r() * 3.0f), -dz * (3.0f + wse_r() * 3.0f)};
            wse_add(p, v, (1.3f + wse_r() * 1.2f) * rs, 0.8f + wse_r() * 0.6f, 4.0f, 1);
        }
    }
}
static const char* WSE_VS =
    "#version 330 core\n"
    "layout(location=0) in vec3 pos; layout(location=1) in vec4 a; layout(location=2) in vec3 col;\n"
    "uniform vec4 wq[17]; uniform vec4 vw; out vec4 va; out vec3 vc; out float vz;\n"
    "void main(){ vec3 e = pos.x*wq[0].xyz + pos.y*wq[1].xyz + pos.z*wq[2].xyz + wq[3].xyz; vec3 v = e.x*wq[4].xyz + e.y*wq[5].xyz + e.z*wq[6].xyz + vec3(wq[4].w, wq[5].w, wq[6].w); float dz = -v.z;\n"
    "  vec2 n = vec2((v.x*wq[7].x + v.z*wq[7].z + wq[8].x)/dz, (v.y*wq[7].y + v.z*wq[7].w + wq[8].y)/dz)*wq[8].zw; vec2 q = (wq[9].zw + wq[9].xy*n)*wq[3].w;\n"
    "  va = a; vc = col; vz = dz; gl_Position = dz > 0.3 ? vec4(q/vw.zw*2.0 - 1.0, 0.0, 1.0) : vec4(2.0, 2.0, 2.0, 1.0); }\n";
static const char* WSE_FS =
    "#version 330 core\n"
    "uniform sampler2D dep; uniform vec4 vw; uniform vec4 zk; uniform vec4 zk2; uniform vec4 wq[17]; uniform vec4 wt; in vec4 va; in vec3 vc; in float vz; out vec4 o;\n"
    "float dist(vec2 p){ float d = texelFetch(dep, ivec2(clamp(p, vec2(0.0), vw.xy - 1.0)), 0).r; float k = (d*65535.0 - zk.x)/zk.y; float v = (zk.w - k*zk2.y)/(k*zk2.x - zk.z); return (v != v || d >= 1.0) ? 1e9 : abs(v); }\n"
    "void main(){ vec2 u = va.xy*2.0 - 1.0; float r2 = dot(u, u); if(r2 > 1.0) discard;\n"
    "  float soft = clamp((dist(gl_FragCoord.xy) - vz)/2.5, 0.0, 1.0); if(soft <= 0.0) discard; vec3 sk = wq[11].rgb;\n"
    "  if(va.z < 1.0){ float m = exp(-r2*3.5)*0.75*va.w*soft; o = vec4(mix(sk, vec3(0.95, 0.97, 1.0), 0.7)*m, m); }   // droplet: a small bright streak (premultiplied)\n"
    "  else { vec2 g = va.xy*3.0 + fract(va.z)*37.0; vec2 i = floor(g), f = fract(g); f = f*f*(3.0 - 2.0*f);   // mist: soft and broken up\n"
    "    vec4 h = fract(sin(vec4(dot(i, vec2(127.1, 311.7)), dot(i + vec2(1.0, 0.0), vec2(127.1, 311.7)), dot(i + vec2(0.0, 1.0), vec2(127.1, 311.7)), dot(i + vec2(1.0, 1.0), vec2(127.1, 311.7))))*43758.5453);\n"
    "    float nz = mix(mix(h.x, h.y, f.x), mix(h.z, h.w, f.x), f.y), m = exp(-r2*2.5)*(0.45 + 0.55*nz);\n"
    "    if(va.z < 2.0){ float al = m*va.w*soft*0.42; o = vec4(mix(sk, vec3(0.9, 0.94, 0.98), 0.6)*al, al); }\n"
    "    else { float ay = abs(u.y), w = pow(max(1.0 - ay, 0.0), 1.6)*(0.75 + 0.35*nz), sp = 1.0 - smoothstep(w*0.55, w, abs(u.x));   // sparking cloud: pointed at both ends, aura colour, mostly additive\n"
    "      float al = sp*(0.55 + 0.45*nz)*smoothstep(1.0, 0.6, ay)*va.w*soft; o = vec4(mix(vc, vec3(1.0), 0.35)*al*0.9, al*0.25); } } }\n";   // sparking cloud: aura colour, mostly additive
static void wse_frame(DestGL* d, int w, int h, float sfar) {
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    float dt = wse_last > 0 ? (float)std::min(now - wse_last, 0.05) : 0.0f; wse_last = now;
    float lv = aw_q[40];
    std::vector<WseEv> ev; { std::lock_guard<std::mutex> g(wse_mx); ev.swap(wse_ev); }
    bool won = wse_on();
    if (!won) { size_t k2 = 0; for (size_t i = 0; i < wse_p.size(); i++) if (wse_p[i].kind == 2) wse_p[k2++] = wse_p[i]; wse_p.resize(k2); }
    else for (const WseEv& e : ev) wse_spawn(e, lv);
    size_t k = 0;
    for (size_t i = 0; i < wse_p.size(); i++) {
        WseP q = wse_p[i];
        q.age += dt;
        if (q.kind == 0) { q.v[1] += 98.0f * dt; for (int j = 0; j < 3; j++) q.v[j] *= 1.0f - 0.25f * dt; }
        else { float dr = std::exp(-2.5f * dt); for (int j = 0; j < 3; j++) q.v[j] *= dr; q.size += q.grow * dt; }
        for (int j = 0; j < 3; j++) q.p[j] += q.v[j] * dt;
        if (q.age >= q.life || (q.kind == 0 && won && q.v[1] > 0.0f && q.p[1] > lv + 0.3f)) continue;
        wse_p[k++] = q;
    }
    wse_p.resize(k);
    if (wse_p.empty()) return;
    if (!wse_prog) {
        wse_prog = gl_program(WSE_VS, WSE_FS);
        if (!wse_prog) { printf("salpicaduras: no pude preparar el programa\n"); wse_prog = 0xFFFFFFFFu; return; }
        const char* nm[6] = {"wq", "vw", "dep", "zk", "zk2", "wt"};
        for (int i = 0; i < 6; i++) wse_u[i] = glGetUniformLocation(wse_prog, nm[i]);
        glGenVertexArrays(1, &wse_vao); glGenBuffers(1, &wse_vbo);
        glBindVertexArray(wse_vao); glBindBuffer(GL_ARRAY_BUFFER, wse_vbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 40, (void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 40, (void*)12);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 40, (void*)28);
    }
    if (wse_prog == 0xFFFFFFFFu) return;
    const float* q = aw_q;
    float R[3] = {q[0], q[4], q[8]}, U[3] = {q[1], q[5], q[9]}, F[3] = {q[2], q[6], q[10]};   // camera right / up / depth axis in the world
    std::vector<float> vb; vb.reserve(wse_p.size() * 60);
    static const float CX[6] = {-1, 1, 1, -1, 1, -1}, CY[6] = {-1, -1, 1, -1, 1, 1};
    for (const WseP& p : wse_p) {
        float t = p.age / p.life, al = p.kind ? (1.0f - t) * std::min(1.0f, p.age * (p.kind == 2 ? 5.0f : 8.0f)) : 1.0f - std::max(0.0f, t - 0.8f) * 5.0f;
        float A[3], B[3], la = p.kind == 2 ? p.size * 1.9f : p.size, lb = p.kind == 2 ? p.size * 0.7f : p.size;
        memcpy(A, U, 12); memcpy(B, R, 12);
        if (p.kind == 0) {   // droplet: a short streak along its motion (what one frame of travel looks like)
            float vl = std::sqrt(p.v[0] * p.v[0] + p.v[1] * p.v[1] + p.v[2] * p.v[2]);
            if (vl > 1e-3f) {
                float dn[3] = {p.v[0] / vl, p.v[1] / vl, p.v[2] / vl}, cr[3] = {dn[1] * F[2] - dn[2] * F[1], dn[2] * F[0] - dn[0] * F[2], dn[0] * F[1] - dn[1] * F[0]}, cl = std::sqrt(cr[0] * cr[0] + cr[1] * cr[1] + cr[2] * cr[2]);
                if (cl > 1e-3f) { memcpy(A, dn, 12); for (int j = 0; j < 3; j++) B[j] = cr[j] / cl; la = p.size + vl / 60.0f; }
            }
        }
        for (int c = 0; c < 6; c++) {
            for (int j = 0; j < 3; j++) vb.push_back(p.p[j] + B[j] * CX[c] * lb + A[j] * CY[c] * la);
            vb.push_back(CX[c] * 0.5f + 0.5f); vb.push_back(CY[c] * 0.5f + 0.5f); vb.push_back((float)p.kind + p.seed); vb.push_back(al);
            vb.push_back(p.col[0]); vb.push_back(p.col[1]); vb.push_back(p.col[2]);
        }
    }
    glUseProgram(wse_prog);
    glUniform4fv(wse_u[0], 17, aw_q);
    glUniform4f(wse_u[1], (float)w, (float)h, (float)d->w, (float)d->h);
    glUniform1i(wse_u[2], 6);
    glUniform4f(wse_u[3], at_zk[0], at_zk[1], at_zk[2], at_zk[3]);
    glUniform4f(wse_u[4], at_zk[4], at_zk[5], sfar, 0.0f);
    { uint32_t wcl = psp_map_wcol; float tc[3] = {(wcl & 255) / 255.0f, ((wcl >> 8) & 255) / 255.0f, ((wcl >> 16) & 255) / 255.0f}; if (wat_scol[3] > 0.0f) for (int j = 0; j < 3; j++) tc[j] = wat_scol[j]; glUniform4f(wse_u[5], tc[0], tc[1], tc[2], 0.0f); }
    glBindVertexArray(wse_vao); glBindBuffer(GL_ARRAY_BUFFER, wse_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vb.size() * 4), vb.data(), GL_STREAM_DRAW);
    glEnable(GL_BLEND); glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(vb.size() / 10));
    glDisable(GL_BLEND);
    glBindVertexArray(at_vao); glUseProgram(at_prog);
}
static int atb_var = -1; static long atb_n = 0; static double atb_px = 0;
static int atl_var = -1;
static bool atl_on() { if (atl_var < 0) { const char* e = getenv("TTT_ATM_LOG"); atl_var = e && *e ? atoi(e) : 0; } return atl_var > 0; }   // old atmosphere log line: its 1-pixel readback stalls the GPU
static bool atb_on() { if (atb_var < 0) { const char* e = getenv("TTT_ATM_BOX"); atb_var = e && *e ? atoi(e) : 1; } return atb_var > 0; }   // charge-only frames: no screen copy, main pass only around the flames (Pause toggles)
// atmosphere A/B by feature (TTT_ATM_AB=1): every 10 s one group off (water, haze, fog and tint, flames, glow and flare, pan blur), GPU ms of the pass per state
static GLint at_ucut = -1, at_ufbb = -1;
static int atab_var = -1, atab_cur = 0;
static double atab_ms[7]; static long atab_n[7];
static const char* ATAB_NOM[7] = {"todo", "sin agua", "sin calima", "sin niebla y tinte", "sin llamas", "sin brillo y destello", "sin desenfoque de giro"};
static int atab_mode() {
    if (atab_var < 0) { const char* e = getenv("TTT_ATM_AB"); atab_var = e && *e ? atoi(e) : 0; }
    if (atab_var <= 0) return 0;
    static uint64_t t0 = 0, rep = 0;
    uint64_t t = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (!t0) t0 = rep = t;
    if (t >= rep + 30000) {
        rep = t;
        printf("atm ab: ms de GPU del pase por estado:");
        for (int k = 0; k < 7; k++) if (atab_n[k]) printf(" %s %.2f (%ld)", ATAB_NOM[k], atab_ms[k] / atab_n[k], atab_n[k]);
        printf("\n"); fflush(stdout);
    }
    return (int)(((t - t0) / 10000) % 7);
}
struct ApT { uint64_t t; ApT() : t(cp_on() ? perf_clock() : 0) {} void cut(int k) { if (t) { uint64_t n = perf_clock(); cp_ap[k] += n - t; t = n; } } ~ApT() { if (t) { cut(7); cp_ap[8]++; } } };
static void atm_pass(DestGL* d, int w, int h) {
    const int nqa = nq_mode(); const uint64_t nqt = perf_clock();
    struct NqA { int m; uint64_t t; ~NqA() { nq_st[m][6] += perf_clock() - t; } } nqa_{nqa, nqt};
    ApT apt_;
    if (nqa) { glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, glr_ramp); }   // unit 3 always holds the cel ramp after this
    else {
        SubT q_(glq_sid());
        static int ul = 0; GLint au = 0, r3 = 0; glGetIntegerv(GL_ACTIVE_TEXTURE, &au);
        glActiveTexture(GL_TEXTURE3); glGetIntegerv(0x8069, &r3);
        if (ul < 4 && (au != GL_TEXTURE0 || (GLuint)r3 != glr_ramp)) { ul++; printf("atm: estado al entrar: unidad activa %d, rampa cel %s\n", au - GL_TEXTURE0, (GLuint)r3 == glr_ramp ? "bien" : "cambiada"); }
        if ((GLuint)r3 != glr_ramp) glBindTexture(GL_TEXTURE_2D, glr_ramp);
    }
    glActiveTexture(GL_TEXTURE0 + 9);
    nq_st[nqa][4]++; nq_st[nqa][5] += perf_clock() - nqt; apt_.cut(0);
    int lv = at_level(), gm = at_glare();
    atm_pick(d->dir & 0x0FFFFFFFu);
    bool gon = gm > 0 && psp_map_glare[6] && at_on_flag(psp_map_glare[0]);
    bool wenh = wat_enh && psp_map_wimm && wsf_frame > 0 && aw_ok && !psp_gpu_split2();
    bool wson = (ws_on() && aw_ok && wat_stale < 30) || wenh;
    bool rson = rn_live() && aw_ok && !psp_gpu_split2();
    bool sson = !wse_p.empty() && aw_ok && !psp_gpu_split2(), cson = (chg_k > 0.0f || psp_spark_n > 0 || spk_any || spk_fn > 0) && !psp_gpu_split2();
    if (!at_ok || (lv == 0 && !gon && !wson && !rson && !sson && !cson) || !at_full || psp_sel3d_open()) return;
    bool conly = atb_on() && lv == 0 && !gon && !wson && !rson && !sson && !wenh && (chg_k <= 0.0f || cdk_done_f == toon_fno) && !d->mfbo;   // charge or sparking alone: the main pass only adds the flames
    if (lv == 0) at_pan[1] = at_pan[3] = 0;
    if (!at_prog) {
        at_prog = gl_program(WAT_VS, ATM_FS);
        if (!at_prog) { printf("atm: no pude preparar el programa de atmosfera\n"); at_prog = 0xFFFFFFFFu; return; }
        glGenVertexArrays(1, &at_vao);
        const char* nm[38] = {"src", "dep", "aux", "ray", "st", "am", "vw", "sun", "zk", "zk2", "ap", "gp", "hal", "bz", "tn", "fid", "tb", "tl", "pb", "wq", "fgt", "fq", "tl2", "hzq", "hzc", "hzj", "tl3", "wm", "wc", "rq", "rw", "rx", "wr", "ck", "skc", "idw", "fpt", "cka"};
        for (int i = 0; i < 38; i++) at_u[i] = glGetUniformLocation(at_prog, nm[i]);
        at_ucut = glGetUniformLocation(at_prog, "cutm"); at_ufbb = glGetUniformLocation(at_prog, "fbb");
        for (int i = 0; i < 2; i++) {
            glGenTextures(1, &at_stex[i]); glGenFramebuffers(1, &at_sfbo[i]);
            glBindTexture(GL_TEXTURE_2D, at_stex[i]);
            float z[8] = {0, 0, 0, 0, 0, 0, 0, 0};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 2, 1, 0, GL_RGBA, GL_FLOAT, z);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, at_sfbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, at_stex[i], 0);
        }
    }
    if (at_prog == 0xFFFFFFFFu) return;
    int sh = d->mfbo ? std::min(d->h, d->mh) : d->h;
    if (at_dw != d->w || at_dh != sh) {
        if (!at_dtex) { glGenTextures(1, &at_dtex); glGenFramebuffers(1, &at_dfbo); }
        glBindTexture(GL_TEXTURE_2D, at_dtex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, d->w, sh, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, at_dfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, at_dtex, 0);
        glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
        GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        printf("atm: profundidad de la escena %dx%d %s\n", d->w, sh, s == GL_FRAMEBUFFER_COMPLETE ? "lista" : "incompleta");
        if (s != GL_FRAMEBUFFER_COMPLETE) { at_prog = 0xFFFFFFFFu; glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d)); return; }
        at_dw = d->w; at_dh = sh;
    }
    bool on_scr = at_sun[2] > 0.5f && at_sun[0] >= 0 && at_sun[0] <= w && at_sun[1] >= 0 && at_sun[1] <= h;
    if (gon) { gl_acc += at_dt * 60.0; int n = 0; while (gl_acc >= 1.0 && n < 8) { glare_step(on_scr, gm); gl_acc -= 1.0; n++; } if (gl_acc > 1.0) gl_acc = 0; }
    int med = (psp_map_water && psp_map_tint[1][5] && at_on_flag(psp_map_tint[1][0]) && at_cam_y > WAT_LSIGN * psp_map_wlevel) ? 1 : 0;   // world Y points down: eye below the water surface
    const int* tt = psp_map_tint[med];
    bool ton = lv > 0 && tt[5] && at_on_flag(tt[0]) && (tt[0] & 2);
    static float lut[256]; static int lut_key[6] = {-1, -1, -1, -1, -1, -1};
    if (ton && memcmp(lut_key, tt, sizeof(lut_key))) {
        memcpy(lut_key, tt, sizeof(lut_key));
        at_curve(tt + 2, lut);
        if (tt[0] & 4) lut[255] = 0.0f;
        printf("atm: curva del tinte: 0x80 = %.3f, 0xC0 = %.3f, 0xF0 = %.3f, 0xFF = %.3f\n", lut[128], lut[192], lut[240], lut[255]);
    }
    float a8 = ((tt[1] >> 24) & 255) / 255.0f, cr = (tt[1] & 255) / 255.0f, cg = ((tt[1] >> 8) & 255) / 255.0f, cb = ((tt[1] >> 16) & 255) / 255.0f;
    bool dd = ddirect(d);
    apt_.cut(1);
    if (dd) dd_n[0]++; else {
    dd_n[1]++;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, dfbo(d));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, at_dfbo);
    { int bw = d->w, bh = sh, ke_ = scale_int(); if (glr_copy_part() && d->w >= 480 * ke_) { bw = std::min(bw, 480 * ke_); bh = std::min(bh, 272 * ke_); }   // the displayed part only
      glBlitFramebuffer(0, 0, bw, bh, 0, 0, bw, bh, GL_DEPTH_BUFFER_BIT, GL_NEAREST); }
    }
    apt_.cut(2);
    if (wenh && wk_put(d)) { static int kl = 0; if (kl < 2) { kl++; printf("agua: mascara del agua repuesta antes del pase (%dx%d)\n", d->w, d->h); } }
    glActiveTexture(GL_TEXTURE0 + 4);
    bool atb = conly && gl_tbar();
    GLuint src = atb ? d->tex : glr_copy_fb(d);
    if (atb) gl_tbar_fn();
    glBindTexture(GL_TEXTURE_2D, src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glActiveTexture(GL_TEXTURE0 + 6); glBindTexture(GL_TEXTURE_2D, dd ? d->dtex : at_dtex);
    glUseProgram(at_prog);
    glBindVertexArray(at_vao);
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST); glStencilMask(0); glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    int fid[8]; float fidf[8];
    bool dmode = toon_env("TTT_FX_ATMDEPTH", 1.0f) > 0.5f;
    for (int j = 0; j < 8; j++) { fid[j] = (uint16_t)(ol_ids[j] & ~ol_bg[j] & (dmode && j == 7 ? 0x7FFF : 0xFFFF)); fidf[j] = (float)fid[j]; }   // id 0x7F = alpha 0xFF, plain scenery
    glUniform4fv(at_u[15], 2, fidf);
    glUniform1i(at_u[0], 4); glUniform1i(at_u[1], 6); glUniform1i(at_u[2], 7); glUniform1i(at_u[3], 8); glUniform1i(at_u[4], 9); glUniform1i(at_u[12], 10); glUniform1i(at_u[20], 11);
    float sfar = toon_env("TTT_FX_SUNFAR", 1500.0f), b0 = toon_env("TTT_FX_SOFTSTART", 2500.0f), b1 = toon_env("TTT_FX_SOFTEND", 9000.0f);
    glUniform4f(at_u[6], (float)w, (float)h, (float)d->w, (float)d->h);
    bool near_scr = at_sun[2] > 0.5f && at_sun[0] > -0.15f * w && at_sun[0] < 1.15f * w && at_sun[1] > -0.15f * h && at_sun[1] < 1.15f * h;
    glUniform4f(at_u[7], at_sun[0], at_sun[1], near_scr ? 1.0f : 0.0f, std::max(1.0f, 0.012f * w));
    glUniform4f(at_u[8], at_zk[0], at_zk[1], at_zk[2], at_zk[3]);
    glUniform4f(at_u[9], at_zk[4], at_zk[5], sfar, 0.0f);
    bool aer = lv == 2 && dmode;
    glUniform4f(at_u[10], aer ? std::max(0.0f, toon_env("TTT_FX_AERK", 0.225f)) : 0.0f, toon_env("TTT_FX_AER0", 150.0f), std::max(1.0f, toon_env("TTT_FX_AERD", 1500.0f)), dmode ? std::max(0.0f, toon_env("TTT_FX_ATMFOGK", 0.5f)) : lv == 2 ? toon_env("TTT_FX_SOFTK", 0.35f) : 0.0f);
    glUniform4f(at_u[13], b0, std::max(b1, b0 + 1.0f), std::max(0.0f, toon_env("TTT_FX_TINTK", 0.7f)), 0);
    glUniform4f(at_u[14], cr * 128.0f * a8, cg * 128.0f * a8, cb * 128.0f * a8, !ton ? 0.0f : (tt[0] & 2) ? 1.0f : 2.0f);
    glUniform4f(at_u[16], cr, cg, cb, a8);
    glUniform4fv(at_u[17], 64, lut);
    static float lut2[256]; static int fog_key[5] = {-1, -1, -1, -1, -1};
    float zsv = toon_env("TTT_FX_ATMZS", 0.0f), zcam = std::max(1.0f, toon_env("TTT_FX_ATMCAM", 26.8f)), zmn = 1.0f, zmx = 10877000.0f, zn = 0.3f, zf = 65536.0f;   // BT3 View z range; scale = BT3 camera to fighter / measured
    float az = zf * zn * (zmx - zmn) / (zf - zn), cz = (zmn * zf - zmx * zn) / (zf - zn);
    const int* fg = psp_map_fog;
    bool fmap = fg[4] && (fg[0] & 1) && (fg[3] & 255) > (fg[1] & 255);   // BT3 feature 3 needs bit 0; otherwise the BT3 default curve
    int fk[5] = {fmap ? fg[0] : 2, fmap ? fg[1] : 0, fmap ? fg[2] : (222 | 23 << 8), fmap ? fg[3] : (255 | 255 << 8), fmap ? 1 : 0};
    bool fon = dmode && lv > 0 && toon_env("TTT_FX_ATMFOG", 1.0f) > 0.5f;
    if (fon && memcmp(fog_key, fk, sizeof(fog_key))) {   // StgFog_BuildClut
        memcpy(fog_key, fk, sizeof(fog_key));
        at_curve(fk + 1, lut2);
        if (fk[0] & 2) lut2[255] = 0.0f;
        printf("atm: desenfoque %s\n", fmap ? "del mapa" : "BT3 por defecto");
    }
    if (fon) glUniform4fv(at_u[22], 64, lut2);
    glUniform4f(at_u[21], !dmode ? 0.0f : fon ? 2.0f : 1.0f, zsv > 0.0f ? zsv : -zcam, az, cz);
    {   // water surface: camera, rings of half-submerged fighters, sky colour, sun
        float* q = aw_q;
        q[40] = WAT_LSIGN * psp_map_wlevel; q[41] = wson ? 1.0f : 0.0f; q[42] = (float)(toon_fno / 60.0); q[43] = std::max(0.0f, std::min(1.0f, toon_env("TTT_FX_WSURFK", 0.75f)));
        if (sky_cur.w > 0.01) { for (int j = 0; j < 3; j++) q[44 + j] = (float)(sky_cur.c[j] / sky_cur.w); } else { q[44] = 0.6f; q[45] = 0.7f; q[46] = 0.9f; }
        q[47] = psp_map_sun[3] > 0.5f ? 1.0f : 0.0f; q[48] = psp_map_sun[0]; q[49] = -psp_map_sun[1]; q[50] = psp_map_sun[2]; q[51] = std::max(0.0f, toon_env("TTT_FX_WSPEC", 1.0f));
        int nr = 0;
        for (int k = 0; k < 9 && nr < 4; k++) {
            const WRing& r = wr_f[k];
            if (r.f != toon_fno && r.f + 1 != toon_fno) continue;
            if (r.n < 3 || !(r.y0 < q[40] && r.y1 > q[40])) continue;
            q[52 + 4 * nr] = (float)(r.sx / r.n); q[53 + 4 * nr] = (float)(r.sz / r.n); q[54 + 4 * nr] = (float)std::max(1.5, 0.5 * std::max(r.x1 - r.x0, r.z1 - r.z0)); q[55 + 4 * nr] = 1.0f; nr++;
        }
        for (int k = nr; k < 4; k++) q[55 + 4 * k] = 0.0f;
        glUniform4fv(at_u[19], 17, q);
        {   // Enhanced water: mask on, the stage's water colour (MAPD +0x90, 0x80 = 1.0) as the deep tint, absorption distance
            uint32_t wcl = psp_map_wcol;
            float tc[3] = {(wcl & 255) / 255.0f, ((wcl >> 8) & 255) / 255.0f, ((wcl >> 16) & 255) / 255.0f};
            if (wat_scol[3] > 0.0f) for (int k = 0; k < 3; k++) tc[k] = wat_scol[k];
            glUniform4f(at_u[27], wenh ? 1.0f : 0.0f, tc[0], tc[1], tc[2]);
            glUniform4f(at_u[28], tc[0] * 0.45f, tc[1] * 0.45f, tc[2] * 0.45f, toon_env("TTT_FX_WDEEP", 150.0f));
            static int wl = 0;
            if (wenh && wl < 3) { wl++; printf("agua: Enhanced, %d triangulos de agua, nivel %.1f, camara y %.1f, tinte (%.2f %.2f %.2f) %s\n", wsf_frame, q[40], aw_q[7], tc[0], tc[1], tc[2], wat_scol[3] > 0.0f ? "del reflejo BT3" : "del agua del escenario"); }
            if (wenh) wat_enh_ok = toon_fno;
            { static int ul = 0; static bool ulast = false; bool un = aw_q[7] > q[40]; if (wat_enh && psp_map_wimm && un != ulast && ul < 8) { ul++; printf("agua: Enhanced, camara %s el agua, %d triangulos de superficie, shader %s\n", un ? "bajo" : "sobre", wsf_frame, wenh ? "si" : "no"); } ulast = un; }
            wsf_frame = 0;
        }
        {   // Enhanced rain: on, lightning flash, drop density, aura; wind tilt, streak and splash strength
            glUniform4f(at_u[29], rson ? 1.0f : 0.0f, rn_flash, std::max(0.0f, std::min(1.0f, toon_env("TTT_FX_RAIN_DENS", 0.38f))), std::max(0.0f, toon_env("TTT_FX_RAIN_AURA", 1.0f)));
            glUniform4f(at_u[30], rn_wind[0], rn_wind[1], std::max(0.0f, toon_env("TTT_FX_RAIN_K", 1.0f)), std::max(0.0f, toon_env("TTT_FX_RAIN_SPLASH", 1.0f)));
            {   // SZ charge: sky darkness, sparking fighters' aura colours by slot, outline id -> slot
                float sc[32] = {}, iw[128] = {};
                for (int sl = 0; sl < 8; sl++) { for (int j = 0; j < 3; j++) sc[4 * sl + j] = spk_c[sl][j]; sc[4 * sl + 3] = spk_f[sl]; }
                for (int i = 0; i < 128; i++) iw[i] = (float)ol_who[i];
                glUniform4f(at_u[33], chg_k * std::max(0.0f, toon_env("TTT_CHARGE2_DARK", 1.0f)), (float)(toon_fno / 60.0 - std::floor(toon_fno / 60.0 / 600.0) * 600.0), spk_any ? std::max(0.0f, toon_env("TTT_CHARGE2_FLAME", 1.0f)) : 0.0f, std::max(10.0f, toon_env("TTT_CHARGE2_NEAR", 300.0f)));
                static int fl = 0;
                if (psp_spark_n > 0 && fl < 3) { fl++; int nw = 0; for (int i = 0; i < 128; i++) nw += ol_who[i] != 0; printf("carga: llamas de sparking, %d luchadores, %d ids de contorno con luchador, casilla %d color (%.2f %.2f %.2f)\n", psp_spark_n, nw, (int)psp_spark[0][9], psp_spark[0][6], psp_spark[0][7], psp_spark[0][8]); }
                glUniform4fv(at_u[34], 8, sc); glUniform4fv(at_u[35], 32, iw); glUniform4fv(at_u[36], 48, &spk_gpu[0][0]);
                glUniform1f(at_u[37], cdk_done_f == toon_fno ? 1.0f : 0.0f);
            }
            { float z[4 * WSR_N] = {}; glUniform4fv(at_u[32], WSR_N, wsr_f && toon_fno + 1 - wsr_f <= 2u ? wsr_q : z); }
            glUniform4f(at_u[31], wt_enh ? rn_wet : 0.0f, std::max(0.0f, toon_env("TTT_FX_WET_REFL", 1.0f)), 0.0f, 0.0f);
            if (rson) rn_drawn = toon_fno + 1;
            static int rl = 0; static bool rlast = false;
            if (rson != rlast && rl < 6) { rl++; printf("lluvia: Enhanced %s, viento (%.2f %.2f)\n", rson ? "activa" : "apagada", rn_wind[0], rn_wind[1]); }
            rlast = rson;
        }
        static int wl = 0; static bool wlast = false;
        if (wson != wlast && wl < 6) { wl++; printf("agua: superficie %s, nivel %.1f, anillos %d\n", wson ? "activa" : "apagada", q[40], nr); }
        wlast = wson;
    }
    glUniform4f(at_u[18], at_pan[0] * w / 512.0f, at_pan[1], at_pan[2] * w / 512.0f, at_pan[3]);
    float gk = gon ? gl_lv / 128.0f * toon_env("TTT_FX_SUNK", 1.0f) : 0.0f, gx = 0.0f, fa = 1.0f;
    if (toon_env("TTT_FX_SUNAIM", 1.0f) > 0.5f) {   // glare above the map minimum and the flare grow as the view turns to the sun
        float a0 = toon_env("TTT_FX_SUNAIM0", 0.1f), a1 = std::max(a0 + 0.01f, toon_env("TTT_FX_SUNAIM1", 1.1f));
        float dn = std::hypot(at_sun[0] - 0.5f * w, at_sun[1] - 0.5f * h) / std::max(1.0f, 0.5f * h), t = std::min(1.0f, std::max(0.0f, (dn - a0) / (a1 - a0)));
        fa = at_sun[2] > 0.5f ? 1.0f - t * t * (3.0f - 2.0f * t) : 0.0f;
        float base = gon ? std::min(gl_lv, psp_map_glare[4]) / 128.0f * toon_env("TTT_FX_SUNK", 1.0f) : 0.0f;
        gx = std::max(0.0f, gk - base) * fa * fa; gk = base;
    }
    float ex = (gm == 2 && near_scr) ? std::max(0.0f, toon_env("TTT_FX_FLAREK", 0.9f)) : 0.0f;   // the lens flare stays while the sun is seen
    glUniform4f(at_u[11], gk, (float)at_dt, gon ? psp_map_glare[5] / 128.0f : 1.0f, ex);
    int nx = at_cur ^ 1;
    apt_.cut(3);
    glActiveTexture(GL_TEXTURE0 + 9); glBindTexture(GL_TEXTURE_2D, at_stex[at_cur]);
    glBindFramebuffer(GL_FRAMEBUFFER, at_sfbo[nx]); glViewport(0, 0, 2, 1);
    glUniform4f(at_u[5], 0, 0, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    at_cur = nx;
    glBindTexture(GL_TEXTURE_2D, at_stex[at_cur]);
    int hw = std::max(w / 2, 1), hh = std::max(h / 2, 1), qw = std::max(w / 4, 1), qh = std::max(h / 4, 1);
    at_target(0, hw, hh); at_target(1, qw, qh); at_target(2, qw, qh); at_target(3, 256, 256); at_target(4, 128, 128); at_target(5, 64, 64); at_target(6, 64, 64);
    apt_.cut(4);
    glActiveTexture(GL_TEXTURE0 + 7);
    if (lv == 2 || fon) { glBindTexture(GL_TEXTURE_2D, src); at_draw(0, hw, hh, 1, (float)w / hw, (float)h / hh); }
    int fh = std::max(1, std::min(lv == 2 ? (int)toon_env("TTT_FX_ATMFOGH", 360.0f) : 224, hh)), fw = std::max(1, std::min(hw, (int)std::lround((double)fh * w / std::max(1, h))));   // BT3 StgFog copy (224 of 448 lines on the PS2)
    if (fon) { at_target(7, fw, fh); glBindTexture(GL_TEXTURE_2D, at_tex[0]); at_draw(7, fw, fh, 1, (float)hw / fw, (float)hh / fh); }
    int gb = 5;
    if (gon) {   // GfxPost_DrawGlow: weighted 256x256, 128, 64, 10 passes 50/50 one texel apart every other pass
        glBindTexture(GL_TEXTURE_2D, src); at_draw(3, 256, 256, 5, (float)w / 256, (float)h / 256);
        glBindTexture(GL_TEXTURE_2D, at_tex[3]); at_draw(4, 128, 128, 1, 2, 2);
        glBindTexture(GL_TEXTURE_2D, at_tex[4]); at_draw(5, 64, 64, 1, 2, 2);
        for (int i = 0; i < 10; i++) { glBindTexture(GL_TEXTURE_2D, at_tex[gb]); at_draw(gb == 5 ? 6 : 5, 64, 64, 4, (i & 1) ? -1.0f : 1.0f, 0); gb = gb == 5 ? 6 : 5; }
    }
    glActiveTexture(GL_TEXTURE0 + 8);
    if (ex > 0.001f) { glBindTexture(GL_TEXTURE_2D, src); at_draw(2, qw, qh, 2, (float)w / qw, (float)h / qh); glBindTexture(GL_TEXTURE_2D, at_tex[2]); }
    else { glBindTexture(GL_TEXTURE_2D, at_tex[gb]); glUniform4f(at_u[11], gk, (float)at_dt, 1.0f, 0.0f); }
    glActiveTexture(GL_TEXTURE0 + 11); glBindTexture(GL_TEXTURE_2D, fon ? at_tex[7] : at_tex[0]);
    glActiveTexture(GL_TEXTURE0 + 10); glBindTexture(GL_TEXTURE_2D, at_tex[0]);
    glActiveTexture(GL_TEXTURE0 + 7); glBindTexture(GL_TEXTURE_2D, at_tex[gb]);
    apt_.cut(5);
    {   // BT3 StgHaze (MAPD +0xA0, StgFx_DrawPre after the fog and tint): only scenery, as BT3 draws the fighters after it
        static int hzon = 1, hn = 0;
        if ((hn++ & 255) == 0) { const char* e = getenv("TTT_BT3_HAZE"); hzon = !(e && *e == '0'); }
        const int* hp = psp_map_haze;
        bool hon = hzon && hp[9] && (hp[0] & 1) && dmode && lv > 0 && !psp_gpu_split2();
        if (hon) {
            int cols = std::min(std::max(hp[5], 2), 20), rows = std::min(std::max(hp[6], 2), 50);
            if (memcmp(hz_key, hp + 2, sizeof(hz_key))) { memcpy(hz_key, hp + 2, sizeof(hz_key)); at_curve(hp + 2, hz_lut); memset(hz_cur, 0, sizeof(hz_cur)); hz_frame = 0; printf("atm: calima activa, curva 0x40 = %.3f, 0x80 = %.3f, 0xFF = %.3f\n", hz_lut[64], hz_lut[128], hz_lut[255]); }
            hz_acc += at_dt * 30.0;
            for (int ns = 0; hz_acc >= 1.0 && ns < 4; ns++) { hz_step(cols, rows, hp[7], hp[8], hp[9] & 255, psp_map_haze_follow); hz_acc -= 1.0; }
            if (hz_acc > 1.0) hz_acc = 0;
            static float jb[50 * 20 * 4];
            for (int r = 0; r < rows; r++) for (int c = 0; c < cols; c++) { float* q = jb + 4 * (r * cols + c); q[0] = hz_cur[r][c][0]; q[1] = hz_cur[r][c][1]; q[2] = q[3] = 0; }
            glActiveTexture(GL_TEXTURE0 + 12);
            if (!hz_tex) glGenTextures(1, &hz_tex);
            glBindTexture(GL_TEXTURE_2D, hz_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, cols, rows, 0, GL_RGBA, GL_FLOAT, jb);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            float sc = psp_map_top < -1.0f ? 1.0f - 0.5f * at_cam_y / psp_map_top : 1.0f;   // StgHaze_UpdateClut: half strength at the stage top
            static int sl = 0; static uint32_t slf = 0;
            if (sl < 6 && toon_fno - slf > 300u) { sl++; slf = toon_fno; printf("atm: calima escala %.2f (camara %.1f, techo %.1f), rejilla %dx%d\n", sc, at_cam_y, psp_map_top, cols, rows); }
            uint32_t hc = (uint32_t)hp[1];
            glUniform4f(at_u[23], 1.0f, sc * std::max(0.0f, toon_env("TTT_FX_HAZEK", 0.75f)), (float)cols, (float)rows);   // his level: BT3 strength x 0.75
            glUniform4f(at_u[24], (hc & 255) / 128.0f, ((hc >> 8) & 255) / 128.0f, ((hc >> 16) & 255) / 128.0f, 0.0f);
            glUniform1i(at_u[25], 12);
            glUniform4fv(at_u[26], 64, hz_lut);
        } else glUniform4f(at_u[23], 0, 0, 0, 0);
    }
    apt_.cut(6);
    glBindFramebuffer(GL_FRAMEBUFFER, dd ? d->cfbo : dfbo(d)); glViewport(0, 0, d->w, d->h);
    glEnable(GL_SCISSOR_TEST); glScissor(0, 0, w, h);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glUniform4f(at_u[5], 3, gx, fa, 0);
    atab_cur = atab_mode();
    if (at_ucut >= 0) glUniform1i(at_ucut, atab_cur ? 1 << (atab_cur - 1) : 0);
    if (at_ufbb >= 0) {   // flames: only inside the union of their boxes (the shader skips the 48-point loop elsewhere)
        float b[4] = {1e9f, 1e9f, -1e9f, -1e9f};
        for (int i = 0; i < 48; i++) { const float* f = spk_gpu[i]; if (f[2] <= 0.0f) continue; b[0] = std::min(b[0], f[0] - 0.65f * f[2]); b[2] = std::max(b[2], f[0] + 0.65f * f[2]); b[1] = std::min(b[1], f[1] - 2.05f * f[2]); b[3] = std::max(b[3], f[1] + 0.4f * f[2]); }
        glUniform4f(at_ufbb, b[0], b[1], b[2], b[3]);
    }
    bool md3 = true;
    if (conly) {
        float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
        if (spk_any) for (int i = 0; i < 48; i++) { const float* f = spk_gpu[i]; if (f[2] <= 0.0f) continue; bx0 = std::min(bx0, f[0] - 0.6f * f[2]); bx1 = std::max(bx1, f[0] + 0.6f * f[2]); by0 = std::min(by0, f[1] - 2.0f * f[2]); by1 = std::max(by1, f[1] + 0.35f * f[2]); }
        int ix0 = std::max(0, (int)std::floor(bx0) - 2), iy0 = std::max(0, (int)std::floor(by0) - 2), ix1 = std::min(w, (int)std::ceil(bx1) + 2), iy1 = std::min(h, (int)std::ceil(by1) + 2);
        if (ix1 <= ix0 || iy1 <= iy0) md3 = false; else glScissor(ix0, iy0, ix1 - ix0, iy1 - iy0);
        atb_n++; if (md3) atb_px += (double)(ix1 - ix0) * (iy1 - iy0);
    }
    if (md3) { if (atb) gl_tbar_fn(); glDrawArrays(GL_TRIANGLES, 0, 3); if (atb) gl_tbar_fn(); }
    if (wenh) { glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE); glUniform4f(at_u[5], 5, gx, fa, 0); glDrawArrays(GL_TRIANGLES, 0, 3); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE); }
    if (wenh || sson) wse_frame(d, w, h, sfar);
    if (dd) glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, glr_ramp);
    glActiveTexture(GL_TEXTURE0 + 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (d->mfbo) d->dirty = true;
    static int zl = 0; static uint32_t zlf = 0;
    if (dmode && zl < 14 && atl_on() && toon_fno - zlf > (zl < 6 ? 120u : 600u)) {
        zl++; zlf = toon_fno; float px[4] = {0, 0, 0, 0};
        glBindFramebuffer(GL_READ_FRAMEBUFFER, at_sfbo[at_cur]); glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, px); glBindFramebuffer(GL_READ_FRAMEBUFFER, dfbo(d));
        float k = zsv > 0.0f ? zsv : px[1] > 0.5f ? zcam / px[1] : 0.0f;
        auto dist_of = [&](const float* t, float v) { for (int b = 0; b < 255; b++) if (t[b] >= v) return k > 0 ? az / std::max(1.0f, (255 - b) * 256.0f - cz) / k : -1.0f; return -1.0f; };
        printf("atm: camara al luchador cercano %.1f, escala a BT3 %.3f, desenfoque 25%% a %.0f 50%% a %.0f, tinte 25%% a %.0f 50%% a %.0f\n", px[1], k, dist_of(lut2, 0.25f), dist_of(lut2, 0.5f), dist_of(lut, 0.25f), dist_of(lut, 0.5f));
    }
    static int logs = 0; static uint32_t lf = 0; static bool was = false;
    if (on_scr != was && logs < 20) { logs++; printf("atm: sol %s de la pantalla (%.0f %.0f), nivel del destello %d\n", on_scr ? "dentro" : "fuera", at_sun[0], at_sun[1], gl_lv); }
    was = on_scr;
    if (logs < 20 && toon_fno - lf > 600) {
        logs++; lf = toon_fno; int nf = 0; for (int j = 0; j < 8; j++) for (int b = 0; b < 16; b++) nf += (fid[j] >> b) & 1;
        printf("atm: atmosfera %d, destello %d (%s) nivel %d, mirada %.2f extra %.2f, tinte %s medio %d, sol en pantalla (%.0f %.0f) %s, ids de luchadores %d\n",
               lv, gm, gon ? "del mapa" : "apagado en el mapa", gl_lv, fa, gx, ton ? "si" : "no", med, at_sun[0], at_sun[1], at_sun[2] > 0.5f ? "delante" : "detras", nf);
    }
}
// SZ charge darkness drawn before the effects batch (sky and far scenery as the atmosphere pass), so effects drawn after stay bright
static const char* CDK_FS =
    "#version 330 core\n"
    "uniform sampler2D src; uniform sampler2D dep; uniform sampler2D st; uniform vec4 vw; uniform vec4 zk; uniform vec4 zk2; uniform vec4 fid[2]; uniform vec4 ck; out vec4 o;\n"
    "float dist(vec2 p){ float d = texelFetch(dep, ivec2(clamp(p, vec2(0.0), vw.xy - 1.0)), 0).r; float k = (d*65535.0 - zk.x)/zk.y; float v = (zk.w - k*zk2.y)/(k*zk2.x - zk.z); return (v != v || d >= 1.0) ? 1e9 : abs(v); }\n"
    "bool sky(vec2 q){ float d = texelFetch(dep, ivec2(clamp(q, vec2(0.0), vw.xy - 1.0)), 0).r; return d >= 1.0 || d <= 0.0; }\n"
    "float lum(vec3 c){ return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    "bool fighter(float a){ int id = int(a*255.0 + 0.5) & 127; int w = int(fid[id >> 6][(id >> 4) & 3] + 0.5); return ((w >> (id & 15)) & 1) != 0; }\n"
    "void main(){ vec2 p = gl_FragCoord.xy; vec4 s0 = texelFetch(src, ivec2(p), 0); vec3 c = s0.rgb; bool fg = fighter(s0.a);\n"
    "  if(sky(p)){ vec4 sr = texelFetch(st, ivec2(1, 0), 0); vec3 ex = sr.a > 0.5 ? max(c - sr.rgb, 0.0) : vec3(0.0), dk = mix(c, vec3(lum(c))*vec3(0.5, 0.55, 0.8), 0.5)*0.165; c = mix(c, dk + ex, ck.x); }\n"
    "  else { float dd = dist(p), fd = fg && dd < ck.w*3.0 ? 0.0 : smoothstep(ck.w, ck.w*5.0, dd); c = mix(c, mix(c, vec3(lum(c))*vec3(0.5, 0.55, 0.8), 0.5)*0.16, fd*ck.x*0.9); }\n"
    "  o = vec4(c, 1.0); }\n";
static GLuint cdk_prog = 0, cdk_vao = 0; static GLint cdk_u[8];
static bool cdk_pass(DestGL* d, int w, int h) {
    GqZone gqz_(GQ_CDK);
    static int el = 0;
    if (!at_prog || at_prog == 0xFFFFFFFFu || !at_dtex || cdk_prog == 0xFFFFFFFFu) { if (el < 4) { el++; printf("carga: oscuridad sin pase de atmosfera (programa %u, profundidad %u)\n", at_prog, at_dtex); } return false; }
    int sh = d->mfbo ? std::min(d->h, d->mh) : d->h;
    if (at_dw != d->w || at_dh != sh) { if (el < 4) { el++; printf("carga: oscuridad con tamano distinto (%dx%d, atmosfera %dx%d)\n", d->w, sh, at_dw, at_dh); } return false; }
    if (!cdk_prog) {
        cdk_prog = gl_program(WAT_VS, CDK_FS);
        if (!cdk_prog) { printf("carga: no pude preparar el programa de oscuridad\n"); cdk_prog = 0xFFFFFFFFu; return false; }
        glGenVertexArrays(1, &cdk_vao);
        const char* nm[8] = {"src", "dep", "st", "vw", "zk", "zk2", "fid", "ck"};
        for (int i = 0; i < 8; i++) cdk_u[i] = glGetUniformLocation(cdk_prog, nm[i]);
        printf("carga: oscuridad de la carga antes de los efectos lista\n");
    }
    GLint vp[4] = {0, 0, 0, 0}, at = GL_TEXTURE0, sm = 0, b4 = 0, b6 = 0, b9 = 0; GLboolean cm[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE}, dm = GL_FALSE;
    const int qm = nq_mode(); const uint64_t qt = perf_clock();   // qm: no queries, the caller rebuilds the whole draw state after the pass
    bool sc = false, dt = false, bl = false, st = false;
    SubT q_(glq_sid());
    if (!qm) {
    glGetIntegerv(GL_VIEWPORT, vp); glGetIntegerv(GL_ACTIVE_TEXTURE, &at); glGetIntegerv(GL_STENCIL_WRITEMASK, &sm); glGetBooleanv(GL_COLOR_WRITEMASK, cm); glGetBooleanv(GL_DEPTH_WRITEMASK, &dm);
    sc = glIsEnabled(GL_SCISSOR_TEST); dt = glIsEnabled(GL_DEPTH_TEST); bl = glIsEnabled(GL_BLEND); st = glIsEnabled(GL_STENCIL_TEST);
    glActiveTexture(GL_TEXTURE0 + 4); glGetIntegerv(0x8069, &b4);
    glActiveTexture(GL_TEXTURE0 + 6); glGetIntegerv(0x8069, &b6);
    glActiveTexture(GL_TEXTURE0 + 9); glGetIntegerv(0x8069, &b9);
    }
    q_.end();
    static int ck_sid = perf_sub_id("oscuridad: copias"); SubT ck_(ck_sid);
    glDisable(GL_SCISSOR_TEST);
    bool dd = ddirect(d);
    if (dd) dd_n[0]++; else {
    dd_n[1]++;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, dfbo(d)); glBindFramebuffer(GL_DRAW_FRAMEBUFFER, at_dfbo);
    { int bw = d->w, bh = sh, ke_ = scale_int(); if (glr_copy_part() && d->w >= 480 * ke_) { bw = std::min(bw, 480 * ke_); bh = std::min(bh, 272 * ke_); }   // the displayed part only
      glBlitFramebuffer(0, 0, bw, bh, 0, 0, bw, bh, GL_DEPTH_BUFFER_BIT, GL_NEAREST); }
    }
    bool tb = !d->mfbo && gl_tbar();
    glActiveTexture(GL_TEXTURE0 + 4); GLuint src = tb ? d->tex : glr_copy_fb(d); glBindTexture(GL_TEXTURE_2D, src);
    ck_.end();
    glActiveTexture(GL_TEXTURE0 + 6); glBindTexture(GL_TEXTURE_2D, dd ? d->dtex : at_dtex);
    glActiveTexture(GL_TEXTURE0 + 9); glBindTexture(GL_TEXTURE_2D, at_stex[at_cur]);
    glUseProgram(cdk_prog); glBindVertexArray(cdk_vao);
    float fidf[8]; bool dmode = toon_env("TTT_FX_ATMDEPTH", 1.0f) > 0.5f;
    for (int j = 0; j < 8; j++) fidf[j] = (float)(uint16_t)(ol_ids[j] & ~ol_bg[j] & (dmode && j == 7 ? 0x7FFF : 0xFFFF));
    glUniform1i(cdk_u[0], 4); glUniform1i(cdk_u[1], 6); glUniform1i(cdk_u[2], 9);
    glUniform4f(cdk_u[3], (float)w, (float)h, (float)d->w, (float)d->h);
    glUniform4f(cdk_u[4], at_zk[0], at_zk[1], at_zk[2], at_zk[3]); glUniform4f(cdk_u[5], at_zk[4], at_zk[5], 0.0f, 0.0f);
    glUniform4fv(cdk_u[6], 2, fidf);
    glUniform4f(cdk_u[7], chg_k * std::max(0.0f, toon_env("TTT_CHARGE2_DARK", 1.0f)), 0.0f, 0.0f, std::max(10.0f, toon_env("TTT_CHARGE2_NEAR", 300.0f)));
    glBindFramebuffer(GL_FRAMEBUFFER, dd ? d->cfbo : dfbo(d)); glViewport(0, 0, d->w, d->h);
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST); glStencilMask(0);
    glEnable(GL_SCISSOR_TEST); glScissor(0, 0, w, h);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    if (tb) { gl_tbar_fn(); gl_tbar_n++; }
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (tb) gl_tbar_fn();
    glActiveTexture(GL_TEXTURE0 + 9); glBindTexture(GL_TEXTURE_2D, (GLuint)b9);
    glActiveTexture(GL_TEXTURE0 + 6); glBindTexture(GL_TEXTURE_2D, (GLuint)b6);
    glActiveTexture(GL_TEXTURE0 + 4); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); glBindTexture(GL_TEXTURE_2D, (GLuint)b4);
    glActiveTexture((GLenum)at);
    glUseProgram(glr_prog); glBindVertexArray(glr_vao);
    glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
    if (!qm) {
        glViewport(vp[0], vp[1], vp[2], vp[3]);
        glColorMask(cm[0], cm[1], cm[2], cm[3]); glDepthMask(dm); glStencilMask((GLuint)sm);
        if (sc) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        if (dt) glEnable(GL_DEPTH_TEST); if (bl) glEnable(GL_BLEND); if (st) glEnable(GL_STENCIL_TEST);
    }
    if (d->mfbo) d->dirty = true;
    nq_st[qm][2]++; nq_st[qm][3] += perf_clock() - qt;
    return true;
}
// BT3 GfxWater_DrawView: 10 x 6 grid over the 512 x 224 copy, two radial waves on the UV (1/16 texel, each term truncated), top row fixed
static bool gfxw_now = false;
static float gfxw_ph[2] = {0, 0};
static double gfxw_t = -1;
static void gfxw_grid(float* uv) {
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() * 30.0;
    if (gfxw_t < 0 || now - gfxw_t > 8) gfxw_t = now - 1;
    for (; gfxw_t < now; gfxw_t += 1) {
        const float sp[2] = {0.06f, 0.04f};
        for (int k = 0; k < 2; k++) { gfxw_ph[k] += sp[k]; if (gfxw_ph[k] > 3.14159265f) gfxw_ph[k] -= 6.2831853f; }
    }
    const int W = 512, cols = 10;
    const float C[2][2] = {{0.2f * W, 44.8f}, {0.8f * W, 179.2f}};
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < cols; c++) {
            int cx = c * W / (cols - 1), cy = 224 * r / 5, gu = cx * 16, gv = cy * 16;
            if (r != 0)
                for (int k = 0; k < 2; k++) {
                    float dx = cx - C[k][0], dy = cy - C[k][1], d = std::sqrt(dx * dx + dy * dy), kk = std::cos(gfxw_ph[k] + 0.2f * d) * 48.0f;
                    if (d > 0) { gu += (int)(kk * dx / d); gv += (int)(kk * dy / d); }
                }
            gu = std::max(0, std::min(W * 16, gu)); gv = std::max(0, std::min(224 * 16, gv));
            uv[2 * (r * cols + c)] = gu / 16.0f / W; uv[2 * (r * cols + c) + 1] = gv / 16.0f / 224.0f;
        }
}
static long wat_skip_n = 0;
static void wat_pass(DestGL* d, int x0, int y0, int w, int h, float wave) {
    if (!wat_prog) {
        wat_prog = gl_program(WAT_VS, WAT_FS);
        if (!wat_prog) { printf("agua: no pude preparar el programa de refraccion\n"); wat_prog = 0xFFFFFFFFu; return; }
        glGenVertexArrays(1, &wat_vao);
        gwt_u_t = glGetUniformLocation(wat_prog, "gwt"); gwt_u_uv = glGetUniformLocation(wat_prog, "guv"); wat_u_src = glGetUniformLocation(wat_prog, "src"); wat_u_r = glGetUniformLocation(wat_prog, "wr"); wat_u_a = glGetUniformLocation(wat_prog, "wa");
        fx_u_b = glGetUniformLocation(wat_prog, "fb"); fx_u_rad = glGetUniformLocation(wat_prog, "rad"); fx_u_dir = glGetUniformLocation(wat_prog, "dirb");
        fx_u_glow = glGetUniformLocation(wat_prog, "glow"); fx_u_bk = glGetUniformLocation(wat_prog, "bk"); fx_u_imp = glGetUniformLocation(wat_prog, "imp");
    }
    if (wat_prog == 0xFFFFFFFFu) return;
    {
        GqZone gqa_(GQ_ATM);
        uint64_t a0 = cp_on() ? perf_clock() : 0;
        if (g3_any && toon_env("TTT_CEL_GLOW", 1) > 0.5f) g3_pass(d, w, h);
        uint64_t a1 = a0 ? perf_clock() : 0;
        atm_pass(d, w, h);
        if (a0) { cp_at[0] += a1 - a0; cp_at[1] += perf_clock() - a1; }
    }
    {   // nothing left for the final pass (charge or sparking alone): it would copy the screen onto itself
        static int sv = -1; if (sv < 0) { const char* e = getenv("TTT_FX_SKIP"); sv = e && *e ? atoi(e) : 1; }
        bool need = wave > 0 || gfxw_now || fx.ab > 0 || fx.lines > 0 || fx.imode || fx.rush > 0 || fx.dash * FX_SPEEDK > 0 || (FX_BLOOM > 0 && FX_BLOOMK > 0) || std::fabs(fx.shx) + std::fabs(fx.shy) > 0.0f;
        if (sv > 0 && !need) {
            glActiveTexture(GL_TEXTURE0);
            glUseProgram(glr_prog); glBindVertexArray(glr_vao);
            if (glr_current) glBindFramebuffer(GL_FRAMEBUFFER, dfbo(glr_current));
            wat_skip_n++; wat_passes++; wat_done = true; return;
        }
    }
    GqZone gqz_(GQ_FX);   // one pass for every screen effect (BT3 GfxWater_DrawView for the wobble)
    int sc = scale_int();
    struct AtT { uint64_t t = 0; ~AtT() { if (t) cp_at[3] += perf_clock() - t; } } att_;
    glActiveTexture(GL_TEXTURE0 + 4);
    uint64_t ac0 = cp_on() ? perf_clock() : 0;
    GLuint t = glr_copy_fb(d);
    if (ac0) { att_.t = perf_clock(); cp_at[2] += att_.t - ac0; }
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindVertexArray(wat_vao);
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST); glStencilMask(0);
    bool soft = FX_BLOOM > 1.5f && fx_pyr_make(w, h);
    bool bloom = soft || (FX_BLOOM > 0 && FX_BLOOMK > 0 && fx_bloom_make(std::max(w / 4, 1), std::max(h / 4, 1)));
    if (soft) {   // bright pass with a soft knee (luminance and max channel), down the pyramid, then back up adding every level
        glUseProgram(fx_bprog);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_SCISSOR_TEST);
        glUniform2f(fx_bu_lim, (w - 0.5f) / d->w, (h - 0.5f) / d->h);
        glUniform1i(fx_bu_src, 4);
        glViewport(0, 0, fx_pw[0], fx_ph[0]);
        glBindFramebuffer(GL_FRAMEBUFFER, fx_pfbo[0]);
        glUniform2f(fx_bu_tg, (float)fx_pw[0], (float)fx_ph[0]);
        glUniform4f(fx_bu_m, 3.0f, FX_B2TH, (float)w / fx_pw[0], std::max(FX_B2KNEE, 0.001f));
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glActiveTexture(GL_TEXTURE0 + 5);
        glUniform1i(fx_bu_src, 5);
        for (int k = 1; k < fx_plv; k++) {
            glViewport(0, 0, fx_pw[k], fx_ph[k]);
            glBindFramebuffer(GL_FRAMEBUFFER, fx_pfbo[k]);
            glBindTexture(GL_TEXTURE_2D, fx_ptex[k - 1]);
            glUniform2f(fx_bu_tg, (float)fx_pw[k], (float)fx_ph[k]);
            glUniform4f(fx_bu_m, 4.0f, 0, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFunc(GL_ONE, GL_ONE);
        for (int k = fx_plv - 2; k >= 0; k--) {
            glViewport(0, 0, fx_pw[k], fx_ph[k]);
            glBindFramebuffer(GL_FRAMEBUFFER, fx_pfbo[k]);
            glBindTexture(GL_TEXTURE_2D, fx_ptex[k + 1]);
            glUniform2f(fx_bu_tg, (float)fx_pw[k], (float)fx_ph[k]);
            glUniform4f(fx_bu_m, 5.0f, 1.0f, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        glDisable(GL_BLEND);
        glBindTexture(GL_TEXTURE_2D, fx_ptex[0]);
        glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    } else if (bloom) {   // bright pass at a quarter of the view, then blur it twice
        glUseProgram(fx_bprog);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, fx_bw, fx_bh);
        glBindFramebuffer(GL_FRAMEBUFFER, fx_bfbo[0]);
        glUniform1i(fx_bu_src, 4);
        glUniform4f(fx_bu_m, 0.0f, FX_BLOOMTH, 4.0f, 0.0f);
        glUniform2f(fx_bu_lim, (w - 0.5f) / d->w, (h - 0.5f) / d->h);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glActiveTexture(GL_TEXTURE0 + 5);
        for (int pass = 0; pass < 2; pass++) {
            glBindFramebuffer(GL_FRAMEBUFFER, fx_bfbo[1 - pass]);
            glBindTexture(GL_TEXTURE_2D, fx_btex[pass]);
            glUniform1i(fx_bu_src, 5);
            glUniform4f(fx_bu_m, 1.0f + pass, 0, 0, 0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        glBindTexture(GL_TEXTURE_2D, fx_btex[0]);
        glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    }
    glUseProgram(wat_prog);
    glUniform1i(wat_u_src, 4);
    glUniform1i(fx_u_glow, 5);
    glUniform1f(fx_u_bk, soft ? -fx.bloom / (float)fx_plv : bloom ? fx.bloom : 0.0f);
    glUniform4f(wat_u_r, (float)w, (float)h, (float)wat_pu, (float)sc);
    glUniform4f(wat_u_a, wave, 0.0f, (float)x0, (float)y0);
    if (gfxw_now) {
        float uv[120];
        gfxw_grid(uv);
        uint32_t c = psp_map_wcol;
        glUniform4f(gwt_u_t, (c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, 1.0f);
        glUniform4fv(gwt_u_uv, 30, uv);
        static int nl = 0;
        if (nl < 3) { nl++; printf("agua: ondulacion BT3 (GfxWater) con tinte %08X, fases %.2f %.2f\n", (unsigned)c, gfxw_ph[0], gfxw_ph[1]); }
    } else glUniform4f(gwt_u_t, 0, 0, 0, 0);
    glUniform4f(fx_u_b, (float)fx.imode, fx.ab, 0.0f, std::min(fx.ab * 0.06f, 0.35f));
    glUniform4f(fx_u_imp, fx.lines, fx.seed, fx.shx, fx.shy);
    if (fx.rush > 0) { glUniform4f(fx_u_rad, fx.rcx * sc, fx.rcy * sc, fx.rush, FX_RUSHK); glUniform4f(fx_u_dir, 0, 0, fx.lcx, fx.lcy); }
    else { glUniform4f(fx_u_rad, fx.dcx * sc, fx.dcy * sc, fx.dash * FX_SPEEDK, FX_SPEEDZ); glUniform4f(fx_u_dir, 1, 0, fx.lcx, fx.lcy); }
    glViewport(0, 0, d->w, d->h);
    glEnable(GL_SCISSOR_TEST); glScissor(x0, y0, w, h);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0 + 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(glr_prog);
    glBindVertexArray(glr_vao);
    wat_passes++; wat_done = true;
    if (d->mfbo) d->dirty = true;
}
static void fx_frame(double now, double dt) {   // per presented frame: event states (rumble.inc events_fx) and camera motion
    at_dt = std::min(std::max(dt, 0.001), 0.1);
    fx_params();
    static float rush_ser = 0, imp_ser = 0; static double rush_t0 = 0, imp_t = -10; static float imp_k = 0; static int logs = 0;
    int carrier = (int)psp_fx_ev[0] - 1;
    if (psp_fx_ev[1] != rush_ser) { rush_ser = psp_fx_ev[1]; rush_t0 = now; }
    bool on = FX_RUSH > 0 && carrier >= 0 && now - rush_t0 < 8.0;
    fx.rush = on ? std::min(fx.rush + (float)dt * 6.0f, 1.0f) : std::max(fx.rush - (float)dt * 15.0f, 0.0f);
    {   // rush centre: the fighter who hits (not the victim whose animation carries VFX4); dash centre: the fighter who dashes
        int dm = (int)psp_fx_ev[4], dk = -1;
        for (int k = 0; k < 8 && dk < 0; k++) if ((dm >> k & 1) && fx_fn[k + 1] > 0) dk = k + 1;
        fx.dash = FX_SPEED > 0 && dk > 0 ? std::min(fx.dash + (float)dt * 5.0f, 1.0f) : std::max(fx.dash - (float)dt * 5.0f, 0.0f);
        { static int dl = 0; static bool was = false; bool on_ = FX_SPEED > 0 && dm != 0; if (on_ && !was && dl < 40) { dl++; printf("fx: blur de movimiento por dash (mascara %d, dibujado %d), fuerza %.2f\n", dm, dk > 0 ? 1 : 0, FX_SPEEDK); } was = on_; }
        if (dk > 0) { float x = (float)(fx_fx[dk] / fx_fn[dk]), y = (float)(fx_fy[dk] / fx_fn[dk]); fx.dcx += (x - fx.dcx) * 0.4f; fx.dcy += (y - fx.dcy) * 0.4f; }
        int best = -1; double bn = 0;
        for (int k = 1; k < 9; k++) if (k != carrier + 1 && fx_fn[k] > bn) { bn = fx_fn[k]; best = k; }
        float cx = 240, cy = 136;
        if (best > 0) { cx = (float)(fx_fx[best] / fx_fn[best]); cy = (float)(fx_fy[best] / fx_fn[best]); }
        if (!fx.rcok || fx.rush <= 0) { fx.rcx = cx; fx.rcy = cy; fx.rcok = true; }
        else { fx.rcx += (cx - fx.rcx) * 0.3f; fx.rcy += (cy - fx.rcy) * 0.3f; }
        for (int k = 0; k < 9; k++) { if (fx_fn[k] > 0) { fx_px[k] = fx_fx[k]; fx_py[k] = fx_fy[k]; fx_pn[k] = fx_fn[k]; } fx_fx[k] = fx_fy[k] = fx_fn[k] = 0; }
    }
    if (psp_fx_ev[2] != imp_ser) { imp_ser = psp_fx_ev[2]; imp_t = now; imp_k = psp_fx_ev[3]; }
    static float sh_ser = 0; static double sh_t = -10; static float sh_k = 0;
    if (psp_fx_ev[6] != sh_ser) { sh_ser = psp_fx_ev[6]; sh_t = now; sh_k = psp_fx_ev[7]; }
    double it = (now - imp_t) * 1000.0;
    // impact (only hits that connect, rumble.inc): manga speed lines and a short shake; TTT_FX_IMPACT=2 keeps the black and white impact frames
    double dur = FX_IMPACTMS * imp_k;
    fx.imode = FX_IMPACT > 1.5f && imp_k > 0 && it < dur ? (it < dur * 0.5 ? 1 : 2) : 0;
    if (it < 1.0) {   // new hit: new line pattern, centred on the fighter who takes it
        static uint32_t sd = 1; sd = sd * 1664525u + 1013904223u; fx.seed = (float)(sd >> 16 & 1023);
        int v = (int)psp_fx_ev[5];
        if (v > 0 && v < 9 && fx_pn[v] > 0) { fx.lcx = (float)(fx_px[v] / fx_pn[v]); fx.lcy = (float)(fx_py[v] / fx_pn[v]); } else { fx.lcx = 240; fx.lcy = 136; }
        if (logs < 30) { logs++; printf("fx: lineas en (%.0f %.0f), luchador %d con %.0f vertices en pantalla, fuerza %.2f\n", fx.lcx, fx.lcy, v - 1, v > 0 && v < 9 ? fx_pn[v] : 0.0, imp_k); }
    }
    fx.lines = FX_IMPACT > 0.5f && FX_IMPACT < 1.5f && it < 240 ? (float)(std::min(1.0, 0.35 + 0.65 * imp_k) * (it < 70 ? 1.0 : 1.0 - (it - 70) / 170.0)) : 0.0f;
    {
        double st = (now - sh_t) * 1000.0, ph = it, a = FX_SHAKE > 0 && it < 400 ? (2.0 + 6.0 * imp_k) * FX_SHAKEK * std::exp(-it / 120.0) : 0.0;   // combo about 5 px, rush 8
        if (FX_SHAKE > 0 && st < 400) { double b = (2.0 + 6.0 * sh_k) * FX_SHAKEK * std::exp(-st / 120.0); if (b > a) { a = b; ph = st; } }
        fx.shx = (float)(a * std::sin(ph * 0.09 + 0.7)); fx.shy = (float)(a * 0.8 * std::sin(ph * 0.073 + 2.1));
    }
    {   // bloom: base strength, more while additive effects (ki blasts, beams) cover the screen
        float v = vfx_prev.w > 0 ? (float)vfx_prev.w : 0.0f, k = FX_BLOOMK * (1.0f + std::min(v * FX_BLOOMVFX, 2.0f));
        if (FX_BLOOM > 1.5f) k = FX_B2K * (1.0f + std::min(v * FX_BLOOMVFX, 1.0f) * FX_B2VFX);   // soft: smaller boost under ki effects (barrier)
        fx.bloom += (k - fx.bloom) * 0.2f;
    }
    fx.ab = FX_SHAKE > 0 && it < 300 ? (float)(2.0 * imp_k * FX_SHAKEK * std::exp(-it / 110.0)) : 0.0f;
}
static void wat_frame() {
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    double dt = wat_last > 0 ? std::min(now - wat_last, 0.1) : 0.0;
    wat_last = now; wat_done = false;
    { static int n = 0; if ((n++ & 63) == 0) wt_enh = toon_env("TTT_WET", 1.0f) > 0.5f; }
    chg_k = psp_charge_dark ? std::min(1.0f, chg_k + (float)dt / 0.7f) : std::max(0.0f, chg_k - (float)dt / 1.2f);
    {   // sparking flames: quick fade in, slow fade out
        static float fo = 1.4f; static int fn = 0; bool on[8] = {};
        if ((fn++ & 63) == 0) fo = std::max(0.1f, toon_env("TTT_CHARGE2_FADE", 1.4f));
        for (int k = 0; k < psp_spark_n && k < 8; k++) { int sl = std::max(0, std::min(7, (int)psp_spark[k][9])); on[sl] = true; for (int j = 0; j < 3; j++) spk_c[sl][j] = psp_spark[k][6 + j]; for (int j = 0; j < 6; j++) spk_p[sl][j] = psp_spark[k][j]; }
        static bool was[8], quick[8]; static int cut = -1; static float qf = 0.25f;
        if (cut < 0) { const char* e = getenv("TTT_SPARK_CUT"); cut = e && *e ? atoi(e) : 1; qf = std::max(0.02f, toon_env("TTT_SPARK_CUT_FADE", 0.25f)); }
        for (int sl = 0; sl < 8; sl++) {   // the flames leave with a short fade when the slot drops out (attack started, or sparking over)
            if (cut > 0 && was[sl] && !on[sl]) quick[sl] = true;
            if (on[sl]) quick[sl] = false;
            was[sl] = on[sl];
        }
        spk_any = false;
        for (int sl = 0; sl < 8; sl++) { spk_f[sl] = on[sl] ? std::min(1.0f, spk_f[sl] + (float)dt / 0.3f) : std::max(0.0f, spk_f[sl] - (float)dt / (quick[sl] ? qf : fo)); spk_any |= spk_f[sl] > 0.0f; }
        static float acc[8]; static uint32_t rs = 0x9E3779B9u; static int nid = 0;
        auto rnd = [&]() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (rs & 0xFFFFFF) / 16777216.0f; };
        float fd = (float)dt;
        for (int sl = 0; sl < 8; sl++) {   // the anchor itself (fighter screen box) is taken at the ATM pass; here only births and ageing
            if (spk_f[sl] <= 0.0f) { spk_a[sl][2] = 0; acc[sl] = 0; continue; }
            acc[sl] += fd * 30.0f * spk_f[sl];
            while (acc[sl] >= 1.0f) {
                acc[sl] -= 1.0f;
                if (spk_fn >= 48) continue;
                SpkFlame& f = spk_fl[spk_fn++];
                f.t = -0.85f + 1.7f * rnd(); f.ox = (rnd() - 0.5f) * 0.9f; f.oy = 0; f.v = 0.45f + 0.35f * rnd();   // along the body axis, across it, rising (half sizes per second)
                f.age = 0; f.life = 0.5f + 0.3f * rnd(); f.sw = 0.42f + 0.28f * rnd(); f.slot = sl; f.id = nid = (nid + 1) & 1023;
            }
        }
        for (int i = 0; i < spk_fn;) { SpkFlame& f = spk_fl[i]; f.age += fd; if (f.age >= f.life) { f = spk_fl[--spk_fn]; continue; } f.oy -= f.v * fd; i++; }
    }
    rn_wet = wt_enh && rn_live() ? std::min(1.0f, rn_wet + (float)dt / 4.0f) : std::max(0.0f, rn_wet - (float)dt / 20.0f);
    fx_frame(now, dt); wat_fseen = false; wat_3d = false; wat_split = false; wat_surfseen = false;
    wat_pu += dt; if (wat_pu > 600.0) wat_pu -= 600.0;
    {   // after-fighter water mesh: the texture with most blended triangles drawn over the fighters (kept while it keeps showing up)
        int b = -1; for (int k = 0; k < wat_mk; k++) if (b < 0 || wat_mn[k] > wat_mn[b]) b = k;
        uint32_t nt = b >= 0 && wat_mn[b] >= 20 ? wat_mt[b] : 0;
        if (nt) wat_mesh_hold = 90; else if (wat_mesh_hold > 0 && --wat_mesh_hold > 0) nt = wat_mesh_tex;
        static int ml = 0;
        if (nt != wat_mesh_tex && ml < 6) { ml++; printf("agua: malla sobre los luchadores textura 0x%08X (%d triangulos), fondo del stencil %d\n", nt, b >= 0 ? wat_mn[b] : 0, wat_bg); }
        wat_mesh_tex = nt; wat_mk = 0;
    }
    static const bool dbg = [] { const char* e = getenv("TTT_DEBUG"); return e && *e == '1'; }();
    static bool f9 = false; bool k9 = dbg && (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    if (k9 && !f9 && psp_map_water) wat_f9 = 2; else if (wat_f9) wat_f9--;
    f9 = k9;
    static float cnt = -1;
    wat_stale = psp_wat_cam[5] != cnt ? 0 : std::min(wat_stale + 1, 1000); cnt = psp_wat_cam[5];
    bool on = wat_on();
    static int logs = 0;
    for (int k = 1; k < 9; k++) {
        float frac = wat_sub[k][1] > 0 ? (float)(wat_sub[k][0] / wat_sub[k][1]) : 0.0f, was = wat_wet[k];
        if (!on) { wat_wet[k] = 0; continue; }
        if (wat_sub[k][1] > 0) wat_wet[k] = std::max(wat_wet[k] - (float)(dt / std::max(WAT_WET, 0.1f)), frac);
        if (logs < 24 && (was > 0.5f) != (wat_wet[k] > 0.5f)) { logs++; printf("agua: luchador %d %s (sumergido %.2f, mojado %.2f)\n", k - 1, wat_wet[k] > 0.5f ? "mojado" : "seco", frac, wat_wet[k]); }
        wat_sub[k][0] = wat_sub[k][1] = 0;
    }
    static uint32_t fno = 0; static int slog = 0;
    if (on && slog < 20 && (++fno % 300) == 1) {
        slog++;
        printf("agua: superficie %ld triangulos (mezcla %ld, escribe profundidad %ld, prueba %ld), antes de luchadores %ld, despues %ld | luchadores %ld vertices, sumergidos %ld, y de %.1f a %.1f | refraccion %d\n",
               wat_st.surf, wat_st.surf_bl, wat_st.surf_zw, wat_st.surf_zt, wat_st.before, wat_st.after, wat_st.fighter, wat_st.fsub, wat_st.fy0, wat_st.fy1, wat_passes);
        wat_st = {}; wat_st.fy0 = 1e9f; wat_st.fy1 = -1e9f;
    }
}
static void map_light_frame() {
    int ord[6], n = 0;
    for (int j = 0; j < vcl_n; j++) if (vcl_cur[j].w > 1e-5) ord[n++] = j;
    std::sort(ord, ord + n, [](int a, int b) { return vcl_cur[a].w > vcl_cur[b].w; });
    bool used[4] = {false, false, false, false};
    for (int q = 0; q < n && q < 4; q++) {
        const VfxCl& v = vcl_cur[ord[q]];
        float P[3], C[3], mx = 1e-6f;
        for (int j = 0; j < 3; j++) { P[j] = (float)(v.p[j] / v.w); C[j] = (float)(v.c[j] / v.w); mx = std::max(mx, C[j]); }
        for (int j = 0; j < 3; j++) C[j] /= mx;   // hue only: sparkle sprites average dark
        float R = FX_VFXR * std::fabs(P[2]) + 1e-3f, bd = 2.25f * R * R; int b = -1;
        for (int j = 0; j < 4; j++) {
            const MapL& m = map_l[j];
            if (used[j] || m.e <= 0.01f || m.vh != v.vh) continue;
            float dx = m.p[0] - P[0], dy = m.p[1] - P[1], dz = m.p[2] - P[2], d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < bd) { bd = d2; b = j; }
        }
        if (b >= 0) for (int j = 0; j < 3; j++) { map_l[b].p[j] += (P[j] - map_l[b].p[j]) * 0.35f; map_l[b].c[j] += (C[j] - map_l[b].c[j]) * 0.3f; }
        else {
            float lo = 1e30f;
            for (int j = 0; j < 4; j++) if (!used[j] && map_l[j].e < lo) { lo = map_l[j].e; b = j; }
            if (b < 0) continue;
            for (int j = 0; j < 3; j++) { map_l[b].p[j] = P[j]; map_l[b].c[j] = C[j]; }
            map_l[b].e = 0; map_l[b].vh = v.vh;
        }
        used[b] = true;
        map_l[b].t = (float)std::min(1.0, v.w * 12.0);
    }
    for (int j = 0; j < 4; j++) {
        MapL& m = map_l[j];
        if (!used[j]) m.t = 0;
        m.e += (m.t - m.e) * (m.t > m.e ? 0.5f : 0.12f);
        if (m.e < 0.005f) m.e = 0;
    }
    vcl_n = 0;
    static int logs = 0; static uint32_t fn = 0; static float le = 0;
    float top = 0; int nl = 0, ti = 0;
    for (int j = 0; j < 4; j++) if (map_l[j].e > 0) { nl++; if (map_l[j].e > top) { top = map_l[j].e; ti = j; } }
    if (logs < 30 && top > 0.05f && (std::fabs(top - le) > 0.3f || ++fn % 240 == 0)) {
        logs++; le = top; const MapL& m = map_l[ti];
        printf("cel: luz del ki en el mapa %d fuentes, mayor %.2f color (%.2f %.2f %.2f) a %.1f radio %.1f\n", nl, top * FX_VFXMAP * FX_VFXK, m.c[0], m.c[1], m.c[2], std::fabs(m.p[2]), FX_VFXR * std::fabs(m.p[2]));
    }
}
static void toon_tune() {
    SH_R = toon_env("TTT_SHADOW_R", 2.4f); SH_BL = (int)toon_env("TTT_SHADOW_BL", 1.0f);
    TF_BIAS = toon_env("TTT_TEXBIAS", -0.5f);
}
static void glr_fx_late(); struct DestGL; static void fx_run(DestGL* d, int mainw, float wave);
static void toon_frame() {
    glr_fx_late();
    wat_frame();
    vfx_prev = vfx_cur; vfx_cur = {}; toon_ground.clear();
    map_light_frame();
    fz_prev = fz_cur; fz_cur = 0;
    ol_reset(); olb_reset();
    toon_fno++;
    int top = -1;
    for (int i = 63; i >= 0 && top < 0; i--) if (sky_bin[i][3] > 0.02) top = i;
    sky_cur = {};
    if (top >= 0) for (int i = std::max(top - 4, 0); i <= top; i++) { for (int j = 0; j < 3; j++) sky_cur.c[j] += sky_bin[i][j]; sky_cur.w += sky_bin[i][3]; }   // farthest draws = sky dome
    memset(sky_bin, 0, sizeof(sky_bin));
    {   // lightning maps (disc.h: PCK1 entry 13 + MAPD ambient particles on): storms are bursts of 2-4 return strokes (fast rise,
        // short hold, overlapping decays, long afterglow on the last), sometimes one dim distant sheet flash; short dark gaps
        struct Stroke { double t, hold, tau; float peak; };
        static std::vector<Stroke> st; static double next = 0; static uint32_t rnd = 2463534242u; static int logs = 0;
        auto fr = [&]() { rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5; return (rnd & 0xFFFFFF) / 16777216.0f; };
        double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if (!psp_map_bolts || TOON_BOLT <= 0.0f || bolts_on() <= 0) { next = 0; st.clear(); }
        else if (next == 0) next = now + TOON_BOLTGMIN + fr() * (TOON_BOLTGMAX - TOON_BOLTGMIN);
        else if (now >= next) {
            float az = fr() * 6.2831853f, up = std::min(std::max(TOON_BOLTUP, 0.0f), 1.0f), h = std::sqrt(1.0f - up * up), sg = TOON_BOLTSIGN < 0 ? -1.0f : 1.0f;
            float ax[3] = {toon_up[1], -toon_up[0], 0}, al = std::sqrt(ax[0] * ax[0] + ax[1] * ax[1]);
            if (al < 1e-3f) { ax[0] = 1; ax[1] = 0; al = 1; }
            for (int j = 0; j < 2; j++) ax[j] /= al;
            float bx[3] = {toon_up[1] * ax[2] - toon_up[2] * ax[1], toon_up[2] * ax[0] - toon_up[0] * ax[2], toon_up[0] * ax[1] - toon_up[1] * ax[0]};
            for (int j = 0; j < 3; j++) bolt_dir[j] = sg * toon_up[j] * up + (ax[j] * std::cos(az) + bx[j] * std::sin(az)) * h;
            { size_t m = 0; for (size_t i = 0; i < st.size(); i++) if (now - st[i].t <= 4.0) st[m++] = st[i]; st.resize(m); }
            double t = now, end = now;
            float base = TOON_BOLT * (0.75f + 0.25f * fr());
            int n = fr() < 0.2f ? 1 : 2 + (int)(fr() * 3.0f);
            if (n == 1) { st.push_back({t, 0.1 + 0.15 * fr(), 0.45 + 0.3 * fr(), base * 0.45f}); end = t + 0.8; }
            else for (int i = 0; i < n; i++) {
                Stroke k{t, 0.05 + 0.1 * fr(), i + 1 < n ? 0.08 + 0.08 * fr() : 0.4 + 0.4 * fr(), i ? base * (0.5f + 0.5f * fr()) : base};
                st.push_back(k);
                end = t + k.hold + 1.2 * k.tau;
                t += k.hold + 0.04 + 0.12 * fr();
            }
            next = end + TOON_BOLTGMIN + fr() * (TOON_BOLTGMAX - TOON_BOLTGMIN);
            if (logs < 12) { logs++; printf("cel: rayo con %d destellos, intensidad %.2f, dura %.2f s, siguiente en %.2f s\n", n, base, end - now, next - now); }
        }
        float k = 0;
        for (const Stroke& q : st) {
            double d = now - q.t;
            if (d < 0) continue;
            float v = d < 0.015 ? (float)(d / 0.015) : d < q.hold ? 1.0f : (float)std::exp(-(d - q.hold) / std::max(q.tau, 0.01));
            k = std::max(k, q.peak * v);
        }
        bolt_k = k < 0.005f ? 0.0f : k;
    }
    double sw = sky_cur.w, gw = amb_cur.w;
    // the stage light is fixed per map like BT3's: measured during the first seconds of the scene, then held until the 3D scene
    // goes away (menus, loading); flying high or looking at the ground no longer changes it (TTT_CEL_MAPLOCK=0: always live)
    static int lock_n = 0, gone_n = 0;
    bool lock_on = toon_env("TTT_CEL_MAPLOCK", 1) > 0.5f;
    if (sw > 0.02 || gw > 0.05) gone_n = 0; else if (++gone_n > 60) lock_n = 0;
    bool held = lock_on && lock_n >= 150;
    scn_held = held;
    if (!held && (sw > 0.02 || gw > 0.05)) lock_n++;
    if (held) { sw = 0; gw = 0; }
    if (sw > 0.02 || gw > 0.05) {   // light colour: mostly sky (sun or moon), a little of the scene
        float s[3], g[3], c[3];
        for (int j = 0; j < 3; j++) { g[j] = gw > 0 ? (float)(amb_cur.c[j] / gw) : 0.0f; s[j] = sw > 0.02 ? (float)(sky_cur.c[j] / sw) : g[j]; }
        float ks = sw > 0.02 ? TOON_SKY : 0.0f;
        if (gw <= 0.05) ks = 1.0f;
        for (int j = 0; j < 3; j++) c[j] = s[j] * ks + g[j] * (1.0f - ks);
        float l = 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2], k = amb_ok ? 0.05f : 1.0f;
        for (int j = 0; j < 3; j++) amb_rgb[j] += (c[j] - amb_rgb[j]) * k;
        amb_lum += (l - amb_lum) * k; amb_ok = true;
        float m = std::max(amb_lum, 1e-3f), f = std::pow(std::max(amb_lum, 0.0f) / std::max(TOON_MAPREF, 1e-3f), 0.75f);
        f = std::min(std::max(f, TOON_MAPMIN), 1.0f);
        bool warm = s[0] > s[2] * 1.3f;
        if (warm) f = std::max(f, TOON_WARMMIN);   // warm sky (dust, dusk): daylight with dark colours, not night
        float hue = warm ? TOON_WARMHUE : TOON_HUE;
        for (int j = 0; j < 3; j++) {
            float h = std::min(std::max(1.0f + (amb_rgb[j] / m - 1.0f) * hue, 0.85f), 1.15f), li = h * std::sqrt(f);
            map_t[j] = 1.0f + (li - 1.0f) * TOON_MAP;
            map_s[j] = 1.0f + (li * (0.75f + 0.25f * f) - 1.0f) * TOON_MAP;
        }
        map_hl = 1.0f - (1.0f - f * f) * TOON_MAP * 0.6f;
        static int alog = 0; static float last = -1; static uint32_t lastf = 0;
        if (alog < 40 && (std::fabs(amb_lum - last) > 0.03f || toon_fno - lastf > 300)) {
            alog++; last = amb_lum; lastf = toon_fno;
            printf("cel: luz del mapa brillo %.3f factor %.2f luz (%.2f %.2f %.2f) sombra (%.2f %.2f %.2f) | cielo (%.2f %.2f %.2f) peso %.2f distancia %.0f | escena (%.2f %.2f %.2f) peso %.2f\n",
                   amb_lum, f, map_t[0], map_t[1], map_t[2], map_s[0], map_s[1], map_s[2], s[0], s[1], s[2], sw, top >= 0 ? std::pow(2.0, top / 2.0) : 0.0, g[0], g[1], g[2], gw);
        }
    }
    amb_cur = {}; sky_cur = {};
    static int logs = 0;
    if (logs < 8 && vfx_prev.w > 0.0005) { logs++; printf("cel: luz de efectos (%.2f %.2f %.2f) peso %.4f\n", vfx_prev.c[0] / vfx_prev.w, vfx_prev.c[1] / vfx_prev.w, vfx_prev.c[2] / vfx_prev.w, vfx_prev.w); }
}
static void toon_scene() {
    auto tri = [](const Job& t, const TexState& e, const uint32_t* col, const float* xs, const float* ys, const float* const* vp) {
        float vc[4] = {0, 0, 0, 0};
        for (int k = 0; k < 3; k++) { uint32_t c = col[k]; for (int j = 0; j < 4; j++) vc[j] += ((c >> (8 * j)) & 255) / 765.0f; }
        float tc[4] = {1, 1, 1, 1}, mc[3];
        if (e.texture) {
            uint32_t a = e.avg ? e.avg : e.chain ? chain_avg(e.chain) : 0;
            if (!a) return;
            for (int j = 0; j < 4; j++) tc[j] = ((a >> (8 * j)) & 255) / 255.0f;
        }
        int tm = e.texture ? (int)(e.tf & 7) : 0;
        bool dbl = e.texture && ((e.tf >> 16) & 1);
        for (int j = 0; j < 3; j++) {   // the colour the GE writes (texture function and doubling)
            float v = tm == 3 ? tc[j] : tm == 4 ? tc[j] + vc[j] : tm == 0 ? tc[j] * vc[j] : vc[j] + (tc[j] - vc[j]) * tc[3];
            mc[j] = std::min(dbl ? v * 2.0f : v, 1.0f);
        }
        float W = (float)e.f.width, H = W * 272.0f / 480.0f;
        float ar = std::fabs((xs[1] - xs[0]) * (ys[2] - ys[0]) - (xs[2] - xs[0]) * (ys[1] - ys[0])) * 0.5f / (W * H);
        float cx = (vp[0][0] + vp[1][0] + vp[2][0]) / 3, cy = (vp[0][1] + vp[1][1] + vp[2][1]) / 3, cz = (vp[0][2] + vp[1][2] + vp[2][2]) / 3;
        if (!(ar > 0) || !(cz == cz)) return;
        ar = std::min(ar, 1.0f);
        bool add = e.blend && ((e.bmode >> 4) & 15) == 10 && ((e.bmode >> 8) & 15) == 0 && ((e.fixed_b & 255) + ((e.fixed_b >> 8) & 255) + ((e.fixed_b >> 16) & 255)) > 384;
        if (add && t.aura) { static int al = 0; if (al < 3) { al++; printf("cel: aura ignorada para la luz de efectos\n"); } return; }
        if (add) {
            double w = ar * (((e.bmode & 15) == 2) ? tc[3] * vc[3] : 1.0);
            for (int j = 0; j < 3; j++) vfx_cur.c[j] += tc[j] * vc[j] * w;
            vfx_cur.p[0] += cx * w; vfx_cur.p[1] += cy * w; vfx_cur.p[2] += cz * w; vfx_cur.w += w;
            vcl_add(cx, cy, cz, tc, vc, w, t.vh);
        } else {
            if (e.blend && !e.zwrite && !(wat_mesh_tex && e.t.dir == wat_mesh_tex)) {   // alpha-blended effects (auras, blasts, sparks) also light the map
                float mx = std::max(tc[0] * vc[0], std::max(tc[1] * vc[1], tc[2] * vc[2]));
                if (mx >= 0.5f) vcl_add(cx, cy, cz, tc, vc, ar * tc[3] * vc[3], t.vh);
            }
            float aw = e.blend ? ar * std::min(tc[3] * vc[3] * 3.0f, 1.0f) : ar;   // alpha-blended map draws count by their alpha
            if (aw > 0 && e.f.width >= 480 * scale_int()) {
                float cl = std::sqrt(cx * cx + cy * cy + cz * cz);
                int bi = cl > 1.0f ? std::min((int)(std::log2(cl) * 2.0f), 63) : 0;
                for (int j = 0; j < 3; j++) { amb_cur.c[j] += mc[j] * aw; sky_bin[bi][j] += mc[j] * aw; }
                amb_cur.w += aw; sky_bin[bi][3] += aw;
            }
            if (e.texture && ar > 1e-5f && toon_ground.size() < 30000)
                toon_ground.push_back({{cx, cy, cz}, {mc[0], mc[1], mc[2]}, t.vh});
        }
    };
    float ke = (float)scale_int();
    bool ground = false;
    if (TOON_BOUNCE > 0.0f && toon_mode() != 2) for (const Job& t : queue) if (t.toon) { ground = true; break; }
    bool need_map = !scn_held || ground || toon_env("TTT_CEL_SCENE_ALL", 0) > 0.5f;
    for (const Job& t : queue) {
        if (t.toon || !t.d3) continue;
        const TexState& e = states[t.st_tex];
        if (!need_map) {   // only effects still matter: additive draws and alpha-blended ones without depth
            bool add = e.blend && ((e.bmode >> 4) & 15) == 10 && ((e.bmode >> 8) & 15) == 0 && ((e.fixed_b & 255) + ((e.fixed_b >> 8) & 255) + ((e.fixed_b >> 16) & 255)) > 384;
            if (!add && !(e.blend && !e.zwrite)) continue;
        }
        if (t.cls == 2) {
            float sw = (float)e.f.width, sh = 512.0f * ke;
            for (int q = 0; q < t.y1; q += 3) {
                uint32_t col[3]; float xs[3], ys[3]; const float* vp[3]; bool ok = true;
                for (int k = 0; k < 3 && ok; k++) {
                    const VtxGL& g = gv_vtx[gv_idx[t.y0 + q + k]];
                    if (!(g.w > 1e-4f)) ok = false;
                    else { xs[k] = (g.x / g.w + 1.0f) * 0.5f * sw; ys[k] = (g.y / g.w + 1.0f) * 0.5f * sh; col[k] = g.col; vp[k] = &g.px; }
                }
                if (!ok || std::max(xs[0], std::max(xs[1], xs[2])) < e.sx0 || std::min(xs[0], std::min(xs[1], xs[2])) > e.sx1 ||
                    std::max(ys[0], std::max(ys[1], ys[2])) < e.sy0 || std::min(ys[0], std::min(ys[1], ys[2])) > e.sy1) continue;
                tri(t, e, col, xs, ys, vp);
            }
            continue;
        }
        if (t.cls) continue;
        uint32_t col[3]; float xs[3], ys[3]; const float* vp[3];
        for (int k = 0; k < 3; k++) { col[k] = e.gouraud ? t.v[k].color : t.col_flat; xs[k] = t.v[k].x; ys[k] = t.v[k].y; vp[k] = t.v[k].vp; }
        tri(t, e, col, xs, ys, vp);
    }
}
// fighter normals per vertex identity (raw model position: stable, joints merge), bent towards round part centres
static std::vector<float> toon_v;
static std::vector<uint32_t> toon_n, toon_l;
static float ol_A = 0, ol_B = 0; static bool ol_fit = false;
static inline uint32_t pack_n(float x, float y, float z) {
    auto q = [](float v) { float f = std::min(std::max(v, -1.0f), 1.0f) * 127.0f; int i = (int)f; float r = f - (float)i; if (r >= 0.5f) i++; else if (r <= -0.5f) i--; return (uint32_t)(uint8_t)(int8_t)i; };   // = lround, exact in [-127, 127]
    return q(x) | q(y) << 8 | q(z) << 16;
}
struct ToonGrp { double s[3], c[6]; int n; float cx, cy, cz, k; };
static void toon_normals() {
    static int fnv = -1, fnn = 0;
    static float b3fx = 0.6f;
    if ((fnn++ & 63) == 0) { const char* e = getenv("TTT_CEL_BT3NRM"); fnv = !(e && *e == '0'); b3fx = toon_env("TTT_CEL_BT3FX", 1) > 0.5f ? toon_env("TTT_CEL_BT3FXK", 0.6f) : 0.0f; }
    bool b3 = toon_mode() == 2, bt3n = fnv && b3;
    toon_v.assign(queue.size() * 6, -9.0f);
    toon_e.assign(queue.size() * 3, 0u);
    toon_n.assign(queue.size() * 3, 0u);
    toon_l.assign(queue.size(), 0u);
    static std::vector<std::array<float, 3>*> vref;
    vref.assign(queue.size() * 3, nullptr);
    bool angw = TOON_SMOOTH > 0.0f;
    struct K { float x, y, z; bool operator==(const K& o) const { return x == o.x && y == o.y && z == o.z; } };
    struct H { size_t operator()(const K& k) const { uint32_t a, b, c; memcpy(&a, &k.x, 4); memcpy(&b, &k.y, 4); memcpy(&c, &k.z, 4); return (size_t)(a * 73856093u ^ b * 19349663u ^ c * 83492791u); } };
    static std::unordered_map<K, std::array<float, 3>, H> accp;
    static std::unordered_map<uint32_t, std::array<float, 3>> acck;
    static std::unordered_map<uint32_t, ToonGrp> grp;
    accp.clear(); acck.clear(); grp.clear();
    auto acc = [](const float* p) -> std::array<float, 3>& {
        uint32_t k; memcpy(&k, &p[3], 4);
        if (k && TOON_MERGE > 0.0f) return acck[k];
        return accp[K{p[0], p[1], p[2]}];
    };
    bool any = false;
    for (size_t ti = 0; ti < queue.size(); ti++) {
        const Job& t = queue[ti];
        if (!t.toon || t.cls) continue;
        any = true;
        if (t.up[0] != 0 || t.up[1] != 0 || t.up[2] != 0) for (int j = 0; j < 3; j++) toon_up[j] = t.up[j];
        const float *a = t.v[0].vp, *b = t.v[1].vp, *c = t.v[2].vp;
        float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        float cx = a[0] + b[0] + c[0], cy = a[1] + b[1] + c[1], cz = a[2] + b[2] + c[2];
        if (nx * cx + ny * cy + nz * cz > 0) { nx = -nx; ny = -ny; nz = -nz; }
        if (angw) {   // corner angle weights: long thin triangles stop dominating the vertex normal
            float nn = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (nn > 1e-20f) {
                const float* P[3] = {a, b, c};
                for (int k = 0; k < 3; k++) {
                    const float *o = P[k], *p1 = P[(k + 1) % 3], *p2 = P[(k + 2) % 3];
                    float e1[3] = {p1[0] - o[0], p1[1] - o[1], p1[2] - o[2]}, e2[3] = {p2[0] - o[0], p2[1] - o[1], p2[2] - o[2]};
                    float l1 = e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2], l2 = e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2];
                    float cs = (l1 > 0 && l2 > 0) ? (e1[0] * e2[0] + e1[1] * e2[1] + e1[2] * e2[2]) / std::sqrt(l1 * l2) : 1.0f;
                    float w = std::acos(std::min(std::max(cs, -1.0f), 1.0f)) / nn;
                    auto& s = acc(t.v[k].vp); s[0] += nx * w; s[1] += ny * w; s[2] += nz * w; vref[ti * 3 + k] = &s;
                }
            }
        } else
            for (int k = 0; k < 3; k++) { auto& s = acc(t.v[k].vp); s[0] += nx; s[1] += ny; s[2] += nz; vref[ti * 3 + k] = &s; }
        if (t.grp && (TOON_SPHERE > 0.0f || t.face)) {
            ToonGrp& g = grp[t.grp];
            for (int k = 0; k < 3; k++) {
                const float* p = t.v[k].vp;
                g.s[0] += p[0]; g.s[1] += p[1]; g.s[2] += p[2];
                g.c[0] += p[0] * p[0]; g.c[1] += p[0] * p[1]; g.c[2] += p[0] * p[2]; g.c[3] += p[1] * p[1]; g.c[4] += p[1] * p[2]; g.c[5] += p[2] * p[2];
                g.n++;
            }
        }
    }
    if (!any) return;
    int its = std::min((int)std::ceil(TOON_SMOOTH - 1e-3f), 4);
    float frac = TOON_SMOOTH - (float)(its - 1);
    if (its > 0) {   // neighbour smoothing over the vertex identity graph: rounder terminator on low-poly parts
        static std::unordered_map<std::array<float, 3>*, int> id;
        static std::vector<int> vid;
        static std::vector<std::array<float, 3>> n0, n1;
        id.clear(); n0.clear(); vid.assign(queue.size() * 3, -1);
        for (size_t i = 0; i < vref.size(); i++) {
            if (!vref[i]) continue;
            auto r = id.try_emplace(vref[i], (int)n0.size());
            if (r.second) {
                std::array<float, 3> v = *vref[i]; float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                if (l > 1e-20f) for (int j = 0; j < 3; j++) v[j] /= l;
                n0.push_back(v);
            }
            vid[i] = r.first->second;
        }
        for (int it = 0; it < its; it++) {
            n1.resize(n0.size());
            for (size_t j = 0; j < n0.size(); j++) for (int c = 0; c < 3; c++) n1[j][c] = n0[j][c] * 2.0f;
            for (size_t i = 0; i + 2 < vid.size(); i += 3) {
                int a = vid[i], b = vid[i + 1], c = vid[i + 2];
                if (a < 0 || b < 0 || c < 0) continue;
                for (int j = 0; j < 3; j++) { n1[a][j] += n0[b][j] + n0[c][j]; n1[b][j] += n0[a][j] + n0[c][j]; n1[c][j] += n0[a][j] + n0[b][j]; }
            }
            for (size_t j = 0; j < n1.size(); j++) {
                float l = std::sqrt(n1[j][0] * n1[j][0] + n1[j][1] * n1[j][1] + n1[j][2] * n1[j][2]);
                if (l > 1e-20f) for (int c = 0; c < 3; c++) n1[j][c] /= l;
            }
            if (it == its - 1 && frac < 0.999f)   // fractional last pass: blend towards the smoothed normal
                for (size_t j = 0; j < n1.size(); j++) {
                    float v[3], l = 0;
                    for (int c = 0; c < 3; c++) { v[c] = n0[j][c] + (n1[j][c] - n0[j][c]) * frac; l += v[c] * v[c]; }
                    l = std::sqrt(l);
                    if (l > 1e-20f) for (int c = 0; c < 3; c++) n1[j][c] = v[c] / l;
                }
            n0.swap(n1);
        }
        for (auto& r : id) *r.first = n0[r.second];
    }
    for (auto& it : grp) {
        ToonGrp& g = it.second;
        double n = g.n, mx = g.s[0] / n, my = g.s[1] / n, mz = g.s[2] / n;
        double a = g.c[0] / n - mx * mx, b = g.c[1] / n - mx * my, c = g.c[2] / n - mx * mz, d = g.c[3] / n - my * my, e = g.c[4] / n - my * mz, f = g.c[5] / n - mz * mz;
        double vx = 1, vy = 0.7, vz = 0.4, l1 = 0;
        for (int i = 0; i < 12; i++) {
            double x = a * vx + b * vy + c * vz, y = b * vx + d * vy + e * vz, z = c * vx + e * vy + f * vz, l = std::sqrt(x * x + y * y + z * z);
            if (l < 1e-20) break;
            vx = x / l; vy = y / l; vz = z / l; l1 = l;
        }
        double tr = a + d + f, el = tr > 1e-20 ? l1 / tr : 1.0;
        float w = (float)((0.75 - el) / 0.4);
        g.cx = (float)mx; g.cy = (float)my; g.cz = (float)mz;
        g.k = TOON_SPHERE * (w < 0 ? 0 : w > 1 ? 1 : w);
    }
    struct Feet { float p[3], d, c[3]; bool ok; };
    static std::unordered_map<uint32_t, Feet> feet;
    feet.clear();
    for (const Job& t : queue) {
        if (!t.toon || t.cls) continue;
        Feet& f = feet.try_emplace(t.grp >> 8, Feet{{0, 0, 0}, 1e30f, {0, 0, 0}, false}).first->second;
        for (int k = 0; k < 3; k++) {
            const float* p = t.v[k].vp; float d = p[0] * t.up[0] + p[1] * t.up[1] + p[2] * t.up[2];
            if (d < f.d) { f.d = d; f.p[0] = p[0]; f.p[1] = p[1]; f.p[2] = p[2]; }
            fz_cur = std::max(fz_cur, std::fabs(p[2]));
        }
    }
    { static int nl = 0; if (nl < 2 && !feet.empty() && toon_ground.empty()) { nl++; printf("cel: sin muestras de suelo en este vaciado\n"); } }
    if (TOON_BOUNCE > 0.0f)
        for (auto& it : feet) {
            Feet& f = it.second;
            float R = 0.35f * std::fabs(f.p[2]) + 1e-3f, ws = 0, c[3] = {0, 0, 0};
            uint32_t vh = 0; for (const Job& t : queue) if (t.toon && !t.cls && (t.grp >> 8) == it.first) { vh = t.vh; break; }
            for (const ToonGround& gr : toon_ground) {
                if (gr.vh != vh) continue;
                float dx = gr.p[0] - f.p[0], dy = gr.p[1] - f.p[1], dz = gr.p[2] - f.p[2], d2 = dx * dx + dy * dy + dz * dz;
                if (d2 > R * R) continue;
                float w = 1.0f / (1.0f + d2 / (0.1f * R * R));
                for (int j = 0; j < 3; j++) c[j] += gr.c[j] * w;
                ws += w;
            }
            struct GS { float c[3]; uint32_t fno; };
            static std::unordered_map<uint32_t, GS> gsm;
            uint32_t who = 0; for (const Job& t : queue) if (t.toon && !t.cls && (t.grp >> 8) == it.first) { who = t.who; break; }
            GS& g = gsm.try_emplace(vh * 31u + who, GS{{0, 0, 0}, 0}).first->second;
            if (g.fno != toon_fno) {   // smoothed once per frame: walking from grass to dirt fades instead of jumping
                g.fno = toon_fno;
                for (int j = 0; j < 3; j++) g.c[j] += ((ws > 0 ? c[j] / ws : 0.0f) - g.c[j]) * 0.08f;
            }
            for (int j = 0; j < 3; j++) f.c[j] = g.c[j];
            f.ok = true;
            static int logs = 0;
            static uint32_t lf = 0;
            if (logs < 12 && toon_fno % 300 == 150 && lf != toon_fno) { logs++; lf = toon_fno; printf("cel: suelo bajo el luchador %u (%.2f %.2f %.2f), peso %.1f, muestras del frame %d\n", who, f.c[0], f.c[1], f.c[2], ws, (int)toon_ground.size()); }
        }
    double vw = vfx_prev.w;
    float vL[3] = {0, 0, 0}, vC[3] = {0, 0, 0}, vR = 1, vE = 0;
    if (vw > 1e-4 && TOON_VFX > 0.0f) {
        for (int j = 0; j < 3; j++) { vL[j] = (float)(vfx_prev.p[j] / vw); vC[j] = (float)(vfx_prev.c[j] / vw); }
        vR = 0.6f * std::fabs(vL[2]) + 1e-3f; vE = (float)std::min(1.0, vw * 8.0) * TOON_VFX;
    }
    if (b3 && b3fx > 0 && vE > 0.05f) { static int n = 0; static uint32_t lf = 0; if (n < 8 && toon_fno - lf > 120) { n++; lf = toon_fno; printf("cel: luz de VFX en Cel BT3, color (%.2f %.2f %.2f), energia %.2f, fuerza %.2f\n", vC[0], vC[1], vC[2], vE, b3fx); } }
    {   // GE depth vs view depth of the fighters (z = A + B / vz): linear depth for the absolute outline
        double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (const Job& t : queue) {
            if (!t.fig || t.cls) continue;
            for (int k = 0; k < 3; k++) { float vz = t.v[k].vp[2]; if (std::fabs(vz) < 1e-4f) continue; double x = 1.0 / vz, y = t.v[k].z; n++; sx += x; sy += y; sxx += x * x; sxy += x * y; }
        }
        double dn = n * sxx - sx * sx;
        if (n > 30 && std::fabs(dn) > 1e-12 * n * n) { ol_B = (float)((n * sxy - sx * sy) / dn); ol_A = (float)((sy - ol_B * sx) / n); ol_fit = true; }
    }
    for (size_t i = 0; i < queue.size(); i++) {
        const Job& t = queue[i];
        if (!t.toon || t.cls) continue;
        const ToonGrp* g = nullptr;
        if (t.grp && (TOON_SPHERE > 0.0f || t.face)) { auto it = grp.find(t.grp); if (it != grp.end()) g = &it->second; }
        for (int k = 0; k < 3; k++) {
            const float* p = t.v[k].vp;
            const auto& sn = acc(p);
            float nx = sn[0], ny = sn[1], nz = sn[2], l = std::sqrt(nx * nx + ny * ny + nz * nz);
            float pl = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
            bool own = bt3n && t.v[0].fn && t.v[1].fn && t.v[2].fn;   // Cel BT3: the model's own normals (BT3), steady like in BT3
            if (own) { float f[3]; fn_unpack(t.v[k].fn, f); nx = f[0]; ny = f[1]; nz = f[2]; l = std::sqrt(nx * nx + ny * ny + nz * nz); }
            if (l < 1e-12f || pl < 1e-12f) continue;
            nx /= l; ny /= l; nz /= l;
            float sk = own ? 0.0f : g ? (t.face ? 0.5f : g->k) : 0.0f;
            if (sk > 0.0f) {
                float rx = p[0] - g->cx, ry = p[1] - g->cy, rz = p[2] - g->cz, rl = std::sqrt(rx * rx + ry * ry + rz * rz);
                if (rl > 1e-9f) {
                    nx = nx * (1 - sk) + rx / rl * sk; ny = ny * (1 - sk) + ry / rl * sk; nz = nz * (1 - sk) + rz / rl * sk;
                    float m = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (m > 1e-9f) { nx /= m; ny /= m; nz /= m; }
                }
            }
            float vx = -p[0] / pl, vy = -p[1] / pl, vz = -p[2] / pl;
            float lx = t.lv[0] + TOON_FILL * vx, ly = t.lv[1] + TOON_FILL * vy, lz = t.lv[2] + TOON_FILL * vz, ll = std::sqrt(lx * lx + ly * ly + lz * lz);
            toon_v[i * 6 + k * 2] = ll > 1e-6f ? (nx * lx + ny * ly + nz * lz) / ll : 0.0f;
            toon_v[i * 6 + k * 2 + 1] = nx * vx + ny * vy + nz * vz + (t.face ? 4.0f : 0.0f);
            toon_n[i * 3 + k] = pack_n(nx, ny, nz);
            if (k == 0) toon_l[i] = pack_n(t.lv[0], t.lv[1], t.lv[2]);
            float ex[3] = {0, 0, 0}, bs = 0, vs = 0;
            auto fit = feet.find(t.grp >> 8);
            if (!b3 && fit != feet.end() && fit->second.ok) {
                float b = (TOON_WRAP - (nx * t.up[0] + ny * t.up[1] + nz * t.up[2])) / (1.0f + TOON_WRAP);
                if (b > 0) for (int j = 0; j < 3; j++) { float q = fit->second.c[j] * b * TOON_BOUNCE; ex[j] += q; bs += q; }
            }
            if (bolt_k > 0 && !b3) {   // lightning: from the sky
                float d = nx * bolt_dir[0] + ny * bolt_dir[1] + nz * bolt_dir[2];
                if (d > 0) for (int j = 0; j < 3; j++) { float q = bolt_rgb[j] * bolt_k * d; ex[j] += q; vs += q; }
            }
            if (vE > 0) {
                float dx = vL[0] - p[0], dy = vL[1] - p[1], dz = vL[2] - p[2], d = std::sqrt(dx * dx + dy * dy + dz * dz);
                float wr = d > 1e-6f ? (nx * dx + ny * dy + nz * dz) / d * 0.5f + 0.5f : 1.0f;
                float at = vE * wr / (1.0f + d * d / (vR * vR)) * (b3 ? b3fx : 1.0f);
                for (int j = 0; j < 3; j++) { ex[j] += vC[j] * at; vs += vC[j] * at; }
            }
            uint32_t pk = 0;
            for (int j = 0; j < 3; j++) { int q = (int)(ex[j] * 255.0f + 0.5f); pk |= (uint32_t)(q < 0 ? 0 : q > 255 ? 255 : q) << (8 * j); }
            if (bs > 0) pk |= (uint32_t)(int)(bs / (bs + vs) * 255.0f + 0.5f) << 24;
            toon_e[i * 3 + k] = pk;
        }
    }
}

// cel normals of the GPU vertex path: same math as toon_normals, once per unique vertex (identity shared across draws)
template <class KT, class HT> struct TnMap {   // open addressing, cleared by generation: same results as try_emplace
    std::vector<KT> k; std::vector<uint32_t> g; std::vector<int> v; uint32_t gen = 0, mask = 0, n = 0;
    void clear() { n = 0; if (++gen == 0) { std::fill(g.begin(), g.end(), 0u); gen = 1; } }
    void grow() {
        std::vector<KT> ok; std::vector<uint32_t> og; std::vector<int> ov; ok.swap(k); og.swap(g); ov.swap(v);
        size_t c = ok.empty() ? (1u << 15) : ok.size() * 2;
        k.resize(c); g.assign(c, 0u); v.resize(c); mask = (uint32_t)c - 1; n = 0;
        for (size_t i = 0; i < ok.size(); i++) if (og[i] == gen) put(ok[i], ov[i]);
    }
    void put(const KT& kk, int vv) { uint32_t h = (uint32_t)HT()(kk) & mask; while (g[h] == gen) h = (h + 1) & mask; g[h] = gen; k[h] = kk; v[h] = vv; n++; }
    int get(const KT& kk, int vv) {
        if (2 * (n + 1) > mask) grow();
        uint32_t h = (uint32_t)HT()(kk) & mask;
        while (g[h] == gen) { if (k[h] == kk) return v[h]; h = (h + 1) & mask; }
        g[h] = gen; k[h] = kk; v[h] = vv; n++; return vv;
    }
};
struct TnH32 { size_t operator()(uint32_t a) const { return (size_t)(a * 2654435761u); } };
static void toon_normals_gv() {
    bool any = false;
    for (const Job& t : queue) if (t.cls == 2 && t.toon) { any = true; break; }
    if (!any) return;
    static int fnv = -1, fnn = 0;
    static float b3fx = 0.6f;
    if ((fnn++ & 63) == 0) { const char* e = getenv("TTT_CEL_BT3NRM"); fnv = !(e && *e == '0'); b3fx = toon_env("TTT_CEL_BT3FX", 1) > 0.5f ? toon_env("TTT_CEL_BT3FXK", 0.6f) : 0.0f; }
    bool b3 = toon_mode() == 2, bt3n = fnv && b3, angw = TOON_SMOOTH > 0.0f;
    struct K { float x, y, z; bool operator==(const K& o) const { return x == o.x && y == o.y && z == o.z; } };
    struct H { size_t operator()(const K& k) const { uint32_t a, b, c; memcpy(&a, &k.x, 4); memcpy(&b, &k.y, 4); memcpy(&c, &k.z, 4); return (size_t)(a * 73856093u ^ b * 19349663u ^ c * 83492791u); } };
    static TnMap<K, H> mp;
    static TnMap<uint32_t, TnH32> mk;
    static std::unordered_map<uint32_t, ToonGrp> grp;
    static std::vector<int> vid, tri;
    static std::vector<std::array<float, 3>> acc, n1;
    mp.clear(); mk.clear(); grp.clear(); acc.clear(); tri.clear();
    vid.assign(gv_vtx.size(), -1);
    for (const Job& t : queue) {
        if (t.cls != 2 || !t.toon) continue;
        if (t.up[0] != 0 || t.up[1] != 0 || t.up[2] != 0) for (int j = 0; j < 3; j++) toon_up[j] = t.up[j];
        for (int v = t.x0; v < t.x0 + t.x1; v++) {
            const VtxGL& g = gv_vtx[v]; uint32_t k = gv_x[v].key;
            int r = k && TOON_MERGE > 0.0f ? mk.get(k, (int)acc.size()) : mp.get(K{g.px, g.py, g.pz}, (int)acc.size());
            if (r == (int)acc.size()) acc.push_back({0, 0, 0});
            vid[v] = r;
        }
        bool sg = t.grp && (TOON_SPHERE > 0.0f || t.face);
        ToonGrp* gp = sg ? &grp[t.grp] : nullptr;
        const float* tnp = t.ag >= 0 && t.x1 > 0 && gv_agv[t.ag].tn ? gv_tn + (size_t)t.y0 / 3 * 6 : nullptr;
        for (int q = 0; q + 2 < t.y1; q += 3) {
            uint32_t ix[3] = {gv_idx[t.y0 + q], gv_idx[t.y0 + q + 1], gv_idx[t.y0 + q + 2]};
            if (tnp) {
                const float* T = tnp + (size_t)q * 2;
                float nx = T[0], ny = T[1], nz = T[2];
                bool ok = true;
                if (angw) { if (!std::isnan(T[3])) for (int k = 0; k < 3; k++) { auto& s = acc[vid[ix[k]]]; s[0] += nx * T[3 + k]; s[1] += ny * T[3 + k]; s[2] += nz * T[3 + k]; } else ok = false; }
                else for (int k = 0; k < 3; k++) { auto& s = acc[vid[ix[k]]]; s[0] += nx; s[1] += ny; s[2] += nz; }
                if (ok) { tri.push_back(vid[ix[0]]); tri.push_back(vid[ix[1]]); tri.push_back(vid[ix[2]]); }
                if (gp) for (int k = 0; k < 3; k++) {
                    const float* p = &gv_vtx[ix[k]].px;
                    gp->s[0] += p[0]; gp->s[1] += p[1]; gp->s[2] += p[2];
                    gp->c[0] += p[0] * p[0]; gp->c[1] += p[0] * p[1]; gp->c[2] += p[0] * p[2]; gp->c[3] += p[1] * p[1]; gp->c[4] += p[1] * p[2]; gp->c[5] += p[2] * p[2];
                    gp->n++;
                }
                continue;
            }
            const float *a = &gv_vtx[ix[0]].px, *b = &gv_vtx[ix[1]].px, *c = &gv_vtx[ix[2]].px;
            float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
            float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
            float cx = a[0] + b[0] + c[0], cy = a[1] + b[1] + c[1], cz = a[2] + b[2] + c[2];
            if (nx * cx + ny * cy + nz * cz > 0) { nx = -nx; ny = -ny; nz = -nz; }
            bool ok = true;
            if (angw) {
                float nn = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (nn > 1e-20f) {
                    const float* P[3] = {a, b, c};
                    for (int k = 0; k < 3; k++) {
                        const float *o = P[k], *p1 = P[(k + 1) % 3], *p2 = P[(k + 2) % 3];
                        float e1[3] = {p1[0] - o[0], p1[1] - o[1], p1[2] - o[2]}, e2[3] = {p2[0] - o[0], p2[1] - o[1], p2[2] - o[2]};
                        float l1 = e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2], l2 = e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2];
                        float cs = (l1 > 0 && l2 > 0) ? (e1[0] * e2[0] + e1[1] * e2[1] + e1[2] * e2[2]) / std::sqrt(l1 * l2) : 1.0f;
                        float w = std::acos(std::min(std::max(cs, -1.0f), 1.0f)) / nn;
                        auto& s = acc[vid[ix[k]]]; s[0] += nx * w; s[1] += ny * w; s[2] += nz * w;
                    }
                } else ok = false;
            } else for (int k = 0; k < 3; k++) { auto& s = acc[vid[ix[k]]]; s[0] += nx; s[1] += ny; s[2] += nz; }
            if (ok) { tri.push_back(vid[ix[0]]); tri.push_back(vid[ix[1]]); tri.push_back(vid[ix[2]]); }
            if (gp) for (int k = 0; k < 3; k++) {
                const float* p = &gv_vtx[ix[k]].px;
                gp->s[0] += p[0]; gp->s[1] += p[1]; gp->s[2] += p[2];
                gp->c[0] += p[0] * p[0]; gp->c[1] += p[0] * p[1]; gp->c[2] += p[0] * p[2]; gp->c[3] += p[1] * p[1]; gp->c[4] += p[1] * p[2]; gp->c[5] += p[2] * p[2];
                gp->n++;
            }
        }
    }
    int its = std::min((int)std::ceil(TOON_SMOOTH - 1e-3f), 4);
    float frac = TOON_SMOOTH - (float)(its - 1);
    if (its > 0) {
        for (auto& v : acc) { float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); if (l > 1e-20f) for (int j = 0; j < 3; j++) v[j] /= l; }
        for (int it = 0; it < its; it++) {
            n1.resize(acc.size());
            for (size_t j = 0; j < acc.size(); j++) for (int c = 0; c < 3; c++) n1[j][c] = acc[j][c] * 2.0f;
            for (size_t i = 0; i + 2 < tri.size(); i += 3) {
                int a = tri[i], b = tri[i + 1], c = tri[i + 2];
                for (int j = 0; j < 3; j++) { n1[a][j] += acc[b][j] + acc[c][j]; n1[b][j] += acc[a][j] + acc[c][j]; n1[c][j] += acc[a][j] + acc[b][j]; }
            }
            for (size_t j = 0; j < n1.size(); j++) {
                float l = std::sqrt(n1[j][0] * n1[j][0] + n1[j][1] * n1[j][1] + n1[j][2] * n1[j][2]);
                if (l > 1e-20f) for (int c = 0; c < 3; c++) n1[j][c] /= l;
            }
            if (it == its - 1 && frac < 0.999f)
                for (size_t j = 0; j < n1.size(); j++) {
                    float v[3], l = 0;
                    for (int c = 0; c < 3; c++) { v[c] = acc[j][c] + (n1[j][c] - acc[j][c]) * frac; l += v[c] * v[c]; }
                    l = std::sqrt(l);
                    if (l > 1e-20f) for (int c = 0; c < 3; c++) n1[j][c] = v[c] / l;
                }
            acc.swap(n1);
        }
    }
    for (auto& it : grp) {
        ToonGrp& g = it.second;
        double n = g.n, mx = g.s[0] / n, my = g.s[1] / n, mz = g.s[2] / n;
        double a = g.c[0] / n - mx * mx, b = g.c[1] / n - mx * my, c = g.c[2] / n - mx * mz, d = g.c[3] / n - my * my, e = g.c[4] / n - my * mz, f = g.c[5] / n - mz * mz;
        double vx = 1, vy = 0.7, vz = 0.4, l1 = 0;
        for (int i = 0; i < 12; i++) {
            double x = a * vx + b * vy + c * vz, y = b * vx + d * vy + e * vz, z = c * vx + e * vy + f * vz, l = std::sqrt(x * x + y * y + z * z);
            if (l < 1e-20) break;
            vx = x / l; vy = y / l; vz = z / l; l1 = l;
        }
        double tr = a + d + f, el = tr > 1e-20 ? l1 / tr : 1.0;
        float w = (float)((0.75 - el) / 0.4);
        g.cx = (float)mx; g.cy = (float)my; g.cz = (float)mz;
        g.k = TOON_SPHERE * (w < 0 ? 0 : w > 1 ? 1 : w);
    }
    struct Feet { float p[3], d, c[3]; bool ok; uint32_t vh, who; };
    static std::unordered_map<uint32_t, Feet> feet;
    feet.clear();
    for (const Job& t : queue) {
        if (t.cls != 2 || !t.toon) continue;
        Feet& f = feet.try_emplace(t.grp >> 8, Feet{{0, 0, 0}, 1e30f, {0, 0, 0}, false, t.vh, t.who}).first->second;
        for (int v = t.x0; v < t.x0 + t.x1; v++) {
            const float* p = &gv_vtx[v].px; float d = p[0] * t.up[0] + p[1] * t.up[1] + p[2] * t.up[2];
            if (d < f.d) { f.d = d; f.p[0] = p[0]; f.p[1] = p[1]; f.p[2] = p[2]; }
            fz_cur = std::max(fz_cur, std::fabs(p[2]));
        }
    }
    if (TOON_BOUNCE > 0.0f)
        for (auto& it : feet) {
            Feet& f = it.second;
            float R = 0.35f * std::fabs(f.p[2]) + 1e-3f, ws = 0, c[3] = {0, 0, 0};
            for (const ToonGround& gr : toon_ground) {
                if (gr.vh != f.vh) continue;
                float dx = gr.p[0] - f.p[0], dy = gr.p[1] - f.p[1], dz = gr.p[2] - f.p[2], d2 = dx * dx + dy * dy + dz * dz;
                if (d2 > R * R) continue;
                float w = 1.0f / (1.0f + d2 / (0.1f * R * R));
                for (int j = 0; j < 3; j++) c[j] += gr.c[j] * w;
                ws += w;
            }
            struct GS { float c[3]; uint32_t fno; };
            static std::unordered_map<uint32_t, GS> gsm;
            GS& g = gsm.try_emplace(f.vh * 31u + f.who, GS{{0, 0, 0}, 0}).first->second;
            if (g.fno != toon_fno) { g.fno = toon_fno; for (int j = 0; j < 3; j++) g.c[j] += ((ws > 0 ? c[j] / ws : 0.0f) - g.c[j]) * 0.08f; }
            for (int j = 0; j < 3; j++) f.c[j] = g.c[j];
            f.ok = true;
        }
    double vw = vfx_prev.w;
    float vL[3] = {0, 0, 0}, vC[3] = {0, 0, 0}, vR = 1, vE = 0;
    if (vw > 1e-4 && TOON_VFX > 0.0f) {
        for (int j = 0; j < 3; j++) { vL[j] = (float)(vfx_prev.p[j] / vw); vC[j] = (float)(vfx_prev.c[j] / vw); }
        vR = 0.6f * std::fabs(vL[2]) + 1e-3f; vE = (float)std::min(1.0, vw * 8.0) * TOON_VFX;
    }
    {
        double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (const Job& t : queue) {
            if (t.cls != 2 || !t.fig) continue;
            for (int v = t.x0; v < t.x0 + t.x1; v++) {
                const VtxGL& g = gv_vtx[v]; float vz = g.pz;
                if (std::fabs(vz) < 1e-4f || !(g.w > 1e-4f)) continue;
                double x = 1.0 / vz, y = (g.z / g.w + 1.0) * 0.5 * 65535.0; n++; sx += x; sy += y; sxx += x * x; sxy += x * y;
            }
        }
        double dn = n * sxx - sx * sx;
        if (n > 30 && std::fabs(dn) > 1e-12 * n * n) { ol_B = (float)((n * sxy - sx * sy) / dn); ol_A = (float)((sy - ol_B * sx) / n); ol_fit = true; }
    }
    for (const Job& t : queue) {
        if (t.cls != 2 || !t.toon) continue;
        const ToonGrp* g = nullptr;
        if (t.grp && (TOON_SPHERE > 0.0f || t.face)) { auto it = grp.find(t.grp); if (it != grp.end()) g = &it->second; }
        auto fit = feet.find(t.grp >> 8);
        uint32_t lg = pack_n(t.lv[0], t.lv[1], t.lv[2]);
        for (int v = t.x0; v < t.x0 + t.x1; v++) {
            VtxGL& o = gv_vtx[v];
            o.tl = o.tf = -9.0f; o.nrm = 0; o.ext = 0; o.lgt = lg;
            const float p[3] = {o.px, o.py, o.pz};
            const auto& sn = acc[vid[v]];
            float nx = sn[0], ny = sn[1], nz = sn[2], l = std::sqrt(nx * nx + ny * ny + nz * nz);
            float pl = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
            bool own = bt3n && gv_x[v].fn;
            if (own) { float f[3]; fn_unpack(gv_x[v].fn, f); nx = f[0]; ny = f[1]; nz = f[2]; l = std::sqrt(nx * nx + ny * ny + nz * nz); }
            if (l < 1e-12f || pl < 1e-12f) continue;
            nx /= l; ny /= l; nz /= l;
            float sk = own ? 0.0f : g ? (t.face ? 0.5f : g->k) : 0.0f;
            if (sk > 0.0f) {
                float rx = p[0] - g->cx, ry = p[1] - g->cy, rz = p[2] - g->cz, rl = std::sqrt(rx * rx + ry * ry + rz * rz);
                if (rl > 1e-9f) {
                    nx = nx * (1 - sk) + rx / rl * sk; ny = ny * (1 - sk) + ry / rl * sk; nz = nz * (1 - sk) + rz / rl * sk;
                    float m = std::sqrt(nx * nx + ny * ny + nz * nz);
                    if (m > 1e-9f) { nx /= m; ny /= m; nz /= m; }
                }
            }
            float vx = -p[0] / pl, vy = -p[1] / pl, vz = -p[2] / pl;
            float lx = t.lv[0] + TOON_FILL * vx, ly = t.lv[1] + TOON_FILL * vy, lz = t.lv[2] + TOON_FILL * vz, ll = std::sqrt(lx * lx + ly * ly + lz * lz);
            o.tl = ll > 1e-6f ? (nx * lx + ny * ly + nz * lz) / ll : 0.0f;
            o.tf = nx * vx + ny * vy + nz * vz + (t.face ? 4.0f : 0.0f);
            o.nrm = pack_n(nx, ny, nz);
            float ex[3] = {0, 0, 0}, bs = 0, vs = 0;
            if (!b3 && fit != feet.end() && fit->second.ok) {
                float b = (TOON_WRAP - (nx * t.up[0] + ny * t.up[1] + nz * t.up[2])) / (1.0f + TOON_WRAP);
                if (b > 0) for (int j = 0; j < 3; j++) { float q = fit->second.c[j] * b * TOON_BOUNCE; ex[j] += q; bs += q; }
            }
            if (bolt_k > 0 && !b3) {
                float d = nx * bolt_dir[0] + ny * bolt_dir[1] + nz * bolt_dir[2];
                if (d > 0) for (int j = 0; j < 3; j++) { float q = bolt_rgb[j] * bolt_k * d; ex[j] += q; vs += q; }
            }
            if (vE > 0) {
                float dx = vL[0] - p[0], dy = vL[1] - p[1], dz = vL[2] - p[2], d = std::sqrt(dx * dx + dy * dy + dz * dz);
                float wr = d > 1e-6f ? (nx * dx + ny * dy + nz * dz) / d * 0.5f + 0.5f : 1.0f;
                float at = vE * wr / (1.0f + d * d / (vR * vR)) * (b3 ? b3fx : 1.0f);
                for (int j = 0; j < 3; j++) { ex[j] += vC[j] * at; vs += vC[j] * at; }
            }
            uint32_t pk = 0;
            for (int j = 0; j < 3; j++) { int q = (int)(ex[j] * 255.0f + 0.5f); pk |= (uint32_t)(q < 0 ? 0 : q > 255 ? 255 : q) << (8 * j); }
            if (bs > 0) pk |= (uint32_t)(int)(bs / (bs + vs) * 255.0f + 0.5f) << 24;
            o.ext = pk;
        }
    }
}

// absolute outline (BT1 style): lines inside one outline layer where the depth jumps (fingers over the palm), written into
// the game's edge mask before its second pass, so they get the game's own colour
static const char* OL_FS =
    "#version 330 core\n"
    "uniform sampler2D col0; uniform sampler2D dep0; uniform vec4 olp; uniform vec4 olr; uniform vec4 olm[2]; uniform vec4 olz[64]; uniform sampler2D msk; out vec4 o;\n"
    "bool gedge(ivec2 p){ vec4 c = texelFetch(msk, p, 0); uint g = uint(c.g*255.0 + 0.5) >> 2, b = uint(c.b*255.0 + 0.5) >> 3; return ((g | (b << 6)) & 255u) != 0u; }\n"
    "float lin(float d){ return abs(olp.y / (d*65535.0 - olp.x)); }\n"
    "vec2 zr(int id){ vec4 v = olz[id >> 1]; return (id & 1) != 0 ? v.zw : v.xy; }\n"
    "bool fighter(int id, float l){ int w = int(olm[id >> 6][(id >> 4) & 3]); if(((w >> (id & 15)) & 1) == 0) return false; vec2 z = zr(id); return l > z.x*0.97 - 1.0 && l < z.y*1.03 + 1.0; }\n"
    "int idat(ivec2 p){ return int(texelFetch(col0, p, 0).a*255.0 + 0.5) & 127; }\n"
    "void main(){\n"
    "  ivec2 p = ivec2(gl_FragCoord.xy), mx = ivec2(olr.yz) - 1;\n"
    "  int id = idat(p); if(id == 0) discard;\n"
    "  float lc = lin(texelFetch(dep0, p, 0).r); if(!fighter(id, lc)) discard;\n"
    "  const vec2 D[8] = vec2[8](vec2(1,0),vec2(-1,0),vec2(0,1),vec2(0,-1),vec2(0.7071,0.7071),vec2(-0.7071,0.7071),vec2(0.7071,-0.7071),vec2(-0.7071,-0.7071));\n"
    "  for(int i=0;i<8;i++){\n"
    "    ivec2 q = clamp(p + ivec2(round(D[i]*olr.x)), ivec2(0), mx), r = clamp(p - ivec2(round(D[i]*olr.x)), ivec2(0), mx);\n"
    "    int iq = idat(q); float ln = lin(texelFetch(dep0, q, 0).r);\n"
    "    if(iq != id){ if(!fighter(iq, ln) && ln - lc > olp.z*4.0 && !gedge(p) && !gedge(q) && !gedge(clamp(p + (q - p)*2, ivec2(0), mx))){ o = vec4(0.0, 1.0, 0.0, 1.0); return; } continue; }\n"
    "    float lo = idat(r) == id ? lin(texelFetch(dep0, r, 0).r) : lc;\n"
    "    if(2.0*lc - lo - ln > olp.z){ o = vec4(0.0, 1.0, 0.0, 1.0); return; }\n"
    "  }\n"
    "  discard;\n"
    "}\n";
static GLuint ol_prog = 0, ol_dfbo = 0, ol_dtex = 0; static int ol_dw = 0, ol_dh = 0;
static GLint ol_u_col = -1, ol_u_dep = -1, ol_u_p = -1, ol_u_r = -1, ol_u_m = -1, ol_u_z = -1, ol_u_msk = -1;
static DestGL* ol_last_b = nullptr; static bool ol_dirty = false;   // the mask was rewritten by the game's first pass
void psp_window_target(int* w, int* h);
static float outline_width();
static int ol_mode() {
    static int v = 1, n = 0;
    if ((n++ & 63) == 0) { const char* e = getenv("TTT_OUTLINE_MODE"); v = e && *e ? atoi(e) : 1; }
    return v;
}
static void ol_pass(DestGL* d, DestGL* b) {
    GqZone gqz_(GQ_OL);
    if (!ol_prog) {
        ol_prog = gl_program(WAT_VS, OL_FS);
        if (!ol_prog) { printf("borde: no pude preparar el programa del delineado absoluto\n"); ol_prog = 0xFFFFFFFFu; return; }
        ol_u_col = glGetUniformLocation(ol_prog, "col0"); ol_u_dep = glGetUniformLocation(ol_prog, "dep0");
        ol_u_p = glGetUniformLocation(ol_prog, "olp"); ol_u_r = glGetUniformLocation(ol_prog, "olr"); ol_u_m = glGetUniformLocation(ol_prog, "olm"); ol_u_z = glGetUniformLocation(ol_prog, "olz"); ol_u_msk = glGetUniformLocation(ol_prog, "msk");
        if (!wat_vao) glGenVertexArrays(1, &wat_vao);
    }
    if (ol_prog == 0xFFFFFFFFu) return;
    if (ol_dw != d->w || ol_dh != d->h) {
        if (!ol_dtex) { glGenTextures(1, &ol_dtex); glGenFramebuffers(1, &ol_dfbo); }
        glBindTexture(GL_TEXTURE_2D, ol_dtex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, d->w, d->h, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, ol_dfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, ol_dtex, 0);
        glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        printf("borde: profundidad para el delineado absoluto %dx%d %s\n", d->w, d->h, st == GL_FRAMEBUFFER_COMPLETE ? "lista" : "incompleta");
        if (st != GL_FRAMEBUFFER_COMPLETE) { ol_prog = 0xFFFFFFFFu; glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : d->fbo); return; }
        ol_dw = d->w; ol_dh = d->h;
    }
    glr_resolve(d);
    static int ol_direct = -1;
    if (ol_direct < 0) { const char* e = getenv("TTT_OL_DIRECT"); ol_direct = e && *e ? atoi(e) : 0; }
    bool odir = d->dtex && ol_direct > 0;
    if (odir) dd_n[0]++; else {
    dd_n[1]++;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ol_dfbo);
    glBlitFramebuffer(0, 0, d->w, d->h, 0, 0, d->w, d->h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, b->fbo);
    glActiveTexture(GL_TEXTURE0 + 6);
    GLuint mcopy = glr_copy_fb(b);   // the game's own edges: the exterior line is added only where it has none
    glBindTexture(GL_TEXTURE_2D, mcopy);
    int dw = 0, dh = 0; psp_window_target(&dw, &dh);
    float wpx = dw > 0 ? 480.0f * scale_int() / dw : 1.0f, r = std::max(1.0f, std::floor(outline_width() * 0.5f * wpx + 0.5f));
    float th = toon_env("TTT_OUTLINE_DEPTH", 0.35f);
    int vw = std::min(d->w, b->w), vh = std::min(d->h, b->h);
    glUseProgram(ol_prog);
    glBindVertexArray(wat_vao);
    glActiveTexture(GL_TEXTURE0 + 4); glBindTexture(GL_TEXTURE_2D, d->tex);
    glActiveTexture(GL_TEXTURE0 + 5); glBindTexture(GL_TEXTURE_2D, odir ? d->dtex : ol_dtex);
    glUniform1i(ol_u_col, 4); glUniform1i(ol_u_dep, 5); glUniform1i(ol_u_msk, 6);
    float mk[8], zr[256]; int nid = 0, nbg = 0;
    uint16_t bg7 = (uint16_t)(1u << 15);   // alpha 0xFF masked: plain scenery
    for (int j = 0; j < 8; j++) {
        uint16_t m = ol_ids[j] & ~ol_bg[j] & (j == 7 ? ~bg7 : 0xFFFF);
        mk[j] = (float)m;
        for (int b2 = 0; b2 < 16; b2++) { nid += (m >> b2) & 1; nbg += (ol_ids[j] >> b2) & (ol_bg[j] >> b2) & 1; }
    }
    for (int i = 0; i < 128; i++) { zr[i * 2] = ol_zr[i][0]; zr[i * 2 + 1] = ol_zr[i][1]; }
    glUniform4fv(ol_u_m, 2, mk);
    glUniform4fv(ol_u_z, 64, zr);
    glUniform4f(ol_u_p, ol_A, ol_B, th, 0.0f);
    glUniform4f(ol_u_r, r, (float)vw, (float)vh, 0.0f);
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST); glStencilMask(0); glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, vw, vh);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0 + 5); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : d->fbo);
    glUseProgram(glr_prog);
    glBindVertexArray(glr_vao);
    static int logs = 0;
    if (logs < 6) { logs++; printf("borde: delineado absoluto sobre 0x%08X (mascara 0x%08X), linea %.0f px, profundidad z = %.1f + %.1f / vz, umbral %.2f, capas de luchadores %d, compartidas con el escenario %d\n", d->dir, b->dir, r, ol_A, ol_B, th, nid, nbg); }
}

// smooth motion (item 70): per-vertex screen motion of the opaque scene, written into a motion buffer at present
struct SmV { float x, y, z, w, px, py, pw, kn; int dst, zw; };
static std::vector<SmV> sm_mv, sm_up;
static uint64_t sm_kn = 0, sm_vn = 0; static double sm_mvsum = 0;
static GLuint sm_scn_tex = 0, sm_scn_fbo = 0; static int sm_scn_w = 0, sm_scn_h = 0, sm_scn_dst = -1; static uint32_t sm_scn_f = 0xFFFFFFFFu;

static void sm_add(const Job& t, float sw, float sh) {
    bool kn = t.v[0].mk && t.v[1].mk && t.v[2].mk;
    const TexState& e = states[t.st_tex];
    for (int k = 0; k < 3; k++) {
        const VP& p = t.v[k];
        SmV o;
        float w = (p.iw > 1e-20f || p.iw < -1e-20f) ? 1.0f / p.iw : 1.0f;
        o.x = (2.0f * p.x / sw - 1.0f) * w; o.y = (2.0f * p.y / sh - 1.0f) * w; o.z = (2.0f * (p.z / 65535.0f) - 1.0f) * w; o.w = w;
        if (kn) { o.pw = p.mw; o.px = (2.0f * p.mx / sw - 1.0f) * p.mw; o.py = (2.0f * p.my / sh - 1.0f) * p.mw; sm_mvsum += std::fabs(p.x - p.mx) + std::fabs(p.y - p.my); }
        else { o.px = o.x; o.py = o.y; o.pw = o.w; }
        o.kn = kn ? 1.0f : 0.0f;
        o.dst = glr_current ? (int)(glr_current - glr_dest) : -1;
        o.zw = e.zwrite && e.ztest ? 1 : 0;
        sm_mv.push_back(o);
    }
    sm_vn += 3; if (kn) sm_kn += 3;
}

// the finished scene before the HUD: pixels that differ from it at present are HUD and do not move
static void sm_scene_copy(DestGL* d) {
    if (!d) return;
    glr_resolve(d);
    int w = d->w, h = 272 * scale_int();
    if (sm_scn_w != w || sm_scn_h != h) {
        if (!sm_scn_tex) { glGenTextures(1, &sm_scn_tex); glGenFramebuffers(1, &sm_scn_fbo); }
        glBindTexture(GL_TEXTURE_2D, sm_scn_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, sm_scn_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sm_scn_tex, 0);
        sm_scn_w = w; sm_scn_h = h;
    }
    bool sc; { SubT q_(glq_sid()); sc = glIsEnabled(GL_SCISSOR_TEST); }
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sm_scn_fbo);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
    if (sc) glEnable(GL_SCISSOR_TEST);
    sm_scn_dst = (int)(d - glr_dest); sm_scn_f = toon_fno;
}

static void lj_cmp(uint32_t a, uint32_t b) {   // probe: what differs between consecutive states of loose 2D jobs
    if (a >= states.size() || b >= states.size()) { cp_lj[4]++; return; }
    const TexState& x = states[a]; const TexState& y = states[b];
    bool g[8];
    g[0] = x.f.dir != y.f.dir || x.f.width != y.f.width || x.f.fmt != y.f.fmt;
    g[1] = x.sx0 != y.sx0 || x.sy0 != y.sy0 || x.sx1 != y.sx1 || x.sy1 != y.sy1;
    g[2] = x.blend != y.blend || x.bmode != y.bmode || x.fixed_a != y.fixed_a || x.fixed_b != y.fixed_b;
    g[3] = x.ztest != y.ztest || x.zwrite != y.zwrite || x.zfun != y.zfun;
    g[4] = x.stest != y.stest || x.sfail != y.sfail || x.szfail != y.szfail || x.szpass != y.szpass || x.stst != y.stst || x.mrgb != y.mrgb || x.ma != y.ma || x.sbits != y.sbits || x.modo != y.modo || x.cleared != y.cleared;
    g[5] = x.atest != y.atest || x.atst != y.atst;
    g[6] = x.tf != y.tf || x.texture != y.texture;
    g[7] = memcmp(&x.t, &y.t, sizeof(Tex)) != 0 || x.gltex != y.gltex || x.glpal != y.glpal || x.gltex_clut != y.gltex_clut || x.gltex_rtfmt != y.gltex_rtfmt || x.gltex_w != y.gltex_w || x.gltex_h != y.gltex_h || x.chain != y.chain || x.wrap != y.wrap || x.mip != y.mip || x.bilinear != y.bilinear || x.clutf != y.clutf;
    bool other = false;
    for (int k = 0; k < 8; k++) { if (g[k]) cp_lg[k]++; if (k < 7) other |= g[k]; }
    if (other) cp_lj[7]++; else if (g[7]) cp_lj[6]++; else cp_lj[5]++;
}

static void glr_flush_queue() {
    GvTime gvt_(4);
    PERF_SUB("vaciado GL");
    if (!glr_init()) { glr_var = 0; gv_vtx.clear(); gv_idx.clear(); gv_x.clear(); gv_agv.clear(); printf("gl: no pude preparar el programa, sigo por software\n"); return; }
    glr_tex_apply();
    static bool first_one = true;
    if (first_one) { first_one = false; printf("gl: primer vaciado por GPU, %d primitivas\n", (int)queue.size()); }
    int wst = -1;
    uint32_t st_prev = 0xFFFFFFFFu;
    int ref_clear = -1;
    bool dump = psp_window_dump_requested();
    if (dump) glr_dumped = 0;
    glUseProgram(glr_prog);
    glBindVertexArray(glr_vao);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_CLAMP);
    toon_params();
    toon_tune();
    glUniform4f(glu_chg, chg_k * std::max(0.0f, toon_env("TTT_CHARGE2_DARK", 1.0f)), 0.0f, 0.0f, 0.0f);
    glr_chg_x = chg_k * std::max(0.0f, toon_env("TTT_CHARGE2_DARK", 1.0f));
    glUniform4f(glu_wetk, wt_enh ? 1.0f : 0.0f, (float)fmod(toon_fno / 60.0, 600.0), rn_wet, 0.0f);
    glUniform1i(glu_toon, toon_mode() > 0 ? toon_mode() : 1);   // with cel off only BT3 fighters reach the shader marked as toon
    glUniform1f(glu_tedge, TOON_EDGE);
    glUniform4f(glu_tk, TOON_SHADE, TOON_TINT, TOON_AURA, TOON_SKIN);
    glUniform4f(glu_tk2, TOON_FACE, TOON_SOFT, TOON_HI, TOON_HL);
    glUniform4f(glu_tk3, TOON_WARM, map_t[0], map_t[1], map_t[2]);
    glUniform4f(glu_tk4, map_s[0], map_s[1], map_s[2], map_hl);
    glUniform1f(glu_tphl, TOON_TPHL);
    glUniform1i(glu_tpx, (int)TOON_TPX);
    glUniform4f(glu_bolt, bolt_rgb[0] * bolt_k * TOON_BOLTAMB, bolt_rgb[1] * bolt_k * TOON_BOLTAMB, bolt_rgb[2] * bolt_k * TOON_BOLTAMB, bolt_k * TOON_BOLTMAP);
    glr_bolt_w = bolt_k * TOON_BOLTMAP;
    auto pt0 = std::chrono::steady_clock::now();
    { GvTime g_(2); toon_scene(); }
    { GvTime g_(3); PERF_SUB("cel normales"); toon_normals(); toon_normals_gv(); }
    {
        static double us = 0; static long nf = 0, tris = 0; static uint32_t lfno = 0; static int logs = 0;
        us += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - pt0).count(); nf++; tris += (long)queue.size();
        if (toon_fno != lfno && toon_fno % 600 == 599 && logs < 10) {
            logs++; lfno = toon_fno;
            printf("cel: luz y normales %.0f us por vaciado, %.1f vaciados por frame, %ld triangulos por vaciado (pixel %d, suavizado %.2f)\n", us / nf, nf / 600.0, tris / nf, (int)TOON_PIXEL, TOON_SMOOTH);
            us = 0; nf = 0; tris = 0;
        }
        if (toon_fno % 600 == 0) { us = 0; nf = 0; tris = 0; }
    }
    uint32_t mvh = 0; bool mon = false;
    {   // ki light on the map: lights of the strongest view, per pixel in the shader
        float lp[16] = {0}, lc[16] = {0}, top = 0;
        for (int j = 0; j < 4; j++) if (map_l[j].e > top) { top = map_l[j].e; mvh = map_l[j].vh; }
        mon = top > 0 && FX_BLOOM > 0 && FX_VFXMAP > 0;
        if (mon) for (int j = 0; j < 4; j++) {
            const MapL& m = map_l[j];
            if (m.e <= 0 || m.vh != mvh) continue;
            lp[j * 4] = m.p[0]; lp[j * 4 + 1] = m.p[1]; lp[j * 4 + 2] = m.p[2]; lp[j * 4 + 3] = FX_VFXR * std::fabs(m.p[2]) + 1e-3f;
            lc[j * 4] = m.c[0]; lc[j * 4 + 1] = m.c[1]; lc[j * 4 + 2] = m.c[2]; lc[j * 4 + 3] = m.e * FX_VFXMAP * FX_VFXK;
        }
        glUniform4fv(glu_fxlp, 4, lp);
        glUniform4fv(glu_fxlc, 4, lc);
        glUniform4f(glu_tpix, TOON_PIXEL > 0 ? 1.0f : 0.0f, TOON_FILL, mon ? 1.0f : 0.0f, FX_VFXGAIN);
    }
    bool won = wat_on(), battle = wat_stale < 30;
    float wlv = WAT_LSIGN * psp_map_wlevel;   // the MAPD stores the level with Y up; the world here has Y down (fighters standing on the island: y 0 to -15)
    int mainw = 480 * scale_int();
    {
        uint32_t c = psp_map_wsplash;
        float f[3];
        if (c) {   // the map's own water colour (splash sprite): hue kept, brightest channel 1, saturation boosted
            float v[3] = {(float)(c & 255), (float)((c >> 8) & 255), (float)((c >> 16) & 255)}, mx = std::max(std::max(v[0], v[1]), std::max(v[2], 1.0f));
            for (int j = 0; j < 3; j++) f[j] = std::min(std::max(1.0f + (v[j] / mx - 1.0f) * WAT_SAT, 0.3f), 1.0f) * WAT_GAIN;
        } else {
            c = psp_map_wcol;
            float bl = std::min(std::max(WAT_BLUE, 0.0f), 2.0f), k[3] = {1.0f + (0.72f - 1.0f) * bl, 1.0f + (0.88f - 1.0f) * bl, 1.0f + (1.12f - 1.0f) * bl};   // pushed to blue
            for (int j = 0; j < 3; j++) f[j] = ((c >> (8 * j)) & 255) / 128.0f * k[j];
        }
        glUniform4f(glu_wc, f[0], f[1], f[2], WAT_SUB);
        glUniform4f(glu_ww, WAT_DARK, WAT_SHINE, std::max(WAT_SOFT, 0.01f), 0.0f);
    }
    // world Y of a view-space point (view 0): GE view inverse to the game's eye space, then the game camera (wrapper on 0x0880BB28)
    auto wat_k = [&](const Job& t, float* kq) -> bool {
        if (t.sv != 0 || psp_wat_cam[4] <= 0) return false;
        const float* r = t.vi; const float* g = psp_wat_cam;
        for (int i = 0; i < 3; i++) kq[i] = r[3 * i] * g[0] + r[3 * i + 1] * g[1] + r[3 * i + 2] * g[2];
        kq[3] = g[3] - (r[9] * kq[0] + r[10] * kq[1] + r[11] * kq[2]);
        return true;
    };
    const bool gw = won && gv_water_on();
    float gxz[2][4]; bool wxz_ok = false;
    if (won && psp_wat_cam[4] > 0) {   // world X and Z rows of the inverse game camera, same form as psp_wat_cam (Y)
        const float* m = psp_cam_m;
        float a00 = m[0], a01 = m[1], a02 = m[2], a10 = m[4], a11 = m[5], a12 = m[6], a20 = m[8], a21 = m[9], a22 = m[10];
        float det = a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);
        if (std::fabs(det) > 1e-12f) {
            float c0[3] = {(a11 * a22 - a12 * a21) / det, -(a10 * a22 - a12 * a20) / det, (a10 * a21 - a11 * a20) / det};
            float c2[3] = {(a01 * a12 - a02 * a11) / det, -(a00 * a12 - a02 * a10) / det, (a00 * a11 - a01 * a10) / det};
            for (int j = 0; j < 3; j++) { gxz[0][j] = c0[j]; gxz[1][j] = c2[j]; }
            gxz[0][3] = -(m[12] * c0[0] + m[13] * c0[1] + m[14] * c0[2]); gxz[1][3] = -(m[12] * c2[0] + m[13] * c2[1] + m[14] * c2[2]);
            wxz_ok = true;
        }
    }
    auto wat_kg = [&](const Job& t, const float* g, float* kq) {
        const float* r = t.vi;
        for (int i = 0; i < 3; i++) kq[i] = r[3 * i] * g[0] + r[3 * i + 1] * g[1] + r[3 * i + 2] * g[2];
        kq[3] = g[3] - (r[9] * kq[0] + r[10] * kq[1] + r[11] * kq[2]);
    };
    if (won) {
        bool seen = false;
        for (int pass = 0; pass < 2 && !seen; pass++)
            for (const Job& t : queue) {   // the eye (projection centre) in world: the fighters' view first
                float kq[4];
                if (seen || (t.cls && !(gw && t.cls == 2)) || !t.d3 || (pass == 0 && !t.toon) || states[t.st_tex].f.width < mainw || !wat_k(t, kq)) continue;
                seen = true;
                float cy = kq[0] * t.eye[0] + kq[1] * t.eye[1] + kq[2] * t.eye[2] + kq[3];
                float dv = WAT_SIGN * (cy - wlv);
                bool u = wat_cam_ok[0] ? (wat_cam[0] ? dv > -0.5f : dv > 0.5f) : dv > 0;   // half a unit of hysteresis at the surface
                static int logs = 0;
                if ((logs < 30 && (u != wat_cam[0] || !wat_cam_ok[0])) || wat_f9 == 2) { logs++; printf("agua: camara %s el agua (ojo y %.1f, superficie %.1f)\n", u ? "bajo" : "sobre", cy, wlv); }
                if (wat_f9 == 2) wat_f9 = 1;
                wat_cam[0] = u; wat_cam_ok[0] = true; wat_camy[0] = cy;
            }
    }
    bool surf_mask = false, mesh_on = false;
    auto main_now = [&]() { return glr_current && glr_current->width >= mainw; };
    if (tpal_dirty) {   // ramps of every TPAL seen, one row per material
        tpal_dirty = false;
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, glr_ramp);
        for (size_t r = 0; r < tpal_rows.size() && r < 255; r++) glTexSubImage2D(GL_TEXTURE_2D, 0, 0, (GLint)r + 1, 16, 1, GL_RGBA, GL_UNSIGNED_BYTE, tpal_rows[r].data());
        glActiveTexture(GL_TEXTURE0);
    }
    size_t last3d = 0; for (size_t i = 0; i < queue.size(); i++) if (queue[i].d3 && (!queue[i].cls || queue[i].cls == 2)) last3d = i + 1;   // the screen effects wait for the last 3D job of this batch
    size_t cdk_at = (size_t)-1;   // charge darkness: the first scene job of a batch without depth-writing scene jobs after this frame's scenery batch (the effects)
    if (battle && chg_k > 0.0f && cdk_done_f != toon_fno) {
        bool hz = false; size_t f3 = (size_t)-1; int n3 = 0, nzw = 0, nbl = 0;
        for (size_t i = 0; i < queue.size(); i++) {
            const Job& q = queue[i]; const TexState& qe = states[q.st_tex];
            if (!q.d3 || (q.cls && q.cls != 2) || qe.f.width < 480 * scale_int()) continue;
            if (f3 == (size_t)-1) f3 = i;
            n3++; nzw += qe.zwrite; nbl += qe.blend;
            if (qe.zwrite && !qe.blend) hz = true;   // opaque scenery with depth: the map batch (effects blend, with or without depth)
        }
        if (hz) cdk_scn_f = toon_fno; else if (f3 != (size_t)-1 && cdk_scn_f == toon_fno) cdk_at = f3;
        static int dl = 0; static uint32_t dlf = 0;
        if (dl < 4 && n3 && !hz && chg_k > 0.5f && toon_fno - dlf > 120) { dl++; dlf = toon_fno; printf("carga: lote de efectos, cuadro %u: %d trabajos de escena (%d con Z, %d con mezcla), primero %u de %u, escenario en el cuadro %u, oscuridad %s; batalla %d, pase hecho %d, dividido %d, principal %d, split2 %d\n", (unsigned)toon_fno, n3, nzw, nbl, (unsigned)f3, (unsigned)queue.size(), (unsigned)cdk_scn_f, cdk_at != (size_t)-1 ? "se dibuja aqui" : "no se dibuja", (int)battle, (int)wat_done, (int)wat_split, (int)main_now(), psp_gpu_split2()); }
    } else { static int dl2 = 0; static uint32_t dlf2 = 0; if (dl2 < 6 && chg_k > 0.5f && toon_fno - dlf2 > 60 && !battle) { dl2++; dlf2 = toon_fno; printf("carga: cuadro %u con oscuridad %.2f pero sin batalla (agua estancada %d)\n", (unsigned)toon_fno, chg_k, wat_stale); } }
    uint64_t gvu0 = perf_clock();
    const bool cpe = cp_on();
    auto wfig = [](const Job& q) { return q.toon || (patch_water_fig.val && q.fig); };   // a fighter for the water: with cel shading off the draws are fig, not toon
    if (!gv_vtx.empty()) {   // GPU vertex path: per-draw flags as the Job loop sets them, then one upload per flush
        bool gw_fs = wat_fseen, gw_ss = wat_surfseen;
        if (gw) gw_fl.assign(queue.size(), 0);
        static std::vector<float> gwd;
        auto gw_mk = [&](const Job& t, const TexState& e) -> int {
            if (wfig(t) || !gw_fs || !e.blend || e.zwrite || e.cleared) return 0;
            bool add = ((e.bmode >> 4) & 15) == 10 && ((e.bmode >> 8) & 15) == 0 && ((e.fixed_b & 255) + ((e.fixed_b >> 8) & 255) + ((e.fixed_b >> 16) & 255)) > 384;
            if (add || t.aura) return 0;
            return wat_mesh_tex && e.t.dir == wat_mesh_tex ? 2 : 1;
        };
        auto gw_tail = [&](const Job& t, const TexState& e, float* wd, int nv, bool q, float wc, int mk) -> uint8_t {
            uint8_t fl = mk ? 8 : 0;
            if (mk == 2) { fl |= 4; if (!wc) for (int k = 0; k < nv; k++) wd[k] = -1e4f; }
            if (!wfig(t)) {
                bool all = q && nv > 0;
                for (int k = 0; k < nv && all; k++) all = std::fabs(wd[k]) < WAT_TOL;
                if (all) { fl |= 1; if (WAT_SURF > 0 && e.blend && e.zwrite && !e.cleared) fl |= 2; gw_ss = true; if (!wc) for (int k = 0; k < nv; k++) wd[k] = -1e4f; }
                if (!gw_ss && !wc) for (int k = 0; k < nv; k++) wd[k] = -1e4f;
            }
            if (wat_mode == 2 && t.sv < 8 && wat_cam[t.sv]) for (int k = 0; k < nv; k++) wd[k] = -1e4f;
            return fl;
        };
        for (size_t qi = 0; qi < queue.size(); qi++) {
            const Job& t = queue[qi];
            if (gw && !t.cls && t.d3 && states[t.st_tex].f.width >= mainw) {   // loose triangles: only the seen flags, in draw order
                const TexState& e = states[t.st_tex];
                if (wfig(t)) gw_fs = true;
                else {
                    float kq[4], wd[3] = {-1e4f, -1e4f, -1e4f}; bool q = wat_k(t, kq);
                    if (q) for (int k = 0; k < 3; k++) wd[k] = WAT_SIGN * (kq[0] * t.v[k].vp[0] + kq[1] * t.v[k].vp[1] + kq[2] * t.v[k].vp[2] + kq[3] - wlv);
                    float wc = wat_mode != 2 && t.sv < 8 && wat_cam[t.sv] ? 1.0f : 0.0f;
                    gw_tail(t, e, wd, 3, q, wc, gw_mk(t, e));
                }
            }
            if (t.cls != 2) continue;
            if (gw && t.d3 && t.x1 > 0 && states[t.st_tex].f.width >= mainw) {
                const TexState& e = states[t.st_tex];
                float wc = wat_mode != 2 && t.sv < 8 && wat_cam[t.sv] ? 1.0f : 0.0f;
                float kq[4]; bool q = wat_k(t, kq);
                gwd.resize((size_t)t.x1);
                float* wd = gwd.data();
                for (int k = 0; k < t.x1; k++) { const VtxGL& g = gv_vtx[t.x0 + k]; wd[k] = q ? WAT_SIGN * (kq[0] * g.px + kq[1] * g.py + kq[2] * g.pz + kq[3] - wlv) : -1e4f; }
                float wet = 0.0f;
                if (wfig(t)) {
                    int who = t.who < 9 ? t.who : 0;
                    wet = wat_wet[who]; gw_fs = true;
                    if (wt_enh) wet = std::max(wet, rn_wet);
                    WRing* r = nullptr; float kx[4], kz[4];
                    if (q && who && wxz_ok) {
                        r = &wr_f[who];
                        if (r->f != toon_fno) { *r = {}; r->f = toon_fno; r->y0 = 1e30f; r->y1 = -1e30f; r->x0 = r->z0 = 1e30; r->x1 = r->z1 = -1e30; }
                        wat_kg(t, gxz[0], kx); wat_kg(t, gxz[1], kz);
                    }
                    for (int k = 0; k < t.x1; k++) {
                        const VtxGL& g = gv_vtx[t.x0 + k];
                        float wy = wd[k] * WAT_SIGN + wlv;
                        if (r) {
                            r->y0 = std::min(r->y0, wy); r->y1 = std::max(r->y1, wy);
                            if (std::fabs(wd[k]) < 2.5f) {
                                double x = kx[0] * g.px + kx[1] * g.py + kx[2] * g.pz + kx[3], z = kz[0] * g.px + kz[1] * g.py + kz[2] * g.pz + kz[3];
                                r->sx += x; r->sz += z; r->n += 1; r->x0 = std::min(r->x0, x); r->x1 = std::max(r->x1, x); r->z0 = std::min(r->z0, z); r->z1 = std::max(r->z1, z);
                            }
                        }
                        wat_st.fighter++; if (wd[k] > 0) wat_st.fsub++; if (who) { wat_sub[who][1] += 1; if (wd[k] > 0) wat_sub[who][0] += 1; }
                        if (q) { wat_st.fy0 = std::min(wat_st.fy0, wy); wat_st.fy1 = std::max(wat_st.fy1, wy); }
                    }
                }
                uint8_t fl = gw_tail(t, e, wd, t.x1, q, wc, gw_mk(t, e));
                gw_fl[qi] = fl | 16;
                if (fl & 1) { uint32_t nt = (uint32_t)t.y1 / 3; wat_st.surf += nt; if (e.blend) wat_st.surf_bl += nt; if (e.zwrite) wat_st.surf_zw += nt; if (e.ztest) wat_st.surf_zt += nt; if (gw_fs) wat_st.after += nt; else wat_st.before += nt; }
                for (int k = 0; k < t.x1; k++) { VtxGL& g = gv_vtx[t.x0 + k]; g.wd = wd[k]; g.wc = wc; if (t.toon) g.wet = wet; }
                if (t.toon) { for (int k = 0; k < t.x1; k++) gv_vtx[t.x0 + k].pf = 0.0f; continue; }
            } else if (t.toon) { float wet = wt_enh && rn_wet > 0.0f && t.d3 ? rn_wet : 0.0f; for (int k = 0; k < t.x1; k++) { VtxGL& g = gv_vtx[t.x0 + k]; g.wet = wet; g.pf = 0.0f; } continue; }
            const TexState& e = states[t.st_tex];
            float scene3d = (t.d3 && e.f.width >= 480 * scale_int()) ? 1.0f : 0.0f;
            bool ad = e.blend && ((e.bmode >> 4) & 15) == 10;
            float pf = scene3d > 0 && !ad ? (mon && t.vh == mvh ? 1.0f : 0.25f) : 0.0f;
            for (int k = 0; k < t.x1; k++) { VtxGL& g = gv_vtx[t.x0 + k]; g.tf = scene3d; g.pf = pf; }
        }
        if (cpe) cp_gj[11] += perf_clock() - gvu0;
        glBindBuffer(GL_ARRAY_BUFFER, gv_vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(gv_vtx.size() * sizeof(VtxGL)), gv_vtx.data(), GL_STREAM_DRAW);
        glBindVertexArray(gv_vao);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(gv_idx.size() * 4), gv_idx.data(), GL_STREAM_DRAW);
        glBindVertexArray(glr_vao);
    }
    uint64_t gvu1 = perf_clock(); gv_t[5] += gvu1 - gvu0;
    for (size_t i = 0; i < queue.size(); i++) {
        const Job& t = queue[i];
        CpJob cpj_(t.cls);
        if (cpe && !t.cls) cp_tj[t.d3 ? 4 : 5]++;
        LjT ljt_(cpe && t.cls != 2 && !t.d3);
        uint64_t ljp = ljt_.t0 ? cp_tj[0] + cp_tj[1] + cp_tj[2] + cp_tj[3] + cp_tj[7] : 0;
        uint64_t tp0 = cpe ? perf_clock() : 0;
        auto tpk = [&](int k) { if (cpe) { uint64_t x = perf_clock(); cp_tj[k] += x - tp0; tp0 = x; } };
        const TexState& e0 = states[t.st_tex];
        bool rt = e0.texture && ((e0.t.dir >= 0x04000000u && e0.t.dir < 0x04200000u) || is_twin(e0.t.dir) || is_vram_high(e0.t.dir));
        for (int k = 0; k < glr_dest_n && !rt && e0.texture; k++) if ((glr_dest[k].dir & 0x0FFFFFFFu) == (e0.t.dir & 0x0FFFFFFFu)) rt = true;   // reads a rendered buffer: outline
        gfxw_now = won && wat_mode == 2 && psp_map_gfxw && wat_cam[0];
        float wave = won && wat_mode != 2 && wat_cam[0] ? WAT_WAVE : 0.0f;
        bool fx_any = gfxw_now || wave > 0 || fx.rush > 0 || fx.dash > 0 || fx.imode || fx.ab > 0 || fx.lines > 0 || (FX_BLOOM > 0 && FX_BLOOMK > 0) || g3_any || atm_on();
        bool fx_here = fx_any && battle && !wat_done && !wat_split && !t.d3 && wat_3d && e0.texture && !rt && e0.f.width >= mainw && main_now() && !(patch_fx_after_back.val && t.back);   // first 2D draw after the scene and the outline (those read the screen): HUD
        if (fx_here && i < last3d) {   // a 2D draw in the middle of the scene (sprite effects, LOD point clouds): wait for the last 3D job
            fx_here = false; static int pe = 0, pf = -1;
            if (pe < 6 && (int)toon_fno != pf) { pe++; pf = (int)toon_fno; size_t n3 = 0; for (size_t k = i; k < last3d; k++) n3 += queue[k].d3; printf("fx: dibujo 2D 0x%08X %dx%d en medio de la escena, quedan %u trabajos 3D: los efectos esperan\n", e0.t.dir, e0.t.w, e0.t.h, (unsigned)n3); }
        }
        DestGL* cdk_d = i == cdk_at && !wat_done && !wat_split && !psp_gpu_split2() ? glr_find(e0.f.dir, e0.f.width, e0.f.fmt) : nullptr;   // the job's own target: the current one may still be the previous batch's
        if (cdk_d && cdk_d->width >= mainw) {
            tpk(7);
            glr_draw_batch();
            PERF_SUB("pase oscuridad");
            if (cdk_pass(cdk_d, mainw, 272 * scale_int())) { cdk_done_f = toon_fno; static int nl = 0; if (nl < 3) { nl++; printf("carga: oscuridad dibujada antes de los efectos (trabajo %u de %u)\n", (unsigned)i, (unsigned)queue.size()); } }
            st_prev = 0xFFFFFFFFu;
            tpk(0);
        }
        if (fx_here) {
            static int pl = 0;
            if (pl < 4) { pl++; printf("fx: efectos de pantalla antes del dibujo 2D con textura 0x%08X %dx%d\n", e0.t.dir, e0.t.w, e0.t.h); }
            tpk(7);
            glr_draw_batch();
            { PERF_SUB("pase efectos/atm"); fx_run(glr_current, mainw, wave); }
            st_prev = 0xFFFFFFFFu;
            tpk(1);
        }
        if (sm_on && sm_scn_f != toon_fno && !t.d3 && e0.f.width >= mainw) {
            static int sdl = 0; static uint32_t sdf = 0;
            if (sdl < 6 && toon_fno - sdf > 90) { sdl++; sdf = toon_fno; printf("suave: dibujo 2D %u de %u en el cuadro %u: batalla %d, escena 3D %d, tras el ultimo 3D %d, lee un destino %d, dividido %d, textura %d, efectos de pantalla %d hechos %d\n", (unsigned)i, (unsigned)queue.size(), (unsigned)toon_fno, (int)battle, (int)wat_3d, (int)(i >= last3d), (int)rt, (int)wat_split, (int)e0.texture, (int)fx_any, (int)wat_done); }
        }
        if (sm_on && sm_scn_f != toon_fno && battle && !t.d3 && !(patch_fx_after_back.val && t.back) && wat_3d && i >= last3d && !rt && !wat_split && e0.texture && (wat_done || !fx_any) && e0.f.width >= mainw) {
            DestGL* sd = glr_find(e0.f.dir, e0.f.width, e0.f.fmt);
            if (sd->width >= mainw) { tpk(7); glr_draw_batch(); sm_scene_copy(sd); st_prev = 0xFFFFFFFFu; tpk(2); }
        }
        if (t.st_tex != st_prev && e0.texture && e0.gltex_clut && e0.t.fmt == 6 && ((e0.clutf >> 2) & 0x1F) == 5 && ((e0.clutf >> 8) & 0xFF) == 0xFF && e0.f.width >= mainw && ol_fit && ol_mode() > 0 && outline_width() > 0.0f) {
            DestGL* dd = glr_find(e0.f.dir, e0.f.width, e0.f.fmt), *b = nullptr;   // second pass of the game outline: its texture is the edge mask
            for (int k = 0; k < glr_dest_n && !b; k++) if (glr_dest[k].tex == e0.gltex) b = &glr_dest[k];
            if (!b) { static int nl = 0; if (nl < 3) { nl++; printf("borde: la mascara del delineado (textura 0x%08X) no es un destino de GL\n", e0.t.dir); } }
            else if (b != dd && (ol_dirty || ol_last_b != b)) { tpk(7); ol_last_b = b; ol_dirty = false; glr_draw_batch(); ol_pass(dd, b); st_prev = 0xFFFFFFFFu; tpk(3); }
        }
        if (ljt_.t0) { ljt_.ex = cp_tj[0] + cp_tj[1] + cp_tj[2] + cp_tj[3] + cp_tj[7] - ljp; cp_lj[2] += perf_clock() - ljt_.t0 - ljt_.ex; }
        if (t.st_tex != st_prev) {
            if (ljt_.t0) lj_cmp(st_prev, t.st_tex);
            uint64_t cs0 = cpe ? perf_clock() : 0;
            glr_draw_batch();
            uint64_t cs1 = cpe ? perf_clock() : 0;
            glr_state(states[t.st_tex]);
            uint64_t cs2 = cpe ? perf_clock() : 0;
            olb_apply(states[t.st_tex], mainw, battle);
            if (glr_current == ol_last_b) ol_dirty = true;
            if (dump) glr_dump(states[t.st_tex]);
            st_prev = t.st_tex;
            ref_clear = -1;
            surf_mask = false; mesh_on = false;
            if (cpe) { uint64_t cs3 = perf_clock(); cp_gj[9] += cs3 - cs0; cp_gj[10]++; if (ljt_.t0) { cp_lj[3]++; cp_lj[8] += cs1 - cs0; cp_lj[9] += cs2 - cs1; cp_lj[10] += cs3 - cs2; } }
        }
        const TexState& e = states[t.st_tex];
        float scene3d = (t.d3 && e.f.width >= 480 * scale_int()) ? 1.0f : 0.0f;   // map/scene draw on the main target (lightning flash)
        bool kmap = mon && scene3d > 0 && !t.toon && !t.cls && t.vh == mvh && !(e.blend && ((e.bmode >> 4) & 15) == 10);
        bool kdk = scene3d > 0 && !t.toon && !t.cls && !(e.blend && ((e.bmode >> 4) & 15) == 10);   // scenery for the charge darkening (additive effects and auras stay bright)
        gq_next = t.toon ? GQ_FIG : (!t.d3 || t.cls == 1) ? GQ_2D : e.zwrite ? GQ_MAP : GQ_FX3;
        auto olbj = [&](const Job& t) {   // outline box: every draw of an outlined layer, merged GPU jobs included
        if (olb_on() && t.d3 && t.cls != 1 && !e.cleared && e.f.width >= mainw && (t.fig || (e.stest && (ol_ids[((e.stst >> 8) & 0x7F) >> 4] >> (((e.stst >> 8) & 0x7F) & 15)) & 1))) {
            float bw = (float)e.f.width, bh = (float)(512 * scale_int());
            uint64_t co0 = cpe ? perf_clock() : 0;
            if (t.cls == 2 && t.ag >= 0) {
                const GvAg& a = gv_agv[t.ag];
                if (t.x1 > 0) { if (a.big) { olb_add(-1e8f, -1e8f); olb_add(1e8f, 1e8f); } if (a.b[0] <= a.b[2]) { olb_add(a.b[0], a.b[1]); olb_add(a.b[2], a.b[3]); } }
            }
            else if (t.cls == 2) { for (int k = 0; k < t.x1; k++) { const VtxGL& g = gv_vtx[t.x0 + k]; if (g.w > 1e-4f) olb_add((g.x / g.w + 1.0f) * 0.5f * bw, (g.y / g.w + 1.0f) * 0.5f * bh); else { olb_add(-1e8f, -1e8f); olb_add(1e8f, 1e8f); } } }
            else for (int k = 0; k < 3; k++) olb_add(t.v[k].x, t.v[k].y);
            if (cpe && t.cls == 2) cp_gj[0] += perf_clock() - co0;
        }
        };
        olbj(t);
        float wd[3] = {-1e4f, -1e4f, -1e4f}, wc = 0.0f, wet = 0.0f;
        bool surf = false, issurf = false, mesh = false;
        if (t.toon && wt_enh && rn_wet > 0.0f && !t.cls && t.d3) wet = rn_wet;
        if (battle && (!t.cls || t.cls == 2) && t.d3 && e.f.width >= mainw) {
            wat_3d = true;

            if (t.sv) wat_split = true;
            if (t.toon && t.who < 9 && t.cls != 2) { int sc = scale_int(); for (int k = 0; k < 3; k++) { fx_fx[t.who] += t.v[k].x / sc; fx_fy[t.who] += t.v[k].y / sc; fx_fn[t.who] += 1; } }
        }
        if (won && !t.cls && t.d3 && e.f.width >= mainw) {
            wc = wat_mode != 2 && t.sv < 8 && wat_cam[t.sv] ? 1.0f : 0.0f;
            float kq[4]; bool q = wat_k(t, kq);
            float wy[3] = {0, 0, 0};
            if (q)
                for (int k = 0; k < 3; k++) { wy[k] = kq[0] * t.v[k].vp[0] + kq[1] * t.v[k].vp[1] + kq[2] * t.v[k].vp[2] + kq[3]; wd[k] = WAT_SIGN * (wy[k] - wlv); }
            if (!wfig(t) && wat_fseen && e.blend && !e.zwrite && !e.cleared) {
                bool add = ((e.bmode >> 4) & 15) == 10 && ((e.bmode >> 8) & 15) == 0 && ((e.fixed_b & 255) + ((e.fixed_b >> 8) & 255) + ((e.fixed_b >> 16) & 255)) > 384;
                if (!add && !t.aura) {
                    int k = 0; while (k < wat_mk && wat_mt[k] != e.t.dir) k++;
                    if (k == wat_mk && wat_mk < 8) { wat_mt[k] = e.t.dir; wat_mn[k] = 0; wat_mk++; }
                    if (k < wat_mk) wat_mn[k]++;
                    bool m = wat_mesh_tex && e.t.dir == wat_mesh_tex;
                    mesh = m && WAT_MESH > 0 && wat_bg >= 0;
                    if (m && !wc) wd[0] = wd[1] = wd[2] = -1e4f;
                }
            }
            if (wfig(t)) {
                int who = t.who < 9 ? t.who : 0;
                wet = wat_wet[who]; wat_fseen = true;
                if (wt_enh) wet = std::max(wet, rn_wet);
                if (q && who && wxz_ok) {
                    WRing& r = wr_f[who];
                    if (r.f != toon_fno) { r = {}; r.f = toon_fno; r.y0 = 1e30f; r.y1 = -1e30f; r.x0 = r.z0 = 1e30; r.x1 = r.z1 = -1e30; }
                    float kx[4], kz[4]; wat_kg(t, gxz[0], kx); wat_kg(t, gxz[1], kz);
                    for (int k = 0; k < 3; k++) {
                        r.y0 = std::min(r.y0, wy[k]); r.y1 = std::max(r.y1, wy[k]);
                        if (std::fabs(wd[k]) < 2.5f) {
                            const float* v = t.v[k].vp; double x = kx[0] * v[0] + kx[1] * v[1] + kx[2] * v[2] + kx[3], z = kz[0] * v[0] + kz[1] * v[1] + kz[2] * v[2] + kz[3];
                            r.sx += x; r.sz += z; r.n += 1; r.x0 = std::min(r.x0, x); r.x1 = std::max(r.x1, x); r.z0 = std::min(r.z0, z); r.z1 = std::max(r.z1, z);
                        }
                    }
                }
                for (int k = 0; k < 3; k++) {
                    wat_st.fighter++; if (wd[k] > 0) wat_st.fsub++; if (who) { wat_sub[who][1] += 1; if (wd[k] > 0) wat_sub[who][0] += 1; }
                    if (q) { wat_st.fy0 = std::min(wat_st.fy0, wy[k]); wat_st.fy1 = std::max(wat_st.fy1, wy[k]); }
                }
            } else if (q && std::fabs(wd[0]) < WAT_TOL && std::fabs(wd[1]) < WAT_TOL && std::fabs(wd[2]) < WAT_TOL) {
                wat_st.surf++; if (e.blend) wat_st.surf_bl++; if (e.zwrite) wat_st.surf_zw++; if (e.ztest) wat_st.surf_zt++;
                if (wat_fseen) wat_st.after++; else wat_st.before++;
                surf = WAT_SURF > 0 && e.blend && e.zwrite && !e.cleared;
                wat_surfseen = true; issurf = true;
                if (!wc) wd[0] = wd[1] = wd[2] = -1e4f;
            }
            if (!wfig(t) && !wat_surfseen && !wc) wd[0] = wd[1] = wd[2] = -1e4f;   // map drawn before the surface: its blend already shows the water
            if (wat_mode == 2 && t.sv < 8 && wat_cam[t.sv]) wd[0] = wd[1] = wd[2] = -1e4f;   // BT3: under water only GfxWater tints the screen
        }
        if (gw && t.cls == 2 && i < gw_fl.size() && (gw_fl[i] & 16)) {   // GPU path prim with water: depth already per vertex, the draw state here
            uint8_t fl = gw_fl[i];
            if (wfig(t)) wat_fseen = true;
            if (fl & 1) { wat_surfseen = true; issurf = true; surf = (fl & 2) != 0; }
            mesh = (fl & 4) && WAT_MESH > 0 && wat_bg >= 0;
            if (fl & 8) {
                int k = 0; while (k < wat_mk && wat_mt[k] != e.t.dir) k++;
                if (k == wat_mk && wat_mk < 8) { wat_mt[k] = e.t.dir; wat_mn[k] = 0; wat_mk++; }
                if (k < wat_mk) wat_mn[k] += std::max(1, t.y1 / 3);
            }
        }
        if (mesh != mesh_on) {   // the mesh only where the stencil keeps the cleared value: never over a fighter (it writes its own)
            glr_draw_batch();
            if (mesh) { glEnable(GL_STENCIL_TEST); glStencilFunc(GL_EQUAL, wat_bg, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); glStencilMask(0); }
            else glr_state(e);
            mesh_on = mesh;
        }
        if (surf != surf_mask) { glr_draw_batch(); glDepthMask(surf ? GL_FALSE : (GLboolean)e.zwrite); surf_mask = surf; }
        if (WAT_HIDE > 0 && wat_cam[0] && issurf) continue;   // diagnostic: no surface while the camera is under
        if (e.cleared) {
            int r = (int)((t.col_flat >> 24) & 0xFF);
            if (r != ref_clear) { glr_draw_batch(); glStencilFunc(GL_ALWAYS, r, 0xFF); ref_clear = r; }
            if (e.f.width >= mainw) wat_bg = r;
        }
        if (t.cls == 2) {
            auto gst = [&](const Job& q) {
                if (q.d3 && e.stest) {
                    int id = (e.stst >> 8) & 0x7F;
                    if (q.fig) {
                        ol_ids[id >> 4] |= (uint16_t)(1u << (id & 15));
                        if (q.who >= 1 && q.who <= 8) ol_who[id] = q.who;
                        if (q.glow) { g3_lut[id] = std::max(g3_lut[id], q.glow); g3_any = true; }
                        if (q.ag >= 0) { if (q.x1 > 0) { ol_zr[id][0] = std::min(ol_zr[id][0], gv_agv[q.ag].z0); ol_zr[id][1] = std::max(ol_zr[id][1], gv_agv[q.ag].z1); } }
                        else for (int v = q.x0; v < q.x0 + q.x1; v++) { float z = std::fabs(gv_vtx[v].pz); ol_zr[id][0] = std::min(ol_zr[id][0], z); ol_zr[id][1] = std::max(ol_zr[id][1], z); }
                    } else ol_bg[id >> 4] |= (uint16_t)(1u << (id & 15));
                }
                if (q.fig) { uint64_t nt = (uint64_t)q.y1 / 3; g3_n[0] += nt; if (q.glow) g3_n[1] += nt; if (q.glow && e.stest) g3_n[2] += nt; }
                bool fxs = battle && q.toon && q.who < 9 && q.d3 && e.f.width >= mainw, bb = q.fig && q.d3 && q.who >= 1 && q.who <= 8;
                if (!fxs && !bb) return;
                float sc = (float)scale_int(), sw = (float)e.f.width, sh = (float)(512 * scale_int());
                if (q.ag >= 0) {
                    const GvAg& a = gv_agv[q.ag];
                    if (q.x1 <= 0 || a.s[0] <= 0) return;
                    if (fxs) { fx_fx[q.who] += a.s[1] / sc; fx_fy[q.who] += a.s[2] / sc; fx_fn[q.who] += a.s[0]; }
                    if (bb) { double* b = fg_bb[q.who - 1]; fg_bbok[q.who - 1] = true; for (int j = 0; j < 5; j++) b[j] += a.s[j]; }
                    return;
                }
                static std::vector<float> sxy; static std::vector<uint8_t> sok;   // screen position once per vertex, summed per index as before
                sxy.resize((size_t)q.x1 * 2); sok.resize((size_t)q.x1);
                for (int v = 0; v < q.x1; v++) {
                    const VtxGL& g = gv_vtx[q.x0 + v];
                    sok[v] = g.w > 1e-4f;
                    if (sok[v]) { sxy[2 * v] = (g.x / g.w + 1.0f) * 0.5f * sw; sxy[2 * v + 1] = (g.y / g.w + 1.0f) * 0.5f * sh; }
                }
                for (int k = 0; k < q.y1; k++) {
                    int vi = (int)gv_idx[q.y0 + k] - q.x0;
                    float x, y;
                    if (vi >= 0 && vi < q.x1) { if (!sok[vi]) continue; x = sxy[2 * vi]; y = sxy[2 * vi + 1]; }
                    else { const VtxGL& g = gv_vtx[gv_idx[q.y0 + k]]; if (!(g.w > 1e-4f)) continue; x = (g.x / g.w + 1.0f) * 0.5f * sw; y = (g.y / g.w + 1.0f) * 0.5f * sh; }
                    if (fxs) { fx_fx[q.who] += x / sc; fx_fy[q.who] += y / sc; fx_fn[q.who] += 1; }
                    if (bb) { double* b = fg_bb[q.who - 1]; fg_bbok[q.who - 1] = true; b[0] += 1; b[1] += x; b[2] += y; b[3] += (double)x * x; b[4] += (double)y * y; }
                }
            };
            uint64_t cg[7] = {0};
            if (cpe) cg[0] = perf_clock();
            gst(t);
            if (cpe) cg[1] = perf_clock();
            glr_draw_batch();
            if (cpe) cg[2] = perf_clock();
            size_t ib = (size_t)t.y0, ni = (size_t)t.y1;
            while (i + 1 < queue.size() && i + 1 != cdk_at) {
                const Job& q = queue[i + 1];
                if (q.cls != 2 || q.st_tex != t.st_tex || q.sv != t.sv || (size_t)q.y0 != ib + ni) break;
                if (olb_merge()) olbj(q);
                gst(q);
                ni += (size_t)q.y1; i++;
                if (cpe) cp_gj[8]++;
            }
            if (cpe) cg[3] = perf_clock();
            glr_pend_copy(gv_vtx.data(), gv_idx.data() + ib, ni);
            if (cpe) cg[4] = perf_clock();
            gq_cat = gq_next;
            GqZone gqz_(gq_cat);
            bool lite = glr_st_lite && glr_lite_on() && glr_vtx_plain(gv_vtx.data() + t.x0, (size_t)(queue[i].x0 + queue[i].x1 - t.x0));
            if (cpe) cg[5] = perf_clock();
            glr_draw_n++; if (lite) { glr_lite_n++; glUseProgram(glr_lite); }
            gq_area(gq_cat, gv_vtx.data(), gv_idx.data() + ib, ni);
            if (cpe) cg[6] = perf_clock();
            bool wmask = wat_enh && psp_map_wimm && t.wsf && !t.sv && t.d3 && e.f.width >= mainw;
            if (wmask) { gw_mask.push_back({ib, ni, (int)t.st_tex}); wsf_frame += (int)(ni / 3); }
            if (!(wmask && toon_fno - wat_enh_ok <= 2u)) {   // the shader draws the water: no BT3 normal-map surface
                glBindVertexArray(gv_vao);
                glDrawElements(GL_TRIANGLES, (GLsizei)ni, GL_UNSIGNED_INT, (const void*)(ib * 4));
                if (kk_rec && glr_current == kk_d) { glBindFramebuffer(GL_FRAMEBUFFER, kk_fbo); glDrawElements(GL_TRIANGLES, (GLsizei)ni, GL_UNSIGNED_INT, (const void*)(ib * 4)); glBindFramebuffer(GL_FRAMEBUFFER, dfbo(glr_current)); }
                glBindVertexArray(glr_vao);
            }
            if (lite) glUseProgram(glr_prog);
            if (cpe) {
                cp_gj[1] += (cg[1] - cg[0]) + (cg[3] - cg[2]); cp_gj[6] += cg[2] - cg[1]; cp_gj[2] += cg[4] - cg[3];
                cp_gj[3] += cg[5] - cg[4]; cp_gj[4] += cg[6] - cg[5]; cp_gj[5] += perf_clock() - cg[6]; cp_gj[7]++;
            }
            if (glr_current && glr_current->mfbo) glr_current->dirty = true;
            gv_draws++;
            continue;
        }
        float sw = (float)e.f.width, sh = (float)(512 * scale_int());
        float tw = e.texture && e.gltex_w ? (float)e.gltex_w : 1.0f;
        float th = e.texture && e.gltex_h ? (float)e.gltex_h : 1.0f;
        if (!t.d3 && t.cls != 2 && !e.cleared && e.blend && (e.bmode & 0xFFF) == 0x032 && !e.zwrite && !(e.stest && e.szpass == 2) && glr_a0_on() && !(e.texture && (e.tf & 7) == 3 && ((e.tf >> 8) & 1))) {
            bool a0 = t.cls == 1 || !e.gouraud ? (t.col_flat >> 24) == 0 : (t.v[0].color >> 24) == 0 && (t.v[1].color >> 24) == 0 && (t.v[2].color >> 24) == 0;
            if (a0) {
                if (t.cls == 1) glr_a0_px += std::fabs((double)(t.v[1].x - t.v[0].x) * (t.v[1].y - t.v[0].y));
                else glr_a0_px += std::fabs((double)(t.v[1].x - t.v[0].x) * (t.v[2].y - t.v[0].y) - (double)(t.v[2].x - t.v[0].x) * (t.v[1].y - t.v[0].y)) * 0.5;
                continue;
            }
        }
        if (t.cls == 1 && e.cleared && glr_clr_on()) {
            int sx0 = e.sx0 < 0 ? 0 : e.sx0, sy0 = e.sy0 < 0 ? 0 : e.sy0;
            float x0 = std::min(t.v[0].x, t.v[1].x), x1 = std::max(t.v[0].x, t.v[1].x), y0 = std::min(t.v[0].y, t.v[1].y), y1 = std::max(t.v[0].y, t.v[1].y);
            if (x0 <= (float)sx0 && y0 <= (float)sy0 && x1 >= (float)(e.sx1 + 1) && y1 >= (float)(e.sy1 + 1)) {
                glr_draw_batch();
                uint32_t c = t.col_flat;
                glClearColor((c & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, ((c >> 16) & 255) / 255.0f, (c >> 24) / 255.0f);
                glClearDepth(std::min(1.0, std::max(0.0, t.v[2].z / 65535.0)));
                glClearStencil(0);
                glClear(GL_COLOR_BUFFER_BIT | (((e.modo >> 10) & 1) ? GL_DEPTH_BUFFER_BIT : 0) | (((e.modo >> 9) & 1) ? GL_STENCIL_BUFFER_BIT : 0));
                if (glr_current && glr_current->mfbo) glr_current->dirty = true;
                glr_clr_n++;
                continue;
            }
        }
        if (t.cls == 1) {
            gq_cat = gq_next;
            float x0 = t.v[0].x, y0 = t.v[0].y, x1 = t.v[1].x, y1 = t.v[1].y;
            float u0 = t.v[0].u, v0 = t.v[0].v, u1 = t.v[1].u, v1 = t.v[1].v;
            float z = t.v[2].z;
            uint32_t c = t.col_flat;
            glr_push(glr_vtx, x0, y0, z, 1.0f, u0, v0, c, sw, sh, tw, th);
            glr_push(glr_vtx, x1, y0, z, 1.0f, u1, v0, c, sw, sh, tw, th);
            glr_push(glr_vtx, x1, y1, z, 1.0f, u1, v1, c, sw, sh, tw, th);
            glr_push(glr_vtx, x0, y0, z, 1.0f, u0, v0, c, sw, sh, tw, th);
            glr_push(glr_vtx, x1, y1, z, 1.0f, u1, v1, c, sw, sh, tw, th);
            glr_push(glr_vtx, x0, y1, z, 1.0f, u0, v1, c, sw, sh, tw, th);
        } else {
            if (e.texture && glr_traceuv() && (e.t.dir & 0x0FFFFFFFu) == (glr_traceuv() & 0x0FFFFFFFu) && glr_trace_n < 12) {
                glr_trace_n++;
                printf("traza %d: tex 0x%08X %dx%d vtype 0x%08X esc %.4f/%.4f off %.4f/%.4f wrap %d/%d\n",
                       glr_trace_n, e.t.dir, e.t.w, e.t.h, R[0x12],
                       f24(R[0x48]), f24(R[0x49]), f24(R[0x4A]), f24(R[0x4B]),
                       (int)(e.wrap & 1), (int)((e.wrap >> 8) & 1));
                for (int k = 0; k < 3; k++) {
                    const VP& p = t.v[k];
                    float iw = p.iw != 0.0f ? p.iw : 1.0f;
                    printf("   v%d pantalla %.1f,%.1f z %.0f w %.3f uv %.2f,%.2f\n",
                           k, p.x, p.y, p.z, 1.0f / iw, p.u / iw, p.v / iw);
                }
            }
            if (glr_uv() && e.texture && glr_new(0x55000000u ^ e.t.dir)) {
                float u0 = 1e30f, u1 = -1e30f, v0 = 1e30f, v1 = -1e30f;
                float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
                for (int k = 0; k < 3; k++) {
                    const VP& p = t.v[k];
                    float uu = p.iw != 0.0f ? p.u / p.iw : p.u, vv = p.iw != 0.0f ? p.v / p.iw : p.v;
                    if (uu < u0) u0 = uu; if (uu > u1) u1 = uu;
                    if (vv < v0) v0 = vv; if (vv > v1) v1 = vv;
                    if (p.x < x0) x0 = p.x; if (p.x > x1) x1 = p.x;
                    if (p.y < y0) y0 = p.y; if (p.y > y1) y1 = p.y;
                }
                float dx = x1 - x0 > 1.0f ? x1 - x0 : 1.0f, dy = y1 - y0 > 1.0f ? y1 - y0 : 1.0f;
                printf("uv tex 0x%08X %dx%d fmt %d swz %d stride %d wrap %d/%d mip %d bil %d tmap %d | u %.1f..%.1f v %.1f..%.1f | pantalla %.0fx%.0f | texeles por pixel %.1f/%.1f\n",
                       e.t.dir, e.t.w, e.t.h, e.t.fmt, e.t.swz, e.t.stride,
                       (int)(e.wrap & 1), (int)((e.wrap >> 8) & 1), (int)e.mip, (int)e.bilinear, (int)(R[0xC0] & 3),
                       u0, u1, v0, v1, dx, dy, (u1 - u0) / dx, (v1 - v0) / dy);
            }
            if (t.fig) { g3_n[0]++; if (t.glow) g3_n[1]++; if (t.glow && e.stest) g3_n[2]++; }
            if (t.fig && t.d3 && t.who >= 1 && t.who <= 8) { double* b = fg_bb[t.who - 1]; fg_bbok[t.who - 1] = true; for (int k = 0; k < 3; k++) { double x = t.v[k].x, y = t.v[k].y; b[0] += 1; b[1] += x; b[2] += y; b[3] += x * x; b[4] += y * y; } }
            if (t.d3 && e.stest && !t.cls) {
                int id = (e.stst >> 8) & 0x7F;
                if (t.fig) {
                    ol_ids[id >> 4] |= (uint16_t)(1u << (id & 15));
                    if (t.who >= 1 && t.who <= 8) ol_who[id] = t.who;
                    if (t.glow) { g3_lut[id] = std::max(g3_lut[id], t.glow); g3_any = true; }
                    for (int k = 0; k < 3; k++) { float z = std::fabs(t.v[k].vp[2]); ol_zr[id][0] = std::min(ol_zr[id][0], z); ol_zr[id][1] = std::max(ol_zr[id][1], z); }
                } else ol_bg[id >> 4] |= (uint16_t)(1u << (id & 15));
            }
            gq_cat = gq_next;
            for (int k = 0; k < 3; k++) {
                const VP& p = t.v[k];
                uint32_t c = e.gouraud ? p.color : t.col_flat;
                if (t.toon) glr_push(glr_vtx, p.x, p.y, p.z, p.iw, p.u, p.v, c, sw, sh, tw, th, toon_v[i * 6 + k * 2], toon_v[i * 6 + k * 2 + 1], t.tint, toon_e[i * 3 + k], t.tp, wd[k], wc, wet, toon_n[i * 3 + k], toon_l[i], p.vp, 0.0f);
                else glr_push(glr_vtx, p.x, p.y, p.z, p.iw, p.u, p.v, c, sw, sh, tw, th, -9.0f, scene3d, 0, 0u, 0, wd[k], wc, 0.0f, 0u, 0u, kmap ? p.vp : nullptr, (kmap ? 1.0f : kdk ? 0.25f : 0.0f));
            }
            if (sm_on && t.d3 && !t.sv && !e.blend && e.f.width >= mainw && !issurf) sm_add(t, sw, sh);
            if (wat_enh && psp_map_wimm && t.wsf && !t.sv && t.d3 && e.f.width >= mainw && glr_vtx.size() >= 3) {
                glr_wvtx.insert(glr_wvtx.end(), glr_vtx.end() - 3, glr_vtx.end()); wst = (int)t.st_tex; wsf_frame++;
                if (toon_fno - wat_enh_ok <= 2u) glr_vtx.resize(glr_vtx.size() - 3);   // the shader draws the water: no BT3 normal-map surface
            }
        }
    }
    uint64_t gvu2 = perf_clock(); gv_t[6] += gvu2 - gvu1;
    glr_draw_batch();
    gv_t[7] += perf_clock() - gvu2;
    bool wk_now = (!glr_wvtx.empty() && wst >= 0) || !gw_mask.empty();
    if (!glr_wvtx.empty() && wst >= 0) {   // water mask: the BT3 surfaces again, alpha only, id 0x7E (depth tested, no writes)
        glr_state(states[wst]);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE); glDisable(GL_BLEND); glDepthMask(GL_FALSE); glDisable(GL_STENCIL_TEST);
        glUniform1i(glu_ston, 1); glUniform1f(glu_stref, 126.0f / 255.0f);
        glr_vtx.swap(glr_wvtx); glr_draw_batch(); glr_wvtx.clear();
        glUniform1i(glu_ston, 0); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        ol_bg[0x7E >> 4] |= (uint16_t)(1u << (0x7E & 15));
    }
    if (!gw_mask.empty()) {   // the same mask for the surfaces that went through the GPU vertex path
        glUseProgram(glr_prog);
        int cst = -1;
        glBindVertexArray(gv_vao);
        for (const GwMask& m : gw_mask) {
            if (m.st != cst) {
                glr_state(states[m.st]); cst = m.st;
                glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE); glDisable(GL_BLEND); glDepthMask(GL_FALSE); glDisable(GL_STENCIL_TEST);
                glUniform1i(glu_ston, 1); glUniform1f(glu_stref, 126.0f / 255.0f);
            }
            glDrawElements(GL_TRIANGLES, (GLsizei)m.ni, GL_UNSIGNED_INT, (const void*)(m.ib * 4));
        }
        glBindVertexArray(glr_vao);
        glUniform1i(glu_ston, 0); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        ol_bg[0x7E >> 4] |= (uint16_t)(1u << (0x7E & 15));
        gw_mask.clear();
    }
    if (wk_now && patch_water_mask_keep.val && glr_current && glr_current->width >= mainw) { wk_save(glr_current); glUseProgram(glr_prog); glBindVertexArray(glr_vao); }
    glr_wvtx.clear();
    gv_vtx.clear(); gv_idx.clear(); gv_x.clear(); gv_agv.clear();
    gv_tn_fit();
}

// bars behind the fighters: screen before (0) and after (1) the bars' packet, and before the screen effects (2); after the effects the
// pixels where the bars changed the screen and nothing covered them since are put back, so the effects never tint the HUD
static GLuint bk_tex[4] = {0, 0, 0, 0}, bk_fbo[4] = {0, 0, 0, 0}, bk_prog = 0, bk_vao = 0; static int bk_w = 0, bk_h = 0; static bool bk_ok = false; static DestGL* bk_d = nullptr;
static void bk_grab(DestGL* d, int k) {
    if (bk_w != d->w || bk_h != d->h) {
        for (int i = 0; i < 4; i++) {
            if (!bk_tex[i]) { glGenTextures(1, &bk_tex[i]); glGenFramebuffers(1, &bk_fbo[i]); }
            glBindTexture(GL_TEXTURE_2D, bk_tex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d->w, d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, bk_fbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bk_tex[i], 0);
        }
        bk_w = d->w; bk_h = d->h;
    }
    GLboolean sc = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    glr_resolve(d);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, bk_fbo[k]);
    glBlitFramebuffer(0, 0, d->w, d->h, 0, 0, d->w, d->h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    if (sc) glEnable(GL_SCISSOR_TEST);
}
// layer of the bars: the same draws on a float target that starts at 1 where the scene is dark and 0 where it is bright (per
// channel); with the screen it gives per pixel how much of the scene shows through (w) and what the bars add (c)
static GLuint kk_tex = 0, kk_ds = 0, kk_prog = 0, kk_vao = 0; static int kk_w = 0, kk_h = 0; static bool kk_ok = false;
static bool kk_begin(DestGL* d) {
    if (!kk_prog) {
        kk_prog = gl_program("#version 330 core\nvoid main(){ vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID >> 1) * 4.0 - 1.0); gl_Position = vec4(p, 0.0, 1.0); }\n",
                             "#version 330 core\nuniform sampler2D a; out vec4 o;\n"
                             "void main(){ vec4 A = texelFetch(a, ivec2(gl_FragCoord.xy), 0); o = vec4(vec3(lessThan(A.rgb, vec3(0.5))), A.a); }\n");
        if (!kk_prog) { kk_prog = 0xFFFFFFFFu; return false; }
        glGenVertexArrays(1, &kk_vao);
    }
    if (kk_prog == 0xFFFFFFFFu) return false;
    if (kk_w != d->w || kk_h != d->h) {
        if (!kk_tex) { glGenTextures(1, &kk_tex); glGenRenderbuffers(1, &kk_ds); glGenFramebuffers(1, &kk_fbo); }
        glBindTexture(GL_TEXTURE_2D, kk_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, d->w, d->h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindRenderbuffer(GL_RENDERBUFFER, kk_ds);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, d->w, d->h);
        glBindFramebuffer(GL_FRAMEBUFFER, kk_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, kk_tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, kk_ds);
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        kk_w = d->w; kk_h = d->h;
        printf("hud: capa de las barras detras %dx%d %s\n", d->w, d->h, st == GL_FRAMEBUFFER_COMPLETE ? "lista" : "incompleta, sin capa");
        if (st != GL_FRAMEBUFFER_COMPLETE) { kk_prog = 0xFFFFFFFFu; glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d)); return false; }
    }
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, d->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, kk_fbo);
    glBlitFramebuffer(0, 0, d->w, d->h, 0, 0, d->w, d->h, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, kk_fbo);
    glViewport(0, 0, d->w, d->h);
    glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(kk_prog); glBindVertexArray(kk_vao);
    glActiveTexture(GL_TEXTURE0 + 6); glBindTexture(GL_TEXTURE_2D, bk_tex[0]); glUniform1i(glGetUniformLocation(kk_prog, "a"), 6);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0); glUseProgram(glr_prog); glBindVertexArray(glr_vao);
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    return true;
}
static void glr_bars_back(bool begin) {
    DestGL* d = glr_current;
    kk_rec = false;
    if (!patch_fx_keep_bars.val || !d || d->width < 480 * scale_int()) { bk_ok = false; kk_ok = false; return; }
    if (begin) { bk_grab(d, 0); bk_ok = false; kk_ok = false; if (patch_bars_layer.val && kk_begin(d)) { kk_rec = true; kk_d = d; } return; }
    bk_grab(d, 1); bk_ok = true; bk_d = d; kk_ok = kk_d == d && kk_w == d->w && kk_h == d->h && kk_prog != 0xFFFFFFFFu && patch_bars_layer.val;
}
static GLuint bk_prog2 = 0;
static void bk_restore(DestGL* d, int mode) {   // mode 0: old rule (no layer); 1: scene back under the bars before the pass; 2: bars over the pass by their layer
    GLuint& pr = mode ? bk_prog2 : bk_prog;
    if (!pr) {
        pr = gl_program("#version 330 core\nvoid main(){ vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID >> 1) * 4.0 - 1.0); gl_Position = vec4(p, 0.0, 1.0); }\n",
                        mode ? "#version 330 core\nuniform sampler2D a; uniform sampler2D b; uniform sampler2D p; uniform sampler2D e; uniform sampler2D k; uniform int m; out vec4 o;\n"
                               "void main(){ ivec2 q = ivec2(gl_FragCoord.xy); vec3 A = texelFetch(a, q, 0).rgb, B = texelFetch(b, q, 0).rgb, P = texelFetch(p, q, 0).rgb, K = texelFetch(k, q, 0).rgb;\n"
                               "  vec3 w = (K - B) / (vec3(lessThan(A, vec3(0.5))) - A), c = B - A * w;\n"
                               "  if(!(any(greaterThan(abs(w - 1.0), vec3(0.01))) || any(greaterThan(abs(c), vec3(0.004)))) || any(greaterThan(abs(P - B), vec3(0.5/255.0)))) discard;\n"
                               "  o = m == 1 ? vec4(A, 1.0) : vec4(texelFetch(e, q, 0).rgb * clamp(w, 0.0, 1.0) + c, 1.0); }\n"
                             : "#version 330 core\nuniform sampler2D a; uniform sampler2D b; uniform sampler2D p; uniform sampler2D e; out vec4 o;\n"
                               "void main(){ ivec2 q = ivec2(gl_FragCoord.xy); vec3 A = texelFetch(a, q, 0).rgb, P = texelFetch(p, q, 0).rgb, E = texelFetch(e, q, 0).rgb; vec4 B = texelFetch(b, q, 0);\n"
                               "  if(any(greaterThan(abs(A - B.rgb), vec3(0.5/255.0))) && all(lessThan(abs(P - B.rgb), vec3(0.5/255.0)))) o = vec4(mix(E, P, B.a), 1.0); else discard; }\n");
        if (!pr) { pr = 0xFFFFFFFFu; return; }
        if (!bk_vao) glGenVertexArrays(1, &bk_vao);
    }
    if (pr == 0xFFFFFFFFu) return;
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    glViewport(0, 0, d->w, d->mfbo ? d->mh : d->h);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glUseProgram(pr); glBindVertexArray(bk_vao);
    const char* nm[5] = {"a", "b", "p", "e", "k"};
    for (int i = 0; i < (mode ? 5 : 4); i++) { glActiveTexture(GL_TEXTURE0 + 6 + i); glBindTexture(GL_TEXTURE_2D, i < 4 ? bk_tex[i] : kk_tex); glUniform1i(glGetUniformLocation(pr, nm[i]), 6 + i); }
    if (mode) glUniform1i(glGetUniformLocation(pr, "m"), mode);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glActiveTexture(GL_TEXTURE0);
    if (d->mfbo) d->dirty = true;
    static int ll = 0; if (mode != 1 && ll < 2) { ll++; printf("hud: barras detras protegidas de los efectos de pantalla (%dx%d, %s)\n", d->w, d->h, mode ? "capa de mezcla" : "regla anterior"); }
}
static void fx_run(DestGL* d, int mainw, float wave) {
    bool bk = bk_ok && bk_d == d && d && bk_w == d->w && bk_h == d->h;
    bool kl = bk && kk_ok;
    if (bk) bk_grab(d, 2);
    if (kl) bk_restore(d, 1);
    wat_pass(d, 0, 0, mainw, 272 * scale_int(), wave);
    if (bk) { bk_grab(d, 3); bk_restore(d, kl ? 2 : 0); }
    bk_ok = false; kk_ok = false;
    glActiveTexture(GL_TEXTURE0); glUseProgram(glr_prog); glBindVertexArray(glr_vao);
    if (glr_current) glBindFramebuffer(GL_FRAMEBUFFER, dfbo(glr_current));
}

// end of the frame with no HUD draw after the scene (every bar behind the fighters, HUD hidden): the screen effects run here
static void glr_fx_late() {
    struct BkEnd { ~BkEnd() { bk_ok = false; kk_ok = false; kk_rec = false; } } bk_end_;
    DestGL* d = glr_current; int mainw = 480 * scale_int();
    if (!patch_fx_late.val || !d || d->width < mainw || wat_stale >= 30 || wat_done || !wat_3d || wat_split) return;
    bool won = wat_on();
    gfxw_now = won && wat_mode == 2 && psp_map_gfxw && wat_cam[0];
    float wave = won && wat_mode != 2 && wat_cam[0] ? WAT_WAVE : 0.0f;
    bool fx_any = gfxw_now || wave > 0 || fx.rush > 0 || fx.dash > 0 || fx.imode || fx.ab > 0 || fx.lines > 0 || (FX_BLOOM > 0 && FX_BLOOMK > 0) || g3_any || atm_on();
    if (!fx_any) return;
    fx_run(d, mainw, wave);
}

// read back into guest VRAM: captures only
// end of a capture: the zoomed rect of the screen read back (colour and depth, after the antialiasing resolve); coverage = depth
// written or a colour other than the key; an optional plain line around the silhouette (TTT_GEN_OUTLINE px at x1, default 0)
static void glr_cap_read() {
    DestGL* d = cap_main;
    cap_main = nullptr;
    auto publish = [](std::vector<uint32_t>& o, int w, int h) { std::lock_guard<std::mutex> g(cap_mx); cap_out.swap(o); cap_w = w; cap_h = h; cap_done_tag = cap_tag; cap_ready = true; };
    std::vector<uint32_t> none;
    if (!d) { publish(none, 0, 0); return; }
    float ke = (float)scale_int();
    int x0 = (int)(cap_sx0 * ke + 0.5f), y0 = (int)(cap_sy0 * ke + 0.5f);
    int w = std::min(d->w - x0, (int)((cap_sx1 - cap_sx0) * ke + 0.5f)), h = std::min(d->h - y0, (int)((cap_sy1 - cap_sy0) * ke + 0.5f));
    if (w <= 0 || h <= 0) { publish(none, 0, 0); return; }
    d->dirty = true;
    glr_resolve(d);
    std::vector<uint32_t> c((size_t)w * h);
    std::vector<float> z((size_t)w * h);
    glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
    glDisable(GL_SCISSOR_TEST);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(x0, y0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, c.data());
    glReadPixels(x0, y0, w, h, GL_DEPTH_COMPONENT, GL_FLOAT, z.data());
    glBindFramebuffer(GL_FRAMEBUFFER, dfbo(d));
    glr_current = d;
    if (cap_map) {
        for (auto& v : c) v |= 0xFF000000u;
        static int ml = 0;
        if (ml++ < 6) printf("render: captura de mapa %dx%d de la pantalla (%d, %d)\n", w, h, x0, y0);
        publish(c, w, h);
        return;
    }
    float zc = z[0];
    std::vector<uint8_t> in(c.size());
    for (size_t i = 0; i < c.size(); i++) in[i] = std::fabs(z[i] - zc) > 1e-7f || (c[i] & 0xFFFFFFu) != 0x010001u;
    float ow = 0.0f;
    if (const char* e = getenv("TTT_GEN_OUTLINE")) ow = (float)atof(e);
    float rad = ow * w / 272.0f;
    long line = 0;
    if (rad > 0.0f) {   // chamfer distance to the model
        std::vector<float> dd(c.size());
        for (size_t i = 0; i < c.size(); i++) dd[i] = in[i] ? 0.0f : 1e9f;
        const float D1 = 1.0f, D2 = 1.4142f;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                float& v = dd[(size_t)y * w + x];
                if (x > 0) v = std::min(v, dd[(size_t)y * w + x - 1] + D1);
                if (y > 0) { v = std::min(v, dd[(size_t)(y - 1) * w + x] + D1); if (x > 0) v = std::min(v, dd[(size_t)(y - 1) * w + x - 1] + D2); if (x + 1 < w) v = std::min(v, dd[(size_t)(y - 1) * w + x + 1] + D2); }
            }
        for (int y = h - 1; y >= 0; y--)
            for (int x = w - 1; x >= 0; x--) {
                float& v = dd[(size_t)y * w + x];
                if (x + 1 < w) v = std::min(v, dd[(size_t)y * w + x + 1] + D1);
                if (y + 1 < h) { v = std::min(v, dd[(size_t)(y + 1) * w + x] + D1); if (x + 1 < w) v = std::min(v, dd[(size_t)(y + 1) * w + x + 1] + D2); if (x > 0) v = std::min(v, dd[(size_t)(y + 1) * w + x - 1] + D2); }
            }
        for (size_t i = 0; i < c.size(); i++) if (!in[i] && dd[i] <= rad) { in[i] = 1; c[i] = 0xFF000000u; line++; }
    }
    long cov = 0;
    for (size_t i = 0; i < c.size(); i++) { if (in[i]) { cov++; c[i] |= 0xFF000000u; } else c[i] = 0; }
    static int logs = 0;
    if (logs++ < 6) printf("render: captura %dx%d de la pantalla (%d, %d), fondo de profundidad %.3f, %.1f%% cubierto, linea extra %.1f px (%ld pixeles)\n", w, h, x0, y0, zc, 100.0 * cov / (double)c.size(), rad, line);
    publish(c, w, h);
}

static void glr_read(uint32_t dir, int width, int fmt) {
    DestGL* d = nullptr;
    for (int i = 0; i < glr_dest_n; i++) if (glr_dest[i].dir == dir) d = &glr_dest[i];
    if (!d) return;
    static std::vector<uint32_t> px;
    px.resize((size_t)d->w * d->h);
    glr_resolve(d);
    glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
    glReadPixels(0, 0, d->w, d->h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
    FB f; f.dir = dir; f.width = width; f.fmt = fmt;
    for (int y = 0; y < d->h; y++)
        for (int x = 0; x < d->w && x < width; x++) write_pix(f, x, y, px[(size_t)y * d->w + x]);
}

// menu probe: rows (or the whole image) of a finished target, bottom-up RGBA
static bool glr_peek(uint32_t dir, int rows, std::vector<uint32_t>& px, int* w, int* h) {
    DestGL* d = nullptr;
    for (int i = 0; i < glr_dest_n; i++) if (glr_dest[i].dir == dir) d = &glr_dest[i];
    if (!d) return false;
    *w = d->w; *h = d->h;
    glr_resolve(d);
    glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
    if (rows <= 0) { px.resize((size_t)d->w * d->h); glReadPixels(0, 0, d->w, d->h, GL_RGBA, GL_UNSIGNED_BYTE, px.data()); }
    else { px.resize((size_t)d->w * rows); for (int r = 0; r < rows; r++) glReadPixels(0, (2 * r + 1) * d->h / (2 * rows), d->w, 1, GL_RGBA, GL_UNSIGNED_BYTE, px.data() + (size_t)r * d->w); }
    glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
    return true;
}

// paints a finished target black (port blackout while a scene hop shows a screen that should not be seen)
static void glr_blank(uint32_t dir) {
    DestGL* d = nullptr;
    for (int i = 0; i < glr_dest_n; i++) if (glr_dest[i].dir == dir) d = &glr_dest[i];
    if (!d) return;
    glr_resolve(d);
    GLboolean sc = glIsEnabled(GL_SCISSOR_TEST), cm[4];
    GLfloat cc[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, cm);
    glGetFloatv(0x0C22, cc);
    glBindFramebuffer(GL_FRAMEBUFFER, d->fbo);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(cc[0], cc[1], cc[2], cc[3]);
    glColorMask(cm[0], cm[1], cm[2], cm[3]);
    if (sc) glEnable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
}

static int glr_fence_var = -1;

static void glr_report() {
    if (!glr_enabled()) return;
    printf("gl: %d destinos, %lld lecturas de destino (%lld sin pareja), %lld copias de destino (%lld Mpx)\n",
           glr_dest_n, glr_reads, glr_no_dest, glr_copies, glr_copy_px / 1000000);
    for (int i = 0; i < glr_dest_n; i++)
        printf("gl:   destino %d 0x%08X %dx%d ancho %d fmt %d\n", i, glr_dest[i].dir, glr_dest[i].w, glr_dest[i].h,
               glr_dest[i].width, glr_dest[i].fmt);
}

static unsigned glr_twin_tex(uint32_t dir, int* w, int* h) {
    for (int i = 0; i < glr_dest_n; i++) if (glr_dest[i].dir == dir) { glr_resolve(&glr_dest[i]); *w = glr_dest[i].w; *h = glr_dest[i].h; return glr_dest[i].tex; }
    return 0;
}

// FXAA (Graphics "Antialiasing", TTT_FXAA): classic FXAA over the whole frame before it goes to the window
static GLuint fxaa_prog = 0, fxaa_vao = 0, fxaa_fbo[2] = {0, 0}, fxaa_tex[2] = {0, 0}; static GLint fxaa_u_src = -1, fxaa_u_r = -1; static int fxaa_w = 0, fxaa_h = 0, fxaa_k = 0;   // two outputs: the window may still show the last one
static int glr_fxaa() {
    static int v = 1, n = 0;
    if ((n++ & 31) == 0) { const char* e = getenv("TTT_FXAA"); v = e && *e ? atoi(e) : 0; }
    return v;
}
static GLuint fxaa_pass(DestGL* d, int w, int h) {
    GqZone gqz_(GQ_FXAA);
    if (!fxaa_prog) {
        fxaa_prog = gl_program("#version 330 core\nvoid main(){ vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID >> 1) * 4.0 - 1.0); gl_Position = vec4(p, 0.0, 1.0); }\n",
            "#version 330 core\nuniform sampler2D src; uniform vec4 r; out vec4 o;\n"
            "vec3 t(vec2 p){ return texture(src, clamp(p, vec2(0.5)*r.zw, (r.xy - 0.5)*r.zw)).rgb; }\n"
            "void main(){\n"
            "  vec2 px = r.zw, uv = gl_FragCoord.xy*px; const vec3 L = vec3(0.299, 0.587, 0.114);\n"
            "  vec3 nw = t(uv + vec2(-1.0,-1.0)*px), ne = t(uv + vec2(1.0,-1.0)*px), sw = t(uv + vec2(-1.0,1.0)*px), se = t(uv + vec2(1.0,1.0)*px), m = t(uv);\n"
            "  float lnw = dot(nw,L), lne = dot(ne,L), lsw = dot(sw,L), lse = dot(se,L), lm = dot(m,L);\n"
            "  float lmin = min(lm, min(min(lnw,lne), min(lsw,lse))), lmax = max(lm, max(max(lnw,lne), max(lsw,lse)));\n"
            "  vec2 dir = vec2(-((lnw + lne) - (lsw + lse)), (lnw + lsw) - (lne + lse));\n"
            "  float red = max((lnw + lne + lsw + lse)*(0.25/8.0), 1.0/128.0), rcp = 1.0/(min(abs(dir.x), abs(dir.y)) + red);\n"
            "  dir = clamp(dir*rcp, vec2(-8.0), vec2(8.0))*px;\n"
            "  vec3 a = 0.5*(t(uv + dir*(1.0/3.0 - 0.5)) + t(uv + dir*(2.0/3.0 - 0.5)));\n"
            "  vec3 b = a*0.5 + 0.25*(t(uv - dir*0.5) + t(uv + dir*0.5));\n"
            "  float lb = dot(b,L); o = vec4((lb < lmin || lb > lmax) ? a : b, 1.0);\n"
            "}\n");
        glGenVertexArrays(1, &fxaa_vao);
        fxaa_u_src = glGetUniformLocation(fxaa_prog, "src"); fxaa_u_r = glGetUniformLocation(fxaa_prog, "r");
    }
    if (!fxaa_prog) return 0;
    if (fxaa_w != d->w || fxaa_h != d->h) {
        for (int k = 0; k < 2; k++) {
            if (!fxaa_tex[k]) { glGenTextures(1, &fxaa_tex[k]); glGenFramebuffers(1, &fxaa_fbo[k]); }
            glBindTexture(GL_TEXTURE_2D, fxaa_tex[k]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d->w, d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, fxaa_fbo[k]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fxaa_tex[k], 0);
        }
        fxaa_w = d->w; fxaa_h = d->h;
        printf("gl: fxaa listo %dx%d\n", w, h);
    }
    fxaa_k ^= 1;
    glBindFramebuffer(GL_FRAMEBUFFER, fxaa_fbo[fxaa_k]);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(fxaa_prog); glBindVertexArray(fxaa_vao);
    glActiveTexture(GL_TEXTURE0 + 6); glBindTexture(GL_TEXTURE_2D, d->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(fxaa_u_src, 6); glUniform4f(fxaa_u_r, (float)w, (float)h, 1.0f / d->w, 1.0f / d->h);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glActiveTexture(GL_TEXTURE0);
    if (glr_prog) { glUseProgram(glr_prog); glBindVertexArray(glr_vao); }
    glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
    glEnable(GL_SCISSOR_TEST);
    return fxaa_tex[fxaa_k];
}

// smooth motion: motion buffer (vertex motion of the opaque scene; depth-written draws pass only where they are the visible surface)
// and the in-between picture warped from the finished one; mode 1 = in-between before the real picture, 2 = after it (no added latency)
static GLuint sm_mprog[2] = {0, 0}, sm_wprog[2] = {0, 0}, sm_vao = 0, sm_vbo = 0, sm_evao = 0, sm_mtex = 0;
static GLuint sm_mfbo[GLR_DEST] = {0}, sm_otex[2] = {0, 0}, sm_ofbo[2] = {0, 0};
static int sm_mw = 0, sm_mh = 0, sm_mms = -1, sm_ow = 0, sm_oh = 0, sm_ok = 0;
static GLuint sm_mdep[GLR_DEST] = {0};
static int sm_mode_v = 0;
static int sm_mode() {
    static int n = 0;
    if ((n++ & 31) == 0) { const char* e = getenv("TTT_SMOOTH"); sm_mode_v = e && *e ? atoi(e) : 0; }
    return sm_mode_v;
}
static float SM_CUT_KN = 0.35f, SM_CUT_MV = 0.12f;
static long sm_st_f = 0, sm_st_mid = 0, sm_st_cut = 0, sm_st_hud = 0, sm_st_few = 0; static double sm_st_us = 0, sm_st_kn = 0, sm_st_mv = 0;

static bool sm_build(int ms) {
    for (int k = 0; k < 2; k++) {
        if (sm_mprog[k] && sm_wprog[k]) continue;
        sm_mprog[k] = gl_program(
            "#version 330 core\nlayout(location=0) in vec4 pos; layout(location=1) in vec4 pv; out vec4 vp;\nvoid main(){ gl_Position = pos; vp = pv; }\n",
            "#version 330 core\nin vec4 vp; uniform vec2 vs; out vec4 o;\nvoid main(){ vec2 q = (vp.xy / vp.z * 0.5 + 0.5) * vs; o = vec4(gl_FragCoord.xy - q, vp.w, 1.0); }\n");
        std::string fs = std::string("#version 330 core\n") + (k ? "uniform sampler2DMS mot;\n" : "uniform sampler2D mot;\n") +
            "uniform sampler2D fin, pre, scn; uniform float kk; uniform int hud; uniform vec2 lim; out vec4 o;\n"
            "vec3 mv(vec2 p){ return texelFetch(mot, ivec2(clamp(p, vec2(0.0), lim)), 0).xyz; }\n"
            "bool isHud(ivec2 p){ return hud != 0 && any(greaterThan(abs(texelFetch(pre, p, 0).rgb - texelFetch(scn, p, 0).rgb), vec3(0.6/255.0))); }\n"
            "void main(){\n"
            "  vec2 q = gl_FragCoord.xy; ivec2 iq = ivec2(q);\n"
            "  if (isHud(iq)) { o = texelFetch(fin, iq, 0); return; }\n"
            "  vec3 m = mv(q); vec2 s = q + kk * m.xy * m.z; m = mv(s); s = q + kk * m.xy * m.z;\n"
            "  ivec2 is = ivec2(clamp(s, vec2(0.0), lim));\n"
            "  o = isHud(is) ? texelFetch(scn, is, 0) : texelFetch(fin, is, 0);\n"
            "}\n";
        sm_wprog[k] = gl_program("#version 330 core\nvoid main(){ vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID >> 1) * 4.0 - 1.0); gl_Position = vec4(p, 0.0, 1.0); }\n", fs.c_str());
        if (!sm_mprog[k] || !sm_wprog[k]) { printf("suave: no pude preparar los programas\n"); return false; }
    }
    if (!sm_vao) {
        glGenVertexArrays(1, &sm_vao); glGenBuffers(1, &sm_vbo); glGenVertexArrays(1, &sm_evao);
        glBindVertexArray(sm_vao); glBindBuffer(GL_ARRAY_BUFFER, sm_vbo);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(SmV), (const void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(SmV), (const void*)16);
    }
    (void)ms;
    return true;
}

static GLint ul_cache(GLuint p, const char* nm) {   // name pointers are literals
    static struct { GLuint p; const char* n; GLint l; } c[32]; static int nc = 0;
    for (int i = 0; i < nc; i++) if (c[i].p == p && c[i].n == nm) return c[i].l;
    GLint l = glGetUniformLocation(p, nm);
    if (nc < 32) c[nc++] = {p, nm, l};
    return l;
}
static GLuint sm_present(DestGL* d, GLuint fin, int ke) {
    GqZone gqz_(GQ_SM);
    int mode = sm_mode();
    int di = (int)(d - glr_dest);
    uint64_t vn = 0, kn = 0;
    for (const SmV& v : sm_mv) if (v.dst == di) { vn++; if (v.kn > 0.5f) kn++; }
    if (!mode) return 0;
    if (vn < 300) { sm_st_few++; return 0; }
    auto t0 = std::chrono::steady_clock::now();
    sm_st_f++;
    double kf = (double)kn / (double)vn, mvm = kn ? sm_mvsum / (double)kn : 0.0;
    sm_st_kn += kf; sm_st_mv += mvm / ke;
    if (kf < SM_CUT_KN || mvm > SM_CUT_MV * 480.0 * ke) { sm_st_cut++; return 0; }
    int ms = d->mfbo ? glr_msaa() : 0, k = ms ? 1 : 0;
    if (!sm_build(ms)) return 0;
    int mh = d->mfbo ? d->mh : d->h;
    if (sm_mw != d->w || sm_mh != mh || sm_mms != ms) {
        if (sm_mtex) glDeleteTextures(1, &sm_mtex);
        glGenTextures(1, &sm_mtex);
        if (ms) { glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, sm_mtex); glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, ms, GL_RGBA16F, d->w, mh, GL_TRUE); glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0); }
        else { glBindTexture(GL_TEXTURE_2D, sm_mtex); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, d->w, mh, 0, GL_RGBA, GL_FLOAT, nullptr); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); }
        for (int i = 0; i < GLR_DEST; i++) sm_mdep[i] = 0;
        sm_mw = d->w; sm_mh = mh; sm_mms = ms;
        printf("suave: bufer de movimiento %dx%d, muestras %d\n", d->w, mh, ms);
    }
    GLuint dep = d->mfbo ? d->mdep : (d->dtex ? d->dtex : d->depth);
    if (!sm_mfbo[di]) glGenFramebuffers(1, &sm_mfbo[di]);
    glBindFramebuffer(GL_FRAMEBUFFER, sm_mfbo[di]);
    if (sm_mdep[di] != dep) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, ms ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, sm_mtex, 0);
        if (!d->mfbo && d->dtex) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, dep, 0);
        else glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dep);
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (st != GL_FRAMEBUFFER_COMPLETE) { printf("suave: bufer de movimiento incompleto (0x%X)\n", st); glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0); return 0; }
        sm_mdep[di] = dep;
    }
    const int qm = nq_mode(); const uint64_t qt = perf_clock();
    GLint vp[4] = {0, 0, 0, 0}, prog = 0, vao = 0, at = GL_TEXTURE0, df = GL_LESS; GLboolean cm[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE}, dm = GL_FALSE;
    bool sc = false, dt = false, bl = false, st = false, cf = false;
    if (!qm) {
        SubT q_(glq_sid());
        glGetIntegerv(GL_VIEWPORT, vp); glGetIntegerv(GL_CURRENT_PROGRAM, &prog); glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao); glGetIntegerv(GL_ACTIVE_TEXTURE, &at); glGetIntegerv(GL_DEPTH_FUNC, &df);
        glGetBooleanv(GL_COLOR_WRITEMASK, cm); glGetBooleanv(GL_DEPTH_WRITEMASK, &dm);
        sc = glIsEnabled(GL_SCISSOR_TEST); dt = glIsEnabled(GL_DEPTH_TEST); bl = glIsEnabled(GL_BLEND); st = glIsEnabled(GL_STENCIL_TEST); cf = glIsEnabled(GL_CULL_FACE);
        gls_cmp(vp, prog, vao, at, cm, dm, sc, dt, bl, st, -1, cf);
    }
    glViewport(0, 0, d->w, d->h);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_STENCIL_TEST); glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glDepthMask(GL_FALSE);
    glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
    sm_up.clear();
    for (const SmV& v : sm_mv) if (v.dst == di && !v.zw) sm_up.push_back(v);
    size_t nz = sm_up.size();
    for (const SmV& v : sm_mv) if (v.dst == di && v.zw) sm_up.push_back(v);
    glUseProgram(sm_mprog[k]); glBindVertexArray(sm_vao);
    glUniform2f(ul_cache(sm_mprog[k], "vs"), (float)d->w, (float)d->h);
    glBindBuffer(GL_ARRAY_BUFFER, sm_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sm_up.size() * sizeof(SmV)), sm_up.data(), GL_STREAM_DRAW);
    glDisable(GL_DEPTH_TEST);
    if (nz) glDrawArrays(GL_TRIANGLES, 0, (GLsizei)nz);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_EQUAL);
    if (sm_up.size() > nz) glDrawArrays(GL_TRIANGLES, (GLint)nz, (GLsizei)(sm_up.size() - nz));
    glDisable(GL_DEPTH_TEST);
    if (sm_ow != d->w || sm_oh != d->h) {
        for (int i = 0; i < 2; i++) {
            if (!sm_otex[i]) { glGenTextures(1, &sm_otex[i]); glGenFramebuffers(1, &sm_ofbo[i]); }
            glBindTexture(GL_TEXTURE_2D, sm_otex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, d->w, d->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, sm_ofbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sm_otex[i], 0);
        }
        sm_ow = d->w; sm_oh = d->h;
    }
    sm_ok ^= 1;
    int w = 480 * ke, h = 272 * ke;
    bool hud = sm_scn_f == toon_fno && sm_scn_dst == di && sm_scn_tex;
    if (hud) sm_st_hud++;
    glBindFramebuffer(GL_FRAMEBUFFER, sm_ofbo[sm_ok]);
    glViewport(0, 0, w, h);
    GLuint wp = sm_wprog[k];
    glUseProgram(wp); glBindVertexArray(sm_evao);
    glActiveTexture(GL_TEXTURE0 + 8); glBindTexture(ms ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D, sm_mtex);
    glActiveTexture(GL_TEXTURE0 + 9); glBindTexture(GL_TEXTURE_2D, fin);
    glActiveTexture(GL_TEXTURE0 + 10); glBindTexture(GL_TEXTURE_2D, d->tex);
    glActiveTexture(GL_TEXTURE0 + 11); glBindTexture(GL_TEXTURE_2D, hud ? sm_scn_tex : d->tex);
    glUniform1i(ul_cache(wp, "mot"), 8); glUniform1i(ul_cache(wp, "fin"), 9);
    glUniform1i(ul_cache(wp, "pre"), 10); glUniform1i(ul_cache(wp, "scn"), 11);
    glUniform1f(ul_cache(wp, "kk"), mode == 2 ? -0.5f : 0.5f);
    glUniform1i(ul_cache(wp, "hud"), hud ? 1 : 0);
    glUniform2f(ul_cache(wp, "lim"), (float)(w - 1), (float)(h - 1));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (qm) { glActiveTexture(GL_TEXTURE0); glUseProgram(glr_prog); glBindVertexArray(glr_vao); glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0); }   // the next flush rebuilds the draw state
    else {
        glActiveTexture((GLenum)at);
        glUseProgram((GLuint)prog); glBindVertexArray((GLuint)vao);
        glBindFramebuffer(GL_FRAMEBUFFER, glr_current ? dfbo(glr_current) : 0);
        glViewport(vp[0], vp[1], vp[2], vp[3]);
        glColorMask(cm[0], cm[1], cm[2], cm[3]); glDepthMask(dm); glDepthFunc((GLenum)df);
        if (sc) glEnable(GL_SCISSOR_TEST); if (dt) glEnable(GL_DEPTH_TEST); if (bl) glEnable(GL_BLEND); if (st) glEnable(GL_STENCIL_TEST); if (cf) glEnable(GL_CULL_FACE);
    }
    nq_st[qm][8]++; nq_st[qm][9] += perf_clock() - qt;
    sm_st_mid++;
    sm_st_us += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    return sm_otex[sm_ok];
}

#include "gpu/burst.h"

// every pass program compiled at the first frame (a pass compiling on its first use stalled: first charge, first water, first sparks)
static void glr_prewarm() {
    auto t0 = std::chrono::steady_clock::now();
    gl_prewarm(WAT_VS, WAT_FS); gl_prewarm(WAT_VS, FX_BFS); gl_prewarm(WAT_VS, ATM_FS); gl_prewarm(WAT_VS, CDK_FS);
    gl_prewarm(WAT_VS, OL_FS); gl_prewarm(WSE_VS, WSE_FS); gl_prewarm(PB_VS, PB_FS);
    printf("gl: programas de los pases compilados por adelantado en %.0f ms\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
}

static void gq_frame() {
    if (!gq_tid_set) { gq_tid = std::this_thread::get_id(); gq_tid_set = true; return; }
    if (gq_ok <= 0 || gq_active >= 0) return;
    gq_slot = (gq_slot + 1) & 15;
    int s = gq_slot;   // the oldest slot: 15 frames ago (the driver keeps several frames queued behind the window's present)
    if (gq_n[s]) {
        GLint av = 0; gq_getiv(gq_ids[s][gq_n[s] - 1], 0x8867, &av);
        if (!av) gq_lost++;
        else {
            double t = 0;
            double ta = 0;
            for (int i = 0; i < gq_n[s]; i++) { unsigned long long ns = 0; gq_get64(gq_ids[s][i], 0x8866, &ns); gq_ms[gq_z[s][i]] += ns / 1e6; t += ns / 1e6; if (gq_z[s][i] == GQ_ATM) ta += ns / 1e6; }
            if (ta > 0) { atab_ms[gq_ab[s]] += ta; atab_n[gq_ab[s]]++; }
            gq_max = std::max(gq_max, t); gq_frames++;
        }
    }
    gq_n[s] = 0; gq_ab[s] = (uint8_t)atab_cur;
    if (gq_every < 0) { const char* e = getenv("TTT_GPU_PROF_N"); gq_every = e && *e ? std::max(1, atoi(e)) : 4; }
    gq_rf++; gq_samp = gq_rf % gq_every == 0;
    static int lines = 0;
    if (gq_frames + gq_lost >= 300 / gq_every && lines < 60) {   // also when every query came back unread: the line says how many
        long gqf = gq_frames; if (!gq_frames) gq_frames = 1;
        lines++;
        double grf = (double)std::max(1L, gq_rf - gq_rf0); gq_rf0 = gq_rf;
        double tot = 0; for (int z = 0; z < GQ_N; z++) tot += gq_ms[z];
        printf("gpu: ms por cuadro %.2f (peor %.1f, %ld cuadros leidos) |", tot / gq_frames, gq_max, gqf);
        for (int z = 0; z < GQ_N; z++) if (gq_ms[z] > 0) { printf(" %s %.2f", GQ_NOM[z], gq_ms[z] / gq_frames); if (z >= GQ_FIG) printf(" (%.1f Mpx, %.0f dibujos)", gq_px[z] / 1e6 / gq_frames, gq_dn[z] / (double)gq_frames); }
        printf(" | copias %.1f (%.1f Mpx) por cuadro, %ld sin leer | dibujos ligeros %.0f de %.0f por cuadro\n", (glr_copies - gq_cp0) / grf, (glr_copy_px - gq_px0) / 1e6 / grf, gq_lost, glr_lite_n / grf, glr_draw_n / grf);
        static int bigl = 0;
        if (bigl < 20 && gq_big_n) {
            bigl++;
            std::sort(gq_big, gq_big + gq_big_n, [](const BigDraw& a, const BigDraw& b) { return a.px > b.px; });
            printf("gpu: 2D grandes por cuadro:");
            for (int i = 0; i < gq_big_n && i < 6; i++) printf(" | tex 0x%08X %dx%d mezcla %d: %.2f Mpx en %.1f dibujos", gq_big[i].dir, gq_big[i].w, gq_big[i].h, gq_big[i].bl, gq_big[i].px / 1e6 / gq_frames, gq_big[i].n / (double)gq_frames);
            printf("\n");
        }
        printf("gpu: delineado en caja %.1f pasadas por cuadro, %.1f a pantalla completa | borrados rapidos %.1f, omitidos alfa 0: %.2f Mpx por cuadro | pase final de efectos omitido %.2f, oscuridad sin copia %.2f, carga sola %.2f (%.2f Mpx) por cuadro\n", olb_n / grf, olb_full / grf, glr_clr_n / grf, glr_a0_px / 1e6 / grf, wat_skip_n / grf, gl_tbar_n / grf, atb_n / grf, atb_px / 1e6 / grf);
        printf("gpu: profundidad leida directa %.1f veces por cuadro, copiada %.1f\n", dd_n[0] / grf, dd_n[1] / grf); dd_n[0] = dd_n[1] = 0;
        olb_n = olb_full = 0; glr_clr_n = 0; glr_a0_px = 0; wat_skip_n = 0; gl_tbar_n = 0; atb_n = 0; atb_px = 0;
        gq_big_n = 0; for (auto& x : gq_px) x = 0; for (auto& x : gq_dn) x = 0;
        fflush(stdout); glr_lite_n = glr_draw_n = 0;
        for (auto& x : gq_ms) x = 0; gq_max = 0; gq_frames = 0; gq_lost = 0; gq_cp0 = glr_copies; gq_px0 = glr_copy_px;
    }
}
static bool glr_present(uint32_t dir) {
    DestGL* d = nullptr;
    for (int i = 0; i < glr_dest_n; i++) if (glr_dest[i].dir == dir) d = &glr_dest[i];
    if (!d) return false;
    { static bool pw = false; if (!pw) { pw = true; glr_prewarm(); } }
    if (glr_fence_var < 0) { const char* e = getenv("TTT_FENCE"); glr_fence_var = e ? atoi(e) : 1; }
    glr_resolve(d);
    int ke = scale_int();
    GLuint pt = d->tex, pb = pb_present(d, ke);
    if (pb) pt = pb;
    else if (glr_fxaa() > 0) { GLuint f = fxaa_pass(d, 480 * ke, 272 * ke); if (f) pt = f; }
    GLuint mid = sm_on && !pb ? sm_present(d, pt, ke) : 0;
    void* v = nullptr;
    gq_frame();
    if (glr_fence_var) { v = (void*)glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0); glFlush(); }
    else glFinish();
    psp_window_texture(pt, d->w, d->h, 480 * ke, 272 * ke, v);
    psp_window_texture_mid(mid, mid ? sm_mode() : 0);
    return true;
}

#else
struct SmV { int dst; }; static std::vector<SmV> sm_mv; static double sm_mvsum = 0; static int sm_mode() { return 0; }
static long sm_st_f = 0, sm_st_mid = 0, sm_st_cut = 0, sm_st_hud = 0, sm_st_few = 0; static double sm_st_us = 0, sm_st_kn = 0, sm_st_mv = 0;
static int glr_up_of(int, int) { return 0; }
static void glr_tex_free(unsigned) {}
static void glr_upload_async(Chain&, int) {}
static bool glr_view_copy(uint32_t, int, int, int, int, int, bool) { return false; }

static void glr_report() {}
static bool glr_enabled() { return false; }
// F5 saves the decoded texture of every state of the current flush as PNG
static int glr_dumped = 0;

static void glr_dump(const TexState& e) {
    if (!e.chain || glr_dumped >= 24) return;
    char nm[160];
    snprintf(nm, sizeof(nm), "tex_%02d_%08X_%dx%d_fmt%d_swz%d_stride%d.png",
             glr_dumped, e.t.dir, e.t.w, e.t.h, e.t.fmt, e.t.swz, e.t.stride);
    save_png(nm, e.chain->px[0].data(), e.chain->aw[0], e.chain->ah[0]);
    printf("volcada %s (niveles %d)\n", nm, e.chain->nn);
    glr_dumped++;
}

static void glr_flush_queue() {}
static void glr_cap_read() {}
static void glr_alpha_keep(bool) {}
static void glr_bars_back(bool) {}
static void glr_read(uint32_t, int, int) {}
static void glr_blank(uint32_t) {}
static bool glr_peek(uint32_t, int, std::vector<uint32_t>&, int*, int*) { return false; }
static bool glr_present(uint32_t) { return false; }
void psp_burst_load(const std::string&) {}
static unsigned glr_twin_tex(uint32_t, int*, int*) { return 0; }

static unsigned glr_palette() { return 0; }

#endif
