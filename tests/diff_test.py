import os, sys, json, struct, random, subprocess, argparse, collections

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, 'recompiler'))
import mips, emitter, interpreter
from analyzer import Imagen

RASGUNO = 0x09000000
RASGUNO_TAM = 0x10000
RANGOS = [(RASGUNO, RASGUNO_TAM), (0x08A66000, 0x00101000)]
RA_FIN = 0x0BFFFFF0
MASCARA = 0x0FFFFFFF

MAIN_CPP = r'''
#include "psp.h"
#include <cstdio>
#include <cstdlib>

uint8_t* psp_mem = nullptr;
uint32_t psp_mem_mask = 0x0FFFFFFFu;

void psp_mem_init(uint32_t tam) { psp_mem = (uint8_t*)calloc(1, tam); }
void psp_mem_free() { free(psp_mem); }
void psp_call(Ctx&, uint32_t d) { fprintf(stderr, "llamada no resuelta 0x%08X\n", d); exit(2); }
void psp_jump(Ctx&, uint32_t d) { fprintf(stderr, "salto no resuelto 0x%08X\n", d); exit(2); }
void psp_syscall(Ctx&, uint32_t c) { fprintf(stderr, "syscall 0x%X\n", c); exit(3); }
void psp_break(Ctx&, uint32_t c) { fprintf(stderr, "break 0x%X\n", c); exit(3); }
void psp_vfpu(Ctx&, uint32_t pc, uint32_t) { fprintf(stderr, "vfpu 0x%08X\n", pc); exit(4); }

typedef void (*Fn)(Ctx&);
extern Fn TABLA[];
extern const int TABLA_N;
extern const uint32_t RANGOS[][2];
extern const int RANGOS_N;

static void cargar(const char* p) {
    FILE* f = fopen(p, "rb");
    if (!f) { fprintf(stderr, "no puedo abrir %s\n", p); exit(1); }
    uint32_t n; 
    if (fread(&n, 4, 1, f) != 1) exit(1);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t va, len;
        if (fread(&va, 4, 1, f) != 1 || fread(&len, 4, 1, f) != 1) exit(1);
        if (fread(psp_p(va), 1, len, f) != len) exit(1);
    }
    fclose(f);
}

int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "uso: prueba idx imagen.bin estado.bin salida.bin\n"); return 1; }
    psp_mem_init(0x10000000u);
    cargar(argv[2]);
    FILE* f = fopen(argv[3], "rb");
    Ctx c; memset(&c, 0, sizeof(c));
    uint32_t fcc = 0;
    if (fread(c.r, 4, 32, f) != 32) return 1;
    if (fread(&c.hi, 4, 1, f) != 1) return 1;
    if (fread(&c.lo, 4, 1, f) != 1) return 1;
    for (int i = 0; i < 32; i++) if (fread(&c.f[i].u, 4, 1, f) != 1) return 1;
    if (fread(&fcc, 4, 1, f) != 1) return 1;
    fclose(f);
    c.fcc = fcc != 0;
    c.r[0] = 0;
    int idx = atoi(argv[1]);
    if (idx < 0 || idx >= TABLA_N) return 1;
    TABLA[idx](c);
    FILE* o = fopen(argv[4], "wb");
    c.r[0] = 0;
    fwrite(c.r, 4, 32, o);
    fwrite(&c.hi, 4, 1, o);
    fwrite(&c.lo, 4, 1, o);
    for (int i = 0; i < 32; i++) fwrite(&c.f[i].u, 4, 1, o);
    uint32_t v = c.fcc ? 1u : 0u;
    fwrite(&v, 4, 1, o);
    for (int i = 0; i < RANGOS_N; i++) fwrite(psp_p(RANGOS[i][0]), 1, RANGOS[i][1], o);
    fclose(o);
    psp_mem_free();
    return 0;
}
'''


def es_nan(u):
    return (u & 0x7F800000) == 0x7F800000 and (u & 0x007FFFFF) != 0


def cierre(llamadas, raiz, tope=60):
    vistas, pend = set(), [raiz]
    while pend:
        a = pend.pop()
        if a in vistas:
            continue
        vistas.add(a)
        if len(vistas) > tope:
            return None
        for t in llamadas.get(a, ()):
            pend.append(t)
    return vistas


def analizar(img, direcciones):
    # llamadas directas y banderas por funcion
    info = {}
    for a in direcciones:
        f = emitter.Funcion(img, a, set())
        dest, malo = set(), None
        for pc, ins in sorted(f.instr.items()):
            if ins is None:
                malo = 'vfpu'
                break
            if ins.name == 'jal':
                dest.add(ins.tgt)
            elif ins.name in ('jalr', 'syscall', 'break'):
                malo = ins.name
                break
            elif ins.name == 'jr' and ins.rs != 31 and pc not in f.tablas:
                malo = 'jr indirecto'
                break
        info[a] = dict(dest=dest, malo=malo, instr=len(f.instr))
    return info


def imagen_bin(img, ruta):
    trozos = []
    for va, off, fsz, msz in img.e.segments():
        trozos.append((va, img.e.d[off:off + fsz]))
    with open(ruta, 'wb') as f:
        f.write(struct.pack('<I', len(trozos)))
        for va, d in trozos:
            f.write(struct.pack('<II', va, len(d)))
            f.write(d)
    return trozos


def estado_bin(regs, fregs, hi, lo, fcc):
    b = struct.pack('<32I', *regs) + struct.pack('<II', hi, lo)
    b += struct.pack('<32I', *fregs) + struct.pack('<I', 1 if fcc else 0)
    return b


def main():
    ap = argparse.ArgumentParser(description='Compara el C++ generado contra el interprete de referencia')
    ap.add_argument('--eboot', default=os.path.join(RAIZ, 'game', 'EBOOT.OLD'))
    ap.add_argument('--mapa', default=os.path.join(RAIZ, 'generated', 'programa.json'))
    ap.add_argument('--funciones', type=int, default=120)
    ap.add_argument('--casos', type=int, default=8)
    ap.add_argument('--semilla', type=int, default=1)
    a = ap.parse_args()

    random.seed(a.semilla)
    img = Imagen(a.eboot, 'principal')
    doc = json.load(open(a.mapa))
    principal = doc['imagenes'][0]
    todas = [int(f['a'], 16) for f in principal['funciones']]
    simples = [int(f['a'], 16) for f in principal['funciones']
               if f['vfpu'] == 0 and f['indirecto'] == 0 and 8 <= f['instr'] <= 400]
    random.shuffle(simples)

    info = analizar(img, set(simples[:a.funciones * 6]))
    llamadas = {k: v['dest'] for k, v in info.items()}
    raices, conjunto = [], set()
    for r in simples:
        if len(raices) >= a.funciones:
            break
        if r not in info or info[r]['malo']:
            continue
        c = cierre(llamadas, r)
        if c is None or any(x not in info or info[x]['malo'] for x in c):
            continue
        raices.append(r)
        conjunto |= c
    if not raices:
        print('no encontre funciones aptas')
        return

    print('%d funciones raiz, %d funciones a emitir' % (len(raices), len(conjunto)))
    gen = os.path.join(RAIZ, 'generated')
    os.makedirs(gen, exist_ok=True)
    src, _ = emitter.emitir_unidad(img, conjunto, conjunto)
    extra = ['', 'typedef void (*Fn)(Ctx&);',
             'Fn TABLA[] = {%s};' % ', '.join('f_%08X' % r for r in raices),
             'extern const int TABLA_N; const int TABLA_N = %d;' % len(raices),
             'extern const uint32_t RANGOS[][2]; const uint32_t RANGOS[][2] = {%s};' % ', '.join('{0x%08Xu, 0x%Xu}' % r for r in RANGOS),
             'extern const int RANGOS_N; const int RANGOS_N = %d;' % len(RANGOS)]
    open(os.path.join(gen, 'prueba.cpp'), 'w').write(src + '\n'.join(extra) + '\n')
    open(os.path.join(gen, 'arnes.cpp'), 'w').write(MAIN_CPP)

    exe = os.path.join(gen, 'prueba')
    cmd = ['g++', '-O1', '-std=c++17', '-fno-strict-aliasing', '-w',
           '-I', os.path.join(RAIZ, 'runtime'),
           os.path.join(gen, 'prueba.cpp'), os.path.join(gen, 'arnes.cpp'), '-o', exe]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        print(r.stderr[:4000])
        return
    print('compilado')

    img_bin = os.path.join(gen, 'imagen.bin')
    trozos = imagen_bin(img, img_bin)

    ok = fallo = saltado = nan = 0
    detalles = []
    for idx, raiz in enumerate(raices):
        for caso in range(a.casos):
            regs = [0] * 32
            for i in range(1, 32):
                regs[i] = random.choice([0, 1, random.randrange(0, 0x100),
                                         RASGUNO + random.randrange(0, 0x8000) & ~3,
                                         random.randrange(0, 1 << 32)])
            regs[29] = RASGUNO + 0xE000
            regs[31] = RA_FIN
            regs[0] = 0
            fregs = [random.choice([0, 0x3F800000, 0xBF800000, random.randrange(0, 1 << 32)]) for _ in range(32)]
            hi, lo, fcc = random.randrange(0, 1 << 32), random.randrange(0, 1 << 32), random.random() < 0.5
            rasguno = bytes(random.randrange(0, 256) for _ in range(RASGUNO_TAM))

            mem = interpreter.Memoria(MASCARA)
            for va, d in trozos:
                mem.escribir(va, d)
            mem.escribir(RASGUNO, rasguno)
            cpu = interpreter.Cpu(mem)
            cpu.r = list(regs)
            cpu.f = list(fregs)
            cpu.hi, cpu.lo, cpu.fcc = hi, lo, fcc
            cpu.limite = 400000
            try:
                cpu.correr(raiz, RA_FIN)
            except interpreter.Alto as ex:
                saltado += 1
                continue
            except RecursionError:
                saltado += 1
                continue

            est = os.path.join(gen, 'estado_in.bin')
            sal = os.path.join(gen, 'estado_out.bin')
            with open(est, 'wb') as f:
                f.write(estado_bin(regs, fregs, hi, lo, fcc))
            with open(img_bin, 'ab'):
                pass
            # la imagen incluye el rasguno de este caso
            with open(img_bin, 'wb') as f:
                f.write(struct.pack('<I', len(trozos) + 1))
                for va, d in trozos:
                    f.write(struct.pack('<II', va, len(d)))
                    f.write(d)
                f.write(struct.pack('<II', RASGUNO, len(rasguno)))
                f.write(rasguno)
            p = subprocess.run([exe, str(idx), img_bin, est, sal], capture_output=True, text=True)
            if p.returncode:
                saltado += 1
                continue
            d = open(sal, 'rb').read()
            cr = list(struct.unpack_from('<32I', d, 0))
            chi, clo = struct.unpack_from('<II', d, 128)
            cf = list(struct.unpack_from('<32I', d, 136))
            cfcc = struct.unpack_from('<I', d, 264)[0]
            off = 268
            malo = []
            for i in range(1, 32):
                if i == 31:
                    continue
                if cr[i] != cpu.r[i]:
                    malo.append('$%s C=%08X I=%08X' % (mips.GPR[i], cr[i], cpu.r[i]))
            if (chi, clo) != (cpu.hi, cpu.lo):
                malo.append('hi/lo C=%08X:%08X I=%08X:%08X' % (chi, clo, cpu.hi, cpu.lo))
            for i in range(32):
                if cf[i] != cpu.f[i]:
                    if es_nan(cf[i]) and es_nan(cpu.f[i]):
                        nan += 1
                        continue
                    malo.append('$f%d C=%08X I=%08X' % (i, cf[i], cpu.f[i]))
            if bool(cfcc) != bool(cpu.fcc):
                malo.append('fcc C=%d I=%d' % (cfcc, cpu.fcc))
            for base, tam in RANGOS:
                bc = d[off:off + tam]
                off += tam
                bi = bytes(mem.l8(base + k) for k in range(0, tam)) if tam <= 0x10000 else None
                if bi is None:
                    dif = [k for k in range(0, tam, 4) if struct.unpack_from('<I', bc, k)[0] != mem.l32(base + k)]
                    if dif:
                        malo.append('memoria 0x%08X: %d palabras distintas (primera 0x%08X)' % (base, len(dif), base + dif[0]))
                elif bc != bi:
                    k = next(j for j in range(tam) if bc[j] != bi[j])
                    malo.append('memoria 0x%08X+0x%X C=%02X I=%02X' % (base, k, bc[k], bi[k]))
            if malo:
                fallo += 1
                if len(detalles) < 12:
                    detalles.append('f_%08X caso %d: %s' % (raiz, caso, '; '.join(malo[:4])))
            else:
                ok += 1

    print('coincide %d, difiere %d, saltado %d, NaN de distinta carga %d' % (ok, fallo, saltado, nan))
    for d in detalles:
        print('  ' + d)


if __name__ == '__main__':
    main()
