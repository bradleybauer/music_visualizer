#include <iostream>
using std::cout;
using std::endl;
#include <thread>
#include <string>
using std::string;
#include <chrono>
using ClockT = std::chrono::steady_clock;
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
using AudioProcessT = AudioProcess<ClockT, AudioStreamT>;

// TODO rename to shader player (like vmware player) ?
// TODO adding builtin uniforms should be as easy as adding an entry to a list

#if defined(WINDOWS) && defined(DEBUG)
int WinMain() {
#else
int main(int argc, char* argv[]) {
#endif

    // TODO add no system window border/title bar option?
    // TODO add stay on top of all other windows option?
    // TODO could i use some kind of neural net to maximize temporal consistency but also minimize difference between displayed signal and actual signal?

    // TODO find a better way to get a decent fps
    //      fps is also used in AudioProcess.h. Search the project for TODOFPS.
    static const int fps = 144;

    filesys::path shader_folder("shaders");
    // TODO should this be here or in ShaderConfig?
    filesys::path shader_config_path = shader_folder / "shader.json";

    FileWatcher watcher(shader_folder);

    ShaderConfig *shader_config = nullptr;
    ShaderPrograms *shader_programs = nullptr;
    Renderer* renderer = nullptr;
    Window *window = nullptr;
    // TODO extract to get_valid_config(&, &, &, &)
    while (!(shader_config && shader_programs && window)) {
        try {
            shader_config = new ShaderConfig(shader_folder, shader_config_path);
            window = new Window(shader_config->mInitWinSize.width, shader_config->mInitWinSize.height);
            renderer = new Renderer(*shader_config, *window);
            shader_programs = new ShaderPrograms(*shader_config, *renderer, *window, shader_folder);
            renderer->set_programs(shader_programs);
        }
        catch (runtime_error &msg) {
            cout << msg.what() << endl;

            // something failed so reset state
            delete shader_config;
            delete shader_programs;
            delete window;
            delete renderer;
            shader_config = nullptr;
            shader_programs = nullptr;
            window = nullptr;
            renderer = nullptr;

            while (!watcher.files_changed()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }
    cout << "Successfully compiled shaders." << endl;

    //AudioStreamT audio_stream(); // Most Vexing Parse
    AudioStreamT audio_stream;
    AudioProcessT audio_process{audio_stream, shader_config->mAudio_ops};
    std::thread audio_thread = std::thread(&AudioProcess<ClockT, AudioStreamT>::start, &audio_process);
    if (shader_config->mAudio_enabled)
        audio_process.start_audio_system();

    auto update_shader = [&]() {
        cout << "Updating shaders." << endl;
        try {
            ShaderConfig new_shader_config(shader_folder, shader_config_path);
            Renderer new_renderer(new_shader_config, *window);
            ShaderPrograms new_shader_programs(new_shader_config, new_renderer, *window, shader_folder);
            *shader_config = new_shader_config;
            *shader_programs = std::move(new_shader_programs);
            *renderer = std::move(new_renderer);
            renderer->set_programs(shader_programs);
        }
        catch (runtime_error &msg) {
            cout << msg.what() << endl;
            cout << "Failed to update shaders." << endl << endl;
            return;
        }
        if (shader_config->mAudio_enabled) {
            audio_process.start_audio_system();
            audio_process.set_audio_options(shader_config->mAudio_ops);
        }
        else {
            audio_process.pause_audio_system();
        }
        cout << "Successfully updated shaders." << endl << endl;
    };

    auto lastFrameTime = ClockT::now();
    auto frameRateDuration = std::chrono::microseconds(int64_t(1 / 144.0 * 1000000));
    while (window->is_alive()) {
        auto now = ClockT::now();
        if ((now - lastFrameTime) > frameRateDuration) {
            if (watcher.files_changed())
                update_shader();
            auto now = ClockT::now();
            renderer->update(audio_process.get_audio_data());
            renderer->render();
            window->swap_buffers();
            window->poll_events();
            lastFrameTime = now;
        }
    }

    audio_process.exit_audio_system();
    audio_thread.join();

    return 0;
}
