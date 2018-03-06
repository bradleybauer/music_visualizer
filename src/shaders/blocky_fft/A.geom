layout(triangle_strip, max_vertices=24) out;

out vec2 P;

float integ(float x) {
    float f = texture(iFreqL, x).r*2.;
    f += texture(iFreqL, x+1./2048.).r*.5;
    f += texture(iFreqL, x-1./2048.).r*.5;
    f += texture(iFreqL, x+2./2048.).r*.5;
    f += texture(iFreqL, x-2./2048.).r*.5;
    f += texture(iFreqL, x+3./2048.).r*.5;
    f += texture(iFreqL, x-3./2048.).r*.5;
    return f/5.;
}

void main() {
	float n = iGeomIter/iNumGeomIters;

	float width = 0.014;

	n+=width/4.;

	float stretch = 100.;
	float f = integ(pow(stretch, n-1.)-(1.-n)/stretch);

	// LOG
	vec2 p = vec2(n*2.-1., log(40.*f)/4.+.4);

	// SQRT
	// vec2 p = vec2(n*2.-1., .5*sqrt(f));

	// NORMAL FFT
	// p = vec2(n*2.-1., .5*f-1.);

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
