#include <iostream>
#include <typeinfo>
#include <chrono>
namespace chrono = std::chrono;
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
	static auto prev_time = chrono::steady_clock::now();
	static int counter = 0;
	auto now = chrono::steady_clock::now();
	counter++;
    if (now > prev_time + chrono::seconds(1)) {
		cout << "snd fps: "<< counter << endl;
		counter = 0;
		prev_time = now;
    }
}
static double get_harmonic(double freq) {
	if (freq < 1.f) freq = 1.f;
	else if (freq > 1000.f) freq = 1000.f;
	else if ((!(freq >= 0.f))&&(!(freq <= 0.f))) freq = 1.f; // NaN

	// double o = freq;

	// bound freq into the interval [20, 60]
	const double b = log(freq)/log(2.);
	const double l = log(20.)/log(2.);
	const double u = log(60.)/log(2.);
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
static inline double tau(double x) {
	return 1./4. * log(3.*x*x + 6.*x + 1.) - sqrt(6.)/24. * log((x + 1. - sqrt(2./3.))  /  (x + 1. + sqrt(2./3.)));
}

#include <string.h> // memory stuff
#include <algorithm> // std::max
using std::max;
void* audioThreadMain(void* data) {
	// Variables
	//
	// PL length of the pulse audio output buffer
	// PN number of pulse audio output buffers to keep
	// VL length of visualizer 1D texture buffers
	// C  channel count of audio
	// SR requested sample rate of the audio
	//
	// I am taking the fft of all pulse audio output that I buffer
	// So the fft is PN*PL in size
	// I discard most of the output and only keep VL numbers in each visualizer buffer
	const int PL = 1024;
	const int PN = 8;
	const int VL = VISUALIZER_BUFSIZE;
	const int C = 2;
	const int SR = 96000;

	float* pulse_buf_l = (float*)calloc(PN*PL*C, sizeof(float));
	float* pulse_buf_r = pulse_buf_l + PN*PL;

	struct audio_data* audio = (struct audio_data*)data;
	audio->audio_l = (float*)calloc(VL*C*2, sizeof(float));
	audio->audio_r = audio->audio_l + VL;
	audio->freq_l = audio->audio_r + VL;
	audio->freq_r = audio->freq_l + VL;

	fftw::maxthreads=get_max_threads();
	array1<Complex> fl(PL*PN,sizeof(Complex));
	array1<Complex> fr(PL*PN,sizeof(Complex));
	fft1d Forwardl(-1, fl);
	fft1d Forwardr(-1, fr);
	
	float fft_window[PL*PN];
	const float win_arg = 2.f * M_PI / (float(PL*PN) - 1.f);
	for (int i = 0; i < PL*PN; ++i)
		fft_window[i] = .5f-.5f*cos(win_arg*i);

	float buffer[PL*C];
	pa_sample_spec ss;
	ss.channels = C;
	ss.rate = SR;
	ss.format = PA_SAMPLE_FLOAT32NE;

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
		return sqrt(a.real()*a.real()+a.imag()*a.imag());
      // #define RMS(x, y) sqrt((x*x+y*y)*.5f)
		// return 1.f - 1.f/ (RMS(a.real(), a.imag())/140.f + 1.f);
	};
	auto max_frequency = [mag, SR, PL, PN](array1<Complex>&f) {
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

		// more info -> http://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak
		const int k = max_i;
		// const double smfk = f[k].real() * f[k].real() + f[k].imag() * f[k].imag();
		const double smfk = max*max;
		const double ap = (f[k + 1].real() * f[k].real() + f[k+1].imag() * f[k].imag())  /  smfk;
		const double dp = -ap / (1 - ap);
		const double am = (f[k - 1].real() * f[k].real() + f[k - 1].imag() * f[k].imag())  /  smfk;
		const double dm = am / (1. - am);
		const double d =(dp + dm) / 2. + tau(dp * dp) - tau(dm * dm);
		const double peak_index = double(k) + d;
		// http://stackoverflow.com/questions/4364823/how-do-i-obtain-the-frequencies-of-each-value-in-an-fft
		return  peak_index * double(SR) / double(PL*PN);
	};

	// dura() takes a double representing a second and converts it to the appropriate time unit of
	// chrono::steady_clock with the appropriate type
	// using dura() avoids errors like this : chrono::seconds(double initializer)
	// dura() : <double,seconds> -> clock::<typeof count(), time unit>
	typedef chrono::steady_clock clock;
	auto dura = [](double x) -> clock::duration {
		return clock::duration((clock::rep)(x*double(clock::period::den)/double(clock::period::num)));
	};
	auto now = clock::now();
	auto prev_time_l = now;
	auto prev_time_r = now;
	auto duration_l = dura(0);
	auto duration_r = dura(0);
	// double fetch = 0;
	// int kkk = 1;
	while (1) {
		if (pa_simple_read(s, buffer, sizeof(buffer), &error) < 0) {
			cout << "pa_simple_read() failed: " << pa_strerror(error) << endl;
			exit(EXIT_FAILURE);
		}
		memmove(pulse_buf_l, pulse_buf_l+PL, (PN*PL-1)*sizeof(float));
		memmove(pulse_buf_r, pulse_buf_r+PL, (PN*PL-1)*sizeof(float));
		for (int i = 0; i < PL; i++) {
			pulse_buf_l[PL*(PN-1) + i] = buffer[i*C+0];
			pulse_buf_r[PL*(PN-1) + i] = buffer[i*C+1];
		}
		for (int i = 0; i < PL*PN; i++) {
			fl[i] = Complex(pulse_buf_l[i] * fft_window[i]);
			fr[i] = Complex(pulse_buf_r[i] * fft_window[i]);
		}
		Forwardl.fft(fl);
		Forwardr.fft(fr);
		fl[0];
		fr[0];
		for(int i = 0; i < VL; i++) {
			audio->freq_l[i] = mag(fl[i])/sqrt(PL*PN);
			audio->freq_r[i] = mag(fr[i])/sqrt(PL*PN);
		}

		// TODO the app crashes sometimes and it might be related to calling memcpy
		// while opengl is reading from the destination of memcpy.

		now = clock::now();
		if (now > prev_time_l + duration_l) {
			fps();
			prev_time_l = now;
			duration_l = dura(1./get_harmonic(max_frequency(fl)));
			memcpy(audio->audio_l, pulse_buf_l, VL*sizeof(float));
		}
		if (now > prev_time_r + duration_r) {
			prev_time_r = now;
			memcpy(audio->audio_r, pulse_buf_r, VL*sizeof(float));
		}

		if (audio->thread_join) {
			pa_simple_free(s);
			break;
		}
	}
	return 0;
}
// fetch+= 96000*(duration_l.count()/1e9);
// cout << fetch << endl;
// if ((!(fetch >= 0))&&(!(fetch <= 0))) fetch = 1.; // NaN
// if (fetch > PN*PL-VL) fetch -= PN*PL-VL;
/*
kkk = !kkk;
if (kkk)
	for (int lll = 0; lll < VL; ++lll)
		audio->audio_l[lll]=audio->audio_l[lll]*.5+.5;
else
	for (int lll = 0; lll < VL; ++lll)
		audio->audio_l[lll]=audio->audio_l[lll]*.5-.5;
*/
// memcpy(audio->audio_l, pulse_buf_l+PN*PL/2-VL, VL*sizeof(float));
// memcpy(audio->audio_l, pulse_buf_l+(int)fetch, VL*sizeof(float));

	// auto now = chrono::steady_clock::now();
	// auto prev_time_l = now;
	// auto prev_time_r = now;
	// typedef chrono::duration<double, std::ratio<1>> dseconds;
	// auto duration_l = dseconds(0.);
	// auto duration_r = dseconds(0.);
	// int fetch = 0;
	// while (1) {
	// 	now = chrono::steady_clock::now();
	// 	// TODO I need to choose where in the buffer the next set of samples come from.
	// 	// I think being able to choose is where my android port get's it's power.
	// 	if (now > prev_time_l + duration_l) {
	// 		prev_time_l = now;
	// 		duration_l = dseconds(1./get_harmonic(max_frequency(fl)));
	// 		memcpy(audio->audio_l, pulse_buf_l+max(PL,VL)-VL, VL*sizeof(float));
	// 		// TODO the app crashes sometimes and it might be related to calling memcpy
	// 		// while opengl is reading from the destination of memcpy.
	// 	}
