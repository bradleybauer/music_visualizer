#pragma once

#include <vector>

#include "Window.h"
#include "ShaderConfig.h"
#include "ShaderPrograms.h"

#include "AudioProcess.h"

class Renderer {
public:
	Renderer(const ShaderConfig& config, const ShaderPrograms& shaders, const Window& window);
	Renderer& operator=(Renderer&& o);
	~Renderer();

	void update(AudioData &data);
	void update();
	void render();

private:
	Renderer(Renderer&) = delete;
	Renderer(Renderer&&) = delete;
	Renderer& operator=(Renderer& o) = delete;

	const ShaderConfig& config;
	const ShaderPrograms& shaders;
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
