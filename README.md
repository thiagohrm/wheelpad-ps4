# wheelpad

Drive Assetto Corsa on an exploited PS4 with a Thrustmaster Ferrari Racing
Wheel Red Legend Edition — in software only. The plugin reads the wheel
directly over USB and injects it into the normal pad stream, so the game
needs no patch and no adapter, soldering, or donor controller.

Tested on PS4 11.02 with GoldHEN 2.4b18.9, Assetto Corsa `CUSA01830` and
`CUSA00001`.

## What works

| input | in game |
|-------|---------|
| steering | left stick X, full analog |
| throttle | R2, full analog |
| brake | L2, full analog |
| D-pad | D-pad |
| buttons 1-4 | square, cross, circle, triangle |
| paddles | R1 / L1 (shift up / down) |
| SE / ST / HOME | options / touch pad / L3 |

Controller buttons and the vertical stick keep working, so menus stay
usable with the pad in hand. If the wheel stops sending reports for 300 ms
the merge stops, so a dead wheel cannot leave the car steering on its own.

## Install

1. Upload `wheelpad.prx` to `/data/GoldHEN/plugins/` on the console (FTP
   works; delete the old file first, the PS4 FTP server ignores
   overwrites).
2. Register it in `/data/GoldHEN/settings/plugins.ini`:

   ```ini
   ; Assetto Corsa
   [CUSA01830]
   /data/GoldHEN/plugins/wheelpad.prx

   ; Assetto Corsa Competizione
   [CUSA17266]
   /data/GoldHEN/plugins/wheelpad.prx

   ; Gran Turismo 7 (untested)
   [CUSA24767]
   /data/GoldHEN/plugins/wheelpad.prx
   [CUSA24769]
   /data/GoldHEN/plugins/wheelpad.prx
   ```

   `tools/ps4_ini.py` edits the file over FTP and keeps a backup:
   `python tools/ps4_ini.py add CUSA17266 /data/GoldHEN/plugins/wheelpad.prx`.

3. Reboot the console, then launch the game.

## Usage

Set the wheel's mode switch to **PC** before plugging it in. That makes the
wheel enumerate as `044F:0B655`, the identity the plugin prefers, and the
one where the live interrupt reports (pedals included) are reachable.

One caveat inherited from the wheel itself: it has two pedal modes. If
throttle and brake sit at 50% at idle, or the brake never registers, press
**both pedals + the HOME button** at the same time and release. Repeat
until idle reads 0%/0%. The mode is stable for the rest of the session.

## Build

```
powershell -ExecutionPolicy Bypass -File ps4\build.ps1
```

The build needs `zig cc` (0.17.x) on PATH, or in the WinGet packages
directory — `build.ps1` finds it in both places. Output:
`build/ps4/wheelpad.prx`.

## Tests

```
powershell -ExecutionPolicy Bypass -File tests\run.ps1
```

Parses and maps replayed real frames from `docs/captures/`: report
fixtures, mapping fixtures, console fixtures. The parser and mapping live
in `src/`, shared between the plugin and the host tests, so there is a
single implementation of the wheel layout.

## How it works

- `libSceUsbd.sprx` is loaded explicitly, then the wheel is found by
  vendor id, the kernel HID driver is detached, and the interface claimed.
- In PC mode the plugin polls the interrupt IN endpoint (`0x81`) for live
  19-byte reports. In PS3 mode that endpoint stalls, so it falls back to
  the wheel's vendor control read (`0xA1 0x01`), which returns a 27-byte
  report. Phantom and echo replies are filtered by signature.
- `scePadReadState` from `libScePad.sprx` is hooked (x64 detour behind the
  exported thunk) and wheel state is merged into what the game reads.

## Layout

```
ps4/          GoldHEN plugin: USB reader, pad hook, build script
src/          shared parser and mapping, also used by host tests
tests/        host test suites
tools/        capture, klog, upload, and analysis helpers
docs/         report layout, mapping, plan, captures
```

## License

Code and documentation by Thiago Mattos, licensed **CC BY-NC-SA 4.0** — see
`LICENSE`. In short: use it, modify it, share it; credit the author; no
commercial use; share your adaptations under the same license. Components
compiled in from the GoldHEN Plugin SDK remain under their own MIT license
(see `THIRD-PARTY-NOTICES.md`).

## Credits

- [GoldHEN](https://github.com/GoldHEN) and the
  [GoldHEN Plugin SDK](https://github.com/GoldHEN/GoldHEN_Plugins_SDK)
  (MIT) — the detour engine and plugin glue compiled into the PRX, and the
  loader itself.
- [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)
  — Orbis headers and link stubs.
- [OSM-Made / PS4-Notify](https://github.com/OSM-Made/PS4-Notify) — the
  notification recipe used for the load banner.
- [crucifix86 / xbox_controller_plugin](https://github.com/crucifix86/xbox_controller_plugin)
  — working reference for `sceUsbd` + pad injection.

Full license texts: `LICENSE` and `THIRD-PARTY-NOTICES.md`.

## Docs

- `docs/REPORTS.md` — byte maps for both report flavours, verified on
  hardware
- `docs/MAPPING.md` — wheel input to pad output
- `docs/PLAN.md` — design history and milestones
