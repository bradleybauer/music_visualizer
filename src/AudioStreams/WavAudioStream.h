#include "AudioStream.h"

#ifdef WINDOWS
#include <filesystem>
namespace filesys = std::filesystem;
#else
#include <experimental/filesystem>
namespace filesys = std::experimental::filesystem;
#endif

class WavAudioStream : public AudioStream {
public:
	WavAudioStream(const filesys::path & wav_path);
	~WavAudioStream();
	void get_next_pcm(float* buff_l, float* buff_r, int buff_size);
	int get_sample_rate();
	int get_max_buff_size();
private:
	int sample_rate;
	const int max_buff_size = 512;
	short* buf_interlaced;
};
