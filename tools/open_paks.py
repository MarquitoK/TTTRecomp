#!/usr/bin/env python3
# Despliega los paks del PACKFILE en carpetas con los nombres de las plantillas. La carpeta es la
# unica verdad: el orden y la cantidad de entradas salen de sus archivos, sin metadatos.
#   --sugerir-reglas   lista lo que ninguna regla ni plantilla resuelve
#   --abrir            despliega los paks con plantilla y, si el contenido coincide, borra el .pak
#   --conservar        deja el .pak junto a la carpeta
#   --abrir-tex        despliega los .atex y .rhg de las carpetas ya abiertas como PNG indexados (sin copia del original)
#   --rehacer-tex      vuelve a generar los png de las carpetas .atex/.rhg sin editar
#   --empaquetar       vuelve a generar los .pak desde las carpetas y borra las carpetas
#   --limpiar          borra las carpetas de paks que aun conservan su .pak
# Contenedores anidados (.pak o .txt1 dentro de una carpeta) se abren como subcarpeta con su nombre
# completo, extension incluida. Un .txt1 abierto es una carpeta con _txt1.bin y texto.txt (UTF-8, una
# cadena tras cada linea [N]); las carpetas viejas con N-HEX.txt se aceptan y --unir-txt1 las convierte.
import argparse, os, re, struct, fnmatch, collections, shutil, time, tempfile, zlib

CONTENEDORES = ('.pak', '.txt1', '.vagp', '.atex', '.rhg')
TEXTURAS = ('.atex', '.rhg')
TXT1_CAB = 0x48

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
ALINEACION = 64          # inicio del cuerpo y paso entre entradas: los contenedores rearmados pierden su relleno original
IGNORAR = {'thumbs.db', 'desktop.ini', '_pak.txt', '_cola.bin', '_txt1.bin', '_vagp.txt', '_tex.txt', 'texto.txt', 'config.set', 'preview_txts.txt',
           '043_move_list_en.txt', '043_move_list_es.txt', '1_p_dmg'}
FRANCES = re.compile(r'_fr(?=[/_.]|$)', re.I)


# French is not part of the port: only a _fr name part counts (never an unsuffixed name: the nested char_select/map_select
# paks of every language menu have no suffix)
def es_frances(ruta):
    return bool(FRANCES.search(ruta.replace('\\', '/')))


def clave_natural(ruta):
    return [[(0, int(t)) if t.isdigit() else (1, t.lower()) for t in re.split(r'(\d+)', parte)] for parte in ruta.replace('\\', '/').split('/')]


def clave_parte(ruta):
    r = ruta.replace('\\', '/')
    return clave_natural(r.rsplit('/', 1)[-1]) + clave_natural(r)


# orden de las partes de un pak: si cada archivo tiene un numero inicial propio, ese numero manda y la seccion
# es solo una subcarpeta; si las secciones reinician la numeracion, manda la ruta completa
def orden_partes(rutas):
    nums = [prefijo_num(r.replace('\\', '/').rsplit('/', 1)[-1]) for r in rutas]
    tpal = lambda r: r.lower().endswith('.tpal')
    if None not in nums and len(set(nums)) == len(nums):
        return sorted(rutas, key=lambda r: (tpal(r), clave_parte(r)))
    return sorted(rutas, key=lambda r: (tpal(r), clave_natural(r)))


def leer_plantilla(ruta):
    entradas, seccion = {}, ''
    for linea in open(ruta, encoding='utf-8-sig', errors='replace'):
        linea = linea.strip()
        if not linea:
            continue
        m = re.match(r'(\d+)-[0-9A-Fa-f]+:\s*(.+)$', linea)
        if m:
            entradas[int(m.group(1))] = (seccion, m.group(2).strip().rstrip('. '))
        elif linea.endswith(':'):
            seccion = linea[:-1].strip()
    return entradas


def leer_plantillas(dirp):
    p = {}
    if os.path.isdir(dirp):
        for nombre in sorted(os.listdir(dirp)):
            if nombre.endswith('.txt'):
                p[nombre[:-4]] = leer_plantilla(os.path.join(dirp, nombre))
    return p


def analizar(d, tam=None):
    if tam is None:
        tam = len(d)
    if tam < 12:
        return None
    n = struct.unpack_from('>I', d, 0)[0]
    if not (1 <= n <= 4000) or 4 + 4 * (n + 1) > tam:
        return None
    for extra in (1, 0):
        offs = list(struct.unpack_from('>%dI' % (n + extra), d, 4))
        if offs[0] < 4 + 4 * (n + extra) or offs[0] > tam:
            continue
        if any(offs[i] > offs[i + 1] for i in range(len(offs) - 1)) or offs[-1] > tam:
            continue
        if extra and not (tam - 2048 < offs[-1] <= tam):
            continue
        return {'n': n, 'limites': offs + ([] if extra else [tam])}
    return None


def es_contenedor(nombre):
    return nombre.lower().endswith(CONTENEDORES)


# txt1: cabecera de 0x48 bytes, tabla de desplazamientos little endian y cadenas UTF-16 terminadas en 0
def analizar_txt1(d):
    if len(d) < TXT1_CAB + 4 or d[:2] != b'\x64\x00':
        return None
    primero = struct.unpack_from('<I', d, TXT1_CAB)[0]
    if primero < TXT1_CAB + 4 or primero > len(d) or (primero - TXT1_CAB) % 4:
        return None
    offs = []
    for pos in range(TXT1_CAB, primero, 4):
        v = struct.unpack_from('<I', d, pos)[0]
        if not v:
            break
        offs.append(v)
    if not offs or any(offs[i] > offs[i + 1] for i in range(len(offs) - 1)) or offs[-1] > len(d):
        return None
    return offs


def cortar_utf16(d):
    for j in range(0, len(d) & ~1, 2):
        if d[j] == 0 and d[j + 1] == 0:
            return d[:j]
    return d[:len(d) & ~1]


def texto_a_utf16(d):
    if d[:2] == b'\xff\xfe':
        out = d[2:]
    elif d[:2] == b'\xfe\xff':
        out = d[2:].decode('utf-16-be').encode('utf-16-le')
    else:
        if d[:3] == b'\xef\xbb\xbf':
            d = d[3:]
        try:
            out = d.decode('utf-8').encode('utf-16-le')
        except UnicodeDecodeError:
            out = d.decode('latin-1').encode('utf-16-le')
    out = out.replace(b'\r\x00', b'')
    if len(out) % 2:
        out = out[:-1]
    if out[-2:] != b'\x00\x00':
        out += b'\x00\x00'
    return out


# the txt1 header is rebuilt from its first id and the language slots it holds (2 en, 4 es; 3 is the game's French),
# listed in assets/txt1.txt; a header that does not follow that pattern is kept there in hex
TXT1_IDIOMAS = {2: 'en', 3: 'fr', 4: 'es'}


def cabecera_de(base, n, idiomas):
    h = [0x64, base, (base + n - 1) & 0xFFFF] + [0] * 33
    acum = 0
    for i in range(17):
        c = n if i in idiomas else 0
        acum += c
        if 3 + 2 * i < 36:
            h[3 + 2 * i] = c & 0xFFFF
        if 4 + 2 * i < 36:
            h[4 + 2 * i] = acum & 0xFFFF
    return struct.pack('<36H', *h)


def idiomas_de(cab):
    base, ultimo = struct.unpack_from('<HH', cab, 2)
    n = (ultimo - base + 1) & 0xFFFF
    idiomas = [i for i in range(17) if struct.unpack_from('<H', cab, 6 + 4 * i)[0] == n]
    return (base, idiomas) if idiomas and cabecera_de(base, n, idiomas) == bytes(cab[:TXT1_CAB]) else None


def raiz_txt1(carpeta):
    partes = os.path.abspath(carpeta).replace('\\', '/').split('/')
    if 'Packfile' not in partes:
        return None, None
    k = len(partes) - 1 - partes[::-1].index('Packfile')
    raiz = '/'.join(partes[:k])
    if not os.path.isfile(os.path.join(raiz, 'manifiesto.txt')):
        return None, None
    return raiz, '/'.join(partes[k + 1:])


_indice_txt1 = {}


def indice_txt1(raiz):
    ruta = os.path.join(raiz, 'txt1.txt')
    fecha = os.path.getmtime(ruta) if os.path.isfile(ruta) else 0
    guardado = _indice_txt1.get(raiz)
    if guardado and guardado[0] == fecha:
        return guardado[1]
    tabla = {}
    if fecha:
        for linea in open(ruta, encoding='utf-8'):
            c = linea.rstrip('\r\n').split('\t')
            if len(c) == 3 and not linea.startswith('#'):
                tabla[c[0].lower()] = (c[1], c[2])
    _indice_txt1[raiz] = (fecha, tabla)
    return tabla


def guardar_cabecera(ruta, cab):
    raiz, rel = raiz_txt1(ruta)
    if not raiz:
        with open(os.path.join(ruta, '_txt1.bin'), 'wb') as o:
            o.write(cab)
        return
    r = idiomas_de(cab)
    if r:
        linea = '%s\t%d\t%s\n' % (rel, r[0], ' '.join(TXT1_IDIOMAS.get(i, str(i)) for i in r[1]))
    else:
        linea = '%s\traw\t%s\n' % (rel, bytes(cab).hex())
    ruta_i = os.path.join(raiz, 'txt1.txt')
    nuevo = not os.path.isfile(ruta_i)
    with open(ruta_i, 'a', encoding='utf-8', newline='\n') as o:
        if nuevo:
            o.write('# Packfile path\tfirst id\tlanguages (or raw and the header in hex)\n')
        o.write(linea)


def abrir_txt1(ruta, d):
    offs = analizar_txt1(d)
    if not offs:
        return None
    if os.path.isdir(ruta) and not borrar_carpeta(ruta):
        return None
    if os.path.isfile(ruta):
        os.remove(ruta)
    os.makedirs(ruta)
    guardar_cabecera(ruta, d[:TXT1_CAB])
    textos = []
    for i, ini in enumerate(offs):
        fin = offs[i + 1] if i + 1 < len(offs) else len(d)
        textos.append(cortar_utf16(d[ini:fin]).decode('utf-16-le', 'replace').replace('\r', ''))
    escribir_texto(ruta, textos)
    return ruta


# txt1 unido: <nombre>.txt dentro de <nombre>.txt1, "[N]" en su propia linea y debajo la cadena N
def ruta_texto(carpeta):
    nombre = os.path.basename(os.path.normpath(carpeta))
    if nombre.lower().endswith('.txt1'):
        nombre = nombre[:-5]
    propio = os.path.join(carpeta, nombre + '.txt')
    viejo = os.path.join(carpeta, 'texto.txt')
    return propio if os.path.isfile(propio) or not os.path.isfile(viejo) else viejo


def escribir_texto(carpeta, textos):
    with open(ruta_texto(carpeta), 'w', encoding='utf-8-sig', newline='\n') as o:
        for i, t in enumerate(textos):
            o.write('[%d]\n%s\n' % (i + 1, t))


def leer_texto(carpeta):
    with open(ruta_texto(carpeta), 'rb') as f:
        d = f.read()
    if d[:2] == b'\xff\xfe':
        s = d[2:].decode('utf-16-le', 'replace')
    else:
        s = d[3:].decode('utf-8', 'replace') if d[:3] == b'\xef\xbb\xbf' else d.decode('utf-8', 'replace')
    s = s.replace('\r', '')
    if not s.startswith('[1]\n'):
        raise ValueError('%s: texto.txt no empieza por [1]' % carpeta)
    textos, pos, n = [], 4, 1
    while True:
        marca = '\n[%d]\n' % (n + 1)
        fin = s.find(marca, pos - 1)
        if fin < 0:
            f = len(s)
            if f > pos and s[f - 1] == '\n':
                f -= 1
            textos.append(s[pos:max(f, pos)])
            return textos
        textos.append(s[pos:max(fin, pos)])
        pos, n = fin + len(marca), n + 1


def listar_txt1(carpeta):
    nums = {}
    for x in os.listdir(carpeta):
        m = re.match(r'^(\d+)-[0-9A-Fa-f]+\.txt$', x, re.I)
        if m:
            nums[int(m.group(1))] = x
    n = max(nums) if nums else 0
    faltan = [i for i in range(1, n + 1) if i not in nums]
    if faltan:
        raise ValueError('%s: faltan %s (los ids son posiciones, un hueco corre los siguientes)' % (carpeta, ', '.join('%d-%X.txt' % (i, i) for i in faltan[:10])))
    return [nums[i] for i in range(1, n + 1)]


# la cabecera guarda el id base en +2, el ultimo id en +4 y la cantidad repetida mas adelante
def cabecera_txt1(carpeta, n):
    propio = os.path.join(carpeta, '_txt1.bin')
    if not os.path.isfile(propio):
        raiz, rel = raiz_txt1(carpeta)
        e = indice_txt1(raiz).get(rel.lower()) if raiz else None
        if not e:
            raise ValueError('%s: sin cabecera en txt1.txt' % carpeta)
        if e[0] == 'raw':
            cab = bytearray.fromhex(e[1])
        else:
            inv = {v: k for k, v in TXT1_IDIOMAS.items()}
            idiomas = [inv[x] if x in inv else int(x) for x in e[1].split()]
            return cabecera_de(int(e[0]), n // max(1, len(idiomas)), idiomas)
    else:
        cab = bytearray(open(propio, 'rb').read()[:TXT1_CAB])
    base, ultimo = struct.unpack_from('<HH', cab, 2)
    viejo = ultimo - base + 1
    for pos in range(4, TXT1_CAB - 1, 2):
        v = struct.unpack_from('<H', cab, pos)[0]
        if pos == 4 or v == ultimo:
            struct.pack_into('<H', cab, pos, base + n - 1)
        elif v == viejo:
            struct.pack_into('<H', cab, pos, n)
    return bytes(cab)


def empaquetar_txt1(carpeta):
    if os.path.isfile(ruta_texto(carpeta)):
        piezas = [t.encode('utf-8') for t in leer_texto(carpeta)]
    else:
        piezas = []
        for nombre in listar_txt1(carpeta):
            with open(os.path.join(carpeta, nombre), 'rb') as f:
                piezas.append(f.read())
    n = len(piezas)
    cuerpo, offs = bytearray(), []
    for pieza in piezas:
        offs.append(TXT1_CAB + 4 * n + len(cuerpo))
        cuerpo += texto_a_utf16(pieza)
    return cabecera_txt1(carpeta, n) + struct.pack('<%dI' % n, *offs) + bytes(cuerpo)


# ---------------- texturas ATEX / RHG a PNG ----------------
# Sin metadatos: la estructura del contenedor se genera desde los png (lo que el juego lee de las cabeceras
# sale del tamano y la paleta de cada imagen). Un contenedor de una textura es un png suelto (nombre.atex.png).
# NN@2x.png o nombre.rhg@2x.png: la imagen esta al doble del tamano que ve el juego.
TAB_LO = bytes(b & 15 for b in range(256))
TAB_HI = bytes(b >> 4 for b in range(256))
RE_ESCALA = re.compile(r'@(\d+)x$', re.I)


def al(x, a):
    return (x + a - 1) // a * a


def pot2(x):
    p = 1
    while p < x:
        p <<= 1
    return p


# modelos y efectos guardan el alfa en 0..0x80 (el juego lo duplica al mezclar); el png lo lleva en 0..255
def escala_psp(ruta):
    r = ruta.replace('\\', '/').lower()
    b = r.rsplit('/', 1)[-1]
    return b.endswith('.atex') and (b.endswith('_textures.atex') or b.endswith('_texture.atex') or '/4_effects/' in r)


# 'x.rhg.png' / 'x.rhg@2x.png' -> ('x.rhg', 2); None si no es una textura suelta
def png_suelto(nombre):
    if not nombre.lower().endswith('.png'):
        return None
    base, esc = nombre[:-4], 1
    m = RE_ESCALA.search(base)
    if m:
        base, esc = base[:m.start()], max(1, int(m.group(1)))
    return (base, esc) if base.lower().endswith(TEXTURAS) else None


def png_de_textura(nombre):
    base, esc = nombre[:-4], 1
    m = RE_ESCALA.search(base)
    if m:
        base, esc = base[:m.start()], max(1, int(m.group(1)))
    return prefijo_num(base), esc


def textura_suelta(ruta):
    carpeta, nom = os.path.split(ruta)
    if not os.path.isdir(carpeta or '.'):
        return None
    for x in os.listdir(carpeta or '.'):
        r = png_suelto(x)
        if r and r[0].lower() == nom.lower() and os.path.isfile(os.path.join(carpeta, x)):
            return os.path.join(carpeta, x), r[1]
    return None


def pngs_de(ruta):
    if os.path.isdir(ruta):
        salida = []
        for x in os.listdir(ruta):
            if x.lower().endswith('.png') and os.path.isfile(os.path.join(ruta, x)):
                n, esc = png_de_textura(x)
                if n is not None:
                    salida.append((n, os.path.join(ruta, x), esc))
        return [(p, e) for n, p, e in sorted(salida)]
    s = textura_suelta(ruta)
    return [s] if s else []


def trozo_png(t, c):
    return struct.pack('>I', len(c)) + t + c + struct.pack('>I', zlib.crc32(t + c) & 0xFFFFFFFF)


def escribir_png_indexado(ruta, w, h, indices, paleta):
    filas = b''.join(b'\x00' + bytes(indices[y * w:(y + 1) * w]) for y in range(h))
    plte = b''.join(paleta[i * 4:i * 4 + 3] for i in range(len(paleta) // 4))
    trns = bytes(paleta[i * 4 + 3] for i in range(len(paleta) // 4))
    with open(ruta, 'wb') as o:
        o.write(b'\x89PNG\r\n\x1a\n' + trozo_png(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 3, 0, 0, 0)) + trozo_png(b'PLTE', plte) +
                trozo_png(b'tRNS', trns) + trozo_png(b'IDAT', zlib.compress(filas, 9)) + trozo_png(b'IEND', b''))


def leer_png_indexado(ruta):
    with open(ruta, 'rb') as f:
        d = f.read()
    p, idat, plte, trns, cab = 8, b'', b'', b'', None
    while p + 8 <= len(d):
        n = struct.unpack_from('>I', d, p)[0]
        t = d[p + 4:p + 8]
        c = d[p + 8:p + 8 + n]
        if t == b'IHDR':
            cab = struct.unpack('>IIBBBBB', c)
        elif t == b'PLTE':
            plte = c
        elif t == b'tRNS':
            trns = c
        elif t == b'IDAT':
            idat += c
        p += 12 + n
    if not cab or cab[3] != 3 or cab[2] != 8 or cab[6]:
        return None
    w, h = cab[0], cab[1]
    cr = zlib.decompress(idat)
    idx, ant = bytearray(), bytearray(w)
    for y in range(h):
        ft, fila = cr[y * (w + 1)], bytearray(cr[y * (w + 1) + 1:(y + 1) * (w + 1)])
        if ft:
            for i in range(w):
                a, b, c = fila[i - 1] if i else 0, ant[i], ant[i - 1] if i else 0
                if ft == 1: fila[i] = (fila[i] + a) & 255
                elif ft == 2: fila[i] = (fila[i] + b) & 255
                elif ft == 3: fila[i] = (fila[i] + (a + b) // 2) & 255
                elif ft == 4:
                    pp = a + b - c
                    pa, pb, pc = abs(pp - a), abs(pp - b), abs(pp - c)
                    fila[i] = (fila[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        idx += fila
        ant = fila
    pal = b''.join(plte[i * 3:i * 3 + 3] + bytes([trns[i] if i < len(trns) else 255]) for i in range(len(plte) // 3))
    return w, h, bytes(idx), pal


# indices del PSP: bloques de 16 bytes x 8 filas cuando el paso y el alto lo permiten, lineal si no
def desswizzle(datos, paso, h):
    if paso % 16 or h % 8:
        return bytes(datos[:paso * h])
    out = bytearray(paso * h)
    k, bloques = 0, paso // 16
    for by in range(0, h, 8):
        for bx in range(bloques):
            for r in range(8):
                o = (by + r) * paso + bx * 16
                out[o:o + 16] = datos[k:k + 16]
                k += 16
    return bytes(out)


def swizzle(lin, paso, h):
    if paso % 16 or h % 8:
        return bytes(lin)
    out = bytearray(paso * h)
    k, bloques = 0, paso // 16
    for by in range(0, h, 8):
        for bx in range(bloques):
            for r in range(8):
                o = (by + r) * paso + bx * 16
                out[k:k + 16] = lin[o:o + 16]
                k += 16
    return bytes(out)


def indices_lineales(datos, w, h, bpp):
    paso = w // 2 if bpp == 4 else w
    lin = desswizzle(bytes(datos).ljust(paso * h, b'\x00'), paso, h)
    if bpp == 8:
        return lin
    idx = bytearray(w * h)
    idx[0::2] = lin.translate(TAB_LO)
    idx[1::2] = lin.translate(TAB_HI)
    return bytes(idx)


def indices_psp(idx, w, h, bpp):
    if bpp == 4:
        lin = bytes((a & 15) | ((b & 15) << 4) for a, b in zip(idx[0::2], idx[1::2]))
        return swizzle(lin, w // 2, h)
    return swizzle(idx, w, h)


def recortar(idx, w, tw, th):
    return b''.join(idx[y * w:y * w + tw] for y in range(th)) if tw != w else bytes(idx[:w * th])


def extender(idx, tw, th, w, h):
    filas = [idx[y * tw:(y + 1) * tw] + idx[y * tw + tw - 1:y * tw + tw] * (w - tw) for y in range(th)]
    return b''.join(filas + [filas[-1]] * (h - th))


def alfa_doble(pal):
    p = bytearray(pal)
    p[3::4] = bytes(min(255, a * 2) for a in p[3::4])
    return bytes(p)


def alfa_medio(pal):
    p = bytearray(pal)
    p[3::4] = bytes((a + 1) // 2 for a in p[3::4])
    return bytes(p)


# ATEX: cantidad, tabla/4 (siempre 0x20); entradas de 0x60 con datos/4, paleta/4, tamanos, +0x28 log2 alto,
# +0x29 log2 ancho, +0x2B formato (8 CLUT4, 9 CLUT8), +0x34/+0x38 filtro y repeticion (0xFFFF = por defecto)
def analizar_atex(d):
    if len(d) < 0x20:
        return []
    n, tabla = struct.unpack_from('<II', d, 0)
    tabla *= 4
    if n == 0 or n > 512 or tabla < 8 or tabla + n * 0x60 > len(d):
        return []
    salida = []
    for i in range(n):
        e = tabla + i * 0x60
        io, po, it, pt = struct.unpack_from('<IIII', d, e)
        w, h = 1 << d[e + 0x29], 1 << d[e + 0x28]
        bpp = 4 if d[e + 0x2B] == 8 or w * h // 2 <= it < w * h else 8
        if io * 4 + it > len(d) or po * 4 + pt > len(d) or it < (w * h * bpp) // 8 or not pt:
            return []
        salida.append({'w': w, 'h': h, 'tw': w, 'th': h, 'off': io * 4, 'tam': it, 'pal': po * 4, 'pal_tam': pt, 'bpp': bpp})
    return salida


# RHG (segun el cargador del juego en 0x0882C050): TEX de 40 bytes en 0x30, DAT de 36 y PAL de 32 alineadas
# a 16; cada bloque de pixeles y cada paleta empieza alineado a 128 desde el inicio del archivo
def analizar_rhg(d):
    if len(d) < 0x30 or d[:4] != b'RHG\x00':
        return []
    ntex, ndat, npal = struct.unpack_from('<I', d, 0x14)[0], struct.unpack_from('<I', d, 0x1C)[0], struct.unpack_from('<I', d, 0x24)[0]
    if ntex == 0 or ntex + ndat + npal > 65535:
        return []
    bd = 0x30 + al(ntex * 40, 16)
    bp = bd + al(ndat * 36, 16)
    cur = bp + npal * 32
    if cur > len(d):
        return []
    dat, pal = [], []
    for k in range(ndat):
        _m, fmt, w, h, _p, tam = struct.unpack_from('<6I', d, bd + k * 36)
        cur = al(cur, 128)
        dat.append((fmt, w, h, cur, tam))
        cur += tam
    for k in range(npal):
        _m, _p, tam, colores = struct.unpack_from('<4I', d, bp + k * 32)
        cur = al(cur, 128)
        pal.append((cur, tam, colores))
        cur += tam
    if cur > len(d):
        return []
    salida = []
    for k in range(ntex):
        di, pi = struct.unpack_from('<II', d, 0x30 + k * 40 + 4)
        tw, th = struct.unpack_from('<II', d, 0x30 + k * 40 + 20)
        if not (0 <= di < ndat and 0 <= pi < npal):
            return []
        fmt, w, h, off, tam = dat[di]
        po, pt, colores = pal[pi]
        if not pt or not tam:
            return []
        bpp = 4 if fmt == 0x12 or (fmt != 0x11 and colores <= 16) else 8
        if not tw or not th or tw > w or th > h:
            tw, th = w, h
        salida.append({'w': w, 'h': h, 'tw': tw, 'th': th, 'off': off, 'tam': tam, 'pal': po, 'pal_tam': pt, 'bpp': bpp})
    return salida


def analizar_tex(nombre, d):
    return analizar_rhg(d) if nombre.lower().endswith('.rhg') else analizar_atex(d)


# texturas del juego: (ancho, alto, bpp, indices ancho*alto, paleta RGBA con el alfa del contenedor)
def texturas_de(nombre, d):
    salida = []
    for t in analizar_tex(nombre, d):
        idx = recortar(indices_lineales(d[t['off']:t['off'] + t['tam']], t['w'], t['h'], t['bpp']), t['w'], t['tw'], t['th'])
        salida.append((t['tw'], t['th'], t['bpp'], idx, bytes(d[t['pal']:t['pal'] + t['pal_tam']])))
    return salida


def construir_atex(texs):
    n = len(texs)
    d = bytearray(0x20 + n * 0x60)
    cur, blobs = len(d), bytearray()
    for i, (tw, th, bpp, idx, pal) in enumerate(texs):
        w, h = pot2(tw), pot2(th)
        datos = indices_psp(extender(idx, tw, th, w, h), w, h, bpp)
        pal = pal.ljust(64 if bpp == 4 else 1024, b'\x00')[:64 if bpp == 4 else 1024]
        e = 0x20 + i * 0x60
        struct.pack_into('<6I', d, e, cur // 4, (cur + len(datos)) // 4, len(datos), len(pal), len(pal) // 4, 1 if bpp == 4 else 4)
        d[e + 0x28:e + 0x2C] = bytes([h.bit_length() - 1, w.bit_length() - 1, 0, 8 if bpp == 4 else 9])
        d[e + 0x34:e + 0x40] = b'\xff' * 12
        blobs += datos + pal
        cur += len(datos) + len(pal)
    struct.pack_into('<4I', d, 0, n, 8, struct.unpack_from('<I', d, 0x30)[0], struct.unpack_from('<I', d, 0x34)[0])
    return bytes(d + blobs)


def construir_rhg(texs):
    n = len(texs)
    bd = 0x30 + al(n * 40, 16)
    bp = bd + al(n * 36, 16)
    cur = bp + n * 32
    datos, pals = [], []
    for tw, th, bpp, idx, pal in texs:
        w, h = al(tw, 32 if bpp == 4 else 16), al(th, 8)
        datos.append((w, h, indices_psp(extender(idx, tw, th, w, h), w, h, bpp)))
        pals.append(pal.ljust(64 if bpp == 4 else 1024, b'\x00')[:64 if bpp == 4 else 1024])
    offs = []
    for w, h, b in datos:
        cur = al(cur, 128); offs.append(cur); cur += len(b)
    poffs = []
    for p in pals:
        cur = al(cur, 128); poffs.append(cur); cur += len(p)
    d = bytearray(cur)
    d[0:4] = b'RHG\x00'
    struct.pack_into('<I', d, 4, 1)
    for o in (0x14, 0x1C, 0x24):
        struct.pack_into('<I', d, o, n)
    struct.pack_into('<I', d, 0x28, 2)
    for k, (tw, th, bpp, idx, pal) in enumerate(texs):
        w, h, b = datos[k]
        struct.pack_into('<3I', d, 0x30 + k * 40, 1, k, k)
        struct.pack_into('<2I', d, 0x30 + k * 40 + 20, tw, th)
        struct.pack_into('<4I', d, bd + k * 36, 2, 0x12 if bpp == 4 else 0x11, w, h)
        struct.pack_into('<2I', d, bd + k * 36 + 20, len(b), w // 2 if bpp == 4 else w)
        struct.pack_into('<I', d, bp + k * 32, 0x14)
        struct.pack_into('<3I', d, bp + k * 32 + 8, len(pals[k]), len(pals[k]) // 4, 2)
        d[offs[k]:offs[k] + len(b)] = b
        d[poffs[k]:poffs[k] + len(pals[k])] = pals[k]
    return bytes(d)


def construir_tex(nombre, texs):
    return construir_rhg(texs) if nombre.lower().endswith('.rhg') else construir_atex(texs)


# mismas imagenes y paletas en la zona que ve el juego, y los campos que lee de las cabeceras
def mismo_tex(nombre, a, b):
    ta, tb = texturas_de(nombre, a), texturas_de(nombre, b)
    return len(ta) == len(tb) and all(x[:3] == y[:3] and x[3] == y[3] and x[4].ljust(len(y[4]), b'\x00') == y[4].ljust(len(x[4]), b'\x00') for x, y in zip(ta, tb))


def quitar_textura(ruta):
    if os.path.isdir(ruta) and not borrar_carpeta(ruta):
        return False
    if os.path.isfile(ruta):
        os.remove(ruta)
    s = textura_suelta(ruta)
    while s:
        os.remove(s[0])
        s = textura_suelta(ruta)
    return True


def abrir_tex(ruta, d):
    texs = texturas_de(ruta, d)
    if not texs:
        return None
    psp = escala_psp(ruta)
    if not quitar_textura(ruta):
        return None
    if len(texs) == 1:
        tw, th, bpp, idx, pal = texs[0]
        escribir_png_indexado(ruta + '.png', tw, th, idx, alfa_doble(pal) if psp else pal)
    else:
        os.makedirs(ruta)
        for i, (tw, th, bpp, idx, pal) in enumerate(texs):
            escribir_png_indexado(os.path.join(ruta, '%02d.png' % (i + 1)), tw, th, idx, alfa_doble(pal) if psp else pal)
    rearmado = empaquetar_tex(ruta)
    if rearmado is None or not mismo_tex(ruta, d, rearmado):
        print('aviso: %s no coincide al rearmar' % ruta)
    return ruta


# rearma desde los png: exacto con png indexados al tamano del juego; los demas los adapta el runtime
def empaquetar_tex(ruta, estricto=False):
    psp = escala_psp(ruta)
    texs = []
    for png, esc in pngs_de(ruta):
        r = leer_png_indexado(png)
        if not r or esc != 1 or len(r[3]) > 1024:
            print('aviso: %s no es un png indexado al tamano del juego; el runtime lo adapta, --empaquetar no' % png)
            return None
        w, h, idx, pal = r
        texs.append((w, h, 4 if len(pal) <= 64 else 8, idx, alfa_medio(pal) if psp else pal))
    return construir_tex(ruta, texs) if texs else None


# formato anterior (_tex.txt con la estructura): solo para convertir carpetas viejas sin reextraer
def empaquetar_tex_txt(carpeta):
    d, texs = bytearray(), []
    for linea in open(os.path.join(carpeta, '_tex.txt'), encoding='utf-8'):
        c = linea.split(None, 8)
        if not c or c[0] == 'alfa':
            continue
        if c[0] == 'total':
            d = bytearray(int(c[1]))
        elif c[0] == 'bytes':
            o, b = int(c[1]), bytes.fromhex(c[2].strip())
            d[o:o + len(b)] = b
        elif len(c) >= 9:
            texs.append([int(x) for x in c[:8]] + [c[8].strip()])
    for idx, off, tam, w, h, bpp, po, pt, nombre in texs:
        r = leer_png_indexado(os.path.join(carpeta, nombre)) if os.path.isfile(os.path.join(carpeta, nombre)) else None
        if not r or r[0] != w or r[1] != h:
            return None
        datos = indices_psp(r[2], w, h, bpp)
        d[off:off + min(tam, len(datos))] = datos[:tam]
        d[po:po + min(pt, len(r[3]))] = r[3][:pt]
    return bytes(d)


def tex_vacia(d):
    return len(d) == 0 or (d[:4] == b'RHG\x00' and len(d) >= 0x18 and struct.unpack_from('<I', d, 0x14)[0] == 0)


# abre los .atex/.rhg aun cerrados y convierte las carpetas con _tex.txt; devuelve (abiertos, vacios, problemas)
def abrir_tex_pendientes(raiz, rehacer=False):
    abiertos, vacios, problemas = 0, 0, []
    for dirpath, dirs, nombres in os.walk(raiz):
        for x in [x for x in dirs if x.lower().endswith(TEXTURAS)]:
            dirs.remove(x)
            ruta = os.path.join(dirpath, x)
            if not os.path.isfile(os.path.join(ruta, '_tex.txt')):
                continue
            d = empaquetar_tex_txt(ruta)
            if d is None:
                problemas.append(os.path.relpath(ruta, raiz).replace('\\', '/') + ' (tiene png editados, no se convierte)')
                continue
            if abrir_tex(ruta, d):
                abiertos += 1
            elif borrar_carpeta(ruta):
                with open(ruta, 'wb') as o:
                    o.write(d)
                problemas.append('%s (%d bytes, %s)' % (os.path.relpath(ruta, raiz).replace('\\', '/'), len(d), d[:8].hex()))
        for x in sorted(nombres, key=clave_natural):
            if not x.lower().endswith(TEXTURAS):
                continue
            ruta = os.path.join(dirpath, x)
            with open(ruta, 'rb') as f:
                d = f.read()
            if tex_vacia(d):
                vacios += 1
                continue
            if abrir_tex(ruta, d):
                abiertos += 1
            else:
                problemas.append('%s (%d bytes, %s)' % (os.path.relpath(ruta, raiz).replace('\\', '/'), len(d), d[:8].hex()))
    return abiertos, vacios, problemas


# arma cualquier contenedor abierto segun su extension
def armar(carpeta):
    if carpeta.lower().endswith(TEXTURAS):
        return empaquetar_tex(carpeta)
    if carpeta.lower().endswith('.txt1'):
        return empaquetar_txt1(carpeta)
    if carpeta.lower().endswith('.vagp'):
        return empaquetar_vagp(carpeta)
    return empaquetar(carpeta)


# mismo texto salvo la terminacion: el ISO recorta algunos txt1 sueltos antes del 0 final
def mismo_txt1(orig, nuevo):
    corto, largo = (orig, nuevo) if len(orig) <= len(nuevo) else (nuevo, orig)
    k = len(corto) & ~1
    return largo[:k] == corto[:k] and not largo[k:].strip(b'\x00') and len(corto) - k <= 1



VAGP_MARCA = b'\x00\x07TTTWAV'
ADPCM_F = ((0, 0), (60, 0), (115, -52), (98, -55), (122, -60))


# ADPCM de PSP igual que lo decodifica el mezclador sceSas del runtime
def vag_a_pcm(s):
    out = bytearray()
    s1 = s2 = 0
    ini = fin = -1
    for k in range(0, len(s) - 15, 16):
        pr, sh, fl = s[k] >> 4, s[k] & 15, s[k + 1]
        if pr > 4:
            pr = 0
        if sh > 12:
            sh = 9
        if fl == 7:
            break
        if fl == 6:
            ini = len(out) // 2
        for i in range(28):
            n = (s[k + 2 + i // 2] >> 4) if i & 1 else (s[k + 2 + i // 2] & 15)
            v = (((n << 12) & 0xFFFF) - 0x10000 if n & 8 else n << 12) >> sh
            v += (s1 * ADPCM_F[pr][0] + s2 * ADPCM_F[pr][1]) >> 6
            v = -32768 if v < -32768 else 32767 if v > 32767 else v
            s2, s1 = s1, v
            out += struct.pack('<h', v)
        if fl == 3:
            fin = len(out) // 2
            break
        if fl == 1:
            break
    return bytes(out), (max(ini, 0), fin) if fin > 0 else None


# el bucle viaja en el trozo smpl: sin el, el sonido suena una vez
def escribir_wav(ruta, pcm, frec, bucle=None):
    extra = b''
    if bucle:
        extra = b'smpl' + struct.pack('<I', 60) + struct.pack('<9I', 0, 0, 1000000000 // frec, 60, 0, 0, 0, 1, 0)
        extra += struct.pack('<6I', 0, 0, bucle[0], bucle[1] - 1, 0, 0)
    with open(ruta, 'wb') as o:
        o.write(b'RIFF' + struct.pack('<I', 36 + len(pcm) + len(extra)) + b'WAVEfmt ')
        o.write(struct.pack('<IHHIIHH', 16, 1, 1, frec, frec * 2, 2, 16))
        o.write(b'data' + struct.pack('<I', len(pcm)) + pcm + extra)


# numero inicial del nombre, o None
def prefijo_num(nombre):
    d = ''
    for c in os.path.basename(nombre):
        if not c.isdigit():
            break
        d += c
    return int(d) if d else None


# la tabla de sonidos vive en el .pphd hermano: bloques desde 0x10 y entradas de 0x10 bytes desde 0x20.
# el hermano es el que lleva el numero siguiente al del vagp; si no aparece se prueba con cualquiera
def tabla_ppva(carpeta, pref=None):
    nombres = sorted(os.listdir(carpeta))
    if pref is not None:
        nombres = [n for n in nombres if prefijo_num(n) == pref + 1] + nombres
    for nombre in nombres:
        ruta = os.path.join(carpeta, nombre)
        if not os.path.isfile(ruta) or not nombre.lower().endswith('.pphd'):
            continue
        with open(ruta, 'rb') as f:
            d = f.read()
        if d[:4] != b'PPHD':
            continue
        bloques = []
        pos = 0x10
        while pos + 4 <= len(d):
            v = struct.unpack_from('<I', d, pos)[0]
            if v == 0xFFFFFFFF or v >= len(d):
                break
            bloques.append(v)
            pos += 4
        bloques.append(len(d))
        for k in range(len(bloques) - 1):
            if d[bloques[k]:bloques[k] + 4] != b'PPVA':
                continue
            p = d[bloques[k]:bloques[k + 1]]
            # el campo de tamano a veces declara una entrada mas de las que caben
            n = min((8 + struct.unpack_from('<I', p, 4)[0] - 0x20) // 16, (len(p) - 0x20) // 16)
            salida = []
            for i in range(max(n, 0)):
                o, fr, l = struct.unpack_from('<III', p, 0x20 + i * 16)
                if (o, fr, l) == (0, 0, 0):
                    break
                salida.append((o, fr, 0 if l >= 0x80000000 else l))
            if salida:
                return salida
    return None


def nombre_sonido(plant, i):
    if plant and i in plant:
        seccion, nombre = plant[i]
        return (seccion + '/' if seccion else '') + os.path.splitext(nombre)[0] + '.wav'
    return '%d-%X.wav' % (i, i)


def abrir_vagp(ruta, d, carpeta_padre, sub='', ctx=None):
    tabla = tabla_ppva(carpeta_padre, prefijo_num(ruta))
    if not tabla:
        return None
    plant = {}
    if ctx:
        nom = plantilla_de(sub, len(tabla), ctx['reglas'], ctx['porcuenta'])
        if nom and nom != '-':
            plant = ctx['plantillas'].get(nom, {})
    if os.path.isdir(ruta) and not borrar_carpeta(ruta):
        return None
    if os.path.isfile(ruta):
        os.remove(ruta)
    os.makedirs(ruta)
    usados = set()
    for i, (off, frec, largo) in enumerate(tabla, 1):
        nombre = nombre_sonido(plant, i)
        if nombre in usados:
            nombre = '%d-%X.wav' % (i, i)
        usados.add(nombre)
        if not largo or off + largo > len(d):
            continue
        destino = os.path.join(ruta, nombre)
        os.makedirs(os.path.dirname(destino), exist_ok=True)
        pcm, bucle = vag_a_pcm(d[off:off + largo])
        escribir_wav(destino, pcm, frec or 11025, bucle)
    return ruta


# sin _vagp.txt: la tabla PPVA del pphd hermano da desplazamiento, frecuencia y largo; el total es el fin alineado a 64
def leer_vagp(carpeta):
    tabla = tabla_ppva(os.path.dirname(os.path.abspath(carpeta)), prefijo_num(carpeta)) or []
    filas = [(off, largo, frec) for off, frec, largo in tabla]
    fin = max((off + largo for off, largo, _f in filas), default=0)
    return (fin + 63) & ~63, filas


# el blob solo lleva la marca de cada sonido; el audio real sale de los wav en tiempo de reproduccion
def empaquetar_vagp(carpeta):
    total, filas = leer_vagp(carpeta)
    d = bytearray(total)
    for off, largo, _frec in filas:
        if largo >= 16 and off + 16 <= total:
            d[off:off + 16] = VAGP_MARCA + struct.pack('<II', 0, 0)
    return bytes(d)

# regla: patron [@cantidad] = plantilla; "-" abre sin plantilla; "!" no se abre. Gana la primera que coincida.
# convierte una carpeta txt1 con N-HEX.txt al formato texto.txt; devuelve cuantas cadenas
def unir_txt1(carpeta):
    viejo = os.path.join(carpeta, 'texto.txt')
    nuevo = os.path.join(carpeta, os.path.basename(os.path.normpath(carpeta))[:-5] + '.txt')
    if os.path.isfile(viejo) and not os.path.isfile(nuevo):
        os.rename(viejo, nuevo)
        return 0
    if os.path.isfile(nuevo):
        return 0
    nombres = listar_txt1(carpeta)
    textos = []
    for nombre in nombres:
        with open(os.path.join(carpeta, nombre), 'rb') as f:
            u = texto_a_utf16(f.read())
        textos.append(u[:-2].decode('utf-16-le', 'replace'))
    antes = empaquetar_txt1(carpeta)
    escribir_texto(carpeta, textos)
    if empaquetar_txt1(carpeta) != antes:
        os.remove(ruta_texto(carpeta))
        raise ValueError('%s: texto.txt no reproduce las cadenas, se deja como estaba' % carpeta)
    for nombre in nombres:
        os.remove(os.path.join(carpeta, nombre))
    return len(nombres)


def carpetas_txt1(raiz):
    for dirpath, dirs, nombres in os.walk(raiz):
        if dirpath.lower().endswith('.txt1') and '_txt1.bin' in nombres:
            yield dirpath


def leer_reglas(ruta):
    reglas = []
    if os.path.exists(ruta):
        for linea in open(ruta, encoding='utf-8', errors='replace'):
            linea = linea.split('#')[0].strip()
            if '=' not in linea:
                continue
            pat, plant = linea.split('=', 1)
            pat, n = pat.strip(), 0
            if '@' in pat:
                pat, cant = pat.rsplit('@', 1)
                pat, n = pat.strip(), int(cant)
            reglas.append((pat, n, plant.strip()))
    return reglas


def plantilla_de(rel, n, reglas, porcuenta):
    for pat, cant, plant in reglas:
        if (not cant or cant == n) and fnmatch.fnmatch(rel, pat):
            return plant
    unica = porcuenta.get(n, [])
    return unica[0] if len(unica) == 1 else None


def rutas_manifiesto(assets):
    rutas = []
    for linea in open(os.path.join(assets, 'manifiesto.txt'), encoding='utf-8'):
        if linea.startswith('#'):
            continue
        campos = linea.rstrip('\r\n').split('\t')
        if len(campos) >= 7:
            rutas.append(campos[6])
    return rutas


def nombre_sub(plant, i, con_secciones=True):
    if plant and i in plant:
        seccion, nombre = plant[i]
        return (seccion + '/' if seccion and con_secciones else '') + nombre
    return 'sin_nombre_%03d.bin' % i


# en Windows a veces un archivo queda ocupado un instante; se reintenta y si no se puede se avisa
def borrar_carpeta(carpeta):
    for intento in range(5):
        try:
            shutil.rmtree(carpeta)
            return True
        except OSError:
            time.sleep(0.3)
    print('aviso: no pude borrar %s (algun programa la tiene abierta)' % carpeta)
    return False


# cada entrada se guarda tal cual, relleno incluido: asi el reempaquetado es exacto
def contenido(sub):
    return sub


def sin_ceros_finales(sub):
    real = len(sub)
    while real > 0 and sub[real - 1] == 0:
        real -= 1
    return sub[:real]


def listar_carpeta(carpeta):
    salida = []
    for dirpath, dirs, nombres in os.walk(carpeta):
        dirs[:] = [x for x in dirs if not x.lower().endswith('.vfx') and x.lower() != '1_p_dmg' and not x.lower().startswith('011_textures_dmg.')]
        for x in list(dirs):
            if es_contenedor(x):
                dirs.remove(x)
                salida.append(os.path.relpath(os.path.join(dirpath, x), carpeta).replace('\\', '/'))
        for x in nombres:
            if x.lower() in IGNORAR or x.startswith('.') or x.lower().startswith('011_textures_dmg.'):
                continue
            s = png_suelto(x)
            if s:
                salida.append(os.path.relpath(os.path.join(dirpath, s[0]), carpeta).replace('\\', '/'))
                continue
            salida.append(os.path.relpath(os.path.join(dirpath, x), carpeta).replace('\\', '/'))
    return orden_partes(salida)


# tabla de N+1 desplazamientos, cuerpo desde el siguiente multiplo de 64 y archivos uno tras otro.
# Cada archivo ya lleva su relleno, asi que el resultado calza con el original salvo que se edite algo.
def empaquetar(carpeta):
    nombres = listar_carpeta(carpeta)
    n = len(nombres)
    inicio = (4 + 4 * (n + 1) + ALINEACION - 1) // ALINEACION * ALINEACION
    offs, cuerpo = [], bytearray()
    for nombre in nombres:
        offs.append(inicio + len(cuerpo))
        ruta = os.path.join(carpeta, nombre)
        if os.path.isdir(ruta) or (nombre.lower().endswith(TEXTURAS) and not os.path.isfile(ruta)):
            cuerpo += armar(ruta) or b''
        else:
            with open(ruta, 'rb') as f:
                cuerpo += f.read()
        if len(cuerpo) % ALINEACION:
            cuerpo += b'\x00' * (ALINEACION - len(cuerpo) % ALINEACION)
    offs.append(inicio + len(cuerpo))
    return struct.pack('>I', n) + struct.pack('>%dI' % (n + 1), *offs) + b'\x00' * (inicio - 4 - 4 * (n + 1)) + bytes(cuerpo)


# mismo contenido entrada por entrada, sin contar relleno ni cola
def mismo_contenido(a, b):
    ra, rb = analizar(a), analizar(b)
    if not ra or not rb or ra['n'] != rb['n']:
        return False
    for i in range(ra['n']):
        if a[ra['limites'][i]:ra['limites'][i + 1]] != b[rb['limites'][i]:rb['limites'][i + 1]]:
            return False
    return True


# ---------------- tipo por contenido ----------------
# las plantillas dan el nombre; la extension la decide la firma del archivo (una plantilla por cantidad de
# entradas puede nombrar como banco de sonido un par AGS + RHG)
MAGIAS = ((b'AGS\x00', 'ags'), (b'RHG\x00', 'rhg'), (b'PPHD', 'pphd'), (b'\x7fELF', 'elf'), (b'.BBQ', 'bbq'))
TIPADAS = ('.pak', '.txt1', '.vagp', '.atex', '.rhg', '.pphd', '.ags', '.elf', '.bbq')
PALABRA = {'vagp': 'sfx', 'pphd': 'index', 'rhg': 'img', 'ags': 'anim', 'txt1': 'text', 'atex': 'tex', 'pak': 'pak', 'elf': 'code', 'bbq': 'data'}
GENERICAS = set(PALABRA.values()) | {'textures', 'texture'}


def tipo_por_contenido(d, siguiente=b''):
    for m, t in MAGIAS:
        if d[:len(m)] == m:
            return t
    if len(d) >= 32 and not any(d[:16]) and siguiente[:4] == b'PPHD':
        return 'vagp'
    if analizar_txt1(d):
        return 'txt1'
    if analizar_atex(d):
        return 'atex'
    if analizar(d):
        return 'pak'
    return None


def renombrar_tipo(nombre, tipo):
    carpeta, nom = nombre.rsplit('/', 1) if '/' in nombre else ('', nombre)
    base = os.path.splitext(nom)[0]
    m = re.match(r'(\d+)_(.+)$', base)
    if m and m.group(2).lower() in GENERICAS:
        base = '%s_%s' % (m.group(1), PALABRA[tipo])
    return (carpeta + '/' if carpeta else '') + base + '.' + tipo


def tipo_por_firma(d):
    return next((t for m, t in MAGIAS if d[:len(m)] == m), None)


# un .vag de plantilla solo se corrige por firma (AGS, RHG...): el audio crudo no tiene firma fiable
def corregir_tipos(nombres, datos):
    salida = list(nombres)
    for i, nom in enumerate(nombres):
        if nom.lower().endswith('.vag'):
            t = tipo_por_firma(datos[i])
            if t:
                salida[i] = renombrar_tipo(nom, t)
            continue
        if not nom.lower().endswith(TIPADAS):
            continue
        t = tipo_por_contenido(datos[i], datos[i + 1] if i + 1 < len(datos) else b'')
        if t and not nom.lower().endswith('.' + t):
            salida[i] = renombrar_tipo(nom, t)
    return salida


# renombra en assets los archivos cuya firma contradice su extension y abre los contenedores que aparecen;
# las entradas del manifiesto no se renombran (el runtime las busca por su nombre)
# ruta dentro de assets -> ruta como la ven las reglas (la carpeta de nivel superior lleva su .pak)
def sub_manifiesto(rel, manifiesto):
    partes = rel.split('/')
    for k in range(len(partes), 0, -1):
        pref = '/'.join(partes[:k])
        if pref + '.pak' in manifiesto:
            return '/'.join([pref + '.pak'] + partes[k:])
    return rel


# pone a los wav con nombre generico (N-HEX.wav) el nombre de la plantilla del banco, sin reconvertir nada
def nombrar_wavs(carpeta, rel, manifiesto, ctx):
    tabla = tabla_ppva(os.path.dirname(carpeta), prefijo_num(carpeta))
    if not tabla:
        return 0
    nom = plantilla_de(sub_manifiesto(rel, manifiesto), len(tabla), ctx['reglas'], ctx['porcuenta'])
    plant = ctx['plantillas'].get(nom, {}) if nom and nom != '-' else {}
    genericos = [x for x in os.listdir(carpeta) if re.match(r'\d+-[0-9A-Fa-f]+\.wav$', x)]
    if genericos and not plant:
        print('  sin plantilla: %s (%d sonidos; regla buscada con %s; plantillas con esa cantidad: %s)' % (
            rel, len(tabla), sub_manifiesto(rel, manifiesto), ', '.join(ctx['porcuenta'].get(len(tabla), [])) or 'ninguna'))
    n = 0
    for x in genericos:
        m = re.match(r'(\d+)-[0-9A-Fa-f]+\.wav$', x)
        if not m:
            continue
        nuevo = nombre_sonido(plant, int(m.group(1)))
        destino = os.path.join(carpeta, nuevo)
        if nuevo == x or os.path.exists(destino):
            continue
        os.makedirs(os.path.dirname(destino), exist_ok=True)
        os.replace(os.path.join(carpeta, x), destino)
        n += 1
    return n


def renombrar_por_contenido(raiz, manifiesto, ctx, raros_dir):
    cambios, raros, nuevos = [], [], []
    for dirpath, dirs, nombres in os.walk(raiz):
        for x in [x for x in dirs if x.lower().endswith('.vagp')]:
            dirs.remove(x)
            ruta = os.path.join(dirpath, x)
            n = nombrar_wavs(ruta, os.path.relpath(ruta, raiz).replace('\\', '/'), manifiesto, ctx)
            if n:
                cambios.append('%s: %d wav con nombre de plantilla' % (os.path.relpath(ruta, raiz).replace('\\', '/'), n))
        orden = sorted([x for x in nombres], key=clave_natural)
        for k, x in enumerate(orden):
            if not x.lower().endswith(TIPADAS):
                continue
            ruta = os.path.join(dirpath, x)
            rel = os.path.relpath(ruta, raiz).replace('\\', '/')
            with open(ruta, 'rb') as f:
                d = f.read()
            if not d or tex_vacia(d):
                continue
            sig = b''
            if k + 1 < len(orden):
                with open(os.path.join(dirpath, orden[k + 1]), 'rb') as f:
                    sig = f.read(4)
            t = tipo_por_contenido(d, sig)
            if t is None:
                if x.lower().endswith('.vagp'):
                    vecinos = []
                    for y in orden:
                        with open(os.path.join(dirpath, y), 'rb') as f:
                            vecinos.append('%s=%s' % (y, f.read(4).hex()))
                    print('  vagp cerrado: %s; vecinos: %s' % (rel, ' '.join(vecinos)))
                elif x.lower().endswith(('.txt1', '.atex', '.rhg')) and len(d) >= 256:
                    raros.append((rel, d, True))
                continue
            if x.lower().endswith('.' + t):
                continue
            if rel in manifiesto:
                if len(d) >= 256:
                    raros.append((rel + ' (del manifiesto, es ' + t + ')', d, False))
                continue
            nuevo = renombrar_tipo(x, t)
            if os.path.exists(os.path.join(dirpath, nuevo)):
                raros.append((rel + ' (ya existe ' + nuevo + ')', d, False))
                continue
            os.replace(ruta, os.path.join(dirpath, nuevo))
            cambios.append('%s -> %s' % (rel, nuevo))
            nuevos.append((os.path.join(dirpath, nuevo), d, t))
    abiertos = collections.Counter()
    for ruta, d, t in nuevos:
        sub = os.path.relpath(ruta, raiz).replace('\\', '/')
        if t in ('rhg', 'atex') and abrir_tex(ruta, d):
            abiertos[t] += 1
        elif t == 'txt1' and abrir_txt1(ruta, d):
            abiertos[t] += 1
        elif t == 'vagp' and abrir_vagp(ruta, d, os.path.dirname(ruta), sub_manifiesto(sub, manifiesto), ctx):
            abiertos[t] += 1
    for rel, d, copiar in raros:
        if copiar:
            os.makedirs(raros_dir, exist_ok=True)
            with open(os.path.join(raros_dir, rel.split(' ')[0].replace('/', '_')), 'wb') as o:
                o.write(d)
    return cambios, abiertos, raros


def abrir(ruta, d, r, plant, carpeta=None):
    if carpeta is None:
        carpeta = os.path.splitext(ruta)[0]
    # las secciones de la plantilla son subcarpetas; si los nombres no reproducen el orden del indice se despliega plano
    nombres = None
    datos = [d[r['limites'][i]:r['limites'][i + 1]] for i in range(r['n'])]
    for con_secciones in (True, False):
        base = [nombre_sub(plant, i + 1, con_secciones) for i in range(r['n'])]
        for cand in (corregir_tipos(base, datos), base):
            if orden_partes(cand) == cand and len(set(cand)) == len(cand):
                nombres = cand
                break
        if nombres:
            break
    if nombres is None:
        return None
    if os.path.isdir(carpeta) and not borrar_carpeta(carpeta):
        return None
    for i, nombre in enumerate(nombres):
        destino = os.path.join(carpeta, nombre)
        os.makedirs(os.path.dirname(destino), exist_ok=True)
        with open(destino, 'wb') as o:
            o.write(contenido(datos[i]))
    return carpeta


# abre lo anidado dentro de una carpeta recien desplegada; devuelve (abiertos, problemas)
def abrir_anidados(carpeta, rel, ctx):
    abiertos, problemas = collections.Counter(), []
    for dirpath, _, nombres in os.walk(carpeta):
        for x in sorted(nombres, key=clave_natural):
            if not es_contenedor(x):
                continue
            ruta = os.path.join(dirpath, x)
            sub = rel + '/' + os.path.relpath(ruta, carpeta).replace('\\', '/')
            with open(ruta, 'rb') as f:
                d = f.read()
            if x.lower().endswith('.txt1'):
                if abrir_txt1(ruta, d):
                    if not mismo_txt1(d, empaquetar_txt1(ruta)):
                        problemas.append(sub + ' (txt1 no coincide al rearmar)')
                    abiertos['txt1'] += 1
                else:
                    problemas.append(sub + ' (no parece txt1)')
                continue
            if x.lower().endswith(TEXTURAS):
                if abrir_tex(ruta, d):
                    abiertos['tex'] += 1
                else:
                    problemas.append(sub + ' (no parece ATEX/RHG)')
                continue
            if x.lower().endswith('.vagp'):
                if abrir_vagp(ruta, d, dirpath, sub, ctx):
                    abiertos['vagp'] += 1
                else:
                    problemas.append(sub + ' (no encuentro la tabla PPVA en el pphd hermano)')
                continue
            r = analizar(d)
            if not r:
                continue
            nombre_plantilla = plantilla_de(sub, r['n'], ctx['reglas'], ctx['porcuenta'])
            if nombre_plantilla == '!':
                continue
            if not nombre_plantilla:
                ctx['sin'][(os.path.dirname(sub), r['n'])] += 1
                continue
            plant = ctx['plantillas'].get(nombre_plantilla, {}) if nombre_plantilla != '-' else {}
            tmp = ruta + '.abriendo'
            os.replace(ruta, tmp)
            hecho = abrir(ruta, d, r, plant, carpeta=ruta)
            if not hecho:
                os.replace(tmp, ruta)
                problemas.append(sub + ' (plantilla %s desordenada)' % nombre_plantilla)
                continue
            os.remove(tmp)
            if not mismo_contenido(empaquetar(hecho), d):
                problemas.append(sub + ' (pak no coincide al rearmar)')
            abiertos[nombre_plantilla] += 1
            a2, p2 = abrir_anidados(hecho, sub, ctx)
            abiertos.update(a2)
            problemas += p2
    return abiertos, problemas


# compara entrada por entrada el .pak con lo que sale de su carpeta y muestra la primera diferencia
def diagnosticar(ruta):
    carpeta = os.path.splitext(ruta)[0]
    with open(ruta, 'rb') as f:
        d = f.read()
    nuevo = empaquetar(carpeta)
    ra, rb = analizar(d), analizar(nuevo)
    print('original: %d bytes, %s entradas; reempaquetado: %d bytes, %s entradas' % (len(d), ra and ra['n'], len(nuevo), rb and rb['n']))
    if not ra or not rb:
        return
    nombres = listar_carpeta(carpeta)
    for i in range(min(ra['n'], rb['n'])):
        a_ = d[ra['limites'][i]:ra['limites'][i + 1]]
        b_ = nuevo[rb['limites'][i]:rb['limites'][i + 1]]
        ca, cb = sin_ceros_finales(a_), sin_ceros_finales(b_)
        if ca == cb:
            continue
        k = next((j for j in range(min(len(ca), len(cb))) if ca[j] != cb[j]), min(len(ca), len(cb)))
        print('entrada %d (%s): original %d bytes (%d con relleno), nuevo %d bytes (%d con relleno), primera diferencia en %d' % (i + 1, nombres[i] if i < len(nombres) else '?', len(ca), len(a_), len(cb), len(b_), k))
        print('  original: %s' % a_[max(0, k - 8):k + 24].hex())
        print('  nuevo:    %s' % b_[max(0, k - 8):k + 24].hex())
        print('  alineacion original: desplazamiento %d, siguiente %d' % (ra['limites'][i], ra['limites'][i + 1]))
        return
    print('todas las entradas coinciden')


def modulo_extraccion():
    import importlib.util
    spec = importlib.util.spec_from_file_location('extract_assets', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'extract_assets.py'))
    ea = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(ea)
    return ea


def peso_carpeta(carpeta):
    return sum(os.path.getsize(os.path.join(d, x)) for d, _, ns in os.walk(carpeta) for x in ns)


# paks con regla "!" que estan abiertos: se listan o se vuelven a cerrar con sus bytes originales del ISO
def cerrar(raiz, assets, iso, eboot, reglas, porcuenta, aplicar):
    if os.path.isfile(os.path.join(assets, 'shared.txt')):
        print('cerrar: primero python3 tools/unify_assets.py --undo')
        return
    ea = modulo_extraccion()
    indice = {}
    for linea in open(os.path.join(assets, 'manifiesto.txt'), encoding='utf-8'):
        campos = linea.rstrip('\r\n').split('\t')
        if not linea.startswith('#') and len(campos) >= 7:
            indice[int(campos[0])] = campos[6]
    f = open(iso, 'rb')
    archivos = ea.iso_listar(f)
    clave_pk = next(k for k in archivos if k.upper().endswith('USRDIR/PACKFILE.BIN'))
    pk_lba, pk_tam = archivos[clave_pk]
    sd_eboot, _ = ea.leer_eboot(eboot)
    n, sd, entradas, _ = ea.leer_indice(f, pk_lba, pk_tam, sd_eboot)
    mods = os.path.join(BASE, 'mods')
    hechos, abierto_t, cerrado_t, fallos = 0, 0, 0, []
    for i in range(1, n + 1):
        rel = indice.get(i)
        if not rel or not rel.endswith('.pak'):
            continue
        carpeta = os.path.join(raiz, os.path.splitext(rel)[0])
        if not os.path.isdir(carpeta) or os.path.isfile(os.path.join(raiz, rel)):
            continue
        s, t, _a, _c = entradas[i]
        f.seek((pk_lba + sd + s) * ea.SECTOR)
        d = f.read(t)
        r = analizar(d)
        if not r or plantilla_de(rel, r['n'], reglas, porcuenta) != '!':
            continue
        peso = peso_carpeta(carpeta)
        en_mods = [m for m in (os.listdir(mods) if os.path.isdir(mods) else []) if os.path.isdir(os.path.join(mods, m, 'Packfile', os.path.splitext(rel)[0]))]
        print('cerrar: %s, abierto %.1f MB, cerrado %.1f MB%s' % (rel, peso / 1048576, len(d) / 1048576, (', lo tocan los mods: ' + ' '.join(en_mods)) if en_mods else ''))
        abierto_t += peso
        cerrado_t += len(d)
        if not aplicar:
            hechos += 1
            continue
        tmp = os.path.join(raiz, rel + '.tmp')
        with open(tmp, 'wb') as o:
            o.write(d)
        ok = False
        for intento in range(20):
            if borrar_carpeta(carpeta):
                ok = True
                break
        if not ok:
            os.remove(tmp)
            fallos.append(rel)
            continue
        os.replace(tmp, os.path.join(raiz, rel))
        hechos += 1
    f.close()
    print('cerrar: %d paks %s, %.1f MB abiertos -> %.1f MB cerrados' % (hechos, 'cerrados' if aplicar else 'por cerrar', abierto_t / 1048576, cerrado_t / 1048576))
    for x in fallos:
        print('cerrar: no se pudo %s' % x)


# vuelve a abrir desde el ISO solo los .vagp ya abiertos, para renombrarlos o tomar sus marcas de bucle
def rehacer_vagp(raiz, assets, iso, eboot, ctx):
    ea = modulo_extraccion()
    indice = {}
    for linea in open(os.path.join(assets, 'manifiesto.txt'), encoding='utf-8'):
        campos = linea.rstrip('\r\n').split('\t')
        if not linea.startswith('#') and len(campos) >= 7:
            indice[int(campos[0])] = campos[6]
    f = open(iso, 'rb')
    archivos = ea.iso_listar(f)
    clave_pk = next(k for k in archivos if k.upper().endswith('USRDIR/PACKFILE.BIN'))
    pk_lba, pk_tam = archivos[clave_pk]
    sd_eboot, _ = ea.leer_eboot(eboot)
    n, sd, entradas, _ = ea.leer_indice(f, pk_lba, pk_tam, sd_eboot)
    hechos, saltados = 0, []
    for i in range(1, n + 1):
        rel = indice.get(i)
        if not rel or not rel.endswith('.pak'):
            continue
        carpeta = os.path.join(raiz, os.path.splitext(rel)[0])
        if not os.path.isdir(carpeta):
            continue
        abiertos = []
        for dirpath, dirs, _n in os.walk(carpeta):
            for x in list(dirs):
                if x.lower().endswith('.vagp'):
                    abiertos.append(os.path.relpath(os.path.join(dirpath, x), carpeta).replace('\\', '/'))
                    dirs.remove(x)
        if not abiertos:
            continue
        s, t, _a, _c = entradas[i]
        f.seek((pk_lba + sd + s) * ea.SECTOR)
        d = f.read(t)
        r = analizar(d)
        nombre_plantilla = plantilla_de(rel, r['n'], ctx['reglas'], ctx['porcuenta']) if r else None
        if not r or not nombre_plantilla or nombre_plantilla == '!':
            saltados.append(rel)
            continue
        plant = ctx['plantillas'].get(nombre_plantilla, {}) if nombre_plantilla != '-' else {}
        tmp = tempfile.mkdtemp(prefix='vagp_')
        try:
            destino = os.path.join(tmp, os.path.basename(carpeta))
            if not abrir(os.path.join(tmp, os.path.basename(rel)), d, r, plant, carpeta=destino):
                saltados.append(rel)
                continue
            abrir_anidados(destino, rel, ctx)
            for sub in abiertos:
                nuevo = os.path.join(destino, sub)
                viejo = os.path.join(carpeta, sub)
                if not os.path.isdir(nuevo):
                    saltados.append(rel + '/' + sub)
                    continue
                if not borrar_carpeta(viejo):
                    continue
                shutil.move(nuevo, viejo)
                hechos += 1
        finally:
            shutil.rmtree(tmp, ignore_errors=True)
    print('rehechos %d vagp desde el ISO' % hechos)
    for x in saltados[:20]:
        print('  sin rehacer: %s' % x)


def sugerir(candidatos, raiz, reglas, porcuenta, destino):
    grupos = collections.OrderedDict()
    for rel in candidatos:
        ruta = os.path.join(raiz, rel)
        with open(ruta, 'rb') as f:
            d = f.read(65536)
        r = analizar(d, os.path.getsize(ruta))
        if not r or plantilla_de(rel, r['n'], reglas, porcuenta):
            continue
        grupos.setdefault((os.path.dirname(rel), r['n']), []).append(rel)
    with open(destino, 'w', encoding='utf-8', newline='\n') as w:
        w.write('# pendientes: anade una linea  patron @cantidad = plantilla  por cada bloque\n')
        for (carpeta, n), rutas in grupos.items():
            w.write('\n# %d paks de %d entradas en %s\n' % (len(rutas), n, carpeta or '(raiz)'))
            for x in rutas[:6]:
                w.write('#   %s\n' % x)
            w.write('# candidatas con esa cantidad: %s\n' % (', '.join(porcuenta.get(n, [])) or 'ninguna'))
            w.write('# %s/*.pak @%d = ?\n' % (carpeta, n) if carpeta else '# *.pak @%d = ?\n' % n)
    print('pendientes escritos en %s: %d bloques' % (destino, len(grupos)))


# tamano del juego de cada textura tal como sale de la extraccion; el runtime adapta a el las imagenes editadas.
# Solo agrega las que falten: una vez escrito, el tamano original no cambia aunque se edite el png
def registrar_tamanos(assets):
    ruta = os.path.join(assets, 'tamanos_texturas.txt')
    raiz = os.path.join(assets, 'Packfile')
    hechos = {}
    if os.path.isfile(ruta):
        for linea in open(ruta, encoding='utf-8'):
            c = linea.rstrip('\r\n').split(' ', 2)
            if len(c) == 3:
                hechos[c[2].lower()] = linea.rstrip('\r\n')
    nuevos = 0
    def anotar(clave, png, esc):
        nonlocal nuevos
        if clave.lower() in hechos:
            return
        with open(png, 'rb') as f:
            cab = f.read(24)
        if cab[:8] != b'\x89PNG\r\n\x1a\n':
            return
        w, h = struct.unpack('>II', cab[16:24])
        hechos[clave.lower()] = '%d %d %s' % (max(1, w // esc), max(1, h // esc), clave)
        nuevos += 1
    for dirpath, dirs, nombres in os.walk(raiz):
        rel = os.path.relpath(dirpath, raiz).replace('\\', '/')
        rel = '' if rel == '.' else rel + '/'
        if dirpath.lower().endswith(TEXTURAS):
            for x in nombres:
                if x.lower().endswith('.png'):
                    n, esc = png_de_textura(x)
                    if n is not None:
                        anotar(rel[:-1] + '#%d' % n, os.path.join(dirpath, x), esc)
            continue
        for x in nombres:
            r = png_suelto(x)
            if r:
                anotar(rel + r[0] + '#1', os.path.join(dirpath, x), r[1])
    if nuevos:
        with open(ruta, 'w', encoding='utf-8', newline='\n') as o:
            for k in sorted(hechos):
                o.write(hechos[k] + '\n')
    print('tamanos de texturas: %d registrados, %d nuevos' % (len(hechos), nuevos))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--assets', default=os.path.join(BASE, 'assets'))
    ap.add_argument('--plantillas', default=os.path.join(BASE, 'tools', 'lists', 'paks'))
    ap.add_argument('--reglas', default=os.path.join(BASE, 'tools', 'lists', 'REGLAS_PAK.txt'))
    ap.add_argument('--sugerir-reglas', action='store_true')
    ap.add_argument('--abrir', action='store_true')
    ap.add_argument('--conservar', action='store_true')
    ap.add_argument('--empaquetar', action='store_true')
    ap.add_argument('--limpiar', action='store_true')
    ap.add_argument('--diagnosticar')
    ap.add_argument('--rehacer-vagp', action='store_true')
    ap.add_argument('--cerrar', action='store_true')
    ap.add_argument('--cerrar-aplicar', action='store_true')
    ap.add_argument('--unir-txt1', action='store_true')
    ap.add_argument('--abrir-tex', action='store_true')
    ap.add_argument('--rehacer-tex', action='store_true')
    ap.add_argument('--renombrar', action='store_true')
    ap.add_argument('--tamanos', action='store_true')
    ap.add_argument('--iso', default=os.path.join(BASE, 'game', 'game.iso'))
    ap.add_argument('--eboot', default=os.path.join(BASE, 'game', 'EBOOT.OLD'))
    a = ap.parse_args()

    raiz = os.path.join(a.assets, 'Packfile')
    rutas = rutas_manifiesto(a.assets)
    con_pak = [x for x in rutas if x.endswith('.pak') and os.path.isfile(os.path.join(raiz, x))]
    abiertos = [x for x in rutas if x.endswith('.pak') and not os.path.isfile(os.path.join(raiz, x)) and os.path.isdir(os.path.join(raiz, os.path.splitext(x)[0]))]
    con_txt1 = [x for x in rutas if x.endswith('.txt1') and os.path.isfile(os.path.join(raiz, x))]
    txt1_abiertos = [x for x in rutas if x.endswith('.txt1') and os.path.isdir(os.path.join(raiz, x))]
    print('%d paks en el manifiesto: %d cerrados, %d abiertos; %d txt1: %d cerrados, %d abiertos' % (sum(1 for x in rutas if x.endswith('.pak')), len(con_pak), len(abiertos), len(con_txt1) + len(txt1_abiertos), len(con_txt1), len(txt1_abiertos)))

    if a.limpiar:
        n, m = 0, 0
        for rel in con_pak:
            carpeta = os.path.join(raiz, os.path.splitext(rel)[0])
            if os.path.isdir(carpeta) and borrar_carpeta(carpeta):
                n += 1
        for dirpath, _, nombres in os.walk(raiz):
            for x in nombres:
                if x in ('_pak.txt', '_cola.bin'):
                    os.remove(os.path.join(dirpath, x))
                    m += 1
        print('borradas %d carpetas que aun tenian su .pak y %d archivos de metadatos sueltos' % (n, m))
        return

    if a.diagnosticar:
        diagnosticar(os.path.join(raiz, a.diagnosticar))
        return

    if a.unir_txt1:
        n, m, fallos = 0, 0, []
        for carpeta in carpetas_txt1(raiz):
            try:
                k = unir_txt1(carpeta)
            except ValueError as e:
                fallos.append(str(e))
                continue
            if k:
                n += 1
                m += k
        print('unidos %d txt1 (%d cadenas) en texto.txt; %d con problemas' % (n, m, len(fallos)))
        for x in fallos:
            print('  ' + x)
        return

    if a.empaquetar:
        n = 0
        for rel in abiertos:
            carpeta = os.path.join(raiz, os.path.splitext(rel)[0])
            with open(os.path.join(raiz, rel), 'wb') as o:
                o.write(empaquetar(carpeta))
            shutil.rmtree(carpeta)
            n += 1
        for rel in txt1_abiertos:
            carpeta = os.path.join(raiz, rel)
            datos = empaquetar_txt1(carpeta)
            shutil.rmtree(carpeta)
            with open(carpeta, 'wb') as o:
                o.write(datos)
            n += 1
        print('empaquetados %d contenedores' % n)
        return

    plantillas = leer_plantillas(a.plantillas)
    porcuenta = collections.defaultdict(list)
    for p in plantillas:
        porcuenta[len(plantillas[p])].append(p)
    reglas = leer_reglas(a.reglas)
    if a.sugerir_reglas:
        sugerir(con_pak, raiz, reglas, porcuenta, a.reglas + '.pendientes')
        return
    if a.renombrar:
        ctx = {'reglas': reglas, 'porcuenta': porcuenta, 'plantillas': plantillas, 'sin': collections.Counter()}
        cambios, abiertos_r, raros = renombrar_por_contenido(raiz, set(rutas), ctx, os.path.join(BASE, 'muestras', 'raros'))
        print('renombrados %d archivos por su contenido; abiertos %s; %d sin identificar' % (len(cambios), dict(abiertos_r), len(raros)))
        for x in cambios:
            print('  renombrado: %s' % x)
        for rel, d, _ in raros:
            print('  sin identificar: %s (%d bytes, %s)' % (rel, len(d), d[:16].hex()))
        return
    if a.tamanos:
        registrar_tamanos(a.assets)
        return
    if a.abrir_tex or a.rehacer_tex:
        n, vacios, problemas = abrir_tex_pendientes(raiz, a.rehacer_tex)
        print('abiertos %d contenedores de texturas como png, %d vacios quedan como archivo, %d sin abrir' % (n, vacios, len(problemas)))
        for x in problemas[:40]:
            print('  sin abrir: %s' % x)
        registrar_tamanos(a.assets)
        return
    if a.cerrar or a.cerrar_aplicar:
        cerrar(raiz, a.assets, a.iso, a.eboot, reglas, porcuenta, a.cerrar_aplicar)
        return
    if a.rehacer_vagp:
        rehacer_vagp(raiz, a.assets, a.iso, a.eboot, {'reglas': reglas, 'porcuenta': porcuenta, 'plantillas': plantillas, 'sin': collections.Counter()})
        return
    if not a.abrir:
        return

    hechos, distintos, desorden, sin = collections.Counter(), [], [], collections.Counter()
    ctx = {'reglas': reglas, 'porcuenta': porcuenta, 'plantillas': plantillas, 'sin': sin}
    for rel in con_txt1:
        ruta = os.path.join(raiz, rel)
        with open(ruta, 'rb') as f:
            d = f.read()
        if not abrir_txt1(ruta, d):
            distintos.append(rel + ' (no parece txt1)')
            continue
        if not mismo_txt1(d, empaquetar_txt1(ruta)):
            distintos.append(rel + ' (txt1 no coincide al rearmar)')
        hechos['txt1'] += 1
    for rel in con_pak:
        ruta = os.path.join(raiz, rel)
        with open(ruta, 'rb') as f:
            d = f.read()
        r = analizar(d)
        if not r:
            continue
        nombre_plantilla = plantilla_de(rel, r['n'], reglas, porcuenta)
        if nombre_plantilla == '!':
            continue
        if not nombre_plantilla:
            sin[(os.path.dirname(rel), r['n'])] += 1
            continue
        plant = plantillas.get(nombre_plantilla, {}) if nombre_plantilla != '-' else {}
        carpeta = abrir(ruta, d, r, plant)
        if not carpeta:
            desorden.append('%s (plantilla %s)' % (rel, nombre_plantilla))
            continue
        if mismo_contenido(empaquetar(carpeta), d):
            if not a.conservar:
                os.remove(ruta)
        else:
            distintos.append(rel)
        hechos[nombre_plantilla] += 1
        a2, p2 = abrir_anidados(carpeta, rel, ctx)
        hechos.update(a2)
        distintos += p2
    # paks ya abiertos en sesiones anteriores: solo se abre lo anidado que quede cerrado
    for rel in abiertos:
        a2, p2 = abrir_anidados(os.path.join(raiz, os.path.splitext(rel)[0]), rel, ctx)
        hechos.update(a2)
        distintos += p2
    print('abiertos %d contenedores%s' % (sum(hechos.values()), (', ' + ', '.join('%s x%d' % kv for kv in hechos.most_common(10))) if hechos else ''))
    print('contenido distinto al reempaquetar: %d' % len(distintos))
    for s in distintos[:20]:
        print('  %s' % s)
    if desorden:
        print('plantillas cuyo orden de nombres no sigue al indice (no se abren): %d' % len(desorden))
        for s in desorden[:10]:
            print('  %s' % s)
    if sin:
        print('sin plantilla: %d grupos' % len(sin))
        for (c, n), v in sin.most_common(12):
            print('  %s, %d entradas, %d paks' % (c or '(raiz)', n, v))
    registrar_tamanos(a.assets)


if __name__ == '__main__':
    main()
