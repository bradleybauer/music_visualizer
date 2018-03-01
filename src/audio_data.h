#pragma once

#include <mutex>
const int VISUALIZER_BUFSIZE = 1024;
struct audio_data {
	float* audio_l;
	float* audio_r;
	float* freq_l;
	float* freq_r;
	bool thread_join;
	std::mutex mtx;
};
