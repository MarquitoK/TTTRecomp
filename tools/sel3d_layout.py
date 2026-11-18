# select layout of the 3D view (item 33): the editor's entries of patch_in/char_select_en.pak into games/ttt/select3d/layout
# usage: python3 tools/sel3d_layout.py 35 39 [22:5 = texture 5 of RHG 22] [--pak patch_in/char_select_en.pak] [--check patch_in/char_select_es.pak]
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import open_paks as op

OUT = 'games/ttt/select3d/layout'


def entries(path):
    d = open(path, 'rb').read()
    n = struct.unpack('>I', d[:4])[0]
    off = [struct.unpack('>I', d[4 + 4 * i:8 + 4 * i])[0] for i in range(n + 1)]
    return [d[off[i]:off[i + 1]] for i in range(n)]


def main(argv):
    pak, chk, items = 'patch_in/char_select_en.pak', 'patch_in/char_select_es.pak', []
    i = 0
    while i < len(argv):
        if argv[i] == '--pak': pak = argv[i + 1]; i += 2; continue
        if argv[i] == '--check': chk = argv[i + 1]; i += 2; continue
        items.append(argv[i]); i += 1
    a = entries(pak)
    b = entries(chk) if chk and os.path.isfile(chk) else None
    os.makedirs(OUT, exist_ok=True)
    for it in items:
        if ':' in it:
            e, t = (int(x) for x in it.split(':'))
            texs = op.texturas_de('x.rhg', a[e - 1])
            if b and op.texturas_de('x.rhg', b[e - 1])[t - 1] != texs[t - 1]:
                print('aviso: la textura %d del RHG %d no es igual en los dos idiomas' % (t, e))
            tw, th, bpp, idx, pal = texs[t - 1]
            d = os.path.join(OUT, '%03d.rhg' % e)
            os.makedirs(d, exist_ok=True)
            op.escribir_png_indexado(os.path.join(d, '%02d.png' % t), tw, th, idx, pal)
            print('RHG %d textura %d: %dx%d' % (e, t, tw, th))
            continue
        e = int(it)
        x = a[e - 1]
        if x[:4] != b'AGS\x00':
            print('entrada %d: no es un AGS, se omite' % e); continue
        if b and b[e - 1] != x:
            print('aviso: el AGS %d no es igual en los dos idiomas' % e)
        open(os.path.join(OUT, '%03d.ags' % e), 'wb').write(x)
        print('AGS %d: %d bytes' % (e, len(x)))


if __name__ == '__main__':
    main(sys.argv[1:])
