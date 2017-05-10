// vim: foldmarker=\/\-,\-\/:foldmethod=marker

	//- Feature Toggles
	// ------WAVE RENORMALIZATION
	// renormalize the wave to be unit amplitude.
	// there are two methods to do this, dont enable both of these
	// #define RENORM_1
	// #define RENORM_2
	//
	// ------WAVE FFT STABILIZATION
	// Use the fft to intelligently choose when to advance the waveform.
	// Imagine running along next to a audio wave, and you want to take a picture of the audio
	// wave with a camera such that each picture looks almost exactly the same.
	// How do we know when to take these pictures? We need to try and take a picture every
	// time that we pass a distance of one period (one wavelength) of the audio wave. The audio
	// pointer in the circular buffer is like the camera, and every time we update the audio
	// pointer we take a picture of the audio wave with our camera.
	//
	// The audio pointer in the circular buffer should be moved such that it travels only distances
	// that are whole multiples, not fractional multiples, of the audio wave's wavelength.
	// But how do we find the wavelength of the audio wave? If we try and look for the zero's
	// of the audio wave and then note the distance between them, then we will not get an accurate
	// or consistent measure of the wavelength because the noise in the waveform can throw
	// our measure off.
	//
	// The wavelength of a waveform is related to the frequency by this equation
	// wavelength = velocity/frequency
	//
	// velocity = sample rate
	// wavelength = number of samples of one period of the wave
	// frequency = dominant frequency of the waveform
	//
	// In order to find the wavelength of the audio wave we can take the fft of the wave
	// and then use that to choose at what distances we should move the audio pointer by.
	// Is there a better way to get the wavelength? idk, this works pretty good.
	//
	// In the code I move the audio pointer forward by ( steps*SR/freq ) instead of ( SR/freq )
	// because it could happen that the audio loop takes enough time to process that we should move
	// the audio pointer forward more than one wavelength. If we imagine that we're driving past the
	// audio waveform, then we could say that we forgot to take a picture the last time we passed a
	// wavelength, and so we need to move the next picture-taking-position (audio pointer) ahead by
	// 'steps' number of wavelengths.
	//
	#define FFT_SYNC
	// -/

/*
 * Proof in progress
 * Work of concept
 * ...
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <limits>
#include <cmath>
#include <string.h>
#include "audio_data.h"
#include "pulse_misc.h"
#include "ffts/include/ffts.h"

namespace chrono = std::chrono;
using std::cout;
using std::endl;

// template <int...I>
// constexpr auto
// make_compile_time_sequence(std::index_sequence<I...>) {
// 	return std::array<int,sizeof...(I)>{{cos(I)...}};
// }
// constexpr auto array_1_20 = make_compile_time_sequence(std::make_index_sequence<40>{});

//- Constants
//
// PL length of the pulse audio output buffer
// PN number of pulse audio output buffers to keep
// L  length of all PN buffers combined
// VL length of visualizer 1D texture buffers
// C  channel count of audio
// SR requested sample rate of the audio
// SRF sample rate of audio given to FFT
const int PL = 512;
const int PN = 16;
const int L = PL * PN;
const int FFTLEN = L / 2;
const int VL = VISUALIZER_BUFSIZE; // defined in audio_data.h
const int C = 2;
const int SR = 48000;
const int SRF = SR / 2;
// I think the FFT looks best when it has 8192/48000, or 4096/24000, time granularity. However, I
//   like the wave to be 96000hz just because then it fits on the screen nice.
// To have both an FFT on 24000hz data and to display 96000hz waveform data, I ask
//   pulseaudio to output 48000hz and then resample accordingly.
// I've compared the 4096 sample FFT over 24000hz data to the 8192 sample FFT over 48000hz data
//   and they are surprisingly similar. I couldn't tell a difference really.
// I've also compared resampling with ffmpeg to my current naive impl. Hard to notice a
//   difference.
// Of course the user might want to change all this (I hope not). That's for another day.
// -/

//- Functions
static float mag(float* f, int i) {
	return sqrt(f[i*2]*f[i*2] + f[i*2+1]*f[i*2+1]);
}
static int max_bin(float* f) {
	float max = 0.f;
	int max_i = 0;
	// catch frequencies from 5.86 to 586
	for (int i = 0; i < 100; ++i) {
		const float mmag = mag(f,i);
		if (mmag > max) {
			max = mmag;
			max_i = i;
		}
	}
	if (max_i == 0)
		max_i++;
	return max_i;
}
static void move_index(float& p, float amount) {
	p += amount;
	if (p < 0.f) {
		p += L;
	} else if (p > L) {
		p -= L;
	}
}
static float dist_forward(float from, float to) {
	float d = to - from;
	if (d < 0)
		d += L;
	return d;
}
static float sign(float x) {
	return std::copysign(1.f, x);
}
static void adjust_reader(float& r, float w, float hm) {
	// This function attempts to separate two points in a circular buffer by distance L/2
	const float a = w - r;
	const float b = fabs(a) - L / 2;
	const float foo = fabs(b);
	// minimize foo(r)
	// foo(r) = |b(a(r))|
	//        = ||w-r|-L/2|
	//
	// dfoodr = dfoodb * dbda * dadr
	//
	// dadr   = dadr
	// dbdr   = dbda * dadr
	// dfoodr = dfoodb * dbdr
	//
	// dadr   = -1
	// dbdr   = sign(a) * dadr
	// dfoodr = sign(b) * dbdr
	//
	// -foo'(r) = is the direction r should move to decrease foo(r)
	// foo(r) = happens to be the number of samples such that foo(r - foo'(r)*foo(r)) == 0
	//
	// see mathematica notebook (pointer_adjust_function.nb) in this directory
	const float dadr = -1.f;
	const float dbdr = sign(a) * dadr;
	const float dfoodr = sign(b) * dbdr;
	// move r by a magnitude of foo in the -foo'(r) direction but by taking only steps of size SR/hm
	float grid = SR / hm;
	grid = ceil(foo / grid) * grid;
	move_index(r, -dfoodr * grid);
}
static bool fps(chrono::steady_clock::time_point& now) {
	static auto prev_time = chrono::steady_clock::now();
	static int counter = 0;
	counter++;
	if (now > prev_time + chrono::seconds(1)) {
		cout << "snd fps: " << counter << endl;
		counter = 0;
		prev_time = now;
		return true;
	}
	return false;
}
static chrono::steady_clock::duration dura(float x) {
	// dura() converts a second, represented as a double, into the appropriate unit of
	// time for chrono::steady_clock and with the appropriate arithematic type
	// using dura() avoids errors like this : chrono::seconds(double initializer)
	// dura() : <double,seconds> -> chrono::steady_clock::<typeof count(), time unit>
	return chrono::duration_cast<chrono::steady_clock::duration>(
	    chrono::duration<float, std::ratio<1>>(x));
}
static float max_frequency(float* f) {
	// more info -> http://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak
	const int k = max_bin(f);
	const float y1 = mag(f,k - 1);
	const float y2 = mag(f,k);
	const float y3 = mag(f,k + 1);
	const float d = (y3 - y1) / (2 * (2 * y2 - y1 - y3)+1/32.f); // +1/32 to avoid divide by zero
	const float kp = k + d;
	return kp * float(SRF) / float(FFTLEN);
}
static float clamp(float x, float l, float h) {
	if (x < l)
		x = l;
	else if (x > h)
		x = h;
	return x;
}
static inline float mix(float x, float y, float m) {
	return x * (1.f - m) + y * m;
}
// setting low mixer can lead to some interesting lissajous
template<typename T>
static void renorm(T* f, float& max, float mixer, float scale) {
	float _max = -16.f;
	for (int i = 0; i < VL; ++i)
		if (fabs(f[i]) > _max)
			_max = fabs(f[i]);
	max = mix(max, _max, mixer);
	if (max != 0.f)
		for (int i = 0; i < VL; ++i)
			f[i] = f[i] / max * scale;
}
// -/

void* audioThreadMain(void* data) {

	//- TODOs
	//
	// *!need a faster fft library. try ffts (https://github.com/linkotec/ffts) or fftw!
	//
	// *would alsa be more stable/faster?
	// *use template programming to compute the window at compile time.
	// *how much time does pa_simple_read take to finish? If the app is in the error state, then
	//   how much time does it take?
	// -/

	//- Audio repositories
	float* pulse_buf_l = (float*)calloc(L * C, sizeof(float));
	float* pulse_buf_r = pulse_buf_l + L;
	// -/

	//- Presentation buffers
	struct audio_data* audio = (struct audio_data*)data;
	audio->audio_l = (float*)calloc(VL * C * 2, sizeof(float));
	audio->audio_r = audio->audio_l + VL;
	audio->freq_l = audio->audio_r + VL;
	audio->freq_r = audio->freq_l + VL;
	// -/

	//- FFT setup
	ffts_plan_t* fft_plan;
	fft_plan = ffts_init_1d(FFTLEN, FFTS_FORWARD);
	float __attribute__((aligned(32))) fl[2 * FFTLEN];
	float __attribute__((aligned(32))) fft_inl[2 * FFTLEN];
	float __attribute__((aligned(32))) fr[2 * FFTLEN];
	float __attribute__((aligned(32))) fft_inr[2 * FFTLEN];
	float FFTwindow[FFTLEN];
	for (int i = 1; i < FFTLEN; ++i)
		FFTwindow[i] = (1.f - cosf(2.f * M_PI * i / float(FFTLEN))) / 2.f;
	// -/

	//- Pulse setup
	getPulseDefaultSink((void*)audio);
	float pulse_buf_interlaced[PL * C];
	pa_sample_spec ss;
	ss.channels = C;
	ss.rate = SR;
	ss.format = PA_SAMPLE_FLOAT32NE;
	pa_buffer_attr pb;
	pb.fragsize = sizeof(pulse_buf_interlaced) / 2;
	pb.maxlength = sizeof(pulse_buf_interlaced);
	pa_simple* pulseState = NULL;
	int error;
	pulseState = pa_simple_new(NULL, "APPNAME", PA_STREAM_RECORD, audio->source.data(), "APPNAME", &ss, NULL,
	                  &pb, &error);
	if (!pulseState) {
		cout << "Could not open pulseaudio source: " << audio->source << pa_strerror(error)
		     << ". To find a list of your pulseaudio sources run 'pacmd list-sources'" << endl;
		exit(EXIT_FAILURE);
	}
	// -/

	//- Time Management
	auto now = chrono::steady_clock::now();
	auto _60fpsDura = dura(1.f / 60.f);
	auto next_tl = now + _60fpsDura;
	auto next_tr = next_tl;
	auto prev_t = next_tr; // Compute the fft only once every 1/60 of a second
	// -/

	//- Repository Indice Management
	int W = 0;     // The index of the writer in the audio repositories, of the newest sample in the
	               // buffers, and of a discontinuity in the waveform
	float irl = 0; // Index of a reader in the audio repositories (left channel)
	float irr = irl;
	float freql = 60.f; // Harmonic frequency of the dominant frequency of the audio.
	float freqr = freql; // we capture an image of the waveform at this rate
	// -/

	//- Wave Renormalizer
	float maxl = 1.f;
	float maxr = 1.f;
#ifdef RENORM_2
	float* sinArray = (float*)malloc(VL * sizeof(float));
	for (int i = 0; i < VL; ++i)
		sinArray[i] = sinf(6.288f * i / float(VL));
#endif
	// -/

	while (1) {

		//- Read Datums
		if (pa_simple_read(pulseState, pulse_buf_interlaced, sizeof(pulse_buf_interlaced), &error) < 0) {
			cout << "pa_simple_read() failed: " << pa_strerror(error) << endl;
			exit(EXIT_FAILURE);
		}
		// -/

		//- Fill audio repositories
		for (int i = 0; i < PL; i++) {
			pulse_buf_l[PL * W + i] = pulse_buf_interlaced[i * C + 0];
			pulse_buf_r[PL * W + i] = pulse_buf_interlaced[i * C + 1];
		}
		W++;
		W %= PN;
		// -/

		//- Compute FFT and fill presentation buffers
		now = chrono::steady_clock::now();
		if (now > prev_t) {
			prev_t = now + _60fpsDura;

			int steps = (now-next_tl)/dura(1.f/freql);
			move_index(irl, steps * SR / freql);
			next_tl += dura(steps/freql);

			steps = (now-next_tr)/dura(1.f/freqr);
			move_index(irr, steps * SR / freqr);
			next_tr += dura(steps/freqr);

			if (dist_forward(irl, PL * W) < VL) {
				adjust_reader(irl, PL * W, freql);
				cout << "oopsl" << endl;
			}
			if (dist_forward(irr, PL * W) < VL) {
				adjust_reader(irr, PL * W, freqr);
				cout << "oopsr" << endl;
			}
			freql = max_frequency(fl);
			if (freql < 24.) freql = 60.;
			freqr = max_frequency(fr);
			if (freqr < 24.) freqr = 60.;

			for (int i = 0; i < FFTLEN; ++i) {
				fft_inl[i * 2] = pulse_buf_l[(i * 2 + PL * W) % L] * FFTwindow[i];
				fft_inl[i * 2 + 1] = 0;
				fft_inr[i * 2] = pulse_buf_r[(i * 2 + PL * W) % L] * FFTwindow[i];
				fft_inr[i * 2 + 1] = 0;
			}
			ffts_execute(fft_plan, fft_inl, fl);
			ffts_execute(fft_plan, fft_inr, fr);
			for (int i = 0; i < FFTLEN / 2; i++) {
				audio->freq_l[i] = (float)mag(fl, i)/sqrt(FFTLEN);
				audio->freq_r[i] = (float)mag(fr, i)/sqrt(FFTLEN);
			}

			audio->mtx.lock();
			const float smoother = 1.;
			for (int i = 0; i < VL; ++i) {
				#define upsmpl(S, Y) (S[(i/2 + int(Y)) % L] + S[((i+1)/2 + int(Y)) % L])/2.f
				const float al = upsmpl(pulse_buf_l, irl);
				audio->audio_l[i] = mix(audio->audio_l[i], al, smoother);
				const float ar = upsmpl(pulse_buf_r, irr);
				audio->audio_r[i] = mix(audio->audio_r[i], ar, smoother);
			}
			// renorm(audio->audio_l, maxl, .5, .8);
			// renorm(audio->audio_r, maxr, .5, .8);
			audio->mtx.unlock();
		}
		// -/

		if (audio->thread_join) {
			pa_simple_free(pulseState);
			break;
		}

		// fps(now);
		// Turning this on causes the indices to get all fd up
		// std::this_thread::sleep_for(std::chrono::milliseconds(13));
	}
	return 0;
}
