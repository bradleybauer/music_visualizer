in vec2 geom_p;
out vec4 c;

float hash(float p) {
	vec3 p3  = fract(vec3(p) * 443.8975);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

void main () {
    // use manhatten dist in voronoi to get the blocky effect.
    // todo look into plane tilings

    // todo parallelogram is neat but a diagonal high tech (possibly mini voronoi)
    // pattern might be cooler

    vec2 uv = geom_p;
    uv.x *= iRes.x / iRes.y;

    // background
    vec2 rep_p = fract(uv*sqrt(iRes.y*14.));
    float bgmesh_bezel = .4 - .08 * smoothstep(500., 900., iRes.y);
    vec4 bgmesh = 1.5*vec4(.1, .092, .07, 1.) * step(bgmesh_bezel, rep_p.x) * step(bgmesh_bezel, rep_p.y);

    // main grid division
    const float freq = 33.;
    const float bezel = .7;
    const float skew = .5;

    // give pixel a skewed lattice point
    uv = vec2(uv.x-skew*uv.y, uv.y);
    vec2 lattice = floor(uv * freq) / freq;
    // unskew latice point
    lattice.x += floor(skew * uv.y * freq)/freq;

    // grid lines
    uv = fract(uv * freq);
    float grid = step(bezel, 1. - abs(uv.x - .5)) * step(bezel, 1. - abs(uv.y - .5));

    const float pinkness = .7;
    const float blueness = .8;
    const float brightness = 2.;

    vec4 pink = 1.4 * vec4(255./256., 20./256., 144./256., 1);
    vec4 blue = vec4(0., 204./256., 1., 1);
    vec4 bg = vec4(0);

    lattice.x *= iRes.y / iRes.x; // put lattice.x in range [0, 1]
    float x = texture(ia, lattice).r;
    c = mix(mix(bg, blue, smoothstep(.0, .3, blueness*x)), pink, pinkness*x);
    c *= grid * brightness;
    c = mix(bgmesh, c, smoothstep(.05, 1., length(c)));

    // vignette
    c.rgb *= pow(16.*geom_p.x*geom_p.y*(1.-geom_p.x)*(1.-geom_p.y), .3);

    // a bit of grain
    c *= 1. - .5*sqrt(hash(geom_p.x*geom_p.y+2.));

    c.a = 1.;
}
