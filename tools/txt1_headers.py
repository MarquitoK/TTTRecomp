#!/usr/bin/env python3
# Read only: what the 0x48-byte txt1 headers (_txt1.bin) contain, to stop storing them. Writes txt1_report.txt.
import collections, os, struct, sys

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
assets = sys.argv[1] if len(sys.argv) > 1 else os.path.join(BASE, 'assets')
out = os.path.join(BASE, 'txt1_report.txt')

rows = []
for top in ('Packfile', 'Shared'):
    for dp, dn, fn in os.walk(os.path.join(assets, top)):
        for n in fn:
            if n.lower().startswith('_txt1') and n.lower().endswith('.bin'):
                p = os.path.join(dp, n)
                d = open(p, 'rb').read()[:0x48]
                rel = os.path.relpath(p, assets).replace('\\', '/')
                texts = 0
                t = os.path.join(dp, 'texto.txt')
                if os.path.isfile(t):
                    texts = open(t, encoding='utf-8', errors='replace').read().count('\n[') + 1
                rows.append((rel, d, texts))

templates = collections.defaultdict(list)
bases = []
for rel, d, texts in rows:
    if len(d) < 0x48:
        templates['short'].append(rel)
        continue
    base, last = struct.unpack_from('<HH', d, 2)
    n = last - base + 1
    sym = []
    for pos in range(0, 0x48, 2):
        v = struct.unpack_from('<H', d, pos)[0]
        sym.append('B' if pos > 2 and v == base else 'L' if v == last and pos >= 4 else 'N' if v == n and pos > 4 else '%04X' % v)
    sym[1], sym[2] = 'B', 'L'
    templates[' '.join(sym)].append(rel)
    bases.append((base, n, texts, rel))

with open(out, 'w', encoding='utf-8', newline='\n') as o:
    o.write('# %d headers, %d distinct templates (B base id, L last id, N count)\n' % (len(rows), len(templates)))
    for k, v in sorted(templates.items(), key=lambda x: -len(x[1])):
        o.write('T %d\t%s\t%s\n' % (len(v), k, ' | '.join(v[:4])))
    o.write('# base\tcount\ttexts in texto.txt\tpath\n')
    for b, n, t, r in sorted(bases, key=lambda x: x[3]):
        o.write('%d\t%d\t%d\t%s\n' % (b, n, t, r))
print('%d headers, %d templates; report in %s' % (len(rows), len(templates), out))
