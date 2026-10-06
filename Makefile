.PHONY: all clean

all: build/protogen-frontier-v2.gba

build/protogen-frontier-v2.gba: src/game.c build_rom.py
	python3 build_rom.py

clean:
	rm -rf build