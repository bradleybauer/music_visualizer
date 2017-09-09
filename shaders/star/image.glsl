#version 330
precision highp float;

uniform sampler1D FL; // left channel
uniform vec2 R; // resolution
uniform float T; // T

uniform vec2 resolution;
uniform vec3 mouse;
uniform vec3 color1;
uniform vec3 color2;

uniform vec2 info; // x == bass bump (gets smaller when bass is larger)
                   // y == fish_swim T
                   // z == button panel height in gl coords

float f(float x)
{
    vec4 t = texture2D(FL,vec2(x,0.));
    return (t.r/256.+t.a)*2.-1.;
}
void main()
{
	vec2 p = tex_pos*2.-1.;
	float aspect = resolution.x/resolution.y;
	p.y /= aspect;
	p *= 1. + fract(max(aspect, .7))/2.;

	float theta = atan(p.y,p.x);
	float len = length(p);

	// Make star fish

	// Set the fishies parameters
	float fish_number_legs = 10.;

	// Make the fish twirl around
	float fish_spin = T/2.;

	// Make the fish get bigger
	float fish_leg_len = .1*(.7+.7*info.x); // half leg length

	// Make the fish move its legs, if mouse.x==0 star fish dead
	float fish_leg_move = mouse.x*sin(4.*T+len)*(2.*3.141592653589793);

	// Put the fish togeter
	float fish = fish_leg_len*sin(fish_leg_move + fish_spin + fish_number_legs*theta);

	// Make the fish jump a little
	float fish_jump = .08*info.x; // just a soft bump

	// Pixel distance to fish
	float fish_dist = 1.-len*(.8+fish_jump+fish);

	// Fishes swim away
	float fish_swim = info.y;

	// Fishes are packed fin to fin and gill to gill
	float fish_school = 2.0*abs(fract(.5*fish_dist + fish_swim)-.5);
	fish_school+=0.01; // remove the always zero frequencies.

	// Eat the fish
	float v = (fish_school+1.)*3.*f(fish_school);

	// But only the biggest fish
	v *= clamp(exp((abs(v)-.888*smoothstep(0.,1.,mouse.y))*25.), 0., 1.);

	// TODO consider mixing 3 colors when in d/dt[FFT] mode
	// gl_FragColor.rgb = mix(color2, color1, v*(3.-2.*smoothstep(-0.9, -.7, mouse.y*2.-1.)));
	gl_FragColor.rgb = mix(color2, color1, v);
	gl_FragColor.a = 1.;
}
