vec4 fg = vec4(0,1,1,1);
vec4 bg = vec4(0.06);

in vec2 geom_p;
out vec4 C;
void main() {
	vec2 U = geom_p;

	U.x/=2.;
	U.y = 2.*U.y-1.;

	float sl = texture(iSoundL, U.x).r;
	float sr = texture(iSoundR, U.x).r;
	sl = sl/1.75 + .5;
	sr = sr/1.75 - .5;

	C = bg;
	if (abs(U.y - sl) < .005)
		C = fg;
	if (abs(U.y - sr) < .005)
		C = fg;
	C.a = 1.;
}
