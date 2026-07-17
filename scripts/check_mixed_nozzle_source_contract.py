#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    dialog = (ROOT / "src/slic3r/GUI/MixedNozzleWorkstationDialog.cpp").read_text(encoding="utf-8")
    plater = (ROOT / "src/slic3r/GUI/Plater.cpp").read_text(encoding="utf-8")

    set_width_match = re.search(
        r"void\s+MixedNozzleWorkstationDialog::set_feature_line_width\b.*?\{(?P<body>.*?)\n\}",
        dialog,
        re.S,
    )
    if not set_width_match:
        fail("MixedNozzleWorkstationDialog::set_feature_line_width was not found")
    if "multiplier * 100." not in set_width_match.group("body") or "true" not in set_width_match.group("body"):
        fail("mixed nozzle workstation must write feature line widths as nozzle-relative percentages")

    nozzle_ui_match = re.search(
        r"void\s+Sidebar::update_nozzle_settings\b.*?\{(?P<body>.*?)\n\}\n\nObjectList\*",
        plater,
        re.S,
    )
    if not nozzle_ui_match:
        fail("Sidebar::update_nozzle_settings was not found")
    body = nozzle_ui_match.group("body")
    required_sidebar_snippets = [
        "wxEVT_COMBOBOX",
        "apply_printer_nozzle_diameters",
        "current_printer_nozzle_diameters",
        "diameters_of_selected_printer",
    ]
    for snippet in required_sidebar_snippets:
        if snippet not in body:
            fail("sidebar nozzle panel must be a shortcut editor for native nozzle_diameter")
    if "m_nozzle_diameter_lists" in plater or "m_nozzle_edit_btns" in plater:
        fail("sidebar nozzle shortcut must not keep a second nozzle diameter control state")

    print("PASS: mixed nozzle source contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
