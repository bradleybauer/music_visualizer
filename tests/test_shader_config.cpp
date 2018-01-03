#include <iostream>
#include <string>
using std::cout;
using std::endl;
using std::string;

// TODO should probably use a testing frame work
// or think of a better way to add tests to execute. I keep forgetting things when I add new tests.

#include "Test.h"
#include "ShaderConfig.h"

std::ostream& operator<<(std::ostream& os, const Buffer& o);
std::ostream& operator<<(std::ostream& os, const AudioOptions& o);
std::ostream& operator<<(std::ostream& os, const Uniform& o);
std::ostream& operator<<(std::ostream& os, const ShaderConfig& o);
bool operator==(const AudioOptions& l, const AudioOptions& o);
bool operator==(const Buffer& l, const Buffer& o);
bool operator==(const Uniform& l, const Uniform& o);

static bool compare(ShaderConfig& l, ShaderConfig& r) {
	bool confs_eq, eq;

	eq = l.mAudio_ops == r.mAudio_ops;
	confs_eq = eq;
	if (!eq) cout << "AudioOptions difference" << endl;

	eq = l.mRender_order == r.mRender_order;
	confs_eq &= eq;
	if (!eq) cout << "render_order difference" << endl;

	eq = l.mBuffers.size() == r.mBuffers.size();
	confs_eq &= eq;
	if(!eq) cout << "buffers.size difference" << endl;

	if (eq)
		for (int i = 0; i < l.mBuffers.size(); ++i) {
			eq = l.mBuffers[i] == r.mBuffers[i]; 
			confs_eq &= eq;
			if (!eq) cout << "Buffer " + std::to_string(i) + " difference" << endl;
		}

	eq = l.mUniforms.size() == r.mUniforms.size();
	confs_eq &= eq;
	if (!eq) cout << "uniforms.size difference" << endl;

	if (eq)
		for (int i = 0; i < l.mUniforms.size(); ++i) {
			eq = l.mUniforms[i] == r.mUniforms[i];
			confs_eq &= eq;
			if (!eq) cout << "Uniform " + std::to_string(i) + " difference" << endl;
		}

	return confs_eq;
}

bool ShaderConfigTest::parse_invalid11() { // incorrect uniform value
	string json_str = R"(
	{
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"FFT_SYNC": false,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"this_is_my_uni": "geom_iters"
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_invalid10() { // incorrect uniform value
	string json_str = R"(
	{
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"FFT_SYNC": false,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 4.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_invalid9() { // FFT_SMOOTH out of range
	string json_str = R"(
	{
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"FFT_SYNC": false,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1.2,
			"WAVE_SMOOTH": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_invalid8() { // too many clear color values
	string json_str = R"(
	{
		"buffers": {
			"mybuff": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1, 0]
			}
		},
		"audio_options":{
			"FFT_SYNC": false,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"render_order":["mybuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_invalid7() { // empty buffer name
	string json_str = R"(
	{
		"buffers": {
			"": {
				"size":[1,2],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"FFT_SYNC": false,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"render_order":["MyBuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_invalid6() { // incorrect buffer.size
	string json_str = R"(
	{
		"buffers": {
			"MyBuff": {
				"size":"win_size",
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"FFT_SYNC": false,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"render_order":["MyBuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_invalid5() { // incorrect buffer.size
	string json_str = R"(
	{
		"buffers": {
			"MyBuff": {
				"size":[12],
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"FFT_SYNC": false,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"render_order":["MyBuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_invalid4() { // missing buffer.size
	string json_str = R"(
	{
		"buffers": {
			"MyBuff": {
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"FFT_SYNC": false,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"render_order":["MyBuff"],
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_invalid3() { // missing render_order
	string json_str = R"(
	{
		"buffers": {
			"MyBuff": {
				"size": "window_size",
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"FFT_SYNC": false,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_invalid2() { // missing FFT_SYNC option
	string json_str = R"(
	{
		"audio_options":{
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_invalid1() { // no audio_options member
	string json_str = R"(
	{
		"audio":{
			"FFT_SYNC": true,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);
	if (is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return !is_ok;
}
bool ShaderConfigTest::parse_valid3() {
	string json_str = R"(
	{
		"audio_options":{
			"FFT_SYNC": true,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 1,
			"WAVE_SMOOTH": 0
		},
		"uniforms" : {
			"this_is_my_uni": [1.0, 2.0, 3.0, 4.0]
		}
	}
	)";

	ShaderConfig mock_conf;
	mock_conf.mAudio_ops.fft_sync = true;
	mock_conf.mAudio_ops.diff_sync = false;
	mock_conf.mAudio_ops.fft_smooth = 1.f;
	mock_conf.mAudio_ops.wave_smooth = 0.f;
	Uniform u;
	u.name = string("this_is_my_uni");
	u.values = { 1.f, 2.f, 3.f, 4.f };
	mock_conf.mUniforms.push_back(u);

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);

	if (!is_ok)
		return false;

	is_ok = compare(conf, mock_conf);
	if (!is_ok) {
		cout << "EXPECTED" << endl;
		cout << mock_conf << endl << endl;
		cout << "RETURNED" << endl;
		cout << conf << endl << endl;
	}

	if (!is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return is_ok;
}

bool ShaderConfigTest::parse_valid2() {
	string json_str = R"(
	{
		"buffers":{
			"MyBuff": {
				"size": "window_size",
				"geom_iters": 12,
				"clear_color":[1, 1, 1]
			}
		},
		"audio_options":{
			"FFT_SYNC": true,
			"DIFF_SYNC": false,
			"FFT_SMOOTH": 0.0,
			"WAVE_SMOOTH": 0.1
		},
		"render_order":["MyBuff"]
	}
	)";
	ShaderConfig mock_conf;
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

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);

	if (!is_ok)
		return false;

	is_ok = compare(conf, mock_conf);
	if (!is_ok) {
		cout << "EXPECTED" << endl;
		cout << mock_conf << endl << endl;
		cout << "RETURNED" << endl;
		cout << conf << endl << endl;
	}

	if (!is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return is_ok;
}

bool ShaderConfigTest::parse_valid1() {
	string json_str = R"(
	{
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
			"FFT_SYNC": true,
			"DIFF_SYNC": true,
			"FFT_SMOOTH": 0.6,
			"WAVE_SMOOTH": 0.5
		},
		"render_order":["A", "B", "A", "B"]
	}
	)";
	ShaderConfig mock_conf;
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

	bool is_ok;
	ShaderConfig conf(json_str, is_ok);

	if (!is_ok)
		return false;

	is_ok = compare(conf, mock_conf);
	if (!is_ok) {
		cout << "EXPECTED" << endl;
		cout << mock_conf << endl << endl;
		cout << "RETURNED" << endl;
		cout << conf << endl << endl;
	}

	if (!is_ok) cout << FAIL_MSG << endl;
	else cout << PASS_MSG << endl;
	return is_ok;
}

bool ShaderConfigTest::test() {
	bool ok;

	cout << "json parse test valid 1: " << endl;
	ok = parse_valid1();

	cout << "json parse test valid 2: " << endl;
	ok &= parse_valid2();

	cout << "json parse test valid 3: " << endl;
	ok &= parse_valid3();

	cout << "json parse test invalid 1: " << endl;
	ok &= parse_invalid1();

	cout << "json parse test invalid 2: " << endl;
	ok &= parse_invalid2();

	cout << "json parse test invalid 3: " << endl;
	ok &= parse_invalid3();

	cout << "json parse test invalid 4: " << endl;
	ok &= parse_invalid4();

	cout << "json parse test invalid 5: " << endl;
	ok &= parse_invalid5();

	cout << "json parse test invalid 6: " << endl;
	ok &= parse_invalid6();

	cout << "json parse test invalid 7: " << endl;
	ok &= parse_invalid7();

	cout << "json parse test invalid 8: " << endl;
	ok &= parse_invalid8();

	cout << "json parse test invalid 9: " << endl;
	ok &= parse_invalid9();

	cout << "json parse test invalid 10: " << endl;
	ok &= parse_invalid10();

	cout << "json parse test invalid 11: " << endl;
	ok &= parse_invalid11();

	return ok;
}

// Convenience operators

std::ostream& operator<<(std::ostream& os, const Buffer& o) {
	os << std::boolalpha;
	os << "name   : " << o.name << std::endl;
	os << "width  : " << o.width << std::endl;
	os << "height : " << o.height << std::endl;
	os << "is_window_size: " << o.is_window_size;
	return os;
}

std::ostream& operator<<(std::ostream& os, const AudioOptions& o) {
	os << std::boolalpha;
	os << "diff_sync   : " << o.diff_sync << std::endl;
	os << "fft_sync	: " << o.fft_sync << std::endl;
	os << "wave_smooth : " << o.wave_smooth << std::endl;
	os << "fft_smooth  : " << o.fft_smooth;
	return os;
}

std::ostream& operator<<(std::ostream& os, const Uniform& o) {
	os << "name	 : " << o.name << std::endl;
	os << "values = {";
	for (int i = 0; i < o.values.size(); ++i)
		os << o.values[i] << ", ";
	os << "}";
	return os;
}

std::ostream& operator<<(std::ostream& os, const ShaderConfig& o) {
	os << o.mAudio_ops << endl;

	for (int i = 0; i < o.mBuffers.size(); ++i)
		os << o.mBuffers[i] << endl;

	for (int i = 0; i < o.mRender_order.size(); ++i)
		os << o.mRender_order[i] << ", ";
	os << std::endl;

	for (int i = 0; i < o.mUniforms.size(); ++i)
		os << o.mUniforms[i];
	os << std::endl;

	return os;
}

bool operator==(const AudioOptions& l, const AudioOptions& o) {
	return l.fft_sync == o.fft_sync &&
		l.diff_sync == o.diff_sync &&
		l.fft_smooth == o.fft_smooth &&
		l.wave_smooth == o.wave_smooth;
}

bool operator==(const Buffer& l, const Buffer& o) {
	return l.name == o.name &&
		l.width == o.width &&
		l.height == o.height &&
		l.is_window_size == o.is_window_size &&
		l.geom_iters == o.geom_iters &&
		l.clear_color == o.clear_color;
}

bool operator==(const Uniform& l, const Uniform& o) {
	return l.name == o.name &&
		l.values == o.values;
}
