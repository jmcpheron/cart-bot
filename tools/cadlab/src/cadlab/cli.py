"""cadlab CLI — inspect / diff / build / render / all.

Path convention (repo-relative, run from the cart-bot root):
    cad/step/<name>.step            design revisions (source of record)
    cad/step/<name>.meta.toml       optional hand-written design notes
    cad/reports/<name>.{txt,toml}   generated metric reports (diffable)
    cad/stl/<name>.stl              generated mesh (GitHub renders in 3D)
    cad/img/<name>-iso.png          generated preview
"""

from __future__ import annotations

import sys
from pathlib import Path

import click

from cadlab.inspect import compute_metrics, format_report, format_toml, load_sidecar


@click.group()
def main() -> None:
    """Track 3D design revisions: metrics, meshes, previews, diffs."""


@main.command()
@click.argument("steps", nargs=-1, required=True,
                type=click.Path(exists=True, path_type=Path))
@click.option("--reports-dir", type=click.Path(path_type=Path), default=None,
              help="Also write <stem>.txt/.toml reports here.")
def inspect(steps: tuple[Path, ...], reports_dir: Path | None) -> None:
    """Print (and optionally save) the metrics report for STEP file(s)."""
    for step in steps:
        metrics = compute_metrics(step)
        sidecar = load_sidecar(step)
        report = format_report(metrics, sidecar)
        click.echo(report)
        click.echo()
        if reports_dir is not None:
            reports_dir.mkdir(parents=True, exist_ok=True)
            (reports_dir / f"{step.stem}.txt").write_text(report + "\n")
            (reports_dir / f"{step.stem}.toml").write_text(
                format_toml(metrics, sidecar))


@main.command()
@click.argument("a", type=click.Path(exists=True, path_type=Path))
@click.argument("b", type=click.Path(exists=True, path_type=Path))
def diff(a: Path, b: Path) -> None:
    """Compare two revisions (STEP files or report TOMLs)."""
    from cadlab.diff import diff_report
    click.echo(diff_report(a, b))


@main.command()
@click.option("--in", "input_path", required=True,
              type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out", required=True, type=click.Path(path_type=Path))
def build(input_path: Path, out: Path) -> None:
    """Tessellate a STEP file to binary STL."""
    from cadlab.build import build_stl
    click.echo(f"wrote {build_stl(input_path, out)}")


@main.command()
@click.option("--in", "input_path", required=True,
              type=click.Path(exists=True, path_type=Path))
@click.option("--out", "out", required=True, type=click.Path(path_type=Path))
@click.option("--angle", default="iso")
@click.option("--size", default="1200x800")
def render(input_path: Path, out: Path, angle: str, size: str) -> None:
    """Render a STEP/STL to PNG via OpenSCAD."""
    from cadlab.render import render as render_step
    click.echo(f"wrote {render_step(input_path, out, angle=angle, size=size)}")


@main.command(name="all")
@click.option("--cad-dir", type=click.Path(exists=True, path_type=Path),
              default=Path("cad"), show_default=True,
              help="Repo cad/ directory (expects step/ inside).")
@click.option("--skip-render", is_flag=True,
              help="Skip PNG previews (no OpenSCAD available).")
def run_all(cad_dir: Path, skip_render: bool) -> None:
    """Regenerate reports + STL + PNG for every cad/step/*.step."""
    step_dir = cad_dir / "step"
    steps = sorted(step_dir.glob("*.step")) + sorted(step_dir.glob("*.stp"))
    if not steps:
        click.echo(f"no STEP files under {step_dir}", err=True)
        sys.exit(1)

    reports = cad_dir / "reports"
    reports.mkdir(parents=True, exist_ok=True)
    for step in steps:
        click.echo(f"── {step.name}")
        metrics = compute_metrics(step)
        sidecar = load_sidecar(step)
        (reports / f"{step.stem}.txt").write_text(
            format_report(metrics, sidecar) + "\n")
        (reports / f"{step.stem}.toml").write_text(format_toml(metrics, sidecar))
        click.echo(f"   reports/{step.stem}.txt .toml")

        from cadlab.build import build_stl
        stl = build_stl(step, cad_dir / "stl" / f"{step.stem}.stl")
        click.echo(f"   stl/{stl.name}")

        if not skip_render:
            from cadlab.render import render_stl
            png = render_stl(stl, cad_dir / "img" / f"{step.stem}-iso.png")
            click.echo(f"   img/{png.name}")
    click.echo("done")
