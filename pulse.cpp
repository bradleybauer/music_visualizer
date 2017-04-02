#include <iostream>
using std::cout;
using std::endl;
#include <chrono>

#include "audio_data.h"

#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

#include "Array.h"
#include "fftwpp/fftw++.h"
using namespace utils;
using namespace fftwpp;


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

pa_mainloop* m_pulseaudio_mainloop;
void cb(pa_context* pulseaudio_context, const pa_server_info* i, void* data) {
	struct audio_data* audio = (struct audio_data*)data;
	audio->source = i->default_sink_name;
	audio->source += ".monitor";
	pa_mainloop_quit(m_pulseaudio_mainloop, 0);
}
void pulseaudio_context_state_callback(pa_context* pulseaudio_context, void* data) {
	switch (pa_context_get_state(pulseaudio_context)) {
	case PA_CONTEXT_UNCONNECTED:
		// cout << "UNCONNECTED" << endl;
		break;
	case PA_CONTEXT_CONNECTING:
		// cout << "CONNECTING" << endl;
		break;
	case PA_CONTEXT_AUTHORIZING:
		// cout << "AUTHORIZING" << endl;
		break;
	case PA_CONTEXT_SETTING_NAME:
		// cout << "SETTING_NAME" << endl;
		break;
	case PA_CONTEXT_READY: // extract default sink name
		// cout << "READY" << endl;
		pa_operation_unref(pa_context_get_server_info(pulseaudio_context, cb, data));
		break;
	case PA_CONTEXT_FAILED:
		// cout << "FAILED" << endl;
		break;
	case PA_CONTEXT_TERMINATED:
		// cout << "TERMINATED" << endl;
		pa_mainloop_quit(m_pulseaudio_mainloop, 0);
		break;
	}
}
void getPulseDefaultSink(void* data) {
	pa_mainloop_api* mainloop_api;
	pa_context* pulseaudio_context;
	int ret;
	// Create a mainloop API and connection to the default server
	m_pulseaudio_mainloop = pa_mainloop_new();
	mainloop_api = pa_mainloop_get_api(m_pulseaudio_mainloop);
	pulseaudio_context = pa_context_new(mainloop_api, "APPNAME device list");
	// This function connects to the pulse server
	pa_context_connect(pulseaudio_context, NULL, PA_CONTEXT_NOFLAGS, NULL);
	// This function defines a callback so the server will tell us its state.
	pa_context_set_state_callback(pulseaudio_context, pulseaudio_context_state_callback, data);
	// starting a mainloop to get default sink
	if (pa_mainloop_run(m_pulseaudio_mainloop, &ret) < 0) {
		cout << "Could not open pulseaudio mainloop to find default device name: %d" << ret << endl;
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

static inline double tau(double x) {
	return 1./4. * log(3.*x*x + 6.*x + 1.) - sqrt(6.)/24. * log((x + 1. - sqrt(2./3.))  /  (x + 1. + sqrt(2./3.)));
}

void* audioThreadMain(void* data) {
	struct audio_data* audio = (struct audio_data*)data;
	float buffer[BUFFSIZE*2];

	float fft_window[BUFFSIZE];
	const float win_arg = 2.f * M_PI / (float(BUFFSIZE) - 1.f);
	for (int i = 0; i < BUFFSIZE; ++i)
		fft_window[i] = .5f * (1.f - cos(win_arg * i));

	fftw::maxthreads=get_max_threads();
	const int numFFTs = 2;
	const int fftLen = BUFFSIZE;
	Complex* f = ComplexAlign(fftLen*numFFTs);
	mfft1d Forward(fftLen,-1,numFFTs,numFFTs,1);

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
		return sqrt(a.real()*a.real()+a.imag()*a.imag());
	};
	const auto max_frequency = [&f, mag, fftLen](int offset) {
		// f is fft output for both channels
		// offset is offset into the array 
		double max = 0.;
		int max_i = 0;
		// test |i|+|r| instead of magnitude
		for (int i = 0; i < 50; ++i) {
			if (mag(f[offset+i]) > max) {
				max = mag(f[offset+i]);
				max_i = i;
			}
		}
		if (max_i == 0) max_i++;
		if (max_i == 50) max_i--;

		// more info -> http://dspguru.com/dsp/howtos/how-to-interpolate-fft-peak
		const auto gfr = [&f, offset](int i){return f[offset + i].real();};
		const auto gfi = [&f, offset](int i){return f[offset + i].imag();};
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
		for (int i = 0; i < BUFFSIZE; i++) {
			audio->audio_l[i] = .5f + .5f*buffer[i*2+0];
			f[i] = Complex((double)buffer[i*2] * fft_window[i]);
			audio->audio_r[i] = .5f + .5f*buffer[i*2+1];
			f[fftLen+i] = Complex((double)buffer[i*2+1] * fft_window[i]);
		}
		Forward.fft(f);
		for(int i=0; i < fftLen; i++) {
			audio->freq_l[i] = mag(f[i])/sqrt(fftLen);
			audio->freq_r[i] = mag(f[2*fftLen+i])/sqrt(fftLen);
			// I write f[2*fftLen + i] to skip the mirrored half of the first channel's fft
		}
		if (audio->thread_join) {
			pa_simple_free(s);
			break;
		}
	}
	deleteAlign(f);
	return 0;
}
