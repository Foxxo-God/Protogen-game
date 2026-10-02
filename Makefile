.PHONY: all clean

all: build/protogen-frontier.gba

build/protogen-frontier.gba: src/game.c build_rom.py
	python3 build_rom.py

clean:
	rm -rf build