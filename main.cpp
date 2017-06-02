#include <iostream>
#include <thread>
using std::cout;
using std::endl;
using sclock = std::chrono::steady_clock;
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "draw.h"
#include "audio_data.h"
int main() {
	struct audio_data audio;
	audio.thread_join = false;
	std::thread audioThread(audioThreadMain, &audio);
	if (!initialize_gl()) { cout << "graphics borked. exiting." << endl; return 0; };
	glfwSetTime(0.0);
	while (should_loop()) {
		// limit fps to 60
		auto start = sclock::now();
		draw(&audio);
		std::this_thread::sleep_for(sclock::duration(std::chrono::milliseconds(16)) - (sclock::now() - start));
	}
	audio.thread_join = true;
	audioThread.join();
	deinit_drawing();
	glfwTerminate();
	return 0;
}
