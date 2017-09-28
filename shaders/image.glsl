#version 330
precision highp float;

uniform sampler1D FL; // left channel
uniform sampler1D FR; // left channel
uniform vec2 R; // resolution
uniform float T; // time

float f(float x)
{
	x /= 3.;
	return (texture(FL, x).r + texture(FR, x).r)/2.;
}

#define SPIRAL

out vec4 C;
void main()
{
	float threshold = .2;
	float time = T/100.;
	vec3 color1 = vec3(1);
	vec3 color2 = vec3(0);

	vec2 p = gl_FragCoord.xy/R*2.-1.;
#ifdef SPIRAL
	p*=1.5;
	p-=vec2(.0,-.4);
#endif
	float aspect = R.x/R.y;
	p.y /= aspect;
	p *= 1. + fract(max(aspect, .7))/2.;

	float theta = atan(p.y,p.x);
	float len = length(p);

	// Make star fish

	// bass
	// float bump = (f(0.01)+f(0.02)+f(0.03))*.05;
	float bump = f(0.02)*.07;
	// bump*=0.;

	// Set the fishies parameters
	float fish_number_legs = 10.;

	// Make the fish twirl around
	float fish_spin = time*5.;

	// Make the fish get bigger
	float fish_leg_len = .1*(1.1+bump);

	// Make the fish move its legs, if mouse.x==0 star fish dead
	float fish_leg_move = 0.1*sin(40.*time+len)*(2.*3.141592653589793);

	// Put the fish togeter
	float fish = fish_leg_len*sin(fish_leg_move + fish_spin + fish_number_legs*theta);

	// Make the fish jump a little
	float fish_jump = .25*bump; // just a soft bump

#ifdef SPIRAL
	const float PI = 3.141592;
	float spiral = (theta-PI/2.)/PI;
	float fish_dist = 1.-len*(.8+fish_jump+fish)+spiral;
	float fish_swim = time;
	float fish_school = fract(fish_dist*.5);
	fish_school = pow(fish_school, 1.3);
	float v = f(fish_school);
	v = log(2.* v+.9);
	v *= smoothstep(0.,0.05,len*(fish_school));
#else
	// Pixel distance to fish
	float fish_dist = 1.-len*(.8+fish_jump+fish);

	// Fishes swim away
	float fish_swim = time/4.;

	// Fishes are packed fin to fin and gill to gill
	float fish_school = abs(fract(.5*fish_dist + fish_swim)-.5);
	fish_school += 0.05; // remove the always zero frequencies.
	fish_school = fract(fish_school*.8);

	// Eat the fish
	float v = f(fish_school);
	v = log(2.* v+.9);
	v *= smoothstep(0.,.07, len*(.8+fish));
#endif

	C.rgb = mix(color2, color1, v);
	C.a = 1.;
}
