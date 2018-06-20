#include <iostream>
using std::cout;
using std::endl;
#include <string>
using std::string;
#include <stdexcept>
using std::runtime_error;

#define CATCH_CONFIG_MAIN
#include "catch2/catch.hpp"

#define TEST
#include "ShaderConfig.h"

// convenience functions to print state and compare parsed state to mock state
static std::ostream& operator<<(std::ostream& os, const Buffer& o);
static std::ostream& operator<<(std::ostream& os, const AudioOptions& o);
static std::ostream& operator<<(std::ostream& os, const Uniform& o);
static std::ostream& operator<<(std::ostream& os, const ShaderConfig& o);
static bool operator==(const AudioOptions& l, const AudioOptions& o);
static bool operator==(const Buffer& l, const Buffer& o);
static bool operator==(const Uniform& l, const Uniform& o);
static bool compare_eq(ShaderConfig& l, ShaderConfig& r);

TEST_CASE("non existant buffer in render_order") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["mybuff", "i_dont_exist"],
		"uniforms" : {
			"my_uni": [1.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("uniforms with the same name") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"my_uni": [1.0],
			"my_uni": [1.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("more than one render_order object") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["mybuff"],
		"render_order":["mybuff"],
		"uniforms" : {
			"my_uni": [1.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("more than one uniforms object") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"my_uni": [1.0]
		},
		"uniforms" : {
			"my_uni": [1.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("buffers with the same name") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			},
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"my_uni": [1.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("incorrect uniform value 1") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"this_is_my_uni": "geom_iters"
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("incorrect uniform value 2") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 4.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("fft_smooth out of range") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1.2,
			"wave_smooth": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("too many clear color values") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1, 0]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("empty buffer name") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["MyBuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("incorrect buffer.size 1") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"MyBuff": {
				"size":"win_size",
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["MyBuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("incorrect buffer.size 2") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"MyBuff": {
				"size":[12],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled": true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["MyBuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("missing buffer.size") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers": {
			"MyBuff": {
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": false,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order":["MyBuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("missing fft_sync option") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"audio_options":{
			"audio_enabled":true,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	try {
		ShaderConfig conf(json_str);
	}
	catch (runtime_error& msg) {
		CHECK(true);
		return;
	}
	CHECK(false);
}
TEST_CASE("test valid config 0") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers":{
			"x":{
				"size":[1,2],
				"clear_color":[0.1,0.2,0.3],
				"geom_iters":3,
			},
			"y":{
				"size":[2,1],
				"clear_color":[0.1,0.2,0.3],
				"geom_iters":3,
			},
			"asdf":{
				"size":[1,2],
				"clear_color":[0.1,0.2,0.3],
				"geom_iters":3,
			},
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": true,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"render_order": ["x", "y", "x"]
	}
	)";

	ShaderConfig mock_conf;
	mock_conf.mInitWinSize.width = 400;
	mock_conf.mInitWinSize.height = 400;
	mock_conf.mBlend = false;
	mock_conf.mAudio_enabled = true;
	mock_conf.mAudio_ops.fft_sync = true;
	mock_conf.mAudio_ops.diff_sync = false;
	mock_conf.mAudio_ops.fft_smooth = 1.f;
	mock_conf.mAudio_ops.wave_smooth = 0.f;
	Buffer b;
	b.name = string("x");
	b.geom_iters = 3;
	b.width = 1;
	b.height = 2;
	b.is_window_size = false;
	b.clear_color = {.1f, .2f, .3f};
	mock_conf.mBuffers.push_back(b);
	b.name = string("y");
	b.geom_iters = 3;
	b.width = 2;
	b.height = 1;
	b.is_window_size = false;
	b.clear_color = {.1f, .2f, .3f};
	mock_conf.mBuffers.push_back(b);
	mock_conf.mRender_order = {0,1,0};
	mock_conf.mImage.is_window_size = true;
	mock_conf.mImage.geom_iters = 1;
	mock_conf.mImage.clear_color = {0.f, 0.f, 0.f};

	try {
		ShaderConfig conf(json_str);
		if (!compare_eq(conf, mock_conf)) {
			cout << "EXPECTED" << endl;
			cout << mock_conf << endl << endl;
			cout << "RETURNED" << endl;
			cout << conf << endl << endl;
			throw runtime_error("mock neq parsed");
		}
	}
	catch (runtime_error& msg) {
		CHECK(false);
		return;
	}
	CHECK(true);
}
TEST_CASE("test valid config 1") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": true,
			"diff_sync": false,
			"fft_smooth": 1,
			"wave_smooth": 0
		},
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	ShaderConfig mock_conf;
	mock_conf.mInitWinSize.width = 400;
	mock_conf.mInitWinSize.height = 400;
	mock_conf.mBlend = false;
	mock_conf.mAudio_enabled = true;
	mock_conf.mAudio_ops.fft_sync = true;
	mock_conf.mAudio_ops.diff_sync = false;
	mock_conf.mAudio_ops.fft_smooth = 1.f;
	mock_conf.mAudio_ops.wave_smooth = 0.f;
	Uniform u;
	u.name = string("this_is_my_uni");
	u.values = { 1.f, 2.f, 3.f, 4.f };
	mock_conf.mUniforms.push_back(u);
	mock_conf.mImage.geom_iters = 1;
	mock_conf.mImage.clear_color = {0.f, 0.f, 0.f};
	mock_conf.mImage.is_window_size = true;

	try {
		ShaderConfig conf(json_str);
		if (!compare_eq(conf, mock_conf)) {
			cout << "EXPECTED" << endl;
			cout << mock_conf << endl << endl;
			cout << "RETURNED" << endl;
			cout << conf << endl << endl;
			throw runtime_error("mock neq parsed");
		}
	}
	catch (runtime_error& msg) {
		CHECK(false);
		return;
	}
	CHECK(true);
}
TEST_CASE("test valid config 2") {
	string json_str = R"(
	{
		"initial_window_size": [200,200],
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers":{
			"MyBuff": {
				"size": "window_size",
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": true,
			"diff_sync": false,
			"fft_smooth": 0.0,
			"wave_smooth": 0.1
		},
		"render_order":["MyBuff"]
	}
	)";
	ShaderConfig mock_conf;
	mock_conf.mInitWinSize.width = 200;
	mock_conf.mInitWinSize.height = 200;
	mock_conf.mBlend = false;
	mock_conf.mAudio_enabled = true;
	mock_conf.mAudio_ops.fft_sync = true;
	mock_conf.mAudio_ops.diff_sync = false;
	mock_conf.mAudio_ops.fft_smooth = .0f;
	mock_conf.mAudio_ops.wave_smooth = .1f;
	Buffer b;
	b.name = string("MyBuff");
	b.geom_iters = 12;
	b.width = 0;
	b.height = 0;
	b.is_window_size = true;
	b.clear_color = {1.f, 1.f, 1.f};
	mock_conf.mBuffers.push_back(b);
	mock_conf.mRender_order = {0};
	mock_conf.mImage.is_window_size = true;
	mock_conf.mImage.geom_iters = 1;
	mock_conf.mImage.clear_color = {0.f, 0.f, 0.f};

	try {
		ShaderConfig conf(json_str);
		if (!compare_eq(conf, mock_conf)) {
			cout << "EXPECTED" << endl;
			cout << mock_conf << endl << endl;
			cout << "RETURNED" << endl;
			cout << conf << endl << endl;
			throw runtime_error("mock neq parsed");
		}
	}
	catch (runtime_error& msg) {
		CHECK(false);
		return;
	}
	CHECK(true);
}
TEST_CASE("test valid config 3") {
	string json_str = R"(
	{
		"image" : {
			"geom_iters":1,
			"clear_color":[0,0,0]
		},
		"buffers":{
			"A": {
				"size": [100, 200],
				"geom_iters": 1,
				"clear_color":[1, 1, 1]
			},
			"B": {
				"size": [200, 200],
				"geom_iters": 2,
				"clear_color":[0, 0.5, 0]
			}
		},
		"audio_options":{
			"audio_enabled":true,
			"fft_sync": true,
			"diff_sync": true,
			"fft_smooth": 0.6,
			"wave_smooth": 0.5
		},
		"render_order":["A", "B", "A", "B"]
	}
	)";
	ShaderConfig mock_conf;
	mock_conf.mInitWinSize.width = 400;
	mock_conf.mInitWinSize.height = 400;
	mock_conf.mBlend = false;
	mock_conf.mAudio_enabled = true;
	mock_conf.mAudio_ops.fft_sync = true;
	mock_conf.mAudio_ops.diff_sync = true;
	mock_conf.mAudio_ops.fft_smooth = .6f;
	mock_conf.mAudio_ops.wave_smooth = .5f;

	Buffer b;
	b.name = string("A");
	b.geom_iters = 1;
	b.width = 100;
	b.height = 200;
	b.is_window_size = false;
	b.clear_color = {1.f, 1.f, 1.f};
	mock_conf.mBuffers.push_back(b);

	b.name = string("B");
	b.geom_iters = 2;
	b.width = 200;
	b.height = 200;
	b.is_window_size = false;
	b.clear_color = {0.f, .5f, 0.f};
	mock_conf.mBuffers.push_back(b);
	mock_conf.mRender_order = {0,1,0,1};
	mock_conf.mImage.is_window_size = true;
	mock_conf.mImage.geom_iters = 1;
	mock_conf.mImage.clear_color = {0.f, 0.f, 0.f};

	try {
		ShaderConfig conf(json_str);
		if (!compare_eq(conf, mock_conf)) {
			cout << "EXPECTED" << endl;
			cout << mock_conf << endl << endl;
			cout << "RETURNED" << endl;
			cout << conf << endl << endl;
			throw runtime_error("mock neq parsed");
		}
	}
	catch (runtime_error& msg) {
		CHECK(false);
		return;
	}
	CHECK(true);
}

// Convenience operators

static std::ostream& operator<<(std::ostream& os, const Buffer& o) {
	os << std::boolalpha;
	os << "name   : " << o.name << "\n";
	os << "width  : " << o.width << "\n";
	os << "height : " << o.height << "\n";
	os << "is_window_size: " << o.is_window_size << "\n";
	return os;
}

static std::ostream& operator<<(std::ostream& os, const AudioOptions& o) {
	os << std::boolalpha;
	os << "diff_sync   : " << o.diff_sync << "\n";
	os << "fft_sync    : " << o.fft_sync << "\n";
	os << "wave_smooth : " << o.wave_smooth << "\n";
	os << "fft_smooth  : " << o.fft_smooth << "\n";
	return os;
}

static std::ostream& operator<<(std::ostream& os, const Uniform& o) {
	os << "name	 : " << o.name << "\n";
	os << "values = {";
	for (int i = 0; i < o.values.size(); ++i)
		os << o.values[i] << ", ";
	os << "}";
	return os;
}

static std::ostream& operator<<(std::ostream& os, const ShaderConfig& o) {
	os << "image buffer {\n";
	os << o.mImage << "\n";
	os << "}\n";

	os << "audio options {\n";
	os << o.mAudio_ops << "\n";
	os << "}\n";

	os << "user buffers {\n";
	for (int i = 0; i < o.mBuffers.size(); ++i)
		os << o.mBuffers[i];
	os << "}\n";

	for (int i = 0; i < o.mRender_order.size(); ++i)
		os << o.mRender_order[i] << ", ";
	os << "\n";

	os << "uniforms {\n";
	for (int i = 0; i < o.mUniforms.size(); ++i)
		os << o.mUniforms[i];
	os << "\n";

	os << std::boolalpha << "Blend: " << o.mBlend << "\n";

	return os;
}

static bool operator==(const AudioOptions& l, const AudioOptions& o) {
	return l.fft_sync == o.fft_sync &&
		l.diff_sync == o.diff_sync &&
		l.fft_smooth == o.fft_smooth &&
		l.wave_smooth == o.wave_smooth;
}

static bool operator==(const Buffer& l, const Buffer& o) {
	return l.name == o.name &&
		l.width == o.width &&
		l.height == o.height &&
		l.is_window_size == o.is_window_size &&
		l.geom_iters == o.geom_iters &&
		l.clear_color == o.clear_color;
}

static bool operator==(const Uniform& l, const Uniform& o) {
	return l.name == o.name &&
		l.values == o.values;
}

static bool compare_eq(ShaderConfig& l, ShaderConfig& r) {
	bool confs_eq, eq;

	eq = l.mAudio_ops == r.mAudio_ops;
	confs_eq = eq;
	if (!eq) cout << "AudioOptions difference" << endl;

	eq = l.mRender_order == r.mRender_order;
	confs_eq &= eq;
	if (!eq) cout << "render_order difference" << endl;

	eq = l.mBuffers.size() == r.mBuffers.size();
	confs_eq &= eq;
	if (!eq) cout << "configs specify different number of buffers" << endl;

	if (eq)
		for (int i = 0; i < l.mBuffers.size(); ++i) {
			eq = l.mBuffers[i] == r.mBuffers[i];
			confs_eq &= eq;
			if (!eq) cout << "Buffer " + std::to_string(i) + " difference" << endl;
		}

	eq = l.mImage == r.mImage;
	confs_eq &= eq;
	if (!eq) cout << "image settings differ" << endl;

	eq = l.mUniforms.size() == r.mUniforms.size();
	confs_eq &= eq;
	if (!eq) cout << "configs specify different number of uniforms" << endl;

	if (eq)
		for (int i = 0; i < l.mUniforms.size(); ++i) {
			eq = l.mUniforms[i] == r.mUniforms[i];
			confs_eq &= eq;
			if (!eq) cout << "Uniform " + std::to_string(i) + " difference" << endl;
		}

	eq = l.mBlend == r.mBlend;
	confs_eq &= eq;
	if (!eq) cout << "blend is different" << endl;

	eq = (l.mInitWinSize.width == r.mInitWinSize.width) && (l.mInitWinSize.height == r.mInitWinSize.height);
	confs_eq &= eq;
	if (!eq) cout << "mInitWinSize differs" << endl;

	return confs_eq;
}
