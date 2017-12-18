#pragma once
// vim: foldmarker=\/\-,\-\/:foldmethod=marker

//- Includes
#include <iostream>
#include <chrono>
#include <functional>
#include <thread>
#include <limits>
#include <cmath>
// #include <cstdlib>
#include <stdlib.h>

#include "audio_data.h"
#include "audio_capture.h"
#include "ffts/include/ffts.h"
// -/

namespace chrono = std::chrono;
using std::cout;
using std::endl;
#ifdef WINDOWS
#define Aligned_Alloc(align, sz) _aligned_malloc(sz, align)
#else
#define Aligned_Alloc(align, sz) aligned_alloc(align, sz)
#endif

//- Feature Toggles
// ------------------------------------------------------
// To turn off all the fancy stuff
// undef
//   FFT_SYNC
//   DIFFERENCE_SYNC
// def
//   RENORM_4
// set
//   smoother = 1
//   MIX = 0 in draw.cpp
// This probably could cause screen tearing (or make it observable).
// If you can, turn on vsync and tripple buffering in your graphics driver (easy with nvidia on windows.)
// ------------------------------------------------------
//
// WAVE RENORMALIZATION
// RENORM_1: just normalize the wave by dividing each sample by the max amplitude
// RENORM_2: is used to cause the lissajous to tend towards a circle when the audio 'cuts out'.
// RENORM_3: a bouncy renorm, the springy bounce effect is what occurs when we normalize by using
//           max = mix(max_prev, max_current, parameter); max_prev = max. So if there is a pulse of
//           amplitude in the sound, then a lounder sound is normalized by a smaller maximum and
//           therefore the amplitude is greater than 1. The amplitude will be inflated for a short
//           period of time until the dynamics of the max mixing catch up and max_prev is approximately
//           equal to max_current.
// RENORM_4: use max observed amplitude so far as the max to normalize by.
// CONST_RENORM: use this to scale the raw audio samples by a constant factor.
//
// #define RENORM_1
// #define RENORM_2
#define RENORM_3
// #define RENORM_4
// #define CONST_RENORM 10
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
// Mixes the old buffer of audio with the new buffer of audio.
static const float smoother = .98f;
//static const float smoother = 1.f;
//
// DIFFERENCE_SYNC
// Makes the wave stand almost uncomfortably still. It is nice to watch the phase change when
// a person is humming a tune for instance.
#define DIFFERENCE_SYNC 40
// #define DIFFERENCE_SYNC_LOG
// TODO What is a good value for M?
static const int M = DIFFERENCE_SYNC; // Search an 2*M region for a min. Computes M*VL multiplications
// CONVOLUTIONAL_SYNC
// TODO would be too computationally expensive?
// Make a 2D array set to zero. For each point on the new_wave set the corresponding point in thee
// 2D array to 1. Do the same for the old_wave.
// Convolve the 2D array with a nxn 1's matrix.
// Sum the resulting array to get a real value.
// Minimize this real value for each new_wave in some interval.
// I like this method the best, because what the user sees in the music visualizer is not the wave's height values
// but rather the amount of color delivered to their eyeballs over a 1/10 of a second from a specific point in the plane.
// This convolutional approach could take even more than just the previous wave. The conv could consider the last 5 waves.
// The conv approach will try to condense the stream of points to cover as small of an area as possible.
//
// Maybe this isn't what I want. Because it isn't immediately clear to me that it will always put the waveform where I
// would put the waveform. But where would I put the waveform say if old_wave(t) = noisy_sin(t) + .8 and
// new_wave(t) = noisy_sin(t)? I would shift new_wave so that the periods line up.
// but the conv solution would shift new_wave so that the hills of new_wave cover up the valleys of old_wave.
//
// How about this: Maximize the size and minimize the number of patches of empty area on the screen.
// The convolutional approach maximizes the amount of empty area on the screen, but unfortunately fails on examples like
// above with noisy_sin(t)+.8 and noisy_sin(t). That example maximized both the amount of and the number of patches of
// empty space on the screen. This could use non-overlapping (image size reducing) product sum (by nxn 1's filter)
//
// Should I attempt to minimize the difference between phase(new_wave) and phase(old_wave)? No, what if new_wave is all
// crazy looking and phase minimization tells me to align it some way that only adds more chaos to the screen? I would
// use the phase as returned by the fft, but this phase could be really jittery from frame to frame and could just plain
// not put the wave where it seems like it should be.
//
// I'm trying to minimize the entropy in the information received by the theoretical user who is utterly attentive to my
// awesome artwork XD. The information is basically the set of graphics frames the user sees over a short amount of time.
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
// -/

class audio_processor {
public:

	//- Static Utility Functions
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

		// TODO what does this do?
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
	// TODO this is ridiculous
	static void renorm(float* buff, float& prev_max_amp, float mixer, float scale, int buff_size) {
		float new_max_amp = -16.f;
		for (int i = 0; i < buff_size; ++i)
			if (std::fabs(buff[i]) > new_max_amp)
				new_max_amp = std::fabs(buff[i]);
		prev_max_amp = mix(prev_max_amp, new_max_amp, mixer);
		for (int i = 0; i < buff_size; ++i)
			buff[i] /= (prev_max_amp+0.001f) / scale;
	}
	// Adds the wavelength computed by sample_rate / frequency to r.
	// If w is within VL units in front of r, then adjust r so that this is no longer true.
	// w: index of the writer in the circular buffer
	// r: index of the reader in the circular buffer
	// freq: frequency to compute the wavelength with
	static float advance_index(float w, float r, float freq, float tbl) {
		float wave_len = SR / freq;
		r = move_index(r, wave_len, tbl);
		if (dist_forward(r, w, tbl) < VL) { // if dist from r to w is < what is read by graphics system
			float delta = adjust_reader(r, w, wave_len, tbl);
			r = move_index(r, delta, tbl);
			cout << "Reader too close to writer, adjusting." << endl;
		}
		return r;
	}
	// w: writer
	// r: reader
	// dist: half the amount to search by
	// buff: buffer of audio to compare prev_buffer with
	//
	// TODO not completely testable.
	// buffer sizes: Sure, I could make it all variable, but does that mean the function can work with arbitrary inputs?
	// No, the inputs depend on each other too much.
	// Writing the tests/asserts in anticipation that the inputs would vary is too much work, when I know that the inputs actually will not vary.
	//
	static float difference_sync(float w, float r, int dist, float* prev_buff, float* buff) {
		// TODO compute bounds for search
		//float furthest_search = min(float(dist), dist_forward(rr, iw*ABL, TBL)); not correct
		if (dist_backward(r, w, TBL) > dist && dist_forward(r, w, TBL) > dist) {
			float orig_r = r;
			r = move_index(r, -dist, TBL);
			float min_r = 0;
			float min_mae = std::numeric_limits<float>::infinity();
			for (int i = 0; i < dist*2; ++i) {
				float mae = 0.f;
				for (int j = 0; j < VL; ++j) {
					const float a = buff[(j + int(r)) % TBL];
					const float b = prev_buff[j];
					const float d = a - b; // or |a| - |b|, or a^2 - b^2
					mae += std::fabs(d);
				}
				if (mae < min_mae) {
					min_mae = mae;
					min_r = r;
				}
				r = move_index(r, 1.0f, TBL);
			}

			// Find out how far we've been shifted.
			const float df = dist_forward(orig_r , min_r, TBL);
			const float db = dist_backward(orig_r , min_r, TBL);
			float diff;
			if (df < db)
				diff = df;
			else
				diff = -db;

			// We want the pointer to be somwhere around where advanced_index put it. We ONLY want to make a small nudge left or right
			// in order to increase the similarity between this frame and the previous. We do not want to move the index too far.
			// The histogram of where the index is moved can be viewed by defining DIFFERENCE_SYNC_LOG. The histogram shows that,
			// most of the time the index is put either at the ends or somewhere near the center.
			if (std::fabs(diff) < dist-1)
				r = min_r;
			else
				r = orig_r;

			#ifdef DIFFERENCE_SYNC_LOG
			static int counts[2*M] = {};
			const int diff_int = int(diff);
			if (diff_int < dist && diff_int >= -dist)
				counts[diff_int+dist] += 1;
			for (int i = 0; i < dist*2; ++i)
				cout << counts[i] << ",";
			cout << endl;
			#endif
		}
		return r;
	}
	template<typename T>
	static T min(T a, T b) {
		if (a < b) return a;
		return b;
	}
	// -/

	//- Destructor
	~audio_processor() {
		free(audio_sink->audio_l);
		free(audio_sink->audio_r);
		free(audio_sink->freq_l);
		free(audio_sink->freq_r);
		free(audio_buff_l);
		free(audio_buff_r);
		free(staging_buff_l);
		free(staging_buff_r);

		free(fft_inl);
		free(fft_inr);
		free(fft_outl);
		free(fft_outl);
		free(FFTwindow);

		free(prev_buff_l);
		free(prev_buff_r);

		ffts_free(fft_plan);

		#ifdef RENORM_2
		free(sinArray);
		#endif
	}
	// -/

	//- Constructor
	// audio_processor owns the memory it allocates here.
	audio_processor(struct audio_data* _audio_sink,
	                std::function<void(float*, float*, int)> _pcm_getter,
	                std::function<void(int, int, struct audio_data*)> _audio_initializer) {
		audio_sink = _audio_sink;
		pcm_getter = _pcm_getter;
		audio_initializer = _audio_initializer;

		//- Internal audio buffers
		audio_buff_l = (float*)calloc(TBL, sizeof(float));
		audio_buff_r = (float*)calloc(TBL, sizeof(float));
		// -/

		prev_buff_l = (float*)calloc(VL, sizeof(float));
		prev_buff_r = (float*)calloc(VL, sizeof(float));

		//- Output buffers
		audio_sink->audio_l = (float*)calloc(VL, sizeof(float));
		audio_sink->audio_r = (float*)calloc(VL, sizeof(float));
		audio_sink->freq_l = (float*)calloc(VL, sizeof(float));
		audio_sink->freq_r = (float*)calloc(VL, sizeof(float));
		// -/

		audio_initializer(ABL, SR, audio_sink);

		//- Smoothing buffers
		// Helps smooth the appearance of the waveform. Useful when we're not updating the waveform data at 60fps.
		// Holds the audio that is just about ready to be sent to the visualizer.
		// Audio in these buffers are 'staged' to be visualized
		// The staging buffers are mixed with the final output buffers 'continuously'
		// until the staging buffers are updated again. The mixing parameter (smoother) controls a molasses like effect.
		staging_buff_l = (float*)calloc(VL + 1, sizeof(float));
		staging_buff_r = (float*)calloc(VL + 1, sizeof(float));
		// -/

		//- FFT setup
		const int N = FFTLEN;
		fft_plan = ffts_init_1d_real(N, FFTS_FORWARD);
		fft_inl = (float*)Aligned_Alloc(32, N*sizeof(float));
		fft_inr = (float*)Aligned_Alloc(32, N*sizeof(float));
		fft_outl = (float*)Aligned_Alloc(32, (N/2 + 1)*(2*sizeof(float))); // alloc N/2+1 complex numbers (sizeof(complex) == 2*sizeof(float))
		fft_outr = (float*)Aligned_Alloc(32, (N/2 + 1)*(2*sizeof(float)));
		FFTwindow = (float*)malloc(N*sizeof(float));
		for (int i = 0; i < N; ++i)
			FFTwindow[i] = (1.f - cosf(2.f*3.141592f * i / float(N))) / 2.f; // sin(x)^2
		// -/

		//- Buffer Indice Management
		iw = 0; // The index of the writer in the audio buffers, of the newest sample in the
		        // buffers, and of a discontinuity in the waveform
		rl = TBL - VL; // Index of a reader in the audio buffers (left channel)
		rr = TBL - VL;
		freql = 60.f; // Holds a harmonic frequency of the dominant frequency of the audio.
		freqr = freql; // we capture an image of the waveform at this rate
		// -/

		//- Wave Renormalizer
		channel_max_l = 1.f;
		channel_max_r = 1.f;
		#ifdef RENORM_2
		sinArray = (float*)malloc(sizeof(float)*VL);
		for (int i = 0; i < VL; ++i)
			sinArray[i] = sinf(6.288f * i / float(VL));
		#endif
		// -/

		#ifdef DIFFERENCE_SYNC_LOG
		// for (int i = 0; i < 2*M; ++i)
		// 	counts[i] = 0;
		#endif
	};
	// -/

	void service_channel(float  w,
	                     float& r,
	                     float* prev_buff,
	                     float* audio_buff,
	                     float* staging_buff,
											 float* fft_out,
	                     float& freq,
											 float& channel_max,
	                     chrono::steady_clock::time_point& now,
	                     chrono::steady_clock::time_point& next_t) {
			if (now > next_t) {

				r = advance_index(w, r, freq, TBL);

				#ifdef DIFFERENCE_SYNC
				r = difference_sync(w, r, M, prev_buff, audio_buff);
				#endif

				for (int i = 0; i < VL; ++i) {
					staging_buff[i] = audio_buff[(i + int(r)) % TBL];

					#ifdef DIFFERENCE_SYNC
					// The mix helps reduce jitter in the choice of where to place the next wave.
					prev_buff[i] = mix(staging_buff[i], prev_buff[i], .95);
					#endif

					#ifdef RENORM_4
					if (staging_buff[i] > max_amplitude_so_far)
						max_amplitude_so_far = staging_buff[i];
					#endif
				}

				#ifdef RENORM_1
				renorm(staging_buff, channel_max, .5, 1.f, VL);
				#endif
				#ifdef RENORM_4
				renorm(staging_buff, max_amplitude_so_far, 0.f, .83f, VL);
				#endif

				#ifdef FFT_SYNC
				freq = get_harmonic(max_frequency(fft_out));
				next_t += dura(1.f/freq);
				#else
				next_t += dura(1.f/60.f);
				#endif
			}
				// // cout << "left" << endl;
				// const float new_rl = advance_index(iw*ABL, rl, freql, TBL);
				// const float SAMPLES = dist_forward(rl, new_rl, TBL);
				// rl = new_rl;
				// const float TIME = SAMPLES / SR;
				// next_l += dura(TIME);
	}

	//- Program
	int run() {

		//- Time Management
		auto now = chrono::steady_clock::now();
		auto _60fpsDura = dura(1.f / 60.f);
		auto next_l = now + _60fpsDura;
		auto next_r = now + _60fpsDura;
		auto next_fft = now + _60fpsDura; // Compute the fft only once every 1/60 of a second
		// -/

		while (!audio_sink->thread_join) { // Break when audio_sink->thread_join true
			fps(now);
			now = chrono::steady_clock::now();
			// We want to use next += dura(1/freq) in the loop below and not next = now + dura(1/freq) because ... TODO,
			// BUT if now >> next, then we probably were stalled in pcm_getter and should update the next times
			if (now - next_l > std::chrono::milliseconds(17*4)) { // less than 60/2/2 fps
				next_r = now;
				next_l = now;
				next_fft = now;
			}

			// TODO test this function. The samples it writes to the circular buffer should be the same for the same sound
			// on windows and on linux.
			// TODO On windows this can prevent the app from closing if the music is paused.
			// auto perf_timepoint = std::chrono::steady_clock::now();
			pcm_getter(audio_buff_l+iw*ABL, audio_buff_r+iw*ABL, ABL);
			// cout << (std::chrono::steady_clock::now() - perf_timepoint).count()/1e9 << endl;

			//- Manage indices and fill smoothing buffers
			iw++;
			iw %= ABN; // iw * ABN marks the index of the discontinuity in the circular buffer

			service_channel(iw*ABL,         // writer
	                    rl,             // reader
	                    prev_buff_l,    // previously visualized audio
	                    audio_buff_l,   // audio collected from system audio stream
	                    staging_buff_l, // audio for visualizer goes here
											fft_outl,       // fft complex outputs
	                    freql,          // most recent dominant frequency < 600hz
											channel_max_l,  // channel renormalization term
											now,            // current time point
	                    next_l);        // update staging at this timepoint again

			service_channel(iw*ABL,
	                    rr,
	                    prev_buff_r,
	                    audio_buff_r,
	                    staging_buff_r,
											fft_outr,
	                    freqr,
											channel_max_r,
											now,
	                    next_r);
			// -/

			//- Execute fft and fill fft output buff
			// This downsamples and windows the audio
			if (now > next_fft) {
				next_fft += _60fpsDura;
				for (int i = 0; i < FFTLEN; ++i) {
					#define downsmpl(s) s[(i*2+ABL*iw)%TBL]
					fft_inl[i] = downsmpl(audio_buff_l) * FFTwindow[i];
					fft_inr[i] = downsmpl(audio_buff_r) * FFTwindow[i];
					#undef downsmpl
				}
				ffts_execute(fft_plan, fft_inl, fft_outl);
				ffts_execute(fft_plan, fft_inr, fft_outr);
				for (int i = 0; i < VL; i++) {
					audio_sink->freq_l[i] = 2*mag(fft_outl, i)/sqrt(FFTLEN);
					audio_sink->freq_r[i] = 2*mag(fft_outr, i)/sqrt(FFTLEN);
				}
			}
			// -/

			//- Fill output buffers
			audio_sink->mtx.lock();

			// Smooth and upsample the wave
			#ifdef RENORM_2
			const float pow_max_l = pow(channel_max_l, .1);
			const float pow_max_r = pow(channel_max_r, .1);
			#endif
			for (int i = 0; i < VL; ++i) {
				float oldl, oldr, newl, newr, mixl, mixr;

				// we only observe VL/2 samples of audio
				#define upsmpl(s) (s[i/2] + s[(i+1)/2])/2.f
				newl = upsmpl(staging_buff_l);
				newr = upsmpl(staging_buff_r);
				#undef upsmpl

				oldl = audio_sink->audio_l[i];
				oldr = audio_sink->audio_r[i];

				mixl = mix(oldl, newl, smoother);
				mixr = mix(oldr, newr, smoother);

				#ifdef CONST_RENORM
				mixl *= CONST_RENORM;
				mixr *= CONST_RENORM;
				#endif

				#ifdef RENORM_2
				mixl += (1.f-pow_max_l)*.005*sinArray[i];
				mixr += (1.f-pow_max_r)*.005*sinArray[(i+VL/4)%VL]; // i+vl/4 : sine -> cosine
				#endif

				audio_sink->audio_l[i] = mixl;
				audio_sink->audio_r[i] = mixr;
			}

			#if defined(RENORM_2) || defined(RENORM_3)
			const float bound = .66;
			const float spring = .3; // can spring past bound... oops
			renorm(audio_sink->audio_l, channel_max_l, spring, bound, VL);
			renorm(audio_sink->audio_r, channel_max_r, spring, bound, VL);
			#endif

			audio_sink->mtx.unlock();
			// -/

			// TODO how does DIFFERENCE_SYNC affect this?
			// fps(now); // Should be about 90 fps for the sound loop
			// Turning this on causes the indices to get all fd up, this makes sense because ... TODO
			// std::this_thread::sleep_for(std::chrono::milliseconds(13));
		}
		audio_destroy();
		return 0;
	}
	// -/

	//- Members
private:
	float* prev_buff_l;
	float* prev_buff_r;

	float* audio_buff_l;
	float* audio_buff_r;
	float* staging_buff_l;
	float* staging_buff_r;
	ffts_plan_t* fft_plan;

	float* fft_outl;
	float* fft_inl;
	float* fft_outr;
	float* fft_inr;
	float* FFTwindow;

	int iw;
	float rl;
	float rr;
	float freql;
	float freqr;

	float channel_max_l = 1.f;
	float channel_max_r = 1.f;
	#ifdef RENORM_2
	float* sinArray;
	#endif

	float max_amplitude_so_far = 0.;

	std::function<void(float*, float*, int)> pcm_getter;
	std::function<void(int, int, struct audio_data*)> audio_initializer;

	struct audio_data* audio_sink;
	// -/
};
