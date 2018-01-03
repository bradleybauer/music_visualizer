#pragma once

#include "gl.h"
#include "ShaderConfig.h"

class ShaderPrograms {
public:
    ShaderPrograms(ShaderConfig& config, bool& is_ok);
    ~ShaderPrograms();
    void use_program(int i) const;
private:
    std::vector<GLuint> programs;
};
