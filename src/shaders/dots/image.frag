in vec2 p;
out vec4 c;

void main() {
    const float freq = 30.;
    const float bezel = .7;

    // read from texture at grid
    vec2 uv = p;
    uv.x *= iRes.x / iRes.y;

    vec2 grid = floor(uv * freq) / freq;
    uv = fract(uv * freq);

    float separation = step(bezel, 1. - abs(uv.x - .5));
    separation *= step(bezel, 1. - abs(uv.y - .5));

    vec4 pink = vec4(1);
    vec4 bg = vec4(0);

    grid.x *= iRes.y / iRes.x;
    float x = texture(ia, grid).r;
    c = mix(bg, pink, x) * separation;
}
