#pragma once

#define BUFFSIZE 2048
struct audio_data {
	float audio_r[BUFFSIZE];
	float audio_l[BUFFSIZE];
	float freq_r[BUFFSIZE];
	float freq_l[BUFFSIZE];
	std::string source;
	bool thread_join;
	pthread_t thread;
};

void* audioThreadMain(void* data);
void getPulseDefaultSink(void* data);

