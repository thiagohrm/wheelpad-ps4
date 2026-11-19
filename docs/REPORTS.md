# Report layout — Thrustmaster Red Legend Edition (FGT Rumble 3-in-1 board)

Two identities, one wheel:

- **PC switch position** (`0x044F:0xB655`): a 19-byte live HID input report,
  read over the interrupt endpoint (`0x81`). This is the mode the plugin
  uses. Captured in `docs/captures/`.
- **PS3 switch position** (`0x044F:0xB658`): a 27-byte reply to a vendor
  control read. The interrupt endpoint stalls there, and its pedal bytes
  never move, so it is the fallback only.

## PC layout, 19 bytes

| byte | field | notes |
|------|-------|-------|
| 0 | buttons 1-6 | bit0 = button 1 ... bit5 = button 6 |
| 1 | buttons 7-11 | bit0 = SE, bit1 = ST, bit4 = HOME. Bits 5-7 read `0xE0`, mask them out |
| 2 | hat switch | low nibble: 0=up, 2=right, 4=down, 6=left, `0x0F`=centred. High nibble drifts, mask it |
| 3 | X | steering, 8-bit. `0x00` full left, `0x80` centre, `0xFF` full right |
| 4 | Y | brake. `0xFF` released, `0x00` fully pressed |
| 5 | Rz | throttle. `0xFF` released, `0x00` fully pressed |
| 6 | constant | `0x80` |
| 7 | vendor | `0xFF` while D-pad right, else `0x00` |
| 8 | vendor | `0xFF` while D-pad left |
| 9 | vendor | `0xFF` while D-pad up |
| 10 | vendor | `0xFF` while D-pad down |
| 11 | vendor | `0xFF` while button 4 |
| 12 | vendor | `0xFF` while button 3 |
| 13 | vendor | `0xFF` while button 2 |
| 14 | vendor | `0xFF` while button 1 |
| 15 | vendor | `0xFF` while paddle DOWN |
| 16 | vendor | `0xFF` while paddle UP |
| 17-18 | unused | `0x00` |

Bytes 7-16 duplicate the buttons with inverted polarity. Use bytes 0-2 as the
source of truth, keep 7-16 as a cross-check.

## PS4 layout, 27 bytes

The console reply has the **same field order as the PC**. The only difference
is the pedal scale: a pedal rests at `0x80` on the console and at `0xFF` on
the PC, so the travel is `0x80` wide instead of `0xFF` wide.

| byte | field | notes |
|------|-------|-------|
| 0 | buttons 1-6 | bit0 = button 1 ... bit5 = button 6 |
| 1 | buttons 7-11 | bit0 = SE, bit1 = ST, bit4 = HOME. Bits 5-7 read `0xE0`, mask them out |
| 2 | hat switch | `0xFF` centred, `0x06` left. The PC uses `0x0F` for centred |
| **3** | **steering** | `0x80` centre, same as the PC |
| **4** | **brake** | `0x80` released, `0x00` fully pressed |
| **5** | **throttle** | `0x80` released, `0x00` fully pressed |
| 6 | extra axis | `0x80` at rest |
| 7-15 | vendor | button mirrors, same roles as PC bytes 7-14 |
| 16 | vendor | `0xFF` while paddle UP, same as PC byte 15 |
| 17-18 | vendor | `0xFF` while paddle DOWN, then unused, same as PC byte 16 |
| 19-26 | status | `0x00 02 00 02 00 02 00` at rest |

Evidence from a capture with one action at a time:

```
byte 1: 00 -> e0     buttons 7-11 mask appears
byte 2: 00 -> ff     hat appears
byte 3: 80 -> 81 -> 84 -> 86 -> 87 -> 84 -> 82 -> 81 -> 80   steering
byte 4: 80 -> 00     brake pressed
byte 5: 80 -> 00     throttle pressed
```

The pedal scale is `0x80` wide, so the shared representation (0xFF released,
0x00 pressed) needs a rescale, not an inversion.

## How to read it on the PS4

The wheel speaks a Thrustmaster vendor protocol, not standard HID.

- `libSceUsbd` must be loaded first, otherwise the call faults with
  `PRX_NOT_RESOLVED_FUNCTION` (`0xA0020101`).
- Find the device with `sceUsbdGetDeviceList` and match
  `0x044F:0xB658`. The device address changes on every plug, so it must be
  matched by id.
- Claim the interface with `sceUsbdClaimInterface` after
  `sceUsbdDetachKernelDriver`.
- The interrupt endpoint (`0x81`, interrupt, 19-byte packets) **works in PC
  mode** (`0x044F:0xB655`) and returns the same data as the vendor read.
  In PS3 mode (`0x044F:0xB658`) it stalls with `0x809C000F`, so the
  vendor control read is used there.
- What works is a vendor control IN request:
  `bmRequestType = 0xA1`, `bRequest = 0x01`, `wValue = 0x0000`,
  `wIndex = interface`. It returns 27 bytes of live state in PS3 mode.
  In PC mode the interrupt endpoint is preferred and this read is the
  fallback.
- A vendor OUT request `bmRequestType = 0x21`, `bRequest = 0x09`,
  `wValue = 0x0250` with the payload `42 01 00 ...` is accepted, but it is
  **not sent in PC mode**: a real PC host never sends it, and the wheel
  behaves correctly without it there.
- `wValue = 0x0300` returns a different 8-byte reply
  (`21 26 01 03 00 00 00 00`).

## Mode differences

Pedal reporting depends on the pedal-mode toggle (next section), not on
the switch position. The switch position only selects the USB identity and
report size:

| switch | PID | report | brake / throttle bytes |
|--------|-----|--------|------------------------|
| PS3 | `0xB658` | 27 bytes, vendor control read | byte 4 / byte 5 (dead on the console in every capture) |
| PC | `0xB655` | 19 bytes, interrupt endpoint | byte 4 / byte 5, live after the toggle |

Console use targets the PC position: it is the only one where the interrupt
endpoint answers and the pedals report.

## Analog pedal mode (pedal-mode toggle)

The wheel has two pedal modes, toggled by a physical combo documented in the
official manual:

> Simultaneously press and release **both pedals + the HOME button**.

The manual calls red "separate" and green "combined". What matters is the
observable state in game, not the LED:

- **Combined axis**: idle shows brake and throttle at 50% (the shared axis
  rests at `0x80`, so the HUD reads 127/127). Brake and throttle are not
  independent.
- **Separate axes (the working state)**: idle shows 0%/0%, brake byte 4
  rests at `0xFF` and ramps to `0x00` fully pressed, throttle byte 5
  independent. Verified on track: both pedals analog and independent.

On the console use **PC switch position** so the interrupt endpoint answers
(`044F:0xB655`). If idle shows 50/50, or the brake byte never moves, press
**both pedals + HOME** and repeat until idle is 0/0 and the pedals are
independent — one cycle was needed on this wheel before the brake reported
at all.

## Caveats

- Pedals are inverted on the wire: `0xFF` means released. The PS3-mode
  vendor reply instead rests at `0x80` and needs a rescale.
- Steering centre is `0x80` but real hardware centre may sit a few counts off.
  Deadzone and calibration belong in the mapping layer, not the parser.
- Hat values 1, 3, 5, 7 (diagonals) were not captured. Assume standard
  clockwise encoding until proven otherwise.
- Buttons 9 and 10 (byte 1 bits 2 and 3) never changed. Unknown, probably absent
  on this wheel.
- The console captures now separate the two pedals: in PC mode with the
  wheel in separate pedal mode, byte 4 is brake and byte 5 is throttle,
  both verified on track with independent idle 0/0.
