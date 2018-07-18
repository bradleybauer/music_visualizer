in vec2 geom_p;
out vec4 c;

vec4 bg = vec4(52./256., 9./256., 38./256., 1.);
vec4 fg = vec4(1.,195./256.,31./256.,1.);

//vec4 fg = vec4(248./256.,73./256.,52./256.,1.);
//vec4 bg = vec4(77./256., 94./256., 95./256., 1.);

//vec4 fg = vec4(221./256.,249./256.,30./256.,1.);
//vec4 bg = vec4(246./256., 69./256., 114./256., 1.);

//vec4 fg = vec4(1);
//vec4 bg = vec4(0);

//vec4 fg = vec4(1,vec3(0));
//vec4 bg = vec4(0);

void main() {
	if (iFrame == 0) {
		c = fg;
		return;
	}

	float al = texture(iA, geom_p).r;
	al *= 5.;
	vec4 new_color = mix(bg, fg, al);
	vec4 old_color = texture(iB, geom_p);

	c = mix(new_color, old_color, .75);
	c.a = 1.; // Replaces color
}
