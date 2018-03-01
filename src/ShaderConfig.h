#pragma once

#include <string>
#include <vector>
#include <array>

#include "filesystem.h"

struct Buffer {
	std::string name;
	int width;
	int height;
	bool is_window_size;
	int geom_iters;
	std::array<float, 3> clear_color;
};

struct Uniform {
	std::string name;
	std::vector<float> values;
};

struct AudioOptions {
	bool fft_sync;
	bool diff_sync;
	float fft_smooth;
	float wave_smooth;
};

class ShaderConfig {
public:
	ShaderConfig(filesys::path conf_file, bool& is_ok);
	ShaderConfig(std::string json_str, bool& is_ok);

	struct {
		int width;
		int height;
	} mInitWinSize;
	bool mBlend;
	Buffer mImage;
	std::vector<Buffer> mBuffers;
	std::vector<int> mRender_order; // render_order[n] is an index into buffers
	std::vector<Uniform> mUniforms;
	AudioOptions mAudio_ops;

#ifdef TEST
	ShaderConfig() {}; // For generating mock instances
#endif

};
