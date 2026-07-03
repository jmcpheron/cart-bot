// Shared dimensions for the kitchen tester printed parts.
// ⚠ TT motor clones vary by ±0.5mm — measure YOUR DORHEA motors and print ONE
// bracket to check fit before running off all four.

// ---- TT motor (yellow gearbox type) ----
tt_body_len    = 65.0;   // gearbox + can, excluding shaft
tt_body_w      = 22.5;   // across the flats
tt_body_h      = 18.5;
tt_hole_d      = 3.2;    // M3 clearance
tt_hole_space  = 17.6;   // between the two gearbox through-holes — MEASURE THIS
tt_hole_z      = tt_body_h / 2;  // holes run through mid-height of the box

// ---- printing ----
fit_clr   = 0.25;  // per-side clearance for press/slide fits
wall      = 3.0;
m3_clr    = 3.4;   // M3 clearance hole
$fn       = 48;

// ---- chassis ----
plate_size  = 190;   // square, mm
plate_thick = 4;
grid_pitch  = 20;    // M3 mounting-hole grid
