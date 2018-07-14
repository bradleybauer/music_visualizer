#include <cmath>

static float fract(float x) {
    return x - std::floor(x);
}

static float mix(float x, float y, float m) {
    return (1.f - m) * x + (m) * y;
}

static float hash11(float p) {
    return fract(std::sin(p) * 43758.5453123f); 
}

static float noise(float x) {
	float u = std::floor(x);
	float v = fract(x);
	v = v * v * (3.f - 2.f * v);
    return mix(hash11(u), hash11(u + 1.f), v);
}

float fbm(float p) {
	float f = .5f * noise(p);
	p *= 2.01f;
	f += .25f * noise(p);
	p *= 2.02f;
	f += .25f * noise(p);
	p *= 2.03f;
	f += .25f * noise(p);
	p *= 2.04f;
	f += .25f * noise(p);
    return f * .7f;
}
