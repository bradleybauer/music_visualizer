#include "AudioStream.h"

#include "filesystem.h"

class WavAudioStream : public AudioStream {
public:
	WavAudioStream(const filesys::path &wav_path, bool &is_ok);
	~WavAudioStream();
	void get_next_pcm(float* buff_l, float* buff_r, int buff_size);
	int get_sample_rate();
private:
	int sample_rate;
	short* buf_interlaced;
};
