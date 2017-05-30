// vim: foldmarker=\/\-,\-\/:foldmethod=marker

	//- Feature Toggles
	// ------WAVE RENORMALIZATION
	// renormalize the wave to be unit amplitude.
	// there are two methods to do this, dont enable both of these
	// #define RENORM_1
	#define RENORM_2
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
	#define FFT_SYNC
	//
	// ------NewWave-oldWave mix / Exponential smooth
	// static const float smoother = 1.;
	static const float smoother = .4;
	// static const float smoother = .2;
	// static const float smoother = .1;
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
static void fps(chrono::steady_clock::time_point& now) {
	static auto prev_time = chrono::steady_clock::now();
	static int counter = 0;
	counter++;
	if (now > prev_time + chrono::seconds(1)) {
		cout << "snd fps: " << counter << endl;
		counter = 0;
		prev_time = now;
	}
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
static float get_harmonic(float freq) {
	// bound freq into the interval [24, 60]

	freq = clamp(freq, 1.f, SRF / float(FFTLEN) * 100);

	const float b = log2f(freq);
	const float l = log2f(24.f);
	const float u = log2f(60.f);
	// a == number of iterations for one of the below loops
	int a = 0;
	if (b < l)
		a = floor(b - l); // round towards -inf
	else if (b > u)
		a = ceil(b - u); // round towards +inf
	freq = pow(2.f, b - a);

	// double o = freq;
	// while (o > 61.f) o /= 2.f; // dividing frequency by two does not change the flipbook
	// image
	// while (o < 24.f) o *= 2.f; // multiplying frequency by two produces the ghosting effect
	return freq;
}
static inline float mix(float x, float y, float m) {
	return x * (1.f - m) + y * m;
}
// setting low mixer can lead to some interesting lissajous
static void renorm(float* f, float& max, float mixer, float scale) {
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

	//- Smoothing buffers
	// helps give the impression that the wave data updates at 60fps
	float* tl = (float*)calloc(VL * C + 2, sizeof(float));
	float* tr = tl + VL + 1;
	// -/

	//- FFT setup
	ffts_plan_t* fft_plan;
	fft_plan = ffts_init_1d_real(FFTLEN, FFTS_FORWARD);
	float __attribute__((aligned(32))) fft_outl[FFTLEN/2+1];
	float __attribute__((aligned(32))) fft_inl[FFTLEN];
	float __attribute__((aligned(32))) fft_outr[FFTLEN/2+1];
	float __attribute__((aligned(32))) fft_inr[FFTLEN];
	float FFTwindow[FFTLEN];
	for (int i = 1; i < FFTLEN; ++i)
		FFTwindow[i] = (1.f - cosf(2.f * M_PI * i / float(FFTLEN))) / 2.f;
	// -/

	//- Pulse setup
	getPulseDefaultSink((void*)audio);
	float pulse_buf_interlaced[PL * C];
	pa_sample_spec pulseSampleSpec;
	pulseSampleSpec.channels = C;
	pulseSampleSpec.rate = SR;
	pulseSampleSpec.format = PA_SAMPLE_FLOAT32NE;
	pa_buffer_attr pb;
	pb.fragsize = sizeof(pulse_buf_interlaced) / 2;
	pb.maxlength = sizeof(pulse_buf_interlaced);
	pa_simple* pulseState = NULL;
	int pulseError;
	pulseState = pa_simple_new(NULL, "APPNAME", PA_STREAM_RECORD, audio->source.data(), "APPNAME", &pulseSampleSpec, NULL,
	                  &pb, &pulseError);
	if (!pulseState) {
		cout << "Could not open pulseaudio source: " << audio->source << pa_strerror(pulseError)
		     << ". To find a list of your pulseaudio sources run 'pacmd list-sources'" << endl;
		exit(EXIT_FAILURE);
	}
	// -/

	//- Time Management
	auto now = chrono::steady_clock::now();
	auto _60fpsDura = dura(1.f / 60.f);
	auto next_l = now + _60fpsDura;
	auto next_r = next_l;
	auto prevfft = next_r; // Compute the fft only once every 1/60 of a second
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
		if (pa_simple_read(pulseState, pulse_buf_interlaced, sizeof(pulse_buf_interlaced), &pulseError) < 0) {
			cout << "pa_simple_read() failed: " << pa_strerror(pulseError) << endl;
			exit(EXIT_FAILURE);
		}
		// -/

		//- Fill audio repositories
		for (int i = 0; i < PL; i++) {
			pulse_buf_l[PL * W + i] = pulse_buf_interlaced[i * C + 0];
			pulse_buf_r[PL * W + i] = pulse_buf_interlaced[i * C + 1];
		}
		// -/

		//- Manage indices and fill smoothing buffers
		W++;
		W %= PN;
		now = chrono::steady_clock::now();
		if (now > next_l) {
			move_index(irl,  SR / freql);
			if (dist_forward(irl, PL * W) < VL) {
				adjust_reader(irl, PL * W, freql);
				cout << "oopsl" << endl;
			}
			for (int i = 0; i < VL; ++i)
				tl[i] = pulse_buf_l[(i + int(irl)) % L];

#ifdef RENORM_1
			renorm(tl, maxl, .5, 1.f);
#endif

#ifdef FFT_SYNC
			freql = get_harmonic(max_frequency(fft_outl));
			next_l += dura(1.f/ freql);
#else
			next_l += _60fpsDura;
#endif
		}

		if (now > next_r) {
			move_index(irr, SR / freqr);
			if (dist_forward(irr, PL * W) < VL) {
				adjust_reader(irr, PL * W, freqr);
				cout << "oopsr" << endl;
			}
			for (int i = 0; i < VL; ++i)
				tr[i] = pulse_buf_r[(i + int(irr)) % L];

#ifdef RENORM_1
			renorm(tr, maxr, .5, 1.f);
#endif

#ifdef FFT_SYNC
			freqr = get_harmonic(max_frequency(fft_outr));
			next_r += dura(1.f/ freqr);
#else
			next_r += _60fpsDura;
#endif
		}
// -/

		//- Preprocess audio, execute the FFT, and fill fft presentation buff
#ifdef FFT_SYNC
		if (now > prevfft) {
			prevfft = now + _60fpsDura;
			for (int i = 0; i < FFTLEN; ++i) {
				fft_inl[i] = pulse_buf_l[(i * 2 + PL * W) % L] * FFTwindow[i]; // i * 2 downsamples
				fft_inr[i] = pulse_buf_r[(i * 2 + PL * W) % L] * FFTwindow[i];
			}
			ffts_execute(fft_plan, fft_inl, fft_outl);
			ffts_execute(fft_plan, fft_inr, fft_outr);
			for (int i = 0; i < FFTLEN / 2; i++) {
				audio->freq_l[i] = (float)mag(fft_outl, i)/sqrt(FFTLEN);
				audio->freq_r[i] = (float)mag(fft_outr, i)/sqrt(FFTLEN);
			}
		}
#endif
		// -/

		//- Fill presentation buffers
		audio->mtx.lock();
		// Smooth and upsample the wave
#ifdef RENORM_2
		const float Pl = pow(maxl, .1);
		const float Pr = pow(maxr, .1);
#endif

		for (int i = 0; i < VL; ++i) {
			#define upsmpl(s) (s[i/2] + s[(i+1)/2])/2.f
			const float al = upsmpl(tl);
			const float ar = upsmpl(tr);
#ifdef RENORM_2
			audio->audio_l[i] = mix(audio->audio_l[i], al, smoother)+.005*sinArray[i]*(1.f-Pl);
			audio->audio_r[i] = mix(audio->audio_r[i], ar, smoother)+.005*sinArray[(i+VL/4)%VL]*(1.f-Pr); // i+vl/4 : sine -> cosine
#endif
#ifdef RENORM_1
			audio->audio_l[i] = mix(audio->audio_l[i], al, smoother);
			audio->audio_r[i] = mix(audio->audio_r[i], ar, smoother);
#endif
#ifndef RENORM_1
#ifndef RENORM_2
			audio->audio_l[i] = mix(audio->audio_l[i], al, smoother);
			audio->audio_r[i] = mix(audio->audio_r[i], ar, smoother);
#endif
#endif
		}

#ifdef RENORM_2
		const float bound = .75;
		const float spring = .1; // can spring past bound... oops
		renorm(audio->audio_l, maxl, spring, bound);
		renorm(audio->audio_r, maxr, spring, bound);
#endif
		audio->mtx.unlock();
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
