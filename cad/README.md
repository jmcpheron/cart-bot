# Printed Parts

OpenSCAD sources in `openscad/`, design revisions as STEP in `step/`,
generated STLs in `stl/`, metric reports in `reports/`, previews in `img/`.

## Tracking design revisions

Chassis revisions are exported from Onshape as numbered STEP files
(`step/tt-mount-chassis-<n>.step`) with an optional `<name>.meta.toml` sidecar
holding design notes. The `tools/cadlab` CLI (ported from
[jmcpheron/pycon2026](https://github.com/jmcpheron/pycon2026)) turns each into
a diffable snapshot; CI (`.github/workflows/cad.yml`) regenerates everything
on push and auto-commits, so **the git diff of `reports/*.txt` is the design
change record**.

```sh
# from the repo root
uv run --project tools/cadlab pytest tools/cadlab/tests   # self-test
uv run --project tools/cadlab cadlab all                  # regen reports/stl/png
uv run --project tools/cadlab cadlab diff \
    cad/step/tt-mount-chassis-1.step cad/step/tt-mount-chassis-2.step
```

`diff` prints a markdown delta table (bbox, volume, surface area, PLA mass,
solids, triangles) between any two revisions — STEP files or report TOMLs.

## Parts

| Part | Qty | Material | Settings | Notes |
|---|---|---|---|---|
| Jason's chassis | 1 | — | — | **The chassis actually in use** (vertical motor mounts, wheels outboard). Onshape design, tracked as `step/tt-mount-chassis-<n>.step` revisions |
| `cable-clip` | 4–6 | PLA | 0.2mm, 100% | Prints on its side, no supports |
| `tt-motor-bracket` | 0–4 | PLA | 0.2mm, 3 walls, 25% | **Unused** — superseded by Jason's chassis. Kept for the alternative flat-plate build |
| `chassis-plate` | 0–1 | PETG | 0.28mm, 4 walls, 30% | **Unused** — alternative flat-plate chassis, needs ~200mm bed |

## Regenerating STLs

Shared dimensions live in `openscad/params.scad`. After editing:

```sh
cd cad
for p in tt-motor-bracket cable-clip chassis-plate; do
  openscad -o stl/$p.stl openscad/$p.scad
done
```

Commit both the `.scad` change and the regenerated `.stl`.
