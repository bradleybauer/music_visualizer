in vec2 p;
out vec4 c;
void main () {
    float wave = texture(iSoundL, p.x).r * .5 + .5;

    vec4 prev_col = texture(ia, p);
    float dist = abs(p.y - wave);
    dist *= dist;

    /*
    c = vec4(clamp(.002 / dist, 0., 1.));
    /*/
    c = prev_col*.5 + vec4(clamp(.0005 / dist, 0., 1.));
    c = clamp(c, vec4(0), vec4(1.5));
    //*/
}
