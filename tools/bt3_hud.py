#!/usr/bin/env python3
# BT3 battle HUD as a TTT mod: <mod>.mkpak in the root (mod.txt + BT3Hud/common|es|en, one PNG per texture and palette)
#   --iso BT3_US.iso --iso BT3_EN.iso [--entry 5] [--afs PZS3US1.AFS]   one disc per language, detected from the file
#   --pak Battle_ES.pak --pak Battle_EN.pak                               HUD files already extracted
#   --skin DIR   technique list skin list_l/list_m/list_r.png (default tools/bt3_hud_list)
import argparse, hashlib, os, struct, sys, zipfile, zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bt3_open as b3
import char_ref_bt3 as crb

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SHEETS = {0: 'gauge', 3: 'notice', 4: 'combo'}
SUBS = {(0, 0): 3, (0, 8): 6}
IDIOMA = {'f1a2886b6bb87550': 'es', 'c61ecc6361b12e21': 'en'}
SKIP = {(0, 4), (0, 5), (0, 23), (0, 24), (3, 8)}
TEXT = {(3, k) for k in range(7)} | {(4, k) for k in range(2, 8)}


def png(w, h, px):
    filas = b''.join(b'\x00' + px[y * w * 4:(y + 1) * w * 4] for y in range(h))
    t = b3.ap.trozo_png
    return b'\x89PNG\r\n\x1a\n' + t(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + t(b'IDAT', zlib.compress(filas, 9)) + t(b'IEND', b'')


def pak(d):
    n = struct.unpack_from('<I', d, 0)[0] if len(d) >= 8 else 0
    if not 8 <= n <= 1024 or 8 + 4 * n > len(d):
        return None
    o = struct.unpack_from('<%dI' % (n + 1), d, 4)
    if any(o[i] > o[i + 1] for i in range(n)) or o[n] > len(d):
        return None
    return [d[o[i]:o[i + 1]] for i in range(n)]


def una(sheet, k, pal_k=None):
    e = 32 + 64 * k
    one = bytearray(sheet)
    struct.pack_into('<I', one, 0, 1)
    one[32:96] = sheet[e:e + 64]
    if pal_k is not None:
        one[36:40] = sheet[32 + 64 * pal_k + 4:32 + 64 * pal_k + 8]
        one[44:48] = sheet[32 + 64 * pal_k + 12:32 + 64 * pal_k + 16]
    r = b3.texturas_dbt(bytes(one))
    return r[0] if r else None


def rgba(t):
    w, h, idx, pal = t
    if idx is None:
        return w, h, pal
    px = bytearray(w * h * 4)
    for i, c in enumerate(idx):
        p = pal[4 * c:4 * c + 4]
        if len(p) == 4:
            px[4 * i:4 * i + 4] = bytes((p[0], p[1], p[2], b3.alfa(p[3])))
    return w, h, bytes(px)


def extraer(d, z, mod, hechos):
    p = pak(d)
    if not p or len(p) < 5 or any(len(p[k]) < 32 or struct.unpack_from('<I', p[k], 4)[0] != 8 for k in SHEETS):
        sys.exit('hud: el archivo no es el HUD de batalla de BT3')
    lang = IDIOMA.get(hashlib.sha1(p[3]).hexdigest()[:16])
    if not lang:
        sys.exit('hud: HUD de BT3 en un idioma sin soporte (solo en y es)')
    n = 0
    for sh, parte in SHEETS.items():
        s = p[sh]
        for k in range(struct.unpack_from('<I', s, 0)[0]):
            t = una(s, k) if (sh, k) not in SKIP else None
            if t is None:
                continue
            carpeta = lang if (sh, k) in TEXT else 'common'
            for sub in range(SUBS.get((sh, k), 1)):
                nombre = '%s/BT3Hud/%s/%s_%02d%s.png' % (mod, carpeta, parte, k, '_%d' % sub if sub else '')
                if nombre in hechos:
                    continue
                hechos.add(nombre)
                w, h, px = rgba(t if sub == 0 else una(s, k, k + sub))
                z.writestr(nombre, png(w, h, px))
                n += 1
    print('hud: %d texturas (%s)' % (n, lang))
    return lang


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--iso', action='append', default=[])
    ap.add_argument('--pak', action='append', default=[])
    ap.add_argument('--entry', type=int, default=5)
    ap.add_argument('--afs', default='PZS3US1.AFS')
    ap.add_argument('--mod', default='bt3_hud')
    ap.add_argument('--skin', default=os.path.join(BASE, 'tools', 'bt3_hud_list'))
    a = ap.parse_args()
    datos = [open(x, 'rb').read() for x in a.pak]
    for iso in a.iso:
        f, ents = crb.afs(iso, a.afs)
        datos.append(crb.leer(f, ents[a.entry]))
    if not datos:
        ap.print_help()
        return
    out = os.path.join(BASE, a.mod + '.mkpak')
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr(a.mod + '/mod.txt', 'id=%s\nnombre=BT3 Life Bars\nversion=1\netiquetas=Textures\ndescripcion=Budokai Tenkaichi 3 battle HUD: life bars, timer, combo counter and battle notices.\n' % a.mod)
        hechos = set()
        langs = [extraer(d, z, a.mod, hechos) for d in datos]
        piel = [n for n in ('list_l', 'list_m', 'list_r') if os.path.isfile(os.path.join(a.skin, n + '.png'))]
        for n in piel:
            z.write(os.path.join(a.skin, n + '.png'), '%s/BT3Hud/common/%s.png' % (a.mod, n))
        print('hud: piel de la lista %s' % ('con %d piezas' % len(piel) if len(piel) == 3 else 'incompleta (%d de 3), sin piel' % len(piel) if piel else 'no encontrada'))
    print('hud: %s (%s)' % (os.path.normpath(out), ', '.join(langs)))


if __name__ == '__main__':
    main()
