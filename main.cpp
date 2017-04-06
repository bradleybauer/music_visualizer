#include <iostream>
#include <thread>
using std::cout;
using std::endl;

#include "audio_data.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "draw.h"
int main() {
    if (!initialize_gl()) { cout << "graphics borked. exiting." << endl; return 0; };
	struct audio_data audio;
	audio.thread_join = false;
	getPulseDefaultSink((void*)&audio);
	// audio.mut = PTHREAD_MUTEX_INITIALIZER;
	std::thread audioThread(audioThreadMain, &audio);
    glfwSetTime(0.0);
    while (should_loop()) { 
		draw(&audio);
	}
	audio.thread_join = true;
	audioThread.join();
    deinit_drawing();
    glfwTerminate();
    return 0;
}
