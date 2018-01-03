#pragma once

#include "Window.h"
#include "ShaderConfig.h"
#include "ShaderPrograms.h"

class Renderer {
friend class Window;

public:
  Renderer(ShaderConfig& config, ShaderPrograms& shaders);
  ~Renderer();
  void render(struct audio_data*) const;

private:
  ShaderConfig config;
  ShaderPrograms shaders;

  // std::array<float, 3> mouse_data;
  // Time
  // Resolution
	// Frame (unsigned int ?)
	//

  const int num_builtin_uniforms;
  const int num_builtin_samplers;
  int num_user_samplers;
  int num_user_uniforms;
};
