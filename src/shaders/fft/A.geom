layout(triangle_strip, max_vertices=24) out;

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
	if (dl < .001)
		dir = vec2(1., 0.);
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

float integ(float x) {
    float f = texture(iFreqL, x).r*2.;
    f += texture(iFreqL, x+1./2048.).r*.5;
    f += texture(iFreqL, x-1./2048.).r*.5;
    f += texture(iFreqL, x+2./2048.).r*.5;
    f += texture(iFreqL, x-2./2048.).r*.5;
    f += texture(iFreqL, x+3./2048.).r*.5;
    f += texture(iFreqL, x-3./2048.).r*.5;
    f += texture(iFreqL, x+4./2048.).r*.5;
    f += texture(iFreqL, x-4./2048.).r*.5;
    return f/6.;
}

void main() {
	float t0 = (iGeomIter+0)/iNumGeomIters;
	float t1 = (iGeomIter+1)/iNumGeomIters;

	width = .015;
	intensity = .1;
	min_intensity = .01;

	const float stretch = 100.;
	float ft0 = integ(pow(stretch, t0-1.)-(1.-t0)/stretch);
	float ft1 = integ(pow(stretch, t1-1.)-(1.-t1)/stretch);

	vec2 P0;
	vec2 P1;

	// LOG
	P0 = vec2(t0*2.-1., log(ft0+0.002)/4.8+.12);
	P1 = vec2(t1*2.-1., log(ft1+0.002)/4.8+.12);

	// SQRT
	// P0 = vec2(t0*2.-1., .56*sqrt(ft0)-.9);
	// P1 = vec2(t1*2.-1., .56*sqrt(ft1)-.9);

	// NORMAL FFT
	// P0 = vec2(t0*2.-1., .5*ft0-1.);
	// P1 = vec2(t1*2.-1., .5*ft1-1.);

	quad(P0, P1, width);
}
