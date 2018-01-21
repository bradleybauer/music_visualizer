#pragma once

void audio_setup(const int ABL, const int SR);

void get_pcm(float* audio_buf_l, float* audio_buf_r, int ABL);

void audio_destroy();
