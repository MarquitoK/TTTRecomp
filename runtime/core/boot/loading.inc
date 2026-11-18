// load: burst of PACKFILE reads excluding streamed audio; closes after 0.5 s without reads
static double clock_s() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Load { bool active = false; double t0 = 0, ult = 0, read = 0, wait_for = 0; long c0 = 0; uint64_t bytes = 0, ms0 = 0, armed0 = 0; unsigned n = 0; int e0 = -1, e1 = -1; };
static Load cg;
// turbo: in a large load (8 reads or more) the wall clock is not awaited; the vcount follows the frames
static bool load_turbo() {
    static int v = -1;
    if (v < 0) { const char* e = getenv("TTT_LOAD_TURBO"); v = e ? atoi(e) : 1; }
    return v && cg.active && cg.n >= 8 && clock_s() - cg.ult < 0.15;
}
// TTT_READS=text: prints every PACKFILE read whose path contains that text
static void read_trace(uint32_t bytes) {
    static const char* f = getenv("TTT_READS");
    if (!f || !*f || disc_read_entry < 0 || disc_read_entry >= (int)disc_pk.size()) return;
    if (strstr(disc_pk[disc_read_entry].fpath.c_str(), f)) printf("read: %s, %u bytes, cuadro %ld\n", disc_pk[disc_read_entry].fpath.c_str(), bytes, cuadros);
}
static void load_read(double t_start, uint32_t bytes) {
    read_trace(bytes);
    if (disc_read_at3 || !bytes) return;
    double t = clock_s();
    if (t - t_start > 0.02) printf("load: lectura lenta de %s, %u bytes, %.0f ms\n", disc_read_entry >= 0 ? disc_pk[disc_read_entry].fpath.c_str() : "?", bytes, (t - t_start) * 1000.0);
    if (!cg.active) { cg = Load(); cg.active = true; cg.t0 = t_start; cg.c0 = cuadros; cg.ms0 = disc_ms; cg.armed0 = disc_built; cg.e0 = disc_read_entry; }
    cg.ult = t; cg.read += t - t_start; cg.bytes += bytes; cg.n++; cg.e1 = disc_read_entry;
}
static void load_check() {
    if (!cg.active || clock_s() - cg.ult < 0.5) return;
    cg.active = false;
    auto nombre = [](int i) { return i >= 0 && i < (int)disc_pk.size() ? disc_pk[i].fpath.c_str() : "?"; };
    printf("load: %.2f s, %u lecturas, %.1f MB, leyendo %.0f ms (%llu armados en %llu ms), %ld cuadros, espera de ritmo %.0f ms, de %s a %s\n",
           cg.ult - cg.t0, cg.n, cg.bytes / 1048576.0, cg.read * 1000.0, (unsigned long long)(disc_built - cg.armed0),
           (unsigned long long)(disc_ms - cg.ms0), cuadros - cg.c0, cg.wait_for * 1000.0, nombre(cg.e0), nombre(cg.e1));
}

// guest room left from dst to the end of its region (main RAM, port heap, targets)
static uint32_t io_room(uint32_t dst) {
    uint32_t a = dst & psp_mem_mask;
    uint32_t top = std::max(brk_free, 0x0A000000u);   // the game's blocks run on past 0x0A000000 (TTT_HEAP_KB)
    uint32_t end = a < top ? top : a < 0x10000000u ? 0x10000000u : psp_mem_mask + 1u;
    return end - a;
}
static const char* io_name(const IoFile& x, uint64_t pos) {
    if (!x.virtual_) return "archivo local";
    uint32_t s = (uint32_t)(pos / DISC_SECTOR);
    size_t k = (size_t)(std::upper_bound(disc_spans.begin(), disc_spans.end(), s, [](uint32_t v, const DiscSpan& t) { return v < t.lbn; }) - disc_spans.begin());
    while (k) {
        const DiscSpan& t = disc_spans[--k];
        if (!t.sectors && k) continue;
        if (t.type == 2 && t.idx >= 0 && t.idx < (int)disc_pk.size()) return disc_pk[t.idx].fpath.c_str();
        if (t.type == 0 && t.idx >= 0 && t.idx < (int)disc_files.size()) return disc_files[t.idx].fpath.c_str();
        break;
    }
    return "?";
}
static bool io_guard() {
    static int v = -1;
    if (v < 0) { const char* e = getenv("TTT_IO_GUARD"); v = e ? atoi(e) : 1; }
    return v != 0;
}

static uint32_t io_last_dst = 0, io_last_bytes = 0; static long io_last_frame = -1; static std::string io_last_name;
const char* psp_io_last(uint32_t* dst, uint32_t* bytes, long* frame) { *dst = io_last_dst; *bytes = io_last_bytes; *frame = io_last_frame; return io_last_name.c_str(); }
static uint32_t io_read(int fd, uint32_t dst, uint32_t n) {
    if (fd < 1 || fd >= 32 || (!files[fd].f && !files[fd].virtual_)) return 0xFFFFFFFFu;
    IoFile& x = files[fd];
    io_last_dst = dst; io_last_frame = cuadros;
    io_last_name = io_name(x, (uint64_t)x.base + x.pos);
    uint64_t want = x.sectors ? (uint64_t)n * 2048u : n;
    if (x.pos >= x.size_b) want = 0;
    else if ((uint64_t)x.pos + want > (uint64_t)x.size_b) want = (uint64_t)(x.size_b - x.pos);
    uint32_t bytes = (uint32_t)std::min<uint64_t>(want, 0xFFFFFFFFu);
    io_last_bytes = bytes;
    if (io_guard()) {
        uint32_t room = io_room(dst);
        if (bytes > room) {
            static int warned = 0;
            if (warned++ < 12) printf("io: lectura recortada %s, pedidos %u %s (%u bytes), caben %u desde 0x%08X, pos 0x%llX de %lld\n",
                                      io_name(x, (uint64_t)x.base + x.pos), n, x.sectors ? "sectores" : "bytes", bytes, room, dst, x.pos, x.size_b);
            bytes = x.sectors ? room / 2048u * 2048u : room;
        }
    }
    if (x.virtual_) {
        uint8_t* d = psp_p(dst);
        double t_start = clock_s();
        disc_read((uint64_t)x.base + x.pos, d, bytes);
        load_read(t_start, bytes);
        disc_verify((uint64_t)x.base + x.pos, d, bytes);
        if (io_traces < 8) { printf("  read fd %d pos 0x%llX %u %s -> %u bytes a 0x%08X (virtual)\n", fd, x.pos, n, x.sectors ? "sectores" : "bytes", bytes, dst); io_traces++; }
        x.pos += bytes;
        return x.sectors ? bytes / 2048 : bytes;
    }
    fseek(x.f, (long)(x.base + x.pos), SEEK_SET);
    size_t read_n = fread(psp_p(dst), 1, bytes, x.f);
    if (io_traces < 8) { printf("  read fd %d pos 0x%llX %u %s -> %zu bytes a 0x%08X\n", fd, x.pos, n, x.sectors ? "sectores" : "bytes", read_n, dst); io_traces++; }
    x.pos += (long long)read_n;
    return x.sectors ? (uint32_t)(read_n / 2048) : (uint32_t)read_n;
}

static uint32_t io_lseek(int fd, int32_t off, int mode) {
    if (fd < 1 || fd >= 32 || (!files[fd].f && !files[fd].virtual_)) return 0xFFFFFFFFu;
    IoFile& x = files[fd];
    long long o = x.sectors ? (long long)off * 2048 : (long long)off;
    if (mode == 0) x.pos = o;
    else if (mode == 1) x.pos += o;
    else x.pos = x.size_b + o;
    if (x.pos < 0) x.pos = 0;
    if (x.pos > x.size_b) x.pos = x.size_b;
    if (io_traces < 8) { printf("  lseek fd %d -> pos 0x%llX (modo %d, arg %d)\n", fd, x.pos, mode, off); io_traces++; }
    return x.sectors ? (uint32_t)(x.pos / 2048) : (uint32_t)x.pos;
}

static void io_close(int fd) {
    if (fd < 1 || fd >= 32) return;
    files[fd].virtual_ = 0;
    if (!files[fd].f) return;
    fclose(files[fd].f);
    files[fd].f = nullptr;
}

