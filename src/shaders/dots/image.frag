vec4 fg = vec4(0,1,1,1);
vec4 bg = vec4(0);

in vec2 geom_p;
out vec4 c;

void main() {
    const float freq = 30.;
    const float bezel = .7;

    // read from texture at grid
    vec2 uv = geom_p;
    uv.x *= iRes.x / iRes.y;

    vec2 grid = floor(uv * freq) / freq;
    uv = fract(uv * freq);

    float separation = step(bezel, 1. - abs(uv.x - .5));
    separation *= step(bezel, 1. - abs(uv.y - .5));

    grid.x *= iRes.y / iRes.x;
    float x = texture(ia, grid).r;
    c = mix(bg, fg, x) * separation;
}
