#!/usr/bin/env python3
# Everything unlocked from the start: the EBOOT list of locked characters is emptied and the menu counts of the overlays
# cover every entry (24_18, 32_20, 48_30, 56_38). Usage: patch_unlock.py [--undo]
import os, shutil, struct, sys

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'game')
EB = 'EBOOT.OLD'
LOCKED = [(0x08A7F710, bytes.fromhex('0b111516171e1f20212324' '2a2b2c2d2e2f35363738' '3f4243')), (0x08A7F729, b'\x45')]
OVL = {
    '24_18.elf': [(0x08A81B88, 0x3405000A, 0x34050024), (0x08A81BAC, 0x34040009, 0x34040023), (0x08A82A6C, 0x3408002F, 0x34080063),
                  (0x08A82AE0, 0x3415002F, 0x34150063), (0x08A831D8, 0x3409002F, 0x34090063)],
    '32_20.elf': [(0x08A8F394, 0x3414002F, 0x34140063), (0x08A91088, 0x3409002F, 0x34090063), (0x08AA45BC, 0x3409002F, 0x34090063),
                  (0x08AA48BC, 0x3414002F, 0x34140063)],
    '48_30.elf': [(0x08A81934, 0x3405000A, 0x34050024), (0x08A81954, 0x34040009, 0x34040023), (0x08A8DD68, 0x3409002F, 0x34090063),
                  (0x08A8E4D0, 0x3408002F, 0x34080063), (0x08A8E550, 0x3404002F, 0x34040063)],
    '56_38.elf': [(0x08AA930C, 0x3406002F, 0x34060063), (0x08AB77A4, 0x3407002F, 0x34070063), (0x08AB7824, 0x3407002F, 0x34070063),
                  (0x08AB9FA0, 0x3404000A, 0x34040024), (0x08AB9FA8, 0x3404000A, 0x34040024), (0x08AB9FB8, 0x34040009, 0x34040023)],
}


def ovl_off(d, va):
    phoff, phnum = struct.unpack_from('<I', d, 28)[0], struct.unpack_from('<H', d, 44)[0]
    for i in range(phnum):
        t, o, v, p, fsz = struct.unpack_from('<IIIII', d, phoff + i * 32)
        if t == 1 and v <= va < v + fsz:
            return va - v + o
    raise SystemExit('0x%08X fuera del overlay' % va)


def files():
    out = [(EB, None)] + [(EB + '.60', None)] * os.path.exists(os.path.join(D, EB + '.60'))
    for n in OVL:
        out.append(('overlays/' + n, n))
        if os.path.exists(os.path.join(D, 'overlays', n + '.60')):
            out.append(('overlays/' + n + '.60', n))
    return out


if '--undo' in sys.argv:
    for rel, _ in files():
        p = os.path.join(D, rel)
        if os.path.exists(p + '.desbloqueo'):
            shutil.copy2(p + '.desbloqueo', p)
            print('restored', p)
    raise SystemExit

for rel, ov in files():
    p = os.path.join(D, rel)
    if not os.path.exists(p + '.desbloqueo'):
        shutil.copy2(p, p + '.desbloqueo')
    d = bytearray(open(p + '.desbloqueo', 'rb').read())
    if ov is None:
        for va, old in LOCKED:
            o = va - 0x08804040 + 0x1018
            if d[o:o + len(old)] != old:
                raise SystemExit('%s: lista de bloqueados distinta en 0x%08X' % (rel, va))
            d[o:o + len(old)] = b'\xff' * len(old)
    else:
        for va, old, new in OVL[ov]:
            o = ovl_off(d, va)
            w = struct.unpack_from('<I', d, o)[0]
            if w != old:
                raise SystemExit('%s: en 0x%08X esperaba %08X y hay %08X' % (rel, va, old, w))
            struct.pack_into('<I', d, o, new)
    open(p, 'wb').write(d)
    print('%s: desbloqueado' % rel)
