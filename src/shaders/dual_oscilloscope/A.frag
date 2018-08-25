const float TAU = 6.283185307179586;
const float TAUR = 2.5066282746310002;
const float SQRT2 = 1.4142135623730951;

in vec3 uvl;
in float width;
in float intensity;
in float min_intensity;

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

	float sigma = width/(2. + 2000.*width/50.);
	alpha = erf(xy.x/SQRT2/sigma) - erf((xy.x-len)/SQRT2/sigma);
	alpha *= exp(-xy.y*xy.y/(2.0*sigma*sigma))/2.0/len*width;

	alpha = pow(alpha,1.0-min_intensity)*(0.01+min(0.99,intensity*3.0));
	C = vec4(vec3(1.), alpha);
}
