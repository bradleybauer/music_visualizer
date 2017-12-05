#include <iostream>
#include <thread>
using std::cout;
using std::endl;
#include <chrono>
using clk = std::chrono::steady_clock;
#include <GLFW/glfw3.h>
#include "draw.h"
#include "audio_data.h"
#include "audio_process.h"

#ifdef WINDOWS
#ifdef DEBUG
int WinMain() {
#else
int main() {
#endif
#else
int main() {
#endif

	struct audio_data my_audio_data;
	my_audio_data.thread_join = false;

	audio_processor* ap = new audio_processor(&my_audio_data, &get_pcm, &audio_setup);
	std::thread audioThread(&audio_processor::run, ap);

	if (!initialize_gl()) { cout << "graphics borked. exiting." << endl; return 0; };
	glfwSetTime(0.0);
	while (should_loop()) {
		auto start = clk::now();
		draw(&my_audio_data);
		auto dura = clk::duration(std::chrono::milliseconds(16)) - (clk::now() - start);
		std::this_thread::sleep_for(dura > clk::duration(0) ? dura : clk::duration(0));
	}
	my_audio_data.thread_join = true;
	audioThread.join();
	deinit_drawing();
	glfwTerminate();
	return 0;
}
