#include <iostream>
#include <thread>
using std::cout;
using std::endl;
#include <chrono>
#include <vector>
#include <array>
using clk = std::chrono::steady_clock;

#include <fstream>
#include <sstream>
#include <filesystem>

#include "gl.h"
#include "Window.h"
#include "ShaderConfig.h"
#include "ShaderPrograms.h"

#include "draw.h"
#include "audio_data.h"
#include "audio_process.h"


#if defined(WINDOWS) && defined(DEBUG)
int WinMain() {
#else
int main(int argc, char* argv[]) {
#endif
	// cout << argc << endl;
	// cout << argv[0] << endl;

/*
	// Configuration folder    - one of the defaults or passed in by argument
	//      shaderA folder     - if shaderA does not contain shader.json, then no shader is read from this folder
	//         shader.json
	//         image.geom      - if shaderA does not contain image.geom and image.frag, then no shader is read from this folder
	//         image.frag
	//      shaderB folder
	//         shader.json
	//         A.geom
	//         A.frag
	//         B.geom
	//         B.frag
	//         C.geom
	//         C.frag
	//         img.geom
	//         img.frag

	// Init program
	//    Find config folder path.
	//       If there is no config folder, then tell user and exit.
	//    Register recursive FileWatcher on config folder path.
	//       FileWatcher keeps an available folder list up to date.
	//       The available folder list contains all subfolders of config folder that have valid shader folders.
	//       A valid shader folder is a folder that contains all of these files: (shader.json, image.geom, image.frag)
	//    If there is no available folder, then tell user and exit.
	//    Create app window and initialize opengl context.
	//    Loop until user quits
	//       Choose next shader folder from available folders list.
	//       If there is no available folder, then if currently rendering a shader do nothing otherwise tell user and exit.
	//       Parse ShaderConfig from shader.json .
	//       If ShaderConfig ok, then create a ShaderPrograms instance given a ShaderConfig instance.
	//       If ShaderPrograms ok, then destruct old Renderer and render with the new Renderer.
	//       Loop until next shader requested or FileWatcher observes changes in shader folder.
	//          If shader is not paused or if mouse input is occurring.
	//             Render current shader
*/
	if (!initialize_gl()) { cout << "graphics borked. exiting." << endl; return 0; };
	glfwSetTime(0.0);

	filesys::path shader_folder("shaders");
	bool is_ok = true;
	ShaderConfig shader_conf(shader_folder / "shader.json", is_ok);
	ShaderPrograms shader_programs(shader_conf, shader_folder, is_ok);

	struct audio_data my_audio_data;
	my_audio_data.thread_join = false;

	audio_processor* ap = new audio_processor(&my_audio_data, &get_pcm, &audio_setup);
	std::thread audioThread(&audio_processor::run, ap);

	while (should_loop()) {
		auto start = clk::now();
		draw(&my_audio_data);
		auto dura = clk::duration(std::chrono::milliseconds(14)) - (clk::now() - start);
		std::this_thread::sleep_for(dura > clk::duration(0) ? dura : clk::duration(0));
	}
	my_audio_data.thread_join = true;
	//audioThread.join(); // I would like to exit the program the right way, but sometimes this blocks due to the windows audio system.
	deinit_drawing();
	glfwTerminate();
	exit(0);
	return 0;
}
