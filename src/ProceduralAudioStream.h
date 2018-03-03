#pragma once

#include <functional>
#include "AudioStream.h"

class ProceduralAudioStream : public AudioStream {
public:
	ProceduralAudioStream(std::function<void(float*, float*, int)> lambda) : lambda(lambda) {};
	void get_next_pcm(float* buff_l, float* buff_r, int buff_size) {
		lambda(buff_l, buff_r, buff_size);
	}
	int get_sample_rate() {
		return sample_rate;
	};
private:
	const int sample_rate = 48000;
	std::function<void(float*, float*, int)> lambda;
};
