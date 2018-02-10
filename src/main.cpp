#include <iostream>
#include <thread>
#include <string>
using std::cout;
using std::endl;
using std::string;
#include <chrono>
using clk = std::chrono::steady_clock;

#include "filesystem.h"

#include "Window.h"
#include "ShaderConfig.h"
#include "ShaderPrograms.h"
#include "Renderer.h"
#include "FileWatcher.h"

#include "audio_data.h"
#include "audio_process.h"


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
	filesys::path img_geom_path = shader_folder / "image.geom";
	filesys::path img_frag_path = shader_folder / "image.frag";
	if (!filesys::exists(shader_folder)) {
		cout << "shaders folder does not exist" << endl;
		cout << "make the diretory shaders in the folder containing this executable" << endl;
		exit(0);
	}
	if (!filesys::exists(json_path)) {
		cout << "shaders should contain a shader.json file" << endl;
		exit(0);
	}
	if (!filesys::exists(img_geom_path)) {
		cout << "shaders should contain an image.geom file" << endl;
		exit(0);
	}
	if (!filesys::exists(img_frag_path)) {
		cout << "shaders should contain an image.frag file" << endl;
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

	struct audio_data my_audio_data;
	my_audio_data.thread_join = false;
	audio_processor* ap = new audio_processor(&my_audio_data, &get_pcm, &audio_setup);
	std::thread audioThread(&audio_processor::run, ap);

	ShaderPrograms shader_programs(shader_config, shader_folder, is_ok);
	if (!is_ok)
		return 0;
	Renderer renderer (shader_config, shader_programs, window);

	while (window.is_alive()) {
		if (watcher.shaders_changed) {
			cout << "Updating shaders." << endl;
			watcher.shaders_changed = false;
			if (filesys::exists(shader_folder)
			 && filesys::exists(json_path)
			 && filesys::exists(img_geom_path)
			 && filesys::exists(img_frag_path))
			{
				is_ok = true;
				ShaderConfig new_shader_config(json_path, is_ok);
				if (is_ok) {
					ShaderPrograms new_shader_programs(new_shader_config, shader_folder, is_ok);
					if (is_ok) {
						shader_config = new_shader_config;
						shader_programs = std::move(new_shader_programs);
						renderer = std::move(Renderer(shader_config, shader_programs, window));
					}
				}
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
