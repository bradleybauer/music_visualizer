#include <iostream>
using std::cout;
using std::endl;
#include <thread>
#include <string>
using std::string;
#include <chrono>
using clk = std::chrono::steady_clock;
#include <fstream>
using std::ifstream;

#include "filesystem.h"

#include "Window.h"
#include "ShaderConfig.h"
#include "ShaderPrograms.h"
#include "Renderer.h"
#include "FileWatcher.h"

#include "audio_data.h"
#include "audio_process.h"

#include "WavReader.h"

#if defined(WINDOWS) && defined(DEBUG)
int WinMain() {
#else
int main(int argc, char* argv[]) {
#endif

	/*
	If shader_folder does not exist
		tell user and exit.
	If shader_folder does not contain the neccessary files
		tell user and exit.
	Register FileWatcher on shader_folder

	Create app window and initialize opengl context.
	Create audio system (optional?)

	shader_config = ShaderConfig(...)
	if !is_ok
		tell user and exit
	shader_programs = ShaderPrograms(...)
	if !is_ok
		tell user and exit
	renderer = Renderer(...)
	Loop until user quits
		If (FileWatcher has seen a change in shader_folder)
			If shader_folder contains neccessary files
				new_shader_config = ShaderConfig(shader_folder)
				If is_ok, then
					new_shader_programs = ShaderPrograms(new_shader_config)
					If is_ok, then
						shader_config = new_shader_config
						shader_programs = new_shader_programs
						renderer = new_renderer(shader_config, shader_programs)

		Render
	*/

	filesys::path shader_folder("shaders");
	filesys::path json_path = shader_folder / "shader.json";
	if (!filesys::exists(shader_folder)) {
		cout << "shaders folder does not exist" << endl;
		cout << "make a directory named 'shaders' in the folder containing this executable" << endl;
		exit(0);
	}
	if (!filesys::exists(json_path)) {
		cout << "shaders folder should contain a shader.json file" << endl;
		exit(0);
	}

	FileWatcher watcher(shader_folder);

	bool is_ok = true;
	ShaderConfig shader_config(json_path, is_ok);
	if (!is_ok)
		return 0;

	is_ok = true;
	Window window(shader_config.mInitWinSize.width, shader_config.mInitWinSize.height, is_ok);
	if (!is_ok)
		return 0;

	ShaderPrograms shader_programs(shader_config, shader_folder, is_ok);
	if (!is_ok)
		return 0;
	Renderer renderer (shader_config, shader_programs, window);

	struct audio_data my_audio_data;
	my_audio_data.thread_join = false;
	/*
	short* data = WavReader("mywav.wav");
	auto get_pcm = [&](float* left, float* right, int size) {
		static int j = 0;
		for (int i = 0; i < size; ++i) {
			left[i] = data[2 * j] / 32768.f;
			right[i] = data[2 * j + 1] / 32768.f;
			j++;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(8));
	};
	*/
	// TODO I don't like that I can supply 44100hz audio when the audio_processor expects 96khz
	auto* ap = new audio_processor<chrono::steady_clock>(&my_audio_data, &get_pcm, &audio_setup);
	std::thread audioThread(&audio_processor<chrono::steady_clock>::run, ap);

	while (window.is_alive()) {
		if (watcher.shaders_changed) {
			cout << "Updating shaders." << endl;
			watcher.shaders_changed = false;
			is_ok = true;
			if (!filesys::exists(shader_folder)) {
				cout << "shaders folder does not exist" << endl;
				cout << "make a directory named 'shaders' in the folder containing this executable" << endl;
				is_ok = false;
			}
			if (!filesys::exists(json_path)) {
				cout << "shaders folder should contain a shader.json file" << endl;
				is_ok = false;
			}
			if (is_ok) {
				ShaderConfig new_shader_config(json_path, is_ok);
				if (is_ok) {
					ShaderPrograms new_shader_programs(new_shader_config, shader_folder, is_ok);
					if (is_ok) {
						shader_config = new_shader_config;
						shader_programs = std::move(new_shader_programs);
						renderer = std::move(Renderer(shader_config, shader_programs, window));
						cout << "Successfully updated shader." << endl;
					}
				}
			}
			else {
				cout << "Failed to update shader." << endl;
			}
		}

		auto now = clk::now();

		renderer.update(&my_audio_data);
		renderer.render();
		window.swap_buffers();
		window.poll_events();

		std::this_thread::sleep_for(std::chrono::milliseconds(16) - (clk::now() - now));

		//static int frames = 0;
		//static auto last = clk::now();
		//if (now - last > std::chrono::seconds(1)) {
		//	last = now;
		//	//cout << frames << endl;
		//	frames=0;
		//}
		//frames++;
	}
	my_audio_data.thread_join = true;
	//audioThread.join(); // I would like to exit the program the right way, but sometimes this blocks due to the windows audio system.
	exit(0); // TODO need this without the above join?
	return 0;
}
