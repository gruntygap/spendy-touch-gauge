// Parameters
// NOTE: Odd sizing has been made to print nicely with ABS on Bambu Labs P1S
bezel_diameter = 61; 
// Allegedly the AEM 30-0300 is 61mm diameter on bezel
// From AEM Website: slim 0.825" gauge depth(Gauge cup depth only .200" deep)
// Assessing the 0.825" - 0.2" = 0.625" == 15.875mm
bezel_height = 10;
fillet_radius = 1;
// The portion that friction fits into the gauge pod
friction_base_diameter  = 52.25;
friction_base_height    = 15;
wall_thickness = 2;
// Circular Screen (Waveshare 1.85")
screen_d = 55;
screen_d_allowance = 0.5;
screen_pcb_d = 49;
screen_to_mounting_post_base=5.1;
screen_assembly_thickness=9.1;

screen_plate_thickness=2;
screen_plate_recession = 0.5;
screen_plate_screw_diameter = 2.1; // meant to be 2, but you know how it is
top_screw_y_pos=21;
bottom_screw_y_pos=14.85;
bottom_screw_x_pos=29.70/2;


//unused for now
mounting_plate_depth = 9.1;

$fn = 170;

module bezel() {
    translate([0,0,fillet_radius]) // Move the object down, due to added sphere size
    minkowski() {
        cylinder(h = bezel_height - (2*fillet_radius), d = bezel_diameter-(fillet_radius*2));
        sphere(r = fillet_radius, $fn = 50);
    }
}

module friction_base() {
    translate([0, 0, bezel_height])
        cylinder(h = friction_base_height, d = friction_base_diameter);
}

module screen_mount() {
    // Move mount down into gauge
    translate([0,0,screen_assembly_thickness+screen_plate_recession])
    difference() {
        // The main cylinder / plate
        cylinder(d=friction_base_diameter-wall_thickness, h=screen_plate_thickness);
        // Remove screw-holes, etc
        // TOP SCREW
        translate([0,top_screw_y_pos,0])
        cylinder(d=screen_plate_screw_diameter, h=100, $fn = 30);
        // RIGHT SCREW
        translate([bottom_screw_x_pos,-bottom_screw_y_pos,0])
        cylinder(d=screen_plate_screw_diameter, h=100, $fn = 30);
        // LEFT SCREW
        translate([-bottom_screw_x_pos,-bottom_screw_y_pos,0])
        cylinder(d=screen_plate_screw_diameter, h=100, $fn = 30);
        // RIGHT BIG CONNECTOR
        translate([17,0,0])
        cube([15,25,100], center=true);
        // LEFT RIBBON CONNECTOR
        translate([-17,0,0])
        cube([15,15,100], center=true);
    }
}

module relief_cut() {
    intersection(){
        cylinder(d=screen_d+screen_d_allowance,h=8,$fn=60);
        translate([22,0,0])
        cube([15,25,30],center=true);
    }
}

// Screen for mocking up in renderer
module fake_screen() {
    translate([0,0,screen_plate_recession])
    cylinder(d=screen_d, h=1);
}

module gauge() {    
    difference() {
        union() {          
            bezel();
            friction_base();
        }
        translate([0, 0, wall_thickness])
        // Hollowing
        cylinder(
            h = friction_base_height+bezel_height,
            d = friction_base_diameter - 2 * wall_thickness,
            $fn = 60
        );
        // Screen negative
        cylinder(h=screen_to_mounting_post_base, d=screen_d+screen_d_allowance, $fn = 60);
        // Screen pcb/mounting negative 
        cylinder(h=screen_assembly_thickness+screen_plate_recession, d=screen_pcb_d+screen_d_allowance, $fn = 60);
        // RIGHT WIRE BEND RELIEF
//        translate([21.75,-10,4.3])
//        rotate([0,-10,0])
//        cube([4,20,5]);
        relief_cut();
    }
}

gauge();

screen_mount();

//translate([20,0,8])
//rotate([0,45,0])
//cube([5,25,5],center=true);



//fake_screen();




