#include <iostream>
using std::cout;
using std::endl;
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
#include "AudioStreams/ProceduralAudioStream.h"
using AudioStreamT = WindowsAudioStream;
//using AudioStreamT = ProceduralAudioStream;
#else
#include "AudioStreams/LinuxAudioStream.h"
using AudioStreamT = LinuxAudioStream;
#endif
using AudioProcessT = AudioProcess<steady_clock, AudioStreamT>;

// TODO?
// rename to shader player (like vmware player)

#if defined(WINDOWS) && defined(DEBUG)
int WinMain() {
#else
int main(int argc, char* argv[]) {
#endif

	filesys::path shader_folder("shaders");
	filesys::path shader_config_path = shader_folder / "shader.json";

	FileWatcher watcher(shader_folder);

	ShaderConfig *shader_config = nullptr;
	ShaderPrograms *shader_programs = nullptr;
	Window *window = nullptr;
    while (!(shader_config && shader_programs && window)) {
        try {
            shader_config = new ShaderConfig(shader_config_path);
            window = new Window(shader_config->mInitWinSize.width, shader_config->mInitWinSize.height);
            shader_programs = new ShaderPrograms(*shader_config, shader_folder);
        }
        catch (runtime_error &msg) {
            cout << msg.what() << endl;

			// something failed so reset state
			delete shader_config;
			delete shader_programs;
			delete window;
			shader_config = nullptr;
			shader_programs = nullptr;
			window = nullptr;

            while (!watcher.files_changed()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }
	cout << "Successfully compiled shaders." << endl;

	Renderer renderer(*shader_config, *shader_programs, *window);

	//AudioStreamT audio_stream(); // Most Vexing Parse
	AudioStreamT audio_stream;
    AudioProcessT audio_process{ audio_stream, shader_config->mAudio_ops };
	std::thread audioThread = std::thread(&AudioProcess<chrono::steady_clock, AudioStreamT>::start, &audio_process);
    if (shader_config->mAudio_enabled)
        audio_process.start_audio_system();

    auto update_shader = [&]() {
        cout << "Updating shaders." << endl;
        try {
            ShaderConfig new_shader_config(shader_config_path);
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
        if (shader_config->mAudio_enabled) {
            audio_process.start_audio_system();
            audio_process.set_audio_options(shader_config->mAudio_ops);
        }
        else {
            audio_process.pause_audio_system();
        }
		cout << "Successfully updated shaders." << endl << endl;
	};

	while (window->is_alive()) {
		if (watcher.files_changed())
			update_shader();
		auto now = steady_clock::now();
        renderer.update(audio_process.get_audio_data());
		renderer.render();
		window->swap_buffers();
		window->poll_events();
		std::this_thread::sleep_for(std::chrono::milliseconds(16) - (steady_clock::now() - now));
	}
    audio_process.exit_audio_system();
	//audioThread.join(); // I would like to exit the program the right way, but sometimes this blocks due to the windows audio system.
	exit(0);
}
