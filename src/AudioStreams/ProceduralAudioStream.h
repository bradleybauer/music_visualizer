#pragma once

#include <iostream>
#include <functional>
#include <limits>
#include "AudioStream.h"

// max is a define in winmindef.h
#ifdef max
#undef max
#endif

class ProceduralAudioStream : public AudioStream {
public:
	ProceduralAudioStream(std::function<void(float*, float*, int)> lambda) : lambda(lambda) {}
	void get_next_pcm(float* buff_l, float* buff_r, int buff_size) {
		lambda(buff_l, buff_r, buff_size);
	}
	int get_sample_rate() {
		return sample_rate;
	};
	int get_max_buff_size() {
		return max_buff_size;
	}
private:
	const int sample_rate = 48000;
	int max_buff_size = std::numeric_limits<int>::max();
	std::function<void(float*, float*, int)> lambda;
};
