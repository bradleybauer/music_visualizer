#include <iostream>
using std::cout;
using std::endl;
#include <fstream>
using std::ifstream;
#include <vector>
using std::vector;
#include <stdexcept>
#include <chrono>
namespace chrono = std::chrono;

#include "fake_clock.h"
#include "AudioProcess.h"
#include "AudioStreams/WavAudioStream.h"
#include "AudioStreams/ProceduralAudioStream.h"
using AudioStreamT = ProceduralAudioStream;
#include "noise.h"

#include "catch2/catch.hpp"

static const int canvas_height = 400;
static const int canvas_width = 1024;

static float abs_diff_sum(vector<vector<float>>& previous_buffers, int frame_id) {
    float sum = 0.;
    const float* current_buffer = previous_buffers[frame_id % HISTORY_NUM_FRAMES].data();
    for (int b = 0; b < HISTORY_NUM_FRAMES; ++b) {
        const int cur_buf = (frame_id + b) % HISTORY_NUM_FRAMES;
        for (int i = 0; i < HISTORY_BUFF_SZ; ++i) {
            sum += std::abs(previous_buffers[cur_buf][i] - current_buffer[i]);
        }
    }
    return sum;
}

static float test_for_audio_options(AudioOptions ao) {
    vector<vector<float>> previous_buffers(HISTORY_NUM_FRAMES, vector<float>(HISTORY_BUFF_SZ, 0));

    float freq_modulator = 0.;
    float freq = 0.;
    float t = 0.;
	AudioStreamT as([&](float* l, float* r, int s) {
        // set a frequency for this buffer's wave
        freq = 2. * 3.1415926 / s * (1. + 2.*(.5 + .5*sin(freq_modulator)));

        for (int i = 0; i < s; ++i) {
            //l[i] = r[i] = sin(t);
            //t += freq;

            l[i] = r[i] = .5*sin(t) + (2.*fbm(t) - 1.);
            t += freq;

            //l[i] = r[i] = 2.*fbm(t) - 1.;
            //t += freq;
        }
	});

	AudioProcess<fake_clock, AudioStreamT> ap(as);
    ap.set_audio_options(ao);

    // TODO bradley search : class of functions that always have inverses (injections?)

    /* Simulation notes
    The purpose is to simulate the behavior of the audio loop in a test environment.

    Probability that stall time in system specific get_pcm function is x (then use uniform distribution & inverse CDF)

    On average the gfx loop accesses AudioData roughly every 16.7 ms. Considering that the average stall in get_pcm is
    about equal to the half of the time between gfx thread accesses we can simply step the audio thread twice in
    the below loop for a good approximation.
    */
    float sum = 0.;
    const auto avg_get_pcm_stall_time = chrono::microseconds(8650);
    for (int frame_id = 0; frame_id < 60 * 30; ++frame_id) {
		ap.step();
		fake_clock::advance(avg_get_pcm_stall_time);
		ap.step();
		fake_clock::advance(avg_get_pcm_stall_time);

        const AudioData& ad = ap.get_audio_data();
        std::copy(ad.audio_l, ad.audio_l + HISTORY_BUFF_SZ, previous_buffers[frame_id % HISTORY_NUM_FRAMES].data());

        sum += abs_diff_sum(previous_buffers, frame_id);
	}
    std::cout << std::fixed << sum << std::endl;
    return sum;
}

TEST_CASE("Optimization performance test") {
    AudioOptions ao;
    ao.diff_sync = false;
    ao.fft_sync = false;
    ao.wave_smooth = 1.0;
    ao.fft_smooth = 1.0;

    const float basic_chaos_measure = test_for_audio_options(ao);

    ao.fft_sync = true;
    const float fft_measure = test_for_audio_options(ao);
    CHECK(basic_chaos_measure > fft_measure);
    CHECK(fft_measure <= 2560882.000000); // regression tests, todo: could vary accross systems due to ffts

    ao.diff_sync = true;
    const float diff_measure = test_for_audio_options(ao);
    CHECK(basic_chaos_measure > diff_measure);
    //CHECK(diff_measure <= 2498677.250000);
}