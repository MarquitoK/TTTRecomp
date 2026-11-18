#!/usr/bin/env python3
# Technique names per character (item G 61): each colour's 1_p gets 043_move_list_<lang>.txt with the name and the command
# of every record of its 043_move_list_name.dat, taken from the system text of that language. The port reads these files.
import argparse, os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import open_paks as ap

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
IDIOMAS = ('en', 'es')
DAT = '043_move_list_name.dat'


def nombre_txt(lang):
    return '043_move_list_%s.txt' % lang


def textos_system(assets, lang):
    carpeta = os.path.join(assets, 'Packfile', 'Main', 'system_' + lang, '2_general', '003_system.txt1')
    return ap.leer_texto(carpeta) if os.path.isdir(carpeta) else None


def registros(d):
    out = []
    for i in range(0, len(d) - 31, 32):
        r = struct.unpack_from('<8i', d, i)
        if r[0] == -1:
            break
        out.append(r)
    return out


def escribir(ruta, recs, sys_t):
    with open(ruta, 'w', encoding='utf-8-sig', newline='\n') as o:
        for k, r in enumerate(recs, 1):
            nom = sys_t[r[1]] if 0 <= r[1] < len(sys_t) else ''
            com = sys_t[r[2]] if 0 <= r[2] < len(sys_t) else ''
            o.write('[%d name]\n%s\n[%d command]\n%s\n' % (k, nom, k, com))


def leer(ruta):
    with open(ruta, encoding='utf-8-sig') as f:
        s = f.read().replace('\r', '')
    partes = re.split(r'^\[(\d+) (name|command)\]\n', s, flags=re.M)
    out = {}
    for i in range(1, len(partes), 3):
        out[(int(partes[i]), partes[i + 1])] = partes[i + 2][:-1] if partes[i + 2].endswith('\n') else partes[i + 2]
    return out


def colores(assets):
    raiz = os.path.join(assets, 'Packfile', 'Characters')
    for dp, dn, fn in os.walk(raiz):
        if os.path.basename(dp) == '1_p' and DAT in fn:
            yield dp


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--assets', default=os.path.join(BASE, 'assets'))
    p.add_argument('--apply', action='store_true')
    p.add_argument('--overwrite', action='store_true')
    a = p.parse_args()
    sys_t = {l: textos_system(a.assets, l) for l in IDIOMAS}
    faltan = [l for l in IDIOMAS if sys_t[l] is None]
    if faltan:
        print('aviso: sin texto de system para %s' % ', '.join(faltan))
    hechos = existentes = vacios = 0
    for dp in colores(a.assets):
        with open(os.path.join(dp, DAT), 'rb') as f:
            recs = registros(f.read())
        if not recs:
            vacios += 1
            continue
        for l in IDIOMAS:
            if sys_t[l] is None:
                continue
            ruta = os.path.join(dp, nombre_txt(l))
            if os.path.isfile(ruta) and not a.overwrite:
                existentes += 1
                continue
            if a.apply:
                escribir(ruta, recs, sys_t[l])
            hechos += 1
    print('move lists: %d archivos %s, %d ya existian, %d colores sin registros%s' % (hechos, 'escritos' if a.apply else 'por escribir', existentes, vacios,
          '' if a.apply else ' (usa --apply)'))


if __name__ == '__main__':
    main()
