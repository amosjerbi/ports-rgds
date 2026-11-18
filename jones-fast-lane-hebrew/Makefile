# Builds the Hebrew-top-screen ROMs with the port in ../jones-fast-lane:
#   make      -> ../jones-fast-lane/build/jones-he.nds
#   make cd   -> ../jones-fast-lane/build/jones-cdrom-he.nds
#   make font -> screens/font.png (preview of the pixel font)
# GAMEDIR / CDDIR are passed through (see ../jones-fast-lane/BUILDING.md).
PORT := ../jones-fast-lane
.PHONY: all cd font
all:
	$(MAKE) -C $(PORT) he HEDIR=$(CURDIR)
cd:
	$(MAKE) -C $(PORT) cd-he HEDIR=$(CURDIR)
font:
	python3 mkhebrew.py hebrew.tsv /dev/null screens/font.png
