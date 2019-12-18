const vec4 fg = vec4(1);
const vec4 bg = vec4(.1);

in float is_background;
out vec4 c;
void main() {
    if (is_background > .5)
        c = bg;
    else
        c = fg;
}
