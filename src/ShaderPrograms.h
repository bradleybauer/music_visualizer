#pragma once

#include <vector>
#include <string>
#include <functional>
#include "filesystem.h"

#include <GL/glew.h>
#include "ShaderConfig.h"
#include "Renderer.h"
#include "Window.h"

// Programs
// program for buffer n is in mPrograms[n]
// program for image shader is in mPrograms.back()

class ShaderPrograms {
public:
	ShaderPrograms(const ShaderConfig& config,
        const Renderer& renderer,
        const Window& window,
        const filesys::path& shader_folder);
	ShaderPrograms& operator=(ShaderPrograms&&);
	~ShaderPrograms();

	void use_program(int i) const;
    //void upload_uniforms(const Buffer& buff, const int buff_index) const;
	GLint get_uniform_loc(int program_i, int uniform_i) const;

    struct uniform_info {
        std::string type;
        std::string name;
        std::function<void(const int, const Buffer&)> update;
    };
    std::vector<uniform_info> builtin_uniforms;

private:
	ShaderPrograms(ShaderPrograms&) = delete;
	ShaderPrograms(ShaderPrograms&&) = delete;
	ShaderPrograms& operator=(ShaderPrograms&) = delete;

	bool compile_shader(const GLchar* s, GLuint& sn, GLenum stype);
	bool link_program(GLuint& pn, GLuint vs, GLuint gs, GLuint fs);
	void compile_buffer_shaders(const filesys::path& shader_folder, const std::string& buff_name, const std::string& uniform_header, const bool uses_default_geometry_shader);

	std::vector<GLuint> mPrograms;
	std::vector<std::vector<GLint>> mUniformLocs;
};
