#include <iostream>
#include <vector>
using std::cout;
using std::endl;
using std::vector;
#include <fstream>
#include <sstream>
using std::string;
using std::stringstream;

#include "gl.h"
#include "ShaderConfig.h"
#include "ShaderPrograms.h"

ShaderPrograms::ShaderPrograms(ShaderConfig& config, filesys::path shader_folder, bool& is_ok) {
	is_ok = true;
	for (int i = 0; i < config.mBuffers.size(); ++i)
		is_ok &= compile_buffer_shaders(shader_folder, config.mBuffers[i].name);
	is_ok &= compile_buffer_shaders(shader_folder, "image");

	//    user buffer samplers [num_uu, num_bs]
	//    buffer size uniform
	//    user uniforms [m+1, num_uu] // append as constant vectors/floats?

	// Layout the uniforms to have explicit uniform locations so that we do not need to keep
	// a list of uniform locations.
}

ShaderPrograms::~ShaderPrograms() {
	for (GLuint p : mPrograms)
		glDeleteShader(p);
}

void ShaderPrograms::use_program(int i) const {
	if (i < mPrograms.size())
		glUseProgram(mPrograms[i]);
	else
		cout << "i = " + std::to_string(i) + " is not a program index" << endl;
}

bool ShaderPrograms::compile_shader(const GLchar* s, GLuint& sn, GLenum stype) {
	sn = glCreateShader(stype);
	glShaderSource(sn, 1, &s, NULL);
	glCompileShader(sn);
	GLint isCompiled = 0;
	glGetShaderiv(sn, GL_COMPILE_STATUS, &isCompiled);
	if (isCompiled == GL_FALSE) {
		GLint maxLength = 0;
		glGetShaderiv(sn, GL_INFO_LOG_LENGTH, &maxLength);
		vector<GLchar> errorLog(maxLength);
		glGetShaderInfoLog(sn, maxLength, &maxLength, &errorLog[0]);
		for (GLchar c : errorLog)
			cout << c;
		cout << endl;
		glDeleteShader(sn);
		return false;
	}
	return true;
}

bool ShaderPrograms::link_program(GLuint& pn, GLuint& vs, GLuint& gs, GLuint fs) {
	pn = glCreateProgram();
	glAttachShader(pn, gs);
	glAttachShader(pn, fs);
	glAttachShader(pn, vs);
	glLinkProgram(pn);
	GLint isLinked = 0;
	glGetProgramiv(pn, GL_LINK_STATUS, &isLinked);
	if (isLinked == GL_FALSE) {
		GLint maxLength = 0;
		glGetProgramiv(pn, GL_INFO_LOG_LENGTH, &maxLength);
		vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(pn, maxLength, &maxLength, &infoLog[0]);
		for (GLchar c : infoLog)
			cout << c;
		cout << endl;
		glDeleteShader(vs);
		glDeleteShader(fs);
		glDeleteShader(gs);
		glDeleteProgram(pn); // automatically detaches shaders
		return false;
	}
	// Always detach and delete shaders after a successful link.
	glDeleteShader(vs);
	glDeleteShader(fs);
	glDeleteShader(gs);
	glDetachShader(pn, vs);
	glDetachShader(pn, fs);
	glDetachShader(pn, gs);
	return true;
}

bool ShaderPrograms::compile_buffer_shaders(filesys::path shader_folder, string buff_name) {
	cout << "Compiling shaders for buffer " + buff_name + "." << endl;
	filesys::path filepath;
	std::ifstream shader_file;
	stringstream geom_str;
	stringstream frag_str;

	string ver_header = R"(
#version 430
precision highp float;
)";
	string uni_header = R"(
layout(location=0) uniform vec3 iMouse;
layout(location=1) uniform vec2 iRes;
layout(location=2) uniform float iTime;
layout(location=3) uniform int iFrame;
layout(location=4) uniform sampler1D iSoundR;
layout(location=5) uniform sampler1D iSoundL;
layout(location=6) uniform sampler1D iFreqR;
layout(location=7) uniform sampler1D iFreqL;
layout(location=8) uniform int iAudioFrames;

)";

	filepath = filesys::path(shader_folder / (buff_name + ".geom"));
	if (! filesys::exists(filepath)) {
		cout << '\t' << "Geometry shader does not exist." << endl;
		return false;
	}
	if (! filesys::is_regular_file(filepath)) {
		cout << '\t' << buff_name+".geom is not a regular file." << endl;
		return false;
	}
	shader_file = std::ifstream(filepath);
	if (! shader_file.is_open()) {
		cout << '\t' << "Error opening geometry shader." << endl;
		return false;
	}
	geom_str << ver_header;
	geom_str << uni_header;
	geom_str << string("layout(points) in;\n");
	geom_str << shader_file.rdbuf();

	filepath = filesys::path(shader_folder / (buff_name + ".frag"));
	if (! filesys::exists(filepath)) {
		cout << '\t' << "Fragment shader does not exist." << endl;
		return false;
	}
	if (! filesys::is_regular_file(filepath)) {
		cout << '\t' << buff_name+".frag is not a regular file." << endl;
		return false;
	}
	if (shader_file.is_open())
		shader_file.close();
	shader_file = std::ifstream(filepath);
	if (! shader_file.is_open()) {
		cout << '\t' << "Error opening fragment shader." << endl;
		return false;
	}
	frag_str << ver_header;
	frag_str << uni_header;
	frag_str << shader_file.rdbuf();

	string vertex_shader = ver_header + string("void main(){}");
	GLuint vs, gs, fs;
	bool ok = compile_shader(vertex_shader.c_str(), vs, GL_VERTEX_SHADER);
	if (!ok) {
		cout << '\t' << "Internal error: vertex shader didn't compile." << endl;
		return false;
	}
	ok = compile_shader(geom_str.str().c_str(), gs, GL_GEOMETRY_SHADER);
	if (!ok) {
		cout << '\t' << "Failed to compile geometry shader." << endl;
		return false;
	}
	ok = compile_shader(frag_str.str().c_str(), fs, GL_FRAGMENT_SHADER);
	if (!ok) {
		cout << '\t' << "Failed to compile fragment shader." << endl;
		return false;
	}
	GLuint program;
	ok = link_program(program, vs, gs, fs);
	if (!ok) {
		cout << '\t' << "Failed to link program." << endl;
		return false;
	}

	mPrograms.push_back(program);

	cout << "Successfully compiled shader program for buffer " + buff_name + "." << endl;
	return true;
}
