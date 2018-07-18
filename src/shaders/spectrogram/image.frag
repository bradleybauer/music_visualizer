in vec2 geom_p;
out vec4 c;

float hash(float p) {
	vec3 p3  = fract(vec3(p) * 443.8975);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

void main () {
    c = texture(ia, geom_p);
    c.a = 1.;
}
