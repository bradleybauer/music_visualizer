#version 330
precision highp float;

uniform int maxOutputVertices;
uniform int numPoints;
in int pointIndex[];

// sound amplitude in red component
uniform sampler1D SL; // left channel
uniform sampler1D SR; // right channel
// frequency
uniform sampler1D FL; // left channel
uniform sampler1D FR; // right channel
uniform vec2 R; // resolution

#define EPS 1E-6
#define param .02
// uniform float param;
out vec4 uvl;

layout(points) in;
layout(triangle_strip) out;
layout(max_vertices=6) out;

void main() {
	int quad_id = pointIndex[0];
	float t0 = (quad_id+0)/float(numPoints);
	float t1 = (quad_id+1)/float(numPoints);
	// vec2 P0 = vec2(texture(SL, t0).r, texture(SR, t0).r)*2.-1.;
	// vec2 P1 = vec2(texture(SL, t1).r, texture(SR, t1).r)*2.-1.;
	// P0*=1.3;
	// P1*=1.3;
	// vec2 P0 = vec2(t0, texture(SL, t0).r)*2.-1.;
	// vec2 P1 = vec2(t1, texture(SL, t1).r)*2.-1.;
	// P0.y*=1.3;
	// P1.y*=1.3;
	vec2 P0 = vec2(t0, texture(FL, t0/2.).r)*2.-1.;
	vec2 P1 = vec2(t1, texture(FL, t1/2.).r)*2.-1.;

	// TODO interpolation to make curves nice a smooth?

	// P0.x*=R.y/R.x;
	// P1.x*=R.y/R.x;
	vec2 dir = P1-P0;
	float dl = length(dir);
	// If the segment is too short, just draw a square
    if (dl < EPS)
        dir = vec2(1.0, 0.0);
    else 
		dir = normalize(dir);
    vec2 norm = vec2(-dir.y, dir.x);

	/*
	   1------3
	   | \    |
	   |   \  |
	   |     \|
	   0------2
	*/

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
