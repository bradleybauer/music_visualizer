out vec4 C;
void main() {
	vec2 U = gl_FragCoord.xy/iRes;
	U.x/=6.;
	float sl = texture(iFreqL, U.x).r;
	sl = sl/2. + .5;
	float sr = texture(iFreqR, U.x).r;
	sr = sr/2.;
	C-=C;
	if (abs(U.y - sl) < 0.002)
		C = vec4(1.);
	if (abs(U.y - sr) < 0.002)
		C = vec4(1.);
	C.a = 1.;
}
