#include "audio_capture.h"
#include <iostream>
using std::cout;
using std::endl;
#ifdef LINUX
#include "pulse_misc.h"
#endif
#ifdef WINDOWS
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <audiopolicy.h>
#include <mmreg.h>
#endif


// ABL number of samples to capture in each channel of audio
// C number of channels
// SR sample rate to capture

static float* buf_interlaced;

#ifdef LINUX
static int pulseError;
static pa_simple* pulseState = NULL;
static pa_sample_spec pulseSampleSpec;
#endif
#ifdef WINDOWS
static IAudioClient* m_pAudioClient = NULL;
static IAudioCaptureClient* m_pCaptureClient = NULL;
static IMMDeviceEnumerator* m_pEnumerator = NULL;
static IMMDevice* m_pDevice = NULL;
static const int m_refTimesPerMS = 10000;
#endif

static void get_pcm_linux(float* audio_buf_l, float* audio_buf_r, int ABL, int C) {
#ifdef LINUX
	if (pa_simple_read(pulseState, buf_interlaced, sizeof(buf_interlaced), &pulseError) < 0) {
		cout << "pa_simple_read() failed: " << pa_strerror(pulseError) << endl;
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < ABL; i++) {
		audio_buf_l[i] = buf_interlaced[i * C + 0];
		audio_buf_r[i] = buf_interlaced[i * C + 1];
	}
#endif
}

// TODO
// Sample rate
// Understand setup code and make sure it is right

#ifdef WINDOWS
static const int CACHE_SIZE = 10000;
static int cache_fill = 0;
static short cache[CACHE_SIZE];
#endif
static void get_pcm_windows(float* audio_buf_l, float* audio_buf_r, int ABL, int C) {
#ifdef WINDOWS
	HRESULT hr;

	// if cache
	//		use cache
	//		shrink cache
	// else
	//		use system
	//		grow cache
	
	static int maxC = 0;
	if (cache_fill > maxC) {
		maxC = cache_fill;
		cout << maxC << endl;
	}

	int i = 0;
	if (cache_fill) {
		int read = cache_fill > ABL ? ABL : cache_fill;
		for (; i < read; i++) {
			audio_buf_l[i] = cache[i * C + 0]/32768.f;
			audio_buf_r[i] = cache[i * C + 1]/32768.f;
		}
		cache_fill -= read;
		if (read == ABL) {
			memcpy(cache, cache + C*read, sizeof(short)*2000);
			return;
		}
	}

	short* pData;
	DWORD flags;
	UINT32 numFramesAvailable;
	UINT32 packetLength = 0;
	while (i < ABL) {
		hr = m_pCaptureClient->GetNextPacketSize(&packetLength);
		if (hr) throw hr;

		if (packetLength != 0) {
			// Get the available data in the shared buffer.
			hr = m_pCaptureClient->GetBuffer(
				(BYTE**)&pData,
				&numFramesAvailable,
				&flags, NULL, NULL);
			if (hr) throw hr;

			// Copy the available capture data to the audio sink.
			int j = 0;
			for (; i + j < ABL && j < numFramesAvailable; j++) {
				audio_buf_l[i + j] = pData[j * C + 0]/32768.f;
				audio_buf_r[i + j] = pData[j * C + 1]/32768.f;
			}
			i += j;
			if (j != numFramesAvailable) {
				memcpy(cache, pData + j*C, sizeof(short)*2000);
				cache_fill = numFramesAvailable - j;
			}

			hr = m_pCaptureClient->ReleaseBuffer(numFramesAvailable);
			if (hr) throw hr;
		}
	}
#endif
}

void get_pcm(float* audio_buf_l, float* audio_buf_r, int ABL, int C) {
#ifdef LINUX
	get_pcm_linux(audio_buf_l, audio_buf_r, ABL, C);
#endif
#ifdef WINDOWS
	get_pcm_windows(audio_buf_l, audio_buf_r, ABL, C);
#endif
}

static void setup_linux(const int ABL, const int C, const int SR, struct audio_data* audio) {
#ifdef LINUX
	getPulseDefaultSink((void*)audio);
	buf_interlaced = new float[ABL * C];
	pulseSampleSpec.channels = C;
	pulseSampleSpec.rate = SR;
	pulseSampleSpec.format = PA_SAMPLE_FLOAT32NE;
	pa_buffer_attr pb;
	pb.fragsize = sizeof(buf_interlaced) / 2;
	pb.maxlength = sizeof(buf_interlaced);
	pulseState = pa_simple_new(NULL, "APPNAME", PA_STREAM_RECORD, audio->source.data(), "APPNAME", &pulseSampleSpec, NULL,
	                  &pb, &pulseError);
	if (!pulseState) {
		cout << "Could not open pulseaudio source: " << audio->source << pa_strerror(pulseError)
		     << ". To find a list of your pulseaudio sources run 'pacmd list-sources'" << endl;
		exit(EXIT_FAILURE);
	}
#endif
}

static void setup_windows(const int ABL, const int C, const int SR, struct audio_data* audio) {
#ifdef WINDOWS
	// Difficulties arise
	// PCM CAPTURE CONTROL FLOW
	// PCM SAMPLE RATE
	// pulse audio's function that gives me pcm blocks until the amount i've asked for has been delivered
	// window's function tells me how much I get, gives me that much and the returns.
	// if i want the apis to act the same I need to wrap window's function with a loop until the amount
	// i ask for has been given.
	// but then there might be excess
	// i would have to cache the excess and use it in the next call
	// sounds complicated
	// unless windows happens to give me the amount i need every single time I ask for some data
	// also pulse allows me to ask for a certain sample rate
	// windows tells me what sample rate i'll get

	CoInitialize(nullptr);

	WAVEFORMATEX* m_pwfx = NULL;
	UINT32 m_bufferFrameCount;
	const CLSID m_CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID m_IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID m_IID_IAudioClient = __uuidof(IAudioClient);
	const IID m_IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
	HRESULT hr;
	const int m_refTimesPerSec = 10000000;
	REFERENCE_TIME hnsRequestedDuration = m_refTimesPerSec;

	hr = CoCreateInstance(
		m_CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, m_IID_IMMDeviceEnumerator,
		(void**)&m_pEnumerator);
	if (hr)	throw hr;

	hr = m_pEnumerator->GetDefaultAudioEndpoint(
		//eCapture, eConsole, &pDevice); //set this to capture from default recording device instead of render device
		eRender, eConsole, &m_pDevice);
	if (hr)	throw hr;

	hr = m_pDevice->Activate(
		m_IID_IAudioClient, CLSCTX_ALL,
		NULL, (void**)&m_pAudioClient);
	if (hr)	throw hr;

	hr = m_pAudioClient->GetMixFormat(&m_pwfx);
	if (hr)	throw hr;

	const int bits_per_byte = 8;
	m_pwfx->wBitsPerSample = sizeof(short) * bits_per_byte;
	m_pwfx->nBlockAlign = 4;
	m_pwfx->wFormatTag = 1;
	m_pwfx->cbSize = 0;
	m_pwfx->nAvgBytesPerSec = m_pwfx->nSamplesPerSec * m_pwfx->nBlockAlign;

	hr = m_pAudioClient->Initialize(
		//AUDCLNT_SHAREMODE_EXCLUSIVE,
		AUDCLNT_SHAREMODE_SHARED,
		//0, //set this instead of loopback to capture from default recording device
		AUDCLNT_STREAMFLAGS_LOOPBACK,
		hnsRequestedDuration,
		0,
		m_pwfx,
		NULL);
	if (hr)	throw hr;

	// Get the size of the allocated buffer.
	hr = m_pAudioClient->GetBufferSize(&m_bufferFrameCount);
	if (hr)	throw hr;

	hr = m_pAudioClient->GetService(
		m_IID_IAudioCaptureClient,
		(void**)&m_pCaptureClient);
	if (hr) throw hr;

	// Start capturing audio
	hr = m_pAudioClient->Start();
	if (hr) throw hr;
#endif
}

void audio_setup(const int ABL, const int C, const int SR, struct audio_data* audio) {
#ifdef LINUX
	setup_linux(ABL, C, SR, audio);
#endif
#ifdef WINDOWS
	setup_windows(ABL, C, SR, audio);
#endif
}

void audio_destroy() {
#ifdef LINUX
	pa_simple_free(pulseState);
#endif
#ifdef WINDOWS
	HRESULT hr = m_pAudioClient->Stop();  // Stop recording.
	if (hr) throw hr;
#endif
}
