// TT motor bracket — U-channel cradle, 4 needed (all identical).
//
// The motor drops into the channel; two M3×30 screws pass through both walls
// and the motor's gearbox through-holes. Wall holes are SLOTS (±2mm along the
// motor axis) to absorb clone-to-clone hole-position variance. The base bolts
// to the chassis plate with two M3 screws on the 20mm grid.
//
// Print: PLA, 0.2mm layers, 3 perimeters, 25% infill, no supports
// (slots print as bridges — they're short, any printer handles it).

include <params.scad>

channel_w  = tt_body_w + 2 * fit_clr;       // motor slides in
bracket_len = 30;                            // along the motor axis
wall_h     = tt_hole_z + 6;                  // wall covers holes + margin
base_thick = 3;
base_ear   = 8;                              // ear width outside each wall
slot_len   = 4;                              // hole slop along motor axis

module wall_slot() {
    // M3 through-slot at motor-hole height, elongated along motor axis (Y)
    hull()
        for (y = [-slot_len/2, slot_len/2])
            translate([0, y, base_thick + tt_hole_z])
                rotate([0, 90, 0])
                    cylinder(d = m3_clr, h = 100, center = true);
}

module tt_motor_bracket() {
    difference() {
        union() {
            // base with mounting ears
            translate([-(channel_w/2 + wall + base_ear), -bracket_len/2, 0])
                cube([channel_w + 2*(wall + base_ear), bracket_len, base_thick]);
            // two walls
            for (s = [-1, 1])
                translate([s*(channel_w/2 + wall/2) - wall/2, -bracket_len/2, 0])
                    cube([wall, bracket_len, base_thick + wall_h]);
        }
        // motor through-screw slots (both walls at once)
        for (y = [-tt_hole_space/2, tt_hole_space/2])
            translate([0, y, 0]) wall_slot();
        // chassis mounting holes, one per ear, on-axis
        for (s = [-1, 1])
            translate([s*(channel_w/2 + wall + base_ear/2), 0, -1])
                cylinder(d = m3_clr, h = base_thick + 2);
    }
}

tt_motor_bracket();
