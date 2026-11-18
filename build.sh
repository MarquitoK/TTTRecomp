#!/bin/sh
# 1) mapa del programa  2) generacion del C++  3) compilacion
set -e
D=$(dirname "$0")
python3 "$D/recompiler/analyzer.py" "$D/game/EBOOT.OLD" --overlays "$D/game/overlays" -o "$D/generated/programa.json"
python3 "$D/recompiler/generator.py"
make -C "$D/generated/src" -j4
