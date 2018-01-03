#pragma once

#include <vector>
#include <string>
#include <filesystem>
namespace filesys = std::experimental::filesystem;

#include "gl.h"
#include "ShaderConfig.h"

class ShaderPrograms {
public:
	ShaderPrograms(ShaderConfig& config, filesys::path shader_folder, bool& is_ok);
	~ShaderPrograms();
	void use_program(int i) const;
private:
	bool compile_shader(const GLchar* s, GLuint& sn, GLenum stype);
	bool link_program(GLuint& pn, GLuint& vs, GLuint& gs, GLuint fs);
	bool compile_buffer_shaders(filesys::path shader_folder, std::string buff_name);

	std::vector<GLuint> mPrograms;
};
