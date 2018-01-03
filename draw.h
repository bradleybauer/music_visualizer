#pragma once

bool initialize_gl(std::string shader_folder);
void deinit_drawing();
void draw(struct audio_data*);
bool should_loop();
