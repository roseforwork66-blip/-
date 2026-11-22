// Conceptual assembly layout. Dimensions are in millimetres.
// Set view_mode to "top" for a projected layout. Measure purchased parts before fabrication.
$fn = 64;
view_mode = "assembly";  // [assembly,top]
wheel_diameter = 150;
wheel_width = 30;
wheel_track = 250;
wheel_angle = 25;        // degrees, left positive
scan_height = 225;
lidar_forward = 120;     // longitudinal distance from steering pivot

axle_z = wheel_diameter / 2;
pivot_z = axle_z + 50;
bridge_width = wheel_track - wheel_width - 10;

module wheel_frame() {
    rotate([0, 0, wheel_angle]) {
        color("dimgray")
            for (side = [-1, 1]) {
                translate([0, side * wheel_track / 2, axle_z])
                    rotate([90, 0, 0]) cylinder(d=wheel_diameter, h=wheel_width, center=true);
            }
        color("steelblue") {
            translate([0, 0, axle_z + 30]) cube([40, bridge_width, 8], center=true);
            for (side = [-1, 1]) {
                translate([0, side * (bridge_width / 2 - 4), axle_z + 13])
                    cube([40, 8, 34], center=true);
            }
        }
    }
}

module assembly() {
    wheel_frame();
    color("silver") translate([0, 0, axle_z + 30]) cylinder(d=16, h=53);
    color("dodgerblue") translate([65, 0, pivot_z + 30]) cube([168, 93, 8], center=true);
    color("goldenrod") translate([lidar_forward, 0, scan_height - 31]) cylinder(d=78, h=31);
    color("gray") translate([-10, 0, pivot_z + 55]) rotate([0, -25, 0]) cylinder(d=26, h=745);
    if (view_mode == "assembly") {
        color([0.9, 0.2, 0.1, 0.25]) translate([lidar_forward, 0, scan_height]) cylinder(d=180, h=1);
    }
}

if (view_mode == "top") {
    projection(cut=false) assembly();
} else {
    assembly();
}
