# Mixed Nozzle U1 Release Notes

## Scope

This release is an experimental Snapmaker U1 software build for mixed-nozzle
printing, paired with a patched U1 extended firmware build.

Real-print validated scenario:

- 0.2 mm nozzle for visible outer walls
- 0.4 mm nozzle for inner walls and infill

The slicer now exposes two generic mixed-nozzle modes instead of relying on
fixed per-nozzle-pair process presets:

- Same layer, different line widths
- Mixed layer, different line widths

The Prepare sidebar also includes a Mixed Nozzle workstation that groups the
mode, feature assignment, layer combining, nozzle map, and validation summary
into one workflow.

## Software Changes

- Added `outer_wall_filament` as a separate feature filament setting.
- Routed external perimeter extrusion to `outer_wall_filament`.
- Preserved existing inner wall, sparse infill, and solid infill feature
  filament settings.
- Added the Mixed Nozzle workstation in the Prepare sidebar.
- Split mixed-layer combination into separate sparse infill, inner wall, and
  internal solid infill switches.
- Sparse infill combining is the conservative mixed-layer default. Inner wall
  and internal solid infill combining are experimental options.
- Added automatic coarse layer height targeting. The default target is half of
  the coarse nozzle diameter, limited by the coarse extruder max layer height
  when configured.
- Added `inner_wall_combination` and
  `inner_wall_combination_max_layer_height`.
- Added `mixed_nozzle_mode`, automatic coarse layer height targeting, and a
  manual coarse layer height override so mixed-layer height is derived from the
  active fine layer height and the selected coarse nozzle. Legacy ratio controls
  remain available as an advanced compatibility path.
- Kept U1 nozzle diameters independently editable per nozzle tab.
- Added nozzle diameter labels to U1 nozzle tabs.
- Updated printer filament sync to read filament and nozzle information from
  the connected U1.
- Mapped machine filament data from physical head order into logical T-slot
  order with `extruder_map_table`.
- Added starter Snapmaker U1 mixed-nozzle process presets as mode examples.
- Added `scripts/check_mixed_nozzle_gcode.py` for repeatable role/tool G-code
  verification.
- Added focused unit tests for machine filament slot mapping and mixed-layer
  span planning.

## Firmware Requirement

Use a U1 firmware build that includes the mixed-nozzle validation patch from
`SnapmakerU1-Extended-Firmware`.

The patch changes Klipper-side print task validation so each used logical
extruder is checked against its mapped physical toolhead:

`logical_index -> extruder_map_table[logical_index] -> actual physical nozzle`

Without this patch, the printer may reject mixed-nozzle G-code even when the
physical nozzles match the slicer setup.

## Validation

Local validation completed on Windows:

- Snapmaker Orca Release build: passed.
- Focused `MachineFilamentSync` unit test: passed.
- Focused `MixedLayerHeight` unit tests: cover span planning, automatic coarse
  layer height, max layer height clamping, and legacy ratio fallback.
- Snapmaker profile validator: passed.
- Cube G-code role inspection: passed.
- Real Snapmaker U1 mixed-nozzle print on 2026-06-18: passed.

G-code inspection results for the 0.10 mixed-layer cube:

- outer wall: T1, 0.2 mm nozzle, 0.10 mm layers
- inner wall: T0, 0.4 mm nozzle, 0.20 mm combined layers
- sparse infill: T0, 0.4 mm nozzle, mostly 0.20 mm combined layers
- internal solid infill: T0, 0.10 mm or 0.20 mm depending on overlap
- no object extrusion on T2/T3
- the checked profile had `inner_wall_combination = 0` and
  `infill_combination = 0`, so mixed-layer behavior was driven by
  `mixed_nozzle_mode = mixed_layer`

The repeatable G-code checker entry point is:

```powershell
python scripts\check_mixed_nozzle_gcode.py `
  --gcode path\to\plate_1.gcode `
  --outer-tool T1 --inner-tool T0 --sparse-infill-tool T0 --solid-infill-tool T0 `
  --forbid-object-tools T2,T3
```

Additional G-code-only validation for a 0.2/0.8 mm pairing passed:

- outer wall: T1, 0.2 mm nozzle, 0.10 mm layers
- inner wall: T0, 0.8 mm nozzle, 0.40 mm combined layers
- sparse infill: T0, 0.8 mm nozzle, mostly 0.40 mm combined layers
- internal solid infill: T0, remained 0.10 mm on the cube where no
  four-layer-overlapped internal solid area was available
- role/tool checker violations: 0

Real print validation:

![Successful Snapmaker U1 mixed-nozzle print](assets/real-print-cube.jpg)

## Compatibility Notes

- Target printer: Snapmaker U1.
- Target firmware base: SnapmakerU1-Extended-Firmware 1.4.1 based build.
- Target slicer base: Snapmaker Orca 2.3.5 Beta (`761718a5`).
- Real-print tested nozzle pairing: 0.4 mm logical T0 and 0.2 mm logical T1.

Other nozzle pairings should use the same mode controls. The 0.2/0.8 mm pairing
has G-code validation for automatic 4-layer combining, but has not been
real-print validated yet.

## Limitations

- The first real print passed, but print quality is still experimental. Tune
  speeds, temperature, flow, pressure advance, wipe, and purge before long
  prints.
- Mixed-layer internal walls are not geometry-aware across different Z slices
  yet and must be enabled explicitly. Complex sloped or thin geometry can still
  need slicer logic changes.
- Mixed-layer internal solid infill is intersection-based and will only combine
  fully overlapping internal-solid areas. Top and bottom surfaces intentionally
  stay at the process layer height.
- Same-layer mode still depends on the selected feature filaments and line
  widths being physically sensible for the installed nozzles.
- Preview and material summaries can be confusing when multiple slots use the
  same filament name/color.
- The firmware patch only changes validation. It does not make every mechanical
  or material combination safe.
- Release packaging remains experimental and must be marked as a pre-release.

## Recovery

Keep a known-good U1 firmware `.bin` available before flashing. If the firmware
test build fails to boot cleanly, use the extended firmware recovery/reset path
or reflash a known-good release build.

ESP32 Timelapse Box support included in the same slicer build is independent
of the mixed-nozzle firmware patch. It uses `ESP_TIMELAPSE_SHOT` on Klipper and
must not be described as a firmware-patch dependency.
