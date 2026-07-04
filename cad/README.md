# Printed Parts

OpenSCAD sources in `openscad/`, generated STLs in `stl/`. Externally designed
parts (Onshape etc.) also land in `stl/` — note their source link here.

## Parts

| Part | Qty | Material | Settings | Notes |
|---|---|---|---|---|
| Jason's chassis | 1 | — | — | **The chassis actually in use** (vertical motor mounts, wheels outboard). Own design, not yet committed — drop STL/source in `stl/` to version it |
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
