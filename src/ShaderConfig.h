#pragma once

#include <string>
#include <vector>
#include <array>
#include "filesystem.h"

struct Buffer {
    std::string name;
    int width = 0;
    int height = 0;
    bool is_window_size = true;
    bool uses_default_geometry_shader = true;
    int geom_iters = 1;
    std::array<float, 3> clear_color;
    // Enables building w/ g++-5
    Buffer() { clear_color = {0}; }
};

struct Uniform {
	std::string name;
	std::vector<float> values;
};

struct AudioOptions {
	bool fft_sync = true;
	bool xcorr_sync = true;
    float fft_smooth = 1.f;
	float wave_smooth = .8f;
};

class ShaderConfig {
public:
    ShaderConfig(const filesys::path& shader_folder, const filesys::path& conf_file_path);
	ShaderConfig(const std::string &json_str); // used in testing

	struct {
		int width = 400;
		int height = 300;
	} mInitWinSize;
    bool mSimpleMode;
	bool mBlend = false;
	bool mAudio_enabled = true;
	Buffer mImage;
	std::vector<Buffer> mBuffers;
	std::vector<int> mRender_order; // render_order[n] is an index into buffers
	std::vector<Uniform> mUniforms;
	AudioOptions mAudio_ops;

#ifdef TEST
	ShaderConfig() {}; // For generating mock instances
#endif

private:
    void parse_config_from_string(const std::string&);
    void parse_simple_config(const filesys::path& shader_folder);
};
