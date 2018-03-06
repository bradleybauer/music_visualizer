#pragma once

class AudioStream {
public:
	virtual void get_next_pcm(float* buff_l, float* buff_r, int buff_size) = 0;
	virtual int get_sample_rate() = 0;
	virtual int get_max_buff_size() = 0;
};
