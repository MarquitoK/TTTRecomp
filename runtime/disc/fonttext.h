#pragma once
// names written with the app's fonts (games/ttt/fonts/<style>/: fill.png, outline.png, font.fnt, style.txt), drawn at the
// atlas scale (4x) into a cell of the game's size and reduced to the quality asked
namespace ft {
struct Glyph { int x = 0, y = 0, w = 0, h = 0, xo = 0, yo = 0, xa = 0; };
struct Font {
    bool ok = false;
    int lh = 0, base = 0, scale = 4, align = 2, fw = 0, fh = 0, ow = 0, oh = 0;
    float sdx = 0, sdy = 0, size = 1;   // size: text scale inside the cell (the port's choice per use)
    uint32_t sc = 0;   // shadow colour RGBA (r in the low byte)
    std::map<uint32_t, Glyph> g;
    std::map<uint64_t, int> kern;
    std::vector<uint32_t> fill, outline;
};
struct Img { int w = 0, h = 0; std::vector<uint32_t> px; };

static bool load_png(const std::string& f, int* w, int* h, std::vector<uint32_t>& px) {
    FILE* fp = fopen(f.c_str(), "rb");
    if (!fp) return false;
    std::vector<uint8_t> d;
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    d.resize(n > 0 ? (size_t)n : 0);
    bool r = fread(d.data(), 1, d.size(), fp) == d.size();
    fclose(fp);
    std::string err;
    return r && png_decode(d, w, h, px, &err) && *w > 0;
}
static int kv(const char* line, const char* key) {
    const char* p = strstr(line, key);
    return p ? atoi(p + strlen(key)) : 0;
}
static Font load(const std::string& dir) {
    Font f;
    FILE* fp = fopen((dir + "/font.fnt").c_str(), "r");
    if (!fp) return f;
    char line[512];
    while (fgets(line, sizeof line, fp)) {
        if (!strncmp(line, "common ", 7)) { f.lh = kv(line, " lineHeight="); f.base = kv(line, " base="); }
        else if (!strncmp(line, "char ", 5)) {
            Glyph g; g.x = kv(line, " x="); g.y = kv(line, " y="); g.w = kv(line, " width="); g.h = kv(line, " height=");
            g.xo = kv(line, " xoffset="); g.yo = kv(line, " yoffset="); g.xa = kv(line, " xadvance=");
            f.g[(uint32_t)kv(line, " id=")] = g;
        } else if (!strncmp(line, "kerning ", 8)) f.kern[(uint64_t)(uint32_t)kv(line, " first=") << 32 | (uint32_t)kv(line, " second=")] = kv(line, " amount=");
    }
    fclose(fp);
    if ((fp = fopen((dir + "/style.txt").c_str(), "r"))) {
        while (fgets(line, sizeof line, fp)) {
            std::string s = line;
            size_t h = s.find('#'); if (h != std::string::npos) s = s.substr(0, h);
            size_t eq = s.find('=');
            if (eq == std::string::npos) continue;
            std::string k = s.substr(0, eq), v = s.substr(eq + 1);
            while (!v.empty() && isspace((unsigned char)v.back())) v.pop_back();
            if (k == "size") f.size = (float)atof(v.c_str());
            else if (k == "scale") f.scale = std::max(1, atoi(v.c_str()));
            else if (k == "align") f.align = v.find("left") == 0 ? 0 : v.find("center") == 0 ? 1 : 2;
            else if (k == "shadow_dx") f.sdx = (float)atof(v.c_str());
            else if (k == "shadow_dy") f.sdy = (float)atof(v.c_str());
            else if (k == "shadow_color") { uint32_t c = (uint32_t)strtoul(v.c_str(), nullptr, 16); f.sc = (c >> 24) | ((c >> 8) & 0xFF00) | ((c << 8) & 0xFF0000) | (c << 24); }
        }
        fclose(fp);
    }
    f.ok = f.lh > 0 && !f.g.empty() && load_png(dir + "/fill.png", &f.fw, &f.fh, f.fill);
    if (f.ok && !load_png(dir + "/outline.png", &f.ow, &f.oh, f.outline)) { f.outline.clear(); f.ow = f.oh = 0; }
    return f;
}
static std::vector<uint32_t> utf32(const std::string& s) {
    std::vector<uint32_t> o;
    for (size_t i = 0; i < s.size();) {
        uint8_t c = (uint8_t)s[i];
        int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        uint32_t cp = n == 1 ? c : n == 2 ? c & 0x1F : n == 3 ? c & 0x0F : c & 0x07;
        for (int k = 1; k < n && i + k < s.size(); k++) cp = cp << 6 | ((uint8_t)s[i + k] & 0x3F);
        o.push_back(cp); i += n;
    }
    return o;
}
// premultiplied source-over of one atlas glyph (alpha times tint when tint != 0) at (dx, dy), x scaled by kx
static void blit(std::vector<float>& c, int W, int H, const std::vector<uint32_t>& at, int aw, const Glyph& g, float dx, float dy, float kx, uint32_t tint) {
    int x0 = (int)std::floor(dx), x1 = (int)std::ceil(dx + g.w * kx), y0 = (int)std::floor(dy);
    for (int y = 0; y < g.h; y++) {
        int Y = y0 + y;
        if (Y < 0 || Y >= H) continue;
        for (int X = std::max(0, x0); X < std::min(W, x1); X++) {
            float sx = (X + 0.5f - dx) / kx - 0.5f;
            int a = (int)std::floor(sx);
            float t = sx - a;
            float s[4] = {0, 0, 0, 0};
            for (int k = 0; k < 2; k++) {
                int u = a + k;
                if (u < 0 || u >= g.w) continue;
                uint32_t p = at[(size_t)(g.y + y) * aw + g.x + u];
                float wgt = (k ? t : 1 - t) * ((p >> 24) / 255.0f);
                if (tint) { s[0] += wgt * (tint & 255); s[1] += wgt * ((tint >> 8) & 255); s[2] += wgt * ((tint >> 16) & 255); s[3] += wgt * ((tint >> 24) / 255.0f) * 255.0f; }
                else { s[0] += wgt * (p & 255); s[1] += wgt * ((p >> 8) & 255); s[2] += wgt * ((p >> 16) & 255); s[3] += wgt * 255.0f; }
            }
            if (tint) { float ta = (tint >> 24) / 255.0f; if (ta > 0) { s[0] *= ta; s[1] *= ta; s[2] *= ta; } }
            float sa = s[3] / 255.0f;
            if (sa <= 0) continue;
            float* d = &c[((size_t)Y * W + X) * 4];
            for (int k = 0; k < 4; k++) d[k] = s[k] + d[k] * (1.0f - sa);
        }
    }
}
// text in a cell of cw x ch game pixels at quality q (1, 2, 4); align -1 = the style's; margin on the aligned side and the widest
// text allowed (0 = the cell) in game pixels
static Img compose1(const Font& f, const std::string& text, int cw, int ch, int q, int align, float margin, float maxw) {
    Img o;
    if (!f.ok || cw <= 0 || ch <= 0) return o;
    int S = f.scale, W = cw * S, H = ch * S;
    std::vector<uint32_t> cp = utf32(text);
    std::vector<float> pen;
    float x = 0, mn = 1e9f, mx = -1e9f;
    for (size_t i = 0; i < cp.size(); i++) {
        auto it = f.g.find(cp[i]);
        if (it == f.g.end()) it = f.g.find('?');
        if (it == f.g.end()) { pen.push_back(-1e9f); continue; }
        if (i) { auto k = f.kern.find((uint64_t)cp[i - 1] << 32 | cp[i]); if (k != f.kern.end()) x += k->second; }
        pen.push_back(x);
        if (it->second.w) { mn = std::min(mn, x + it->second.xo); mx = std::max(mx, x + it->second.xo + it->second.w); }
        x += it->second.xa;
    }
    o.w = cw * q; o.h = ch * q; o.px.assign((size_t)o.w * o.h, 0);
    if (mx <= mn) return o;
    int al = align >= 0 ? align : f.align;
    float room = (maxw > 0 ? std::min(maxw, cw - 2 * margin) : cw - 2 * margin) * S, ink = mx - mn, kx = ink > room ? room / ink : 1.0f;
    float ox = al == 0 ? margin * S - mn * kx : al == 1 ? (W - ink * kx) * 0.5f - mn * kx : (cw - margin) * S - mx * kx;
    float oy = (H - f.lh) * 0.5f;
    std::vector<float> c((size_t)W * H * 4, 0.0f);
    for (int pass = 0; pass < 3; pass++) {   // shadow, outline, fill
        if (pass == 0 && !(f.sc >> 24)) continue;
        if (pass == 1 && f.outline.empty()) continue;
        const std::vector<uint32_t>& at = pass == 2 || (pass == 0 && f.outline.empty()) ? f.fill : f.outline;
        int aw = &at == &f.fill ? f.fw : f.ow;
        for (size_t i = 0; i < cp.size(); i++) {
            if (pen[i] < -1e8f) continue;
            auto it = f.g.find(cp[i]);
            if (it == f.g.end()) it = f.g.find('?');
            const Glyph& g = it->second;
            if (!g.w || !g.h) continue;
            float dx = ox + (pen[i] + g.xo) * kx, dy = oy + g.yo;
            if (pass == 0) { dx += f.sdx; dy += f.sdy; }
            blit(c, W, H, at, aw, g, dx, dy, kx, pass == 0 ? f.sc : 0);
        }
    }
    float r = (float)S / q;   // box reduction to the quality
    for (int y = 0; y < o.h; y++)
        for (int X = 0; X < o.w; X++) {
            float s[4] = {0, 0, 0, 0};
            int x0 = (int)(X * r), x1 = std::max(x0 + 1, std::min(W, (int)((X + 1) * r))), y0 = (int)(y * r), y1 = std::max(y0 + 1, std::min(H, (int)((y + 1) * r)));
            for (int j = y0; j < y1; j++) for (int i = x0; i < x1; i++) { const float* p = &c[((size_t)j * W + i) * 4]; for (int k = 0; k < 4; k++) s[k] += p[k]; }
            float a = s[3] / ((x1 - x0) * (y1 - y0));
            if (a <= 0.5f) continue;
            uint32_t R = (uint32_t)std::min(255.0f, s[0] / s[3] * 255.0f + 0.5f), G = (uint32_t)std::min(255.0f, s[1] / s[3] * 255.0f + 0.5f), B = (uint32_t)std::min(255.0f, s[2] / s[3] * 255.0f + 0.5f);
            o.px[(size_t)y * o.w + X] = R | G << 8 | B << 16 | (uint32_t)std::min(255.0f, a + 0.5f) << 24;
        }
    return o;
}
// with f.size != 1 the text is laid out in a cell 1/size as big and the result reduced into the real cell
static Img compose(const Font& f, const std::string& text, int cw, int ch, int q, int align = -1, float margin = 0, float maxw = 0) {
    if (f.size <= 0 || std::fabs(f.size - 1.0f) < 0.01f) return compose1(f, text, cw, ch, q, align, margin, maxw);
    float k = 1.0f / f.size;
    Img b = compose1(f, text, (int)std::ceil(cw * k), (int)std::ceil(ch * k), q, align, margin * k, maxw * k);
    Img o; o.w = cw * q; o.h = ch * q; o.px.assign((size_t)o.w * o.h, 0);
    if (!b.w) return o;
    float sx = (float)b.w / o.w, sy = (float)b.h / o.h;
    for (int y = 0; y < o.h; y++)
        for (int x = 0; x < o.w; x++) {
            int x0 = (int)(x * sx), x1 = std::max(x0 + 1, std::min(b.w, (int)((x + 1) * sx))), y0 = (int)(y * sy), y1 = std::max(y0 + 1, std::min(b.h, (int)((y + 1) * sy)));
            float a = 0, c[3] = {0, 0, 0};
            for (int j = y0; j < y1; j++) for (int i = x0; i < x1; i++) { uint32_t p = b.px[(size_t)j * b.w + i]; float pa = (float)(p >> 24); a += pa; c[0] += pa * (p & 255); c[1] += pa * ((p >> 8) & 255); c[2] += pa * ((p >> 16) & 255); }
            if (a <= 0) continue;
            float n = (float)((x1 - x0) * (y1 - y0));
            o.px[(size_t)y * o.w + x] = (uint32_t)(c[0] / a + 0.5f) | (uint32_t)(c[1] / a + 0.5f) << 8 | (uint32_t)(c[2] / a + 0.5f) << 16 | (uint32_t)(a / n + 0.5f) << 24;
        }
    return o;
}
}
