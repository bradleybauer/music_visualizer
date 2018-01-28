layout(triangle_strip, max_vertices=24) out;

out vec2 P;

void main() {
	float n = iGeomIter/iNumGeomIters;

	float width = 0.014;

	n+=width/4.;

        float f = 10.*texture(iFreqL, n).r;
	float s0 = f;

	vec2 p = vec2(n*2.-1., s0);

        float height = .5;
	gl_Position = vec4(p.x        , -1.   , 0., 1.);
	EmitVertex(); // 0

	gl_Position = vec4(p.x        , p.y-1., 0., 1.);
	EmitVertex(); // 1

	gl_Position = vec4(p.x + width, -1.   , 0., 1.);
	EmitVertex(); // 2
	EndPrimitive();

	gl_Position = vec4(p.x + width, -1.   , 0., 1.);
	EmitVertex(); // 2

	gl_Position = vec4(p.x        , p.y-1., 0., 1.);
	EmitVertex(); // 1

	gl_Position = vec4(p.x + width, p.y-1., 0., 1.);
	EmitVertex(); // 3
	EndPrimitive();
}
