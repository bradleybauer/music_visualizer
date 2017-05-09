#version 330
precision highp float;

// sound
uniform sampler1D SL; // left
uniform sampler1D SR; // right
// frequency
uniform sampler1D FL; // left
uniform sampler1D FR; // right
uniform vec2 R; // resolution
uniform float T; // time

out vec4 C;
void main() {
	C-=C;
	vec2 U = gl_FragCoord.xy/R;
	U.x/=6.;
	float sl = texture(FL, U.x).r;
	sl = log2(sl*20.)/8.;
	sl = max(sl, 0.);
	C.x+=sl;
	sl = sl/2. + .5;
	float sr = texture(FR, U.x).r;
	sr = log2(sr*20.)/8.;
	sr = max(sr, 0.);
	C.y+=sr;
	sr = sr/2.;
	if (U.y < sr && U.y < .5)
		C = vec4(C.y);
	else
		C.y=0.;
	if (U.y < sl && U.y > .5)
		C = vec4(C.x);
	else
		C.x=0.;
	C.a = 1.;
}
