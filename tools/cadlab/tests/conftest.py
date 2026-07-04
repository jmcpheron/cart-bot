import pytest


@pytest.fixture(scope="session")
def step_pair(tmp_path_factory):
    """Two known-geometry STEP revisions, mirroring the chassis iteration:
    rev-b is narrower in Y and has a bore removed (like the bigger hub hole).

    rev-a: 40 × 20 × 10 box            volume 8000 mm³, area 2800 mm²
    rev-b: 40 × 16 × 10 box − r6 bore  volume 6400 − π·36·10 ≈ 5269.0 mm³
    """
    from build123d import Box, Cylinder
    from build123d.exporters3d import export_step

    d = tmp_path_factory.mktemp("steps")
    a = d / "rev-a.step"
    b = d / "rev-b.step"
    export_step(Box(40, 20, 10), str(a))
    export_step(Box(40, 16, 10) - Cylinder(radius=6, height=40), str(b))
    return a, b
