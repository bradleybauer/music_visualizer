#version 330
precision highp float;

in int _pointIndex;
out int pointIndex;

void main () {
	pointIndex = _pointIndex;
}
