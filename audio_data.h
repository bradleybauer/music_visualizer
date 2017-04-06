#pragma once

const int VISUALIZER_BUFSIZE = 2048;
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
	pthread_mutex_t mut;
	pthread_cond_t cond;
};

void* audioThreadMain(void* data);
void getPulseDefaultSink(void* data);

