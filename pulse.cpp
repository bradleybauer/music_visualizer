// vim: foldmarker=\/\-,\-\/:foldmethod=marker

#include <iostream>
#include <chrono>
#include <thread>
#include <limits>
#include <cmath>
#include <string.h>
#include "audio_data.h"
#include "pulse_misc.h"
#include "fftwpp/Array.h"
#include "fftwpp/fftw++.h"

/*
 * Proof in progress
 * Work of concept
 * ...
 */

using Array::array1;
using namespace fftwpp;
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
static double sign(double x) {
	return std::copysign(1., x);
}
static double mag(Complex a) {
	return sqrt(a.real() * a.real() + a.imag() * a.imag());
	// #define RMS(x, y) sqrt((x*x+y*y)*1./2.)
	// return 1.f - 1.f / (RMS(a.real(), a.imag())/140.f + 1.f);
}
static int max_bin(array1<Complex>& f) {
	double max = 0.;
	int max_i = 0;
	// catch frequencies from 5.86 to 586
	for (int i = 0; i < 100; ++i) {
		const double mmag = mag(f[i]);
		if (mmag > max) {
			max = mmag;
			max_i = i;
		}
	}
	if (max_i == 0)
		max_i++;
	return max_i;
}
static void move_index(double& p, double amount) {
	p += amount;
	if (p < 0.) {
		p += L;
	} else if (p > L) {
		p -= L;
	}
}
static double dist_forward(double from, double to) {
	double d = to - from;
	if (d < 0)
		d += L;
	return d;
}
static void adjust_reader(double& r, double w, double hm) {
	// This function attempts to separate two points in a circular buffer by distance L/2.
	const double a = w - r;
	const double b = fabs(a) - L / 2;
	const double foo = fabs(b);
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
	const double dadr = -1.0;
	const double dbdr = sign(a) * dadr;
	const double dfoodr = sign(b) * dbdr;
	// move r by a magnitude of foo in the -foo'(r) direction but by taking only steps of size SR/hm
	double grid = SR / hm;
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
static chrono::steady_clock::duration dura(double x) {
	// dura() converts a second, represented as a double, into the appropriate unit of
	// time for chrono::steady_clock and with the appropriate arithematic type
	// using dura() avoids errors like this : chrono::seconds(double initializer)
	// dura() : <double,seconds> -> chrono::steady_clock::<typeof count(), time unit>
	return chrono::duration_cast<chrono::steady_clock::duration>(
	    chrono::duration<double, std::ratio<1>>(x));
}
static double max_frequency(array1<Complex>& f) {
	// more info -> http://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak
	const int k = max_bin(f);
	const double y1 = mag(f[k - 1]);
	const double y2 = mag(f[k]);
	const double y3 = mag(f[k + 1]);
	const double d = (y3 - y1) / (2 * (2 * y2 - y1 - y3)+1./32.); // +1/32 to avoid divide by zero
	const double kp = k + d;
	return kp * double(SRF) / double(FFTLEN);
}
static double clamp(double x, double l, double h) {
	if (x < l)
		x = l;
	else if (x > h)
		x = h;
	return x;
}
static double get_harmonic(double freq) {
	// bound freq into the interval [24, 60]

	freq = clamp(freq, 1., double(SRF) / double(FFTLEN) * 100.);

	const double b = log2(freq);
	const double l = log2(24.);
	const double u = log2(60.);
	// a == number of iterations for one of the below loops
	int a = 0;
	if (b < l)
		a = floor(b - l); // round towards -inf
	else if (b > u)
		a = ceil(b - u); // round towards +inf
	freq = pow(2.0, b - a);

	// double o = freq;
	// while (o > 61.f) o /= 2.f; // dividing frequency by two does not change the flipbook
	// image
	// while (o < 24.f) o *= 2.f; // multiplying frequency by two produces the ghosting effect
	return freq;
}
static inline double mix(double x, double y, double m) {
	return x * (1. - m) + y * m;
}
// setting low mixer can lead to some interesting lissajous
template<typename T>
static void renorm(T* f, double& max, double mixer, double scale) {
	double _max = -16.;
	for (int i = 0; i < VL; ++i)
		if (fabs(f[i]) > _max)
			_max = fabs(f[i]);
	max = mix(max, _max, mixer);
	if (max != 0.)
		for (int i = 0; i < VL; ++i)
			f[i] = f[i] / max * scale;
}
// -/

void* audioThreadMain(void* data) {

	//- TODOs
	// TODO would alsa be more stable/faster?
	// TODO use template programming to compute the window at compile time.
	// TODO how much time does pa_simple_read take to finish? If the app is in the error state, then
	//   how much time does it take?
	// TODO could the get_harmonic function be approximated by some modulus operation?
	//   It is not nailing down the absolute best frequency to choose the loop rate for
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

	//- Smoothing buffers
	// helps give the impression that the wave data updates at 60fps
	float* tl = (float*)calloc(VL * C + 2, sizeof(float));
	float* tr = tl + VL + 1;
	// -/

	//- FFT setup
	fftw::maxthreads = get_max_threads();
	array1<Complex> fl(FFTLEN, sizeof(Complex));
	array1<Complex> fr(FFTLEN, sizeof(Complex));
	fft1d Forwardl(-1, fl);
	fft1d Forwardr(-1, fr);
	float FFTwindow[FFTLEN];
	for (int i = 1; i < FFTLEN; ++i)
		FFTwindow[i] = (1. - cos(2. * M_PI * i / float(FFTLEN))) / 2.;
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
	auto next_l = now + dura(1. / 60.);
	auto next_r = next_l;
	auto prev = next_r; // Compute the fft only once every 1/60 of a second
	// -/

	//- Repository Indice Management
	int W = 0;     // The index of the writer in the audio repositories, of the newest sample in the
	               // buffers, and of a discontinuity in the waveform
	double rl = 0; // Index of a reader in the audio repositories (left channel)
	double rr = rl;
	double hml = 60.; // Harmonic frequency of the dominant frequency of the audio.
	double hmr = hml; // we capture an image of the waveform at this rate
	// -/

	//- Wave Renormalizer
	double maxl = 1.;
	double maxr = 1.;
// renormalize the wave to be unit amplitude. there are two methods to do this
#define RENORM_1
// #define RENORM_2
#ifdef RENORM_2
	float* sinArray = (float*)malloc(VL * sizeof(float));
	for (int i = 0; i < VL; ++i)
		sinArray[i] = sin(6.288 * i / float(VL));
#endif
	// -/

	while (1) {

		//- Read Datums
		// auto test = std::chrono::steady_clock::now();
		if (pa_simple_read(pulseState, pulse_buf_interlaced, sizeof(pulse_buf_interlaced), &error) < 0) {
			cout << "pa_simple_read() failed: " << pa_strerror(error) << endl;
			exit(EXIT_FAILURE);
		}
		// cout << std::chrono::duration_cast<std::chrono::duration<double,
		// std::ratio<1>>>(std::chrono::steady_clock::now()-test).count()<<endl;
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
			move_index(rl, SR / hml);
			if (dist_forward(rl, PL * W) < VL) {
				adjust_reader(rl, PL * W, hml);
				cout << "oopsl" << endl;
			}
			for (int i = 0; i < VL; ++i)
				tl[i] = pulse_buf_l[(i + int(rl)) % L];

#ifdef RENORM_1
			renorm(tl, maxl, .5, 1.);
#endif

			hml = get_harmonic(max_frequency(fl));
			next_l += dura(1. / hml);
		}

		if (now > next_r) {
			move_index(rr, SR / hmr);
			if (dist_forward(rr, PL * W) < VL) {
				adjust_reader(rr, PL * W, hmr);
				cout << "oopsr" << endl;
			}
			for (int i = 0; i < VL; ++i)
				tr[i] = pulse_buf_r[(i + int(rr)) % L];

#ifdef RENORM_1
			renorm(tr, maxr, .5, 1.);
#endif

			hmr = get_harmonic(max_frequency(fr));
			next_r += dura(1. / hmr);
		}
// -/

		//- Preprocess audio and execute the FFT
		if (now > prev) {
			prev = now + dura(1. / 60.);
			for (int i = 0; i < FFTLEN; ++i) {
				fl[i] = Complex(FFTwindow[i] * pulse_buf_l[(i * 2 + PL * W) % L]);
				fr[i] = Complex(FFTwindow[i] * pulse_buf_r[(i * 2 + PL * W) % L]);
			}
			Forwardl.fft(fl);
			Forwardr.fft(fr);
		}
		// -/

		//- Fill presentation buffers
		audio->mtx.lock();
		for (int i = 0; i < FFTLEN / 2; i++) {
			audio->freq_l[i] = (float)mag(fl[i] / double(FFTLEN));
			audio->freq_r[i] = (float)mag(fr[i] / double(FFTLEN));
		}
		// Smooth and upsample the wave
		// const double smoother = .85;
		// const double smoother = .06;
		const double smoother = .2;

#ifdef RENORM_2
		const double Pl = pow(maxl, .1);
		const double Pr = pow(maxr, .1);
#endif

		for (int i = 0; i < VL; ++i) {
			#define upsmpl(s) (s[i/2] + s[(i+1)/2])/2.
			const double al = upsmpl(tl);
			const double ar = upsmpl(tr);
#ifdef RENORM_2
			audio->audio_l[i] = mix(audio->audio_l[i], al, smoother)+.005*sinArray[i]*(1.-Pl);
			audio->audio_r[i] = mix(audio->audio_r[i], ar, smoother)+.005*sinArray[(i+VL/4)%VL]*(1.-Pr); // i+vl/4 : sine -> cosine
#else
			audio->audio_l[i] = mix(audio->audio_l[i], al, smoother);
			audio->audio_r[i] = mix(audio->audio_r[i], ar, smoother);
#endif
		}

#ifdef RENORM_2
		const double bound = .75;
		const double spring = .1; // can spring past bound... oops
		renorm(audio->audio_l, maxl, spring, bound);
		renorm(audio->audio_r, maxr, spring, bound);
		// double _max = -std::numeric_limits<double>::infinity();
		// for (int i = 0; i < VL; ++i) {
		// 	if (fabs(audio->audio_l[i]) > _max)
		// 		_max = fabs(audio->audio_l[i]);
		// 	if (fabs(audio->audio_r[i]) > _max)
		// 		_max = fabs(audio->audio_r[i]);
		// }
		// maxr = mix(maxr, _max, spring);
		// maxl = mix(maxl, _max, spring);
		// if (maxl != 0.)
		// 	for (int i = 0; i < VL; ++i)
		// 		audio->audio_l[i] /= maxl / bound;
		// if (maxr != 0.)
		// 	for (int i = 0; i < VL; ++i)
		// 		audio->audio_r[i] /= maxl / bound;
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
