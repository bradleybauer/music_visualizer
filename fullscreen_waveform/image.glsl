#version 330
precision highp float;

// sound
uniform sampler1D SL; // left
uniform sampler1D SR; // right
// frequency
uniform sampler1D FL; // left
uniform sampler1D FR; // right
uniform vec2 R; // resolution

out vec4 C;
void main() {
	vec2 U = gl_FragCoord.xy/R;
	float sl = texture(FL, U.x).r;
	sl = sl/2. + .5;
	float sr = texture(FR, U.x).r;
	sr = sr/2.;
	C-=C;
	// U = vec2(U.x, (1.-U.y)/2.);
	if (abs(U.y - sl) < 0.002)
		C = vec4(1.);
	if (abs(U.y - sr) < 0.002)
		C = vec4(1.);
	C.a = 1.;
}
