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

  const int num_builtin_uniforms;
  const int num_builtin_samplers;
  int num_user_samplers;
  int num_user_uniforms;

  // Shader header
  //    #version 330
  //    builtin uniforms [0,n]
  //    builtin samplers [n+1,m]
  //    user buffer samplers [num_uu, num_bs]
  //    user uniforms [m+1, num_uu] // append as constant vectors/floats?

  // Layout the uniforms to have explicit uniform locations so that we do not need to keep
  // a list of uniform locations.
};
