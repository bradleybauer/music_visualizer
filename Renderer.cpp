#include "Renderer.h"
#include <chrono>
using std::cout;
using std::endl;

// TODO Test the output of the shaders. Use dummy data in audio_data. Compute similarity between expected images
// and produced images.

Renderer::Renderer(ShaderConfig& config, const ShaderPrograms& shaders, Window& window)
	: config(config), shaders(shaders), window(window), frame_counter(0), num_user_buffers(0), buffers_last_drawn(config.mBuffers.size(), 0)
{
	if (config.mBlend) {
		// I chose the following blending func because it allows the user to completely
		// replace the colors in the buffer by setting their output alpha to 1.
		// unfortunately it forces the user to make one of three choices:
		// 1) replace color in the framebuffer
		// 2) leave framebuffer unchanged
		// 3) mix new color with old color
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // mix(old_color.rgb, new_color.rgb, new_color_alpha)
	}
	else {
		glDisable(GL_BLEND);
	}

	num_user_buffers = config.mBuffers.size();

	// Create framebuffers and textures
	for (int i = 0; i < num_user_buffers; ++i) {
		GLuint tex1, tex2, fbo;

		const Buffer& buff = config.mBuffers[i];
		glGenTextures(1, &tex1);
		glActiveTexture(GL_TEXTURE0 + 2*i);
		glBindTexture(GL_TEXTURE_2D, tex1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, buff.width, buff.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glGenTextures(1, &tex2);
		glActiveTexture(GL_TEXTURE0 + 2*i+1);
		glGenTextures(1, &tex2);
		glBindTexture(GL_TEXTURE_2D, tex2);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, buff.width, buff.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glGenFramebuffers(1, &fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex2, 0);

		texs.push_back(tex1);
		texs.push_back(tex2);
		fbos.push_back(fbo);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	start_time = std::chrono::steady_clock::now();
}

Renderer::~Renderer() {
	// revert opengl state

	glDeleteFramebuffers(num_user_buffers, fbos.data());
	// The default window's fbo is now bound

	glDeleteTextures(2 * num_user_buffers, texs.data());
	// Textures in texs are unboud from their targets
}

void Renderer::render(audio_data * audio_source) {
	auto now = std::chrono::steady_clock::now();
	elapsed_time = (now - start_time).count() / 1e9f;

	// Update audio textures
	// glActivateTexture activates a certain texture unit.
	// each texture unit holds one texture of each dimension of texture
	//     {GL_TEXTURE_1D, GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBEMAP}
	// because I'm using four 1D textures I need to store them in separate texture units
	//
	// glUniform1i(textureLoc, int) sets what texture unit the sampler in the shader reads from
	//
	// A texture binding created with glBindTexture remains active until a different texture is
	// bound to the same target (in the active unit? I think), or until the bound texture is deleted
	// with glDeleteTextures. So I do not need to rebind
	// glBindTexture(GL_TEXTURE_1D, tex[X]);
	audio_source->mtx.lock();
	glActiveTexture(GL_TEXTURE0 + 0);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, VISUALIZER_BUFSIZE, GL_RED, GL_FLOAT, audio_source->audio_r);
	glActiveTexture(GL_TEXTURE0 + 1);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, VISUALIZER_BUFSIZE, GL_RED, GL_FLOAT, audio_source->audio_l);
	glActiveTexture(GL_TEXTURE0 + 2);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, VISUALIZER_BUFSIZE, GL_RED, GL_FLOAT, audio_source->freq_r);
	glActiveTexture(GL_TEXTURE0 + 3);
	glTexSubImage1D(GL_TEXTURE_1D, 0, 0, VISUALIZER_BUFSIZE, GL_RED, GL_FLOAT, audio_source->freq_l);
	audio_source->mtx.unlock();

	if (window.size_changed) {
		window.size_changed = false;
		// Resize textures for window sized buffers
		for (int i = 0; i < config.mBuffers.size(); ++i) {
			Buffer& buff = config.mBuffers[i];
			if (buff.is_window_size) {
				buff.width = window.width;
				buff.height = window.height;
			}
			glActiveTexture(GL_TEXTURE0 + 2*i);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, buff.width, buff.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glActiveTexture(GL_TEXTURE0 + 2*i+1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, buff.width, buff.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		}
		frame_counter = 0;
		this->start_time = now;
	}

	// Render user buffers
	for (const int r : config.mRender_order) {
		const Buffer& buff = config.mBuffers[r];
		shaders.use_program(r);
		upload_uniforms(buff);
		glBindFramebuffer(GL_FRAMEBUFFER, fbos[r]);
		glDrawBuffer(GL_COLOR_ATTACHMENT0 + (buffers_last_drawn[r] + 1) % 2);
		glViewport(0, 0, buff.width, buff.height);
		glClearColor(buff.clear_color[0], buff.clear_color[1], buff.clear_color[2], 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_POINTS, 0, buff.geom_iters);
		buffers_last_drawn[r] += 1;
		buffers_last_drawn[r] %= 2;
	}

	// Render image
	shaders.use_program(config.mBuffers.size());
	const Buffer& buff = config.mImage;
	upload_uniforms(config.mImage);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, window.width, window.height);
	glClearColor(buff.clear_color[0], buff.clear_color[1], buff.clear_color[2], 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_POINTS, 0, buff.geom_iters);
	frame_counter++;
}

void Renderer::upload_uniforms(const Buffer& buff) const {
	// Upload builtin uniforms
	glUniform2f(0, window.mouse.x, window.mouse.y);
	glUniform1i(1, window.mouse.down);
	glUniform2f(2, window.width, window.height);
	glUniform1f(3, elapsed_time);
	glUniform1i(4, frame_counter);
	glUniform1f(5, float(buff.geom_iters));
	int uniform_offset = 5;
	for (int i = 0; i < 4; ++i) // Point samplers to texture units
		glUniform1i(uniform_offset + i+1, i);
	uniform_offset = ShaderPrograms::num_builtin_uniforms;

	// User samplers
	for (int i = 0; i < config.mBuffers.size(); ++i) // Point samplers to texture units
		glUniform1i(uniform_offset + i+1, 2*i + buffers_last_drawn[i]);
	uniform_offset += config.mBuffers.size();

	// Users uniforms
	for (int i = 0; i < config.mUniforms.size(); ++i) {
		const std::vector<float>& uv = config.mUniforms[i].values;
		switch (uv.size()) {
		case 1:
			glUniform1f(uniform_offset + i+1, uv[0]);
			break;
		case 2:
			glUniform2f(uniform_offset + i+1, uv[0], uv[1]);
			break;
		case 3:
			glUniform3f(uniform_offset + i+1, uv[0], uv[1], uv[2]);
			break;
		case 4:
			glUniform4f(uniform_offset + i+1, uv[0], uv[1], uv[2], uv[3]);
			break;
		}
	}
}
