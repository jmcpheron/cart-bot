"""STEP inspection — header metadata, geometry metrics, diffable reports.

Ported from pycon2026 cardlab/inspect.py; adds exact volume, surface area,
center of mass, and an estimated PLA print mass (the metrics that matter when
the question is "what changed between chassis rev 1 and rev 2").
"""

from __future__ import annotations

import re
import struct
import tempfile
import tomllib
from dataclasses import dataclass
from pathlib import Path

# The HEADER section of a STEP file is plain text and almost always under
# a few KB. Parsing it with regex is faster, simpler, and dependency-free
# compared to spinning up the OCCT XCAF stack just to read the schema name.
_HEADER_FIELD_RE = re.compile(r"FILE_(NAME|SCHEMA|DESCRIPTION)\s*\(([^;]*)\);", re.S)

PLA_DENSITY_G_CM3 = 1.24

# Tessellation used for the committed STLs and the triangle-count metric.
LINEAR_DEFLECTION = 0.1   # mm — max chord error from the true surface
ANGULAR_DEFLECTION = 0.5  # rad — max angle between adjacent facets


@dataclass(frozen=True)
class StepHeader:
    schema: str
    originating: str
    author: str
    timestamp: str
    description: str


def parse_header(path: Path) -> StepHeader:
    """Read just the HEADER section of a STEP file (text, top of file)."""
    with path.open("r", encoding="utf-8", errors="replace") as f:
        text = []
        for line in f:
            text.append(line)
            if line.strip().startswith("ENDSEC") and len(text) > 5:
                break
        head = "".join(text)

    fields: dict[str, str] = {}
    for match in _HEADER_FIELD_RE.finditer(head):
        fields[match.group(1)] = match.group(2)

    def _strs(blob: str) -> list[str]:
        return [s.strip() for s in re.findall(r"'([^']*)'", blob)]

    schema_strs = _strs(fields.get("SCHEMA", ""))
    name_strs = _strs(fields.get("NAME", ""))
    desc_strs = _strs(fields.get("DESCRIPTION", ""))

    # FILE_NAME positional layout: name, time_stamp, author, organization,
    # preprocessor_version, originating_system, authorisation.
    return StepHeader(
        schema=schema_strs[0] if schema_strs else "(unknown)",
        originating=name_strs[5] if len(name_strs) > 5 else "(unknown)",
        author=name_strs[2] if len(name_strs) > 2 else "",
        timestamp=name_strs[1] if len(name_strs) > 1 else "",
        description=desc_strs[0] if desc_strs else "",
    )


def _stl_triangle_count(stl_path: Path) -> int:
    """Read the 4-byte triangle count from a binary STL header (offset 80)."""
    with stl_path.open("rb") as f:
        f.seek(80)
        return struct.unpack("<I", f.read(4))[0]


def compute_metrics(path: Path) -> dict:
    """Compute the full metric set for one STEP file.

    All floats are rounded here — reports must be byte-stable across runs so
    the CI auto-commit only fires on real geometry changes.
    """
    from build123d import CenterOf, Compound, import_step
    from build123d.exporters3d import export_stl

    header = parse_header(path)
    shape: Compound = import_step(str(path))

    bbox = shape.bounding_box()
    volume_mm3 = float(shape.volume)
    area_mm2 = float(shape.area)
    try:
        com = shape.center(CenterOf.MASS)
    except Exception:  # some compounds only support geometric center
        com = shape.center(CenterOf.BOUNDING_BOX)

    with tempfile.NamedTemporaryFile(suffix=".stl", delete=False) as tmp:
        tmp_path = Path(tmp.name)
    try:
        export_stl(
            to_export=shape,
            file_path=str(tmp_path),
            tolerance=LINEAR_DEFLECTION,
            angular_tolerance=ANGULAR_DEFLECTION,
            ascii_format=False,
        )
        tri_count = _stl_triangle_count(tmp_path)
    finally:
        tmp_path.unlink(missing_ok=True)

    return {
        "file": path.name,
        "schema": header.schema,
        "originating": header.originating,
        "timestamp": header.timestamp,
        "solids": sum(1 for _ in shape.solids()),
        "children": len(list(shape.children or [])),
        "bbox_min": [round(v, 3) for v in (bbox.min.X, bbox.min.Y, bbox.min.Z)],
        "bbox_max": [round(v, 3) for v in (bbox.max.X, bbox.max.Y, bbox.max.Z)],
        "bbox_size": [round(v, 3) for v in (bbox.size.X, bbox.size.Y, bbox.size.Z)],
        "volume_mm3": round(volume_mm3, 1),
        "area_mm2": round(area_mm2, 1),
        "center_of_mass": [round(v, 3) for v in (com.X, com.Y, com.Z)],
        "pla_mass_g": round(volume_mm3 / 1000.0 * PLA_DENSITY_G_CM3, 1),
        "triangles": tri_count,
    }


def format_report(metrics: dict, sidecar: dict | None) -> str:
    """Human-readable report text for one STEP file."""
    sx, sy, sz = metrics["bbox_size"]
    mn, mx = metrics["bbox_min"], metrics["bbox_max"]
    cx, cy, cz = metrics["center_of_mass"]
    lines = [
        f"File:           {metrics['file']}",
        f"Schema:         {metrics['schema']}",
        f"Originating:    {metrics['originating']}",
        f"Timestamp:      {metrics['timestamp']}",
        f"Solids:         {metrics['solids']}   Top-level children: {metrics['children']}",
        f"Bounding box:   X[{mn[0]}, {mx[0]}]  Y[{mn[1]}, {mx[1]}]  Z[{mn[2]}, {mx[2]}]",
        f"Size:           {sx} × {sy} × {sz} mm",
        f"Volume:         {metrics['volume_mm3']:,} mm³"
        f"   (≈{metrics['pla_mass_g']} g PLA @ 100% infill)",
        f"Surface area:   {metrics['area_mm2']:,} mm²",
        f"Center of mass: ({cx}, {cy}, {cz})",
        f"Tessellation:   {metrics['triangles']:,} triangles"
        f"  (linear={LINEAR_DEFLECTION} mm  angular={ANGULAR_DEFLECTION} rad)",
    ]
    if sidecar:
        lines.append("")
        lines.append("Design metadata:")
        for table, values in sidecar.items():
            if isinstance(values, dict):
                for k, v in values.items():
                    lines.append(f"  {table}.{k}: {v}")
            else:
                lines.append(f"  {table}: {values}")
    return "\n".join(lines)


def format_toml(metrics: dict, sidecar: dict | None) -> str:
    """Machine-readable snapshot (hand-rolled TOML writer, stdlib-only)."""

    def _val(v) -> str:
        if isinstance(v, bool):
            return "true" if v else "false"
        if isinstance(v, (int, float)):
            return repr(v)
        if isinstance(v, list):
            return "[" + ", ".join(_val(x) for x in v) + "]"
        return '"' + str(v).replace("\\", "\\\\").replace('"', '\\"') + '"'

    out = ["[metrics]"]
    for k, v in metrics.items():
        out.append(f"{k} = {_val(v)}")
    if sidecar:
        for table, values in sidecar.items():
            if isinstance(values, dict):
                out.append("")
                out.append(f"[{table}]")
                for k, v in values.items():
                    out.append(f"{k} = {_val(v)}")
    return "\n".join(out) + "\n"


def load_sidecar(step_path: Path) -> dict | None:
    """Look for `<stem>.meta.toml` next to the STEP and parse it."""
    sidecar = step_path.parent / f"{step_path.stem}.meta.toml"
    if not sidecar.exists():
        return None
    return tomllib.loads(sidecar.read_text())
