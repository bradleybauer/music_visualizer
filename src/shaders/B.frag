vec4 fg = vec4(1., 2., 3., 1.);
vec4 bg = vec4(0);

in vec2 geom_p;
out vec4 c;

void main() {
	if (iFrame == 0) {
		c = fg;
		return;
	}

	float al = texture(iA, geom_p).r;

        if (geom_p.y > .5)
            al *= 1.3;

	vec4 new_color = 5.*mix(bg, fg, al);
	vec4 old_color = texture(iB, geom_p);

	c = mix(old_color, new_color, .1);
	c.a = 1.; // Replaces color
}
