<p align="center"><img src="https://raw.githubusercontent.com/alexbeavs-ps1-ports/psxrecomp-ports/main/docs/assets/alexbeav-ps1-recomps-banner.png" alt="Alexbeav's PS1 Recomps" width="100%"></p>

# Dino Crisis Recompiled — Mod Edition

> **This is a fork** of [alexbeavs-ps1-ports/dino-crisis-recomp](https://github.com/alexbeavs-ps1-ports/dino-crisis-recomp) (a PSXRecomp port of *Dino Crisis*, SLUS-00922). The goal of this fork is **not** to ship the port as-is, but to turn it into a **modding platform for Dino Crisis specifically**: analysis infrastructure, decompilation tooling, matched replacements of native game functions, and a declarative mod system on top of the recompiled runtime.

If you just want to play the vanilla recompiled port, use the upstream project instead.

## What this fork adds

| Phase | What | Where |
|---|---|---|
| 0 — Analysis infra | Ghidra headless + PyGhidra pipeline over the boot EXE, function/edge/ref databases, symbol sync into the build (`symbols.toml` → `psx_symbols.h`), ~2000 named functions | `analysis/`, `tools/`, `docs/ANALYSIS.md`, `docs/COMPILER.md` |
| 1 — Mapping | Gameplay architecture mapped: main loop (`GameMain`), cooperative task system, event-flag system, scene lifecycle; per-area function tables and leads for dino AI / danger meter | `docs/ANALYSIS.md`, `annotations/` |
| 2 — Matched decomp | EventFlag functions (`func_8004A87C`, `func_8004A7E8`, `func_8004A830`) replaced by hand-written C, proven bit-exact by a 30 000-case differential test | `src_game/`, `docs/MATCHED.md` |
| 3 — Mod system | Declarative runtime patching: manifest v5 packages with options, hex patches with expected-byte guards, and per-option ladders — patches apply at boot into guest RAM via `psx_write_byte` + dirty-RAM guard. Ships with the first mod: `dc.enhancement.fade-speed` (1×–32×, screen fade speed) | `mods/packages/` |

See [`STEPS.md`](STEPS.md) for the full plan and decision log.

## Target dump

This fork is retargeted to the **Dino Crisis (USA) v1.0 (Rev 0)** dump — accepted disc sizes/hashes live in [`game.toml`](game.toml) (`known_sizes` / `known_md5`). If you own the Rev 1 dump, extend those lists instead of editing the hashes.

BIOS: SCPH-5502/5552 Europe (524288 bytes, SHA-256 `1faaa18fa820a0225e488d9f086296b8e6c46df739666093987ff7d8fd352c09`).

## Building

Linux prerequisites: CMake, Ninja, Python 3, C/C++ compiler, `glslc` (Vulkan SDK headers for the launcher).

```bash
cmake -B build -DPSXRECOMP_ALLOW_NO_BIOS=ON
cmake --build build
```

Then launch the built executable, point the wizard at your CUE + BIOS, and run *Generate & rebuild*.

### Writing a mod

Mods are TOML manifests under `mods/packages/<package>/<version>/manifest.toml`. A package declares options and byte patches against the main EXE:

```toml
format_version = 5
game_id = "*"

[[feature]]
id = "my-feature"
default_enabled = false

[[option]]
feature = "my-feature"
id = "amount"
type = "int"
min = 1
max = 16
default = 1

[[patch]]
address = 0x8004714C          # guest PC (main_exe)
expected = "06004224"         # hex the runtime must find there (safety guard)
replace = "30004224"          # replacement (MIPS LE: immediate = first 2 bytes)
when_integer = { option = "amount", op = "eq", value = 2 }
```

Gotchas learned the hard way (all documented in `STEPS.md`): the patch key for the main EXE is `address`; `scan()` failures are silent, so a wrong `address`/`expected` just disables the patch; hex is byte-order-sensitive (little-endian immediates first); option values live in `[[feature]]` entries in `build/mods/state.toml`.

## Repository layout (fork additions)

- `STEPS.md` — master plan, phases, decision log
- `docs/` — `ANALYSIS.md` (architecture + tooling), `COMPILER.md` (compiler ID), `MATCHED.md` (matched decomp policy)
- `analysis/` — Ghidra scripts (import, decompile, symbol apply/fix), function/edge/ref JSON databases
- `annotations/` — Ghidra annotations export
- `tools/filter_generated.py` — build-time filtering of replaced functions + extraction of originals for the differential test
- `src_game/` — matched C replacements, replacement list, differential test harness
- `mods/packages/` — declarative mod packages
- `symbols.toml`, `seeds/` — curated symbols and Ghidra function seeds

## Credits and licenses

- Port and packaging: **Alexbeav** ([upstream repo](https://github.com/alexbeavs-ps1-ports/dino-crisis-recomp)) — all of the port itself remains his work; this fork's changes build on it.
- Framework: [PSXRecomp](https://github.com/mstan/psxrecomp). Launcher: [recomp-ui](https://github.com/mstan/recomp-ui). Their licenses and dependency notices remain in the corresponding submodule directories.
- *Dino Crisis* and its trademarks belong to Capcom. This repository contains **no game disc, no retail BIOS, and no generated retail game code**.
