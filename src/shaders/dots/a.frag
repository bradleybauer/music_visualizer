in vec2 geom_p;
out vec4 c;
void main() {
    float wave = texture(iSoundL, geom_p.x).r*.5+.5;
    float dist = abs(geom_p.y - wave);
    dist *= sqrt(dist);
    c = vec4(clamp(0.004 / dist, 0., 1.));
}
