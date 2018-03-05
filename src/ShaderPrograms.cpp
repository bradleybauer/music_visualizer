#include <iostream>
using std::cout;
using std::endl;
#include <vector>
using std::vector;
#include <fstream>
#include <string>
using std::string;
using std::to_string;
#include <sstream>
using std::stringstream;
#include <stdexcept>
using std::runtime_error;

#include "ShaderConfig.h"
#include "ShaderPrograms.h"

ShaderPrograms::ShaderPrograms(const ShaderConfig& config, filesys::path shader_folder) {
	stringstream uniform_header;
	uniform_header << R"(
		uniform vec2 iMouse;
		uniform bool iMouseDown;
		uniform vec2 iRes;
		uniform float iTime;
		uniform int iFrame;
		uniform float iNumGeomIters;
		uniform sampler1D iSoundR;
		uniform sampler1D iSoundL;
		uniform sampler1D iFreqR;
		uniform sampler1D iFreqL;

		#define iResolution iRes
	)";

	int num_uniforms = ShaderPrograms::num_builtin_uniforms;

	// Put samplers for user buffers in header
	for (auto buff : config.mBuffers) {
		uniform_header << "uniform sampler2D i" << buff.name << ";\n";
	}
	num_uniforms += config.mBuffers.size();

	// Put user's uniforms in header
	for (auto uniform : config.mUniforms) {
		string type;
		if (uniform.values.size() == 1) // ShaderConfig ensures size is in [1,4]
			type = "float";
		else
			type = "vec" + to_string(uniform.values.size());

		uniform_header << "uniform " << type << " " << uniform.name << ";\n";
	}
	num_uniforms += config.mUniforms.size();
	uniform_header << "\n";

	for (auto buff : config.mBuffers)
		compile_buffer_shaders(shader_folder, buff.name, uniform_header.str());
	compile_buffer_shaders(shader_folder, "image", uniform_header.str());

	// get uniform locations for each program
	for (auto p : mPrograms) {
		vector<GLint> uniform_locs;
		uniform_locs.push_back(glGetUniformLocation(p, "iMouse"));
		uniform_locs.push_back(glGetUniformLocation(p, "iMouseDown"));
		uniform_locs.push_back(glGetUniformLocation(p, "iRes"));
		uniform_locs.push_back(glGetUniformLocation(p, "iTime"));
		uniform_locs.push_back(glGetUniformLocation(p, "iFrame"));
		uniform_locs.push_back(glGetUniformLocation(p, "iNumGeomIters"));
		uniform_locs.push_back(glGetUniformLocation(p, "iSoundR"));
		uniform_locs.push_back(glGetUniformLocation(p, "iSoundL"));
		uniform_locs.push_back(glGetUniformLocation(p, "iFreqR"));
		uniform_locs.push_back(glGetUniformLocation(p, "iFreqL"));
		for (auto b : config.mBuffers)
			uniform_locs.push_back(glGetUniformLocation(p, ("i" + b.name).c_str()));
		for (auto u : config.mUniforms)
			uniform_locs.push_back(glGetUniformLocation(p, u.name.c_str()));
		mUniformLocs.push_back(std::move(uniform_locs));
	}
}

ShaderPrograms & ShaderPrograms::operator=(ShaderPrograms && o) {
	// Delete my shaders
	for (auto p : mPrograms)
		glDeleteProgram(p);
	
	// Move other's shaders
	mPrograms = std::move(o.mPrograms);
	mUniformLocs = std::move(o.mUniformLocs);

	return *this;
}

ShaderPrograms::~ShaderPrograms() {
	for (auto p : mPrograms)
		glDeleteProgram(p);
}

void ShaderPrograms::use_program(int i) const {
	if (i < mPrograms.size())
		glUseProgram(mPrograms[i]);
	else
		cout << "i = " + to_string(i) + " is not a program index" << endl;
}

GLint ShaderPrograms::get_uniform_loc(int program_i, int uniform_i) const {
	if (program_i >= mPrograms.size()) {
		cout << "program_i = " + to_string(program_i) + " is not a program index" << endl;
		return 0;
	}
	if (uniform_i >= mUniformLocs[program_i].size()) {
		cout << "uniform_i = " + to_string(uniform_i) + " is not a uniform index" << endl;
		return 0;
	}
	return mUniformLocs[program_i][uniform_i];
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

bool ShaderPrograms::link_program(GLuint& pn, GLuint vs, GLuint gs, GLuint fs) {
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

void ShaderPrograms::compile_buffer_shaders(const filesys::path& shader_folder, const string& buff_name, const string& uniform_header) {
	cout << "Compiling shaders for buffer: " << buff_name << endl;

	filesys::path filepath;
	std::ifstream shader_file;
	stringstream geom_str;
	stringstream frag_str;

	string version_header = R"(
		#version 330
		precision highp float;
	)";

	filepath = filesys::path(shader_folder / (buff_name + ".geom"));
	if (! filesys::exists(filepath))
		throw runtime_error("\tGeometry shader does not exist.");
	if (! filesys::is_regular_file(filepath))
		throw runtime_error("\t" + buff_name + ".geom is not a regular file.");
	shader_file = std::ifstream(filepath.string());
	if (! shader_file.is_open())
		throw runtime_error("\tError opening geometry shader.");
	geom_str << version_header;
	geom_str << uniform_header;
	geom_str << string("layout(points) in;\n #define iGeomIter (float(gl_PrimitiveIDIn)) \n");
	geom_str << shader_file.rdbuf();
	if (shader_file.is_open())
		shader_file.close();

	filepath = filesys::path(shader_folder / (buff_name + ".frag"));
	if (! filesys::exists(filepath))
		throw runtime_error("\tFragment shader does not exist.");
	if (! filesys::is_regular_file(filepath))
		throw runtime_error("\t" + buff_name + ".frag is not a regular file.");
	shader_file = std::ifstream(filepath.string());
	if (! shader_file.is_open())
		throw runtime_error("\tError opening fragment shader.");
	frag_str << version_header;
	frag_str << uniform_header;
	frag_str << shader_file.rdbuf();
	if (frag_str.str().find("mainImage", uniform_header.size() + version_header.size()) != std::string::npos)
		frag_str << "\nout vec4 asdsfasdFDSDf; void main() {mainImage(asdsfasdFDSDf, gl_FragCoord.xy);}";

	string vertex_shader = version_header + string("void main(){}");
	GLuint vs, gs, fs;
	bool ok = compile_shader(vertex_shader.c_str(), vs, GL_VERTEX_SHADER);
	if (!ok)
		throw runtime_error("\tInternal error: vertex shader didn't compile.");
	cout << "Compiling " + buff_name + ".geom" << endl;
	ok = compile_shader(geom_str.str().c_str(), gs, GL_GEOMETRY_SHADER);
	if (!ok)
		throw runtime_error("Failed to compile geometry shader.");
	cout << "Compiling " + buff_name + ".frag" << endl;
	ok = compile_shader(frag_str.str().c_str(), fs, GL_FRAGMENT_SHADER);
	if (!ok)
		throw runtime_error("Failed to compile fragment shader.");
	GLuint program;
	ok = link_program(program, vs, gs, fs);
	if (!ok)
		throw runtime_error("Failed to link program.");

	mPrograms.push_back(program);
}
