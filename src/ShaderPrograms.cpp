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

#include "ShaderPrograms.h"

ShaderPrograms::ShaderPrograms(const ShaderConfig& config,
                               const Renderer& renderer,
                               const Window& window,
                               const filesys::path& shader_folder) {
    #define lambda [&](const int p, const Buffer& b)
    builtin_uniforms = {
        {"vec2","iMouse",         lambda{ glUniform2f(get_uniform_loc(p, 0), window.mouse.x, window.mouse.y); }},
        {"bool","iMouseDown",     lambda{ glUniform1i(get_uniform_loc(p, 1), window.mouse.down); } },
        {"vec2","iMouseDownPos",  lambda{ glUniform2f(get_uniform_loc(p, 2), window.mouse.last_down_x, window.mouse.last_down_y); }},
        {"vec2","iRes",           lambda{ glUniform2f(get_uniform_loc(p, 3), window.width, window.height); }},
        {"float","iTime",         lambda{ glUniform1f(get_uniform_loc(p, 4), renderer.elapsed_time); }},
        {"int","iFrame",          lambda{ glUniform1i(get_uniform_loc(p, 5), renderer.frame_counter); }},
        {"float","iNumGeomIters", lambda{ glUniform1f(get_uniform_loc(p, 6), float(b.geom_iters)); }},
        {"sampler1D", "iSoundR",  lambda{ glUniform1i(get_uniform_loc(p, 7), 0); }}, // texture_unit 0
        {"sampler1D", "iSoundL",  lambda{ glUniform1i(get_uniform_loc(p, 8), 1); }}, // texture_unit 1
        {"sampler1D", "iFreqR",   lambda{ glUniform1i(get_uniform_loc(p, 9), 2); }}, // texture_unit 2
        {"sampler1D", "iFreqL",   lambda{ glUniform1i(get_uniform_loc(p, 10), 3); }}, // texture_unit 3
        {"vec2", "iBuffRes",      lambda{ glUniform2f(get_uniform_loc(p, 11), float(b.width), float(b.height)); }}
    };
    #undef lambda

	stringstream uniform_header;
    for (const uniform_info& uniform : builtin_uniforms) {
        uniform_header << "uniform " << uniform.type << " " << uniform.name << ";\n";
    }
    uniform_header << "#define iResolution iRes\n";

	// Put samplers for user buffers in header
	for (const Buffer& b : config.mBuffers) {
		uniform_header << "uniform sampler2D i" << b.name << ";\n";
	}

	// Put user's uniforms in header
	for (const Uniform& uniform : config.mUniforms) {
		string type;
		if (uniform.values.size() == 1) // ShaderConfig ensures size is in [1,4]
			type = "float";
		else
			type = "vec" + to_string(uniform.values.size());

		uniform_header << "uniform " << type << " " << uniform.name << ";\n";
	}

    // make error message line numbers correspond to line numbers in my text editor
    uniform_header << "#line 0\n";

	for (const Buffer& b : config.mBuffers)
		compile_buffer_shaders(shader_folder, b.name, uniform_header.str(), b.uses_default_geometry_shader);
	compile_buffer_shaders(shader_folder, config.mImage.name, uniform_header.str(), config.mImage.uses_default_geometry_shader);

	// get uniform locations for each program
	for (GLuint p : mPrograms) {
		vector<GLint> uniform_locs;
        for (const uniform_info& u : builtin_uniforms)
            uniform_locs.push_back(glGetUniformLocation(p, u.name.c_str()));
		for (const Buffer& b : config.mBuffers)
			uniform_locs.push_back(glGetUniformLocation(p, ("i" + b.name).c_str()));
		for (const Uniform& u : config.mUniforms)
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

// TODO always report warnings
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

static const std::string default_geom_shader = R"(
layout(points) in;
layout(triangle_strip, max_vertices = 6) out;
out vec2 geom_p;
void main() {
    /* 1------3
       | \    |
       |   \  |
       |     \|
       0------2 */
    const vec2 p0 = vec2(-1., -1.);
    const vec2 p1 = vec2(-1., 1.);
    gl_Position = vec4(p0, 0., 1.);
    geom_p = p0 * .5 + .5;
    EmitVertex(); // 0
    gl_Position = vec4(p1, 0., 1.);
    geom_p = p1 * .5 + .5;
    EmitVertex(); // 1
    gl_Position = vec4(-p1, 0., 1.);
    geom_p = -p1 * .5 + .5;
    EmitVertex(); // 2
    EndPrimitive();

    gl_Position = vec4(-p1, 0., 1.);
    geom_p = -p1 * .5 + .5;
    EmitVertex(); // 2
    gl_Position = vec4(p1, 0., 1.);
    geom_p = p1 * .5 + .5;
    EmitVertex(); // 1
    gl_Position = vec4(-p0, 0., 1.);
    geom_p = -p0 * .5 + .5;
    EmitVertex(); // 3
    EndPrimitive();
}
)";

void ShaderPrograms::compile_buffer_shaders(const filesys::path& shader_folder, const string& buff_name, const string& uniform_header, const bool uses_default_geometry_shader) {
	cout << "Compiling shaders for buffer: " << buff_name << endl;

	filesys::path filepath;
	std::ifstream shader_file;
	stringstream geom_str;
	stringstream frag_str;

	string version_header = R"(
		#version 330
		precision highp float;
	)";

    if (uses_default_geometry_shader) {
        geom_str << version_header;
        geom_str << uniform_header;
        geom_str << default_geom_shader;
    }
    else {
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
    }

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

