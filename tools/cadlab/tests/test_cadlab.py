import math
import tomllib

from cadlab.diff import diff_report
from cadlab.inspect import compute_metrics, format_report, format_toml, load_sidecar


def test_inspect_box_metrics(step_pair):
    a, _ = step_pair
    m = compute_metrics(a)
    assert m["bbox_size"] == [40.0, 20.0, 10.0]
    assert abs(m["volume_mm3"] - 8000.0) < 0.5
    assert abs(m["area_mm2"] - 2800.0) < 0.5
    assert m["solids"] == 1
    assert m["pla_mass_g"] == 9.9  # 8 cm³ × 1.24 g/cm³
    assert m["triangles"] >= 12    # a tessellated box is at least 12 tris
    assert m["schema"] != "(unknown)"
    assert m["center_of_mass"] == [0.0, 0.0, 0.0]


def test_bore_removes_expected_volume(step_pair):
    _, b = step_pair
    m = compute_metrics(b)
    expected = 40 * 16 * 10 - math.pi * 6 * 6 * 10
    assert abs(m["volume_mm3"] - expected) < 1.0
    assert m["bbox_size"][1] == 16.0


def test_diff_report_direction_and_summary(step_pair):
    a, b = step_pair
    report = diff_report(a, b)
    assert "| Metric | A | B |" in report
    assert "Y shrank by 4.00 mm" in report
    assert "volume removed" in report
    # X and Z unchanged -> no note about them
    assert "X shrank" not in report and "X grew" not in report


def test_toml_snapshot_roundtrip(step_pair):
    a, _ = step_pair
    m = compute_metrics(a)
    parsed = tomllib.loads(format_toml(m, None))
    assert parsed["metrics"]["volume_mm3"] == m["volume_mm3"]
    assert parsed["metrics"]["bbox_size"] == m["bbox_size"]


def test_sidecar_merges_into_report(step_pair):
    a, _ = step_pair
    sidecar_path = a.parent / f"{a.stem}.meta.toml"
    sidecar_path.write_text(
        '[design]\nrevision = 1\nnotes = "test revision"\n')
    try:
        sidecar = load_sidecar(a)
        assert sidecar == {"design": {"revision": 1, "notes": "test revision"}}
        report = format_report(compute_metrics(a), sidecar)
        assert "design.notes: test revision" in report
    finally:
        sidecar_path.unlink()
