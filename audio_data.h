#pragma once

// buffer has 4 * 1024 elements
struct audio_data {
	float* audio_l;
	float* audio_r;
	float* freq_l;
	float* freq_r;
	// float freq_history_l[];
	// float freq_history_r[];
	std::string source;
	bool thread_join;
	pthread_t thread;
};

void* audioThreadMain(void* data);
void getPulseDefaultSink(void* data);

