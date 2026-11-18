#pragma once

// OpenGL 3.3 core context and pointer loading. Everything comes from opengl32.dll at run
// time, so neither -lopengl32 nor system headers are needed.
// One translation unit defines TTT_GL_IMPL before including this file.

#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <map>
#include "perf.h"
#include <string>

#ifndef GL_VERSION_1_1
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef void GLvoid;
#endif
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef int64_t GLint64;
typedef uint64_t GLuint64;
typedef struct __GLsync* GLsync;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_LESS 0x0201
#define GL_LEQUAL 0x0203
#define GL_ALWAYS 0x0207
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_VIEWPORT 0x0BA2
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_COLOR_WRITEMASK 0x0C23
#define GL_STENCIL_WRITEMASK 0x0B98
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#define GL_TEXTURE_2D 0x0DE1
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_DEPTH_COMPONENT 0x1902
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_NEAREST 0x2600
#define GL_NONE 0
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_STENCIL_BUFFER_BIT 0x00000400
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_BGRA 0x80E1
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_FUNC_ADD 0x8006
#define GL_MIN 0x8007
#define GL_MAX 0x8008
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_DEPTH_STENCIL 0x84F9
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9

#define GL_NEVER 0x0200
#define GL_EQUAL 0x0202
#define GL_DEPTH_FUNC 0x0B74
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_CONSTANT_COLOR 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR 0x8002
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_CLAMP 0x864F
#define GL_ZERO 0
#define GL_ONE 1
#define GL_KEEP 0x1E00
#define GL_REPLACE 0x1E01
#define GL_INCR 0x1E02
#define GL_DECR 0x1E03
#define GL_INVERT 0x150A
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#define GL_TEXTURE_LOD_BIAS 0x8501
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_SYNC_GPU_COMMANDS_COMPLETE 0x9117
#define GL_TIMEOUT_IGNORED 0xFFFFFFFFFFFFFFFFull

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x0001

#define TTT_GL_FUNCS \
    X(void,   glClear,                 (GLbitfield)) \
    X(void,   glClearColor,            (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void,   glClearDepth,            (double)) \
    X(void,   glClearStencil,          (GLint)) \
    X(void,   glViewport,              (GLint, GLint, GLsizei, GLsizei)) \
    X(void,   glScissor,               (GLint, GLint, GLsizei, GLsizei)) \
    X(void,   glEnable,                (GLenum)) \
    X(void,   glDisable,               (GLenum)) \
    X(void,   glBlendFunc,             (GLenum, GLenum)) \
    X(void,   glDepthFunc,             (GLenum)) \
    X(void,   glDepthMask,             (GLboolean)) \
    X(void,   glColorMask,             (GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(void,   glCullFace,              (GLenum)) \
    X(void,   glStencilFunc,           (GLenum, GLint, GLuint)) \
    X(void,   glStencilOp,             (GLenum, GLenum, GLenum)) \
    X(void,   glStencilMask,           (GLuint)) \
    X(void,   glGenerateMipmap,        (GLenum)) \
    X(void,   glGetFloatv,             (GLenum, GLfloat*)) \
    X(void,   glTexParameterf,         (GLenum, GLenum, GLfloat)) \
    X(void,   glFrontFace,             (GLenum)) \
    X(void,   glGenTextures,           (GLsizei, GLuint*)) \
    X(void,   glDeleteTextures,        (GLsizei, const GLuint*)) \
    X(void,   glBindTexture,           (GLenum, GLuint)) \
    X(void,   glTexImage2D,            (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
    X(void,   glTexSubImage2D,         (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)) \
    X(void,   glTexParameteri,         (GLenum, GLenum, GLint)) \
    X(void,   glCopyTexSubImage2D,     (GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei)) \
    X(void,   glPixelStorei,           (GLenum, GLint)) \
    X(void,   glReadPixels,            (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*)) \
    X(void,   glDrawArrays,            (GLenum, GLint, GLsizei)) \
    X(void,   glDrawElements,          (GLenum, GLsizei, GLenum, const void*)) \
    X(void,   glFinish,                (void)) \
    X(void,   glFlush,                 (void)) \
    X(GLenum, glGetError,              (void)) \
    X(const GLubyte*, glGetString,     (GLenum)) \
    X(void,   glActiveTexture,         (GLenum)) \
    X(void,   glBlendEquation,         (GLenum)) \
    X(void,   glBlendFuncSeparate,     (GLenum, GLenum, GLenum, GLenum)) \
    X(void,   glBlendColor,            (GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(GLsync, glFenceSync,             (GLenum, GLbitfield)) \
    X(void,   glWaitSync,              (GLsync, GLbitfield, GLuint64)) \
    X(GLenum, glClientWaitSync,        (GLsync, GLbitfield, GLuint64)) \
    X(void,   glDeleteSync,            (GLsync)) \
    X(GLuint, glCreateShader,          (GLenum)) \
    X(void,   glShaderSource,          (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
    X(void,   glCompileShader,         (GLuint)) \
    X(void,   glGetShaderiv,           (GLuint, GLenum, GLint*)) \
    X(void,   glGetShaderInfoLog,      (GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void,   glDeleteShader,          (GLuint)) \
    X(GLuint, glCreateProgram,         (void)) \
    X(void,   glAttachShader,          (GLuint, GLuint)) \
    X(void,   glLinkProgram,           (GLuint)) \
    X(void,   glGetProgramiv,          (GLuint, GLenum, GLint*)) \
    X(void,   glGetProgramInfoLog,     (GLuint, GLsizei, GLsizei*, GLchar*)) \
    X(void,   glUseProgram,            (GLuint)) \
    X(void,   glDeleteProgram,         (GLuint)) \
    X(GLint,  glGetUniformLocation,    (GLuint, const GLchar*)) \
    X(void,   glUniform1i,             (GLint, GLint)) \
    X(void,   glUniform1f,             (GLint, GLfloat)) \
    X(void,   glUniform2f,             (GLint, GLfloat, GLfloat)) \
    X(void,   glUniform4f,             (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void,   glUniform4fv,            (GLint, GLsizei, const GLfloat*)) \
    X(void,   glDrawBuffer,            (GLenum)) \
    X(void,   glRenderbufferStorageMultisample, (GLenum, GLsizei, GLenum, GLsizei, GLsizei)) \
    X(void,   glTexImage2DMultisample, (GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean)) \
    X(void,   glGetIntegerv,           (GLenum, GLint*)) \
    X(void,   glGetBooleanv,           (GLenum, GLboolean*)) \
    X(GLboolean, glIsEnabled,          (GLenum)) \
    X(void,   glReadBuffer,            (GLenum)) \
    X(void,   glUniformMatrix4fv,      (GLint, GLsizei, GLboolean, const GLfloat*)) \
    X(void,   glGenVertexArrays,       (GLsizei, GLuint*)) \
    X(void,   glBindVertexArray,       (GLuint)) \
    X(void,   glDeleteVertexArrays,    (GLsizei, const GLuint*)) \
    X(void,   glGenBuffers,            (GLsizei, GLuint*)) \
    X(void,   glBindBuffer,            (GLenum, GLuint)) \
    X(void,   glBufferData,            (GLenum, GLsizeiptr, const void*, GLenum)) \
    X(void,   glBufferSubData,         (GLenum, GLintptr, GLsizeiptr, const void*)) \
    X(void,   glDeleteBuffers,         (GLsizei, const GLuint*)) \
    X(void,   glEnableVertexAttribArray,  (GLuint)) \
    X(void,   glDisableVertexAttribArray, (GLuint)) \
    X(void,   glVertexAttribPointer,   (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
    X(void,   glGenFramebuffers,       (GLsizei, GLuint*)) \
    X(void,   glBindFramebuffer,       (GLenum, GLuint)) \
    X(void,   glDeleteFramebuffers,    (GLsizei, const GLuint*)) \
    X(void,   glFramebufferTexture2D,  (GLenum, GLenum, GLenum, GLuint, GLint)) \
    X(GLenum, glCheckFramebufferStatus,(GLenum)) \
    X(void,   glGenRenderbuffers,      (GLsizei, GLuint*)) \
    X(void,   glDeleteRenderbuffers,   (GLsizei, const GLuint*)) \
    X(void,   glBindRenderbuffer,      (GLenum, GLuint)) \
    X(void,   glRenderbufferStorage,   (GLenum, GLenum, GLsizei, GLsizei)) \
    X(void,   glFramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint)) \
    X(void,   glBlitFramebuffer,       (GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum))

#define X(ret, nombre, args) typedef ret (__stdcall *T_##nombre) args; extern T_##nombre nombre;
TTT_GL_FUNCS
#undef X

bool gl_init(HWND h);
bool gl_here();
void gl_swap();
bool gl_ready();
GLuint gl_program(const char* vs, const char* fs);
void gl_prewarm(const char* vs, const char* fs);
void* gl_proc(const char* n);

#ifdef TTT_GL_IMPL

#define X(ret, nombre, args) T_##nombre nombre = nullptr;
TTT_GL_FUNCS
#undef X

typedef HGLRC(__stdcall* T_create_attribs)(HDC, HGLRC, const int*);
typedef BOOL(__stdcall* T_swap_interval)(int);
typedef PROC(__stdcall* T_getproc)(LPCSTR);
typedef HGLRC(__stdcall* T_wglCreate)(HDC);
typedef BOOL(__stdcall* T_wglMakeCurrent)(HDC, HGLRC);
typedef BOOL(__stdcall* T_wglDelete)(HGLRC);

static HMODULE gl_dll = nullptr;
static HDC gl_dc = nullptr;
static HGLRC gl_rc = nullptr;
static T_getproc gl_getproc = nullptr;
static bool gl_ok = false;

// every drawing thread needs its own context; objects are shared with the main one
static T_create_attribs gl_create_attr = nullptr;
static T_wglCreate gl_create_basic = nullptr;
static T_wglMakeCurrent gl_activate = nullptr;
static unsigned long gl_main_thread = 0;
static std::mutex gl_mtx;
static thread_local HGLRC gl_rc_thread = nullptr;
#define GL_RESERVA 4
static HGLRC gl_pool[GL_RESERVA] = {nullptr, nullptr, nullptr, nullptr};
static bool gl_pool_warned = false;


static void* gl_find(const char* n) {
    void* p = nullptr;
    if (gl_getproc) p = (void*)gl_getproc(n);
    if (p == nullptr || p == (void*)1 || p == (void*)2 || p == (void*)3 || p == (void*)-1)
        p = (void*)GetProcAddress(gl_dll, n);
    return p;
}

bool gl_ready() { return gl_ok; }
void* gl_proc(const char* n) { return gl_ok ? gl_find(n) : nullptr; }

void gl_swap() { if (gl_ok) SwapBuffers(gl_dc); }

bool gl_init(HWND h) {
    printf("gl: runtime compilado %s %s\n", __DATE__, __TIME__);
    gl_dll = LoadLibraryA("opengl32.dll");
    if (!gl_dll) { printf("gl: no encuentro opengl32.dll\n"); return false; }
    gl_getproc = (T_getproc)GetProcAddress(gl_dll, "wglGetProcAddress");
    T_wglCreate create = (T_wglCreate)GetProcAddress(gl_dll, "wglCreateContext");
    T_wglMakeCurrent activate = (T_wglMakeCurrent)GetProcAddress(gl_dll, "wglMakeCurrent");
    T_wglDelete clear_fb = (T_wglDelete)GetProcAddress(gl_dll, "wglDeleteContext");
    if (!gl_getproc || !create || !activate || !clear_fb) { printf("gl: opengl32.dll incompleta\n"); return false; }

    gl_dc = GetDC(h);
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    int pf = ChoosePixelFormat(gl_dc, &pfd);
    if (!pf || !SetPixelFormat(gl_dc, pf, &pfd)) { printf("gl: sin formato de pixel\n"); return false; }

    HGLRC old = create(gl_dc);
    if (!old || !activate(gl_dc, old)) { printf("gl: no puedo crear el contexto inicial\n"); return false; }

    T_create_attribs create_attr = (T_create_attribs)gl_getproc("wglCreateContextAttribsARB");
    if (create_attr) {
        const int attr[] = {WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 3,
                            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0};
        HGLRC created = create_attr(gl_dc, nullptr, attr);
        // shared contexts must be created now, while the main one is not current yet
        if (created) {
            int n = 0;
            for (int i = 0; i < GL_RESERVA; i++) { gl_pool[i] = create_attr(gl_dc, created, attr); if (gl_pool[i]) n++; }
            if (!n) printf("gl: sin contextos compartidos (error %lu)\n", (unsigned long)GetLastError());
            activate(nullptr, nullptr); clear_fb(old); activate(gl_dc, created); gl_rc = created;
        } else gl_rc = old;
    } else gl_rc = old;

    const char* missing = nullptr;
#define X(ret, nombre, args) nombre = (T_##nombre)gl_find(#nombre); if (!nombre && !missing) missing = #nombre;
    TTT_GL_FUNCS
#undef X
    if (missing) { printf("gl: falta %s, vuelvo a GDI\n", missing); return false; }

    T_swap_interval interval = (T_swap_interval)gl_getproc("wglSwapIntervalEXT");
    if (interval) {
        const char* e = getenv("TTT_VSYNC"); int v = e ? atoi(e) : 0;
        typedef const char*(__stdcall* T_wgl_ext)(void);
        T_wgl_ext ext = (T_wgl_ext)gl_getproc("wglGetExtensionsStringEXT");
        const char* tv = getenv("TTT_VSYNC_TEAR");
        const char* es = ext ? ext() : nullptr;
        bool tear = v > 0 && (!tv || !*tv || atoi(tv) != 0) && es && strstr(es, "WGL_EXT_swap_control_tear");
        BOOL ok = interval(tear ? -v : v);
        if (!ok && tear) { tear = false; interval(v); }
        printf("gl: sincronia vertical %d%s\n", v, tear ? ", adaptativa (un cuadro tarde se muestra sin esperar)" : v > 0 ? ", fija" : "");
    } else printf("gl: sincronia vertical sin control (wglSwapIntervalEXT no disponible)\n");

    printf("gl: %s | %s | %s\n", (const char*)glGetString(GL_VENDOR),
           (const char*)glGetString(GL_RENDERER), (const char*)glGetString(GL_VERSION));
    gl_create_attr = create_attr; gl_create_basic = create; gl_activate = activate;
    gl_main_thread = GetCurrentThreadId();
    gl_rc_thread = gl_rc;
    gl_ok = true;
    return true;
}

bool gl_here() {
    if (!gl_ok) return false;
    if (gl_rc_thread) return true;
    if (GetCurrentThreadId() == gl_main_thread) { gl_rc_thread = gl_rc; return true; }
    std::lock_guard<std::mutex> g(gl_mtx);
    for (int i = 0; i < GL_RESERVA; i++) {
        if (!gl_pool[i]) continue;
        HGLRC r = gl_pool[i];
        gl_pool[i] = nullptr;
        if (!gl_activate(gl_dc, r)) continue;
        gl_rc_thread = r;
        printf("gl: contexto compartido %d para el hilo %lu\n", i, (unsigned long)GetCurrentThreadId());
        return true;
    }
    if (!gl_pool_warned) { gl_pool_warned = true; printf("gl: no queda contexto compartido libre (error %lu)\n", (unsigned long)GetLastError()); }
    return false;
}

static GLuint gl_compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        printf("gl: error compilando shader: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint gl_build(const char* vs, const char* fs);
// programs compiled ahead (gl_prewarm at the first frame) are handed out once to the pass that asks for the same sources
static std::map<std::string, GLuint> gl_warm;
static std::mutex gl_warm_mx;
GLuint gl_program(const char* vs, const char* fs) {
    {
        std::lock_guard<std::mutex> g(gl_warm_mx);
        auto it = gl_warm.find(std::string(vs) + '\x01' + fs);
        if (it != gl_warm.end()) { GLuint p = it->second; gl_warm.erase(it); return p; }
    }
    return gl_build(vs, fs);
}
void gl_prewarm(const char* vs, const char* fs) {
    GLuint p = gl_build(vs, fs);
    if (!p) return;
    std::lock_guard<std::mutex> g(gl_warm_mx);
    gl_warm[std::string(vs) + '\x01' + fs] = p;
}
static GLuint gl_build(const char* vs, const char* fs) {
    PERF_SUB("compilar programa GL");
    GLuint a = gl_compile(GL_VERTEX_SHADER, vs), b = gl_compile(GL_FRAGMENT_SHADER, fs);
    if (!a || !b) return 0;
    GLuint p = glCreateProgram();
    glAttachShader(p, a); glAttachShader(p, b);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    glDeleteShader(a); glDeleteShader(b);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        printf("gl: error enlazando programa: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

#endif
#endif
