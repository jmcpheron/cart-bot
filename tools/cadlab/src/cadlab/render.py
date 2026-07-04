"""Render a STEP/STL to PNG via OpenSCAD (ported from pycon2026 cardlab).

No bundled 3D renderer — OpenSCAD (+xvfb in CI) already does this well: we
tessellate to STL, emit a one-line .scad shim that import()s it centered at
the origin, and shell out to ``openscad`` with a preset camera.
"""

from __future__ import annotations

import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

from cadlab.build import build_stl


class OpenSCADNotFound(RuntimeError):
    pass


# OpenSCAD's 7-arg --camera form is tx,ty,tz,rotx,roty,rotz,dist.
_PRESET_ROTATIONS: dict[str, tuple[float, float, float]] = {
    "iso":   (58, 0, 28),
    "top":   (0, 0, 0),
    "front": (90, 0, 0),
    "right": (90, 0, 90),
}
PRESETS = _PRESET_ROTATIONS


def render(input_path: Path, out: Path, angle: str = "iso",
           size: str = "1200x800") -> Path:
    """STEP (or STL) → PNG."""
    if input_path.suffix.lower() == ".stl":
        return render_stl(input_path, out, angle=angle, size=size)
    with tempfile.TemporaryDirectory(prefix="cadlab-render-") as tmpdir:
        stl = Path(tmpdir) / f"{input_path.stem}.stl"
        build_stl(input_path, stl)
        return render_stl(stl, out, angle=angle, size=size)


def render_stl(stl_path: Path, out: Path, angle: str = "iso",
               size: str = "1200x800") -> Path:
    if shutil.which("openscad") is None:
        raise OpenSCADNotFound(
            "openscad not found on PATH — install from https://openscad.org/")
    if angle not in _PRESET_ROTATIONS:
        raise ValueError(f"unknown angle {angle!r}; one of {sorted(_PRESET_ROTATIONS)}")
    width, _, height = size.partition("x")
    if not (width.isdigit() and height.isdigit()):
        raise ValueError(f"size must be WIDTHxHEIGHT, got {size!r}")

    out.parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="cadlab-render-stl-") as tmpdir:
        tmp = Path(tmpdir)
        cx, cy, cz, diag = _stl_bbox(stl_path)
        distance = max(diag * 1.8, 50.0)

        # OpenSCAD's import() resolves relative to the .scad file's dir.
        local_stl = tmp / stl_path.name
        local_stl.write_bytes(stl_path.read_bytes())
        shim = tmp / "render.scad"
        name = local_stl.name.replace("\\", "\\\\").replace('"', '\\"')
        shim.write_text(
            f"translate([{-cx:.6f}, {-cy:.6f}, {-cz:.6f}])\n"
            f'  import("{name}");\n'
        )

        rx, ry, rz = _PRESET_ROTATIONS[angle]
        subprocess.run(
            ["openscad", "-o", str(out),
             "--imgsize", f"{width},{height}",
             "--camera", f"0,0,0,{rx},{ry},{rz},{distance:.3f}",
             "--projection", "ortho",
             "--colorscheme", "Cornfield",
             str(shim)],
            check=True,
        )
    return out


def _stl_bbox(stl_path: Path) -> tuple[float, float, float, float]:
    """Return (cx, cy, cz, diag) from a binary STL. Streaming, no full load."""
    with stl_path.open("rb") as f:
        f.seek(80)
        n_tri = struct.unpack("<I", f.read(4))[0]
        mins = [float("inf")] * 3
        maxs = [float("-inf")] * 3
        for _ in range(n_tri):
            f.read(12)  # skip normal
            for _v in range(3):
                xyz = struct.unpack("<fff", f.read(12))
                for i, v in enumerate(xyz):
                    if v < mins[i]:
                        mins[i] = v
                    if v > maxs[i]:
                        maxs[i] = v
            f.read(2)  # attr byte count
    cx, cy, cz = ((mins[i] + maxs[i]) / 2 for i in range(3))
    sx, sy, sz = (maxs[i] - mins[i] for i in range(3))
    return cx, cy, cz, (sx * sx + sy * sy + sz * sz) ** 0.5
