#version 100
#define EPS 1E-6
// uniform float uSize;
attribute vec2 aStart;
attribute vec2 aEnd;
attribute float aIdx;
// uvl.xy is used later in fragment shader
varying vec4 uvl;
void main () {
    float tang;
    vec2 current;
    // All points in quad contain the same data:
    // segment start point and segment end point.
    // We determine point position using its index.
    //float idx = mod(aIdx,4.0);
	float idx = aIdx; // attribute already cycles

    // `dir` vector is storing the normalized difference
    // between end and start
    vec2 dir = aEnd-aStart;
    uvl.z = length(dir);

    if (uvl.z > EPS) {
        dir = dir / uvl.z;
    } else {
    // If the segment is too short, just draw a square
        dir = vec2(1.0, 0.0);
    }
    // norm stores direction normal to the segment difference
    vec2 norm = vec2(-dir.y, dir.x);

	const float uSize = .049;

    // `tang` corresponds to shift "forward" or "backward"
    if (idx >= 2.0) {
        current = aEnd;
        tang = 1.0;
        uvl.x = -uSize;
    } else {
        current = aStart;
        tang = -1.0;
        uvl.x = uvl.z + uSize;
    }
    // `side` corresponds to shift to the "right" or "left"
    float side = (mod(idx, 2.0)-0.5)*2.0;
    uvl.y = side * uSize;
    uvl.w = floor(aIdx / 4.0 + 0.5);

    gl_Position = vec4((current+(tang*dir+norm*side)*uSize),0.0,1.0);
}

