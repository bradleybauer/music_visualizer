#include <iostream>
using std::cout; using std::endl;
#include <thread>
#include <string>
using std::string;
#include <chrono>
using steady_clock = std::chrono::steady_clock;
#include <fstream>
using std::ifstream;
#include <stdexcept>
using std::runtime_error;

#include "filesystem.h"
#include "FileWatcher.h"

#include "Window.h"
#include "ShaderConfig.h"
#include "ShaderPrograms.h"
#include "Renderer.h"

#include "AudioProcess.h"
#ifdef WINDOWS
#include "AudioStreams/WindowsAudioStream.h"
#else
#include "AudioStreams/LinuxAudioStream.h"
#endif

#if defined(WINDOWS) && defined(DEBUG)
int WinMain() {
#else
int main(int argc, char* argv[]) {
#endif

	/*
	Register FileWatcher on shader_folder
	Create app window and initialize opengl context.
	try
		shader_config = ShaderConfig(...)
		shader_programs = ShaderPrograms(...)
	if either fails to construct, exit
	renderer = Renderer(...)
	Create audio system (optional)
	Loop until user quits
		If (FileWatcher has seen a change in shader_folder)
			try
				new_shader_config = ShaderConfig(shader_folder)
				new_shader_programs = ShaderPrograms(new_shader_config)
			if neither fails to construct, then
				shader_config = new_shader_config
				shader_programs = new_shader_programs
				renderer = new_renderer(shader_config, shader_programs)

		Render
	*/

	filesys::path shader_folder("shaders");
	filesys::path json_path = shader_folder / "shader.json";

	// Build objects
	FileWatcher watcher(shader_folder);

	ShaderConfig *shader_config;
	Window *window;
	ShaderPrograms *shader_programs;
	AudioStream *as;
	try {
		shader_config = new ShaderConfig(json_path);
		window = new Window(shader_config->mInitWinSize.width, shader_config->mInitWinSize.height);
		shader_programs = new ShaderPrograms(*shader_config, shader_folder);
#ifdef WINDOWS
		as = new WindowsAudioStream();
#else
		as = new LinuxAudioStream();
#endif
	}
	catch (runtime_error &msg) {
		cout << msg.what() << endl;
		return 0;
	}

	Renderer renderer(*shader_config, *shader_programs, *window);
	cout << "Successfully compiled shaders." << endl;

	// TODO make sure AudioProcess lifetime is consistent with mAudio_ops.audio_enabled
	AudioProcess<steady_clock> *ap = nullptr;
	std::thread audioThread;
	if (shader_config->mAudio_enabled) {
		ap = new AudioProcess<steady_clock>(*as);
		audioThread = std::thread(&AudioProcess<steady_clock>::start, ap);
	}

	auto update_shaders = [&]() {
		cout << "Updating shaders." << endl;
		try {
			ShaderConfig new_shader_config(json_path);
			ShaderPrograms new_shader_programs(new_shader_config, shader_folder);
			*shader_config = new_shader_config;
			*shader_programs = std::move(new_shader_programs);
		}
		catch (runtime_error &msg) {
			cout << msg.what() << endl;
			cout << "Failed to update shaders." << endl << endl;
			return;
		}
		renderer = std::move(Renderer(*shader_config, *shader_programs, *window));
		ap->set_audio_options(shader_config->mAudio_ops);
		cout << "Successfully updated shaders." << endl << endl;
	};

	while (window->is_alive()) {
		if (watcher.files_changed())
			update_shaders();
		auto now = steady_clock::now();
		//if (shader_config->mAudio_ops.audio_enabled && ap != nullptr)
			renderer.update(*ap);
		//else
		//	renderer.update();
		renderer.render();
		window->swap_buffers();
		window->poll_events();
		std::this_thread::sleep_for(std::chrono::milliseconds(16) - (steady_clock::now() - now));
	}
	if (ap)
		ap->stop();
	//audioThread.join(); // I would like to exit the program the right way, but sometimes this blocks due to the windows audio system.
	exit(0);
}
