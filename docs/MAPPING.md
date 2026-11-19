# Mapping — wheel inputs to pad data

Two layers:

1. `docs/REPORTS.md` — how the wheel encodes its inputs. Done, tested.
2. This file — how those inputs become the pad data a game reads.

Constants verified against `OpenOrbis-PS4-Toolchain/include/orbis/_types/pad.h`.

## Target structure

`OrbisPadData` fields we fill:

```c
uint32_t buttons;        /* ORBIS_PAD_BUTTON_* bits */
stick    leftStick;      /* uint8_t x, y */
stick    rightStick;     /* left at centre */
analog   analogButtons;  /* uint8_t l2, r2 */
uint8_t  connected;      /* set to 1 */
```

Relevant button bits:

| bit | value | name |
|-----|-------|------|
| 0x0002 | L3 |
| 0x0004 | R3 |
| 0x0008 | OPTIONS |
| 0x0010 | UP |
| 0x0020 | RIGHT |
| 0x0040 | DOWN |
| 0x0080 | LEFT |
| 0x0100 | L2 |
| 0x0200 | R2 |
| 0x0400 | L1 |
| 0x0800 | R1 |
| 0x1000 | TRIANGLE |
| 0x2000 | CIRCLE |
| 0x4000 | CROSS |
| 0x8000 | SQUARE |
| 0x100000 | TOUCH_PAD |

## Default mapping

| wheel input | pad target | notes |
|-------------|-----------|-------|
| steering (byte 3) | `leftStick.x` | 0-255, `0x80` centre |
| brake (byte 4) | `analogButtons.l2` + `L2` bit | inverted, see formula |
| throttle (byte 5) | `analogButtons.r2` + `R2` bit | inverted, see formula |
| paddle UP | `R1` | shift up |
| paddle DOWN | `L1` | shift down |
| button 1 | `SQUARE` | follows the wheel's printed label |
| button 2 | `CROSS` | |
| button 3 | `CIRCLE` | |
| button 4 | `TRIANGLE` | |
| SE | `OPTIONS` | start |
| ST | `TOUCH_PAD` | spare button, no dedicated share bit in this header |
| HOME | `L3` | PS button is not exposed to games, so reuse it |
| D-pad | `UP` / `RIGHT` / `DOWN` / `LEFT` | direct |

Unused: `L3`/`R3` conflict solved above, `rightStick` stays at `0x80, 0x80`.

## Formulas

```
pad.l2        = 255 - wheel.brake        /* wheel: 255 = released */
pad.r2        = 255 - wheel.throttle

if (pad.l2 >= TRIGGER_THRESHOLD) buttons |= L2
if (pad.r2 >= TRIGGER_THRESHOLD) buttons |= R2     /* TRIGGER_THRESHOLD = 32 */

pad.leftStick.x = wheel.steering        /* after centre calibration */
pad.leftStick.y = 128
```

## Calibration

The parser reports raw values. A calibration layer sits between the parser and
the pad writer:

- `steering_center` — measured at rest, nominally `0x80`. Subtract the offset,
  clamp, then rescale so the full mechanical range reaches 0 and 255.
- `steering_deadzone` — optional, default 0. Wheels do not need one.
- `steering_invert` — for games that expect the opposite direction.
- `pedal_min` / `pedal_max` — measured released and fully pressed. Rescale to
  0-255 so partial pedal travel is usable.

Store as constants first, expose as a config file only if real hardware needs it.
Do not build a config system before the first working run.

## Ordering

`steering`, `brake` and `throttle` update every frame. Buttons are edge-free:
write the whole state each frame, do not accumulate.

## Open question for M3 — resolved

`OrbisPadInformation.deviceClass` only defines `PAD`, `GUITAR`, `DRUMS`. No
wheel class exists in the public header. It turned out not to matter: the
game accepts the injected wheel state as pad input, so no wheel device
class was ever presented. The fallbacks below stay here for any future
game that refuses pad input:

1. Map pedals to `leftStick.y` as well, so pad-style games still drive.
2. Hook `scePadGetControllerInformation` to adjust what the game reads.
