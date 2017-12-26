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

// I think the FFT looks best when it has 8192/48000, or 4096/24000, time granularity. However, I
//   like the wave to be 96000hz just because then it fits on the screen nice.
// To have both an FFT on 24000hz data and to display 96000hz waveform data, I ask
//   pulseaudio to output 48000hz and then resample accordingly.
// I've compared the 4096 sample FFT over 24000hz data to the 8192 sample FFT over 48000hz data
//   and they are surprisingly similar. I couldn't tell a difference really.
// I've also compared resampling with ffmpeg to my current naive impl. Hard to notice a difference.
// -/

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
#define FFT_SYNC
//
// TODO why in the code do I use the frequency to both check WHEN to update next and WHERE to put
// the index next. Shouldn't I only need to do one in order to find the next ideal frame of audio?
//
// EXPONENTIAL SMOOTHER
// A smaller smoother makes the wave more like molasses.
// A larger smoother makes the wave more snappy.
// Mixes the old buffer of audio with the new buffer of audio.
static const float smoother = .97f;
// static const float smoother = 1.f;
//
// DIFFERENCE_SYNC
// Makes the wave stand almost uncomfortably still. It is nice to watch the phase change when
// a person is humming a tune for instance.
#define DIFFERENCE_SYNC
// #define DIFFERENCE_SYNC_HISTOGRAM_LOG
// If history > 1, then difference_sync compares agains multiple previous frames

// Observations:
// if I set these params
// frames: 3
// gran: 4
// range:16*gran
// then Slow Motion (by TEB) visualizes ok, but there is very noticable drift in the wave
// during the intro (sound=clean constant tone)
// But if I set
// frames: 9
// then drift is reduced (almost eliminated) throughout the whole song.
// This observation shows that the history frames help establish a correlation
// in frame placement (horizontal shift) between new frames and older frames.
#define HISTORY_NUM_FRAMES 9
#define HISTORY_SEARCH_GRANULARITY 4
#define HISTORY_SEARCH_RANGE 16*HISTORY_SEARCH_GRANULARITY
static const int HISTORY_BUFF_SZ = VL/2; // we only load VL/2 samples into the visualzier
// The performance cost for the history_search/difference_sync is
//
// 2 * HISTORY_SEARCH_RANGE * HISTORY_NUM_FRAMES * HISTORY_BUFF_SZ
// ---------------------------------------------------------------
//                 HISTORY_SEARCH_GRANULARITY
//
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
	static int move_index(int p, int delta, int tbl) {
		p += delta;
		if (p >= tbl) {
			p -= tbl;
		} else if (p < 0) {
			p += tbl;
		}
		return p;
	}
	static int dist_forward(int from, int to, int tbl) {
		int d = to - from;
		if (d < 0)
			d += tbl;
		return d;
	}
	static int dist_backward(int from, int to, int tbl) {
		return dist_forward(to, from, tbl);
	}
	static int sign(int x) {
		if (x < 0) return -1;
		else return 1;
	}
	// Attempts to separate two points, in a circular buffer, r (reader) and w (writer)
	// by distance tbl/2.f by moving r in steps of size step_size.
	// Returns the delta that r should be moved by
	static int adjust_reader(int r, int w, int step_size, int tbl) {
		const int a = w - r;
		const int b = std::abs(a) - tbl / 2;
		const int foo = std::abs(b);
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
		const int dadr = -1;
		const int dbdr = sign(a) * dadr;
		const int dfoodr = sign(b) * dbdr;
		// move r by a magnitude of foo in the -foo'(r) direction, but only take
		// steps of size step_size
		int grid = step_size;
		grid = int(std::ceil(foo / float(grid))) * grid;
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
	static float get_harmonic_less_than(float freq, float thres) {
		//while (freq > 121.f)
		//	freq /= 2.f;
		float a = std::log2f(freq);
		float b = std::log2f(thres);
		if (a > b)
			freq *= std::pow(2.f, std::floor(b-a));
		if (!std::isnormal(freq))
			freq = 60.f;
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
	static int advance_index(int w, int r, float freq, int tbl) {
		int wave_len = int(SR / freq + .5f);
		r = move_index(r, wave_len, tbl);
		if (dist_forward(r, w, tbl) < VL) { // if dist from r to w is < what is read by graphics system
			int delta = adjust_reader(r, w, wave_len, tbl);
			r = move_index(r, delta, tbl);
			cout << "Reader too close to writer, adjusting." << endl;
		}
		return r;
	}
	// Compute the manhattan distance between two vectors.
	// a_circular: a circular buffer containing the first vector
	// b_buff: a buffer containing the second vector
	// a_sz: circumference of a_circular
	// b_sz: length of b_buff
	static float manhattan_dist(float* a_circular, float* b_buff, int a_offset, int a_sz, int b_sz) {
		float md = 0.f;
		for (int i = 0; i < b_sz; ++i) {
			const float xi_1 = a_circular[(i + a_offset) % a_sz];
			const float xi_2 = b_buff[i];
			md += std::fabs(xi_1-xi_2);
			//md += std::pow(xi_1-xi_2, 2);
		}
		return md;
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
	static int difference_sync(int w, int r, int dist, float* prev_buff[HISTORY_NUM_FRAMES], int frame_id, float* buff) {
		if (dist_backward(r, w, TBL) > dist && dist_forward(r, w, TBL) > dist) {
			int orig_r = r;
			r = move_index(r, -dist, TBL);

			// Find r that gives best similarity between buff and prev_buff
			int min_r = orig_r;
			float min_md = std::numeric_limits<float>::infinity();
			for (int i = 0; i < dist*2/HISTORY_SEARCH_GRANULARITY; ++i) {
				float md = 0.f;
				for (int b = 0; b < HISTORY_NUM_FRAMES; ++b) {
					int cur_buf = (frame_id + b) % HISTORY_NUM_FRAMES;
					//md += mix(.5f, 1.f, b/float(HISTORY_NUM_FRAMES)) * manhattan_dist(buff, prev_buff[cur_buf], r, TBL, HISTORY_BUFF_SZ);
					md += manhattan_dist(buff, prev_buff[cur_buf], r, TBL, HISTORY_BUFF_SZ);
				}
				if (md < min_md) {
					min_md = md;
					min_r = r;
				}
				r = move_index(r, HISTORY_SEARCH_GRANULARITY, TBL);
			}

			// Find out how far we've been shifted.
			const int df = dist_forward(orig_r, min_r, TBL);
			const int db = dist_backward(orig_r, min_r, TBL);
			int diff;
			if (df < db)
				diff = df;
			else
				diff = -db;

			// We want the pointer to be somewhere around where advanced_index put it. We ONLY want to make a small nudge left or right
			// in order to increase the similarity between this frame and the previous. We do not want to move the index too far.
			// The histogram of where the index is moved can be viewed by defining DIFFERENCE_SYNC_HISTOGRAM_LOG. The histogram shows that,
			// most of the time the index is put either at the ends or somewhere near the center.
			if (std::abs(diff) < dist-1)
				r = min_r;
			else
				r = orig_r;

			#ifdef DIFFERENCE_SYNC_HISTOGRAM_LOG
			static int counts[2*HISTORY_SEARCH_RANGE] = {};
			if (diff < dist && diff >= -dist)
				counts[diff+dist] += 1;
			if (frame_id % 1000 == 0) {
				for (int i = 0; i < dist*2; i+=HISTORY_SEARCH_GRANULARITY)
					cout << counts[i] << ",";
				cout << endl;
			}
			#endif
		}
		return r;
	}
	// -/

	//- Constructor
	// audio_processor owns the memory it allocates here.
	audio_processor(struct audio_data* _audio_sink,
	                std::function<void(float*, float*, int)> _pcm_getter,
	                std::function<void(int, int)> _audio_initializer) {
		audio_sink = _audio_sink;
		pcm_getter = _pcm_getter;
		audio_initializer = _audio_initializer;

		//- Internal audio buffers
		audio_buff_l = (float*)calloc(TBL, sizeof(float));
		audio_buff_r = (float*)calloc(TBL, sizeof(float));
		// -/

		#ifdef DIFFERENCE_SYNC
		for (int i = 0; i < HISTORY_NUM_FRAMES; ++i) {
			prev_buff_l[i] = (float*)calloc(HISTORY_BUFF_SZ, sizeof(float));
			prev_buff_r[i] = (float*)calloc(HISTORY_BUFF_SZ, sizeof(float));
		}
		#endif

		//- Output buffers
		audio_sink->audio_l = (float*)calloc(VL, sizeof(float));
		audio_sink->audio_r = (float*)calloc(VL, sizeof(float));
		audio_sink->freq_l = (float*)calloc(VL, sizeof(float));
		audio_sink->freq_r = (float*)calloc(VL, sizeof(float));
		// -/

		audio_initializer(ABL, SR);

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
		writer = 0; // The index of the writer in the audio buffers, of the newest sample in the
		            // buffers, and of a discontinuity in the waveform
		reader_l = TBL - VL; // Index of a reader in the audio buffers
		reader_r = TBL - VL;
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

	};
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

		#ifdef DIFFERENCE_SYNC
		for (int i = 0; i < HISTORY_NUM_FRAMES; ++i) {
			free(prev_buff_l[i]);
			free(prev_buff_r[i]);
		}
		#endif

		ffts_free(fft_plan);

		#ifdef RENORM_2
		free(sinArray);
		#endif
	}
	// -/

	void service_channel(int  w,
	                     int& r,
	                     int& frame_id,
	                     float* prev_buff[HISTORY_NUM_FRAMES],
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
				r = difference_sync(w, r, HISTORY_SEARCH_RANGE, prev_buff, frame_id, audio_buff);
				#endif

				for (int i = 0; i < VL; ++i) {
					staging_buff[i] = audio_buff[(i + r) % TBL];

					#ifdef CONST_RENORM
					staging_buff[i] *= CONST_RENORM;
					#endif

					#ifdef RENORM_4
					if (std::fabs(staging_buff[i]) > max_amplitude_so_far)
						max_amplitude_so_far = std::fabs(staging_buff[i]);
					#endif
				}

				#ifdef DIFFERENCE_SYNC
				// The mix helps reduce jitter in the choice of where to place the next wave.
				//prev_buff[0][i] = mix(staging_buff[i], prev_buff[0][i], difference_smoother);
				for (int i = 0; i < HISTORY_BUFF_SZ; ++i)
					prev_buff[frame_id%HISTORY_NUM_FRAMES][i] = staging_buff[i];
				#endif
				frame_id++;

				#ifdef RENORM_1
				renorm(staging_buff, channel_max, .5, 1.f, VL);
				#endif

				#ifdef RENORM_4
				renorm(staging_buff, max_amplitude_so_far, 0.f, .83f, VL);
				#endif

				#ifdef FFT_SYNC
				freq = get_harmonic_less_than(max_frequency(fft_out), 121.f);
				next_t += dura(1.f/freq);
				#else
				next_t += dura(1.f/60.f);
				#endif
			}
			// // cout << "left" << endl;
			// const float new_rl = advance_index(writer, reader_l, freql, TBL);
			// const float SAMPLES = dist_forward(reader_l, new_rl, TBL);
			// reader_l = new_rl;
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

		int frame_id_l = 0;
		int frame_id_r = 0;

		while (!audio_sink->thread_join) { // Break when audio_sink->thread_join true
			fps(now);
			now = chrono::steady_clock::now();
			// We want to use next += dura(1/freq) in the loop below and not next = now + dura(1/freq) because ... TODO,
			// BUT if now >> next, then we probably were stalled in pcm_getter and should update the next times
			if (now - next_l > chrono::milliseconds(17*4)) { // less than 60/2/2 fps
				next_r = now;
				next_l = now;
				next_fft = now;
			}

			// TODO test this function. The samples it writes to the circular buffer should be the same for the same sound
			// on windows and on linux.
			// TODO On windows this can prevent the app from closing if the music is paused.
			//auto perf_timepoint = std::chrono::steady_clock::now();
			pcm_getter(audio_buff_l+writer, audio_buff_r+writer, ABL);
			//cout << (std::chrono::steady_clock::now() - perf_timepoint).count()/1e9 << endl;

			//- Manage indices and fill smoothing buffers
			writer = move_index(writer, ABL, TBL); // writer marks the index of the discontinuity in the circular buffer

			service_channel(writer,         // writer
			                reader_l,       // reader
			                frame_id_l,
			                prev_buff_l,    // previously visualized audio
			                audio_buff_l,   // audio collected from system audio stream
			                staging_buff_l, // audio for visualizer goes here
			                fft_outl,       // fft complex outputs
			                freql,          // most recent dominant frequency < 600hz
			                channel_max_l,  // channel renormalization term
			                now,            // current time point
			                next_l);        // update staging at this timepoint again

			service_channel(writer,
			                reader_r,
			                frame_id_r,
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
					#define downsmpl(s) s[(i*2+writer)%TBL]
					fft_inl[i] = downsmpl(audio_buff_l) * FFTwindow[i];
					fft_inr[i] = downsmpl(audio_buff_r) * FFTwindow[i];
					#undef downsmpl
				}
				ffts_execute(fft_plan, fft_inl, fft_outl);
				ffts_execute(fft_plan, fft_inr, fft_outr);
				// TODO fft magnitudes are different between windows and linux
				for (int i = 0; i < VL; i++) {
					audio_sink->freq_l[i] = mag(fft_outl, i)/std::sqrtf(float(FFTLEN));
					audio_sink->freq_r[i] = mag(fft_outr, i)/std::sqrtf(float(FFTLEN));
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
				// newr = upsmpl(prev_buff_r);
				#undef upsmpl

				oldl = audio_sink->audio_l[i];
				oldr = audio_sink->audio_r[i];

				mixl = mix(oldl, newl, smoother);
				mixr = mix(oldr, newr, smoother);

				#ifdef RENORM_2
				mixl += (1.f-pow_max_l)*.005*sinArray[i];
				mixr += (1.f-pow_max_r)*.005*sinArray[(i+VL/4)%VL]; // i+vl/4 : sine -> cosine
				#endif

				//*
				audio_sink->audio_l[i] = mixl;
				audio_sink->audio_r[i] = mixr;
				/*/
				// View the input audio
				//audio_sink->audio_l[i] = audio_buff_l[(i + reader_l)%TBL]/channel_max_l;
				//audio_sink->audio_r[i] = audio_buff_r[(i + reader_r)%TBL]/channel_max_r;
				// View the history frames used in difference_sync
				//audio_sink->audio_l[i] = prev_buff_l[(frame_id_l + HISTORY_NUM_FRAMES - 1)%HISTORY_NUM_FRAMES][i/2];
				//audio_sink->audio_r[i] = prev_buff_r[(frame_id_r + HISTORY_NUM_FRAMES - 1)%HISTORY_NUM_FRAMES][i/2];
				//#undef RENORM_4
				//#undef RENORM_3
				//#undef RENORM_2
				//#undef RENORM_1
				// */
			}

			#if defined(RENORM_2) || defined(RENORM_3)
			const float bound = .66f;
			const float spring = .3f; // can spring past bound... oops
			renorm(audio_sink->audio_l, channel_max_l, spring, bound, VL);
			renorm(audio_sink->audio_r, channel_max_r, spring, bound, VL);
			#endif

			audio_sink->mtx.unlock();
			// -/

		}
		audio_destroy();
		return 0;
	}
	// -/

	//- Members
private:
	float* prev_buff_l[HISTORY_NUM_FRAMES];
	float* prev_buff_r[HISTORY_NUM_FRAMES];

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

	int writer;
	int reader_l;
	int reader_r;
	float freql;
	float freqr;

	float channel_max_l;
	float channel_max_r;
	#ifdef RENORM_2
	float* sinArray;
	#endif

	float max_amplitude_so_far = 0.;

	std::function<void(float*, float*, int)> pcm_getter;
	std::function<void(int, int)> audio_initializer;

	struct audio_data* audio_sink;
	// -/
};
