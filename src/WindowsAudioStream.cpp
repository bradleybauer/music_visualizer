#include <iostream>
using std::cout; using std::endl;

#include "WindowsAudioStream.h"

#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <audiopolicy.h>
#include <mmreg.h>

WindowsAudioStream::WindowsAudioStream(bool &is_ok) {
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
	if (hr) {
		cout << "WindowsAudioStream error" << endl;
		is_ok = false;
		return;
	}

	hr = m_pEnumerator->GetDefaultAudioEndpoint(
		//eCapture, eConsole, &pDevice); //set this to capture from default recording device instead of render device
		eRender, eConsole, &m_pDevice);
	if (hr) {
		cout << "WindowsAudioStream error" << endl;
		is_ok = false;
		return;
	}

	hr = m_pDevice->Activate(
		m_IID_IAudioClient, CLSCTX_ALL,
		NULL, (void**)&m_pAudioClient);
	if (hr) {
		cout << "WindowsAudioStream error" << endl;
		is_ok = false;
		return;
	}

	hr = m_pAudioClient->GetMixFormat(&m_pwfx);
	if (hr) {
		cout << "WindowsAudioStream error" << endl;
		is_ok = false;
		return;
	}

	const int bits_per_byte = 8;
	m_pwfx->wBitsPerSample = sizeof(short) * bits_per_byte;
	m_pwfx->nBlockAlign = 4;
	m_pwfx->wFormatTag = 1;
	m_pwfx->cbSize = 0;
	m_pwfx->nAvgBytesPerSec = m_pwfx->nSamplesPerSec * m_pwfx->nBlockAlign;

	sample_rate = m_pwfx->nSamplesPerSec;

	hr = m_pAudioClient->Initialize(
		//AUDCLNT_SHAREMODE_EXCLUSIVE,
		AUDCLNT_SHAREMODE_SHARED,
		//0, //set this instead of loopback to capture from default recording device
		AUDCLNT_STREAMFLAGS_LOOPBACK,
		hnsRequestedDuration,
		0,
		m_pwfx,
		NULL);
	if (hr) {
		cout << "WindowsAudioStream error" << endl;
		is_ok = false;
		return;
	}

	// Get the size of the allocated buffer.
	hr = m_pAudioClient->GetBufferSize(&m_bufferFrameCount);
	if (hr) {
		cout << "WindowsAudioStream error" << endl;
		is_ok = false;
		return;
	}

	hr = m_pAudioClient->GetService(
		m_IID_IAudioCaptureClient,
		(void**)&m_pCaptureClient);
	if (hr) {
		cout << "WindowsAudioStream error" << endl;
		is_ok = false;
		return;
	}

	// Start capturing audio
	hr = m_pAudioClient->Start();
	if (hr) {
		cout << "WindowsAudioStream error" << endl;
		is_ok = false;
		return;
	}
}

WindowsAudioStream::~WindowsAudioStream() {
	HRESULT hr = m_pAudioClient->Stop();
	if (hr) {
		cout << "WindowsAudioStream error" << endl;
		return;
	}
}

void WindowsAudioStream::get_next_pcm(float * buff_l, float * buff_r, int buff_size) {
	const int C = 2;
	HRESULT hr;

	// if frame_cache_fill != 0
	//     use frame_cache
	//     remove frame_cache
	// if more needed
	//     use system provided
	//     grow frame_cache

	int i = 0;
	if (frame_cache_fill) {
		int read = frame_cache_fill > buff_size ? buff_size : frame_cache_fill;
		for (; i < read; i++) {
			buff_l[i] = frame_cache[i * C + 0]/32768.f;
			buff_r[i] = frame_cache[i * C + 1]/32768.f;
		}
		frame_cache_fill -= read;
		// Remove from the fifo the frames we've just read
		memcpy(frame_cache, frame_cache + C*read, sizeof(short)*C*frame_cache_fill);
        if (frame_cache_fill >= CACHE_SIZE)
            cout << frame_cache_fill << endl;
	}

	// if i == buff_size, then we can just return. (audio_buff completely filled from frame_cache)
	// We do not lose some section of the system audio stream if we return here, because we've not called GetBuffer yet.
	// So we do not need to fill the frame_cache with anything here if we return here.
	// We only need to fill the frame_cache with the frames we don't use after calling GetBuffer

	short* pData;
	DWORD flags;
	UINT32 numFramesAvailable;
	UINT32 packetLength = 0;
	while (i < buff_size) {
		hr = m_pCaptureClient->GetNextPacketSize(&packetLength);
		if (hr) {
			cout << "WindowsAudioStream error" << endl;
			return;
		}

		if (packetLength != 0) {
			// Get the available data in the shared buffer.
			hr = m_pCaptureClient->GetBuffer(
				(BYTE**)&pData,
				&numFramesAvailable,
				&flags, NULL, NULL);
			if (hr) {
				cout << "WindowsAudioStream error" << endl;
				return;
			}

			// Copy the available capture data to the audio sink.
			int j = 0; // j==1 is 1 frame
			for (; i + j < buff_size && j < numFramesAvailable; j++) {
				buff_l[i + j] = pData[j * C + 0]/32768.f;
				buff_r[i + j] = pData[j * C + 1]/32768.f;
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
			if (hr) {
				cout << "WindowsAudioStream error" << endl;
				return;
			}
		}
	}
}

int WindowsAudioStream::get_sample_rate() {
	return sample_rate;
}
