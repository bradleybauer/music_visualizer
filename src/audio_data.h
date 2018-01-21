#pragma once

#include <mutex>
const int VISUALIZER_BUFSIZE = 1024;
struct audio_data {
	float* audio_l;
	float* audio_r;
	float* freq_l;
	float* freq_r;
	// float freq_history_l[];
	// float freq_history_r[];
	std::string source;
	bool thread_join;
	std::mutex mtx;
};
