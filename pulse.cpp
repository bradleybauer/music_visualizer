#include <iostream>
#include <typeinfo>
#include <chrono>
#include <string.h>
#include <algorithm>
#include <cmath>
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
// TODO This is not nailing down the absolute best frequency to choose the loop rate for
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
	freq = pow(2.0, b - a);

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
static inline double smag(Complex a) { return smag(a,a); }

// wow
// template <int...I>
// constexpr auto
// make_compile_time_sequence(std::index_sequence<I...>)
// {
// 	return std::array<int,sizeof...(I)>{{cos(I)...}};
// }
// constexpr auto array_1_20 = make_compile_time_sequence(std::make_index_sequence<40>{});

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

	// Audio repository
	float* pulse_buf_l = (float*)calloc(PN*PL*C, sizeof(float));
	float* pulse_buf_r = pulse_buf_l + PN*PL;

	// Audio presentation buffers
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
	// TODO use template programming to compute the window at compile time.
	float fft_window[PL*PN];
	constexpr float win_arg = 2.f * M_PI / (float(PL*PN) - 1.f);
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
	auto max_index = [](array1<Complex>&f) {
		double max = 0.;
		int max_i = 0;
		for (int i = 0; i < 50; ++i) {
			const double mag = smag(f[i]);
			if (mag > max) {
				max = mag;
				max_i = i;
			}
		}
		if (max_i == 0) max_i++;
		return max_i;
	};
	auto bin_to_freq = [SR, PL, PN](double x) {
		 return x*double(SR)/double(PL*PN);
	};
	auto max_frequency = [&](array1<Complex>&f) {
		// more info -> http://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak
		const int k = max_index(f);
		const double y1 = sqrt(smag(f[k-1]));
		const double y2 = sqrt(smag(f[k]));
		const double y3 = sqrt(smag(f[k+1]));
		const double d = (y3 - y1) / (2 * (2 * y2 - y1 - y3));
		const double kp  =  k + d;
		return bin_to_freq(kp);
	};
	auto sign = [](double x) { return std::copysign(1.,x); };
	auto move_index = [PN,PL](double& p, double amount) {
		p += amount;
		if (p < 0.) { p += PN*PL; }
		else if (p > PN*PL) { p -= PN*PL; }
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
	double rl = 0; // Position in audio repository to read from
	double rr = 0;
	double hml = 60.; // Harmonic frequency of the dominant frequency of the audio.
	double hmr = 60.; // we capture an image of the waveform at this rate
	static float tl[VL]; // Temporary buffer to allow the waveform in audio->audio_[lr] to change at a rate
	static float tr[VL]; // greater than hm[lr]. We want to avoid the perception of low fps in the waveform.
	int I = 0; // The index of the writer in the audio repository
/*
	auto adjust_reader = [PL,PN,SR, sign, move_index](double& r, double W, double hm) {
		double w = W;
		const double L = PL*PN;
		const double a = w-r;
		const double b = fabs(a)-L/2;
		const double c = fabs(b);
		// minimize c(r)
		// c(r) = ||w-r|-L/2| ---> take the min distance, either forward or backward, from r needed to get to w. Then check how close this distance is to L/2. We want to make this distance L/2. We want to make this distance L/2.
		//      = |b(a)|
		// dc(b(a(r)))/dr = dcdb*dbda*dadr
		const double dcdb = sign(b);
		const double dcda = sign(a)*dcdb;
		const double dcdr = sign(r)*dcda;
		// if c(r) is greater than a threshold then minimize c(r). I do not know what the threshold should be
		// // TODO test which w-r (or c) lead to breaks in the wave form visually. Then try and avoid those.
		// // I do not know if c>(PN*PL-VL)/2 is the correct way to test if the visualizer is going to pull a non smooth wave
		if (c > (PN*PL-VL)/2) {
			// let w be the point on the opposite side of the ring buffer of w's initial position
			move_index(w, L/2);
			// move in the opposite direction of the gradient the number of steps needed to cross w using a step size of SR/hml
			move_index(r, -dcdr*ceil(fabs(w-r)/(SR/hm/2))*SR/hm/2);
			// cout << " TODO make a forward and backward dist function: moved p: " << ceil(fabs(w-r)/(SR/hm/2)) << "*SR/hm/2 = " << ceil(fabs(w-r)/(SR/hm/2))*SR/hm/2 << " | new w-r: " << fabs(W-r) << endl;
		}
	};
*/
	const auto fill_buffer = [PL, PN](double rl, float* dst, float* src) {
		// Map [a,b] from src to [0,b-a] in dst,
		// then map [0,a] from src to [b-a, VL] in dst;
		int a = (int)rl; // read position
		int b = a + VL;
		int r = 0;
		if (b > PN*PL) { r = b-PN*PL; b = PN*PL; }
		for (int i = 0; i < b-a; ++i)
			dst[i] = src[i+a];
		for (int i = 0; i < r; ++i)
			dst[i+b-a] = src[i];
	};
	// TODO provide option to disable the adaptive buffer rate.
	while (1) {
		if (pa_simple_read(s, buffer, sizeof(buffer), &error) < 0) {
			cout << "pa_simple_read() failed: " << pa_strerror(error) << endl;
			exit(EXIT_FAILURE);
		}
		for (int i = 0; i < PL; i++) {
			pulse_buf_l[PL*I + i] = buffer[i*C+0];
			pulse_buf_r[PL*I + i] = buffer[i*C+1];
		}
		int J = I+1;
		J%=PN;
		for (int i = 0; i < PL*J; i++) {
			fl[i+PL*(PN-J)] = Complex(pulse_buf_l[i] * fft_window[i+PL*(PN-J)]);
			fr[i+PL*(PN-J)] = Complex(pulse_buf_r[i] * fft_window[i+PL*(PN-J)]);
		}
		for (int i = PL*J; i < PL*PN; i++) {
			fl[i-PL*J] = Complex(pulse_buf_l[i] * fft_window[i-PL*J]);
			fr[i-PL*J] = Complex(pulse_buf_r[i] * fft_window[i-PL*J]);
		}
		Forwardl.fft(fl);
		Forwardr.fft(fr);
		fl[0]=Complex(0);
		fr[0]=Complex(0);
		for(int i = 0; i < VL; i++) {
			audio->freq_l[i] = sqrt(smag(fl[i])/(PL*PN));
			audio->freq_r[i] = sqrt(smag(fr[i])/(PL*PN));
		}
		// Kind of the soap opera effect. I could also add smoothing between each frame of graphics for extra soap.
		const double D = .85;
		// const double D = .051;
		for (int i = 0; i < VL; ++i) {
			audio->audio_l[i] = audio->audio_l[i]*(1.-D)+D*tl[i];
			audio->audio_r[i] = audio->audio_r[i]*(1.-D)+D*tr[i];
		}
		now = clock::now();
		if (now > next_l) {
			// TODO check if the amount of time that has passed can fit more than one count of SR/hml
			move_index(rl, SR/hml);
			// cout << SR/hml << endl;
			// adjust_reader(rl, PL*I, hml);
			// cout << fabs(PL*I-rl) <<","<<endl;
			hml = get_harmonic(max_frequency(fl));
			next_l += dura(1./hml);
			audio->mtx.lock();
			fill_buffer(rl, tl, pulse_buf_l);
			audio->mtx.unlock();
		}
		if (now > next_r) {
			move_index(rr, SR/hml);
			// adjust_reader(rr, PL*I, hmr);
			hmr = get_harmonic(max_frequency(fr));
			next_r += dura(1./hmr);
			audio->mtx.lock();
			fill_buffer(rr, tr, pulse_buf_r);
			audio->mtx.unlock();
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
// I'm trying to ensure that my app never displays a discontinuous waveform.
// Assuming that pulse audio always returns continuous waveforms, all I have to do 
// is make sure that I never display a section of the waveform that crosses the begin/end
// of the audio's circular buffer. I also take locks uhh, to prevent, reading data as it is written.
//
// If I start mpd while the youtube music player is running, then the pulse server... well, for some
// reason the read point in the circular buffer gets too close to the write pointer.
// I use the number of samples between the write and read pointers to determine if the waveform
// is going to be discontinuous or not. It seems to work well.
// But... sometimes the app gets in a weird state, where it cannot move the read pointer far
// enough away... I'm just testing what happens when I ask the pulse server to deliver data
// to multiple instances of my app.. I guess I'm just trying to put my app into the weird
// state again to see if I can think of a way to fix it. But, maybe I already have fixed it.
// I don't know, I tweaked a few of the above lines just before I started this video.
//
// Let me launch a few more instance of my app. It's tripping up more often now. lol, much faster
// now.
//
// The app is pulling discontinuous waveforms. If pulse is delivering smooth waves, then the above
// if block should prevent reading uh, not smooth waves.
//
//

