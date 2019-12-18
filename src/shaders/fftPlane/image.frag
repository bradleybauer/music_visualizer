vec4 fg = vec4(1);
vec4 bg = vec4(0);

in vec2 geom_p;
out vec4 C;

float dbox(vec3 p, vec3 dim) {
    return length(max(abs(p) - dim, vec3(0)));
}

vec2 box(vec3 ro, vec3 rd, vec3 dim) {
    float t = 0.;
    float d;
    for (int i = 0; i < 16; ++i) {
        vec3 p = ro + rd * t;
        d = dbox(p, dim);
        t += d;
    }
    return vec2(t, d);
}

float dtl(vec3 p, vec3 lo, vec3 ld, float l) {
    vec3 x = p - lo; // x's origin is now at lo
    float t = clamp(dot(ld, x), 0., l);
    return length(x - ld * t);
}

float tx(sampler1D tex, float x) {
    return .333*texture(tex, x).r +
           .333*texture(tex, x+.005).r +
           .333*texture(tex, x-.005).r;
}

float get_sound(vec2 x) {
    float sl = tx(iSoundR, x.x);
    float sr = tx(iSoundR, x.y);
    return .2*(sl+sr);
}

float get_freq(vec2 x) {
    x = max(vec2(0), x*.9 + (1.-.9));
    x *= x;
    x *= .2;
    float fl = tx(iFreqL, x.x);
    float fr = tx(iFreqR, x.y);
    return 6.*sqrt(fl*fr);
}

float plane(vec3 p, vec3 pb1, vec3 pb2, vec3 pn, vec2 bound) {
    mat3 r3_to_plane_basis = transpose(mat3(pb1, pn, pb2));
    vec3 x = r3_to_plane_basis * p;
    //float frq = .4*sqrt(get_freq((x.xz / bound*.5 + .5)*.75));
    float frq = .3*sqrt(get_freq(abs(x.xz) / bound));
    return x.y - frq + .125;
}

vec3 box_dim = vec3(.5,.25,.5);

float map(vec3 p) {
    float d = plane(p, vec3(0,0,1), vec3(1,0,0), vec3(0,1,0), vec2(.5));
    d = max(d, dbox(p,box_dim));
    return d;
}

vec2 sphere(vec3 p, vec3 ro, vec3 rd, float r) {
    float a = dot(rd, rd);
    float b = 2.0 * dot(ro - p, rd);
    float c = dot(ro - p, ro - p) - r*r;
    float h = b*b - 4.0*a*c;
    if (h < 0.)
        discard;
    h = sqrt(h);
    return vec2(-b - h, -b + h);
}

float tri_min(vec3 x) {
    return min(x.x,min(x.y,x.z));
}

void main() {
    vec2 uv = geom_p * 2. - 1.;
    uv.x *= iRes.x / iRes.y;

    uv /= 2.;

    C = bg;

    float away = 8.;
    float th = - iMouse.x / 80. + iTime / 10.; float cs = cos(th); float sn = sin(th);
    vec3 ro = away*vec3(cs, .5, sn);
    vec3 ta = vec3(0,-.07,0);
    vec3 forward = normalize(ta - ro);
    vec3 up = vec3(0,1,0);
    vec3 right = vec3(-sn, 0., cs); // deriv of ro, which also is (-z,0,x)
    //vec3 right = normalize(cross(forward,up));
    vec3 rd = normalize(vec3(mat3(right, up, forward) * vec3(uv.x, uv.y, away)));

    vec2 bound = box(ro,rd,box_dim);
    if (bound.y > .03)
        discard;
    float t = bound.x;
    vec3 p;
    float i = 0.;
    for (i = 0.; i < 128.; i+=1.) {
        p = ro+rd*t;
        float d = map(p);
        if (d < .005) {
            C = 7.*(p.y+.07) * fg;
            break;
        }
        else if (tri_min(abs(p) - box_dim) > 0.) {
            //C = vec4(1.,0,0,1);
            break;
        }
        t += d*.15;
    }
    //C = vec4(float(i > 100.));
}
