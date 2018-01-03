uniform sampler2D buff_B;
uniform vec2 Res;
in vec2 p;
out vec4 c;
void main() {
    c=texture(buff_B, p);
}
