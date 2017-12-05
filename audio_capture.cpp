#include "audio_capture.h"
#include <iostream>
using std::cout;
using std::endl;
#ifdef LINUX
#include "audio_data.h"
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

#ifdef LINUX
static float* buf_interlaced;
static int pulseError;
static pa_simple* pulseState = NULL;
#endif
#ifdef WINDOWS
static IAudioClient* m_pAudioClient = NULL;
static IAudioCaptureClient* m_pCaptureClient = NULL;
static IMMDeviceEnumerator* m_pEnumerator = NULL;
static IMMDevice* m_pDevice = NULL;
static const int m_refTimesPerMS = 10000;
#endif

static void get_pcm_linux(float* audio_buf_l, float* audio_buf_r, int ABL) {
	int C = 2;
#ifdef LINUX
	if (pa_simple_read(pulseState, buf_interlaced, ABL*C*sizeof(float), &pulseError) < 0) {
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
static int frame_cache_fill = 0;
// This is a fifo
static short frame_cache[CACHE_SIZE];
#endif
static void get_pcm_windows(float* audio_buf_l, float* audio_buf_r, int ABL) {
	// TODO extract the FIFO, write a test

	int C = 2;
#ifdef WINDOWS
	HRESULT hr;

	// if frame_cache_fill != 0
	//     use frame_cache
	//     remove frame_cache
	// if more needed
	//     use system provided
	//     grow frame_cache

	int i = 0;
	if (frame_cache_fill) {
		int read = frame_cache_fill > ABL ? ABL : frame_cache_fill;
		for (; i < read; i++) {
			audio_buf_l[i] = frame_cache[i * C + 0]/32768.f;
			audio_buf_r[i] = frame_cache[i * C + 1]/32768.f;
		}
		frame_cache_fill -= read;
		// Remove from the fifo the frames we've just read
		memcpy(frame_cache, frame_cache + C*read, sizeof(short)*C*frame_cache_fill);
	}

	// if i == ABL, then we can just return. (audio_buff completely filled from frame_cache)
	// We do not lose some section of the system audio stream if we return here, because we've not called GetBuffer yet.
	// So we do not need to fill the frame_cache with anything here if we return here.
	// We only need to fill the frame_cache with the frames we don't use after calling GetBuffer

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
			int j = 0; // j==1 is 1 frame
			for (; i + j < ABL && j < numFramesAvailable; j++) {
				audio_buf_l[i + j] = pData[j * C + 0]/32768.f;
				audio_buf_r[i + j] = pData[j * C + 1]/32768.f;
			}
			i += j;

			// If we didn't use all the frames returned by the system
			if (j != numFramesAvailable) {
				if (frame_cache_fill + (numFramesAvailable - j) >= CACHE_SIZE) {
					cout << "fifo overflow in audio_capture::get_pcm" << endl;
					exit(-1);
				}
				// Then copy what we didn't use to the frame_cache
				memcpy(frame_cache + C*frame_cache_fill, pData + j*C, sizeof(short)*C*(numFramesAvailable - j));
				frame_cache_fill += numFramesAvailable - j;
			}

			hr = m_pCaptureClient->ReleaseBuffer(numFramesAvailable);
			if (hr) throw hr;
		}
	}
#endif
}

void get_pcm(float* audio_buf_l, float* audio_buf_r, int ABL) {
	// nop if windows
	get_pcm_linux(audio_buf_l, audio_buf_r, ABL);
	// nop if linux
	get_pcm_windows(audio_buf_l, audio_buf_r, ABL);
}

static void setup_linux(const int ABL, const int SR, struct audio_data* audio) {
	int C = 2;
#ifdef LINUX
	getPulseDefaultSink((void*)audio);
	buf_interlaced = new float[ABL * C];
	pa_sample_spec pulseSampleSpec;
	pulseSampleSpec.channels = C;
	pulseSampleSpec.rate = SR;
	pulseSampleSpec.format = PA_SAMPLE_FLOAT32NE;
	pa_buffer_attr pb;
	pb.fragsize = ABL*C*sizeof(float) / 2;
	pb.maxlength = ABL*C*sizeof(float);
	pulseState = pa_simple_new(NULL, "Music Visualizer", PA_STREAM_RECORD, audio->source.data(), "Music Visualizer", &pulseSampleSpec, NULL,
	                  &pb, &pulseError);
	if (!pulseState) {
		cout << "Could not open pulseaudio source: " << audio->source << pa_strerror(pulseError)
		     << ". To find a list of your pulseaudio sources run 'pacmd list-sources'" << endl;
		exit(EXIT_FAILURE);
	}
#endif
}

static void setup_windows(const int ABL, const int SR) {
	int C = 2;
#ifdef WINDOWS
	// --Difficulties arise--
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

// ABL: audio buffer length in frames
// SR: sample rate, number of frames per second
void audio_setup(const int ABL, const int SR, struct audio_data* audio) {
	// nop if windows
	setup_linux(ABL, SR, audio);
	// nop if linux
	setup_windows(ABL, SR);
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
