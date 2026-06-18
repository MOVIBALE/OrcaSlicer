# GitHub Release Draft: mixed-nozzle-u1

## Title

Snapmaker U1 mixed-nozzle experimental build

## Tag

`mixed-nozzle-u1-v0.1.0`

## Target Branch

`mixed-nozzle-u1`

## Summary

Experimental Snapmaker U1 mixed-nozzle build for printing fine outer walls with
a 0.2 mm nozzle and inner walls/infill with a 0.4 mm nozzle.

This release requires both:

- the patched Snapmaker Orca build from this branch
- the matching SnapmakerU1-Extended-Firmware mixed-nozzle validation patch

## Highlights

- Independent per-head nozzle diameter editing for U1.
- Nozzle diameter shown in each nozzle tab.
- `outer_wall_filament` setting for routing external perimeters to a different
  tool than inner walls.
- Machine filament sync from the connected U1.
- Correct physical-head to logical T-slot filament mapping through
  `extruder_map_table`.
- Experimental 0.10/0.20 mixed-layer profile:
  `0.10 Mixed Layer Outer Nozzle2 Inner Nozzle1 @Snapmaker U1`.
- Starter same-layer profile:
  `0.12 Mixed Nozzle Outer T0 Inner T1 @Snapmaker U1`.

## Assets To Attach

- Windows Snapmaker Orca build artifact.
- U1 firmware `.bin` built from the firmware branch with the validation patch.
- Optional sample cube G-code generated from the 0.10 mixed-layer profile.
- Real print validation photo:
  `docs/mixed-nozzle-u1/assets/real-print-cube.jpg`

Current local firmware artifact:

`F:\FC\SnapmakerU1-Extended-Firmware\firmware\U1_extended_1.4.1-paxx12-19_mixed-nozzle-codex.bin`

## Validation Results

- Snapmaker Orca Release build passed.
- `libslic3r_tests.exe [MachineFilamentSync]` passed, 17 assertions.
- `libslic3r_tests.exe [MixedLayerHeight]` passed, 7 assertions.
- Snapmaker profile validator passed for the Snapmaker profile bundle.
- 20 mm cube G-code inspection passed:
  - outer wall on T1 / 0.2 mm / 0.10 mm
  - inner wall on T0 / 0.4 mm / 0.20 mm
  - sparse infill on T0 / 0.4 mm / mostly 0.20 mm
  - internal solid infill on T0
  - no object extrusion on T2/T3
- Real Snapmaker U1 mixed-nozzle print on 2026-06-18 passed.

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

- Mixed-layer internal walls are not reclipped against geometry changes between
  fine layers.
- Prime tower, wipe, and purge volumes are not fully tuned for asymmetric
  0.2/0.4 mm swaps.
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
