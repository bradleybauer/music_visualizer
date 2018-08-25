layout(triangle_strip, max_vertices=24) out;

out float width;
out float intensity;
out float min_intensity;
out vec3 uvl;

void quad(vec2 P0, vec2 P1, float thickness) {
    /*
         1------3
         | \    |
         |   \  |
         |     \|
         0------2
    */
    vec2 dir = P1-P0;
    float dl = length(dir);
    // If the segment is too short, just draw a square
    dir = normalize(dir);
    vec2 norm = vec2(-dir.y, dir.x);

    uvl = vec3(dl+thickness, -thickness, dl);
    gl_Position = vec4(P0+(-dir-norm)*thickness, 0., 1.);
    EmitVertex(); // 0

    uvl = vec3(dl+thickness, thickness, dl);
    gl_Position = vec4(P0+(-dir+norm)*thickness, 0., 1.);
    EmitVertex(); // 1

    uvl = vec3(-thickness, -thickness, dl);
    gl_Position = vec4(P1+(dir-norm)*thickness, 0., 1.);
    EmitVertex(); // 2
    EndPrimitive();

    uvl = vec3(-thickness, -thickness, dl);
    gl_Position = vec4(P1+(dir-norm)*thickness, 0., 1.);
    EmitVertex(); // 2

    uvl = vec3(dl+thickness, thickness, dl);
    gl_Position = vec4(P0+(-dir+norm)*thickness, 0., 1.);
    EmitVertex(); // 1

    uvl = vec3(-thickness, thickness, dl);
    gl_Position = vec4(P1+(dir+norm)*thickness, 0., 1.);
    EmitVertex(); // 3
    EndPrimitive();
}

void main() {
    float t0 = (iGeomIter+0)/iNumGeomIters;
    float t1 = (iGeomIter+1)/iNumGeomIters;

    width = .01;
    intensity = .2;
    min_intensity = .1;

    float sl0 = texture(iSoundL, t0).r;
    float sl1 = texture(iSoundL, t1).r;
    float sr0 = texture(iSoundR, t0).r;
    float sr1 = texture(iSoundR, t1).r;

    vec2 P0 = vec2(sl0, sr0);
    vec2 P1 = vec2(sl1, sr1);
    quad(P0, P1, width);
}
