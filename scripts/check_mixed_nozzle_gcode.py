#!/usr/bin/env python3
"""Validate mixed-nozzle tool assignment in Orca/Snapmaker G-code."""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path


FEATURE_ALIASES = {
    "outer": {
        "outer wall",
        "external perimeter",
        "external perimeters",
        "external wall",
    },
    "inner": {
        "inner wall",
        "inner walls",
        "perimeter",
        "perimeters",
        "internal perimeter",
        "internal perimeters",
    },
    "sparse": {
        "sparse infill",
        "internal infill",
        "infill",
    },
    "solid": {
        "internal solid infill",
        "solid infill",
        "solid internal infill",
    },
}


FEATURE_RE = re.compile(r"^;\s*(?:FEATURE|TYPE)\s*:\s*(.+?)\s*$", re.IGNORECASE)
TOOL_RE = re.compile(r"^\s*T(\d+)\b")
MOVE_RE = re.compile(r"^\s*G[0123]\b", re.IGNORECASE)
ABSOLUTE_E_RE = re.compile(r"^\s*M82\b", re.IGNORECASE)
RELATIVE_E_RE = re.compile(r"^\s*M83\b", re.IGNORECASE)
RESET_E_RE = re.compile(r"^\s*G92\b", re.IGNORECASE)
GCODE_NUMBER = r"-?(?:\d+(?:\.\d*)?|\.\d+)"
LAYER_Z_RE = re.compile(rf"^;\s*Z:\s*({GCODE_NUMBER})\s*$", re.IGNORECASE)
XY_RE = re.compile(rf"\b[XY]({GCODE_NUMBER})\b", re.IGNORECASE)
E_RE = re.compile(rf"\bE({GCODE_NUMBER})\b", re.IGNORECASE)


def normalize_tool(value: str | None) -> int | None:
    if value is None or value == "":
        return None
    value = value.strip().upper()
    if value.startswith("T"):
        value = value[1:]
    return int(value)


def parse_tool_list(value: str | None) -> set[int]:
    if not value:
        return set()
    return {normalize_tool(part) for part in value.split(",") if part.strip()}


def normalize_feature(raw: str) -> str | None:
    name = raw.strip().lower().replace("_", " ")
    for feature, aliases in FEATURE_ALIASES.items():
        if name in aliases:
            return feature
    return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gcode", required=True, type=Path)
    parser.add_argument("--outer-tool")
    parser.add_argument("--inner-tool")
    parser.add_argument("--sparse-infill-tool")
    parser.add_argument("--solid-infill-tool")
    parser.add_argument("--forbid-object-tools", default="")
    parser.add_argument("--expect-mixed-layer", action="store_true")
    parser.add_argument("--combined-features", default="sparse,inner,solid")
    parser.add_argument("--fine-layer-height", type=float, default=0.0)
    parser.add_argument("--coarse-layer-height", type=float, default=0.0)
    parser.add_argument("--tolerance", type=float, default=0.015)
    parser.add_argument("--json", action="store_true")
    return parser.parse_args()


def close_enough(value: float, target: float, tolerance: float) -> bool:
    return abs(value - target) <= tolerance


def main() -> int:
    args = parse_args()
    expected = {
        "outer": normalize_tool(args.outer_tool),
        "inner": normalize_tool(args.inner_tool),
        "sparse": normalize_tool(args.sparse_infill_tool),
        "solid": normalize_tool(args.solid_infill_tool),
    }
    forbidden_tools = parse_tool_list(args.forbid_object_tools)

    current_tool: int | None = None
    current_feature: str | None = None
    current_layer_z: float | None = None
    absolute_extrusion = True
    last_e_by_tool: dict[int, float] = defaultdict(float)
    extrusion_counts: dict[str, dict[int, int]] = defaultdict(lambda: defaultdict(int))
    feature_zs: dict[str, list[float]] = defaultdict(list)
    errors: list[str] = []
    warnings: list[str] = []

    with args.gcode.open("r", encoding="utf-8", errors="ignore") as fh:
        for lineno, line in enumerate(fh, 1):
            stripped = line.strip()
            command = line.split(";", 1)[0].strip()

            layer_z_match = LAYER_Z_RE.match(stripped)
            if layer_z_match:
                current_layer_z = float(layer_z_match.group(1))
                continue

            tool_match = TOOL_RE.match(command)
            if tool_match:
                current_tool = int(tool_match.group(1))
                current_feature = None
                continue

            feature_match = FEATURE_RE.match(stripped)
            if feature_match:
                current_feature = normalize_feature(feature_match.group(1))
                continue

            if ABSOLUTE_E_RE.match(command):
                absolute_extrusion = True
                continue

            if RELATIVE_E_RE.match(command):
                absolute_extrusion = False
                continue

            if RESET_E_RE.match(command):
                e_match = E_RE.search(command)
                if e_match and current_tool is not None:
                    last_e_by_tool[current_tool] = float(e_match.group(1))
                continue

            if not MOVE_RE.match(command):
                continue

            e_match = E_RE.search(command)
            if current_feature is None or current_tool is None or e_match is None or XY_RE.search(command) is None:
                continue

            e_value = float(e_match.group(1))
            if absolute_extrusion:
                previous_e = last_e_by_tool[current_tool]
                last_e_by_tool[current_tool] = e_value
                if e_value <= previous_e + 1e-7:
                    continue
            elif e_value <= 1e-7:
                continue

            extrusion_counts[current_feature][current_tool] += 1
            if current_layer_z is not None:
                feature_zs[current_feature].append(current_layer_z)

            expected_tool = expected.get(current_feature)
            if expected_tool is not None and current_tool != expected_tool:
                errors.append(
                    f"line {lineno}: {current_feature} extrusion used T{current_tool}, expected T{expected_tool}"
                )
            if current_tool in forbidden_tools:
                errors.append(
                    f"line {lineno}: object extrusion used forbidden T{current_tool} for {current_feature}"
                )

    if args.expect_mixed_layer and args.fine_layer_height > 0 and args.coarse_layer_height > 0:
        combined_features = {feature.strip() for feature in args.combined_features.split(",") if feature.strip()}
        unknown_features = combined_features.difference({"sparse", "inner", "solid"})
        if unknown_features:
            errors.append(f"unknown combined feature(s): {', '.join(sorted(unknown_features))}")
        for feature in ("sparse", "inner", "solid"):
            if feature not in combined_features:
                continue
            zs = sorted(set(round(z, 5) for z in feature_zs.get(feature, [])))
            if len(zs) < 2:
                continue
            deltas = [round(zs[i] - zs[i - 1], 5) for i in range(1, len(zs))]
            coarse_like = [d for d in deltas if close_enough(d, args.coarse_layer_height, args.tolerance)]
            fine_like = [d for d in deltas if close_enough(d, args.fine_layer_height, args.tolerance)]
            if coarse_like and len(coarse_like) >= len(fine_like):
                continue
            warnings.append(
                f"{feature}: layer cadence does not look predominantly coarse "
                f"({len(coarse_like)} coarse-like deltas, {len(fine_like)} fine-like deltas)"
            )

    report = {
        "gcode": str(args.gcode),
        "counts": {feature: dict(tools) for feature, tools in extrusion_counts.items()},
        "errors": errors,
        "warnings": warnings,
        "ok": not errors,
    }

    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"Checked: {args.gcode}")
        for feature in ("outer", "inner", "sparse", "solid"):
            tools = extrusion_counts.get(feature, {})
            if tools:
                rendered = ", ".join(f"T{tool}: {count}" for tool, count in sorted(tools.items()))
                print(f"{feature}: {rendered}")
        for warning in warnings:
            print(f"WARNING: {warning}", file=sys.stderr)
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        print("PASS" if not errors else "FAIL")

    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
