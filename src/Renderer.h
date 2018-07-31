#pragma once

#include <vector>

#include "ShaderConfig.h"
#include "Window.h"

#include "AudioProcess.h"

class ShaderPrograms;

class Renderer {
    friend class ShaderPrograms; // for uploading uniform values
public:
	Renderer(const ShaderConfig& config, const Window& window);
	Renderer& operator=(Renderer&& o);
	~Renderer();

	void update(AudioData &data);
	void update();
	void render();
    void set_programs(const ShaderPrograms* shaders);

private:
	Renderer(Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer& o) = delete;

	const ShaderConfig& config;
	const ShaderPrograms* shaders;
	const Window& window;

	void upload_uniforms(const Buffer& buff, const int buff_index) const;

	std::chrono::steady_clock::time_point start_time;
	float elapsed_time;

	int frame_counter;
	int num_user_buffers;
	std::vector<int> buffers_last_drawn;
	std::vector<GLuint> fbos; // n * num_user_buffs
	std::vector<GLuint> fbo_textures; // 2n * num_user_buffs
	std::vector<GLuint> audio_textures; // 2n * num_user_buffs
};

#include "ShaderPrograms.h"
