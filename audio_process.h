#pragma once
// vim: foldmarker=\/\-,\-\/:foldmethod=marker

#include <iostream>
#include <chrono>
#include <thread>
#include <limits>
#include <cmath>
#include <string.h>

#include "audio_data.h"
#include "audio_capture.h"
#include "ffts/include/ffts.h"

namespace chrono = std::chrono;
using std::cout;
using std::endl;

//- Feature Toggles
// WAVE RENORMALIZATION
// RENORM_1: just normalize the wave by dividing each sample by the max amplitude
// RENORM_2: is used to cause the lissajous to tend towards a circle when the audio 'cuts out'.
// RENORM_3: a bouncy renorm, the springy bounce effect is what occurs when we normalize by using
//           max = mix(max_prev, max_current, parameter); max_prev = max. So if there is a pulse of
//           amplitude in the sound, then a lounder sound is normalized by a smaller maximum and
//           therefore the amplitude is greater than 1. The amplitude will be inflated for a short
//           period of time until the dynamics of the max mixing catch up and max_prev is approximately
//           equal to max_current.
// CONST_RENORM: use this to scale the raw audio samples by a constant factor. On my pc I reduce the
//           amplitude of all system sounds by about 15dB. I do this because the audio is way too loud
//           in my headphones when the Windows master audio level is at a mere 5 to 10 percent volume setting.
//           However, when the system sound is amplified like this, then the get_pcm function returns audio
//           samples that are also amplified. This motivates a functionality to allow the user to scale the
//           audio waveform by a constant factor in the program.
//           TODO: check that the windows GetBuffer function does not have some parameter that would fix this issue.
//                 for instance a parameter that would fetch audio before any filters are applied by some other
//                 program.
// Leave all RENORM_s undefined for no renorm.
// TODO check if any preprocessing is done on the windows audio stream? Or use a renorm that normalizes using
//      max = max_seen_so_far?
//#define RENORM_1
//#define RENORM_2
#define RENORM_3
//#define CONST_RENORM 60
//
// WAVE FFT STABILIZATION
// Use the fft to intelligently choose when to advance the waveform.
// Imagine running along next to a audio wave (that is not moving), and you want to take a
// picture of the audio wave with a camera such that each picture looks almost exactly the same.
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
// our measure off (put lots of variance in succcessive wavelength measurements).
//
// The wavelength of a waveform is related to the frequency by this equation
// wavelength = velocity/frequency
//
// velocity = sample rate
// wavelength = number of samples of one period of the wave
// frequency = dominant frequency of the waveform
//
// In order to find the wavelength of the audio wave we can take the fft of a section of
// the wave and then use that to choose at what distances we should move the audio pointer by.
// Is there a better way to get the wavelength? idk, this works pretty good.
//
// Many sounds have highly variable wavelengths. Often the wavelength will change almost
// instantly, making any choice of wavelength we've made incorrect and causing the next frame
// to appear completely out of phase with the current.
//
// Improvements?
//     constrained minimization of successive frames to push snapshot points around the timeline.
//         It's not just the difference between successive frames that I want to minimize
//         I want to minimize the whole sum of differences, so that no interval could have a set
//         of snapshot points arranged in any other way (given the constraints) such that the
//         total sum of differences in that interval was smaller. right?
//     interpolate between current frame and next frame?
#define FFT_SYNC
//
// TODO why in the code do I use the frequency to both check WHEN to update next and WHERE to put
// the index next. Shouldn't I only need to do one in order to find the next ideal frame of audio?
//
// EXPONENTIAL SMOOTHER
// A smaller smoother makes the wave more like molasses.
// A larger smoother makes the wave more snappy.
// Smooths the old buffer of audio with the new buffer of audio.
static const float smoother = 1.f;
// -/

//- Constants
//
// ABL length of the system audio capture buffer (in frames)
// ABN number of system audio capture buffers to keep
// TBL total audio buffer length. length of all ABN buffers combined (in frames)
// VL length of visualizer 1D texture buffers. Length of audio data that this module 'outputs' (in frames)
// C channel count of audio
// SR requested sample rate of the audio
// SRF sample rate of audio given to FFT
const int ABL = 512;
const int ABN = 16;
const int TBL = ABL * ABN;
const int FFTLEN = TBL / 2;
const int VL = VISUALIZER_BUFSIZE; // defined in audio_data.h
const int C = 2;
const int SR = 48000;
const int SRF = SR / 2;

//////////////////////////////////IMPORTANT PARAMETER INFO
// I think the FFT looks best when it has 8192/48000, or 4096/24000, time granularity. However, I
//   like the wave to be 96000hz just because then it fits on the screen nice.
// To have both an FFT on 24000hz data and to display 96000hz waveform data, I ask
//   pulseaudio to output 48000hz and then resample accordingly.
// I've compared the 4096 sample FFT over 24000hz data to the 8192 sample FFT over 48000hz data
//   and they are surprisingly similar. I couldn't tell a difference really.
// I've also compared resampling with ffmpeg to my current naive impl. Hard to notice a
//   difference.
// Of course the user might want to change all this. That's for another day, and another program :D
// -/

class audio_processor {
public:

	//- Functions
	// Returns the magnitude of the ith complex number of the fft output stored in f.
	static float mag(float* f, int i) {
		return sqrt(f[i*2]*f[i*2] + f[i*2+1]*f[i*2+1]);
	}
	// Returns the bin holding the max frequency of the fft. We only consider the first 100 bins.
	static int max_bin(float* f) {
		float max = 0.f;
		int max_i = 0;
		// catch frequencies from 5.86 to 586 (that is i * SRF / FFTLEN for i from 0 to 100)
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
	// Move p around the circular buffer by the amound and direction delta
	// tbl: total buffer length of the circular buffer
	static float move_index(float p, float delta, int tbl) {
		p += delta;
		if (p > tbl) {
			p -= tbl;
		} else if (p < 0.f) {
			p += tbl;
		}
		return p;
	}
	static float dist_forward(float from, float to, int tbl) {
		float d = to - from;
		if (d < 0)
			d += tbl;
		return d;
	}
	static float dist_backward(float from, float to, int tbl) {
		float d = from - to;
		if (d < 0)
			d += tbl;
		return d;
	}
	static float sign(float x) {
		return std::copysign(1.f, x);
	}
	// Attempts to separate two points, in a circular buffer, r (reader) and w (writer)
	// by distance tbl/2.f by moving r in steps of size step_size.
	// Returns the delta that r should be moved by
	static float adjust_reader(float r, float w, float step_size, int tbl) {
		const float a = w - r;
		const float b = fabs(a) - tbl / 2.f;
		const float foo = fabs(b);
		// minimize foo(r)
		// foo(r) = |b(a(r))|
		//        = ||w-r|-tbl/2|
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
		const float dadr = -1.f;
		const float dbdr = sign(a) * dadr;
		const float dfoodr = sign(b) * dbdr;
		// move r by a magnitude of foo in the -foo'(r) direction, but only take
		// steps of size step_size
		float grid = step_size;
		grid = ceil(foo / grid) * grid;
		return -dfoodr * grid;
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
	// Bound freq into the interval [24, 60] by multiplying by factors of 2.
	static float get_harmonic(float freq) {

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
		// while (o > 61.f) o /= 2.f; // dividing frequency by two does produce the ghosting effect
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
	// Adds the wavelength computed by sample_rate / frequency to r.
	// If w is within VL units in front of r, then adjust r so that this is no longer true.
	// w: index of the writer in the circular buffer
	// r: index of the reader in the circular buffer
	// freq: frequency to compute the wavelength with
	static float advance_index(float w, float r, float freq, float tbl) {
		float wave_len = SR / freq;
		r = move_index(r, wave_len, tbl);
		//cout << "w:" << w << "\t" << "r:" << r;
		if (dist_forward(r, w, tbl) < VL) { // if dist from r to w is < what is read by graphics system
			float delta = adjust_reader(r, w, wave_len, tbl);
			r = move_index(r, delta, tbl);
			cout << "Reader too close to discontinuity, adjusting." << endl;
			//cout << "\tr_fix:" << r;
		}
		//cout << endl;
		return r;
	}
	template<typename T>
	static T min(T a, T b) {
		if (a < b) return a;
		return b;
	}
	// -/

	~audio_processor() {
		free(audio_sink->audio_l);
		free(audio_sink->audio_r);
		free(audio_sink->freq_l);
		free(audio_sink->freq_r);
		free(audio_buf_l);
		free(audio_buf_r);
		free(staging_buff_l);
		free(staging_buff_r);

		ffts_free(fft_plan);

		#ifdef RENORM_2
		free(sinArray);
		#endif
	}

	// audio_processor owns the memory it allocates here.
	audio_processor(struct audio_data* _audio_sink,
	                std::function<void(float*, float*, int)> _pcm_getter,
	                std::function<void(int, int, struct audio_data*)> _audio_initializer) {
		audio_sink = _audio_sink;
		pcm_getter = _pcm_getter;
		audio_initializer = _audio_initializer;

		//- Internal audio buffers
		audio_buf_l = (float*)calloc(TBL * C, sizeof(float));
		audio_buf_r = audio_buf_l + TBL;
		// -/

		//- Output buffers
		audio_sink->audio_l = (float*)calloc(VL * C * 2, sizeof(float));
		audio_sink->audio_r = audio_sink->audio_l + VL;
		audio_sink->freq_l = audio_sink->audio_r + VL;
		audio_sink->freq_r = audio_sink->freq_l + VL;
		// -/

		//- Smoothing buffers
		// Helps smooth the appearance of the waveform. Useful when we're not updating the waveform data at 60fps.
		// Holds the audio that is just about ready to be sent to the visualizer.
		// Audio in these buffers are 'staged' to be visualized
		// The staging buffers are mixed with the final output buffers 'continuously'
		// until the staging buffers are updated again. The mixing parameter (smoother) controls a molasses like effect.
		staging_buff_l = (float*)calloc(VL * C + 2, sizeof(float));
		staging_buff_r = staging_buff_l + VL + 1;
		// -/

		//- FFT setup
		fft_plan = ffts_init_1d_real(FFTLEN, FFTS_FORWARD);
		for (int i = 0; i < FFTLEN; ++i)
			FFTwindow[i] = (1.f - cosf(2.f*3.141592f * i / float(FFTLEN))) / 2.f;
		// -/

		audio_initializer(ABL, SR, audio_sink);

		//- Buffer Indice Management
		iw = 0; // The index of the writer in the audio buffers, of the newest sample in the
			   // buffers, and of a discontinuity in the waveform
		rl = 0; // Index of a reader in the audio buffers (left channel)
		rr = rl;
		freql = 60.f; // Holds a harmonic frequency of the dominant frequency of the audio.
		freqr = freql; // we capture an image of the waveform at this rate
		// -/

		//- Wave Renormalizer
		maxl = 1.f;
		maxr = 1.f;
		#ifdef RENORM_2
		sinArray = (float*)malloc(sizeof(float)*VL);
		for (int i = 0; i < VL; ++i)
			sinArray[i] = sinf(6.288f * i / float(VL));
		#endif
		// -/
	};

	int run() {

		//- Time Management
		auto now = chrono::steady_clock::now();
		auto _60fpsDura = dura(1.f / 60.f);
		auto next_l = now + _60fpsDura;
		auto next_r = next_l;
		auto next_fft = next_r; // Compute the fft only once every 1/60 of a second
		// -/

		while (true) { // Break when audio_sink->thread_join true
			now = chrono::steady_clock::now();
			// We want to use next += dura(1/freq) in the loop below and not next = now + dura(1/freq) because ... TODO,
			// BUT if now >> next, then we probably were stalled in pcm_getter and should update the next times
			if (now - next_l > std::chrono::milliseconds(17*4)) { // less than 60/2/2 fps
				next_r = next_l = now;
			}

			// TODO On windows this can prevent the app from closing if the music is paused.
			pcm_getter(audio_buf_l+iw*ABL, audio_buf_r+iw*ABL, ABL);

			//- Manage indices and fill smoothing buffers
			iw++;
			iw %= ABN;
			if (now > next_l) {

				rl = advance_index(iw*ABL, rl, freql, TBL);
				for (int i = 0; i < VL; ++i)
					staging_buff_l[i] = audio_buf_l[(i + int(rl)) % TBL];

				#ifdef RENORM_1
				renorm(staging_buff_l, maxl, .5, 1.f);
				#endif

				#ifdef FFT_SYNC
				freql = get_harmonic(max_frequency(fft_outl));
				next_l += dura(1.f / freql);
				#else
				next_l += _60fpsDura;
				#endif
			}

			if (now > next_r) {

				rr = advance_index(iw*ABL, rr, freqr, TBL);
				for (int i = 0; i < VL; ++i)
					staging_buff_r[i] = audio_buf_r[(i + int(rr)) % TBL];

				#ifdef RENORM_1
				renorm(staging_buff_r, maxr, .5, 1.f);
				#endif

				#ifdef FFT_SYNC
				freqr = get_harmonic(max_frequency(fft_outr));
				next_r += dura(1.f / freqr);
				#else
				next_r += _60fpsDura;
				#endif
			}
			// -/

			//- Execute fft and fill fft output buff
			// This downsamples and windows the audio
			if (now > next_fft) {
				next_fft += _60fpsDura;
				for (int i = 0; i < FFTLEN; ++i) {
					fft_inl[i] = audio_buf_l[(i * 2 + ABL * iw) % TBL] * FFTwindow[i]; // i * 2 downsamples
					fft_inr[i] = audio_buf_r[(i * 2 + ABL * iw) % TBL] * FFTwindow[i];
				}
				ffts_execute(fft_plan, fft_inl, fft_outl);
				ffts_execute(fft_plan, fft_inr, fft_outr);
				for (int i = 0; i < FFTLEN / 2; i++) {
					audio_sink->freq_l[i] = mag(fft_outl, i)/sqrt(FFTLEN);
					audio_sink->freq_r[i] = mag(fft_outr, i)/sqrt(FFTLEN);
				}
			}
			// -/

			//- Fill output buffers
			audio_sink->mtx.lock();
			// Smooth and upsample the wave

			#if defined(RENORM_2)
			const float Pl = pow(maxl, .1);
			const float Pr = pow(maxr, .1);
			#endif

			for (int i = 0; i < VL; ++i) {
				#define upsmpl(s) (s[i/2] + s[(i+1)/2])/2.f
				const float al = upsmpl(staging_buff_l);
				const float ar = upsmpl(staging_buff_r);
				#undef upsmpl
				#ifdef RENORM_2
				audio_sink->audio_l[i] = mix(audio_sink->audio_l[i], al, smoother)+.005*sinArray[i]*(1.f-Pl);
				audio_sink->audio_r[i] = mix(audio_sink->audio_r[i], ar, smoother)+.005*sinArray[(i+VL/4)%VL]*(1.f-Pr); // i+vl/4 : sine -> cosine
				#else
				audio_sink->audio_l[i] = mix(audio_sink->audio_l[i], al, smoother);
				audio_sink->audio_r[i] = mix(audio_sink->audio_r[i], ar, smoother);
				#endif

				#ifdef CONST_RENORM
				audio_sink->audio_l[i] *= CONST_RENORM;
				audio_sink->audio_r[i] *= CONST_RENORM;
				#endif
			}

			#if defined(RENORM_2) || defined(RENORM_3)
			const float bound = .7;
			const float spring = .3; // can spring past bound... oops
			renorm(audio_sink->audio_l, maxl, spring, bound);
			renorm(audio_sink->audio_r, maxr, spring, bound);
			#endif
			audio_sink->mtx.unlock();
			// -/

			if (audio_sink->thread_join) {
				audio_destroy();
				break;
			}

			// fps(now);
			// Turning this on causes the indices to get all fd up, this makes sense because ... TODO
			// std::this_thread::sleep_for(std::chrono::milliseconds(13));
		}
		return 0;
	}

private:
	float* audio_buf_l;
	float* audio_buf_r;
	float* staging_buff_l;
	float* staging_buff_r;
	ffts_plan_t* fft_plan;

	#ifdef WINDOWS
	#define ALIGN __declspec(align(32))
	#else
	#define ALIGN __attribute__((aligned(32)))
	#endif
	float ALIGN fft_outl[FFTLEN/2+1];
	float ALIGN fft_inl[FFTLEN];
	float ALIGN fft_outr[FFTLEN/2+1];
	float ALIGN fft_inr[FFTLEN];
	float FFTwindow[FFTLEN];

	int iw;
	float rl;
	float rr;
	float freql;
	float freqr;

	float maxl = 1.f;
	float maxr = 1.f;
	#ifdef RENORM_2
	float* sinArray;
	#endif

	std::function<void(float*, float*, int)> pcm_getter;
	std::function<void(int, int, struct audio_data*)> audio_initializer;

	struct audio_data* audio_sink;
};
