#pragma once

#include <vector>
#include <string>
#include "filesystem.h"

#include <GL/glew.h>
#include "ShaderConfig.h"

// Uniforms
// vec2      iMouse
// bool      iMouseDown
// vec2      iRes
// float     iTime
// int       iFrame
// float     iNumGeomIters
// sampler1D iSoundR   texture_unit 0
// sampler1D iSoundL   texture_unit 1
// sampler1D iFreqR    texture_unit 2
// sampler1D iFreqL    texture_unit 3
//
// n samplers for n user buffers in config.mBuffers
// n uniforms for n user uniforms in config.mUniforms
//
// Programs
// program for buffer n is in mPrograms[n]
// program for image shader is in mPrograms.back()

class ShaderPrograms {
public:
	static const int num_builtin_uniforms = 10;
	ShaderPrograms(const ShaderConfig& config, filesys::path shader_folder);
	ShaderPrograms& operator=(ShaderPrograms&&);
	~ShaderPrograms();

	void use_program(int i) const;
	GLint get_uniform_loc(int program_i, int uniform_i) const;

private:
	ShaderPrograms(ShaderPrograms&) = delete;
	ShaderPrograms(ShaderPrograms&&) = delete;
	ShaderPrograms& operator=(ShaderPrograms&) = delete;

	bool compile_shader(const GLchar* s, GLuint& sn, GLenum stype);
	bool link_program(GLuint& pn, GLuint vs, GLuint gs, GLuint fs);
	void compile_buffer_shaders(const filesys::path& shader_folder, const std::string& buff_name, const std::string& uniform_header);

	std::vector<GLuint> mPrograms;
	std::vector<std::vector<GLint>> mUniformLocs;
};
