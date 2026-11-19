# Release notes

## v3.0.1 — load notification

The plugin now shows the system banner on load, the same style GoldHEN
itself uses: **"wheelpad v3.0.1 loaded"**. The text is derived from the
version constant, so the two cannot drift apart.

Artifact: `build/release/wheelpad-v3.0.1.zip` containing `wheelpad.prx`,
md5 `d04b89b654244c9304f1ed02adf62ca4`.

## v3.0.0 — analog pedals, verified on track

First fully working release. Everything the wheel has is delivered to the
game through the normal pad stream:

- steering, throttle and brake, all full analog
- D-pad, face buttons (square/cross/circle/triangle), paddles as L1/R1
- SE/ST/HOME as options/touch pad/L3
- controller buttons keep working; 300 ms staleness drop keeps a dead wheel
  from steering the car

Technical changes since the first console build:

- Live reports are read from the interrupt IN endpoint (`0x81`) in PC mode;
  the vendor control read remains as the PS3-mode fallback.
- 19-byte PC-mode report parsed: brake byte 4, throttle byte 5, `0xFF`
  released, `0x00` fully pressed.
- The vendor init OUT request is skipped in PC mode, matching a real PC
  host.
- Pedal-mode caveat documented: if throttle and brake idle at 50%, press
  both pedals + HOME until idle reads 0%/0%.

Verified on PS4 11.02, GoldHEN 2.4b18.9, Assetto Corsa `CUSA01830`.

Artifact: `build/release/wheelpad-v3.0.0.zip` containing `wheelpad.prx`,
md5 `8460735b4845d96094ed6fe5ed6262ef`.
