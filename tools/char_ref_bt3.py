#!/usr/bin/env python3
# Character Reference (G 63) data straight from the BT3 PS2 ISO (or its DATA/PZS3US1.AFS texts, DATA/PZS3US2.AFS voices) into mods/<mod>/CharRef
#   --iso BT3.iso --find                    lists the Character Reference paks of the disc (one per text language)
#   --iso BT3.iso --texts --lang en [--entry N]   writes CharRef/<lang>.txt from entry N (0-based) of PZS3US1.AFS, or the only one found
#   --iso BT3.iso --voices --out DIR [--check] [--dest lat]   Chi-Chi lines and two samples per character as .adx in DIR/en and DIR/jp;
#                                                 --dest takes only the English set into DIR/<dest> (dubbed discs)
#   --place DIR --voice-lang en|jp|lat            copies DIR/*.wav into the mod (en = Packfile, others = Lang/voces_<lang>/Packfile)
#   --afs1 / --afs2 take the AFS files instead of the ISO
import argparse, os, re, shutil, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bt3_open as b3
import char_ref as cr

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
CHICHI = 0x82B7 - 0xD48
CHARA = 0x8D4E - 0xD48
CHARA_ALT = 0xCC32 - 0xD48
JP_MENU = 0x55C
COMMENTS = 225
CHARAS = 161


def iso_dir(f, lba, size):
    f.seek(lba * 2048)
    d = f.read(size)
    p, out = 0, []
    while p < len(d):
        n = d[p]
        if n == 0:
            p = (p // 2048 + 1) * 2048
            continue
        ln = d[p + 32]
        if ln and d[p + 33] > 1:
            nombre = d[p + 33:p + 33 + ln].decode('latin-1').split(';')[0].rstrip('.').upper()
            out.append((nombre, struct.unpack_from('<I', d, p + 2)[0], struct.unpack_from('<I', d, p + 10)[0], d[p + 25] & 2))
        p += n
    return out


def iso_buscar(f, nombre):
    f.seek(16 * 2048)
    pvd = f.read(2048)
    if pvd[1:6] != b'CD001':
        sys.exit('cr: el ISO no tiene sistema de archivos ISO 9660')
    raiz = (struct.unpack_from('<I', pvd, 156 + 2)[0], struct.unpack_from('<I', pvd, 156 + 10)[0])
    pend, vistos = [('', raiz[0], raiz[1])], set()
    while pend:
        ruta, lba, size = pend.pop(0)
        if lba in vistos:
            continue
        vistos.add(lba)
        ents = iso_dir(f, lba, size)
        for n, l, sz, es_dir in ents:
            if not es_dir and (n == nombre.upper() or re.fullmatch(nombre.upper().replace('US', '..'), n)):
                print('cr: %s en el ISO: %s/%s' % (nombre, ruta, n))
                return l * 2048
            if es_dir:
                pend.append((ruta + '/' + n, l, sz))
    for n, l, sz, es_dir in iso_dir(f, *raiz):
        print('cr: %s del ISO: %s' % (n, ', '.join(m for m, _, _, _ in iso_dir(f, l, sz)[:60]) if es_dir else 'archivo'))
    sys.exit('cr: %s no esta en el ISO' % nombre)


def afs(path, nombre):
    f = open(path, 'rb')
    base = iso_buscar(f, nombre) if path.lower().endswith('.iso') else 0
    f.seek(base)
    h = f.read(8)
    if h[:4] != b'AFS\x00':
        sys.exit('cr: %s no es un AFS' % nombre)
    n = struct.unpack_from('<I', h, 4)[0]
    t = struct.unpack('<%dI' % (2 * n), f.read(8 * n))
    return f, [(base + t[2 * i], t[2 * i + 1]) for i in range(n)]


def leer(f, e):
    f.seek(e[0])
    return f.read(e[1])


def bpe(d):
    raw, packed = struct.unpack_from('<II', d, 0)
    out = bytearray()
    pos, end = 8, min(len(d), 8 + packed)
    while pos < end and len(out) < raw:
        left = list(range(256))
        right = [0] * 256
        c, count = 0, d[pos]
        pos += 1
        while True:
            if count > 127:
                c += count - 127
                count = 0
            if c == 256:
                break
            for _ in range(count + 1):
                if c >= 256:
                    break
                left[c] = d[pos]
                pos += 1
                if c != left[c]:
                    right[c] = d[pos]
                    pos += 1
                c += 1
            if c == 256:
                break
            count = d[pos]
            pos += 1
        blk = d[pos] << 8 | d[pos + 1]
        pos += 2
        stack = []
        i = 0
        while i < blk or stack:
            if stack:
                ch = stack.pop()
            else:
                ch = d[pos]
                pos += 1
                i += 1
            if ch == left[ch]:
                out.append(ch)
            else:
                stack.append(right[ch])
                stack.append(left[ch])
    return bytes(out[:raw])


def es_cmp1(d, size=None):
    if len(d) < 16:
        return False
    size = len(d) if size is None else size
    raw, packed = struct.unpack_from('<II', d, 0)
    return 8 < packed <= size - 8 and size - 8 - packed < 4096 and packed < raw < 64 << 20


def es_pak(d, lo, hi):
    if len(d) < 16:
        return False
    n = struct.unpack_from('<I', d, 0)[0]
    if not lo <= n <= hi or 4 + 4 * (n + 1) > len(d):
        return False
    o = struct.unpack_from('<%dI' % (n + 1), d, 4)
    return all(o[i] <= o[i + 1] <= len(d) for i in range(n)) and o[0] >= 4 + 4 * (n + 1)


def secciones(d, prof=0):
    if prof > 3:
        return None
    if es_cmp1(d):
        return secciones(bpe(d), prof + 1)
    if es_pak(d, 28, 40):
        s = b3.entradas(d)
        return s if len(s) > 24 and es_pak(s[21], 160, 200) else None
    if es_pak(d, 1, 4):
        for e in b3.entradas(d):
            r = secciones(e, prof + 1)
            if r:
                return r
    return None


def cadenas(d):
    return [b3.texto(e) for e in b3.entradas(d)] if es_pak(d, 1, 4096) else []


def tabla(d):
    return list(struct.unpack('<%di' % (len(d) // 4), d[:len(d) // 4 * 4]))


def textos(s, lang):
    vp = cadenas(s[24])
    voices, profiles = vp[0::2], vp[1::2]
    return cr.texts(lang, cadenas(s[21]), cadenas(s[22]), voices, profiles, cadenas(s[20]), tabla(s[25]), tabla(s[27]), tabla(s[26])), len(profiles)


def buscar(path):
    f, ents = afs(path, 'PZS3US1.AFS')
    print('cr: %s, %d entradas; buscando la referencia de personajes...' % (path, len(ents)))
    n = []
    for i, e in enumerate(ents):
        if e[1] < 0x40000 or e[1] > 0x1000000:
            continue
        f.seek(e[0])
        h = f.read(64)
        cand = es_cmp1(h, e[1])
        if not cand and len(h) >= 16 and 1 <= struct.unpack_from('<I', h, 0)[0] <= 4:
            o0, o1 = struct.unpack_from('<II', h, 4)
            if o0 < o1 <= e[1]:
                f.seek(e[0] + o0)
                cand = es_cmp1(f.read(16), o1 - o0)
        if not cand:
            continue
        s = secciones(leer(f, e))
        if not s:
            continue
        com = cadenas(s[20])
        nom = cadenas(s[21])
        n.append(i)
        print('cr: entrada %d: %d comentarios, %d nombres | %s | %s' % (i, len(com), len(nom), (nom[0] if nom else '')[:20], (com[0] if com else '').replace('\n', ' ')[:60]))
    print('cr: %d paks de referencia encontrados' % len(n))
    return n


def poner_textos(path, entry, lang, mod, check):
    f, ents = afs(path, 'PZS3US1.AFS')
    if entry is None:
        hall = buscar(path)
        if len(hall) != 1:
            sys.exit('cr: %d paks de referencia; elige uno con --entry' % len(hall))
        entry = hall[0]
    if not 0 <= entry < len(ents):
        sys.exit('cr: la entrada %d no existe (%d entradas)' % (entry, len(ents)))
    s = secciones(leer(f, ents[entry]))
    if not s:
        sys.exit('cr: la entrada %d no es la referencia de personajes' % entry)
    lines, perfiles = textos(s, lang)
    root = os.path.join(BASE, 'mods', mod)
    print('cr: entrada %d: %d nombres, %d perfiles, %d comentarios, idioma %s -> %s' % (entry, len(cadenas(s[21])), perfiles, len(cadenas(s[20])), lang, os.path.relpath(root, BASE)))
    if check:
        return
    cr.write_texts(root, lang, lines)
    print('cr: CharRef/%s.txt escrito' % lang)


def adx(d):
    if len(d) < 0x20 or d[0] != 0x80 or d[1] != 0 or d[4] != 3 or d[6] != 4:
        return None
    if d[0x13] in (8, 9):
        return 'cifrado'
    rate, total = struct.unpack_from('>II', d, 8)
    return (d[7], rate, total / float(rate) if rate else 0)


def lista():
    out = []
    for k in range(COMMENTS):
        out.append(('chichi_%03d' % k, CHICHI + JP_MENU + k, CHICHI + k))
    for c in range(CHARAS):
        for l in range(2):
            out.append(('chara_%03d_%d' % (c, l), CHARA_ALT + c * 100 + l, CHARA + c * 100 + l))
    return out


def voces(path, out, check, dest=None):
    if not check:
        voces(path, out, True, dest)
    f, ents = afs(path, 'PZS3US2.AFS')
    print('cr: %s, %d entradas' % (path, len(ents)))
    malos, durs, rates, total = 0, [], set(), 0
    for name, en, jp in lista():
        for lang, i in (((dest, en),) if dest else (('en', en), ('jp', jp))):
            if i >= len(ents):
                malos += 1
                if malos <= 5:
                    print('cr: %s %s: la entrada %d no existe' % (name, lang, i))
                continue
            d = leer(f, ents[i])
            h = adx(d)
            if not isinstance(h, tuple):
                malos += 1
                if malos <= 5:
                    print('cr: %s %s: la entrada %d no es ADX valido (%s)' % (name, lang, i, h or d[:4].hex()))
                continue
            rates.add(h[1])
            durs.append(h[2])
            total += len(d)
            if not check:
                os.makedirs(os.path.join(out, lang), exist_ok=True)
                open(os.path.join(out, lang, name + '.adx'), 'wb').write(d)
    if durs:
        print('cr: %d voces ADX, %.1f MB, %s Hz, duracion %.1f..%.1f s (media %.1f s)' % (len(durs), total / 1048576.0, '/'.join(str(r) for r in sorted(rates)),
              min(durs), max(durs), sum(durs) / len(durs)))
    print('cr: %d voces no validas' % malos)
    if malos:
        sys.exit('cr: el disco no tiene las voces donde las pone BT3; no se escribe nada' if check else 'cr: faltan voces')
    if not check:
        print('cr: ADX en %s (%s); conviertelos a WAV en la misma carpeta y usa --place' % (out, dest or 'en, jp'))


def colocar(folder, lang, mod):
    lang = lang.lower()
    base = os.path.join(BASE, 'mods', mod)
    if lang != 'en':
        base = os.path.join(base, 'Lang', 'voces_' + lang)
    base = os.path.join(base, 'Packfile', 'Menus')
    n, falta = 0, []
    for name, _, _ in lista():
        src = os.path.join(folder, name + '.wav')
        if not os.path.isfile(src):
            falta.append(name)
            continue
        dst = os.path.join(base, 'chara_reference_voices' if name.startswith('chichi') else 'chara_reference_samples')
        os.makedirs(dst, exist_ok=True)
        shutil.copyfile(src, os.path.join(dst, name + '.wav'))
        n += 1
    print('cr: %d wav en %s%s' % (n, os.path.relpath(base, BASE), ', faltan %d (%s...)' % (len(falta), ', '.join(falta[:3])) if falta else ''))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--iso')
    ap.add_argument('--afs1')
    ap.add_argument('--afs2')
    ap.add_argument('--find', action='store_true')
    ap.add_argument('--texts', action='store_true')
    ap.add_argument('--entry', type=int)
    ap.add_argument('--dest')
    ap.add_argument('--lang', default='en', choices=['en', 'es'])
    ap.add_argument('--voices', action='store_true')
    ap.add_argument('--out', default='char_ref_voces')
    ap.add_argument('--place')
    ap.add_argument('--voice-lang', default='en')
    ap.add_argument('--mod', default='char_reference')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    a.afs1 = a.afs1 or a.iso
    a.afs2 = a.afs2 or a.iso
    if a.afs1 and a.find:
        buscar(a.afs1)
    elif a.afs1 and (a.texts or a.entry is not None):
        poner_textos(a.afs1, a.entry, a.lang, a.mod, a.check)
    elif a.afs2 and a.voices:
        voces(a.afs2, a.out, a.check, a.dest.lower() if a.dest else None)
    elif a.place:
        colocar(a.place, a.voice_lang, a.mod)
    else:
        ap.print_help()


if __name__ == '__main__':
    main()
