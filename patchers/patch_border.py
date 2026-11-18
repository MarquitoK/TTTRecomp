#!/usr/bin/env python3
# Opacidad del contorno de cel-shading en DBZ Tenkaichi Tag Team.
# La funcion 0x0880A66C construye la paleta de 256 entradas que tine el pase de
# contorno. Entradas 1..255 en 5650: 0xC618 gris claro, 0x8410 gris medio.
# Cuanto mas oscuro el gris, mas marcado sale el trazo.
#   0x0880A7D4  ori a0,zero,0xC618   modo normal
#   0x0880A7DC  ori a0,zero,0x8410   modo alterno
# Uso: python3 parcheborde.py [--nivel N] [--deshacer]    N de 0 a 4, por defecto 2
import struct, sys, os, shutil

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'game')
EBOOT = os.path.join(D, 'EBOOT.OLD')

NIVELES = [
    (0xC618, 0x8410),   # 0: original
    (0xA514, 0x6B0D),
    (0x8410, 0x528A),   # 2: por defecto
    (0x630C, 0x39E7),
    (0x4208, 0x2104),   # 4: trazo muy marcado
]

def off(va): return va - 0x8804040 + 0x1018

nivel = 2
if '--nivel' in sys.argv: nivel = int(sys.argv[sys.argv.index('--nivel') + 1])
if nivel < 0 or nivel >= len(NIVELES): raise SystemExit('nivel fuera de rango')

if '--deshacer' in sys.argv:
    if os.path.exists(EBOOT + '.borde'): shutil.copy2(EBOOT + '.borde', EBOOT); print('restaurado', EBOOT)
    raise SystemExit

d = bytearray(open(EBOOT, 'rb').read())
if not os.path.exists(EBOOT + '.borde'): shutil.copy2(EBOOT, EBOOT + '.borde')
orig = bytearray(open(EBOOT + '.borde', 'rb').read())

for va, i in ((0x0880A7D4, 0), (0x0880A7DC, 1)):
    o = off(va)
    w = struct.unpack_from('<I', orig, o)[0]
    if (w >> 16) != 0x3404: raise SystemExit('en 0x%08X esperaba ori a0,zero y hay %08X' % (va, w))
    struct.pack_into('<I', d, o, (w & 0xFFFF0000) | NIVELES[nivel][i])
    print('0x%08X  0x%04X -> 0x%04X' % (va, w & 0xFFFF, NIVELES[nivel][i]))

open(EBOOT, 'wb').write(d)
print('nivel de contorno', nivel)
