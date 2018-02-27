#include "Test.h"
#include "fake_clock.h"
#include "audio_process.h"
#include <fstream>
using std::ifstream;
#include <array>
using std::array;

static array<array<double, 700>, 400> canvas;
void paint(audio_processor<fake_clock>* ap) {
	// accumulate blur into canvas
}

// Outputs a measure of performance of the waveform stabilization optimizations in audioprocess.cpp
bool AudioProcessTest::test() {
    // opens a file or generates an audio wave for a set amount of time
    // clock is advanced within a loop until time is finished
    // each iter:
    //    loop audio_process
    //        get_pcm blocks for a certain amount of time and advances the clock?
    //    draw audio into buffer

	// I'm want to note that the below loop isn't simulating reality very well.
	// the reason is that GL doesn't draw after every loop of ap->step().
	// but paint does draw after every loop of ap->step().

	struct audio_data my_audio_data;
	my_audio_data.thread_join = false;
	int test = 0;
	auto get_pcm = [&](float* left, float* right, int size) {
		for (int i = 0; i < size; ++i) {
			left[i] = sin(test / 1000. * 3.141592);
			right[i] = left[i];
			test++;
		}
		return;
	};
	auto audio_setup = [](int, int) { return; };

	auto* ap = new audio_processor<fake_clock>(&my_audio_data, get_pcm, audio_setup);

	int i = 1000;
	while (i > 0) {
		ap->step();
		paint(ap);
		i--;
	}

    return true;
}