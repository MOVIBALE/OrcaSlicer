# Mixed Nozzle G-code Verification Plan

Date: 2026-06-16

## Goal

Verify the V1 Snapmaker U1 mixed-nozzle path:

- T0: 0.2 mm nozzle, outer wall only.
- T1: 0.4 mm nozzle, inner wall, sparse infill, solid infill, and top surface.
- T2/T3: unused for object extrusion.
- Shared layer height: 0.12 mm.

## Test Model

Use the existing 20 mm cube:

`F:\FC\Snapmaker-OrcaSlicer\tests\data\test_stl\ASCII\20mmbox-LF.stl`

Reason: a plain cube is enough to produce middle layers with outer walls, inner walls, and sparse infill. Avoid calibration towers or detailed models for the first check because they add roles and toolpaths that make failures harder to isolate.

## Slicer Setup

Use these presets:

- Machine: `Snapmaker U1 (0.4 nozzle)`
- Nozzle diameters: set T0/nozzle 1 to `0.2 mm`, keep T1/nozzle 2 at `0.4 mm`.
- Process: `0.12 Mixed Nozzle Outer T0 Inner T1 @Snapmaker U1`
- Filaments: use `Generic PLA` or `Snapmaker PLA @U1` for at least T0 and T1. If the CLI requires all four U1 filament slots, load the same filament profile four times.

For the first verification run, disable unrelated output features:

- `enable_prime_tower = 0`
- `brim_width = 0`
- `skirt_loops = 0`
- `enable_support = 0`
- `wall_loops = 2`
- `sparse_infill_density = 15%`

Keep the experimental mapping:

- `outer_wall_filament = 1`
- `wall_filament = 2`
- `sparse_infill_filament = 2`
- `solid_infill_filament = 2`

## GUI Procedure

1. Open the mixed-nozzle branch build.
2. Select machine `Snapmaker U1 (0.4 nozzle)`.
3. In the printer sidebar, set nozzle 1 to `0.2 mm` and leave nozzle 2 at `0.4 mm`.
4. Select process `0.12 Mixed Nozzle Outer T0 Inner T1 @Snapmaker U1`.
5. Load `20mmbox-LF.stl`.
6. Apply the temporary verification overrides above.
7. Slice and export G-code to `F:\FC\Snapmaker-OrcaSlicer\scratch\mixed-nozzle-verify`.
8. In preview, color by tool:
   - Outer perimeter paths should be T0.
   - Inner perimeter and infill paths should be T1.
   - T2/T3 should not appear on the object.

## CLI Candidate

After a local executable exists, use this shape and adjust the executable path:

```powershell
$exe = "F:\FC\Snapmaker-OrcaSlicer\build\src\Release\Snapmaker_Orca.exe"
$out = "F:\FC\Snapmaker-OrcaSlicer\scratch\mixed-nozzle-verify"
$machine = "F:\FC\Snapmaker-OrcaSlicer\resources\profiles\Snapmaker\machine\Snapmaker U1 (0.4 nozzle).json"
$process = "F:\FC\Snapmaker-OrcaSlicer\resources\profiles\Snapmaker\process\0.12 Mixed Nozzle Outer T0 Inner T1 @Snapmaker U1.json"
$fil = "F:\FC\Snapmaker-OrcaSlicer\resources\profiles\Snapmaker\filament\Snapmaker PLA @U1.json"
$model = "F:\FC\Snapmaker-OrcaSlicer\tests\data\test_stl\ASCII\20mmbox-LF.stl"

New-Item -ItemType Directory -Force $out | Out-Null
& $exe --slice 0 --outputdir $out --load_settings "$machine;$process" --load_filaments "$fil;$fil;$fil;$fil" --nozzle_diameter "0.2;0.4;0.4;0.4" $model
```

If this produces a profile compatibility error, do the first run through the GUI, then export a `.3mf` project and use CLI slicing on that `.3mf`.

## G-code Checks

Parse only after the first feature/role tag, so machine start G-code and purge routines do not pollute the result.

Track:

- Current tool from lines matching `^T(\d+)\b`.
- Role from either `; FEATURE: <role>` or `;TYPE:<role>`.
- Line width from either `; LINE_WIDTH: <mm>` or `;WIDTH:<mm>`.
- Extrusion moves from `G1` lines with positive `E`.

Pass criteria:

- At least one object extrusion under T0 and T1.
- Every `Outer wall` extrusion move is under T0.
- Every `Inner wall`, `Sparse infill`, `Internal solid infill`, and `Top surface` extrusion move is under T1.
- No object extrusion moves under T2/T3.
- Median normal-layer line widths are close to:
  - Outer wall: 0.20 mm, tolerance ±0.03 mm.
  - Inner wall: 0.42 mm, tolerance ±0.04 mm.
  - Sparse infill: 0.44 mm, tolerance ±0.05 mm.
- Layer height tags include 0.12 mm for normal object layers.

Initial layer can have different widths because `initial_layer_line_width = 120%`; do not use the first layer as the width pass/fail source.

## Failure Triage

- Outer wall uses T1: profile mapping was not loaded, or perimeter G-code role splitting did not run.
- Outer wall width is around 0.42 mm: external-perimeter flow is still using inner-wall/nozzle state, or the process is using an absolute width override.
- Inner wall or infill uses T0: role-to-extruder mapping or G-code bucketing regressed.
- T2/T3 extrude object paths: filament list/tool ordering is leaking unused tools into object extrusion.
- No sparse infill appears: model/settings generated only walls/top/bottom; reduce top/bottom layers or increase cube height.

## Second-Pass Stress Checks

After the cube passes:

1. Repeat with `wall_loops = 3` to confirm multiple internal perimeter loops stay on T1.
2. Re-enable prime tower and verify wipe tower paths are ignored by the object-role checker.
3. Test a 40 mm cube with `sparse_infill_density = 10%` to get more infill moves.
4. Test `wall_infill_order` variants to confirm tool mapping is independent of print order.
