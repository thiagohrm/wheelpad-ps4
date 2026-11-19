# Volante — Thrustmaster Red Legend on PS4

Goal: use a Thrustmaster Ferrari Racing Wheel Red Legend Edition (PS3/PC, no
force feedback) with racing games on an exploited PS4, appearing as a
normal pad.

## Key decision

Do not emulate USB. Emulate the pad.

A USB host cannot be made to look like a device from software, and licensed
peripherals carry a Sony authentication chip. Both problems disappear if the
plugin synthesizes pad data after authentication instead of faking a device.

```
Thrustmaster HID reports  ->  plugin (game process)  ->  scePadReadState hook
                                                          |
                                                          v
                                                       game sees a pad
```

## Constraints

- No force feedback: no output path needed. Input only.
- Wheel has a PS3/PC selector switch; report layout differs per mode.
- Pedals have combined (green LED) and separate (red LED) axes modes.
- PS4 firmware: 11.02 with GoldHEN 2.4b18.9.
- Nothing persists across reboot.
- **The wheel is plugged into the console**, not the laptop. It cannot be
  moved, so all USB access must happen on the PS4.

## Architecture

One component: `wheelpad.prx`, a GoldHEN plugin loaded per title.

1. The plugin opens the wheel directly on the PS4 with `libSceUsbd`
   (`sceUsbdGetDeviceList`, `sceUsbdOpen`, `sceUsbdClaimInterface`,
   `sceUsbdInterruptTransfer`). The PS4 exposes a libusb-compatible wrapper,
   so the raw 19-byte HID report is read the same way the PC reads it.
2. A poll thread reads reports at ~125 Hz and keeps the newest one.
3. The plugin hooks `scePadReadState` and merges the wheel into the pad data.
4. Wheel data is dropped after 300 ms without a report, so a disconnected
   wheel cannot leave the car steering.

`src/report.c` and `src/mapping.c` hold the report parsing and the
wheel-to-pad mapping. They are plain C with no platform headers, so the host
tests cover the same code the plugin runs.

### Pad presentation (as shipped)

The plugin does **not** present a virtual pad. It hooks only
`scePadReadState` and merges wheel state into the pad data the game already
reads, leaving every controller button working. An earlier idea hooked
`scePadOpen`, `scePadClose` and `scePadGetControllerInformation` to present
the wheel as an extra pad, following the `xbox_controller_plugin` and
`ProCon` projects; it was dropped as unnecessary when the merge proved
enough for the target game.

## Findings

- Wheel is `vid=0x044F pid=0xB655` on the PC, `0xB658` on the PS4, product
  `FGT Rumble 3-in-1`. PC mode gives a live 19-byte interrupt report with
  brake on byte 4 and throttle on byte 5 after the pedal-mode toggle; PS3
  mode gives a 27-byte vendor control reply whose pedal bytes stay dead.
  Layout in `docs/REPORTS.md`.
- The PS4 enumerates the wheel as `usb_hid1` but Sony's device manager
  rejects it: `detachDevice:L1145 unknown device detach. (11 044f b658 00 0b)`.
  So no game sees it natively. Confirmed the problem is real.
- The console exposes `libSceUsbd`, a libusb wrapper. This is the path that
  works from a plugin on the console, and it is what the working USB
  controller plugins use.
- **AC polls the pad through `scePadReadState`.** A discovery build that
  hooked all eight candidate entry points at once showed that only
  `scePadReadState` is called. `scePadRead`, `scePadReadExt`,
  `scePadReadStateExt`, `scePadReadHistory`, `scePadReadForTracker`,
  `scePadShareOutputData` and `scePadGetDataInternal` are never called.
- AC loads a `tm_ffb_module_init`, so the game has its own force-feedback
  layer and may look for a wheel device class.
- `OrbisPadInformation.deviceClass` documents only PAD, GUITAR, DRUMS, but the
  GoldHEN plugin pad header also defines
  `SCE_PAD_DEVICE_CLASS_STEERING_WHEEL = 6`.
- The PRX entry point must be `_init` at offset 0x20, right after the
  `/libexec/ld-elf.so.1` header that `link.x` emits. If the linker orders
  another function first, the loader jumps into unrelated code and the
  plugin never initialises. `ps4/build.ps1` asserts this.

## Milestones

### M0 — capture (PC) — DONE
Layout confirmed and covered by tests: `tools/test_layout.py`,
`tools/test_mapping.py`.

### M1 — read the wheel on the console — DONE
`sceUsbd` opens the wheel and polls it. PC mode reads the interrupt
endpoint, PS3 mode the vendor control reply. klog showed steering, pedals
and buttons changing.

### M2 — pad injection — DONE
`scePadReadState` hooked, wheel merged into the pad stream. Verified in
game: steering, buttons, paddles, analog throttle and brake.

### M3 — wheel identity — NOT NEEDED
The game treats the injected data as a pad and plays fine, so presenting a
steering wheel device class was never required. Revisit only if a game
refuses pad input for wheel-specific features.

## Toolchain

- OpenOrbis PS4 Toolchain: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
- GoldHEN Plugin SDK: https://github.com/GoldHEN/GoldHEN_Plugins_SDK
- Plugin examples: https://github.com/GoldHEN/GoldHEN_Plugins_Repository
- USB controller reference: https://github.com/crucifix86/xbox_controller_plugin

## Risks

- The kernel HID driver may already hold the wheel's interface, so
  `sceUsbdClaimInterface` could fail. `sceUsbdDetachKernelDriver` is the
  escape hatch.
- Wheel-mode gating is undocumented; may need per-game handling.
- No PSN, no persistence, exploit rerun every boot.

## Console facts

- klog on port 3232, FTP on 2121, BinLoader on 9090.
- Assetto Corsa is `CUSA01830`.
- The console's FTP server ignores an upload that would overwrite an
  existing file. Delete first, then upload, then verify.
- The FTP server decodes a PRX container into a plain ELF on read, so a
  downloaded PRX is larger than the uploaded file. Compare content, not size.
- Game sandbox blocks writes to `/data` and `/user/data` from inside a game
  process. Use klog for plugin logging.
