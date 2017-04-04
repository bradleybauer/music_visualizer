#version 330
precision highp float;

uniform int max_output_vertices;
uniform int num_points;
in int pointIndex[];

// sound amplitude in red component
uniform sampler1D SL; // left channel
uniform sampler1D SR; // right channel
// frequency
uniform sampler1D FL; // left channel
uniform sampler1D FR; // right channel
uniform vec2 R; // resolution
uniform float T; // time

// TODO this is where the error was coming from
const float EPS = 1E-6;
const float param = .005;
// uniform float param;
out vec4 uvl;

layout(points) in;
layout(triangle_strip) out;
layout(max_vertices=6) out;

// TODO multiple input methods
// keyboard
// mouse
// is key down
// is mouse down
// direction keys push a point around a grid
// mouse down&drag push a point around the plane
// direction point history
// mouse drag point history
// key press history
// mouse down history
// mouse history
// ....

/*
   1------3
   | \    |
   |   \  |
   |     \|
   0------2
*/
void quad(vec2 P0, vec2 P1, float t0, float t1) {
	vec2 dir = P1-P0;
	float dl = length(dir);
	// If the segment is too short, just draw a square
    if (dl < EPS)
        dir = vec2(1.0, 0.0);
    else 
		dir = normalize(dir);
    vec2 norm = vec2(-dir.y, dir.x);

	uvl = vec4(dl+param, -param, dl, t0);
	gl_Position = vec4(P0+(-dir-norm)*param, 0., 1.);
	EmitVertex(); // 0

	uvl = vec4(dl+param, param, dl, t0);
	gl_Position = vec4(P0+(-dir+norm)*param, 0., 1.);
	EmitVertex(); // 1

	uvl = vec4(-param, -param, dl, t0);
	gl_Position = vec4(P1+(dir-norm)*param, 0., 1.);
	EmitVertex(); // 2
	EndPrimitive();

	uvl = vec4(-param, -param, dl, t0);
	gl_Position = vec4(P1+(dir-norm)*param, 0., 1.);
	EmitVertex(); // 2

	uvl = vec4(dl+param, param, dl, t0);
	gl_Position = vec4(P0+(-dir+norm)*param, 0., 1.);
	EmitVertex(); // 1

	uvl = vec4(-param, param, dl, t0);
	gl_Position = vec4(P1+(dir+norm)*param, 0., 1.);
	EmitVertex(); // 3
	EndPrimitive();
}

// TODO interpolation to make curves nice a smooth?
void main() {
	int quad_id = pointIndex[0];
	float t0 = (quad_id+0)/float(num_points);
	float t1 = (quad_id+1)/float(num_points);

	// float stretch = 4.;
	// t0 = pow(stretch, t0)-stretch+1.;
	// t1 = pow(stretch, t1)-stretch+1.;

	// vec2 P0 = vec2(texture(SL, t0).r, texture(SR, t0).r);
	// vec2 P1 = vec2(texture(SL, t1).r, texture(SR, t1).r);

	// t0/=2.;
	// t1/=2.;
	// vec2 P0 = log(20.*vec2(texture(FL, t0).r, texture(FR, t0).r))/12.;
	// vec2 P1 = log(20.*vec2(texture(FL, t1).r, texture(FR, t1).r))/12.;
	// P0-=.1;
	// P1-=.1;

	vec2 P0 = vec2(t0*2.-1., texture(SL, t0).r);
	vec2 P1 = vec2(t1*2.-1., texture(SL, t1).r);

	// vec2 P0 = vec2(t0*2.-1., texture(FR, t0).r);
	// vec2 P1 = vec2(t1*2.-1., texture(FR, t1).r);

	quad(P0, P1, t0, t1);

	P0 = vec2(t0*2.-1., texture(FR, t0/2.).r-1.);
	P1 = vec2(t1*2.-1., texture(FR, t1/2.).r-1.);

	quad(P0, P1, t0, t1);
}
