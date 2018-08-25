layout(triangle_strip, max_vertices=12) out;

out float width;
out float intensity;
out float min_intensity;
out vec3 uvl;

void quad(vec2 P0, vec2 P1, float thickness) {
	/*
		 1------3
		 | \    |
		 |   \  |
		 |     \|
		 0------2
	*/
	vec2 dir = P1-P0;
	float dl = length(dir);
	// If the segment is too short, just draw a square
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
	float t0 = (iGeomIter+0)/iNumGeomIters;
	float t1 = (iGeomIter+1)/iNumGeomIters;

	width = .006;
	intensity = .15;
	min_intensity = 0.0;

	float sr0 = texture(iSoundR, t0).r * .5 - .5;
	float sr1 = texture(iSoundR, t1).r * .5 - .5;
	float sl0 = texture(iSoundL, t0).r * .5 + .5;
	float sl1 = texture(iSoundL, t1).r * .5 + .5;
        t0 = t0 * 2. - 1.;
        t1 = t1 * 2. - 1.;
	vec2 P0 = vec2(t0, sr0);
	vec2 P1 = vec2(t1, sr1);
	quad(P0, P1, width);
	vec2 P2 = vec2(t0, sl0);
	vec2 P3 = vec2(t1, sl1);
	quad(P2, P3, width);
}
