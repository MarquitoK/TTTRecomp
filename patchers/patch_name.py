#!/usr/bin/env python3
# Acorta el flujo de creacion de partida en DBZ Tenkaichi Tag Team (overlays/13_D.elf).
#   --destino N   pantalla tras aceptar el nombre: 3 flujo original, 5 pregunta de guardado, 6 "checking", 7 guardado directo
#   --sin-aviso   arranca en la pantalla del teclado, sin crear la caja "Enter Player Name..."
#   --sin-espera  quita el minimo de 51 cuadros de la caja "checking" (solo con destino 5 o 6)
#   --deshacer    restaura el overlay original
import struct, sys, os, shutil

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'game', 'overlays')
OV = os.path.join(D, '13_D.elf')
VADDR, FOFF = 145233056, 84

def off(va): return va - VADDR + FOFF

if '--deshacer' in sys.argv:
    if os.path.exists(OV + '.nombre'):
        shutil.copy2(OV + '.nombre', OV)
        print('restaurado', OV)
    raise SystemExit

destino = 7
if '--destino' in sys.argv: destino = int(sys.argv[sys.argv.index('--destino') + 1])
if destino < 0 or destino > 13: raise SystemExit('destino fuera de rango')

if not os.path.exists(OV + '.nombre'): shutil.copy2(OV, OV + '.nombre')
orig = bytearray(open(OV + '.nombre', 'rb').read())
d = bytearray(orig)

def parche(va, esperado, nueva, nombre):
    o = off(va)
    w = struct.unpack_from('<I', orig, o)[0]
    if w != esperado: raise SystemExit('en 0x%08X esperaba %08X y hay %08X' % (va, esperado, w))
    struct.pack_into('<I', d, o, nueva)
    print('0x%08X  %08X -> %08X  %s' % (va, w, nueva, nombre))

# la pantalla 3 es la que copia el nombre a los datos de la partida: se conserva sin su caja Si/No
if destino != 3:
    parche(0x08A8A474, 0x34110000, 0x10000023, 'pantalla 3 sin caja Si/No')
    parche(0x08A8A478, 0x0E26FF7D, 0x00000000, 'pantalla 3 sin reservar la caja')
    parche(0x08A8A530, 0x10800023, 0x1000000A, 'pantalla 3 confirma sola y copia el nombre')
    parche(0x08A8A5AC, 0x3405000A, 0x34050000 | destino, 'destino tras el nombre = %d' % destino)
if '--sin-aviso' in sys.argv:
    parche(0x08A89D7C, 0x8E240004, 0x8E240008, 'pantalla inicial = 2 (teclado)')
if '--sin-espera' in sys.argv:
    parche(0x08A8C0B0, 0x28840033, 0x28840000, 'sin espera minima en checking')

open(OV, 'wb').write(d)
print('overlay del nombre parcheado')
