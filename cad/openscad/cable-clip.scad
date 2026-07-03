// Cable clip — open C-clip with an M3 base hole. Print a handful; mecanum
// rollers eat any wire loose enough to droop.
// Print: PLA, 0.2mm, 100% infill (tiny part), no supports (prints on its side).

include <params.scad>

bundle_d  = 8;     // wire bundle diameter
clip_wall = 2.4;
clip_w    = 8;     // width along the wire
mouth     = 5;     // opening gap — wires push through, then it holds

module cable_clip() {
    rotate([90, 0, 0])  // lay on its side for printing
    union() {
        difference() {
            cylinder(d = bundle_d + 2*clip_wall, h = clip_w);
            translate([0, 0, -1]) cylinder(d = bundle_d, h = clip_w + 2);
            // mouth opening, facing up
            translate([-mouth/2, 0, -1])
                cube([mouth, bundle_d, clip_w + 2]);
        }
        // foot with M3 hole
        translate([-clip_wall - 5, -(bundle_d/2 + clip_wall) - 2.4, 0])
        difference() {
            cube([bundle_d + 2*clip_wall + 4, 2.4, clip_w]);
            translate([(bundle_d + 2*clip_wall + 4)/2, -1, clip_w/2])
                rotate([-90, 0, 0]) cylinder(d = m3_clr, h = 5);
        }
    }
}

cable_clip();
