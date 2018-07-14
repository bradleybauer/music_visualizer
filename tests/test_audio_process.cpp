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

static float correlation(vector<vector<float>>& previous_buffers, int frame_id) {
    float sum = 0.;
    const float* current_buffer = previous_buffers[frame_id % HISTORY_NUM_FRAMES].data();
    for (int b = 0; b < HISTORY_NUM_FRAMES; ++b) {
        const int cur_buf = (frame_id + b) % HISTORY_NUM_FRAMES;
        for (int i = 0; i < HISTORY_BUFF_SZ; ++i) {
            sum += previous_buffers[cur_buf][i] * current_buffer[i];
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
        freq = 2.f * 3.1415926f / s * (1.f + 2.f*(.5f + .5f*sin(freq_modulator)));

        for (int i = 0; i < s; ++i) {
            //l[i] = r[i] = sin(t);
            //t += freq;

            l[i] = r[i] = .5f*sin(t) + (2.f*fbm(t) - 1.f);
            t += freq;

            //l[i] = r[i] = 2.*fbm(t) - 1.;
            //t += freq;
        }
	});

	AudioProcess<fake_clock, AudioStreamT> ap(as, ao);

    /* Simulation notes
    The purpose is to simulate the behavior of the audio loop in a test environment.
    I thought it would be interesting to model the probability that the stall time in the system specific get_pcm function is x (then use uniform distribution & inverse CDF).

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

        sum += correlation(previous_buffers, frame_id);
	}
    return sum;
}

TEST_CASE("Optimization performance test") {
    AudioOptions ao;
    ao.xcorr_sync = false;
    ao.fft_sync = false;
    ao.wave_smooth = 1.0f;
    ao.fft_smooth = 1.0f;

    // none of the wave stabilization optims enabled
    const float baseline_perf = test_for_audio_options(ao);

    ao.fft_sync = true;
    const float fft_perf = test_for_audio_options(ao);
    CHECK(fft_perf > baseline_perf);
    CHECK(fft_perf >= 810910);

    ao.xcorr_sync = true;
    const float xcorr_perf = test_for_audio_options(ao);
    CHECK(xcorr_perf > baseline_perf);
    CHECK(xcorr_perf > fft_perf);
    CHECK(xcorr_perf >= 850551);
}