#include "Test.h"
#include "fake_clock.h"
#include "audio_process.h"
#include <fstream>
using std::ifstream;
#include <array>
using std::array;

#include "WavAudioStream.h"
#include "ProceduralAudioStream.h"

constexpr int canvas_height = 400;
constexpr int canvas_width = VISUALIZER_BUFSIZE;
static array<array<double, canvas_width>, canvas_height> canvas;
void paint(const struct audio_data &my_audio_data) {
	// accumulate blur into canvas
	for (int j = 0; j < canvas_width; ++j) {
		double y = my_audio_data.audio_l[j];
		y = .5*y + .5;
		y *= canvas_height;
		if (y > 399) y = 399;
		if (y < 0) y = 0;
		canvas[int(y)][j] = 1;
	}
}

void error() {
	int sum = 0;
	for (int i = 0; i < canvas_height; ++i) {
		for (int j = 0; j < canvas_width; ++j) {
			sum += canvas[i][j];
		}
	}
	cout << sum << endl;
}

// Outputs a measure of performance of the waveform stabilization optimizations in audioprocess.cpp
bool AudioProcessTest::test() {
	// TODO make audio_processor take flags by argument so that I can turn off renormalization and other stuff
	// for testing specific optimizations.

	struct audio_data my_audio_data;
	my_audio_data.thread_join = false;
	/*
	AudioStream* as = new ProceduralAudioStream([](float* l, float* r, int s) {
			static int j = 0;
			static int t = 0;
			const double step1 = 3.141592 * 2. / float(s/2);
			const double step2 = 3.141592 * 2. / float(s/8);
			for (int i = 0; i < s; ++i, ++j, ++t)
				l[i] = r[i] = sin(step1 * j) + .3*cos(step2 * t);
			j %= s / 2;
			t %= s / 8;
			std::this_thread::sleep_for(std::chrono::microseconds(8560));
	});
	/*/
	bool is_ok = true;
	AudioStream* as = new WavAudioStream("../src/mywav.wav", is_ok);
	if (!is_ok)
		return false;
	// */
	auto* ap = new audio_processor<fake_clock>(&my_audio_data, as);

	auto start = fake_clock::now();
	int i = 100;
	while (i > 0) {
		ap->step();
		paint(my_audio_data);
		fake_clock::advance(chrono::microseconds(8650));
		i--;
	}
	error();

    return true;
}