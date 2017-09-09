#version 330
precision highp float;

in int _point_index;
out int point_index;

void main () {
	point_index = _point_index;
}
