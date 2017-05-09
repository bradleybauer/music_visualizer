#version 330
precision highp float;

const float EPS = 1E-6;
const float TAU = 6.283185307179586;
const float TAUR = 2.5066282746310002;
const float SQRT2 = 1.4142135623730951;
vec3 fg = vec3(1.);
vec3 bg = vec3(0.);
in vec4 uvl;

// A standard gaussian function, used for weighting samples
float gaussian(float x, float sigma) {
  return exp(-(x * x) / (2.0 * sigma * sigma)) / (TAUR * sigma);
}

// This approximates the error function, needed for the gaussian integral
float erf(float x) {
	float s = sign(x), a = abs(x);
	x = 1.0 + (0.278393 + (0.230389 + (0.000972 + 0.078108 * a) * a) * a) * a;
	x *= x;
	return s - s / (x * x);
}

out vec4 C;
void main() {

	float len = uvl.z;
	vec2 xy = uvl.xy;
	float alpha;

	const float param = .005;

	float sigma = param/(2. + 2000.*param/50.);
	alpha = erf(xy.x/SQRT2/sigma) - erf((xy.x-len)/SQRT2/sigma);
	alpha *= exp(-xy.y*xy.y/(2.0*sigma*sigma))/2.0/len*param;

	float uIntensity = .5;
	float uIntensityBase = .09;
	alpha = pow(alpha,1.0-uIntensityBase)*(0.01+min(0.99,uIntensity*3.0));
	C = vec4(fg, alpha);
}
