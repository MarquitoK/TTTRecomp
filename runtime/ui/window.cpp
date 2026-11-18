#include "psp.h"
#include "perf.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>

// Ventana nativa y teclado. En Windows usa Win32 directo, sin dependencias que instalar.
// En cualquier otro sistema queda como funciones vacias para que el modo sin ventana siga compilando.

static std::vector<uint32_t> lienzo((size_t)480 * 272);
static int lien_w = 480, lien_h = 272;
static uint32_t* pixeles = lienzo.data();
static int escala = 2;
static bool abierta = false;

#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned __stdcall timeBeginPeriod(unsigned);
#include <windows.h>
#define TTT_GL_IMPL
#include "gpu/gl.h"

// los drivers buscan estos simbolos exportados para asignar la GPU dedicada en equipos hibridos
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

static HWND ventana = nullptr;
static BITMAPINFO info;
static uint32_t botones = 0;
static int cli_w = 480 * 2, cli_h = 272 * 2;
static bool completa = false;
static RECT antes_completa;
static LONG estilo_antes = 0;
static int vista_var = -1;

// TTT_FIT: 0 bandas negras sin deformar, 1 estirar a la ventana, 2 llenar ampliando el campo 3D
static int modo_vista() {
    if (vista_var < 0) { const char* e = getenv("TTT_FIT"); vista_var = e ? atoi(e) : 2; }
    return vista_var;
}

// rectangulo de destino segun el modo
static void destino_en(int cw, int ch, int* dx, int* dy, int* dw, int* dh) {
    if (modo_vista() != 0) { *dx = 0; *dy = 0; *dw = cw; *dh = ch; return; }
    double a = 480.0 / 272.0;
    int w = cw, h = (int)(cw / a + 0.5);
    if (h > ch) { h = ch; w = (int)(ch * a + 0.5); }
    *dw = w; *dh = h;
    *dx = (cw - w) / 2; *dy = (ch - h) / 2;
}
static void destino(int* dx, int* dy, int* dw, int* dh) { destino_en(cli_w, cli_h, dx, dy, dw, dh); }

// el escalado lo hace el propio runtime y el blit va 1 a 1:
// dejarselo a StretchDIBits costaba 80 ms por cuadro a 1920x1200
static std::vector<uint32_t> buf_esc;
static std::vector<int> mapa_x;
static BITMAPINFO info_esc;
static int esc_w = 0, esc_h = 0, esc_lw = 0, esc_lh = 0;
static bool limpiar_fondo = true;

static void escalar(int dw, int dh) {
    if (dw != esc_w || dh != esc_h || lien_w != esc_lw || lien_h != esc_lh) {
        esc_w = dw; esc_h = dh; esc_lw = lien_w; esc_lh = lien_h;
        buf_esc.resize((size_t)dw * dh);
        mapa_x.resize(dw);
        for (int x = 0; x < dw; x++) mapa_x[x] = (int)((int64_t)x * lien_w / dw);
        memset(&info_esc, 0, sizeof(info_esc));
        info_esc.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info_esc.bmiHeader.biWidth = dw;
        info_esc.bmiHeader.biHeight = -dh;
        info_esc.bmiHeader.biPlanes = 1;
        info_esc.bmiHeader.biBitCount = 32;
        info_esc.bmiHeader.biCompression = BI_RGB;
    }
    const int* mx = mapa_x.data();
    uint32_t* d = buf_esc.data();
    for (int y = 0; y < dh; y++) {
        const uint32_t* f = pixeles + (size_t)((int64_t)y * lien_h / dh) * lien_w;
        uint32_t* o = d + (size_t)y * dw;
        for (int x = 0; x < dw; x++) o[x] = f[mx[x]];
    }
}

static void pintar_en(HDC dc) {
    int dx, dy, dw, dh;
    destino(&dx, &dy, &dw, &dh);
    if (dw < 1 || dh < 1) return;
    if (limpiar_fondo) {
        limpiar_fondo = false;
        RECT r = {0, 0, cli_w, cli_h};
        FillRect(dc, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }
    info.bmiHeader.biWidth = lien_w;
    info.bmiHeader.biHeight = -lien_h;
    if (dw == lien_w && dh == lien_h) {
        SetDIBitsToDevice(dc, dx, dy, lien_w, lien_h, 0, 0, 0, lien_h, pixeles, &info, DIB_RGB_COLORS);
        return;
    }
    escalar(dw, dh);
    SetDIBitsToDevice(dc, dx, dy, dw, dh, 0, 0, 0, dh, buf_esc.data(), &info_esc, DIB_RGB_COLORS);
}

// presentacion por GPU: el lienzo sube como textura y se pinta un triangulo que cubre la ventana
static bool usar_gl = false;
static GLuint tex_gl = 0, prog_gl = 0, vao_gl = 0;
static int tex_w = 0, tex_h = 0, filtro_var = -1;
static GLuint tex_ext = 0;
static float ext_su = 1.0f, ext_sv = 1.0f;
static int ext_tw = 0, ext_th = 0;
static GLsync valla_ext = nullptr;
static int repetir_n = 1;
static LARGE_INTEGER frec_rep = {}, t_rep = {};
// smooth motion: the in-between picture shown half a game frame before (mode 1) or after (mode 2) the real one
static GLuint tex_mid = 0; static int mid_mode = 0; static bool mid_pend = false, mid_show = false;
static double mid_due = 0, mid_prev = 0, mid_per = 1.0 / 30.0, mid_lat = 0, mid_lmax = 0; static int mid_n = 0;
static GLint u_escuv = -1, u_tamtex = -1, u_razon = -1, u_nitidez = -1, u_modo = -1, u_tramo = -1, u_velo = -1;

// TTT_FILTER: 0 vecino, 1 bilineal, 2 caja al reducir y Catmull-Rom al ampliar
static int filtro_modo() {
    if (filtro_var < 0) { const char* e = getenv("TTT_FILTER"); filtro_var = e ? atoi(e) : 2; }
    return filtro_var;
}
static int filtro_gl() { return filtro_modo() == 0 ? GL_NEAREST : GL_LINEAR; }

static float nitidez_var = -1.0f;
static float sharpness() {
    if (nitidez_var < 0.0f) { const char* e = getenv("TTT_SHARPNESS"); nitidez_var = e ? (float)atof(e) : 0.0f; }
    return nitidez_var;
}

static const char* VS_PRES =
    "#version 330 core\n"
    "out vec2 uv;\n"
    "void main(){\n"
    "  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
    "  uv = vec2(p.x, 1.0 - p.y);\n"
    "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

static const char* FS_PRES =
    "#version 330 core\n"
    "in vec2 uv;\n"
    "uniform sampler2D lienzo;\n"
    "uniform vec2 esc_uv;\n"
    "uniform vec2 tam_tex;\n"
    "uniform vec2 razon;\n"
    "uniform float nitidez;\n"
    "uniform int modo;\n"
    "uniform vec2 tramo;\n"
    "uniform float velo;\n"
    "out vec4 salida;\n"
    "vec3 tap(vec2 t){ return texture(lienzo, clamp(t, vec2(0.5), esc_uv * tam_tex - vec2(0.5)) / tam_tex).rgb; }\n"
    "float cr(float x){ x = abs(x);\n"
    "  if (x < 1.0) return 1.5*x*x*x - 2.5*x*x + 1.0;\n"
    "  if (x < 2.0) return -0.5*x*x*x + 2.5*x*x - 4.0*x + 2.0;\n"
    "  return 0.0; }\n"
    "void main(){\n"
    "  vec2 u2 = vec2(tramo.x + uv.x * tramo.y, uv.y);\n"
    "  vec2 tc = u2 * esc_uv * tam_tex;\n"
    "  vec3 c;\n"
    "  if (modo < 2) { c = texture(lienzo, u2 * esc_uv).rgb; }\n"
    "  else if (razon.x > 1.25 || razon.y > 1.25) {\n"
    "    int nx = int(min(ceil(razon.x), 4.0)), ny = int(min(ceil(razon.y), 4.0));\n"
    "    vec2 paso = razon / vec2(float(nx), float(ny));\n"
    "    vec2 base = tc - razon * 0.5 + paso * 0.5;\n"
    "    c = vec3(0.0);\n"
    "    for (int y = 0; y < ny; y++) for (int x = 0; x < nx; x++)\n"
    "      c += tap(base + vec2(float(x), float(y)) * paso);\n"
    "    c /= float(nx * ny);\n"
    "  } else {\n"
    "    vec2 p = tc - 0.5; vec2 f = fract(p); vec2 b = floor(p);\n"
    "    c = vec3(0.0); float suma = 0.0;\n"
    "    for (int y = -1; y <= 2; y++) for (int x = -1; x <= 2; x++) {\n"
    "      float w = cr(float(x) - f.x) * cr(float(y) - f.y);\n"
    "      c += tap(b + vec2(float(x), float(y)) + 0.5) * w; suma += w; }\n"
    "    c /= suma;\n"
    "  }\n"
    "  if (nitidez > 0.0) {\n"
    "    vec2 d = max(razon, vec2(1.0));\n"
    "    vec3 n = (tap(tc + vec2(d.x, 0.0)) + tap(tc - vec2(d.x, 0.0))\n"
    "            + tap(tc + vec2(0.0, d.y)) + tap(tc - vec2(0.0, d.y))) * 0.25;\n"
    "    c = clamp(c + (c - n) * nitidez, 0.0, 1.0);\n"
    "  }\n"
    "  if (velo > 0.0) {\n"
    "    vec2 d = vec2(esc_uv.x * tam_tex.x * 0.0025);\n"
    "    vec3 b = vec3(0.0);\n"
    "    for (int y = -4; y <= 4; y++) for (int x = -4; x <= 4; x++) b += tap(tc + vec2(float(x), float(y)) * d);\n"
    "    c = mix(c, b / 81.0 * 0.4, velo);\n"
    "  }\n"
    "  salida = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
    "}\n";

static bool gl_recursos() {
    prog_gl = gl_program(VS_PRES, FS_PRES);
    if (!prog_gl) return false;
    glGenVertexArrays(1, &vao_gl);
    glGenTextures(1, &tex_gl);
    glBindTexture(GL_TEXTURE_2D, tex_gl);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtro_gl());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtro_gl());
    glUseProgram(prog_gl);
    glUniform1i(glGetUniformLocation(prog_gl, "lienzo"), 0);
    u_escuv = glGetUniformLocation(prog_gl, "esc_uv");
    u_tamtex = glGetUniformLocation(prog_gl, "tam_tex");
    u_razon = glGetUniformLocation(prog_gl, "razon");
    u_nitidez = glGetUniformLocation(prog_gl, "nitidez");
    u_modo = glGetUniformLocation(prog_gl, "modo");
    u_tramo = glGetUniformLocation(prog_gl, "tramo");
    u_velo = glGetUniformLocation(prog_gl, "velo");
    glUniform1f(u_velo, 0.0f);
    glUniform2f(u_escuv, 1.0f, 1.0f);
    glUniform2f(u_tramo, 0.0f, 1.0f);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    return true;
}

static void osk_dibujar(int dx, int dy, int dw, int dh);
static void ov_draw(int cw, int ch);
static int busy_now();
static void busy_dibujar(int dx, int dy, int dw, int dh);

// TTT_LATENCY=1: tiempos de la ruta mando -> pantalla; resumen cada 300 cuadros y la secuencia de 4 cuadros
static int lat_on = -1;
static double lat_muestra = 0, lat_pad = -1, lat_fb = -1, lat_fb_pad = -1, lat_espera = -1;
static bool lat_pend = false;
static int lat_lect = 0, lat_cuadros = 0, lat_total = 0;
static double lat_s[7], lat_m[7];
static int lat_n[7];

static double lat_ms() {
    static LARGE_INTEGER f = {};
    LARGE_INTEGER t;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)f.QuadPart;
}

static bool lat_activa() {
    if (lat_on < 0) { const char* e = getenv("TTT_LATENCY"); lat_on = e ? atoi(e) : 0; }
    return lat_on > 0;
}

static void lat_sumar(int i, double v) { lat_s[i] += v; if (v > lat_m[i]) lat_m[i] = v; lat_n[i]++; }

static void lat_traza(const char* que, int hilo) {
    if (lat_total >= 600 && lat_total < 604) printf("latency: cuadro %d %10.2f ms %s (hilo %d)\n", lat_total, lat_ms(), que, hilo);
}

// tipo 0 lectura del mando, 1 SetFrameBuf, 2 inicio de la espera de vblank, 3 fin de la espera, 4 retraso de lectura (hilo = us)
void psp_window_lat(int tipo, int hilo) {
    if (!lat_activa()) return;
    double t = lat_ms();
    if (tipo == 0) { lat_traza("lectura del mando", hilo); lat_lect++; if (lat_pad < 0) { lat_pad = t; lat_sumar(0, t - lat_muestra); } }
    else if (tipo == 1) { lat_traza("SetFrameBuf", hilo); if (lat_pad >= 0) lat_sumar(1, t - lat_pad); lat_fb = t; lat_fb_pad = lat_pad; lat_pad = -1; lat_pend = true; lat_total++; }
    else if (tipo == 2) { lat_traza("espera de vblank, inicio", hilo); lat_espera = t; }
    else if (tipo == 3) { lat_traza("espera de vblank, fin", hilo); if (lat_espera >= 0) lat_sumar(5, t - lat_espera); lat_espera = -1; }
    else if (tipo == 4) { lat_traza("retraso de lectura", hilo); lat_sumar(6, hilo / 1000.0); }
}

static void lat_swap(bool antes) {
    if (lat_on <= 0 || !lat_pend) return;
    double t = lat_ms();
    if (antes) { lat_traza("pintar", -1); lat_sumar(2, t - lat_fb); return; }
    lat_traza("SwapBuffers", -1);
    lat_sumar(3, t - lat_fb);
    if (lat_fb_pad >= 0) lat_sumar(4, t - lat_fb_pad);
    lat_pend = false;
    if (++lat_cuadros < 300) return;
    static const char* nom[7] = {"muestra->lectura", "lectura->SetFrameBuf", "SetFrameBuf->pintar", "SetFrameBuf->Swap", "lectura->Swap", "espera vblank", "retraso de lectura"};
    printf("latency: %d cuadros, %.1f lecturas por cuadro", lat_cuadros, (double)lat_lect / lat_cuadros);
    for (int i = 0; i < 7; i++) printf(" | %s %.2f/%.2f", nom[i], lat_n[i] ? lat_s[i] / lat_n[i] : 0.0, lat_m[i]);
    printf(" ms\n");
    fflush(stdout);
    lat_cuadros = lat_lect = 0;
    memset(lat_s, 0, sizeof(lat_s)); memset(lat_m, 0, sizeof(lat_m)); memset(lat_n, 0, sizeof(lat_n));
}

// two screens (local multiplayer, 2 views): L1+R1+L3 (LB+RB+LS) on any controller or Ctrl+F11 sends player 2's view to a second monitor
void psp_gpu_dual(int on);
int psp_gpu_split2();
static HWND dual_w = nullptr;
static HDC dual_dc = nullptr;
static int dual_cw = 1, dual_ch = 1;
static unsigned dual_ms = 0;
static bool dual_fs = false;
static GLuint tex_dual = 0;
static void notice_show();
static void alternar_completa();

static void dibujar_tramo(int cw, int ch, GLuint fuente, float t0, float t1, bool osk) {
    int dx, dy, dw, dh;
    destino_en(cw, ch, &dx, &dy, &dw, &dh);
    if (dw < 1 || dh < 1) return;
    glViewport(0, 0, cw, ch);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(dx, ch - dy - dh, dw, dh);
    glUseProgram(prog_gl);
    float su = tex_ext ? ext_su : 1.0f, sv = tex_ext ? ext_sv : 1.0f;
    float tw = tex_ext ? (float)ext_tw : (float)tex_w, th = tex_ext ? (float)ext_th : (float)tex_h;
    glUniform2f(u_escuv, su, sv);
    glUniform2f(u_tamtex, tw, th);
    glUniform2f(u_tramo, t0, t1);
    glUniform2f(u_razon, su * tw * t1 / (float)dw, sv * th / (float)dh);
    glUniform1f(u_nitidez, sharpness());
    glUniform1i(u_modo, filtro_modo());
    int busy = osk ? busy_now() : 0;
    glUniform1f(u_velo, busy ? 1.0f : 0.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fuente);
    if (tex_ext) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtro_gl());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtro_gl());
    }
    glBindVertexArray(vao_gl);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (busy) busy_dibujar(dx, dy, dw, dh);
    if (osk) osk_dibujar(dx, dy, dw, dh);
    if (osk) ov_draw(cw, ch);
}

static void dual_close(const char* why) {
    if (!dual_w) return;
    psp_gpu_dual(0);
    ReleaseDC(dual_w, dual_dc);
    DestroyWindow(dual_w);
    dual_w = nullptr; dual_dc = nullptr;
    if (dual_fs && completa) alternar_completa();
    dual_fs = false;
    printf("ventana: segunda pantalla cerrada (%s)\n", why);
    fflush(stdout);
}

static void pintar_gl() {
    lat_swap(true);
    int dx, dy, dw, dh;
    destino(&dx, &dy, &dw, &dh);
    if (dw < 1 || dh < 1) return;
    GLuint fuente = tex_ext;
    if (fuente && mid_show && tex_mid) fuente = tex_mid;
    if (!fuente) {
        fuente = tex_gl;
        glBindTexture(GL_TEXTURE_2D, tex_gl);
        if (lien_w != tex_w || lien_h != tex_h) {
            tex_w = lien_w; tex_h = lien_h;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_w, tex_h, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtro_gl());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtro_gl());
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_w, tex_h, GL_BGRA, GL_UNSIGNED_BYTE, pixeles);
    }
    if (valla_ext) { glWaitSync(valla_ext, 0, GL_TIMEOUT_IGNORED); glDeleteSync(valla_ext); valla_ext = nullptr; }
    bool dividida = dual_w && psp_gpu_split2();
    GLuint seg = dual_w && tex_ext ? tex_dual : 0;
    bool mitad = dividida && !seg;
    if (dual_w) {
        if (dividida) dual_ms = GetTickCount();
        else if (GetTickCount() - dual_ms > 20000) dual_close("20 s sin pantalla dividida");
    }
    dibujar_tramo(cli_w, cli_h, fuente, 0.0f, mitad ? 0.5f : 1.0f, true);
    { PERF(PZ_SWAP); gl_swap(); }
    if (dual_w && gl_activate(dual_dc, gl_rc_thread)) {
        dibujar_tramo(dual_cw, dual_ch, seg ? seg : fuente, mitad ? 0.5f : 0.0f, mitad ? 0.5f : 1.0f, false);
        SwapBuffers(dual_dc);
        gl_activate(gl_dc, gl_rc_thread);
    }
    lat_swap(false);
}

static bool cur_hidden = false;

static LRESULT CALLBACK dual_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SETCURSOR && LOWORD(l) == HTCLIENT && cur_hidden) { SetCursor(nullptr); return TRUE; }
    if (m == WM_SIZE) { dual_cw = LOWORD(l) > 0 ? LOWORD(l) : 1; dual_ch = HIWORD(l) > 0 ? HIWORD(l) : 1; return 0; }
    if (m == WM_CLOSE) return 0;
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (m == WM_PAINT) { ValidateRect(h, nullptr); return 0; }
    return DefWindowProc(h, m, w, l);
}

struct DualMon { HMONITOR main; RECT r; bool found; };
static BOOL CALLBACK dual_enum(HMONITOR m, HDC, LPRECT r, LPARAM p) {
    DualMon* d = (DualMon*)p;
    if (m != d->main && !d->found) { d->r = *r; d->found = true; }
    return TRUE;
}

static void dual_open() {
    if (!usar_gl) { printf("ventana: la segunda pantalla necesita la presentacion por GL\n"); return; }
    DualMon d = {MonitorFromWindow(ventana, MONITOR_DEFAULTTONEAREST), {0, 0, 0, 0}, false};
    EnumDisplayMonitors(nullptr, nullptr, dual_enum, (LPARAM)&d);
    const char* e = getenv("TTT_DUAL_TEST");
    bool test = !d.found && e && *e == '1';
    if (!d.found && !test) { printf("ventana: no hay segunda pantalla, la vista del jugador 2 sigue en esta\n"); fflush(stdout); return; }
    static bool reg = false;
    if (!reg) {
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = dual_proc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "TTTRecompDual";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassA(&wc);
        reg = true;
    }
    RECT r = d.r;
    DWORD st = WS_POPUP;
    if (test) { st = WS_OVERLAPPEDWINDOW; r = {0, 0, cli_w, cli_h}; AdjustWindowRect(&r, st, FALSE); OffsetRect(&r, 40 - r.left, 40 - r.top); }
    dual_w = CreateWindowExA(WS_EX_NOACTIVATE | (test ? 0 : WS_EX_TOOLWINDOW), "TTTRecompDual", "DBZ Tenkaichi Tag Team - Jugador 2", st,
                             r.left, r.top, r.right - r.left, r.bottom - r.top, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!dual_w) { printf("ventana: no pude crear la segunda ventana (error %lu)\n", (unsigned long)GetLastError()); return; }
    dual_dc = GetDC(dual_w);
    PIXELFORMATDESCRIPTOR pfd;
    int pf = GetPixelFormat(gl_dc);
    if (!pf || !DescribePixelFormat(gl_dc, pf, sizeof(pfd), &pfd) || !SetPixelFormat(dual_dc, pf, &pfd)) {
        printf("ventana: la segunda ventana no acepta el formato de pixel (error %lu)\n", (unsigned long)GetLastError());
        ReleaseDC(dual_w, dual_dc); DestroyWindow(dual_w); dual_w = nullptr; dual_dc = nullptr;
        return;
    }
    RECT c;
    GetClientRect(dual_w, &c);
    dual_cw = c.right > 0 ? c.right : 1; dual_ch = c.bottom > 0 ? c.bottom : 1;
    ShowWindow(dual_w, SW_SHOWNOACTIVATE);
    dual_fs = !completa && !test;
    if (dual_fs) alternar_completa();
    psp_gpu_dual(1);
    dual_ms = GetTickCount();
    printf("ventana: segunda pantalla abierta %dx%d%s, vista del jugador 2 en ella\n", dual_cw, dual_ch, test ? " (prueba en el mismo monitor)" : "");
    fflush(stdout);
}

static void alternar_completa() {
    if (!ventana) return;
    if (!completa) {
        GetWindowRect(ventana, &antes_completa);
        estilo_antes = GetWindowLong(ventana, GWL_STYLE);
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfo(MonitorFromWindow(ventana, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLong(ventana, GWL_STYLE, (estilo_antes & ~WS_OVERLAPPEDWINDOW) | WS_POPUP);
        // a window that covers the monitor exactly is promoted by the driver to direct flip and capture tools (OBS) lose it; TTT_FS_CAPTURE=0 keeps the exact size
        static int cap = -1;
        if (cap < 0) { const char* e = getenv("TTT_FS_CAPTURE"); cap = !(e && *e == '0'); }
        int fw = mi.rcMonitor.right - mi.rcMonitor.left, fh = mi.rcMonitor.bottom - mi.rcMonitor.top + (cap ? 1 : 0);
        SetWindowPos(ventana, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, fw, fh, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        printf("ventana: pantalla completa %dx%d%s\n", fw, fh, cap ? " (1 px extra para captura)" : "");
        fflush(stdout);
        completa = true;
    } else {
        SetWindowLong(ventana, GWL_STYLE, estilo_antes);
        SetWindowPos(ventana, HWND_TOP, antes_completa.left, antes_completa.top,
                     antes_completa.right - antes_completa.left, antes_completa.bottom - antes_completa.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        completa = false;
    }
}

// teclado en pantalla dibujado dentro del juego: panel azul al estilo de los dialogos
// del juego, texto por GDI subido como textura y compuesto sobre el cuadro presentado
static volatile int osk_activo = 0;
static char osk_texto[64] = "";
static int osk_tope = 31, osk_res = 0;
static char osk_titulo[64] = "";
static GLuint tex_osk = 0, prog_osk = 0;
static int osk_w = 0, osk_h = 0;
static char osk_dibujado[80] = "";
static uint32_t* osk_bits = nullptr;
static unsigned osk_cerrado_ms = 0;
static bool osk_reciente() { return osk_activo || (GetTickCount() - osk_cerrado_ms) < 400; }

static volatile int txt_cap = 0;
static volatile unsigned txt_q[32];
static volatile unsigned txt_h = 0, txt_t = 0;

void psp_window_text_capture(int on) {
    if (!on && txt_cap) osk_cerrado_ms = GetTickCount();
    txt_cap = on; txt_h = txt_t = 0;
}

int psp_window_text_pop() {
    if (txt_h == txt_t) return 0;
    unsigned c = txt_q[txt_h % 32];
    txt_h = txt_h + 1;
    return (int)c;
}

static const char* FS_OSK =
    "#version 330 core\n"
    "in vec2 uv;\n"
    "uniform sampler2D panel;\n"
    "out vec4 salida;\n"
    "void main(){ salida = texture(panel, uv); }\n";

void psp_window_osk_open(const char* titulo, const char* inicial, int tope) {
    snprintf(osk_titulo, sizeof(osk_titulo), "%s", titulo ? titulo : "");
    snprintf(osk_texto, sizeof(osk_texto), "%s", inicial ? inicial : "");
    osk_tope = tope > 0 && tope < 63 ? tope : 31;
    if ((int)strlen(osk_texto) > osk_tope) osk_texto[osk_tope] = 0;
    osk_res = 0;
    osk_dibujado[0] = 0;
    osk_activo = 1;
    watchdog_pause.store(1);
}

// 0 sigue abierto, 1 aceptado, 2 cancelado
int psp_window_osk_state(char* salida, int tope) {
    if (osk_activo) return 0;
    if (salida) snprintf(salida, (size_t)tope, "%s", osk_texto);
    return osk_res;
}

static void osk_tecla(unsigned c) {
    if (!osk_activo) return;
    if (c == 13 || c == 27) { osk_res = c == 13 ? 1 : 2; osk_activo = 0; osk_cerrado_ms = GetTickCount(); watchdog_pause.store(0); return; }
    size_t n = strlen(osk_texto);
    if (c == 8) { if (n) osk_texto[n - 1] = 0; return; }
    if (c >= 32 && c < 127 && (int)n < osk_tope) { osk_texto[n] = (char)c; osk_texto[n + 1] = 0; }
}

static void osk_pintar_panel(int w, int h, bool cursor) {
    if (w != osk_w || h != osk_h) {
        free(osk_bits);
        osk_bits = (uint32_t*)malloc((size_t)w * h * 4);
        osk_w = w; osk_h = h;
    }
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* px = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &px, nullptr, 0);
    HGDIOBJ viejo = SelectObject(dc, bm);
    RECT r = {0, 0, w, h};
    HBRUSH fondo = CreateSolidBrush(RGB(24, 48, 140));
    HBRUSH borde = CreateSolidBrush(RGB(70, 110, 220));
    FillRect(dc, &r, borde);
    RECT ri = {3, 3, w - 3, h - 3};
    FillRect(dc, &ri, fondo);
    int corte = h / 9;
    POINT tri[3] = {{0, 0}, {corte, 0}, {0, corte}};
    HBRUSH negro = CreateSolidBrush(RGB(0, 0, 0));
    HGDIOBJ vb = SelectObject(dc, negro);
    HGDIOBJ vp = SelectObject(dc, GetStockObject(NULL_PEN));
    Polygon(dc, tri, 3);
    SelectObject(dc, vp);
    SelectObject(dc, vb);
    SetBkMode(dc, TRANSPARENT);
    int alto1 = h / 6, alto2 = h / 4;
    HFONT f1 = CreateFontA(-alto1, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Trebuchet MS");
    HFONT f2 = CreateFontA(-alto2, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Trebuchet MS");
    HFONT f3 = CreateFontA(-(alto1 * 3 / 4), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Trebuchet MS");
    char linea[80];
    snprintf(linea, sizeof(linea), "%s%s", osk_texto, cursor ? "_" : " ");
    RECT rt = {0, h / 9, w, h / 9 + alto1 + 4};
    RECT rx = {0, h * 2 / 5, w, h * 2 / 5 + alto2 + 6};
    RECT rh = {0, h - alto1 - h / 10, w, h - h / 10};
    HGDIOBJ vf = SelectObject(dc, f1);
    SetTextColor(dc, RGB(0, 0, 0));
    OffsetRect(&rt, 2, 2); DrawTextA(dc, osk_titulo, -1, &rt, DT_CENTER | DT_SINGLELINE | DT_NOCLIP); OffsetRect(&rt, -2, -2);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextA(dc, osk_titulo, -1, &rt, DT_CENTER | DT_SINGLELINE | DT_NOCLIP);
    SelectObject(dc, f2);
    SetTextColor(dc, RGB(0, 0, 0));
    OffsetRect(&rx, 3, 3); DrawTextA(dc, linea, -1, &rx, DT_CENTER | DT_SINGLELINE | DT_NOCLIP); OffsetRect(&rx, -3, -3);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextA(dc, linea, -1, &rx, DT_CENTER | DT_SINGLELINE | DT_NOCLIP);
    SelectObject(dc, f3);
    SetTextColor(dc, RGB(200, 200, 200));
    DrawTextA(dc, "Enter acepta      Esc cancela", -1, &rh, DT_CENTER | DT_SINGLELINE | DT_NOCLIP);
    SelectObject(dc, vf);
    DeleteObject(f1); DeleteObject(f2); DeleteObject(f3);
    DeleteObject(fondo); DeleteObject(borde); DeleteObject(negro);
    const uint32_t* src = (const uint32_t*)px;
    for (int i = 0; i < w * h; i++) {
        uint32_t v = src[i];
        int x = i % w, y = i / w;
        bool fuera = (x + y) < corte;
        osk_bits[i] = fuera ? 0u : (v | 0xFF000000u);
    }
    SelectObject(dc, viejo);
    DeleteObject(bm);
    DeleteDC(dc);
}

static void osk_dibujar(int dx, int dy, int dw, int dh) {
    if (!osk_activo) return;
    if (!prog_osk) {
        prog_osk = gl_program(VS_PRES, FS_OSK);
        glGenTextures(1, &tex_osk);
        glBindTexture(GL_TEXTURE_2D, tex_osk);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    if (!prog_osk) return;
    int pw = dw * 3 / 5, ph = dh * 9 / 40;
    if (pw < 320) pw = 320;
    if (ph < 100) ph = 100;
    bool cursor = ((GetTickCount() / 450) & 1) == 0;
    char clave[80];
    snprintf(clave, sizeof(clave), "%s|%d|%dx%d", osk_texto, cursor ? 1 : 0, pw, ph);
    if (strcmp(clave, osk_dibujado)) {
        snprintf(osk_dibujado, sizeof(osk_dibujado), "%s", clave);
        osk_pintar_panel(pw, ph, cursor);
        glBindTexture(GL_TEXTURE_2D, tex_osk);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pw, ph, 0, GL_BGRA, GL_UNSIGNED_BYTE, osk_bits);
    }
    int px = dx + (dw - pw) / 2, py = dy + (dh - ph) / 2;
    glViewport(px, cli_h - py - ph, pw, ph);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(prog_osk);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_osk);
    glBindVertexArray(vao_gl);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_BLEND);
}

// renders being captured (characters 1, maps 2): the frame blurred and a progress panel, on the window only (never in the captures)
int psp_window_language();
static std::atomic<int> busy_kind{0}, busy_done{0}, busy_total{0};
static std::atomic<unsigned> busy_ms{0};
void psp_window_busy(int kind, int done, int total) { busy_done = done; busy_total = total; busy_ms = GetTickCount(); busy_kind = kind; }
static int busy_now() { int k = busy_kind.load(); return k && GetTickCount() - busy_ms.load() < 1500 ? k : 0; }
static GLuint tex_busy = 0;
static std::vector<uint32_t> busy_bits;
static std::string busy_hecho;
static void busy_pintar(int w, int h, int kind, int done, int total, bool es) {
    busy_bits.assign((size_t)w * h, 0);
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void* px = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &px, nullptr, 0);
    HGDIOBJ viejo = SelectObject(dc, bm);
    RECT r = {0, 0, w, h}, ri = {3, 3, w - 3, h - 3};
    HBRUSH borde = CreateSolidBrush(RGB(70, 110, 220)), fondo = CreateSolidBrush(RGB(24, 48, 140)), barra = CreateSolidBrush(RGB(12, 24, 70)), lleno = CreateSolidBrush(RGB(255, 140, 0));
    FillRect(dc, &r, borde);
    FillRect(dc, &ri, fondo);
    int bx = w / 10, bw = w - 2 * bx, by = h * 58 / 100, bh = std::max(8, h / 11);
    RECT rb = {bx, by, bx + bw, by + bh};
    FillRect(dc, &rb, barra);
    RECT rl = {bx, by, bx + (total > 0 ? bw * std::min(done, total) / total : 0), by + bh};
    FillRect(dc, &rl, lleno);
    SetBkMode(dc, TRANSPARENT);
    int a1 = h / 6, a2 = h / 9;
    HFONT f1 = CreateFontA(-a1, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Trebuchet MS");
    HFONT f2 = CreateFontA(-a2, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Trebuchet MS");
    const char* t1 = kind == 2 ? (es ? "Generando renders de mapas" : "Generating map renders") : (es ? "Generando renders de personajes" : "Generating character renders");
    const char* t3 = es ? "Espera, no presiones nada hasta que termine" : "Please wait, do not press anything until it finishes";
    char t2[48];
    snprintf(t2, sizeof(t2), "%d / %d", std::min(done + 1, std::max(total, 1)), std::max(total, 1));
    RECT r1 = {0, h * 12 / 100, w, h * 12 / 100 + a1 + 4}, r2 = {0, h * 38 / 100, w, h * 38 / 100 + a2 + 4}, r3 = {0, h * 76 / 100, w, h * 76 / 100 + a2 + 4};
    HGDIOBJ vf = SelectObject(dc, f1);
    SetTextColor(dc, RGB(0, 0, 0));
    OffsetRect(&r1, 2, 2); DrawTextA(dc, t1, -1, &r1, DT_CENTER | DT_SINGLELINE | DT_NOCLIP); OffsetRect(&r1, -2, -2);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextA(dc, t1, -1, &r1, DT_CENTER | DT_SINGLELINE | DT_NOCLIP);
    SelectObject(dc, f2);
    DrawTextA(dc, t2, -1, &r2, DT_CENTER | DT_SINGLELINE | DT_NOCLIP);
    SetTextColor(dc, RGB(200, 200, 200));
    DrawTextA(dc, t3, -1, &r3, DT_CENTER | DT_SINGLELINE | DT_NOCLIP);
    SelectObject(dc, vf);
    DeleteObject(f1); DeleteObject(f2);
    DeleteObject(borde); DeleteObject(fondo); DeleteObject(barra); DeleteObject(lleno);
    const uint32_t* src = (const uint32_t*)px;
    for (int i = 0; i < w * h; i++) busy_bits[i] = src[i] | 0xFF000000u;
    SelectObject(dc, viejo);
    DeleteObject(bm);
    DeleteDC(dc);
}
static void busy_dibujar(int dx, int dy, int dw, int dh) {
    if (!prog_osk) prog_osk = gl_program(VS_PRES, FS_OSK);
    if (!prog_osk) return;
    if (!tex_busy) {
        glGenTextures(1, &tex_busy);
        glBindTexture(GL_TEXTURE_2D, tex_busy);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    int pw = std::max(360, dw / 2), ph = std::max(120, dh / 5);
    int kind = busy_kind.load(), done = busy_done.load(), total = busy_total.load();
    bool es = psp_window_language() == 3;
    char clave[96];
    snprintf(clave, sizeof(clave), "%d|%d|%d|%dx%d|%d", kind, done, total, pw, ph, (int)es);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_busy);
    if (busy_hecho != clave) {
        busy_hecho = clave;
        busy_pintar(pw, ph, kind, done, total, es);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pw, ph, 0, GL_BGRA, GL_UNSIGNED_BYTE, busy_bits.data());
    }
    int px = dx + (dw - pw) / 2, py = dy + (dh - ph) / 2;
    glViewport(px, cli_h - py - ph, pw, ph);
    glUseProgram(prog_osk);
    glBindVertexArray(vao_gl);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glUseProgram(prog_gl);
}

#include <atomic>
extern std::atomic<int> psp_dbg_ko;
extern std::atomic<int> psp_dbg_freeze;
extern std::atomic<int> psp_dbg_planet;
#include "input/input.h"
void psp_disc_reload_request();
void psp_gpu_vtx_toggle();
void psp_gpu_copy_toggle();
void psp_gpu_olbox_toggle();
void psp_gpu_mt_toggle();
#include "input/rumble.h"
#include "input/icons.h"
#include "ui/menu.h"
#include "ui/overlay.h"
static bool txt_kb() { return txt_cap && input_mode == 0; }

static bool enfocada();
// item 56: cursor hidden after TTT_HIDE_CURSOR seconds idle (0 = never), always in battle except its pause, at once on a controller press
static void cursor_update() {
    static int secs = 3;
    static DWORD env_ms = 0, move_ms = 0;
    static LONG pad_seen = 0;
    static bool by_pad = false;
    static POINT last = {-100000, -100000};
    DWORD now = GetTickCount();
    if (now - env_ms > 500) { env_ms = now; const char* e = getenv("TTT_HIDE_CURSOR"); secs = e && *e ? atoi(e) : 3; }
    POINT p;
    bool moved = GetCursorPos(&p) && (p.x != last.x || p.y != last.y);
    if (moved) last = p;
    bool btn = ((GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON) | GetAsyncKeyState(VK_MBUTTON)) & 0x8000) != 0;
    if (moved || btn || !move_ms) { move_ms = now; by_pad = false; }
    LONG pn = pad_press_n;
    if (pn != pad_seen) { pad_seen = pn; by_pad = true; }
    int ctx = ctx_last;
    int why = 0;
    if (secs > 0 && enfocada() && IsWindowEnabled(ventana) && pause_n == 0 && !osk_activo && !ov_open) {
        if (ctx == 1 || ctx == 2) why = 1;
        else if (by_pad) why = 2;
        else if (now - move_ms >= (DWORD)secs * 1000u) why = 3;
    }
    bool hide = why != 0;
    if (hide == cur_hidden) return;
    cur_hidden = hide;
    static int logs = 0;
    static const char* names[4] = {"visible", "oculto en combate", "oculto por el mando", "oculto por inactividad"};
    if (logs++ < 40) { printf("cursor: %s\n", names[why]); fflush(stdout); }
    if (cursor_in(ventana)) SetCursor(hide ? nullptr : LoadCursor(nullptr, IDC_ARROW));
}

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SETCURSOR && LOWORD(l) == HTCLIENT && cur_hidden) { SetCursor(nullptr); return TRUE; }
    if (osk_activo && m == WM_CHAR) { osk_tecla((unsigned)w); return 0; }
    if (txt_cap && m == WM_CHAR) {
        if (input_mode == 0 && (w == 8 || w == 13 || w == 27 || (w >= 32 && w < 127)) && txt_t - txt_h < 32) { txt_q[txt_t % 32] = (unsigned)w; txt_t = txt_t + 1; }
        return 0;
    }
    if (osk_activo && m == WM_KEYDOWN && w == VK_ESCAPE) { osk_tecla(27); return 0; }
    { LRESULT r; if (menu_proc(h, m, w, l, &r)) return r; }
    if (m == WM_DESTROY || m == WM_CLOSE) { input_shortcuts_restore(); abierta = false; PostQuitMessage(0); return 0; }
    {   // title bar drag moved by hand: the Windows move loop would stop the game while the button is held (TTT_DRAG_FAST=0: old)
        static int fast = -1; static bool drag = false; static POINT p0; static RECT r0;
        if (fast < 0) { const char* e = getenv("TTT_DRAG_FAST"); fast = e && *e ? atoi(e) : 1; }
        if (fast && m == WM_NCLBUTTONDOWN && w == HTCAPTION && !IsZoomed(h)) {
            SetForegroundWindow(h); GetCursorPos(&p0); GetWindowRect(h, &r0); SetCapture(h); drag = true; return 0;
        }
        if (drag && m == WM_MOUSEMOVE) {
            POINT p; GetCursorPos(&p);
            SetWindowPos(h, nullptr, r0.left + p.x - p0.x, r0.top + p.y - p0.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        if (drag && (m == WM_LBUTTONUP || m == WM_CAPTURECHANGED)) { drag = false; if (m == WM_LBUTTONUP) ReleaseCapture(); return 0; }
    }
    if (m == WM_SIZE) {
        cli_w = LOWORD(l); cli_h = HIWORD(l);
        if (cli_w < 1) cli_w = 1;
        if (cli_h < 1) cli_h = 1;
        limpiar_fondo = true;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    if (m == WM_GETMINMAXINFO) {
        MINMAXINFO* mm = (MINMAXINFO*)l;
        mm->ptMinTrackSize.x = 320; mm->ptMinTrackSize.y = 200;
        return 0;
    }
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_SYSKEYDOWN && w == VK_RETURN) { fullscreen_switch(); return 0; }
    if (m == WM_PAINT) {
        if (usar_gl) { ValidateRect(h, nullptr); pintar_gl(); return 0; }
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        pintar_en(dc);
        EndPaint(h, &ps);
        return 0;
    }
    return DefWindowProc(h, m, w, l);
}

static uint8_t stick_x = 128, stick_y = 128;

static volatile bool informe_pedido = false;
static volatile bool captura_pedida = false;
static volatile int depurar_var = 0;
static volatile int aniso_idx = 2;
static volatile int sesgo_idx = 0;
static volatile bool volcar_pedido = false;
static volatile bool escena_pedida = false;
static volatile bool lod_conmuta = false;
static volatile bool reloj_pedido = false;
static volatile bool ritmo_pedido = false;
static volatile bool pos_pedida = false;

static bool enfocada() {
    HWND f = GetForegroundWindow();
    return f == ventana || (f && GetWindow(f, GW_OWNER) == ventana);
}

// muestreo de mandos y teclado; el HLE de sceCtrl lo llama en el momento en que el juego lee
static unsigned game_sample_ms = 0;
void psp_window_sample() {
    if (!ventana) return;
    game_sample_ms = GetTickCount();
    input_read(&botones, &stick_x, &stick_y, enfocada());
    if (ov_mask_on()) { botones = 0; stick_x = stick_y = 128; ov_mask_local(); }
    lat_muestra = lat_ms();
}

extern bool psp_st_mark;
extern int psp_windows_on;

static void leer_teclado() {
    bool foco = enfocada();
    if (GetTickCount() - game_sample_ms > 250) { input_read(&botones, &stick_x, &stick_y, foco); lat_muestra = lat_ms(); }
    if (ov_open && ov_net) { ov_step(); botones = 0; stick_x = stick_y = 128; ov_mask_local(); return; }
    if (ov_mask_on()) { botones = 0; stick_x = stick_y = 128; ov_mask_local(); }
    static bool ov_combo_was = true;
    bool ov_c = ov_combo();
    if (ov_c && !ov_combo_was && foco) { ov_combo_was = true; ov_run(); return; }
    if (ov_pend && !ov_open) { ov_run(); return; }
    ov_combo_was = ov_c;
    if (!foco) return;
    // F1 menu, F11 pantalla completa, Shift+Esc salir; el resto de teclas de funcion solo con TTT_DEBUG=1
    rumble_update();
    static const bool dbg_keys = [] { const char* e = getenv("TTT_DEBUG"); return e && *e == '1'; }();
    static bool esc_antes = true;
    bool esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    if (esc && !esc_antes && (GetAsyncKeyState(VK_SHIFT) & 0x8000) && !osk_reciente() && !txt_kb() && GetTickCount() > esc_free_ms) abierta = false;
    esc_antes = esc;
    if (dbg_keys && (GetAsyncKeyState(VK_F12) & 0x8000)) informe_pedido = true;
    if (dbg_keys && (GetAsyncKeyState(VK_F9) & 0x8000)) captura_pedida = true;
    static bool f11_antes = false;
    bool f11 = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    if (f11 && !f11_antes && !(GetAsyncKeyState(VK_CONTROL) & 0x8000)) fullscreen_switch();
    f11_antes = f11;
    static bool f10_antes = false;
    bool f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    if (dbg_keys && f10 && !f10_antes) { vista_var = (modo_vista() + 1) % 3; limpiar_fondo = true; settings_save(); }
    f10_antes = f10;
    static bool ins_antes = false;
    bool ins = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (dbg_keys && ins && !ins_antes) psp_gpu_vtx_toggle();
    ins_antes = ins;
    static bool del_antes = false, pau_antes = false;
    bool delk = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0, pauk = (GetAsyncKeyState(VK_PAUSE) & 0x8000) != 0;
    if (dbg_keys && delk && !del_antes && GetForegroundWindow() == ventana) psp_gpu_copy_toggle();
    if (dbg_keys && pauk && !pau_antes && GetForegroundWindow() == ventana) psp_gpu_olbox_toggle();
    del_antes = delk; pau_antes = pauk;
    static bool scr_antes = false;
    bool scrk = (GetAsyncKeyState(VK_SCROLL) & 0x8000) != 0;
    if (dbg_keys && scrk && !scr_antes && GetForegroundWindow() == ventana) psp_gpu_mt_toggle();
    scr_antes = scrk;
    static bool f8_antes = false;
    bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    if (dbg_keys && f8 && !f8_antes) { depurar_var = !depurar_var; printf("vista de nivel de detalle: %d\n", depurar_var); fflush(stdout); }
    f8_antes = f8;
    static bool f7_antes = false;
    bool f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    if (dbg_keys && f7 && !f7_antes) {
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) || psp_windows_on > 0) { psp_st_mark = !psp_st_mark; printf("stepcensus: mark %s\n", psp_st_mark ? "on" : "off"); }
        else { aniso_idx = (aniso_idx + 1) % 3; printf("anisotropia: paso %d\n", (int)aniso_idx); }
        fflush(stdout);
    }
    f7_antes = f7;
    static bool f5_antes = false;
    bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (dbg_keys && f5 && !f5_antes) { volcar_pedido = true; printf("volcado de texturas pedido\n"); fflush(stdout); }
    f5_antes = f5;
    static bool f2_antes = false;
    bool f2 = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
    if (dbg_keys && f2 && !f2_antes) { if (GetAsyncKeyState(VK_CONTROL) & 0x8000) { pos_pedida = true; printf("busqueda de posicion pedida\n"); fflush(stdout); } else if (!ov_open && !psp_net_live()) ov_pend = [] { ov_tab_set(5); }; }
    f2_antes = f2;
    static bool f4_antes = false;
    bool f4 = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
    if (dbg_keys && f4 && !f4_antes) { ritmo_pedido = true; printf("busqueda de ritmo pedida\n"); fflush(stdout); }
    f4_antes = f4;
    static bool f3_antes = false;
    bool f3 = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
    if (dbg_keys && f3 && !f3_antes) { reloj_pedido = true; printf("busqueda de reloj pedida\n"); fflush(stdout); }
    f3_antes = f3;
    static bool f1_antes = false;
    bool f1 = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
    bool mayus = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (f1 && !f1_antes) { if (dbg_keys && mayus) lod_conmuta = true; else if (dbg_keys && ctrl) escena_pedida = true; else { f1_antes = true; ov_run(); return; } }
    f1_antes = f1;
    static bool pg_antes = false;
    bool pgu = (GetAsyncKeyState(VK_PRIOR) & 0x8000) != 0, pgd = (GetAsyncKeyState(VK_NEXT) & 0x8000) != 0;
    if (dbg_keys && (pgu || pgd) && !pg_antes && GetForegroundWindow() == ventana) psp_dbg_ko = pgd ? 2 : 1;
    pg_antes = pgu || pgd;
    static bool end_antes = false;
    bool endk = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
    if (dbg_keys && endk && !end_antes && GetForegroundWindow() == ventana) { psp_dbg_freeze = !psp_dbg_freeze.load(); printf("debug: oponentes %s (End)\n", psp_dbg_freeze.load() ? "quietos" : "con control"); fflush(stdout); }
    end_antes = endk;
    static bool home_antes = false;
    bool homek = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
    if (dbg_keys && homek && !home_antes && GetForegroundWindow() == ventana) psp_dbg_planet = 1;
    home_antes = homek;
    static bool f6_antes = false;
    bool f6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    if (dbg_keys && f6 && !f6_antes) { sesgo_idx = (sesgo_idx + 1) % 4; printf("sesgo de lod: paso %d\n", (int)sesgo_idx); fflush(stdout); }
    f6_antes = f6;
    static bool rl_antes = false;
    bool rl = ctrl && (GetAsyncKeyState('R') & 0x8000) != 0 && GetForegroundWindow() == ventana;
    if (dbg_keys && rl && !rl_antes) { psp_disc_reload_request(); printf("recarga: pedida (Ctrl+R)\n"); fflush(stdout); }
    rl_antes = rl;
    static bool dual_antes = false;
    bool combo = input_dual_combo() || (ctrl && f11);
    if (combo && !dual_antes) {
        if (dual_w) dual_close("combinacion");
        else if (psp_gpu_split2()) dual_open();
        else { printf("ventana: combinacion de dos pantallas sin pantalla dividida de 2 vistas, nada que separar\n"); fflush(stdout); }
    }
    dual_antes = combo;
}

void psp_boot_mark(const char* what);
bool psp_window_init(int esc) {
    escala = esc < 1 ? 1 : esc;
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC;                  // el contexto GL necesita un DC estable
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "TTTRecomp";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassA(&wc);
    int vw = 480 * escala, vh = 272 * escala;
    {   // TTT_WINDOW=ANCHOxALTO fija el tamano del area de dibujo
        const char* e = getenv("TTT_WINDOW");
        int a = 0, b = 0;
        if (!e || !*e) e = "1280x720";
        if (sscanf(e, "%dx%d", &a, &b) == 2 && a > 63 && b > 63) { vw = a; vh = b; }
    }
    {   // nunca mas grande que el area util del monitor
        RECT t = {0, 0, 0, 0};
        if (SystemParametersInfoA(SPI_GETWORKAREA, 0, &t, 0)) {
            int mw = t.right - t.left, mh = t.bottom - t.top - 40;
            if (vw > mw) { vh = (int)((int64_t)vh * mw / vw); vw = mw; }
            if (vh > mh) { vw = (int)((int64_t)vw * mh / vh); vh = mh; }
        }
    }
    RECT r = {0, 0, vw, vh};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    ventana = CreateWindowA("TTTRecomp", "DBZ Tenkaichi Tag Team", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                            nullptr, nullptr, wc.hInstance, nullptr);
    if (!ventana) return false;
    link_icons(ventana);
    DragAcceptFiles(ventana, TRUE);
    std::thread(link_register).detach();
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = lien_w;
    info.bmiHeader.biHeight = -lien_h;          // negativo: primera fila arriba
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    cli_w = vw; cli_h = vh;
    {   // TTT_GL=0 vuelve al blit por GDI
        const char* e = getenv("TTT_GL");
        if (!e || atoi(e) != 0) usar_gl = gl_init(ventana) && gl_recursos();
        if (!usar_gl) printf("gl: presentacion por GDI\n");
    }
    timeBeginPeriod(1);   // sin esto Sleep redondea a 15.6 ms y el ritmo se queda en 32 fps
    settings_load();
    ShowWindow(ventana, SW_SHOW);
    if (saved_fullscreen) alternar_completa();
    input_init();
    rumble_level = cfg.rumble;
    abierta = true;
    return true;
}

// loading wheel: 8 dots at the bottom right, drawn with scissored clears
static void wait_wheel(int w, int h) {
    int s = h / 90 < 3 ? 3 : h / 90, r = s * 3, cx = w - r - s * 5, cy = r + s * 5;
    unsigned p = (unsigned)(GetTickCount() / 100) % 8;
    glEnable(GL_SCISSOR_TEST);
    for (int i = 0; i < 8; i++) {
        float a = 1.5707963f - 6.2831853f * (float)i / 8.0f, v = 1.0f - 0.11f * (float)((p - (unsigned)i + 8) % 8);
        glScissor(cx + (int)(cosf(a) * (float)r) - s / 2, cy + (int)(sinf(a) * (float)r) - s / 2, s, s);
        glClearColor(v, v * 0.55f, v * 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glDisable(GL_SCISSOR_TEST);
}

// while the virtual disc is prepared in another thread: the window answers, black with the loading wheel
void psp_window_wait(volatile bool* done) {
    SetWindowTextA(ventana, "DBZ Tenkaichi Tag Team - Loading...");
    while (!*done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) _Exit(0);
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!abierta) _Exit(0);
        if (usar_gl) {
            RECT rc; GetClientRect(ventana, &rc);
            glViewport(0, 0, rc.right, rc.bottom);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            wait_wheel(rc.right, rc.bottom);
            gl_swap();
        }
        Sleep(15);
    }
    SetWindowTextA(ventana, "DBZ Tenkaichi Tag Team");
}

static volatile bool cuadro_nuevo = false;
static volatile int n_fps = 0;
static unsigned t_fps = 0;
static double fps_ultimo = 0;

static void refrescar_titulo() {
    unsigned ahora = (unsigned)GetTickCount();
    if (!t_fps) { t_fps = ahora; n_fps = 0; return; }
    unsigned dt = ahora - t_fps;
    if (dt < 500) return;
    fps_ultimo = n_fps * 1000.0 / (double)dt;
    t_fps = ahora; n_fps = 0;
    char tit[96];
    snprintf(tit, sizeof(tit), "DBZ Tenkaichi Tag Team - %.1f fps", fps_ultimo);
    SetWindowTextA(ventana, tit);
}

// se llama desde el hilo que creo la ventana (el planificador)
static bool gancho_toca() {
    static LARGE_INTEGER frec = {}, ult = {};
    if (!frec.QuadPart) QueryPerformanceFrequency(&frec);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    if (ult.QuadPart && (t.QuadPart - ult.QuadPart) * 1000 < frec.QuadPart) return false;
    ult = t;
    return true;
}

static void mid_begin() {
    double now = psp_window_clock();
    if (mid_prev > 0) { double d = now - mid_prev; if (d > 0.010 && d < 0.1) mid_per += (d - mid_per) * 0.2; }
    mid_prev = now;
    mid_pend = mid_mode && tex_ext && tex_mid && usar_gl;
    mid_show = mid_pend && mid_mode == 1;
    mid_due = now + mid_per * 0.5;
}

double psp_window_mid_due() { return mid_pend ? mid_due : 0.0; }

void psp_window_mid_paint() {
    if (!mid_pend) return;
    mid_pend = false;
    if (!ventana || !abierta || !usar_gl) return;
    double late = (psp_window_clock() - mid_due) * 1000.0;
    mid_n++; mid_lat += late; if (late > mid_lmax) mid_lmax = late;
    mid_show = !mid_show;
    pintar_gl();
}

void psp_window_mid_stats(int* n, double* avg, double* mx) {
    *n = mid_n; *avg = mid_n ? mid_lat / mid_n : 0.0; *mx = mid_lmax;
    mid_n = 0; mid_lat = mid_lmax = 0;
}

void psp_window_texture_mid(unsigned t, int mode) { tex_mid = (GLuint)t; mid_mode = t ? mode : 0; }

void psp_window_paint() {
    if (!ventana) return;
    PERF_N(PC_HOOK, 1);
    if (mid_pend && !cuadro_nuevo && psp_window_clock() >= mid_due) { psp_window_mid_paint(); return; }
    bool toca = cuadro_nuevo || gancho_toca();
    if (!toca) return;
    // slow hook (> 100 ms): which part took it
    struct HookProbe {
        double t[6] = {}; int k = 0, nmsg = 0; UINT slow_id = 0; double slow_ms = 0;
        void mark() { if (k < 6) t[k++] = psp_window_clock(); }
        ~HookProbe() {
            mark();
            double tot = (t[k - 1] - t[0]) * 1000.0;
            static int logs = 0;
            if (tot < 100.0 || logs >= 20) return;
            logs++;
            static const char* P[5] = {"mensajes", "aviso y editores", "teclado y cursor", "pintar", "titulo"};
            printf("ventana: gancho lento %.0f ms, cuadro %llu |", tot, (unsigned long long)perf_frames);
            for (int i = 1; i < k; i++) printf(" %s %.0f", P[i - 1], (t[i] - t[i - 1]) * 1000.0);
            printf(" | %d mensajes, el mas lento 0x%04X %.0f ms\n", nmsg, slow_id, slow_ms);
            fflush(stdout);
        }
    } hp;
    hp.mark();
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) { abierta = false; break; }
        double m0 = psp_window_clock();
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        double m1 = (psp_window_clock() - m0) * 1000.0;
        hp.nmsg++; if (m1 > hp.slow_ms) { hp.slow_ms = m1; hp.slow_id = msg.message; }
    }
    link_poll();
    hp.mark();
    if (!abierta) return;
    notice_show();
    if (restart_requested) { restart_requested = false; restart(); return; }
    if (psp_render_editor_take()) { render_editor(); if (!abierta) return; }
    if (psp_map_chooser_take()) { map_chooser(); if (!abierta) return; }
    hp.mark();
    leer_teclado();
    cursor_update();
    hp.mark();
    bool repite = false;
    if (repetir_n > 1 && !cuadro_nuevo && usar_gl && tex_ext) {
        if (!frec_rep.QuadPart) QueryPerformanceFrequency(&frec_rep);
        LARGE_INTEGER ahora;
        QueryPerformanceCounter(&ahora);
        double per = (double)frec_rep.QuadPart / (60.0 * repetir_n);
        if (t_rep.QuadPart && (double)(ahora.QuadPart - t_rep.QuadPart) >= per) { repite = true; t_rep = ahora; }
    }
    if (cuadro_nuevo || repite) {
        { static int first = 0; if (cuadro_nuevo && first < 2) psp_boot_mark(first++ ? "segundo cuadro" : "primer cuadro"); }
        if (cuadro_nuevo) { QueryPerformanceCounter(&t_rep); mid_begin(); }
        cuadro_nuevo = false;
        PERF(PZ_PRES);
        if (usar_gl) pintar_gl();
        else { HDC dc = GetDC(ventana); pintar_en(dc); ReleaseDC(ventana, dc); }
    }
    hp.mark();
    refrescar_titulo();
}

void psp_window_frame_ready() { cuadro_nuevo = true; n_fps++; }
bool psp_window_hook_due() { return ventana && (cuadro_nuevo || (mid_pend && psp_window_clock() >= mid_due)); }

void psp_window_repeat(int n) { repetir_n = n < 1 ? 1 : n; }

void psp_window_texture(unsigned t, int tw, int th, int uw, int uh, void* valla) {
    tex_ext = (GLuint)t; ext_tw = tw; ext_th = th;
    ext_su = tw ? (float)uw / (float)tw : 1.0f;
    ext_sv = th ? (float)uh / (float)th : 1.0f;
    if (valla_ext) glDeleteSync(valla_ext);
    valla_ext = (GLsync)valla;
}

void psp_window_texture_dual(unsigned t) { tex_dual = (GLuint)t; }

// short message for the player (network refusals); shown by the window between frames
static std::mutex notice_mtx;
static std::string notice_text;
void psp_window_notice(const char* utf8) { std::lock_guard<std::mutex> g(notice_mtx); notice_text = utf8 ? utf8 : ""; }
static void notice_show() {
    std::string t;
    { std::lock_guard<std::mutex> g(notice_mtx); t.swap(notice_text); }
    if (t.empty() || !ventana) return;
    int n = MultiByteToWideChar(CP_UTF8, 0, t.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n : 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, t.c_str(), -1, &w[0], n);
    MessageBoxW(ventana, w.c_str(), L"Multiplayer", MB_OK | MB_ICONINFORMATION);
}

void psp_window_target(int* w, int* h) { int dx, dy; destino(&dx, &dy, w, h); }

double psp_window_fps() { return fps_ultimo; }

double psp_window_aspect() {
    if (modo_vista() != 2 || cli_h < 1) return 480.0 / 272.0;
    return (double)cli_w / (double)cli_h;
}

uint32_t psp_window_buttons() { return osk_reciente() || txt_kb() ? 0u : botones; }
int psp_window_language() { return language; }
void psp_window_rumble(float fuerte, float debil, int ms) { rumble_request(fuerte, debil, ms); }
// local multiplayer: to player k's own pad, only while he plays with it; player 1 before claiming a device keeps the old way
void psp_window_rumble_player(int k, float fuerte, float debil, int ms) {
    if (local_players() < 2 || (k == 0 && player_pads == 0)) { if (k <= 0) rumble_request(fuerte, debil, ms); return; }
    if (k < 0 || k > 3) return;
    int pd = local_pad_dev(k);
    if (pd >= 0 && pl_mode[k] == 1) rumble_request_dev(pd, fuerte, debil, ms);
}
void psp_window_stick(uint8_t* x, uint8_t* y) { *x = stick_x; *y = stick_y; }
bool psp_window_report_requested() { bool p = informe_pedido; informe_pedido = false; return p; }
bool psp_window_capture_requested() { bool p = captura_pedida; captura_pedida = false; return p; }
int psp_window_debug() { return depurar_var; }
int psp_window_aniso() { return aniso_idx; }
int psp_window_bias() { return sesgo_idx; }
bool psp_window_dump_requested() { bool p = volcar_pedido; volcar_pedido = false; return p; }
bool psp_window_scene_requested() { bool p = escena_pedida; escena_pedida = false; return p; }
bool psp_window_lod_toggle() { bool p = lod_conmuta; lod_conmuta = false; return p; }
bool psp_window_clock_requested() { bool p = reloj_pedido; reloj_pedido = false; return p; }
bool psp_window_rate_requested() { bool p = ritmo_pedido; ritmo_pedido = false; return p; }
bool psp_window_pos_requested() { bool p = pos_pedida; pos_pedida = false; return p; }

void psp_window_sleep(int ms) { if (ms > 0) Sleep(ms); }

unsigned psp_window_ms() { return (unsigned)GetTickCount(); }

double psp_window_clock() {
    return (pause_n > 0 ? pause_t0 : raw_clock()) - pause_total;
}

#else

bool psp_window_init(int esc) { escala = esc; return false; }
void psp_link_queue(const char*) {}
void psp_window_wait(volatile bool*) {}
void psp_window_paint() {}
void psp_window_frame_ready() {}
bool psp_window_hook_due() { return false; }
void psp_window_texture(unsigned, int, int, int, int, void*) {}
void psp_window_texture_dual(unsigned) {}
void psp_window_texture_mid(unsigned, int) {}
void psp_window_mid_stats(int* n, double* avg, double* mx) { *n = 0; *avg = *mx = 0; }
double psp_window_mid_due() { return 0.0; }
void psp_window_mid_paint() {}
void psp_window_notice(const char* utf8) { if (utf8) printf("%s\n", utf8); }
void psp_window_repeat(int) {}
void psp_window_target(int* w, int* h) { *w = 480 * escala; *h = 272 * escala; }
double psp_window_fps() { return 0; }
double psp_window_aspect() { return 480.0 / 272.0; }
uint32_t psp_window_buttons() { return 0; }
void psp_window_sample() {}
void psp_window_lat(int, int) {}
int psp_window_language() { return 1; }
void psp_window_busy(int, int, int) {}
void psp_window_rumble(float, float, int) {}
void psp_window_rumble_player(int, float, float, int) {}
void psp_window_stick(uint8_t* x, uint8_t* y) { *x = 128; *y = 128; }
int psp_input_local(int, uint32_t*, uint8_t*, uint8_t*) { return 1; }
bool psp_window_report_requested() { return false; }
bool psp_window_capture_requested() { return false; }
int psp_window_debug() { return 0; }
int psp_window_aniso() { return 0; }
int psp_window_bias() { return 0; }
bool psp_window_dump_requested() { return false; }
bool psp_window_scene_requested() { return false; }
bool psp_window_lod_toggle() { return false; }
void psp_window_osk_open(const char*, const char*, int) {}
void psp_window_text_capture(int) {}
int psp_window_text_pop() { return 0; }
int psp_window_osk_state(char* s, int) { if (s) *s = 0; return 2; }
bool psp_window_clock_requested() { return false; }
bool psp_window_rate_requested() { return false; }
bool psp_window_pos_requested() { return false; }
void psp_window_sleep(int ms) { (void)ms; }
unsigned psp_window_ms() { return 0; }
double psp_window_clock() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#endif

bool psp_window_open() { return abierta; }

uint32_t* psp_window_pixels() { return pixeles; }

uint32_t* psp_window_canvas(int w, int h) {
    if (w != lien_w || h != lien_h) {
        lienzo.resize((size_t)w * h);
        lien_w = w; lien_h = h;
        pixeles = lienzo.data();
    }
    return pixeles;
}
