#include <iostream>
#include <chrono>
using std::cout;
using std::endl;

#include "audio_data.h"

#include "pulse_misc.h"

#include "Array.h"
#include "fftwpp/fftw++.h"
using namespace utils;
using namespace fftwpp;
using Array::array1;

// TODO choose the clock in a smarter way
static void fps() {
	static auto prev_time = std::chrono::steady_clock::now();
    static int counter = 0;
	auto now = std::chrono::steady_clock::now();
	counter++;
    if (now > prev_time + std::chrono::seconds(1)) {
		cout << "audio updates per second: "<< counter << endl;
		counter = 0;
		prev_time = now;
    }
}

static float get_harmonic(float freq) {
	if (freq < 1.f) freq = 1.f;
	else if (freq > 1000.f) freq = 1000.f;
	else if ((!(freq >= 0.f))&&(!(freq <= 0.f))) freq = 1.f; // NaN

	// double o = freq;

	// bound freq into the interval [20, 60]
	const float b = log(freq)/log(2.f);
	const float l = log(20.f)/log(2.f);
	const float u = log(60.f)/log(2.f);
	// a == number of iterations for one of the below loops
	int a = 0;
	if (b < l)
		a = floor(b - l); // round towards -inf
	else if (b > u)
		a = ceil(b - u); // round towards +inf
	freq = pow(2, b - a);

	// while (o > 61.f) o /= 2.f; // dividing frequency by two does not change the flipbook image
	// while (o < 20.f) o *= 2.f; // multiplying frequency by two produces the ghosting effect 
	return freq;
}

static inline float tau(float x) {
	return 1.f/4.f * log(3.f*x*x + 6.f*x + 1.f) - sqrt(6.f)/24.f * log((x + 1.f - sqrt(2.f/3.f))  /  (x + 1.f + sqrt(2.f/3.f)));
}

#include <string.h>
#define BUFSIZE 1024
#define NUMBUFS 8
void* audioThreadMain(void* data) {
	struct audio_data* audio = (struct audio_data*)data;

	const int fftLen = NUMBUFS*BUFSIZE;
    const int CHANNELS = 2;

	audio->audio_l = (float*)calloc(fftLen * 4, sizeof(float));
	audio->audio_r = audio->audio_l + fftLen;
	audio->freq_l = audio->audio_r + fftLen;
	audio->freq_r = audio->freq_l + fftLen;

	fftw::maxthreads=get_max_threads();
	size_t align = sizeof(Complex);
	array1<Complex> fl(fftLen,align);
	array1<Complex> fr(fftLen,align);
	fft1d Forwardl(-1, fl);
	fft1d Forwardr(-1, fr);

	float buffer[BUFSIZE*CHANNELS];
	
	float fft_window[fftLen];
	const float win_arg = 2.f * M_PI / (float(fftLen) - 1.f);
	for (int i = 0; i < fftLen; ++i)
		fft_window[i] = .5f-.5f*cos(win_arg*i);

	pa_sample_spec ss;
	ss.channels = 2;
	ss.rate = 96000;
	ss.format = PA_SAMPLE_FLOAT32LE;

	pa_buffer_attr pb;
	pb.fragsize = sizeof(buffer)/2;
	pb.maxlength = sizeof(buffer);
	pa_simple* s = NULL;
	int error;
	s = pa_simple_new(NULL, "APPNAME", PA_STREAM_RECORD, audio->source.data(), "APPNAME", &ss, NULL, &pb, &error);
	if (!s) {
		cout << "Could not open pulseaudio source: " << audio->source << pa_strerror(error) << ". To find a list of your pulseaudio sources run 'pacmd list-sources'" << endl;
		exit(EXIT_FAILURE);
	}
	const auto mag = [](Complex& a) {
		// return sqrt(a.real()*a.real()+a.imag()*a.imag());
        #define RMS(x, y) sqrt((x*x+y*y)*.5f)
		return 1.f - 1.f/ (RMS(a.real(), a.imag())/140.f + 1.f);
	};
	const auto max_frequency = [mag, fftLen](Complex* &f) {
		// f is fft output for both channels
		// offset is offset into the array 
		double max = 0.;
		int max_i = 0;
		// test |i|+|r| instead of magnitude
		for (int i = 0; i < 50; ++i) {
			if (mag(f[i]) > max) {
				max = mag(f[i]);
				max_i = i;
			}
		}
		if (max_i == 0) max_i++;
		if (max_i == 50) max_i--;

		// more info -> http://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak
		const auto gfr = [&f](int i){return f[i].real();};
		const auto gfi = [&f](int i){return f[i].imag();};
		const int k = max_i;
		const double ap = (gfr(k + 1) * gfr(k) + gfi(k+1) * gfi(k))  /  (gfr(k) * gfr(k) + gfi(k) * gfi(k));
		const double dp = -ap / (1 - ap);
		const double am = (gfr(k - 1) * gfr(k) + gfi(k - 1) * gfi(k))  /  (gfr(k) * gfr(k) + gfi(k) * gfi(k));
		const double dm = am / (1. - am);
		const double d =(dp + dm) / 2. + tau(dp * dp) - tau(dm * dm);
		const double peak_index = k + d;
		// http://stackoverflow.com/questions/4364823/how-do-i-obtain-the-frequencies-of-each-value-in-an-fft
		return 96000 * peak_index / float(fftLen);
	};
	//
	// pull data as often as possible
	// read 1024 samples at a time
	// will need a 3 pcm history buffers so that we can take a large fft
	// 4 buffers -> 4096 samples -> fft size of 4096
	// compute fft as often as possible
	// draw waveform at certain intervals
	//
	// auto now = std::chrono::system_clock::now();
	// auto prev_time_l = now;
	// auto prev_time_r = now;
	// auto duration_l = std::chrono::seconds(0);
	// auto duration_r = std::chrono::seconds(0);
	// double elapsed = 0.;
	// double harmonic_l = 0.;
	// double harmonic_r = 0.;
	while (1) {
		fps();

		// now
		// if cur > prev+dura
		//     prev=cur
		//     dura = 1./harmonic
		//     update data

		// harmonic_l = get_harmonic(max_frequency(0));
		// harmonic_r = get_harmonic(max_frequency(fftLen));

		// now = std::chrono::system_clock::now();
		// duration_l = std::chrono::seconds(1./harmonic_l).count();
		// duration_r = std::chrono::seconds(1./harmonic_r).count();

		if (pa_simple_read(s, buffer, sizeof(buffer), &error) < 0) {
			cout << "pa_simple_read() failed: " << pa_strerror(error) << endl;
			exit(EXIT_FAILURE);
		}
		memmove(audio->audio_l, audio->audio_l + BUFSIZE, (fftLen-BUFSIZE)*sizeof(float));
		memmove(audio->audio_r, audio->audio_r + BUFSIZE, (fftLen-BUFSIZE)*sizeof(float));
		for (int i = 0; i < BUFSIZE; i++) {
			audio->audio_l[fftLen-BUFSIZE + i] = buffer[i*CHANNELS+0];
			audio->audio_r[fftLen-BUFSIZE + i] = buffer[i*CHANNELS+1];
		}
		for (int i = 0; i < fftLen; i++) {
			fl[i] = Complex(audio->audio_l[i] * fft_window[i]);
			fr[i] = Complex(audio->audio_r[i] * fft_window[i]);
		}
		Forwardl.fft(fl);
		Forwardr.fft(fr);
		for(int i=0; i < fftLen/2-1; i++) {
			audio->freq_l[i] = 2.*mag(fl[i]);//sqrt(fftLen);
			audio->freq_r[i] = 2.*mag(fr[i]);//sqrt(fftLen);
		}
		if (audio->thread_join) {
			pa_simple_free(s);
			break;
		}
	}
	return 0;
}
