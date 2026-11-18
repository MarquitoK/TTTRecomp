#!/usr/bin/env python3
# Sparking Zero battle HUD as a TTT mod: <mod>.mkpak in the root (mod.txt + SZHud/layout.txt + SZHud/common|es|en PNG)
#   --src <FModel export of SparkingZERO/Content/SS/HUD/MainGame (PNG + JSON)>  --l10n <FModel export of Content/L10N>
import argparse, glob, json, os, re, sys, zipfile

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
ROOT = 'WBP_MainGame_HUD'
SKIP = ('StyleAll', 'StylePanel', 'StyleSet', 'CommandGuide', 'PLMatch', 'TTR_HUD', 'SubTitle', 'Impact', 'IMP_', 'Brast', 'Crash',
        'Power', 'Speed', 'BBoost', 'DBEv', 'WM_', 'Emo', 'Boost', 'Button')
EXTRA = ('WBP_Rep_TimeCount', 'WBP_Rep_MainGauge_P1', 'WBP_Rep_MainGauge_P2', 'WBP_Rep_ComboSet_P1', 'WBP_Rep_ComboSet_P2', 'WBP_Rep_BBoost_1P', 'WBP_Rep_SkillGuideSet_P1', 'WBP_Rep_SkillGuideSet_P2')   # widgets SZ adds at runtime (the battle timer with its start effect)
TEX = ('T_UI_HUD_Debuff_01', 'T_UI_SkillGuide_Num_00', 'T_UI_NumMask', 'T_UI_SpIcon', 'T_UI_SkillGuide_Base_TypeA_01', 'T_UI_SkillGuide_Base_TypeB_01', 'T_UI_SkillGuide_Base_TypeC_01',
       'T_UI_SkillGuide_Base_TypeC_EF', 'T_UI_SkillGuide_Base_TypeC_Mask', 'T_UI_SkillGuide_Ef_00', 'T_UI_SkillGuide_Ef_TypeA_20', 'T_UI_SkillGuide_Ef_TypeA_20_Mask',
       'T_UI_SkillGuide_Ef_TypeA_24_Mask')   # textures the port sets at runtime (lowered stats), with their numbered family
TICK = 60000.0
CH = {'Translation': 'tr.x', 'Translation[1]': 'tr.y', 'Scale': 'sc.x', 'Scale[1]': 'sc.y', 'Shear': 'sh.x', 'Shear[1]': 'sh.y',
      'Rotation': 'rot', 'FloatCurve': 'f', 'RedCurve': 'r', 'GreenCurve': 'g', 'BlueCurve': 'b', 'AlphaCurve': 'a'}


def nm(ref):
    s = (ref or {}).get('ObjectName', '')
    s = s.split("'")[1] if "'" in s else s
    return s.split('.')[-1].split(':')[-1]


def g(v):
    return '%g' % round(float(v), 4)


class Src:
    def __init__(self, src):
        self.json = {os.path.basename(p)[:-5]: p for p in glob.glob(os.path.join(src, '**', '*.json'), recursive=True)}
        self.png = {os.path.basename(p)[:-4]: p for p in glob.glob(os.path.join(src, '**', '*.png'), recursive=True)}
        self.tex, self.mats, self.done, self.out = set(), {}, set(), []

    def ref(self, name):
        if not name:
            return ''
        if name in self.png:
            self.tex.add(name)
        elif name not in self.mats and name in self.json:
            self.mats[name] = None
            self.material(name)
        return name

    def material(self, name):
        for o in json.load(open(self.json[name], encoding='utf-8')):
            p = o.get('Properties', {})
            if o.get('Name') != name:
                continue
            t = ['M', name, 'parent=' + nm(p.get('Parent'))]
            for tp in p.get('TextureParameterValues', []):
                t.append('tex.%s=%s' % (tp['ParameterInfo']['Name'], self.ref(nm(tp.get('ParameterValue')))))
            for sp in p.get('ScalarParameterValues', []):
                t.append('s.%s=%s' % (sp['ParameterInfo']['Name'], g(sp['ParameterValue'])))
            for vp in p.get('VectorParameterValues', []):
                c = vp['ParameterValue']
                t.append('c.%s=%s,%s,%s,%s' % (vp['ParameterInfo']['Name'], g(c['R']), g(c['G']), g(c['B']), g(c['A'])))
            refs = [self.ref(nm(tv)) for tv in o.get('CachedExpressionData', {}).get('ReferencedTextures', []) or []]
            refs = [r for r in refs if r in self.png]
            if refs and not p.get('TextureParameterValues'):   # plain material: its first texture (the port adds it when it has no alpha)
                t.append('tex.Texture=' + refs[0])
                t.append('s.Plain=1')
            self.mats[name] = ' '.join(t)

    def widget(self, cls):
        if cls in self.done or cls not in self.json or (cls not in EXTRA and any(k in cls for k in SKIP)):
            return
        self.done.add(cls)
        d = json.load(open(self.json[cls], encoding='utf-8'))
        objs = {o['Name']: o for o in d if 'Name' in o}
        kids, slot = {}, {}
        for o in d:
            if o['Type'].endswith('Slot'):
                p = o.get('Properties', {})
                a, b = nm(p.get('Parent')), nm(p.get('Content'))
                if a and b:
                    kids.setdefault(a, []).append(b)
                    slot[b] = o
        con = {o['Name']: nm(o.get('Properties', {}).get('Content')) for o in d if o['Type'].endswith('Slot')}
        for o in d:
            sl = o.get('Properties', {}).get('Slots')
            if isinstance(sl, list) and o['Name'] in kids:
                order = [con.get(nm(x)) for x in sl]
                kids[o['Name']] = [k for k in order if k in kids[o['Name']]] + [k for k in kids[o['Name']] if k not in order]
        roots = [n for n in kids if n not in slot] or [o['Name'] for o in d if o['Type'] in ('CanvasPanel', 'Overlay') and o['Name'] not in slot][:1]
        self.out.append('C ' + cls)
        idx, subs = {}, []

        def rec(n, par):
            o = objs.get(n)
            if not o:
                return
            ty = o['Type']
            if ty.startswith('WBP_') and any(k in ty for k in SKIP):
                return
            i = len(idx)
            idx[n] = i
            p = o.get('Properties', {})
            t = ['W', str(i), str(par), ('U:' + ty[:-2]) if ty.endswith('_C') else ty, n]
            s = slot.get(n)
            if s:
                sp = s.get('Properties', {})
                ld = sp.get('LayoutData', {})
                off, an, al = ld.get('Offsets', {}), ld.get('Anchors', {}), ld.get('Alignment', {})
                if s['Type'] == 'CanvasPanelSlot':
                    t += ['s=C', 'l=' + g(off.get('Left', 0)), 't=' + g(off.get('Top', 0)), 'w=' + g(off.get('Right', 100)), 'h=' + g(off.get('Bottom', 30))]
                else:
                    t.append('s=' + s['Type'][0])
                if an:
                    mn, mx = an.get('Minimum', {}), an.get('Maximum', {})
                    t.append('anc=%s,%s,%s,%s' % (g(mn.get('X', 0)), g(mn.get('Y', 0)), g(mx.get('X', 0)), g(mx.get('Y', 0))))
                if al:
                    t.append('al=%s,%s' % (g(al.get('X', 0)), g(al.get('Y', 0))))
                if sp.get('bAutoSize'):
                    t.append('auto=1')
                if 'ZOrder' in sp:
                    t.append('z=%d' % sp['ZOrder'])
                pd = sp.get('Padding')
                if pd:
                    t.append('pad=%s,%s,%s,%s' % tuple(g(pd.get(k, 0)) for k in ('Left', 'Top', 'Right', 'Bottom')))
                for k, c in (('HorizontalAlignment', 'ha'), ('VerticalAlignment', 'va')):
                    if k in sp:
                        t.append('%s=%s' % (c, sp[k].split('_')[-1]))
            rt = p.get('RenderTransform', {})
            for k, c in (('Translation', 'tr'), ('Scale', 'sc'), ('Shear', 'sh')):
                if k in rt:
                    t.append('%s=%s,%s' % (c, g(rt[k].get('X', 0)), g(rt[k].get('Y', 0))))
            if 'Angle' in rt:
                t.append('rot=' + g(rt['Angle']))
            if 'RenderTransformPivot' in p:
                t.append('piv=%s,%s' % (g(p['RenderTransformPivot'].get('X', 0)), g(p['RenderTransformPivot'].get('Y', 0))))
            if p.get('bReverse'):
                t.append('rev=1')
            if 'RenderOpacity' in p:
                t.append('op=' + g(p['RenderOpacity']))
            v = str(p.get('Visibility', ''))
            if 'Collapsed' in v or 'Hidden' in v:
                t.append('hid=1')
            c = p.get('ColorAndOpacity')
            if isinstance(c, dict) and 'R' in c:
                t.append('col=%s,%s,%s,%s' % (g(c['R']), g(c['G']), g(c['B']), g(c['A'])))
            b = p.get('Brush')
            if b:
                r = b.get('ResourceObject')
                if r:
                    t.append('img=' + self.ref(nm(r)))
                sz = b.get('ImageSize')
                if sz:
                    t.append('sz=%s,%s' % (g(sz.get('X', 0)), g(sz.get('Y', 0))))
                tc = b.get('TintColor', {}).get('SpecifiedColor')
                if tc:
                    t.append('tint=%s,%s,%s,%s' % (g(tc['R']), g(tc['G']), g(tc['B']), g(tc['A'])))
                if b.get('DrawAs'):
                    t.append('draw=' + b['DrawAs'].split('::')[-1])
                mg = b.get('Margin')
                if mg and 'Box' in str(b.get('DrawAs', '')):
                    t.append('mg=%s,%s,%s,%s' % tuple(g(mg.get(k, 0)) for k in ('Left', 'Top', 'Right', 'Bottom')))
            em = p.get('EffectMaterial')
            if em:
                t.append('fx=' + self.ref(nm(em)))
            if 'Font' in p:
                t.append('font=%s' % p['Font'].get('Size', 0))
            self.out.append(' '.join(t))
            if ty.endswith('_C') and ty.startswith('WBP_'):
                subs.append(ty[:-2])
            for k in kids.get(n, []):
                rec(k, i)
        for r in roots:
            rec(r, -1)
        self.anims(d, objs)
        for s in subs:
            self.widget(s)

    def anims(self, d, objs):
        def at(ref):   # short names repeat in every animation: resolve by the export index of ObjectPath
            try:
                o = d[int((ref or {}).get('ObjectPath', '').rsplit('.', 1)[1])]
                if o.get('Name') == nm(ref):
                    return o
            except (ValueError, IndexError):
                pass
            return objs.get(nm(ref))
        for o in d:
            if o['Type'] != 'MovieScene':
                continue
            p = o.get('Properties', {})
            ub = p.get('PlaybackRange', {}).get('Value', {}).get('UpperBound', {}).get('Value', {}).get('Value', 0)
            self.out.append('A %s len=%s' % (o['Name'], g(ub / TICK)))
            names = {x.get('Guid'): x.get('Name') for x in p.get('Possessables', [])}
            for bnd in p.get('ObjectBindings', []):
                who = names.get(bnd.get('ObjectGuid')) or bnd.get('BindingName', '?')
                for tr in bnd.get('Tracks', []):
                    track = at(tr)
                    if not track or 'Sound' in track.get('Type', ''):
                        continue
                    tp = track.get('Properties', {})
                    prop = tp.get('PropertyBinding', {}).get('PropertyName') or tp.get('TrackName', '?')
                    for sec in tp.get('Sections', []):
                        so = at(sec)
                        if so:
                            self.section(who, prop, so.get('Properties', {}))

    def section(self, who, prop, sp):
        mask = (sp.get('TransformMask') or {}).get('Mask')
        bits = {'Translation': 1, 'Translation[1]': 2, 'Rotation': 4, 'Scale': 8, 'Scale[1]': 16, 'Shear': 32, 'Shear[1]': 64}
        def curve(ch, cv):
            ks = []
            for tm, v in zip(cv.get('Times', []), cv.get('Values', [])):
                if not isinstance(v, dict):
                    v = {'Value': v, 'InterpMode': 1}
                tg = v.get('Tangent', {})
                ks.append('%s:%s:%d:%s:%s' % (g(tm['Value'] / TICK), g(v.get('Value', 0)), v.get('InterpMode', 0), g(tg.get('ArriveTangent', 0) * TICK), g(tg.get('LeaveTangent', 0) * TICK)))
            if ks:
                self.out.append('K %s %s.%s %s' % (who, prop, ch, ','.join(ks)))
        for k, v in sp.items():
            if isinstance(v, dict) and 'Times' in v and (mask is None or k not in bits or mask & bits[k]):
                curve(CH.get(k, k), v)
        for sc in sp.get('ScalarParameterNamesAndCurves', []):
            curve('s.' + sc['ParameterName'], sc['ParameterCurve'])
        for cc in sp.get('ColorParameterNamesAndCurves', []):
            for k in ('RedCurve', 'GreenCurve', 'BlueCurve', 'AlphaCurve'):
                curve('c.%s.%s' % (cc['ParameterName'], CH[k]), cc[k])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', required=True)
    ap.add_argument('--l10n', required=True)
    ap.add_argument('--mod', default='sz_hud')
    a = ap.parse_args()
    s = Src(a.src)
    if ROOT not in s.json:
        sys.exit('hud sz: falta %s.json en %s' % (ROOT, a.src))
    s.widget(ROOT)
    for x in EXTRA:
        s.widget(x)
    s.tex.update(t for t in TEX if t in s.png)
    for t in list(s.tex):   # textures the game swaps at runtime (digits, bar colours, stocks): the whole numbered family
        m = re.match(r'(.*_)(\d+)$', t)
        if m:
            s.tex.update(n for n in s.png if n.startswith(m.group(1)) and n[len(m.group(1)):].isdigit())
    layout = '\n'.join(s.out + [m for m in s.mats.values() if m]) + '\n'
    out = os.path.join(BASE, a.mod + '.mkpak')
    langs = {}
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr(a.mod + '/mod.txt', 'id=%s\nnombre=SZ Life Bars\nversion=1\netiquetas=Textures\ndescripcion=Sparking Zero battle HUD: life bars, ki, energy, combo counter and technique list.\n' % a.mod)
        z.writestr(a.mod + '/SZHud/layout.txt', layout)
        for t in sorted(s.tex):
            rel = os.path.relpath(s.png[t], a.src).replace('\\', '/')
            for lg in ('es', 'en'):
                hit = []
                for d in (('es-419', 'es') if lg == 'es' else (lg,)):   # Latin American Spanish first
                    hit = glob.glob(os.path.join(a.l10n, d, '**', t + '.png'), recursive=True)
                    if hit:
                        break
                hit.sort(key=lambda f: not f.replace('\\', '/').endswith(rel))   # same folder as the original first
                if hit:
                    z.write(hit[0], '%s/SZHud/%s/%s.png' % (a.mod, lg, t))
                    langs[lg] = langs.get(lg, 0) + 1
            z.write(s.png[t], '%s/SZHud/common/%s.png' % (a.mod, t))
    print('hud sz: %d widgets, %d materiales, %d texturas (es %d, en %d), layout %d lineas' % (len(s.done), len(s.mats), len(s.tex), langs.get('es', 0), langs.get('en', 0), layout.count('\n')))
    print('hud sz: %s' % os.path.normpath(out))


if __name__ == '__main__':
    main()
