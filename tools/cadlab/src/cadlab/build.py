"""STEP → STL tessellation via build123d (OCCT).

Trimmed from pycon2026 cardlab/build.py — cart-bot only commits STL (GitHub
renders it natively in the browser), no GLB path needed.
"""

from __future__ import annotations

from pathlib import Path

from cadlab.inspect import ANGULAR_DEFLECTION, LINEAR_DEFLECTION


def build_stl(input_path: Path, out: Path) -> Path:
    from build123d import Compound, import_step
    from build123d.exporters3d import export_stl

    out.parent.mkdir(parents=True, exist_ok=True)
    shape: Compound = import_step(str(input_path))
    export_stl(
        to_export=shape,
        file_path=str(out),
        tolerance=LINEAR_DEFLECTION,
        angular_tolerance=ANGULAR_DEFLECTION,
        ascii_format=False,
    )
    return out
