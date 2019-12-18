layout(triangle_strip, max_vertices=24) out;

out vec2 P;
out float is_background;

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

    gl_Position = vec4(P0+(-dir-norm)*thickness, 0., 1.);
    EmitVertex(); // 0

    gl_Position = vec4(P0+(-dir+norm)*thickness, 0., 1.);
    EmitVertex(); // 1

    gl_Position = vec4(P1+(dir-norm)*thickness, 0., 1.);
    EmitVertex(); // 2
    EndPrimitive();

    gl_Position = vec4(P1+(dir-norm)*thickness, 0., 1.);
    EmitVertex(); // 2

    gl_Position = vec4(P0+(-dir+norm)*thickness, 0., 1.);
    EmitVertex(); // 1

    gl_Position = vec4(P1+(dir+norm)*thickness, 0., 1.);
    EmitVertex(); // 3
    EndPrimitive();
}

void main() {
	float n = iGeomIter/iNumGeomIters;

	float width = 0.014;

	n += width/4.;

	float stretch = 100.;
	float f = integ(pow(stretch, n-1.)-(1.-n)/stretch);

	// LOG
	vec2 p = vec2(n*2.-1., log(40.*f)/4.+.4);

	// SQRT
	// vec2 p = vec2(n*2.-1., .5*sqrt(f));

        float height = .5;

        is_background = 1.;
        if (iGeomIter < 1.)
            quad(vec2(-1, -1), vec2(1,1), 2.);

        is_background = 0.;
        quad(vec2(p.x + width / 2., -1.), vec2(p.x + width / 2., p.y), width / 2.);
}
