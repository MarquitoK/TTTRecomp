#!/usr/bin/env python3
# VFX paks (TTT and BT3): open with names and PNG textures, pack back; the game keeps reading the paks
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import open_paks as ap
import bt3_open as b3

ALIN = 64
EFF_NOMBRES, COMUN = b3.EFF_NOMBRES, b3.COMUN
VFX_NOMBRES = set(EFF_NOMBRES[:5]) | set(COMUN[1:7]) | set(COMUN[8:])


# ---------------- pak (TTT BE, BT3 LE): N, N+1 offsets, body and parts aligned to 64 ----------------
def indice(d, e):
    if len(d) < 8:
        return None
    n = struct.unpack_from(e + 'I', d, 0)[0]
    if not 0 < n < 4096 or 4 + 4 * (n + 1) > len(d):
        return None
    o = struct.unpack_from(e + '%dI' % (n + 1), d, 4)
    if o[0] < 4 + 4 * (n + 1) or any(o[i] > o[i + 1] for i in range(n)) or o[n] > len(d):
        return None
    return [d[o[i]:o[i + 1]] for i in range(n)]


def endian(d):
    a, b = struct.unpack_from('<I', d, 0)[0], struct.unpack_from('>I', d, 0)[0]
    return '<' if a < b else '>'


def pak(partes, e):
    n = len(partes)
    inicio = (4 + 4 * (n + 1) + ALIN - 1) // ALIN * ALIN
    offs, cuerpo = [], bytearray()
    for x in partes:
        offs.append(inicio + len(cuerpo))
        cuerpo += x
        cuerpo += b'\x00' * ((-len(cuerpo)) % ALIN)
    offs.append(inicio + len(cuerpo))
    return struct.pack(e + 'I', n) + struct.pack(e + '%dI' % (n + 1), *offs) + b'\x00' * (inicio - 4 - 4 * (n + 1)) + bytes(cuerpo)


# ---------------- names (his VFX tool rules) ----------------
def es_tex(d):
    return len(d) >= 0x20 and d[:4] != b'V000' and struct.unpack_from('<I', d, 4)[0] == 8 and 0 < struct.unpack_from('<I', d, 0)[0] <= 64 and \
        0x20 + struct.unpack_from('<I', d, 0)[0] * 0x60 <= len(d) + 0x40


def ext_de(d, tex):
    if d[:4] == b'V000':
        return 'v00'
    if es_tex(d):
        return tex
    if len(d) == 0x140:
        return 'shd'
    if len(d) == 192:
        a, b = struct.unpack_from('<II', d, 0)
        return 'shape' if a <= 10000 and b <= 10000 else 'shd'
    if len(d) == 1216:
        return 'anim'
    if len(d) in (768, 640, 576, 448, 512):
        return 'shape'
    return 'dat'


def nombre_de(i, ext):
    return {'atex': '%02d_texture.atex', 'dbt': '%02d_texture.dbt', 'v00': '%02d_shape.v00', 'shd': '%02d_shader.shd',
            'anim': '%02d_anim.dat', 'shape': '%02d_shape.dat'}.get(ext, '%02d.dat') % i


def nombres_vfx(datos, tex):
    skill = not (len(datos[0]) >= 0x40 and datos[0][4] > 0)
    principal = 3 if skill else 0
    salida = []
    for i, d in enumerate(datos):
        if skill and i < principal:
            if i == 0:
                salida.append('00_model.pak' if (b'pMdl' in d or b'pMdF' in d or d[0x40:0x44] == b'DBES') else ('00_texture.' + tex if es_tex(d) else '00.dat'))
            else:
                salida.append('01_skill.cma' if i == 1 else '02_ground.dat')
        elif i == principal:
            salida.append('%02d_main.dat' % i)
        else:
            salida.append(nombre_de(i, ext_de(d, tex)))
    return salida


# ---------------- BT3 .dbt <-> PNG (indices and palette kept; PNG alpha 2a-1, back (a+1)//2) ----------------
def permutacion(f, n):
    cap = [f(bytes((i >> s) & 255 for i in range(n))) for s in (0, 8, 16)]
    if any(len(c) != n for c in cap):
        return None
    p = [cap[0][k] | cap[1][k] << 8 | cap[2][k] << 16 for k in range(n)]
    return p if sorted(p) == list(range(n)) else None


def aplicar_inversa(p, datos):
    out = bytearray(len(p))
    for k, src in enumerate(p):
        out[src] = datos[k]
    return bytes(out)


def dbt_entradas(d):
    n = struct.unpack_from('<I', d, 0)[0]
    salida = []
    for k in range(n):
        e = 32 + 64 * k
        tp, pp, tl, pl = struct.unpack_from('<4I', d, e)
        tex0 = struct.unpack_from('<Q', d, e + 0x30)[0]
        salida.append({'psm': (tex0 >> 20) & 0x3F, 'w': 1 << ((tex0 >> 26) & 0xF), 'h': 1 << ((tex0 >> 30) & 0xF),
                       'tp': tp * 4 + 96, 'pp': pp * 4 + 96, 'tl': tl - 128, 'pl': pl - 128})
    return salida


def abrir_dbt(carpeta, d):
    texs = b3.texturas_dbt(d)
    if texs is None or any(t[2] is None for t in texs):
        return False
    os.makedirs(carpeta)
    with open(os.path.join(carpeta, 'base.dbt'), 'wb') as o:
        o.write(d)
    for k, (w, h, idx, pal) in enumerate(texs):
        p = bytearray(pal)
        p[3::4] = bytes(b3.alfa(a) for a in p[3::4])
        ap.escribir_png_indexado(os.path.join(carpeta, '%02d.png' % (k + 1)), w, h, idx, bytes(p))
    return True


def empaquetar_dbt(carpeta):
    with open(os.path.join(carpeta, 'base.dbt'), 'rb') as f:
        d = bytearray(f.read())
    for k, t in enumerate(dbt_entradas(d)):
        png = os.path.join(carpeta, '%02d.png' % (k + 1))
        r = ap.leer_png_indexado(png) if os.path.isfile(png) else None
        if not r:
            raise SystemExit('%s: falta o no es un PNG indexado' % png)
        w, h, idx, pal = r
        n = 16 if t['psm'] == 20 else 256
        if (w, h) != (t['w'], t['h']) or len(pal) // 4 > n or max(idx) >= n:
            raise SystemExit('%s: debe ser %dx%d con %d colores como maximo' % (png, t['w'], t['h'], n))
        pal = bytearray(pal.ljust(4 * n, b'\x00'))
        pal[3::4] = bytes((a + 1) // 2 for a in pal[3::4])
        W, H = w, h
        if t['psm'] == 19:
            s = bytes(idx[(H - 1 - (i // W)) * W + i % W] for i in range(W * H))
            pb = permutacion(lambda x: b3.orden_bmp(x, W, H), W * H)
            pp = permutacion(b3.reordenar_pal, 1024)
            if pb is None or pp is None:
                raise SystemExit('%s: formato de textura no soportado para reempaquetar' % png)
            raw, praw = aplicar_inversa(pb, s), aplicar_inversa(pp, bytes(pal))
        else:
            s4 = bytes(idx[(H - 1 - (i // W)) * W + i % W] for i in range(W * H))
            pb = permutacion(lambda x: b3.orden_bmp(x, W, H), W * H)
            ns = W * H // 2
            pr = permutacion(lambda x: b3.entrelazado(b3.reordenar_px(x, W, H)), ns)
            if pb is None or pr is None:
                raise SystemExit('%s: formato de textura no soportado para reempaquetar' % png)
            s3 = aplicar_inversa(pb, s4)
            s = bytes((s3[2 * i] & 15) | (s3[2 * i + 1] & 15) << 4 for i in range(ns))
            raw, praw = aplicar_inversa(pr, s), bytes(pal[:64])
        if len(raw) != t['tl'] or len(praw) > t['pl']:
            raise SystemExit('%s: tamano distinto al original' % png)
        d[t['tp']:t['tp'] + len(raw)] = raw
        d[t['pp']:t['pp'] + len(praw)] = praw
    return bytes(d)


# ---------------- open ----------------
def escribir(ruta, d):
    with open(ruta, 'wb') as o:
        o.write(d)


def abrir_vfx(carpeta, d, e):
    datos = indice(d, e)
    if datos is None:
        return False
    tex = 'atex' if e == '>' else 'dbt'
    os.makedirs(carpeta)
    for nombre, x in zip(nombres_vfx(datos, tex), datos):
        ruta = os.path.join(carpeta, nombre)
        if nombre.endswith('.dbt') and x and abrir_dbt(ruta, x):
            continue
        escribir(ruta, x)
        if nombre.endswith('.atex') and x:
            ap.abrir_tex(ruta, x)
    return True


def abrir_camaras(carpeta, d, e):
    datos = indice(d, e)
    if datos is None:
        return False
    os.makedirs(carpeta)
    for i, x in enumerate(datos):
        escribir(os.path.join(carpeta, '%02d_camera.cma' % i), x)
    return True


def abrir_comun(carpeta, d, e):
    datos = indice(d, e)
    if datos is None:
        return False
    os.makedirs(carpeta)
    for i, x in enumerate(datos):
        nombre = COMUN[i] if i < len(COMUN) else '%02d.dat' % i
        ruta = os.path.join(carpeta, nombre)
        if x and nombre == '07_skill_cameras.pak' and abrir_camaras(ruta, x, e):
            continue
        if x and nombre in VFX_NOMBRES and abrir_vfx(ruta, x, e):
            continue
        escribir(ruta, x)
    return True


# TTT VFX open in place (the game reads the folder); BT3 VFX (LE) open in <name>.vfx, the game keeps the closed pak
def abrir(ruta):
    with open(ruta, 'rb') as f:
        d = f.read()
    nombre = os.path.basename(ruta)
    if b3.es_p(d[:16]) or b3.es_anm(d[:16]) or b3.es_eff(d[:16]):
        raise SystemExit('%s: no es un pak de VFX (los archivos BT3 de personaje se abren con tools/bt3_open.py)' % ruta)
    e = endian(d)
    comun = nombre.lower().endswith('_effect_common.pak')
    if e == '<':
        carpeta = os.path.splitext(ruta)[0] + '.vfx'
        if os.path.isdir(carpeta):
            ap.borrar_carpeta(carpeta)
        if not (abrir_comun(carpeta, d, e) if comun else abrir_vfx(carpeta, d, e)):
            raise SystemExit('%s: no es un pak de VFX' % ruta)
        return carpeta, d, e
    tmp = ruta + '.abriendo'
    if os.path.exists(tmp):
        ap.borrar_carpeta(tmp)
    if not (abrir_comun(tmp, d, e) if comun else abrir_vfx(tmp, d, e)):
        raise SystemExit('%s: no es un pak de VFX' % ruta)
    os.remove(ruta)
    os.replace(tmp, ruta)
    return ruta, d, e


# ---------------- pack ----------------
def partes_de(carpeta):
    salida = []
    for x in os.listdir(carpeta):
        if x.lower() in ap.IGNORAR or x.startswith('.'):
            continue
        s = ap.png_suelto(x)
        if s:
            x = s[0]
        n = ap.prefijo_num(x)
        if n is not None and x not in [y for _, y in salida]:
            salida.append((n, x))
    return [x for _, x in sorted(salida)]


def armar(ruta, e):
    nombre = os.path.basename(ruta).lower()
    if nombre.endswith('.dbt') and os.path.isdir(ruta):
        return empaquetar_dbt(ruta)
    if nombre.endswith('.atex') and not os.path.isfile(ruta):
        return ap.empaquetar_tex(ruta) or b''
    if os.path.isdir(ruta):
        return pak([armar(os.path.join(ruta, x), e) for x in partes_de(ruta)], e)
    with open(ruta, 'rb') as f:
        return f.read()


def empaquetar(carpeta):
    carpeta = carpeta.rstrip('/\\')
    if carpeta.lower().endswith('.vfx'):
        d = armar(carpeta, '<')
        destino = carpeta[:-4] + '.pak'
        escribir(destino, d)
        return destino, d
    if os.path.basename(carpeta) not in VFX_NOMBRES and not carpeta.lower().endswith('_effect_common.pak'):
        raise SystemExit('%s: no es una carpeta de VFX abierta' % carpeta)
    d = armar(carpeta, '>')
    tmp = carpeta + '.empaquetando'
    escribir(tmp, d)
    ap.borrar_carpeta(carpeta)
    os.replace(tmp, carpeta)
    return carpeta, d


def mismo(a, b, e):
    x, y = indice(a, e), indice(b, e)
    if x is None or y is None or len(x) != len(y):
        return False
    for p, q in zip(x, y):
        if p.rstrip(b'\x00') == q.rstrip(b'\x00'):
            continue
        if e == '>' and es_tex(p) and ap.mismo_tex('x.atex', p, q):
            continue
        if not (indice(p, e) and mismo(p, q, e)):
            return False
    return True


def vfx_en(raiz):
    salida = []
    for dp, dirs, nombres in os.walk(raiz):
        dirs[:] = [x for x in dirs if not x.lower().endswith('.vfx')]
        for x in nombres:
            if x in VFX_NOMBRES or x.endswith('_effect_common.pak'):
                salida.append(os.path.join(dp, x))
    return sorted(salida)


def uso():
    print('uso: python3 tools/vfx.py <pak de VFX o carpeta> [...]  abre los VFX: TTT en su lugar, BT3 en <nombre>.vfx')
    print('     python3 tools/vfx.py --empaquetar <carpeta> [...]  vuelve a armar el .pak (TTT en su lugar, BT3 <nombre>.vfx -> <nombre>.pak)')


if __name__ == '__main__':
    args = sys.argv[1:]
    if not args:
        uso()
        sys.exit(1)
    if args[0] == '--empaquetar':
        for c in args[1:]:
            destino, d = empaquetar(c)
            print('vfx: armado %s (%d bytes)' % (destino, len(d)))
    else:
        for r in [y for x in args for y in (vfx_en(x) if os.path.isdir(x) else [x])]:
            if os.path.getsize(r) < 8:
                print('vfx: %s vacio, sin abrir' % r)
                continue
            carpeta, d, e = abrir(r)
            rearmado = armar(carpeta, e)
            print('vfx: abierto %s%s' % (carpeta, '' if mismo(d, rearmado, e) else ' (aviso: al rearmar no coincide)'))
