import argparse, os, sys, collections

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import open_paks as op

BASE = op.BASE


def raices(assets):
    r = [('assets', os.path.join(assets, 'Packfile')), ('assets', os.path.join(assets, 'Shared'))]
    mods = os.path.join(BASE, 'mods')
    if os.path.isdir(mods):
        for m in sorted(os.listdir(mods)):
            for sub in ('Packfile', 'Shared'):
                if os.path.isdir(os.path.join(mods, m, sub)):
                    r.append(('mod ' + m, os.path.join(mods, m, sub)))
    return r


def revisar(assets):
    filas = []
    for origen, raiz in raices(assets):
        if not os.path.isdir(raiz):
            continue
        for dirpath, dirs, nombres in os.walk(raiz):
            dirs[:] = [x for x in dirs if not x.lower().endswith('.vagp')]
            for x in nombres:
                ruta = os.path.join(dirpath, x)
                rel = os.path.relpath(ruta, os.path.dirname(raiz)).replace('\\', '/')
                low = x.lower()
                try:
                    with open(ruta, 'rb') as f:
                        cab = f.read(16)
                except OSError:
                    continue
                tam = os.path.getsize(ruta)
                if low.endswith('.vagp'):
                    if not tam:
                        continue
                    tipo = 'vagp_convertible' if op.tabla_ppva(dirpath, op.prefijo_num(x)) else 'vagp_sin_tabla'
                elif low.endswith('.vag') and op.tipo_por_firma(cab):
                    tipo = 'mal_nombrado_' + op.tipo_por_firma(cab)
                elif low.endswith('.vag') or cab[:4] == b'VAGp':
                    tipo = 'vag_suelto'
                else:
                    continue
                filas.append((tipo, origen, rel, tam, cab.hex()))
    return filas


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--assets', default=os.path.join(BASE, 'assets'))
    ap.add_argument('--apply', action='store_true')
    a = ap.parse_args()
    filas = revisar(a.assets)
    with open(os.path.join(BASE, 'audio_audit.txt'), 'w', encoding='utf-8', newline='\n') as o:
        for f in filas:
            o.write('\t'.join(str(v) for v in f) + '\n')
    tot = collections.Counter((f[0], f[1]) for f in filas)
    for (t, origen), n in sorted(tot.items()):
        print('audio: %s en %s: %d' % (t, origen, n))
    grupos = collections.Counter((f[0], '/'.join(f[2].split('/')[:4])) for f in filas)
    for (t, g), n in grupos.most_common(40):
        print('audio: %s %s: %d' % (t, g, n))
    if not filas:
        print('audio: todo convertido')
    if not a.apply:
        return
    if os.path.isfile(os.path.join(a.assets, 'shared.txt')):
        print('audio: primero python3 tools/unify_assets.py --undo')
        return
    plantillas = op.leer_plantillas(os.path.join(BASE, 'tools', 'lists', 'paks'))
    porcuenta = collections.defaultdict(list)
    for p in plantillas:
        porcuenta[len(plantillas[p])].append(p)
    ctx = {'reglas': op.leer_reglas(os.path.join(BASE, 'tools', 'lists', 'REGLAS_PAK.txt')), 'porcuenta': porcuenta, 'plantillas': plantillas, 'sin': collections.Counter()}
    raiz = os.path.join(a.assets, 'Packfile')
    manifiesto = set(op.rutas_manifiesto(a.assets))
    renombrados = 0
    for t, origen, rel, _tam, _c in filas:
        if not t.startswith('mal_nombrado_') or origen != 'assets' or not rel.startswith('Packfile/'):
            continue
        ruta = os.path.join(a.assets, rel)
        carpeta, x = os.path.split(ruta)
        tipo = t[len('mal_nombrado_'):]
        nuevo = op.renombrar_tipo(x, tipo)
        i = op.prefijo_num(x)
        hermanos = [y for y in os.listdir(carpeta) if os.path.isfile(os.path.join(carpeta, y))]
        sub = op.sub_manifiesto(os.path.relpath(carpeta, raiz).replace('\\', '/'), manifiesto)
        nom = op.plantilla_de(sub, len(hermanos), ctx['reglas'], ctx['porcuenta'])
        plant = ctx['plantillas'].get(nom, {}) if nom and nom not in ('-', '!') else {}
        if i in plant and plant[i][1].lower().endswith('.' + tipo):
            nuevo = plant[i][1]
        destino = os.path.join(carpeta, nuevo)
        if os.path.exists(destino):
            print('audio: ya existe %s' % destino)
            continue
        os.replace(ruta, destino)
        renombrados += 1
    print('audio: renombrados %d archivos por su firma' % renombrados)
    hechos, fallos = 0, []
    for t, origen, rel, _tam, _c in filas:
        if t != 'vagp_convertible' or origen != 'assets' or not rel.startswith('Packfile/'):
            continue
        ruta = os.path.join(a.assets, rel)
        with open(ruta, 'rb') as f:
            d = f.read()
        sub = op.sub_manifiesto(rel[len('Packfile/'):], manifiesto)
        if op.abrir_vagp(ruta, d, os.path.dirname(ruta), sub, ctx):
            hechos += 1
        else:
            fallos.append(rel)
    print('audio: abiertos %d vagp como wav, %d fallos' % (hechos, len(fallos)))
    for x in fallos[:20]:
        print('audio: no se pudo %s' % x)


if __name__ == '__main__':
    main()
