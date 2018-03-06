#pragma once

#include "AudioStream.h"

#include "audio_data.h"
#include "pulse_misc.h"

class LinuxAudioStream : public AudioStream {
public:
	LinuxAudioStream();
	~LinuxAudioStream();
	void get_next_pcm(float* buff_l, float* buff_r, int buff_size);
	void get_sample_rate();

private:
	const int sample_rate = 48000;
	const int channels = 2;

	float* buf_interlaced;
	int pulseError;
	pa_simple* pulseState;
};
