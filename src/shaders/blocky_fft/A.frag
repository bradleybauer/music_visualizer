const vec4 fg = vec4(1);
const vec4 bg = vec4(0);

in float is_background;
out vec4 c;
void main() {
    if (is_background)
        c = fg;
    else
        c = bg;
}
