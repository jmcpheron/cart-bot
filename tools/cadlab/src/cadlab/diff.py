"""Compare two design revisions — the command the pycon2026 toolkit lacked.

Takes two STEP files (or two previously written report TOMLs) and prints a
markdown-friendly delta table of the metrics that matter when iterating on a
printed part: bounding box, volume, surface area, mass, solids, triangles.
"""

from __future__ import annotations

import tomllib
from pathlib import Path

from cadlab.inspect import compute_metrics

_SCALARS = [
    ("volume_mm3", "Volume (mm³)"),
    ("area_mm2", "Surface area (mm²)"),
    ("pla_mass_g", "PLA mass @100% (g)"),
    ("solids", "Solids"),
    ("triangles", "Triangles"),
]
_AXES = ["X", "Y", "Z"]


def _metrics_for(path: Path) -> dict:
    if path.suffix.lower() == ".toml":
        return tomllib.loads(path.read_text())["metrics"]
    return compute_metrics(path)


def _fmt(v: float) -> str:
    return f"{v:,.1f}" if isinstance(v, float) else f"{v:,}"


def _row(name: str, a, b) -> str:
    delta = b - a
    pct = f"{delta / a * 100.0:+.1f}%" if a else "—"
    sign = f"{delta:+,.1f}" if isinstance(delta, float) else f"{delta:+,}"
    mark = "" if delta == 0 else "  ◀" if delta < 0 else "  ▶"
    return f"| {name} | {_fmt(a)} | {_fmt(b)} | {sign} | {pct}{mark} |"


def diff_report(path_a: Path, path_b: Path) -> str:
    a, b = _metrics_for(path_a), _metrics_for(path_b)
    lines = [
        f"### CAD diff: `{a['file']}` → `{b['file']}`",
        "",
        "| Metric | A | B | Δ | Δ% |",
        "|---|---|---|---|---|",
    ]
    for axis, name_a, name_b in zip(_AXES, a["bbox_size"], b["bbox_size"]):
        lines.append(_row(f"Bounding box {axis} (mm)", name_a, name_b))
    for key, label in _SCALARS:
        lines.append(_row(label, a[key], b[key]))

    notes = []
    for axis, sa, sb in zip(_AXES, a["bbox_size"], b["bbox_size"]):
        if abs(sb - sa) > 0.05:
            notes.append(f"{axis} {'shrank' if sb < sa else 'grew'} by {abs(sb - sa):.2f} mm")
    dv = b["volume_mm3"] - a["volume_mm3"]
    if abs(dv) > 1:
        notes.append(
            f"volume {'removed' if dv < 0 else 'added'}: {abs(dv):,.0f} mm³ "
            f"(≈{abs(b['pla_mass_g'] - a['pla_mass_g']):.1f} g PLA)")
    if notes:
        lines.extend(["", "Summary: " + "; ".join(notes)])
    return "\n".join(lines)
