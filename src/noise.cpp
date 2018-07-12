#include <cmath>

static float fract(float x) {
    return x - std::floor(x);
}

static float mix(float x, float y, float m) {
    return (1. - m) * x + (m) * y;
}

static float hash11(float p) {
    return fract(std::sin(p) * 43758.5453123); 
}

static float noise(float x) {
	float p = std::floor(x);
	float f = fract(x);
	f = f * f * (3. - 2. * f);
    return mix(hash11(p), hash11(p + 1.), f);
}

float fbm(float p) {
	float f = .5 * noise(p);
	p *= 2.01;
	f += .25 * noise(p);
	p *= 2.02;
	f += .25 * noise(p);
	p *= 2.03;
	f += .25 * noise(p);
	p *= 2.04;
	f += .25 * noise(p);
    return f * .7;
}
