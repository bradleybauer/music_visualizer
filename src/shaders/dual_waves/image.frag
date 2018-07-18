in vec2 geom_p;
out vec4 C;
void main() {
	vec2 U = geom_p/iRes;

	U.x/=2.;
	U.y = 2.*U.y-1.;

	float sl = texture(iSoundL, U.x).r;
	float sr = texture(iSoundR, U.x).r;
	sl = sl/1.75 + .5;
	sr = sr/1.75 - .5;

	C-=C;
	if (abs(U.y - sl) < .005)
		C = vec4(1.);
	if (abs(U.y - sr) < .005)
		C = vec4(1.);
	C.a = 1.;
}
