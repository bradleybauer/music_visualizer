#version 330
precision highp float;

uniform int maxOutputVertices;
uniform int numPoints;
in int pointIndex[];

layout(points) in;
layout(triangle_strip) out;
layout(max_vertices=6) out;

/* Geometry shader info 
Please see the following links for more information on geometry shaders.
	https://open.gl/geometry
	https://www.khronos.org/opengl/wiki/Geometry_Shader
	http://www.informit.com/articles/article.aspx?p=2120983&seqNum=2

The vertex shader runs once per vertex.
The geometry shader runs once per primitive received from the vertex shader (point, line or triangle) and has access to the vertices of that primitive. 
PROGNAME's vertex shader always outputs point primatives.

out can have these primitive types
	points
	line_strip
	triangle_strip

Definition for gl_in[]
in gl_PerVertex {
    vec4  gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
} gl_in[];

PROGNAME's vertex shader is outputing GL_POINTS primitives and so gl_in[] has length of 1.

// Pass-Through geometry shader body
void main() {
	for (i = 0; i < gl_in.length(); i++) {
		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
	}
}
*/

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

void main() {
	// Output a fullscreen quad
	vec2 point = vec2(.5);
	point = point * 2. - 1.;
	quadAtPoint(point, vec2(2.), 0.);
}
