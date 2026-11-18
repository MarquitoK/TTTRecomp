#!/usr/bin/env python3
# Toon palettes (TPAL) helper for the port's editor:
#   --list                 tipo, clave, nombre, parte, carpeta destino, abierto, modelo, origen (mod id o assets), carpeta de assets
#                          la carpeta destino es la del mod activo de mas prioridad que trae ese color, si no la de assets
#   --open CLAVE PARTE     abre como carpeta el .pak cerrado de ese color en el mismo lugar (mod o assets)
import argparse, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import characters as ch


def mods_activos(assets):
    raiz = os.environ.get('TTT_MODS') or os.path.join(assets, '..', 'mods')
    if not os.path.isdir(raiz):
        return raiz, []
    carpetas = sorted(x for x in os.listdir(raiz) if os.path.isdir(os.path.join(raiz, x)))
    orden, vistos = [], set()
    cfg = os.path.join(raiz, 'mods.cfg')
    if os.path.isfile(cfg):
        for l in open(cfg, encoding='utf-8', errors='replace'):
            l = l.rstrip('\r\n ')
            if len(l) < 3 or l[0] not in '+-' or l[1] != ' ' or l[2:] not in carpetas or l[2:] in vistos:
                continue
            vistos.add(l[2:])
            orden.append((l[2:], l[0] == '+'))
    orden += [(c, True) for c in carpetas if c not in vistos]
    return raiz, [m for m, on in orden if on]


# primer mod activo (mas prioridad) que trae el color como .pak o carpeta
def destino(assets, rel):
    raiz_mods, mods = mods_activos(assets)
    for m in mods:
        pk = os.path.join(raiz_mods, m, 'Packfile')
        origen, abierto = ch.en_disco(pk, rel)
        if origen:
            return pk, origen, abierto, m
    pk = os.path.join(assets, 'Packfile')
    origen, abierto = ch.en_disco(pk, rel)
    return pk, origen, abierto, 'assets'


def por_numero(carpeta, num):
    if not os.path.isdir(carpeta):
        return None
    for x in sorted(os.listdir(carpeta)):
        m = re.match(r'(\d+)', x)
        if m and int(m.group(1)) == num and os.path.isfile(os.path.join(carpeta, x)):
            return os.path.join(carpeta, x)
    return None


def modelo(assets, rel, origen, abierto):
    if not abierto:
        return origen
    if abierto:
        r = por_numero(os.path.join(origen, '1_p'), 2)
        if r:
            return r
    base, ab = ch.en_disco(os.path.join(assets, 'Packfile'), rel)
    return por_numero(os.path.join(base, '1_p'), 2) if base and ab else ''


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--assets', default=os.path.join(ch.BASE, 'assets'))
    p.add_argument('--list', action='store_true')
    p.add_argument('--open', nargs=2, metavar=('CLAVE', 'PARTE'))
    a = p.parse_args()
    if a.list:
        for u in ch.unidades(a.assets):
            if u['tipo'] not in ('personajes', 'extra', 'historia'):
                continue
            for parte in ch.partes(u):
                if parte in ('sfx', 'voces'):
                    continue
                rel = ch.rel_de(u, parte)
                if not rel:
                    continue
                pk, origen, abierto, quien = destino(a.assets, rel)
                if not origen:
                    continue
                base, base_ab = ch.en_disco(os.path.join(a.assets, 'Packfile'), rel)
                print('%s\t%s\t%s\t%s\t%s\t%d\t%s\t%s\t%s' % (u['tipo'], u['clave'], u['nombre'], parte, os.path.abspath(origen if abierto else origen[:-4]),
                                                          1 if abierto else 0, os.path.abspath(modelo(a.assets, rel, origen, abierto) or ''), quien,
                                                          os.path.abspath(base) if base and base_ab else ''))
    elif a.open:
        u = ch.buscar(a.assets, a.open[0])
        rel = ch.rel_de(u, a.open[1])
        pk, origen, abierto, quien = destino(a.assets, rel)
        if not origen:
            raise SystemExit('falta: %s' % rel)
        if not abierto:
            ch.abrir_cerrado(pk, rel, ch.contexto())
            origen, abierto = ch.en_disco(pk, rel)
        print(os.path.abspath(origen))
    else:
        p.print_help()


if __name__ == '__main__':
    main()
