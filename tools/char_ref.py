#!/usr/bin/env python3
# Builds the Character Reference mod (G 63): mods/<mod>/Packfile/Menus/menu_chara_reference.pak + CharRef/<lang>.txt
#   --json cr_texts.json   texts exported by the menu editor (BT3 CharRef pack)
#   --pak menu_chara_reference.pak
#   --mod char_reference   folder inside mods/
#   --lang es              language of the texts (en, es)
#   --check                only counts, writes nothing
# The pak goes opened as a folder (like assets): the button bar textures reach the adaptive button icons
import argparse, json, os, shutil, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import open_paks as op

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
NAMES = ['01_cr_main.ags', '02_cr_main.rhg', '03_cr_parts.ags', '04_cr_parts.rhg', '05_cr_message.ags', '06_cr_message.rhg', '07_cr_bg.ags',
         '08_cr_bg.rhg', '09_navbar.pak', '10_header.ags', '11_header.rhg', '12_title.ags', '13_empty.bin', '14_title_en.rhg', '15_title_es.rhg',
         '16_title_fr.rhg', '17_empty.bin', '18_empty.bin', '19_chara_render.ags', '20_empty.bin', '21_cr_parts_en.rhg', '22_cr_parts_es.rhg', '23_cr_parts_fr.rhg', '24_navbar_viewer.pak']


def open_pak(d, folder, names):
    r = op.analizar(d)
    if not r:
        sys.exit('cr: el pak no tiene el formato esperado')
    plant = {i + 1: ('', names[i] if i < len(names) else 'sin_nombre_%03d.bin' % (i + 1)) for i in range(r['n'])}
    if not op.abrir(folder + '.pak', d, r, plant, folder):
        sys.exit('cr: no se pudo abrir ' + folder)
    return r['n']


def esc(s):
    return (s or '').replace('\r', '').replace('\n', '\\n')


def texts(lang, names, forms, voices, profiles, comments, first, count, pose):
    lines = ['# Character Reference texts (BT3), ' + lang]
    lines += ['name.%d=%s' % (i, esc(s)) for i, s in enumerate(names)]
    lines += ['form.%d=%s' % (i, esc((s or '').rstrip())) for i, s in enumerate(forms)]
    for i in range(len(profiles)):
        lines.append('voice.%d=%s' % (i, esc(voices[i] if i < len(voices) else '')))
        lines.append('profile.%d=%s' % (i, esc(profiles[i])))
    lines += ['comment.%d=%s' % (i, esc(s)) for i, s in enumerate(comments)]
    lines += ['first.%d=%d' % (i, v) for i, v in enumerate(first)]
    lines += ['count.%d=%d' % (i, v) for i, v in enumerate(count)]
    lines += ['pose.%d=%d' % (i, v) for i, v in enumerate(pose)]
    return lines


def write_texts(root, lang, lines):
    os.makedirs(os.path.join(root, 'CharRef'), exist_ok=True)
    mt = os.path.join(root, 'mod.txt')
    if not os.path.exists(mt):
        open(mt, 'w', encoding='utf-8').write('id=%s\nnombre=Character Reference BT3\nversion=1\netiquetas=menu\ndescripcion=Referencia de personajes de BT3 en el menu principal.\n' % os.path.basename(root))
    open(os.path.join(root, 'CharRef', lang + '.txt'), 'w', encoding='utf-8', newline='\n').write('\n'.join(lines) + '\n')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', required=True)
    ap.add_argument('--pak')
    ap.add_argument('--mod', default='char_reference')
    ap.add_argument('--lang', default='es', choices=['en', 'es'])
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    t = json.load(open(a.json, encoding='utf-8'))
    tb = t['tables']
    chars = sorted(t['characters'], key=lambda ch: ch['index'])
    lines = texts(a.lang, t['names'], t['forms'], [ch['voice_actor'] for ch in chars], [ch['profile'] for ch in chars], t['all_comments'],
                  tb['s25'], tb['s27'], tb['s26'])
    root = os.path.join(BASE, 'mods', a.mod)
    print('cr: %d nombres, %d perfiles, %d comentarios, idioma %s -> %s' % (len(t['names']), len(chars), len(t['all_comments']), a.lang, os.path.relpath(root, BASE)))
    if a.check:
        return
    write_texts(root, a.lang, lines)
    if a.pak:
        d = os.path.join(root, 'Packfile', 'Menus')
        os.makedirs(d, exist_ok=True)
        old = os.path.join(d, 'menu_chara_reference.pak')
        if os.path.exists(old):
            os.remove(old)
        folder = os.path.join(d, 'menu_chara_reference')
        n = open_pak(open(a.pak, 'rb').read(), folder, NAMES)
        tpl = op.leer_plantilla(os.path.join(BASE, 'tools', 'lists', 'paks', 'navbar.txt'))
        for nv in ('09_navbar.pak', '24_navbar_viewer.pak'):
            nav = os.path.join(folder, nv)
            if not os.path.isfile(nav):
                continue
            nd = open(nav, 'rb').read()
            os.remove(nav)
            nn = op.analizar(nd)['n']
            open_pak(nd, nav, [tpl[i + 1][1] if i + 1 in tpl else 'sin_nombre_%03d.bin' % (i + 1) for i in range(nn)])
            rhg = os.path.join(nav, '04_navbar.rhg')
            if os.path.isfile(rhg):
                op.abrir_tex(rhg, open(rhg, 'rb').read())
        print('cr: pak abierto en %s (%d entradas, barra de botones con texturas PNG)' % (os.path.relpath(folder, BASE), n))
    print('cr: mod listo')


if __name__ == '__main__':
    sys.exit(main())
