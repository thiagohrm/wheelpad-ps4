# volante

Make an old Thrustmaster wheel work on an exploited PS4 (then PS5) by presenting
it to games as a Logitech G29, in software only. No soldering, no USB adapter,
no donor controller.

Target wheel: Thrustmaster Ferrari Racing Wheel Red Legend Edition (PS3/PC, no
force feedback).

## Status

M0 — capture. Nothing runs on the console yet.

## Install

```
python -m pip install --user hidapi
```

Do not install the `hid` package. It is ctypes bindings that need a `hidapi.dll`
they do not ship. The `hidapi` package installs the import module named `hid`
and bundles the DLL.

## Guided capture

Run the wizard, pick the wheel configuration, and follow the prompts.

```
python tools/hid_capture.py
```

Four runs, one per configuration:

1. PC position, pedals separate (red LED)
2. PC position, pedals combined (green LED)
3. PS3 position, pedals separate (red LED)
4. PS3 position, pedals combined (green LED)

Each run writes `docs/captures/<timestamp>_<mode>.json` and a readable `.md`
with the offsets each input touched.

## Raw dump

For ad-hoc inspection without prompts:

```
python tools/hid_dump.py --list
python tools/hid_dump.py --dump
```

## Layout

```
docs/PLAN.md          design, milestones, open questions
docs/REPORTS.md       wheel input report layout, verified
docs/MAPPING.md       wheel input to pad output mapping
docs/captures/        capture output, written by the wizard
src/report.h          parser interface, shared by plugin and host tests
src/report.c          parser implementation
tools/hid_capture.py  guided capture wizard
tools/hid_dump.py     raw report dump
tools/hid_probe.py    device and descriptor diagnostic
tools/test_layout.py  validates the layout against captures
tools/test_mapping.py validates the pad mapping against captures
```

## Tests

```
python tools/test_layout.py
python tools/test_mapping.py
powershell -ExecutionPolicy Bypass -File tests/run.ps1
```

The last one compiles `src/report.c` with `zig cc` and replays the real captured
frames through it. `tests/run.ps1` finds zig on PATH or in the WinGet packages
directory.


