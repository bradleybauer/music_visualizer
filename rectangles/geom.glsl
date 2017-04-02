#version 330
precision highp float;

uniform int maxOutputVertices;
uniform int numPoints;
in int pointIndex[];

layout(points) in;
layout(triangle_strip) out;
layout(max_vertices=6) out;

void quadAtPoint(vec2 p, vec2 size, float th) {
	vec4 P = vec4(p,0.,1.);
	float s = sin(th); float c = cos(th); mat2 rot = mat2(c, s, -s, c);
	vec2 q1 = size;
	vec2 q2 = vec2(size.x, -size.y);
	q1*=rot;
	q2*=rot;
    gl_Position = P - .5 * vec4(q1, 0., 0.);
    EmitVertex();
    gl_Position = P + .5 * vec4(q2, 0., 0.);
    EmitVertex();
    gl_Position = P + .5 * vec4(q1, 0., 0.);
    EmitVertex();
    EndPrimitive();

    gl_Position = P - .5 * vec4(q1, 0., 0.);
    EmitVertex();
    gl_Position = P + .5 * vec4(q1, 0., 0.);
    EmitVertex();
    gl_Position = P - .5 * vec4(q2, 0., 0.);
    EmitVertex();
    EndPrimitive();
}

uniform sampler1D SL;
uniform sampler1D SR;
uniform sampler1D FL;
uniform sampler1D FR;

void main() {
	vec2 point = vec2(.5);
	point = point * 2. - 1.;
	quadAtPoint(point, vec2(.3), texture(FR,0./1024.).r*3.1415);
}
