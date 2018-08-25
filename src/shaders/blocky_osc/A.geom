layout(triangle_strip, max_vertices=24) out;

out vec2 P;
out float is_background;

void quad(vec2 p, float thickness, float height) {
	/*
		 1------3
		 | \    |
		 |   \  |
		 |     \|
		 0------2
	*/
	height /= 2.; // half height
	if (height < .1) height = .1;

	gl_Position = vec4(p.x, p.y - height, 0., 1.);
	EmitVertex(); // 0

	gl_Position = vec4(p.x, p.y + height, 0., 1.);
	EmitVertex(); // 1

	gl_Position = vec4(p.x + thickness, p.y - height, 0., 1.);
	EmitVertex(); // 2
	EndPrimitive();

	gl_Position = vec4(p.x + thickness, p.y - height, 0., 1.);
	EmitVertex(); // 2

	gl_Position = vec4(p.x, p.y + height, 0., 1.);
	EmitVertex(); // 1

	gl_Position = vec4(p.x + thickness, p.y + height, 0., 1.);
	EmitVertex(); // 3
	EndPrimitive();
}

void main() {
	float n = iGeomIter/iNumGeomIters;

	float width = 0.014;

	n+=width/4.;

	float s0 = texture(iSoundL, n).r;
	float s1 = texture(iSoundL, n+1./iNumGeomIters).r;

	vec2 p = vec2(n*2.-1., s0);

        is_background = 1.;
        if (iGeomIter < 1.)
            quad(vec2(-1, 0), 2., 2.);

        is_background = 0.;
	float height = abs(s0-s1);
	quad(p, width, height);
}
