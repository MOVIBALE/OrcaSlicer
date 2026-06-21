# Snapmaker U1 Mixed Nozzle Printing

This branch is an experimental Snapmaker U1 mixed-nozzle workflow for using
different nozzle diameters in one print.

The first real-print validated setup is:

- logical T0 / nozzle 1: 0.4 mm, inner walls and infill
- logical T1 / nozzle 2: 0.2 mm, outer walls
- logical T2/T3: available but unused by the starter profiles

The implementation is not tied to that one nozzle pair. Configure other pairs
by setting each U1 nozzle diameter, choosing feature filaments, setting the
feature line widths, then selecting one of the two mixed nozzle modes:

- `Same layer, different line widths`: outer walls, inner walls, and infill
  stay on the current process layer height.
- `Mixed layer, different line widths`: outer walls stay on the current fine
  layer height, while inner walls and sparse infill are combined by an
  automatic nozzle-diameter ratio. Disable the auto option only when you need
  to force a manual ratio.

The matching U1 firmware patch is required. Stock firmware validates every used
physical nozzle against the first slicer nozzle diameter and rejects mixed-nozzle
jobs. The patched firmware compares each used logical tool with the physical
toolhead selected by `extruder_map_table`.

## What Changed

- Added `outer_wall_filament`, separate from `wall_filament`.
- Routed external perimeter G-code to `outer_wall_filament` while keeping inner
  perimeters and infill on their configured tools.
- Kept per-head U1 nozzle diameters independent in the printer sidebar.
- Displayed the active nozzle diameter in each nozzle tab label.
- Added printer filament sync from the connected U1.
- Mapped synced machine filament slots from physical head order into slicer
  logical T-slot order through `extruder_map_table`.
- Added `mixed_nozzle_mode`, `mixed_nozzle_auto_layer_height_ratio`, and
  `mixed_nozzle_layer_height_ratio` as generic controls for same-layer and
  mixed-layer mixed-nozzle slicing.
- Added experimental mixed-layer planning for internal walls and infill, so
  fine outer walls can be paired with combined coarse-nozzle inner
  walls/infill without hardcoding a 0.20 mm process value.
- Added two starter process profiles as examples:
  - `0.12 Mixed Nozzle Outer T0 Inner T1 @Snapmaker U1`
  - `0.10 Mixed Layer Outer Nozzle2 Inner Nozzle1 @Snapmaker U1`

## Setup

1. Flash a U1 firmware build that includes the mixed-nozzle validation patch.
2. Start this Snapmaker Orca build.
3. Select the Snapmaker U1 printer.
4. Connect to the printer from the Device page.
5. In the Prepare sidebar, use the filament sync button in the filament section
   to load the four head materials from the printer.
6. Set nozzle 1 to `0.4 mm` and nozzle 2 to `0.2 mm`.
7. Select `0.10 Mixed Layer Outer Nozzle2 Inner Nozzle1 @Snapmaker U1`.
8. Check the feature filament settings:
   - Outer wall: slot 2 / T1 / 0.2 mm
   - Wall: slot 1 / T0 / 0.4 mm
   - Sparse infill: slot 1 / T0 / 0.4 mm
   - Solid infill: slot 1 / T0 / 0.4 mm
9. In Strength > Advanced, confirm:
   - Mixed nozzle mode: `Mixed layer, different line widths`
   - Auto mixed nozzle layer ratio: enabled
   - Manual mixed nozzle layer height ratio: hidden unless auto ratio is disabled
10. Slice a simple cube before testing real parts.

For the older same-layer-height test profile, use
`0.12 Mixed Nozzle Outer T0 Inner T1 @Snapmaker U1` and confirm the tool mapping
shown in that profile name. It should use `Same layer, different line widths`.

## G-code Verification

The latest local cube validation used:

- model: `20mmbox-LF.stl`
- slicer process: `0.10 Mixed Layer Outer Nozzle2 Inner Nozzle1 @Snapmaker U1`
- mixed nozzle mode: `mixed_layer`, automatic layer ratio enabled
- nozzle table: T0 `0.4 mm`, T1 `0.2 mm`, T2 `0.4 mm`, T3 `0.4 mm`
- output: `F:\FC\snaporca_gcode_check\auto_ratio_solid_20260621-145510\out_0402\plate_1.gcode`

Observed G-code characteristics:

- tool changes: T0 `103`, T1 `100`
- header widths:
  - external perimeter: `0.20 mm`
  - perimeter: `0.42 mm`
  - infill: `0.44 mm`
  - solid infill: `0.42 mm`
- outer wall moves: all T1, all normal layers at `HEIGHT=0.1`
- inner wall moves: all T0, combined layers at `HEIGHT=0.2`
- sparse infill moves: all T0, mostly `HEIGHT=0.2`
- internal solid infill moves: all T0, `HEIGHT=0.1` or `HEIGHT=0.2`
- no object extrusion on T2/T3
- legacy `inner_wall_combination` and `infill_combination` were both disabled
  in the profile, confirming the mixed-layer result came from
  `mixed_nozzle_mode`

An additional G-code-only check changed the coarse nozzle to T0 `0.8 mm` while
keeping T1 `0.2 mm` for outer walls:

- output: `F:\FC\snaporca_gcode_check\auto_ratio_solid_20260621-145510\out_0802\plate_1.gcode`
- outer wall moves: all T1 at `HEIGHT=0.1`
- inner wall moves: all T0 at `HEIGHT=0.4`
- sparse infill moves: all T0, mostly `HEIGHT=0.4`
- internal solid infill moves: all T0; this cube did not have enough
  four-layer-overlapped internal solid area to produce `HEIGHT=0.4`
- role/tool checker violations: `0`

The object-role checker found no violations for the expected mapping:

- outer wall -> T1 / 0.2 mm
- inner wall -> T0 / 0.4 mm
- sparse infill -> T0 / 0.4 mm
- internal solid infill -> T0 / 0.4 mm

## Real Print Validation

A real Snapmaker U1 test print completed successfully on 2026-06-18 with the
patched slicer and patched U1 firmware. The printed sample shows the mixed
strategy working on hardware: fine outer shell behavior paired with the coarser
internal grid/infill path.

![Successful Snapmaker U1 mixed-nozzle print](assets/real-print-cube.jpg)

## Tests

Local checks run on Windows:

- `Snapmaker_Orca` Release target: build passed.
- `libslic3r_tests.exe [MachineFilamentSync]`: passed, 17 assertions.
- `libslic3r_tests.exe [MixedLayerHeight]`: passed, 17 assertions.
- `Snapmaker_Orca_profile_validator.exe --vendor Snapmaker`: passed.
- Real U1 mixed-nozzle test print: passed.

Build warnings seen during local verification:

- MSVC link warning `LNK4098` from existing dependency runtime mix.
- `Plater.cpp` warning `C4101` for an unused local exception variable.
- CMake developer warnings from existing project CMake policies.

## Known Risks

This is experimental and should be treated as a local test branch, not a
production profile set.

- The firmware patch only fixes mixed-nozzle validation. It does not validate
  pressure advance, flow limits, purge volume, toolchange wiping, bed contact,
  or print quality.
- U1 multi-tool offset calibration is center-to-center. Different nozzle sizes
  can share the same center offsets, but wall overlap, line width, and first
  layer contact still need physical tuning.
- Mixed-layer internal walls are V1 logic. It combines internal wall paths onto
  the upper fine layer and removes the lower fine-layer internal wall path. It
  does not yet reclipped/reintersect internal walls against changing geometry.
- Mixed-layer internal solid infill is best-effort and intersection-based. It
  combines only where the same internal-solid area exists across the full
  automatic layer span; top/bottom surfaces stay at the process layer height.
- Other nozzle pairs now share the same mode controls. The 0.2/0.8 mm pairing
  has G-code validation for automatic 4-layer combining, but only the 0.2/0.4
  mm pairing has real-print validation so far.
- Sloped walls, small islands, thin features, holes, and top/bottom transitions
  need real print testing.
- Identical filament names and colors can make preview/material summaries hard
  to distinguish. Use machine sync or temporarily assign distinct colors during
  verification.
- Prime tower, purge, and wipe behavior is not fully tuned for asymmetric
  0.2/0.4 mm tool changes.
- Always keep a recovery path for U1 firmware before flashing test builds.
- The first real print passed, but longer prints and complex geometry still
  need validation.

## Suggested First Print

Use a 20 mm cube with the 0.10 mixed-layer profile.

Before printing, inspect preview by tool:

- shell outline should alternate as T1 fine outer walls every 0.10 mm
- inner walls and infill should appear on T0 every 0.20 mm
- T2/T3 should not appear in object extrusion

Stop after the first few layers if tool offsets, purge, or wall bonding look
wrong.
