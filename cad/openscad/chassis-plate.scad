// Optional printed chassis plate — use plywood if you have an offcut.
// 190mm square, 4mm PETG, with a 20mm M3 hole grid so brackets, L298Ns, the
// ESP32, and cable clips can mount anywhere, plus two strap slots for the LiPo.
//
// Print: PETG, 0.28mm layers, 4 perimeters, 30% grid infill. Needs a ~200mm
// bed; rotate 45° on smaller beds or split at the centerline.

include <params.scad>

corner_r   = 12;
strap_w    = 22;    // fits a 20mm velcro strap
strap_l    = 4;
edge_keep  = 12;    // no grid holes within this margin

module rounded_plate() {
    hull()
        for (x = [-1, 1], y = [-1, 1])
            translate([x*(plate_size/2 - corner_r), y*(plate_size/2 - corner_r), 0])
                cylinder(r = corner_r, h = plate_thick);
}

module chassis_plate() {
    difference() {
        rounded_plate();
        // M3 grid
        n = floor((plate_size - 2*edge_keep) / grid_pitch);
        for (i = [0:n], j = [0:n]) {
            x = -n*grid_pitch/2 + i*grid_pitch;
            y = -n*grid_pitch/2 + j*grid_pitch;
            // keep the battery strap zone solid
            if (abs(x) > strap_w/2 + 6 || abs(y) > 45)
                translate([x, y, -1]) cylinder(d = m3_clr, h = plate_thick + 2);
        }
        // LiPo strap slots, battery sits centered
        for (y = [-35, 35])
            translate([0, y, plate_thick/2])
                cube([strap_w, strap_l, plate_thick + 2], center = true);
    }
}

chassis_plate();
