#pragma once

#include "AudioStream.h"
#include "pulse_misc.h"

class LinuxAudioStream : public AudioStream {
public:
	LinuxAudioStream();
	~LinuxAudioStream();
	void get_next_pcm(float* buff_l, float* buff_r, int size);
	int get_sample_rate();
	int get_max_buff_size();

private:
	const int sample_rate = 48000;
	const int max_buff_size = 512;
	const int channels = 2;

	float* buf_interlaced;
	int pulseError;
	pa_simple* pulseState;
};
