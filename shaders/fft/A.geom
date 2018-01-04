layout(triangle_strip, max_vertices=24) out;

const float EPS = 1E-6;
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
	float t0 = (iGeomIter+0)/iNumGeomIters;
	float t1 = (iGeomIter+1)/iNumGeomIters;

	width = .015;
	intensity = .1;
	min_intensity = .01;

	const float stretch = 200.;
	float fl0 = 3.*texture(iFreqL, pow(stretch, t0-1.)-(1.-t0)/stretch).r;
	float fl1 = 3.*texture(iFreqL, pow(stretch, t1-1.)-(1.-t1)/stretch).r;
	// float fr0 = texture(iFreqR, pow(stretch, t0-1.)-(1.-t0)/stretch).r;
	// float fr1 = texture(iFreqR, pow(stretch, t1-1.)-(1.-t1)/stretch).r;

	vec2 P0;
	vec2 P1;

	// LOG FFT
	// P0 = vec2(t0*2.-1., log2(10.*fl0+0.002)/18.0-.5);
	// P1 = vec2(t1*2.-1., log2(10.*fl1+0.002)/18.0-.5);
	// quad(P0, P1, width);

	// LOG FFT
	P0 = vec2(t0*2.-1., 2.*sqrt(40.*fl0)/23.0-.9);
	P1 = vec2(t1*2.-1., 2.*sqrt(40.*fl1)/23.0-.9);
	quad(P0, P1, width);

	// // NORMAL FFT
	// P0 = vec2(t0*2.-1., fl0-1.);
	// P1 = vec2(t1*2.-1., fl1-1.);
	// quad(P0, P1, width);
}
