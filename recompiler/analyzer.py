import os, sys, json, argparse, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mips
from elf import Elf

BANCO_VA = 0x08A814A0
BANCO_FIN = 0x08AEFB70
TAB_OVERLAY = 0x08A7EA60
RANURAS = 11
JT_ATRAS = 14
JT_MAX = 512


class Imagen:
    def __init__(self, path, nombre, indice=None):
        self.path, self.nombre, self.indice = path, nombre, indice
        self.e = Elf(path)
        self.code = self.e.code_ranges()
        self.data = self.e.data_ranges()
        self.lo = min(a for a, _ in self.code)
        self.hi = max(b for _, b in self.code)

    def u32(self, va):
        return self.e.u32(va)

    def es_codigo(self, va):
        return va % 4 == 0 and any(a <= va < b for a, b in self.code)

    def tabla(self, jr_pc, reg):
        return tabla_salto(self, jr_pc, reg)

    def cabecera(self):
        # overlay: +4 n_ctors, +6 n_dtors, +8 tabla ctors, +0xC tabla dtors
        n = self.u32(BANCO_VA + 4)
        if n is None:
            return [], []
        cnt = (n & 0xFFFF, (n >> 16) & 0xFFFF)
        tab = (self.u32(BANCO_VA + 8), self.u32(BANCO_VA + 0xC))
        out = []
        for t, c in zip(tab, cnt):
            v = []
            for i in range(c or 0):
                p = self.u32((t or 0) + 4 * i)
                if p and self.es_codigo(p):
                    v.append(p)
            out.append(v)
        return out


def es_prologo(img, va):
    w = img.u32(va)
    if w is None or (w >> 26) != 9 or ((w >> 21) & 31) != 29 or ((w >> 16) & 31) != 29:
        return False
    imm = w & 0xFFFF
    return imm >= 0x8000 and (0x10000 - imm) % 8 == 0


def barrer_jal(img):
    out = set()
    for a, b in img.code:
        for va in range(a, b, 4):
            w = img.u32(va)
            if w is None or mips.is_vfpu(w):
                continue
            ins = mips.decode(w, va)
            if ins is not None and ins.name == 'jal':
                out.add(ins.tgt)
    return out


def es_entrada(img, va):
    # prologo, o instruccion que sigue al final de la funcion anterior (jr $ra / j + hueco de retardo)
    if es_prologo(img, va):
        return True
    w = img.u32(va - 8)
    if w is None:
        return False
    ins = mips.decode(w, va - 8)
    return ins is not None and ((ins.name == 'jr' and ins.rs == 31) or ins.name == 'j')


def punteros_hilo(img):
    # direcciones formadas con lui+addiu: punteros a funcion que nunca se llaman directamente
    out = set()
    for a, b in img.code:
        alto = {}
        for va in range(a, b, 4):
            w = img.u32(va)
            if w is None or mips.is_vfpu(w):
                alto.clear()
                continue
            ins = mips.decode(w, va)
            if ins is None:
                alto.clear()
                continue
            if ins.name == 'lui':
                alto[ins.rt] = ins.imm
                continue
            if ins.name == 'addiu' and ins.rs in alto:
                d = ((alto[ins.rs] << 16) + ins.simm) & 0xFFFFFFFF
                if img.es_codigo(d) and es_entrada(img, d):
                    out.add(d)
            if ins.name in ('lui', 'addiu', 'ori'):
                if ins.name != 'lui':
                    alto.pop(ins.rt, None)
            elif ins.name in mips.BRANCHES or ins.name in ('j', 'jal', 'jr', 'jalr'):
                pass
            else:
                alto.pop(ins.rd, None)
                alto.pop(ins.rt, None)
    return out


def punteros(img, rangos, estricto=True):
    out = set()
    for a, b in rangos:
        for va in range(a, b, 4):
            v = img.u32(va)
            if v is None or not img.es_codigo(v):
                continue
            if not estricto or es_entrada(img, v):
                out.add(v)
    return out


def _ultima_escritura(img, reg, pc, tope):
    # instruccion anterior a pc que escribe reg, dentro de tope instrucciones
    for k in range(1, tope):
        q = pc - 4 * k
        if not img.es_codigo(q):
            return None
        w = img.u32(q)
        if w is None or mips.is_vfpu(w):
            continue
        ins = mips.decode(w, q)
        if ins is None:
            continue
        n = ins.name
        if n in ('lui', 'addiu', 'ori', 'addu', 'lw', 'sll', 'addi', 'or', 'subu', 'and', 'xor', 'lb', 'lbu', 'lh', 'lhu'):
            dst = ins.rd if n in ('addu', 'or', 'subu', 'and', 'xor', 'sll') else ins.rt
            if dst == reg:
                return ins
        elif n in ('jal', 'jalr') and reg in (2, 3, 31):
            return None
    return None


def _valor(img, reg, pc, prof=0):
    # constante que lleva reg justo antes de pc, siguiendo lui/addiu/ori/addu; None si es variable
    if reg == 0:
        return 0
    if prof > 6:
        return None
    ins = _ultima_escritura(img, reg, pc, JT_ATRAS)
    if ins is None:
        return None
    n = ins.name
    if n == 'lui':
        return (ins.imm << 16) & 0xFFFFFFFF
    if n in ('addiu', 'addi'):
        v = _valor(img, ins.rs, ins.pc, prof + 1)
        return None if v is None else (v + ins.simm) & 0xFFFFFFFF
    if n == 'ori':
        v = _valor(img, ins.rs, ins.pc, prof + 1)
        return None if v is None else (v | ins.imm)
    if n in ('addu', 'or'):
        a = _valor(img, ins.rs, ins.pc, prof + 1)
        b = _valor(img, ins.rt, ins.pc, prof + 1)
        if a is not None and b is not None:
            return (a + b) & 0xFFFFFFFF if n == 'addu' else (a | b)
        return a if a is not None else b
    return None


def tabla_salto(img, jr_pc, reg):
    # jr $reg precedido de lw $reg, off($base); la base puede formarse con lui/addiu/addu
    ins = _ultima_escritura(img, reg, jr_pc, JT_ATRAS)
    if ins is None or ins.name != 'lw':
        return None
    base = _valor(img, ins.rs, ins.pc)
    if base is None:
        return None
    base = (base + ins.simm) & 0xFFFFFFFF
    tgts = []
    for i in range(JT_MAX):
        v = img.u32(base + 4 * i)
        if v is None or not img.es_codigo(v):
            break
        tgts.append(v)
    return (base, tgts) if tgts else None


def recorrer(img, semillas):
    funcs, jtabs, llamadas, jalrs, externos = {}, {}, set(), set(), collections.Counter()
    cubierto = set()
    pend = collections.deque(sorted(a for a in semillas if img.es_codigo(a)))
    hechas = set()
    while pend:
        ini = pend.popleft()
        if ini in hechas:
            continue
        hechas.add(ini)
        local, visto = [ini], set()
        nvf = nind = 0
        fin = ini
        while local:
            pc = local.pop()
            while img.es_codigo(pc) and pc not in visto:
                visto.add(pc)
                cubierto.add(pc)
                fin = max(fin, pc)
                w = img.u32(pc)
                ins = mips.decode(w, pc)
                if ins is None:
                    if mips.is_vfpu(w):
                        nvf += 1
                    pc += 4
                    continue
                nm = ins.name
                if nm in mips.BRANCHES:
                    t = ins.branch_target
                    if img.es_codigo(t):
                        local.append(t)
                    pc += 4
                    continue
                if nm == 'jal':
                    llamadas.add((pc, ins.tgt))
                    if img.es_codigo(ins.tgt):
                        if ins.tgt not in hechas:
                            pend.append(ins.tgt)
                    else:
                        externos[ins.tgt] += 1
                    pc += 4
                    continue
                if nm == 'jalr':
                    nind += 1
                    jalrs.add(pc)
                    pc += 4
                    continue
                if nm in ('j', 'jr'):
                    if nm == 'j':
                        if img.es_codigo(ins.tgt):
                            local.append(ins.tgt)
                    elif ins.rs != 31:
                        jt = tabla_salto(img, pc, ins.rs)
                        if jt:
                            jtabs[pc] = jt
                            local.extend(jt[1])
                        else:
                            nind += 1
                    visto.add(pc + 4)
                    cubierto.add(pc + 4)
                    fin = max(fin, pc + 4)
                    break
                pc += 4
        funcs[ini] = dict(a=ini, fin=fin + 4, instr=len(visto), vfpu=nvf, indirecto=nind)
    return dict(funcs=funcs, jtabs=[(a, b, t) for a, (b, t) in sorted(jtabs.items())],
                llamadas=llamadas, jalrs=jalrs, externos=externos, cubierto=cubierto)


def huecos(img, cubierto):
    out = []
    for a, b in img.code:
        run = None
        for va in range(a, b, 4):
            if va in cubierto:
                if run is not None:
                    out.append([run, va])
                    run = None
            elif run is None:
                run = va
        if run is not None:
            out.append([run, b])
    return out


def refinar(img, semillas, vueltas=6):
    r = recorrer(img, semillas)
    conocidas = set(semillas) | set(r['funcs'])
    for _ in range(vueltas):
        extra = set()
        for a, b in huecos(img, r['cubierto']):
            for va in range(a, b, 4):
                if va not in r['cubierto'] and es_prologo(img, va):
                    extra.add(va)
        extra -= conocidas
        if not extra:
            break
        conocidas |= extra
        r = recorrer(img, conocidas)
    return r


def ranuras(principal):
    out = []
    for i in range(RANURAS):
        w = principal.u32(TAB_OVERLAY + 8 * i)
        if w is not None:
            out.append(dict(ranura=i, desc=TAB_OVERLAY + 8 * i, indice=w & 0xFFFF))
    return out


def construir(eboot, dir_overlays, salida, log=print):
    principal = Imagen(eboot, 'principal')
    overlays = []
    if dir_overlays and os.path.isdir(dir_overlays):
        for fn in sorted(os.listdir(dir_overlays)):
            if fn.lower().endswith('.elf'):
                try:
                    idx = int(fn.split('_')[0]) - 1
                except ValueError:
                    idx = None
                overlays.append(Imagen(os.path.join(dir_overlays, fn), fn[:-4], idx))
        overlays.sort(key=lambda o: o.indice if o.indice is not None else 1 << 30)

    semillas = ({principal.e.entry} | barrer_jal(principal) | punteros(principal, principal.data)
                | punteros_hilo(principal))
    r = refinar(principal, {a for a in semillas if a < BANCO_VA})
    al_banco = {t for _, t in r['llamadas'] if BANCO_VA <= t < BANCO_FIN}
    al_banco |= {a for a in semillas if BANCO_VA <= a < BANCO_FIN}
    log('principal: %d funciones, %d tablas de salto, %d destinos en el banco'
        % (len(r['funcs']), len(r['jtabs']), len(al_banco)))

    imgs = [[principal, r, sorted(al_banco)]]
    atras = set()
    for o in overlays:
        c, d = o.cabecera()
        s = set(al_banco) | set(c) | set(d) | barrer_jal(o) | punteros(o, o.code) | punteros_hilo(o)
        ro = refinar(o, s)
        atras |= {t for _, t in ro['llamadas'] if t < BANCO_VA}
        imgs.append([o, ro, sorted(c), sorted(d)])
        cub = len(ro['cubierto']) * 4
        tam = sum(b - a for a, b in o.code)
        log('  %-14s idx %-6s %5d func  %3d tablas  %6d/%6d codigo (%.0f%% datos)'
            % (o.nombre, o.indice, len(ro['funcs']), len(ro['jtabs']), cub, tam, 100.0 * (tam - cub) / tam))

    nuevas = {a for a in atras if a not in r['funcs'] and principal.es_codigo(a)}
    if nuevas:
        r = refinar(principal, set(r['funcs']) | nuevas)
        imgs[0][1] = r
        log('segunda pasada del principal: %d funciones (%d nuevas desde overlays)' % (len(r['funcs']), len(nuevas)))

    doc = dict(eboot=os.path.basename(eboot),
               banco=dict(vaddr=hx(BANCO_VA), fin=hx(BANCO_FIN), tam=BANCO_FIN - BANCO_VA),
               segmentos=[dict(vaddr=hx(v), off='0x%X' % o, filesz='0x%X' % f, memsz='0x%X' % m)
                          for v, o, f, m in principal.e.segments()],
               ranuras_overlay=ranuras(principal), imagenes=[])
    for item in imgs:
        img, res = item[0], item[1]
        e = dict(nombre=img.nombre, indice_packfile=img.indice, vaddr=hx(img.lo), fin=hx(img.hi),
                 regiones_codigo=[[hx(a), hx(b)] for a, b in img.code],
                 funciones=[dict(a=hx(f['a']), fin=hx(f['fin']), instr=f['instr'], vfpu=f['vfpu'],
                                 indirecto=f['indirecto'])
                            for f in sorted(res['funcs'].values(), key=lambda x: x['a'])],
                 tablas_salto=[dict(jr=hx(a), base=hx(b), destinos=[hx(t) for t in ts])
                               for a, b, ts in res['jtabs']],
                 llamadas_indirectas=[hx(a) for a in sorted(res['jalrs'])],
                 datos_en_codigo=[[hx(a), hx(b)] for a, b in huecos(img, res['cubierto'])])
        if img is principal:
            e['llamadas_al_banco'] = [hx(t) for t in item[2]]
        else:
            e['ctors'] = [hx(a) for a in item[2]]
            e['dtors'] = [hx(a) for a in item[3]]
            e['llamadas_al_principal'] = sorted({hx(t) for _, t in res['llamadas'] if t < BANCO_VA})
        doc['imagenes'].append(e)

    json.dump(doc, open(salida, 'w', encoding='utf-8'), indent=1)
    log('escrito %s: %d funciones, %d instrucciones, %d VFPU'
        % (salida, sum(len(i[1]['funcs']) for i in imgs), sum(len(i[1]['cubierto']) for i in imgs),
           sum(sum(f['vfpu'] for f in i[1]['funcs'].values()) for i in imgs)))
    return doc


def hx(v):
    return '0x%08X' % v


def main():
    ap = argparse.ArgumentParser(description='Mapa del programa completo para la recompilacion')
    ap.add_argument('eboot')
    ap.add_argument('--overlays')
    ap.add_argument('-o', '--out', default='programa.json')
    a = ap.parse_args()
    construir(a.eboot, a.overlays, a.out)


if __name__ == '__main__':
    main()
