#!/usr/bin/env python3
# BT3 (PS2) character files opened straight into the TTT folders of their colour: P file -> 1_p (BT3 names, .dbt as PNG folders,
# move list as TTT), _anm.pak -> 2_anims (.canm), _eff.pak -> 3_effects (VFX closed); the port tells BT3 from TTT by content
import os
import time, struct, sys, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import open_paks as ap

LISTA = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'lists', 'bt3', 'bt3_char_p.txt')
EFF_NOMBRES = ['00_skill_001.pak', '01_skill_002.pak', '02_skill_003.pak', '03_skill_004.pak', '04_skill_005.pak', '05_effect_common.pak']
COMUN = ['00_common_param.dat', '01_charge_aura.pak', '02_kidan.pak', '03_charge_kidan.pak', '04_unused.pak', '05_ki_wave.pak',
         '06_energy_wave.pak', '07_skill_cameras.pak', '08_jetpack.pak', '09_unused.pak', '10_absorb.pak']
LISTA_TTT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'lists', 'paks', 'ttt_char.txt')


def es_p(d):
    if len(d) < 12:
        return False
    n, o0 = struct.unpack_from('<II', d, 0)
    return n == 252 and 4 + 4 * (n + 1) <= o0 <= 0x1000 and o0 % 16 == 0


def es_anm(d):
    if len(d) < 12:
        return False
    n, o0 = struct.unpack_from('<II', d, 0)
    return n == 440 and 4 + 4 * (n + 1) <= o0 <= 0x1000 and o0 % 16 == 0


def es_eff(d):
    if len(d) < 12:
        return False
    n, o0 = struct.unpack_from('<II', d, 0)
    return n == 6 and 4 + 4 * (n + 1) <= o0 <= 0x1000 and o0 % 16 == 0


def entradas(d):
    n = struct.unpack_from('<I', d, 0)[0]
    o = struct.unpack_from('<%dI' % (n + 1), d, 4)
    return [d[o[i]:o[i + 1]] for i in range(n)]


# move list texts (UTF-16): the index of entries 43..51 is not reliable (it can point inside or before a text, or a text can
# spill over the next entry): every BOM in that span starts one text, and each text is told by its first category label
ROTULO = {'Special Attack': 'en', 'Ataque especial': 'es', '0': 'es0', '': 'es0'}   # es0: lista parcial sin rotulos


def textos_movelist(d):
    n = struct.unpack_from('<I', d, 0)[0]
    o = struct.unpack_from('<%dI' % (n + 1), d, 4)
    a, b = max(0, o[43] - 64), min(len(d), o[min(52, n)] + 64)
    out = {}
    i = a
    while True:
        j = d.find(b'\xff\xfe', i, b)
        if j < 0:
            break
        i = j + 2
        if j % 2:
            continue
        fin = j + 2
        while fin + 1 < len(d) and d[fin:fin + 2] != b'\x00\x00':
            fin += 2
        t = d[j + 2:fin].decode('utf-16-le', errors='replace')
        if '﻿' in t[:16] or not t.startswith('!F$'):
            continue
        t = t.split('﻿')[0]
        clave = ROTULO.get(t.split('\n')[0][4:].strip())
        if clave and clave not in out:
            out[clave] = t
    if 'es' not in out and 'es0' in out:
        out['es'] = out['es0']
    return {l: out.get(l, '') for l in ('en', 'es')}


def _rarr(s, d, o, n, k):
    for _ in range(k):
        d[n] = s[o]; n += 1; o += 4


def _tbr8(s, d, o, n):
    _rarr(s, d, o, n, 8); _rarr(s, d, o + 2, n + 8, 8)


def _tbr4(s, d, o, n):
    _rarr(s, d, o, n, 4); o -= 16
    _rarr(s, d, o, n + 4, 4); o += 18
    _rarr(s, d, o, n + 8, 4); o -= 16
    _rarr(s, d, o, n + 12, 4)


def orden_bmp(px, w, h):
    out = bytearray(len(px)); red = w * 2; o = 0; n = len(out) - w
    for _ in range((w * h) // ((w * 4) * 2)):
        for _ in range(2):
            for _ in range(w // 16): _tbr8(px, out, o, n); n += 16; o += 32
            n -= red
        o -= red * 2 - 17
        for _ in range(2):
            for _ in range(w // 16): _tbr4(px, out, o, n); n += 16; o += 32
            n -= red
        o -= 1
        for _ in range(w // 16): _tbr4(px, out, o, n); n += 16; o += 32
        n -= red
        for _ in range(w // 16): _tbr4(px, out, o, n); n += 16; o += 32
        o -= red * 2 + 15; n -= red
        for _ in range(2):
            for _ in range(w // 16): _tbr8(px, out, o, n); n += 16; o += 32
            n -= red
        o -= 1
    return out


def entrelazado(px):
    b = bytearray(len(px)); n = o = 0
    for _ in range(len(px) // 32):
        for _ in range(2):
            c = 0
            for _ in range(8):
                for k in range(2):
                    b[n] = px[o + c + k]; n += 1
                c += 4
            o += 2
        o += 28
    return b


def reordenar_px(px, w, h):
    out = bytearray(len(px)); np_ = op = base = col = 0; cs = 32
    cnt, cols = (w // 128, 4) if w > 128 else (1, w // 32)
    for br in range(h // 16):
        for _ in range(8):
            bco = col
            for _ in range(cnt):
                for _ in range(cols):
                    out[np_:np_ + cs] = px[op:op + cs]; op += w * 16; np_ += cs
                bco += 256; op = bco
            col += w * 2; op = col
        base += cs; col = base; op = base
        if (br + 1) % 8 == 0:
            base = 128 * w * ((br + 1) // 8) // 2; col = base; op = base
    return out


def reordenar_pal(pl):
    out = bytearray(len(pl)); bp = 0
    out[0:32] = pl[0:32]; bp = 32; op = 64
    for _ in range(7):
        out[op:op + 32] = pl[bp:bp + 32]; bp += 32; op -= 32
        out[op:op + 32] = pl[bp:bp + 32]; bp += 32; op += 64
        out[op:op + 64] = pl[bp:bp + 64]; bp += 64; op += 96
    out[op:op + 32] = pl[bp:bp + 32]; bp += 32; op -= 32
    out[op:op + 32] = pl[bp:bp + 32]; bp += 32; op += 32
    out[bp:bp + 32] = pl[bp:bp + 32]
    return out


def alfa(a):
    return min(255, a * 2 - 1) if a else 0


# (indices, paleta RGBA) o (None, pixeles RGBA) por textura; filas en el orden de la memoria del GS
def texturas_dbt(d):
    if len(d) < 0x20 or struct.unpack_from('<I', d, 4)[0] != 8:
        return None
    n, tbl = struct.unpack_from('<I', d, 0)[0], 32
    res = []
    for k in range(n):
        e = tbl + 64 * k
        if e + 64 > len(d):
            return None
        tp, pp, tl, pl = struct.unpack_from('<4I', d, e)
        tex0 = struct.unpack_from('<Q', d, e + 0x30)[0]
        psm, w, h = (tex0 >> 20) & 0x3F, 1 << ((tex0 >> 26) & 0xF), 1 << ((tex0 >> 30) & 0xF)
        tp, pp = tp * 4 + 96, pp * 4 + 96
        if not tex0 or tl <= 128 or tp + tl - 128 > len(d):
            return None
        px = bytearray(d[tp:tp + tl - 128])
        if psm in (19, 20):
            if pl <= 128 or pp + pl - 128 > len(d):
                return None
            pal = bytearray(d[pp:pp + pl - 128])
        if psm == 20:
            s = entrelazado(reordenar_px(px, w, h))
            s3 = bytearray(2 * len(s))
            s3[0::2] = bytes(b & 15 for b in s); s3[1::2] = bytes(b >> 4 for b in s)
            s4 = orden_bmp(s3, w, h)
            idx = bytearray(w * h)
            for y in range(h):
                idx[y * w:(y + 1) * w] = s4[(h - 1 - y) * w:(h - y) * w]
            res.append((w, h, bytes(idx), pal[:64]))
        elif psm == 19:
            s = orden_bmp(px, w, h)
            idx = bytearray(w * h)
            for y in range(h):
                idx[(h - 1 - y) * w:(h - y) * w] = s[y * w:(y + 1) * w]
            res.append((w, h, bytes(idx), reordenar_pal(pal)[:1024]))
        elif psm == 0:
            res.append((w, h, None, bytes(px[:w * h * 4])))
        else:
            return None
    return res


def png_rgba(ruta, w, h, px):
    filas = b''.join(b'\x00' + px[y * w * 4:(y + 1) * w * 4] for y in range(h))
    with open(ruta, 'wb') as o:
        o.write(b'\x89PNG\r\n\x1a\n' + ap.trozo_png(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
                ap.trozo_png(b'IDAT', zlib.compress(filas, 9)) + ap.trozo_png(b'IEND', b''))


# the texture entry bytes 0x28..0x2F (material slot, glow strength...) travel inside each PNG as a "bt3" tEXt chunk
def poner_meta(ruta, meta):
    with open(ruta, 'rb') as f:
        d = f.read()
    if len(d) < 33 or d[:8] != b'\x89PNG\r\n\x1a\n':
        return False
    p, out = 8, [d[:8]]
    while p + 12 <= len(d):
        n = struct.unpack_from('>I', d, p)[0]
        t = d[p + 4:p + 8]
        trozo = d[p:p + 12 + n]
        if not (t == b'tEXt' and d[p + 8:p + 12] == b'bt3\x00'):
            out.append(trozo)
        if t == b'IHDR':
            out.append(ap.trozo_png(b'tEXt', b'bt3\x00' + meta.hex().upper().encode()))
        p += 12 + n
    with open(ruta, 'wb') as o:
        o.write(b''.join(out))
    return True


def metas_dbt(d):
    if len(d) < 0x20 or struct.unpack_from('<I', d, 4)[0] != 8:
        return []
    n = struct.unpack_from('<I', d, 0)[0]
    return [d[32 + 64 * k + 0x28:32 + 64 * k + 0x30] for k in range(n) if 32 + 64 * k + 64 <= len(d)]


def abrir_dbt(carpeta, d):
    texs = texturas_dbt(d)
    if not texs:
        return False
    os.makedirs(carpeta, exist_ok=True)
    metas = metas_dbt(d)
    for k, (w, h, idx, pal) in enumerate(texs):
        ruta = os.path.join(carpeta, '%02d.png' % (k + 1))
        if idx is None:
            px = bytearray(pal)
            px[3::4] = bytes(alfa(a) for a in px[3::4])
            png_rgba(ruta, w, h, bytes(px))
        else:
            p = bytearray(pal)
            p[3::4] = bytes(alfa(a) for a in p[3::4])
            ap.escribir_png_indexado(ruta, w, h, idx, bytes(p))
        if k < len(metas):
            poner_meta(ruta, metas[k])
    return True


# characters opened before the chunk existed: their original P files (any folder) are matched to the opened 1_p folders
# by the model entry (002) and only the chunks are written; nothing else changes
def completar_metas(origen, destino):
    modelos = {}
    for r, _, fs in os.walk(origen):
        for n in fs:
            ruta = os.path.join(r, n)
            try:
                with open(ruta, 'rb') as f:
                    d = f.read()
            except OSError:
                continue
            if es_p(d):
                e = entradas(d)
                if len(e) > 16 and e[2]:
                    modelos[zlib.crc32(e[2])] = (ruta, e)
    hechos = 0
    for r, ds, fs in os.walk(destino):
        if os.path.basename(r).lower() not in ('1_p', '1_p_dmg'):
            continue
        mdl = [n for n in fs if n.startswith('002_')]
        if not mdl:
            continue
        with open(os.path.join(r, mdl[0]), 'rb') as f:
            m = modelos.get(zlib.crc32(f.read()))
        if not m:
            continue
        for k in range(11, 17):
            carpetas = sorted(x for x in ds if x.startswith('%03d_' % k) and x.lower().endswith('.dbt') and not x.lower().startswith('011_textures_dmg.'))
            if not carpetas or not m[1][k]:
                continue
            for i, meta in enumerate(metas_dbt(m[1][k])):
                png = os.path.join(r, carpetas[0], '%02d.png' % (i + 1))
                if os.path.isfile(png) and poner_meta(png, meta):
                    hechos += 1
        print('bt3: datos de texturas de %s en %s' % (os.path.basename(m[0]), os.path.relpath(r, destino).replace('\\', '/')))
    print('bt3: %d texturas completadas' % hechos)


# abre el archivo P en <misma carpeta>/<nombre sin .pak>/; devuelve la carpeta o None
# BT3 animations are always .canm (LE size, LE BPE size); the pak pads entries, kept exact; no valid header = empty
def canm(x):
    if len(x) >= 8:
        sz, zs = struct.unpack_from('<II', x, 0)
        if 0 < sz < 1 << 24 and zs and 8 + zs <= len(x):
            return x[:8 + zs]
    return b''


# BT3 pak: LE count and N+1 offsets, body and parts aligned to 64 (exact for the _eff.pak)
def pak_le(partes):
    n = len(partes)
    inicio = (4 + 4 * (n + 1) + 63) // 64 * 64
    offs, cuerpo = [], bytearray()
    for x in partes:
        offs.append(inicio + len(cuerpo))
        cuerpo += x + b'\x00' * ((-len(x)) % 64)
    offs.append(inicio + len(cuerpo))
    return struct.pack('<I', n) + struct.pack('<%dI' % (n + 1), *offs) + b'\x00' * (inicio - 4 - 4 * (n + 1)) + bytes(cuerpo)


SECCIONES = ('1_p', '2_anims', '3_effects')


# the BT3 files go straight into the TTT folders of the colour (1_p, 2_anims, 3_effects), wherever the pak was dropped
def seccion(ruta, sec):
    d = os.path.dirname(os.path.abspath(ruta))
    color = os.path.dirname(d) if os.path.basename(d).lower() in SECCIONES else d
    return os.path.join(color, sec)


def borrar(r):
    if os.path.isdir(r):
        ap.borrar_carpeta(r)
    elif os.path.exists(r):
        os.remove(r)


# entries of that folder with these leading numbers leave room for the BT3 ones; then the staged files move in
def colocar(tmp, destino, nums):
    os.makedirs(destino, exist_ok=True)
    for x in os.listdir(destino):
        if ap.prefijo_num(x) in nums:
            borrar(os.path.join(destino, x))
    for x in os.listdir(tmp):
        os.replace(os.path.join(tmp, x), os.path.join(destino, x))
    ap.borrar_carpeta(tmp)


def preparar(destino):
    tmp = destino + '.bt3'
    if os.path.isdir(tmp):
        ap.borrar_carpeta(tmp)
    os.makedirs(tmp)
    return tmp


# ---------------- move list: BT3 text -> TTT 043_move_list_name.dat + 043_move_list_<lang>.txt (rules of bt3.h move_parse) ----------------
GLIFO = {0x25CB: 0xFF6A, 0x25A1: 0xFF6B, 0x25B3: 0xFF6C, 0xD7: 0xFF6D, 0x2191: 0xFF71, 0x2193: 0xFF72, 0x2192: 0xFF73, 0x2190: 0xFF74,
         0xFF0B: 0x2B, 0xFF08: 0x28, 0xFF09: 0x29, 0xFF0F: 0x2F}


def mov_cmd(pad, holds):
    s = pad
    a = s.find('<PAD=')
    if a >= 0:
        s = s[a + 5:]
        b = s.rfind('>')
        if b >= 0:
            s = s[:b]
    o, last, i = [], 0, 0
    while i < len(s):
        ch = ord(s[i])
        g = GLIFO.get(ch, 0)
        if ch in (0xFF2C, 0xFF32) and i + 1 < len(s) and ord(s[i + 1]) == 0xFF12:
            g = 0xFF6E if ch == 0xFF2C else 0xFF6F
            i += 1
        if ch == 0x2642:
            if holds and last in (0xFF6B, 0xFF6C):
                o[-1] = chr(0xFF79 if last == 0xFF6B else 0xFF7A)
                last = 0
            i += 1
            continue
        c = g or ch
        o.append(chr(c))
        last = c
        i += 1
    return ''.join(o)


def mov_parse(s):
    out, cat, est = [], 0, {'abierto': False}

    def flush():
        if not est['abierto']:
            return
        r = est['r']
        cmd = mov_cmd(est['pad'] or est['alt'], r['cat'] != 0)
        if est['cond']:
            cmd = est['cond'] + (',\n' if len(est['cond']) > 28 else ', ') + cmd
        if est['notas'] and r['cat'] != 0:
            cmd += '\n' + est['notas']
        r['cmd'] = cmd
        if not est['oculto']:
            out.append(r)
        est['abierto'] = False

    for l in s.replace('\r', '').split('\n'):
        if len(l) > 3 and l[0] == '!' and l[2] == '&':
            k = 3
            while k < len(l) and l[k].isdigit():
                k += 1
            l = l[:2] + l[k:]
        if l.startswith('!F$'):
            flush()
            if len(l) > 3 and l[3].isdigit():
                cat = int(l[3])
            continue
        if l.startswith('!F@'):
            flush()
            break
        if len(l) >= 6 and l.startswith('!F*'):
            flush()
            dig = lambda c: int(c) if c.isdigit() else 0
            est.update(r={'cat': 2 if cat == 3 else min(cat, 2), 'a': dig(l[3]), 'b': dig(l[4]), 'c': dig(l[5]), 'name': l[6:]},
                       oculto=l[6:] in ('0', ''), cond='', notas='', pad='', alt='', abierto=True)
            continue
        if not est['abierto']:
            continue
        if len(l) >= 4 and l.startswith('!F%'):
            t = l[4:]
            if t in ('0', ''):
                continue
            if l[3] == '0':
                est['cond'] = t
            else:
                est['notas'] += ('\n' if est['notas'] else '') + t
            continue
        if len(l) >= 3 and l[0] == '!' and l[2] == '#':
            if l[1] in '1D' and not est['pad']:
                est['pad'] = l[3:]
            elif l[1] != '2' and not est['alt']:
                est['alt'] = l[3:]
    flush()
    return out


def texto(b):
    if not b:
        return ''
    if b[:2] == b'\xff\xfe':
        t = b[2:].decode('utf-16-le', errors='replace')
    else:
        t = b.decode('utf-8-sig', errors='replace')
    return t.split('\x00')[0]


# rows of another language matched to the English ones in order: same category required, same costs preferred
def botones(r):
    b = ''.join(ch for ch in r['cmd'] if 0xFF60 <= ord(ch) <= 0xFF7F or ch in '+()/')
    return b.replace(chr(0xFF79), chr(0xFF6B)).replace(chr(0xFF7A), chr(0xFF6C)).replace('()', '')   # held = plain button


# BT3 languages can list different rows (and some in another order): a pair needs the same category and the same buttons;
# first the longest match in order (same costs preferred), then the leftovers anywhere; an unmatched row keeps the English text
def alinear(base, otra):
    if len(otra) == len(base) and all(x['cat'] == y['cat'] for x, y in zip(base, otra)):
        return {i: i for i in range(len(base))}
    n, m = len(base), len(otra)
    pt = [[(2 + ((x['a'], x['b'], x['c']) == (y['a'], y['b'], y['c']))) if x['cat'] == y['cat'] and botones(x) == botones(y) else 0 for y in otra] for x in base]
    t = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(n - 1, -1, -1):
        for j in range(m - 1, -1, -1):
            t[i][j] = max(t[i + 1][j], t[i][j + 1], t[i + 1][j + 1] + pt[i][j] if pt[i][j] else 0)
    par, i, j = {}, 0, 0
    while i < n and j < m:
        if pt[i][j] and t[i][j] == t[i + 1][j + 1] + pt[i][j]:
            par[i] = j
            i += 1
            j += 1
        elif t[i][j] == t[i + 1][j]:
            i += 1
        else:
            j += 1
    usados = set(par.values())
    for i in range(n):
        if i in par:
            continue
        for j in range(m):
            if j not in usados and pt[i][j]:
                par[i] = j
                usados.add(j)
                break
    return par


# each language keeps its own rows in BT3; the records follow English and the other languages are matched row by row
def escribir_movelist(carpeta, textos):
    listas = {l: mov_parse(t) for l, t in textos.items()}
    base = listas.get('en') or next((v for v in listas.values() if v), [])
    if not base:
        return 0, {}
    dat = b''.join(struct.pack('<8i', r['cat'], 0, 0, 0, r['b'], r['c'], r['a'], 1 if r['cat'] == 0 else 0) for r in base)
    with open(os.path.join(carpeta, '043_move_list_name.dat'), 'wb') as o:
        o.write(dat + struct.pack('<i', -1) + bytes(28))
    filas = {}
    for lang in ('en', 'es'):
        otra = listas.get(lang) or []
        if not otra:
            continue
        par = alinear(base, otra)
        with open(os.path.join(carpeta, '043_move_list_%s.txt' % lang), 'w', encoding='utf-8-sig', newline='\n') as o:
            for i in sorted(par):
                o.write('[%d name]\n%s\n[%d command]\n%s\n' % (i + 1, otra[par[i]]['name'], i + 1, otra[par[i]]['cmd']))
        filas[lang] = (len(par), len(otra))
    return len(base), filas


MOVELIST = {'en': 44, 'es': 45}


# ---------------- P file -> 1_p (000..042 with the BT3 names, .dbt as folders of PNGs, move list as TTT) ----------------
def poner_p(tmp, destino, textos):
    n, filas = escribir_movelist(tmp, textos)
    colocar(tmp, destino, set(range(44)))
    print('bt3: lista de movimientos, %d tecnicas en ingles; filas emparejadas: %s' % (n, ', '.join('%s %d de %d' % (l, a, b) for l, (a, b) in filas.items())))


def abrir_p(ruta):
    with open(ruta, 'rb') as f:
        d = f.read()
    if not es_p(d):
        return None
    plant = ap.leer_plantilla(LISTA)
    if os.path.basename(os.path.dirname(os.path.abspath(ruta))).lower() == '1_p_dmg':   # damaged costume: model, faces and textures only
        destino = os.path.dirname(os.path.abspath(ruta))
        tmp = preparar(destino)
        for i, x in enumerate(entradas(d)[:17]):
            nombre = plant.get(i + 1, ('', '%03d.bin' % i))[1]
            if i < 2 or not x or (nombre.lower().endswith('.dbt') and abrir_dbt(os.path.join(tmp, nombre), x)):
                continue
            with open(os.path.join(tmp, nombre), 'wb') as o:
                o.write(x)
        os.remove(ruta)
        colocar(tmp, destino, set(range(2, 17)))
        return destino
    destino = seccion(ruta, '1_p')
    tmp = preparar(destino)
    for i, x in enumerate(entradas(d)[:43]):
        nombre = plant.get(i + 1, ('', '%03d.bin' % i))[1]
        r = os.path.join(tmp, nombre)
        if x and nombre.lower().endswith('.dbt') and abrir_dbt(r, x):
            continue
        with open(r, 'wb') as o:
            o.write(x)
    poner_p(tmp, destino, textos_movelist(d))
    os.remove(ruta)
    return destino


# ---------------- _anm.pak -> 2_anims (NNN_name.canm with the TTT names) ----------------
def abrir_anm(ruta):
    with open(ruta, 'rb') as f:
        d = f.read()
    if not es_anm(d):
        return None
    nombres = [v[1] for k, v in sorted(ap.leer_plantilla(LISTA_TTT).items()) if v[0] == '2_anims']
    destino = seccion(ruta, '2_anims')
    tmp = preparar(destino)
    for i, x in enumerate(entradas(d)):
        base = os.path.splitext(nombres[i])[0] if i < len(nombres) else '%03d' % i
        with open(os.path.join(tmp, base + '.canm'), 'wb') as o:
            o.write(canm(x))
    colocar(tmp, destino, set(range(440)))
    os.remove(ruta)
    return destino


# ---------------- _eff.pak -> 3_effects (VFX closed in BT3 form, effect_common and its cameras opened, empty patch_param) ----------------
def abrir_eff(ruta):
    with open(ruta, 'rb') as f:
        d = f.read()
    if not es_eff(d):
        return None
    top = entradas(d)
    comun = entradas(top[5]) if len(top[5]) >= 8 else []
    camaras = entradas(comun[7]) if len(comun) > 7 and len(comun[7]) >= 8 else None
    rearmado = pak_le(top[:5] + [pak_le(comun[:7] + [pak_le(camaras) if camaras is not None else comun[7]] + comun[8:]) if comun else top[5]])
    if rearmado != d:
        print('aviso: %s no coincide al rearmar; queda cerrado' % ruta)
        return None
    destino = seccion(ruta, '3_effects')
    tmp = preparar(destino)
    for i in range(5):
        with open(os.path.join(tmp, EFF_NOMBRES[i]), 'wb') as o:
            o.write(top[i])
    c = os.path.join(tmp, EFF_NOMBRES[5])
    os.makedirs(c)
    for j, x in enumerate(comun):
        r = os.path.join(c, COMUN[j] if j < len(COMUN) else '%02d.dat' % j)
        if j == 7 and camaras is not None:
            os.makedirs(r)
            for k, y in enumerate(camaras):
                with open(os.path.join(r, '%02d_camera.cma' % k), 'wb') as o:
                    o.write(y)
            continue
        with open(r, 'wb') as o:
            o.write(x)
    open(os.path.join(tmp, '06_patch_param.dat'), 'wb').close()   # TTT's seventh entry; BT3 has none
    colocar(tmp, destino, set(range(7)))
    os.remove(ruta)
    return destino


# ---------------- old layout (session 66): <section>/<name>/ subfolders -> straight into the section ----------------
def migrar(carpeta):
    sec, nombre = os.path.basename(os.path.dirname(carpeta)).lower(), os.path.basename(carpeta)
    hijos = os.listdir(carpeta)
    destino = os.path.dirname(carpeta)
    if '002_model.mdl' in hijos:
        tmp = preparar(destino)
        textos = {}
        for x in hijos:
            n = ap.prefijo_num(x)
            if n is not None and n <= 42:
                os.replace(os.path.join(carpeta, x), os.path.join(tmp, x))
            elif 'move_list_font' in x.lower():
                for l, i in MOVELIST.items():
                    if n == i:
                        with open(os.path.join(carpeta, x), 'rb') as f:
                            textos[l] = texto(f.read())
        poner_p(tmp, destino, textos)
    elif any(x.lower().endswith('.canm') for x in hijos):
        tmp = preparar(destino)
        for x in hijos:
            os.replace(os.path.join(carpeta, x), os.path.join(tmp, x))
        colocar(tmp, destino, {ap.prefijo_num(x) for x in os.listdir(tmp)} - {None})
    elif '00_skill_001.pak' in hijos or '05_effect_common.pak' in hijos:
        tmp = preparar(destino)
        for x in hijos:
            os.replace(os.path.join(carpeta, x), os.path.join(tmp, x))
        if not os.path.exists(os.path.join(tmp, '06_patch_param.dat')):
            open(os.path.join(tmp, '06_patch_param.dat'), 'wb').close()
        colocar(tmp, destino, set(range(7)))
    else:
        return False
    ap.borrar_carpeta(carpeta)
    print('bt3: carpeta antigua %s/%s pasada a %s' % (sec, nombre, sec))
    return True


# todos los archivos BT3 de un arbol (mods): devuelve cuantos abrio
def abrir_arbol(raiz):
    paks, viejas = [], []
    for dp, dirs, nombres in os.walk(raiz):
        if os.path.basename(dp).lower() in SECCIONES:
            viejas += [os.path.join(dp, x) for x in dirs if ap.prefijo_num(x) is None and not x.lower().endswith(('.vfx', '.bt3'))]
        for x in nombres:
            ruta = os.path.join(dp, x)
            if x.lower().endswith('.pak') and ap.prefijo_num(x) is None and os.path.getsize(ruta) >= 0x100:
                paks.append(ruta)
    n = 0
    for c in viejas:
        if os.path.isdir(c) and migrar(c):
            n += 1
    for ruta in paks:
        with open(ruta, 'rb') as f:
            h = f.read(16)
        if not (es_p(h) or es_anm(h) or es_eff(h)):
            continue
        t0 = time.perf_counter()
        if (abrir_p(ruta) if es_p(h) else abrir_anm(ruta) if es_anm(h) else abrir_eff(ruta)):
            print('bt3: abierto %s en %.1f s' % (os.path.relpath(ruta, raiz).replace('\\', '/'), time.perf_counter() - t0))
            n += 1
    return n


if __name__ == '__main__':
    if len(sys.argv) == 4 and sys.argv[1] == '--meta':
        completar_metas(sys.argv[2], sys.argv[3])
        sys.exit(0)
    total = 0
    for a in sys.argv[1:]:
        total += abrir_arbol(a) if os.path.isdir(a) else (1 if abrir_p(a) or abrir_anm(a) or abrir_eff(a) else 0)
    print('bt3: %d archivos BT3 abiertos' % total)
