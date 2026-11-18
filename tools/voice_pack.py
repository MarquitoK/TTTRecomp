#!/usr/bin/env python3
# Arma un mod de idioma de voces desde otra extraccion del juego (por ejemplo assets_jp de la version japonesa):
# cada entrada de audio que difiere de assets se copia (wav, o el pak de SFX) en mods/<mod>/Lang/<mod>/Packfile con la ruta de assets.
#   --origen assets_jp   extraccion con expose_at3.py ya aplicado (los .unk convertidos en .wav)
#   --mod voces_jp       carpeta del mod dentro de mods/
#   --voces "Japanese|Japonés"   nombre del idioma en ingles|espanol
#   --idioma jp|lat|es|ptbr   id, nombre, voces y codigo del idioma ya preparados
#   --carpeta DIR        en lugar de --origen: una carpeta con las rutas de Packfile (Characters/.../Voices/*.wav, sfx.pak, ...)
#   --base               crea <mod>.mkpak en la raiz: mod base vacio (mod.txt, lista_audios.txt, Lang/<mod>/Packfile) para modders
#   --plantilla          crea el mod vacio y mods/<mod>/lista_audios.txt con las rutas de todos los audios a grabar
#   --revisar            solo cuenta, no copia
import argparse, os, shutil, sys, zipfile
from collections import Counter

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def leer(ruta):
    r = {}
    for l in open(ruta, encoding='utf-8'):
        if l.startswith('#'):
            continue
        c = l.rstrip('\n').split('\t')
        r[int(c[0])] = (int(c[2]), c[5], c[6])
    return r


# lo que lleva el mod: voces y SFX de personajes, audios de batalla y del titulo, interacciones especiales y voces del modo historia
def categoria(ruta):
    p = ruta.split('/')
    if p[0] == 'Characters' and len(p) >= 3:
        if p[-2] == 'Voices' and ruta.lower().endswith('.unk'):
            return 'voces de personajes'
        if p[-1].lower() == 'sfx.pak':
            return 'sfx de personajes'
    if ruta.startswith('General_Audios/In_Game/'):
        return 'audios en batalla'
    if ruta.startswith('General_Audios/Title/'):
        return 'audios del titulo'
    if ruta.startswith('Special_Interactions/'):
        return 'interacciones especiales'
    if ruta.startswith('Story_Mode/Chapters/') and len(p) >= 4 and p[3] == 'Voices':
        return 'voces del modo historia'
    return None


IDIOMAS = {
    'jp': ('voces_jp', 'Voces japonesas', 'Japanese|Japonés|Japonais', 'JP'),
    'lat': ('voces_lat', 'Voces en español latino', 'Latin American Spanish|Español latino|Espagnol latino', 'LAT'),
    'es': ('voces_es', 'Voces en español castellano', 'Castilian Spanish|Español castellano|Espagnol castillan', 'ES'),
    'ptbr': ('voces_ptbr', 'Voces en portugués de Brasil', 'Brazilian Portuguese|Portugués de Brasil|Portugais brésilien', 'PTBR'),
}


def escribir_mod(destino, a, n):
    os.makedirs(os.path.join(destino, 'Lang', a.mod, 'Packfile'), exist_ok=True)
    with open(os.path.join(destino, 'mod.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('id=%s\nnombre=%s\nvoces=%s\ncodigo=%s\nparches=%s\n' % (a.mod, a.nombre, a.voces, a.codigo, a.parches))
    print('mod creado: mods/%s con %d audios' % (a.mod, n))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--origen', default=os.path.join(BASE, 'assets_jp'))
    ap.add_argument('--assets', default=os.path.join(BASE, 'assets'))
    ap.add_argument('--mod', default='voces_jp')
    ap.add_argument('--nombre', default='Voces japonesas')
    ap.add_argument('--voces', default='Japanese|Japonés|Japonais')
    ap.add_argument('--parches', default='voice_lang')
    ap.add_argument('--codigo', default='JP')
    ap.add_argument('--idioma', choices=sorted(IDIOMAS))
    ap.add_argument('--carpeta')
    ap.add_argument('--plantilla', action='store_true')
    ap.add_argument('--base', action='store_true')
    ap.add_argument('--revisar', action='store_true')
    a = ap.parse_args()
    if a.idioma:
        a.mod, a.nombre, a.voces, a.codigo = IDIOMAS[a.idioma]
    destino = os.path.join(BASE, 'mods', a.mod)
    us = leer(os.path.join(a.assets, 'manifiesto.txt'))
    if a.plantilla or a.carpeta or a.base:
        rutas = []
        for i in sorted(us):
            ruta = us[i][2]
            if categoria(ruta) and us[i][0] > 0:
                rutas.append((categoria(ruta), ruta[:-4] + '.wav' if ruta.lower().endswith('.unk') else ruta))
        lista = '# audios del idioma: graba cada uno con su ruta dentro de Lang/%s/Packfile/ (wav; sfx.pak de VAG)\n' % a.mod
        lista += ''.join('%s\t%s\n' % (cat, rel) for cat, rel in rutas)
        if a.base:
            mt = 'id=%s\nnombre=%s\nvoces=%s\ncodigo=%s\nparches=%s\n' % (a.mod, a.nombre, a.voces, a.codigo, a.parches)
            leeme = ('Pon aqui los audios del idioma con la misma ruta que en lista_audios.txt,\n'
                     'por ejemplo Characters/1_Goku/1_Base/Voices/078_taunt.wav (wav) o Characters/1_Goku/1_Base/sfx.pak.\n'
                     'Lo que no pongas se oye con las voces originales.\n')
            out = os.path.join(BASE, a.mod + '.mkpak')
            with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
                z.writestr(a.mod + '/mod.txt', mt)
                z.writestr(a.mod + '/lista_audios.txt', lista)
                z.writestr(a.mod + '/Lang/' + a.mod + '/Packfile/LEEME.txt', leeme)
            print('mod base: %s.mkpak (%d audios en la lista)' % (a.mod, len(rutas)))
            return
        if a.plantilla:
            if a.revisar:
                print('revision: la lista tendria %d audios' % len(rutas))
                return
            escribir_mod(destino, a, 0)
            with open(os.path.join(destino, 'lista_audios.txt'), 'w', encoding='utf-8', newline='\n') as f:
                f.write(lista)
            print('plantilla: mods/%s/lista_audios.txt con %d audios' % (a.mod, len(rutas)))
            return
        cuenta = Counter()
        for cat, rel in rutas:
            src = os.path.join(a.carpeta, rel)
            if not os.path.isfile(src):
                continue
            cuenta[cat] += 1
            if not a.revisar:
                d = os.path.join(destino, 'Lang', a.mod, 'Packfile', rel)
                os.makedirs(os.path.dirname(d), exist_ok=True)
                shutil.copyfile(src, d)
        for k, v in sorted(cuenta.items()):
            print('  %s: %d audios' % (k, v))
        if a.revisar:
            print('revision: %d audios se copiarian a mods/%s' % (sum(cuenta.values()), a.mod))
            return
        escribir_mod(destino, a, sum(cuenta.values()))
        return
    jp = leer(os.path.join(a.origen, 'manifiesto.txt'))
    if len(us) != len(jp):
        raise SystemExit('los manifiestos tienen %d y %d entradas: no se pueden emparejar por indice' % (len(us), len(jp)))
    cuenta, faltan = Counter(), 0
    for i in sorted(us):
        ruta = us[i][2]
        cat = categoria(ruta)
        if not cat or jp[i][0] == 0 or us[i][:2] == jp[i][:2]:
            continue
        rel = ruta[:-4] + '.wav' if ruta.lower().endswith('.unk') else ruta
        src = os.path.join(a.origen, 'Packfile', rel)
        if not os.path.isfile(src):
            faltan += 1
            continue
        cuenta[cat] += 1
        if not a.revisar:
            d = os.path.join(destino, 'Lang', a.mod, 'Packfile', rel)
            os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copyfile(src, d)
    for k, v in sorted(cuenta.items()):
        print('  %s: %d audios' % (k, v))
    if faltan:
        print('aviso: %d entradas sin archivo en el origen (huecos vacios en la version de origen)' % faltan)
    if a.revisar:
        print('revision: %d audios se copiarian a mods/%s' % (sum(cuenta.values()), a.mod))
        return
    escribir_mod(destino, a, sum(cuenta.values()))


if __name__ == '__main__':
    main()
