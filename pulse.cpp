#include <iostream>
#include <chrono>
#include <thread>
#include <string.h>
#include "audio_data.h"
#include "pulse_misc.h"
#include "Array.h"
#include "fftwpp/fftw++.h"

using Array::array1;
using namespace fftwpp;
namespace chrono = std::chrono;
using std::cout;
using std::endl;

// template <int...I>
// constexpr auto
// make_compile_time_sequence(std::index_sequence<I...>)
// {
// 	return std::array<int,sizeof...(I)>{{cos(I)...}};
// }
// constexpr auto array_1_20 = make_compile_time_sequence(std::make_index_sequence<40>{});

void* audioThreadMain(void* data) {

	// TODO would alsa be more stable?
	// TODO use template programming to compute the window at compile time.
	// TODO how much time does pa_simple_read take to finish? If the app is in the error state, then how much time does it take?
	// TODO could the get_harmonic function be approximated by some modulus operation?
	// It does is not nailing down the absolute best frequency to choose the loop rate for

	// Constants
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
	const int L = PL*PN;
	const int FFTLEN = L/2;
	const int VL = VISUALIZER_BUFSIZE; // defined in audio_data.h
	const int C = 2;
	const int SR = 48000;
	const int SRF = SR/2;
	// The FFT looks best when it has 8192/48000, or 4096/24000, time granularity.
	// However, I like the wave to be 96000hz just because then it fits on the screen nice.
	// To have both an FFT on 24000hz data and to display 96000hz waveform data, I ask
	// pulseaudio to output 48000hz and then resample accordingly.
	// I've compared the 4096sample FFT over 24000hz data to the 8192sample FFT over 48000hz data
	// and they are surprisingly similar. I couldn't tell a difference really.
	// I've also compared resampling with ffmpeg to my current impl. Hard to notice a difference, if at all.
	// Of course the user might would want to change all this. That's for another day.

	// Audio repositories
	double* pulse_buf_l = (double*)calloc(L*C, sizeof(double));
	double* pulse_buf_r = pulse_buf_l + L;

	// Presentation buffers
	struct audio_data* audio = (struct audio_data*)data;
	audio->audio_l = (float*)calloc(VL*C*2, sizeof(float));
	audio->audio_r = audio->audio_l + VL;
	audio->freq_l = audio->audio_r + VL;
	audio->freq_r = audio->freq_l + VL;

	// Smoothing buffers
	// helps give the impression that the wave data updates at 60fps
	double* tl = (double*)calloc(VL*2+2, sizeof(double));
	double* tr = tl+VL+1;

	// FFT setup
	fftw::maxthreads=get_max_threads();
	array1<Complex> fl(FFTLEN,sizeof(Complex));
	array1<Complex> fr(FFTLEN,sizeof(Complex));
	fft1d Forwardl(-1, fl);
	fft1d Forwardr(-1, fr);
	double FFTwindow[FFTLEN];
	for (int i = 1; i < FFTLEN; ++i)
		FFTwindow[i] = (1.-cos(2.*M_PI*i/double(FFTLEN)))/2.;

	getPulseDefaultSink((void*)audio);
	// Pulse setup
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

	// Functions
	constexpr auto mag = [](Complex a) {
		return sqrt(a.real()*a.real()+a.imag()*a.imag());
		// #define RMS(x, y) sqrt((x*x+y*y)*1./2.)
		// return 1.f - 1.f/ (RMS(a.real(), a.imag())/140.f + 1.f);
	};
	auto max_bin = [&mag](array1<Complex>&f) {
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
	};
	constexpr auto sign = [](double x) {
		return std::copysign(1.,x);
	};
	constexpr auto move_index = [L](double& p, double amount) {
		p += amount;
		if (p < 0.) { p += L; }
		else if (p > L) { p -= L; }
	};
	constexpr auto dist_forward = [L](double from, double to) {
		double d = to-from;
		if (d < 0) d+=L;
		return d;
	};
	constexpr auto adjust_reader = [&](double& r, double w, double hm) {
		const double a = w-r;
		const double b = fabs(a)-L/2;
		const double c = fabs(b);
		// minimize c(r)
		// c(r) = ||w-r|-L/2| 
		//      = |b(a(r))|
		//
		// c'(r) = dc(b(a(r)))/dr = dcdb*dbda*dadr
		// c'(r) = sign(b)*sign(a)*(-1)
		//
		// c(r) = happens to be the number of samples n such that c(r +- n) == 0
		// c'(r) = is the direction r should move to decrease c(r)
		// so c(r + c'(r)*c(r)) == 0
		// see mathematica notebook in this directory
		const double dcdb = sign(b);
		const double dcda = sign(a)*dcdb;
		const double dcdr = -1*dcda;
		// move r by a magnitude of c in the -c'(r) direction but by taking only steps of size SR/hm
		// cout << c << endl;
		// cout << -dcdr*grid << endl;
		double grid = SR/hm;
		grid = ceil(c/grid)*grid;
		move_index(r, -dcdr*grid);
	};
	constexpr auto fps = [](chrono::steady_clock::time_point& now) {
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
	};
	constexpr auto dura = [](double x) -> chrono::steady_clock::duration {
		// dura() converts a second, represented as a double, into the appropriate unit of
		// time for chrono::steady_clock and with the appropriate arithematic type
		// using dura() avoids errors like this : chrono::seconds(double initializer)
		// dura() : <double,seconds> -> chrono::steady_clock::<typeof count(), time unit>
		return chrono::duration_cast<chrono::steady_clock::duration>(chrono::duration<double, std::ratio<1>>(x));
	};
	auto max_frequency = [&](array1<Complex>&f) {
		// more info -> http://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak
		const int k = max_bin(f);
		const double y1 = mag(f[k-1]);
		const double y2 = mag(f[k]);
		const double y3 = mag(f[k+1]);
		const double d = (y3 - y1) / (2 * (2 * y2 - y1 - y3));
		const double kp  =  k + d;
		return kp * double(SRF)/double(FFTLEN);
	};
	constexpr auto clamp = [](double x, double l, double h) {
		if (x <= l) x = l;
		else if (x >= h) x = h;
		return x;
	};
	auto get_harmonic = [SRF, FFTLEN, &clamp](double freq) {
		// bound freq into the interval [24, 60]

		freq = clamp(freq, 1., double(SRF)/double(FFTLEN)*100.);

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
		// while (o > 61.f) o /= 2.f; // dividing frequency by two does not change the flipbook image
		// while (o < 24.f) o *= 2.f; // multiplying frequency by two produces the ghosting effect 
		return freq;
	};

	// Time Management
	auto now = chrono::steady_clock::now();
	auto next_l = now + dura(1./60.);
	auto next_r = next_l;
	auto prev = next_r; // Compute the fft only once every 1/60 of a second
	double rl = 0; // Index of a reader in the audio repositories (left channel)
	double rr = rl;
	double hml = 60.; // Harmonic frequency of the dominant frequency of the audio.
	double hmr = hml; // we capture an image of the waveform at this rate
	int W = 0; // The index of the writer in the audio repositories, of the newest sample in the buffers, and of a discontinuity in the waveform

	double maxl = 1.;
	double maxr = 1.;

	while (1) {

		// auto sss = std::chrono::steady_clock::now();
		// Read Datums
		if (pa_simple_read(s, buffer, sizeof(buffer), &error) < 0) {
			cout << "pa_simple_read() failed: " << pa_strerror(error) << endl;
			exit(EXIT_FAILURE);
		}
		// cout << std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(std::chrono::steady_clock::now()-sss).count()<<endl;

		// Fill audio repositories
		for (int i = 0; i < PL; i++) {
			pulse_buf_l[PL*W + i] = buffer[i*C+0];
			pulse_buf_r[PL*W + i] = buffer[i*C+1];
		}

		// Manage indices and fill wave smoothing buffers
		W++;
		W%=PN;
		now = chrono::steady_clock::now();
		if (now > next_l) {
			move_index(rl, SR/hml);
			if (dist_forward(rl,PL*W) < VL) {
				adjust_reader(rl, PL*W, hml);
				cout << "oopsl" << endl;
			}
			for (int i = 0; i < VL; ++i)
				tl[i] = pulse_buf_l[(i+int(rl))%L];
			// double MAX = 0.;
			// for (int i = 0; i < VL; ++i)
			// 	MAX = fabs(tl[i]) > MAX ? fabs(tl[i]) : MAX;
			// if (MAX>0.) {
			// 	for (int i = 0; i < VL; ++i)
			// 		tl[i] = tl[i]/MAX;
			// }
			hml = get_harmonic(max_frequency(fl));
			next_l += dura(1./hml);
		}
		if (now > next_r) {
			move_index(rr, SR/hmr);
			if (dist_forward(rr,PL*W) < VL) {
				adjust_reader(rr, PL*W, hmr);
				cout << "oopsr" << endl;
			}
			for (int i = 0; i < VL; ++i)
				tr[i] = pulse_buf_r[(i+int(rr))%L];
			// double MAX = 0.;
			// for (int i = 0; i < VL; ++i)
			// 	MAX = fabs(tr[i]) > MAX ? fabs(tr[i]) : MAX;
			// if (MAX>0.) {
			// 	for (int i = 0; i < VL; ++i)
			// 		tr[i] = tr[i]/MAX;
			// }
			hmr = get_harmonic(max_frequency(fr));
			next_r += dura(1./hmr);
		}

		// Preprocess audio and execute the FFT
		if (now > prev) {
			prev = now + dura(1./60.);
			for (int i = 0; i < FFTLEN; ++i) {
				fl[i] = Complex(FFTwindow[i] * pulse_buf_l[(i*2+PL*W)%L]);
				fr[i] = Complex(FFTwindow[i] * pulse_buf_r[(i*2+PL*W)%L]);
			}
			Forwardl.fft(fl);
			Forwardr.fft(fr);
		}

		// Fill presentation buffers
		audio->mtx.lock();
		for(int i = 0; i < FFTLEN/2; i++) {
			audio->freq_l[i] = mag(fl[i]/double(FFTLEN));
			audio->freq_r[i] = mag(fr[i]/double(FFTLEN));
		}
		// Smooth and upsample the wave
		// const double D = .85;
		// const double D = .05;
		const double D = .07;
		// const double D = .2;
		// const double Pl = pow(maxl, 0.1);
		// const double Pr = pow(maxr, 0.1);
		for (int i = 0; i < VL; ++i) {
			const double al = (tl[i/2]+tl[(i+1)/2])/2.;
			const double ar = (tr[i/2]+tr[(i+1)/2])/2.;
			// audio->audio_l[i] = audio->audio_l[i]*(1.-D)+D*(Pl*al + (1.-Pl)*sin(6.288*i/double(VL)));
			// audio->audio_r[i] = audio->audio_r[i]*(1.-D)+D*(Pr*ar + (1.-Pr)*cos(6.288*i/double(VL)));
			audio->audio_l[i] = audio->audio_l[i]*(1.-D)+D*al;
			audio->audio_r[i] = audio->audio_r[i]*(1.-D)+D*ar;
		}
		// double MAX = 0.;
		// double C = .9;
		// for (int i = 0; i < VL; ++i)
		// 	MAX = fabs(audio->audio_l[i]) > MAX ? fabs(audio->audio_l[i]) : MAX;
		// maxl = maxl*C+(1.-C)*MAX;
		// MAX=0.;
		// for (int i = 0; i < VL; ++i)
		// 	MAX = fabs(audio->audio_r[i]) > MAX ? fabs(audio->audio_r[i]) : MAX;
		// maxr = maxr*C+(1.-C)*MAX;
		// if (maxl>0.) {
		// 	for (int i = 0; i < VL; ++i)
		// 		audio->audio_l[i] = audio->audio_l[i]/maxl * .7;
		// }
		// if (maxr>0.) {
		// 	for (int i = 0; i < VL; ++i)
		// 		audio->audio_r[i] = audio->audio_r[i]/maxr * .7;
		// }
		audio->mtx.unlock();

		if (audio->thread_join) {
			pa_simple_free(s);
			break;
		}

		// fps(now);
		// Turning this on causes the indices to get all fd up
		// std::this_thread::sleep_for(std::chrono::milliseconds(13));

	}
	return 0;
}
