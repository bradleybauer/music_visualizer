#include <iostream>
#include <cmath>
using std::cout;
using std::endl;

#include "audio_data.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "draw.h"
int main() {
    initialize_gl();
	struct audio_data audio;
	audio.thread_join = false;
	getPulseDefaultSink((void*)&audio);
	pthread_create(&audio.thread, NULL, audioThreadMain, (void*)&audio);
    glfwSetTime(0.0);
    while (should_loop()) { draw(&audio); }
	audio.thread_join = true;
	pthread_join(audio.thread, NULL);
    deinit_drawing();
    glfwTerminate();
    return 0;
}
