# Neon Frontier: Protogen Defense

A tiny standalone Game Boy Advance homebrew shooter. The ROM is built locally from the included ARM7TDMI C source; it does not patch or include a commercial game ROM.

## Build

Requires Python 3, Clang with ARM target support, and Make:

```sh
make
```

The resulting ROM is `build/protogen-frontier.gba`. Open it in a Game Boy Advance emulator.

## Controls

- D-pad: move
- A or B: fire at the nearest hostile
- START: deploy or retry
- SELECT: pause and open the Protogen lab
- In the lab, LEFT/RIGHT change visor color, L/R change ear modules, and A cycles the visor expression. SELECT resumes a run.

Survive increasingly difficult enemy waves, watch your core integrity, and use the intermission to recover. Appearance changes apply immediately.
