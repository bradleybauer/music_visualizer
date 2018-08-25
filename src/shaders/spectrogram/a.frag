in vec2 geom_p;
out vec4 c;
void main () {
    if (gl_FragCoord.x >= iRes.x-1) {
        float freq = log(1. + 10.*texture(iFreqL, (exp2(geom_p.y / 1.5) - 1.) ).r);
        c = vec4(freq);
    }
    else {
        vec4 adjcol = texture(ia, geom_p + vec2(1. / iRes.x, 0.));
        c = vec4(adjcol);
    }
}
