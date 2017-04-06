#include <iostream>
#include <typeinfo>
#include <chrono>
#include <string.h>
#include <algorithm>
using std::max;
namespace chrono = std::chrono;
using std::cout;
using std::endl;

// #include "circular.cpp"
#include "audio_data.h"
#include "pulse_misc.h"

#include "Array.h"
#include "fftwpp/fftw++.h"
using namespace utils;
using namespace fftwpp;
using Array::array1;

static bool fps(chrono::steady_clock::time_point& now) {
	static auto prev_time = chrono::steady_clock::now();
	static int counter = 0;
	counter++;
    if (now > prev_time + chrono::seconds(1)) {
		cout << "snd fps: "<< counter << endl;
		counter = 0;
		prev_time = now;
		return true;
    }
	return false;
}

// TODO could this function be approximated by some modulus operation?
static double get_harmonic(double freq) {
	if (freq < 1.f) freq = 1.f;
	else if (freq > 1000.f) freq = 1000.f;
	else if ((!(freq >= 0.f))&&(!(freq <= 0.f))) freq = 1.f; // NaN

	// double o = freq;

	// bound freq into the interval [20, 60]
	const double b = log2(freq);
	const double l = log2(24.);
	const double u = log2(60.);
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
// #define RMS(x, y) sqrt((x*x+y*y)*.5f)
// return 1.f - 1.f/ (RMS(a.real(), a.imag())/140.f + 1.f);
static inline double smag(Complex a, Complex b) {
	return a.real()*b.real()+a.imag()*b.imag();
}
static double smag(Complex a) { return smag(a,a); }

void* audioThreadMain(void* data) {
	// cout << "{" << endl;
	// for (double i = 1.; i < 100.; i+=.01)
	// 	cout << "{" << i << ", " << get_harmonic(i) << "}," << endl;
	// cout << "}" << endl;
	// exit(1);


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
		fft_window[i] = (1.f-cos(win_arg*i))/2.f;

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
	auto max_frequency = [SR, PL, PN](array1<Complex>&f) {
		// f is fft output for both channels
		// offset is offset into the array 
		double max = 0.;
		int max_i = 0;
		// test |i|+|r| instead of magnitude
		for (int i = 0; i < 50; ++i) {
			const double mag = smag(f[i]);
			if (mag > max) {
				max = mag;
				max_i = i;
			}
		}
		if (max_i == 0) max_i++;
		// more info -> http://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak
		const int k = max_i;
		const Complex f0 = f[k];
		const Complex f1 = f[k-1];
		const Complex f2 = f[k+1];
		const double ap = smag(f2,f0)/max;
		const double dp = -ap / (1. - ap);
		const double am = smag(f1,f0)/max;
		const double dm =  am / (1. - am);
		const double d = (dp + dm) / 2. + tau(dp * dp) - tau(dm * dm);
		const double peak_index = double(k) + d;
		// http://stackoverflow.com/questions/4364823/how-do-i-obtain-the-frequencies-of-each-value-in-an-fft
		return  peak_index * double(SR) / double(PL*PN);
	};
	// dura() converts a second, represented as a double, into the appropriate unit of
	// time for chrono::steady_clock and with the appropriate arithematic type
	// using dura() avoids errors like this : chrono::seconds(double initializer)
	// dura() : <double,seconds> -> clock::<typeof count(), time unit>
	typedef chrono::steady_clock clock;
	auto dura = [](double x) -> clock::duration {
		return chrono::duration_cast<clock::duration>(chrono::duration<double, std::ratio<1>>(x));
	};
	// auto dsec = [](chrono::seconds& x) -> chrono::duration<double, std::ratio<1>> {
	// 	return chrono::duration_cast<chrono::duration<double, std::ratio<1>>>(x);
	// };
	auto now = clock::now();
	auto next_l = now + dura(1./60.);
	auto next_r = next_l;
	double pl = 0;
	double pr = 0;
	double hml = 60.;
	double hmr = 60.;
	int I = 0;
	// TODO provide option to disable the adaptive buffer rate.
	while (1) {
		if (pa_simple_read(s, buffer, sizeof(buffer), &error) < 0) {
			cout << "pa_simple_read() failed: " << pa_strerror(error) << endl;
			exit(EXIT_FAILURE);
		}
		// TODO instead of moving memory. why don't I use a ring buffer type of system?
		// TODO if I do use a ring buffer, there is a good chance that
		// memmove(pulse_buf_l, pulse_buf_l+PL, (PN*PL-1)*sizeof(float));
		// memmove(pulse_buf_r, pulse_buf_r+PL, (PN*PL-1)*sizeof(float));
		// for (int i = 0; i < PL; i++) {
		// 	pulse_buf_l[PL*(PN-1) + i] = buffer[i*C+0];
		// 	pulse_buf_r[PL*(PN-1) + i] = buffer[i*C+1];
		// }
		// for (int i = 0; i < PL*PN; i++) {
		// 	fl[i] = Complex(pulse_buf_l[i] * fft_window[i]);
		// 	fr[i] = Complex(pulse_buf_r[i] * fft_window[i]);
		// }
		for (int i = 0; i < PL; i++) {
			pulse_buf_l[PL*I + i] = buffer[i*C+0];
			pulse_buf_r[PL*I + i] = buffer[i*C+1];
		}
		int J = I+1;
		J%=PN;
		for (int i = PL*J; i < PL*PN; i++) {
			fl[i-PL*J] = Complex(pulse_buf_l[i] * fft_window[i-PL*J]);
			fr[i-PL*J] = Complex(pulse_buf_r[i] * fft_window[i-PL*J]);
		}
		for (int i = 0; i < PL*J; i++) {
			fl[i+PL*(PN-J)] = Complex(pulse_buf_l[i] * fft_window[i+PL*(PN-J)]);
			fr[i+PL*(PN-J)] = Complex(pulse_buf_r[i] * fft_window[i+PL*(PN-J)]);
		}
		Forwardl.fft(fl);
		Forwardr.fft(fr);
		fl[0]=Complex(0);
		fr[0]=Complex(0);
		for(int i = 0; i < VL; i++) {
			audio->freq_l[i] = sqrt(smag(fl[i])/(PL*PN));
			audio->freq_r[i] = sqrt(smag(fr[i])/(PL*PN));
		}

		// TODO the app crashes sometimes and it might be related to calling memcpy
		// while opengl is reading from the destination of memcpy.

		// TODO why will this loop not execute at a speedy 60fps?
		
		// TODO entirely possible to read a discontinuous waveform into the fft
		// and visualizer buffers

		// TODO add soap opera effects to the audio buffer from the odl buffer to the new buffer.

		const double D = .85;

		now = clock::now();
		if (now > next_l) {
			fps(now);

			// pl = (pl+2.*SR/hml)*.9+ .1*pl;
			pl += SR/hml;
			if (pl > PN*PL) { pl -= PN*PL; }
			int R = (int)pl;
			int a = R-VL;
			if (a < 0) a += PN*PL;
			int r = 0;
			int b = a + VL;
			if (b > PN*PL) { r = b-PN*PL; b = PN*PL; }
			for (int i = 0; i < b-a; ++i)
				audio->audio_l[i] = pulse_buf_l[i+a]*(1.-D) + D*audio->audio_l[i];
			for (int i = 0; i < r; ++i)
				audio->audio_l[i+b-a] = pulse_buf_l[i]*(1.-D)+D*audio->audio_l[i+b-a];
			// memcpy(audio->audio_l, pulse_buf_l + a, (b-a)*sizeof(float));
			// memcpy(audio->audio_l+(b-a), pulse_buf_l, r*sizeof(float));
			// hml = .8*hml + .2*get_harmonic(max_frequency(fl));
			hml = get_harmonic(max_frequency(fl));
			next_l += dura(1./hml);

			// int R = PL*(I-2);
			// int a = R-VL/2;
			// if (a < 0) a += PN*PL;
			// int r = 0;
			// int b = a + VL;
			// if (b > PN*PL) { r = b-PN*PL; b = PN*PL; }
			// for (int i = a; i < b; i++) audio->audio_l[i-a] = pulse_buf_l[i];
			// for (int i = 0; i < r; i++) audio->audio_l[i+a-r] = pulse_buf_l[i];
		}
		if (now > next_r) {
			// pr = (pr+2.*SR/hmr)*.9+ .1*pr;
			pr += SR/hmr;
			if (pr > PN*PL) { pr -= PN*PL; }
			int R = (int)pr;
			int a = R-VL;
			if (a < 0) a += PN*PL;
			int r = 0;
			int b = a + VL;
			if (b > PN*PL) { r = b-PN*PL; b = PN*PL; }
			for (int i = 0; i < b-a; ++i)
				audio->audio_r[i] = pulse_buf_r[i+a]*(1.-D)+D*audio->audio_r[i];
			for (int i = 0; i < r; ++i)
				audio->audio_r[i+b-a] = pulse_buf_r[i]*(1.-D)+D*audio->audio_r[i+b-a];
			// memcpy(audio->audio_l, pulse_buf_l + a, (b-a)*sizeof(float));
			// memcpy(audio->audio_l+(b-a), pulse_buf_l, r*sizeof(float));
			// hmr = .8*hmr + .2*get_harmonic(max_frequency(fr));
			hmr = get_harmonic(max_frequency(fr));
			next_r += dura(1./hmr);
		}
		I++;
		I%=PN;

		if (audio->thread_join) {
			pa_simple_free(s);
			break;
		}
	}
	return 0;
}
