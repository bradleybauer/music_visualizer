vec4 fg = vec4(1);
vec4 bg = vec4(0);

//vec4 bg = .9*vec4(52./256., 9./256., 38./256., 1.);
//vec4 fg = 1.1*vec4(1.,195./256.,31./256.,1.);

// vec4 bg = vec4(vec3(10, 23, 42)/77., 1.);
// vec4 fg = vec4(1.5);

in vec2 geom_p;
out vec4 c;

void main() {
	if (iFrame == 0) {
		c = fg;
		return;
	}

	float al = texture(iA, geom_p).r;
	al *= 4.;
	vec4 new_color = mix(bg, fg, al);
	vec4 old_color = texture(iB, geom_p);

	c = mix(old_color, new_color, .3333);
	c.a = 1.; // Replaces color
}
