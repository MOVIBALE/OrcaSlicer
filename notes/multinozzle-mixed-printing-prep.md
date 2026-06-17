# Multi-Nozzle Mixed Printing Prep

Date: 2026-06-16

## Local workspace

- Main repo: `F:\FC\Snapmaker-OrcaSlicer`
- Current work branch: `codex/multinozzle-mixed-printing`
- Base branch: `main`
- Base HEAD: `b5b85a3`
- Local app version in `version.inc`: `Snapmaker_VERSION 2.3.4`
- Remotes:
  - `origin`: `https://github.com/Snapmaker/OrcaSlicer.git`
  - `orca-upstream`: `https://github.com/OrcaSlicer/OrcaSlicer.git`
  - `fullspectrum`: `https://github.com/ratdoux/OrcaSlicer-FullSpectrum.git`
- Fetched comparison refs:
  - `orca-upstream/main`
  - `orca-upstream/release/v2.4`
  - `fullspectrum/main`

## External references

- Snapmaker Orca repo: https://github.com/Snapmaker/OrcaSlicer
  - Snapmaker Orca is based on OrcaSlicer.
  - Windows build notes mention Visual Studio, CMake, git, git-lfs, Strawberry Perl, and `git lfs pull` after cloning.
- OrcaSlicer mixed nozzle guide: https://www.orcaslicer.com/wiki/guides/mixed_nozzle_sizes
  - OrcaSlicer documents mixed nozzle sizes as available since v2.2.0-beta.
  - The documented setup uses per-extruder nozzle diameters and percent-based line widths.
- OrcaSlicer issue #11424: https://github.com/OrcaSlicer/OrcaSlicer/issues/11424
  - Open user request for toolchanger/U1-style multiple nozzle sizes in one print.
  - Requested UX: show nozzle size per toolhead and allow per-head values.
- OrcaSlicer discussion #10175: https://github.com/OrcaSlicer/OrcaSlicer/discussions/10175
  - Important concern: different nozzles often imply different layer heights, max flow rates, and speeds.
  - "Combine infill layers" may be relevant for coarse-nozzle infill with fine-nozzle walls.
- FullSpectrum fork: https://github.com/ratdoux/OrcaSlicer-FullSpectrum
  - Focuses on virtual mixed-color filaments for Snapmaker U1 by alternating physical filaments/layers.
  - Useful as a reference, but this local Snapmaker main already contains substantial mixed-filament code.

## Local code map

- `src/libslic3r/PrintRegion.cpp`
  - `PrintRegion::flow()` selects line width by flow role, then gets the nozzle diameter from the extruder assigned to that role.
  - This is the main proof that the flow path is already role/extruder aware.
- `src/libslic3r/PrintConfig.cpp`
  - Line width settings use `ConfigOptionFloatOrPercent` with `ratio_over = "nozzle_diameter"`.
  - Percent line widths are the documented path for nozzle-agnostic profiles.
- `src/libslic3r/Print.cpp`
  - Validation computes min/max used nozzle diameter.
  - Current layer height check uses the smallest used nozzle.
  - Extrusion width validation checks absolute values against min/max nozzle sizes.
  - A historical support restriction for different nozzle diameters is currently disabled with `#if 0`.
- `src/slic3r/GUI/Plater.cpp`
  - `Sidebar::update_nozzle_settings()` creates one nozzle tab per configured nozzle diameter.
  - Current Snapmaker U1 behavior warns that changing one nozzle syncs all other nozzles.
  - The combo handler then sets every nozzle combo to the same value and selects a single similar printer preset.
  - This looks like the primary UI/profile blocker for independent per-head nozzle selection.
- `src/libslic3r/MixedFilament.cpp` and `.hpp`
  - Mixed virtual filament IDs start after the physical filament count.
  - `resolve()` and `resolve_perimeter()` map a virtual filament back to physical extruders by layer/perimeter/pattern.
  - `mixed_filament_reference_nozzle_mm()` already computes a reference width from component nozzle diameters.
  - Display/bias code already accepts per-component nozzle diameters.
- `src/libslic3r/GCode.cpp`
  - Mixed virtual extrusions can be split into physical extruder buckets through `resolve_perimeter()`.
  - G-code generation has Local-Z and mixed-filament handling paths that will need regression tests for unequal nozzle sizes.

## Initial implementation hypothesis

1. First target should be "same layer height, different nozzle diameter" because current validation already requires layer height to fit the smallest used nozzle.
2. Profiles should be nozzle-agnostic by converting relevant line widths to percentages of nozzle diameter.
3. UI work should stop syncing all U1 nozzles to one diameter and persist a vector like `0.4;0.6;0.4;0.8`.
4. Slicing logic may already handle role-specific flows if roles map to physical extruders correctly.
5. Mixed virtual filament logic needs tests for virtual IDs whose components have different nozzle diameters.
6. More advanced mixed-height printing should be a later phase because it affects layer planning, flow limits, wipe tower scheduling, and preview/G-code semantics.

## V1 decisions from user

1. Target hardware is Snapmaker U1 with four independent heads.
2. First nozzle combination is `0.2 + 0.4`.
3. Desired feature mapping is outer wall on `0.2`, inner walls and infill on `0.4`.
4. V1 uses one shared layer height capped by the smallest nozzle.
5. Focus is local play/testing first; CI and release packaging are intentionally out of scope for now.
6. SnapOrca has per-head nozzle diameter fields underneath, but the current U1 sidebar/sync path still forces all nozzles to one diameter.

## Implemented V1 patch

1. Added `outer_wall_filament` with `0 = inherit wall_filament`.
2. Updated role-to-extruder mapping so `frExternalPerimeter` can use `outer_wall_filament`.
3. Passed a separate external-perimeter `Flow` into `PerimeterGenerator`, so outer wall width/nozzle calculations can use the 0.2 nozzle.
4. Split mixed external/internal perimeter collections in G-code bucketing when outer wall and wall filaments differ.
5. Added config propagation, invalidation, painting, object-list, preset hint, and dynamic-list handling for the new field.
6. Removed the earlier fake `0.2+0.4` machine variant approach.
7. Added an experimental process preset, `0.12 Mixed Nozzle Outer T0 Inner T1 @Snapmaker U1`, that relies on per-nozzle `nozzle_diameter` values instead of a combined variant.
8. Added G-code verification plan in `notes/multinozzle-gcode-verification-plan.md`.

## Local verification

1. JSON parse passed for the Snapmaker index, new machine, new process, and touched PLA filament profiles.
2. `git diff --check` passed with only existing Windows line-ending warnings.
3. A local Windows build environment was prepared with VS Build Tools 2022 and portable Strawberry Perl.

## Immediate next steps

1. Get a local Windows build environment working or open the project in an existing SnapOrca build setup.
2. Slice a small cube with the standard U1 machine, independent T0/T1 nozzle diameters, and the experimental process; inspect preview/G-code for T0 external walls and T1 inner walls/infill.
3. Tune speeds, prime/wipe behavior, and line widths after the first G-code inspection.
