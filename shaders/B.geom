layout(triangle_strip, max_vertices = 6) out;
out vec2 p;
void main() {
	/* 1------3
	   | \    |
	   |   \  |
	   |     \|
	   0------2 */
	const vec2 p0 = vec2(-1., -1.);
	const vec2 p1 = vec2(-1., 1.);
	gl_Position = vec4(p0, 0., 1.);
	p = p0 * .5 + .5;
	EmitVertex(); // 0
	gl_Position = vec4(p1, 0., 1.);
	p = p1 * .5 + .5;
	EmitVertex(); // 1
	gl_Position = vec4(-p1, 0., 1.);
	p = -p1 * .5 + .5;
	EmitVertex(); // 2
	EndPrimitive();

	gl_Position = vec4(-p1, 0., 1.);
	p = -p1 * .5 + .5;
	EmitVertex(); // 2
	gl_Position = vec4(p1, 0., 1.);
	p = p1 * .5 + .5;
	EmitVertex(); // 1
	gl_Position = vec4(-p0, 0., 1.);
	p = -p0 * .5 + .5;
	EmitVertex(); // 3
	EndPrimitive();
}
