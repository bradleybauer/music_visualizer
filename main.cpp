#include <iostream>
#include <thread>
using std::cout;
using std::endl;
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "draw.h"
#include "audio_data.h"
int main() {
	struct audio_data audio;
	audio.thread_join = false;
	getPulseDefaultSink((void*)&audio);
	std::thread audioThread(audioThreadMain, &audio);
    if (!initialize_gl()) { cout << "graphics borked. exiting." << endl; return 0; };
    glfwSetTime(0.0);
    while (should_loop()) { draw(&audio); }
	audio.thread_join = true;
	audioThread.join();
    deinit_drawing();
    glfwTerminate();
    return 0;
}
