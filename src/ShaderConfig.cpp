#include <iostream>
using std::cout;
using std::endl;
#include <string>
using std::string;
using std::to_string;
#include <fstream>
#include <cctype> // isdigit
#include <algorithm> // find

#include "ShaderConfig.h"
#include "JsonFileReader.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
namespace rj = rapidjson;
#include <stdexcept>
using std::runtime_error;

static const string WINDOW_SZ_KEY("window_size");
static const string AUDIO_NUM_FRAMES_KEY("audio_num_frames");

ShaderConfig::ShaderConfig(filesys::path conf_file_path) : ShaderConfig(JsonFileReader::read(conf_file_path))
{
}

ShaderConfig::ShaderConfig(string json_str) {
	rj::Document user_conf;
	rj::ParseResult ok = user_conf.Parse<rj::kParseCommentsFlag | rj::kParseTrailingCommasFlag>(json_str.c_str());
	if (! ok)
		throw runtime_error("JSON parse error: " + string(rj::GetParseError_En(ok.Code())) + " At char offset (" + to_string(ok.Offset()) + ")");

	if (! user_conf.IsObject())
		throw runtime_error("Invalid json file");

	mInitWinSize.height = 400;
	mInitWinSize.width = 400;
	if (user_conf.HasMember("initial_window_size")) {
		rj::Value& window_size = user_conf["initial_window_size"];
		if (! (window_size.IsArray() && window_size.Size() == 2 && window_size[0].IsNumber() && window_size[1].IsNumber()))
			throw runtime_error("window_size must be an array of 2 numbers");
		mInitWinSize.width = window_size[0].GetInt();
		mInitWinSize.height = window_size[1].GetInt();
	}

	if (user_conf.HasMember("audio_options")) {

		//int count = 0; 
		//for (auto memb = user_conf.MemberBegin(); memb != user_conf.MemberEnd(); memb++) {
		//	if (memb->name == string("audio_options"))
		//		count++;
		//}
		//if (count != 1) {
		//	cout << "Can only define one audio_options object" << endl;
		//	is_ok = false;
		//	return;
		//}

		AudioOptions ao;
		rj::Value& audio_options = user_conf["audio_options"];
		if (! audio_options.IsObject())
			throw runtime_error("Audio options must be a json object");
		if (! audio_options.HasMember("FFT_SMOOTH"))
			throw runtime_error("Audio options must contain the FFT_smooth option");
		if (! audio_options.HasMember("WAVE_SMOOTH"))
			throw runtime_error("Audio options must contain the WAVE_smooth option");
		if (! audio_options.HasMember("FFT_SYNC"))
			throw runtime_error("Audio options must contain the FFT_smooth option");
		if (! audio_options.HasMember("DIFF_SYNC"))
			throw runtime_error("Audio options must contain the DIFF_sync option");

		rj::Value& fft_smooth = audio_options["FFT_SMOOTH"];
		rj::Value& wave_smooth = audio_options["WAVE_SMOOTH"];
		rj::Value& fft_sync = audio_options["FFT_SYNC"];
		rj::Value& diff_sync = audio_options["DIFF_SYNC"];

		if (! fft_smooth.IsNumber())
			throw runtime_error("FFT_SMOOTH must be a number between in the interval [0, 1]");
		if (! wave_smooth.IsNumber())
			throw runtime_error("WAVE_SMOOTH must be a number between in the interval [0, 1]");
		if (! fft_sync.IsBool())
			throw runtime_error("FFT_SYNC must be a bool");
		if (! diff_sync.IsBool())
			throw runtime_error("DIFF_SYNC must be a bool");

		ao.fft_smooth = fft_smooth.GetFloat();
		if (ao.fft_smooth < 0 || ao.fft_smooth > 1)
			throw runtime_error("FFT_SMOOTH must be in the interval [0, 1]");

		ao.wave_smooth = wave_smooth.GetFloat();
		if (ao.wave_smooth < 0 || ao.wave_smooth > 1)
			throw runtime_error("WAVE_SMOOTH must be in the interval [0, 1]");

		ao.fft_sync = fft_sync.GetBool();
		ao.diff_sync = diff_sync.GetBool();

		mAudio_ops = ao;
	}
	else {
		mAudio_ops.diff_sync = true;
		mAudio_ops.fft_sync = true;
		mAudio_ops.fft_smooth = .75;
		mAudio_ops.wave_smooth = .75;
	}

	if (user_conf.HasMember("blend")) {
		if (! user_conf["blend"].IsBool())
			throw runtime_error("Invalid type for blend option");
		mBlend = user_conf["blend"].GetBool();
	}
	else {
		mBlend = false;
	}

	if (! user_conf.HasMember("image"))
		throw runtime_error("shader.json needs the image setting");
	else {
		rj::Value& image = user_conf["image"];
		if (! image.IsObject())
			throw runtime_error("image is not a json object");
		if(! image.HasMember("geom_iters"))
			throw runtime_error("image does not contain the geom_iters option");

		rj::Value& geom_iters = image["geom_iters"];

		if (! (geom_iters.IsInt() && geom_iters.GetInt() > 0))
			throw runtime_error("image has incorrect value for geom_iters option");
		mImage.geom_iters = geom_iters.GetInt();

		if (image.HasMember("clear_color")) {
			rj::Value& clear_color = image["clear_color"];
			if (! (clear_color.IsArray() && clear_color.Size() == 3))
				throw runtime_error("image has incorrect value for clear_color option");
			for (int i = 0; i < 3; ++i) {
				if (clear_color[i].IsNumber())
					mImage.clear_color[i] = clear_color[i].GetFloat();
				else {
					throw runtime_error("image has incorrect value for clear_color option");
				}
			}
		}
		else {
			for (int i = 0; i < 3; ++i)
				mImage.clear_color[i] = 0.f;
		}

		mImage.is_window_size = true;
	}

	if (user_conf.HasMember("buffers")) {
		rj::Value& buffers = user_conf["buffers"];
		if (! buffers.IsObject())
			throw runtime_error("buffers is not a json object");

		if (buffers.MemberCount() > 0) {

			// Catch buffers with the same name
			std::vector<string> buffer_names;

			for (auto memb = buffers.MemberBegin(); memb != buffers.MemberEnd(); memb++) {
				Buffer b;

				rj::Value& buffer = memb->value;
				b.name = memb->name.GetString();

				if (buffer_names.end() != std::find(buffer_names.begin(), buffer_names.end(), b.name))
					throw runtime_error("Buffers must have unique names");
				buffer_names.push_back(b.name);

				if (b.name == string(""))
					throw runtime_error("Buffer must have a name");
				if (! (std::isalpha(b.name[0]) || b.name[0] == '_'))
					throw runtime_error("Invalid buffer name: " + b.name + " buffer names must start with either a letter or an underscore");
				if (b.name == string("image"))
					throw runtime_error("Cannot name buffer image");

				if (! buffer.IsObject())
					throw runtime_error("Buffer "+b.name+" is not a json object");

				if (! buffer.HasMember("size"))
					throw runtime_error(b.name + " does not contain the size option");
				if (! buffer.HasMember("geom_iters"))
					throw runtime_error(b.name + " does not contain the geom_iters option");
				if (buffer.HasMember("clear_color")) {
					rj::Value& b_clear_color = buffer["clear_color"];
					if (! (b_clear_color.IsArray() && b_clear_color.Size() == 3))
						throw runtime_error(b.name + " has incorrect value for clear_color option");
					for (int i = 0; i < 3; ++i) {
						if (b_clear_color[i].IsNumber())
							b.clear_color[i] = b_clear_color[i].GetFloat();
						else {
							throw runtime_error(b.name + " has incorrect value for clear_color option");
						}
						//else if (b_clear_color[i].IsInt())
						//	b.clear_color[i] = b_clear_color[i].GetFloat()/256.f;
					}
				}
				else {
					for (int i = 0; i < 3; ++i)
						b.clear_color[i] = 0.f;
				}


				rj::Value& b_size = buffer["size"];
				rj::Value& b_geom_iters = buffer["geom_iters"];

				if (b_size.IsArray() && b_size.Size() == 2) {
					if (! b_size[0].IsInt() || ! b_size[1].IsInt())
						throw runtime_error(b.name + " has incorrect value for size option");
					b.width = b_size[0].GetInt();
					b.height = b_size[1].GetInt();
					b.is_window_size = false;
				}
				else if (b_size.IsString() && b_size.GetString() == WINDOW_SZ_KEY) {
					b.is_window_size = true;
					b.height = 0;
					b.width = 0;
				}
				else {
					throw runtime_error(b.name + " has incorrect value for size option");
				}

				if (! (b_geom_iters.IsInt() && b_geom_iters.GetInt() > 0))
					throw runtime_error(b.name + " has incorrect value for geom_iters option");
				b.geom_iters = b_geom_iters.GetInt();

				mBuffers.push_back(b);
			}

			if (user_conf.HasMember("render_order")) {
				rj::Value& render_order = user_conf["render_order"];
				if (! (render_order.IsArray() && render_order.Size() != 0))
					throw runtime_error("render_order must be an array with length > 0");
				for (unsigned int i = 0; i < render_order.Size(); ++i) {
					if (! render_order[i].IsString())
						throw runtime_error("render_order can only contain buffer name strings");

					string b_name = render_order[i].GetString();
					int index = -1;
					for (int j = 0; j < mBuffers.size(); ++j) {
						if (mBuffers[j].name == b_name) {
							index = j;
							break;
						}
					}
					if (index == -1)
						throw ("render_order member \"" + b_name + "\" must be the name of a buffer in \"buffers\"");
					
					// mRender_order contains indices into mBuffers
					mRender_order.push_back(index);
				}
				
				// Only keep the buffers that are used to render
				std::vector<int> placed;
				std::vector<Buffer> used_buffs;
				for (int i = 0; i < mRender_order.size(); i++) {
					bool in = false;
					for (int j = 0; j < placed.size(); ++j)
						if (mRender_order[i] == placed[j])
							in = true;
					if (! in) {
						used_buffs.push_back(mBuffers[mRender_order[i]]);
						placed.push_back(mRender_order[i]);
					}
				}
				mBuffers = used_buffs;
			}
			else {
				for (int i = 0; i < mBuffers.size(); ++i)
					mRender_order.push_back(i);
			}
		}
	}

	if (user_conf.HasMember("uniforms")) {
		rj::Value& uniforms = user_conf["uniforms"];
		if (! uniforms.IsObject())
			throw runtime_error("Uniforms must be a json object.");

		if (uniforms.MemberCount() > 0) {

			// Catch uniforms with the same name
			std::vector<string> uniform_names;

			for (auto memb = uniforms.MemberBegin(); memb != uniforms.MemberEnd(); memb++) {
				Uniform u;

				rj::Value& uniform = memb->value;
				u.name = memb->name.GetString();
				if (uniform_names.end() != std::find(uniform_names.begin(), uniform_names.end(), u.name))
					throw runtime_error("Uniforms must have unique names");
				uniform_names.push_back(u.name);

				if (uniform.IsArray()) {
					if (uniform.Size() > 4)
						throw runtime_error("Uniform "+u.name+" must have dimension less than or equal to 4");
					for (unsigned int i = 0; i < uniform.Size(); ++i) {
						if (! uniform[i].IsNumber())
							throw runtime_error("Uniform " + u.name + " contains a non-numeric value.");
						u.values.push_back(uniform[i].GetFloat());
					}
				}
				else if (uniform.IsNumber()) {
					u.values.push_back(uniform.GetFloat());
				}
				else {
					throw runtime_error("Uniform " + u.name + " must be either a number or an array of numbers.");
				}

				mUniforms.push_back(u);
			}
		}
	}
}
