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
#if 1
	C-=C;
	vec2 U = gl_FragCoord.xy/R;
	U.y=U.y*2.-1.;
	float s = texture(SL, U.x).r;
	s*=1.7;
	s=s*.5+.5;
	if (abs(U.y-s)<0.005)
		C=vec4(1.);

	s = texture(SR, U.x).r;
	s*=1.7;
	s=s*.5-.5;
	if (abs(U.y-s)<0.005)
		C=vec4(1.);

	C.a = 1.;
#else
	C-=C;
	vec2 U = gl_FragCoord.xy/R;
	if (U.y<texture(FL, U.x).r)
		C=vec4(1.);
	C.a = 1.;
#endif

	// C-=C;
	// vec2 U = gl_FragCoord.xy/R;
	// U.x/=6.;
	// float s = texture(FL, U.x).r;
	// s = log2(s*20.)/8.;
	// s = max(s, 0.);
	// C.x+=s*.8;
	// s = s/2. + .5;
	// if (U.y<.5) U.y = 1.-U.y;
	// if (U.y < s)
	// 	C = vec4(C.x);
	// C = vec4(length(C));
	// C.a = 1.;
}
