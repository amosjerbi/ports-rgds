#!/bin/sh
# Rebuild the CGB-colour ROM + IPS from YOUR OWN copy of the original ROM.
#   usage: ./make.sh "Donkey Kong (Japan, USA) (En) (SGB Enhanced).gb"      (needs python3 + PyBoy: pip install pyboy)
set -e
cd "$(dirname "$0")"
ROM="${1:?usage: ./make.sh <original Donkey Kong ROM>}"
python3 -I tools/sgbdump.py "$ROM" out/sgb >/dev/null      # SGB palette/attribute data exactly as the game sends it (stays local, never commit out/)
python3 -I tools/dkbuild.py "$ROM" out/dk-color.gb out/dk-color.ips
