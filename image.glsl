#version 330
precision highp float;

#define EPS 1E-6
#define TAU 6.283185307179586
#define TAUR 2.5066282746310002
#define SQRT2 1.4142135623730951
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
  x = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a * a)) * a) * a;
  x *= x;
  return s - s / (x * x);
}

out vec4 C;
void main() {

    float len = uvl.z;
    vec2 xy = uvl.xy;
    float alpha;

	const float param = .005;

    float sigma = param/4.0;
    if (len < EPS) {
		// If the beam segment is too short, just calculate intensity at the position.
        alpha = exp(-pow(length(xy),2.0)/(2.0*sigma*sigma))/2.0/sqrt(param);
    } else {
		// Otherwise, use analytical integral for accumulated intensity.
        alpha = erf(xy.x/SQRT2/sigma) - erf((xy.x-len)/SQRT2/sigma);
        alpha *= exp(-xy.y*xy.y/(2.0*sigma*sigma))/2.0/len*param;
    }

    // float afterglow = smoothstep(0.0, 0.33, uvl.w);
    // alpha *= afterglow * uIntensity;
	alpha *= 4.;
	C = vec4(fg, alpha);
}
