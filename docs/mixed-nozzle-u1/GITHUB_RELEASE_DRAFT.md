# GitHub Release Draft: mixed-nozzle-u1

## Title

Snapmaker Orca 2.3.5 U1 Experimental Alpha

## Tag

`u1-experimental-2.3.5-alpha.1`

## Target Branch

`Min/2.3.5-beta-mixed-nozzle`

## Summary

Experimental Snapmaker U1 mixed-nozzle build for printing fine outer walls with
one nozzle and inner walls/infill with another nozzle. The first real-print
validated pairing is 0.2 mm outer walls plus 0.4 mm inner walls/infill.

This release requires both:

- the patched Snapmaker Orca build from this branch
- the matching SnapmakerU1-Extended-Firmware mixed-nozzle validation patch

## Highlights

- Independent per-head nozzle diameter editing for U1.
- Nozzle diameter shown in each nozzle tab.
- Mixed Nozzle workstation in the Prepare sidebar for quick plans, nozzle map,
  feature assignment, layer combining, and validation.
- `outer_wall_filament` setting for routing external perimeters to a different
  tool than inner walls.
- `mixed_nozzle_mode` selector:
  - Same layer, different line widths
  - Mixed layer, different line widths
- Mixed-layer sparse infill, inner wall, and internal solid infill combining
  are controlled separately.
- Automatic coarse layer height targeting uses half of the coarse nozzle
  diameter, limited by the configured coarse extruder max layer height.
- Legacy manual mixed-layer ratio override remains available as an advanced
  compatibility path.
- Machine filament sync from the connected U1.
- Correct physical-head to logical T-slot filament mapping through
  `extruder_map_table`.
- Experimental mixed-layer profile example:
  `0.10 Mixed Layer Outer Nozzle2 Inner Nozzle1 @Snapmaker U1`.
- Same-layer profile example:
  `0.12 Mixed Nozzle Outer T0 Inner T1 @Snapmaker U1`.

## Assets To Attach

- Windows Snapmaker Orca build artifact.
- U1 firmware `.bin` built from the firmware branch with the validation patch.
- Optional sample cube G-code generated from the 0.10 mixed-layer profile.
- Optional `scripts/check_mixed_nozzle_gcode.py` checker output for the sample
  cube.
- Real print validation photo:
  `docs/mixed-nozzle-u1/assets/real-print-cube.jpg`

Attach only a firmware binary built from the published matching source commit,
and include its SHA-256 checksum and GPL-3.0 source link.

## Validation Results

- Snapmaker Orca Release build passed.
- `libslic3r_tests.exe [MachineFilamentSync]` passed, 17 assertions.
- `libslic3r_tests.exe [MixedLayerHeight]` covers span planning, automatic
  coarse layer height, max layer height clamping, and legacy ratio fallback.
- Snapmaker profile validator passed for the Snapmaker profile bundle.
- 20 mm cube G-code inspection passed:
  - outer wall on T1 / 0.2 mm / 0.10 mm
  - inner wall on T0 / 0.4 mm / 0.20 mm
  - sparse infill on T0 / 0.4 mm / mostly 0.20 mm
  - internal solid infill on T0 / 0.10 mm or 0.20 mm depending on overlap
  - no object extrusion on T2/T3
  - `mixed_nozzle_mode = mixed_layer` drove the result with legacy
    `inner_wall_combination` and `infill_combination` both disabled
- 20 mm cube G-code inspection with T0 `0.8 mm` and T1 `0.2 mm` also passed:
  - outer wall on T1 / 0.2 mm / 0.10 mm
  - inner wall on T0 / 0.8 mm / 0.40 mm
  - sparse infill on T0 / 0.8 mm / mostly 0.40 mm
  - internal solid infill on T0, remaining 0.10 mm on this cube where no
    four-layer-overlapped internal solid area was available
  - role/tool checker violations: 0
- Real Snapmaker U1 mixed-nozzle print on 2026-06-18 passed.

Repeatable checker command:

```powershell
python scripts\check_mixed_nozzle_gcode.py `
  --gcode path\to\plate_1.gcode `
  --outer-tool T1 --inner-tool T0 --sparse-infill-tool T0 --solid-infill-tool T0 `
  --forbid-object-tools T2,T3
```

## Real Print Photo

![Successful Snapmaker U1 mixed-nozzle print](https://raw.githubusercontent.com/MOVIBALE/OrcaSlicer/mixed-nozzle-u1/docs/mixed-nozzle-u1/assets/real-print-cube.jpg)

## Flashing Warning

This is experimental firmware and slicer behavior. Flash only if you can recover
the printer with a known-good firmware image.

The firmware patch changes nozzle validation only. It does not guarantee safe
flow, purge, wipe, temperature, pressure advance, or tool offset behavior for a
specific physical setup.

## Print Risk Notes

- Verify tool offsets after changing nozzle sizes.
- U1 offset calibration is center-to-center; line width and first-layer contact
  still need tuning for each nozzle.
- Watch the first layers and stop if either nozzle drags, under-extrudes, or
  leaves poor wall bonding.
- Avoid complex sloped or thin models until the mixed-layer V1 behavior is
  proven on simple geometry.
- Use distinct slot colors during verification if identical materials make
  preview hard to read.

## Known Limitations

- Mixed-layer internal walls are experimental and are not reclipped against
  geometry changes between fine layers.
- Mixed-layer internal solid infill is experimental and only combines fully overlapping
  internal-solid areas; top/bottom surfaces stay at the process layer height.
- Prime tower, wipe, and purge volumes are not fully tuned for asymmetric
  0.2/0.4 mm swaps.
- Other nozzle diameter pairs use the same generic mode controls. The 0.2/0.8
  mm pairing has G-code validation, but only the 0.2/0.4 mm pairing has
  real-print validation so far.
- Same-name/same-color material slots can make UI summaries look collapsed.
- No CI packaging has been added.

## Suggested Release Checklist

- [ ] Confirm the Windows build launches as the mixed-nozzle branch.
- [ ] Confirm the connected U1 filament sync maps physical heads into logical
      T0/T1/T2/T3 according to `extruder_map_table`.
- [ ] Slice a 20 mm cube and inspect preview by tool.
- [ ] Export G-code and rerun the object-role checker.
- [x] Flash the firmware on one test U1 only.
- [x] Print a small validation part and record first print success.
