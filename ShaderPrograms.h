#pragma once

#include <vector>
#include <string>
#include "filesystem.h"
namespace filesys = std::experimental::filesystem;

#include <GL/glew.h>
#include "ShaderConfig.h"

// Uniform Locations
// location 0: vec2      iMouse
// location 1: bool      iMouseDown
// location 2: vec2      iRes
// location 3: float     iTime
// location 4: int       iFrame
// location 5: float     iNumGeomIters
// location 6: sampler1D iSoundR   texture_unit 0
// location 7: sampler1D iSoundL   texture_unit 1
// location 8: sampler1D iFreqR    texture_unit 2
// location 9: sampler1D iFreqL    texture_unit 3
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
	ShaderPrograms(const ShaderConfig& config, filesys::path shader_folder, bool& is_ok);
	ShaderPrograms& operator=(ShaderPrograms&&);
	~ShaderPrograms();

	void use_program(int i) const;

private:
	ShaderPrograms(ShaderPrograms&) = delete;
	ShaderPrograms(ShaderPrograms&&) = delete;
	ShaderPrograms& operator=(ShaderPrograms&) = delete;

	bool compile_shader(const GLchar* s, GLuint& sn, GLenum stype);
	bool link_program(GLuint& pn, GLuint vs, GLuint gs, GLuint fs);
	bool compile_buffer_shaders(const filesys::path& shader_folder, const std::string& buff_name, const std::string& uniform_header);

	std::vector<GLuint> mPrograms;
};
