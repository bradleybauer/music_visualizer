#version 330
precision highp float;

uniform float num_points;

// sound amplitude in red component
uniform sampler1D SL;					// left channel
uniform sampler1D SR;					// right channel

const float EPS = 1E-6;
out float width;
out float intensity;
out float min_intensity;
out vec3 uvl;

layout(points) in;
layout(triangle_strip) out;
layout(max_vertices=24) out;

/*
   1------3
   | \    |
   |   \  |
   |     \|
   0------2
*/
void quad(vec2 P0, vec2 P1, float thickness) {
	vec2 dir = P1-P0;
	float dl = length(dir);
	// If the segment is too short, just draw a square
	if (dl < EPS)
		dir = vec2(1.0, 0.0);
	else
	 	dir = normalize(dir);
	vec2 norm = vec2(-dir.y, dir.x);

	uvl = vec3(dl+thickness, -thickness, dl);
	gl_Position = vec4(P0+(-dir-norm)*thickness, 0., 1.);
	EmitVertex(); // 0

	uvl = vec3(dl+thickness, thickness, dl);
	gl_Position = vec4(P0+(-dir+norm)*thickness, 0., 1.);
	EmitVertex(); // 1

	uvl = vec3(-thickness, -thickness, dl);
	gl_Position = vec4(P1+(dir-norm)*thickness, 0., 1.);
	EmitVertex(); // 2
	EndPrimitive();

	uvl = vec3(-thickness, -thickness, dl);
	gl_Position = vec4(P1+(dir-norm)*thickness, 0., 1.);
	EmitVertex(); // 2

	uvl = vec3(dl+thickness, thickness, dl);
	gl_Position = vec4(P0+(-dir+norm)*thickness, 0., 1.);
	EmitVertex(); // 1

	uvl = vec3(-thickness, thickness, dl);
	gl_Position = vec4(P1+(dir+norm)*thickness, 0., 1.);
	EmitVertex(); // 3
	EndPrimitive();
}

void main() {
    int quad_id = gl_PrimitiveIDIn;
    float t0 = (quad_id+0)/(num_points);
    float t1 = (quad_id+1)/(num_points);

    width = .015;
    intensity = .1;
    min_intensity = .01;

    float sr0 = texture(SR, t0).r;
    float sr1 = texture(SR, t1).r;
    vec2 P0 = vec2(t0*2.-1., sr0);
    vec2 P1 = vec2(t1*2.-1., sr1);
    quad(P0, P1, width);
}
