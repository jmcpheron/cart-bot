# Printed Parts

OpenSCAD sources in `openscad/`, generated STLs in `stl/`. Externally designed
parts (Onshape etc.) also land in `stl/` — note their source link here.

## Parts

| Part | Qty | Material | Settings | Notes |
|---|---|---|---|---|
| `tt-motor-bracket` | 4 | PLA | 0.2mm, 3 walls, 25% | **Print ONE first** — verify against a real DORHEA motor, then adjust `params.scad` (`tt_hole_space` especially) |
| `cable-clip` | 4–6 | PLA | 0.2mm, 100% | Prints on its side, no supports |
| `chassis-plate` | 0–1 | PETG | 0.28mm, 4 walls, 30% | Optional — plywood offcut works too. Needs ~200mm bed |

## Regenerating STLs

Shared dimensions live in `openscad/params.scad`. After editing:

```sh
cd cad
for p in tt-motor-bracket cable-clip chassis-plate; do
  openscad -o stl/$p.stl openscad/$p.scad
done
```

Commit both the `.scad` change and the regenerated `.stl`.
