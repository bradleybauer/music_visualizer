#include <iostream>
using std::cout;
using std::endl;
#include <chrono>
namespace chrono = std::chrono;
#include <stdexcept>
using std::runtime_error;

#include "WindowsAudioStream.h"

#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <audiopolicy.h>
#include <mmreg.h>

#define CHECK(call) if (HRESULT x = call; x) { cout << "Windows audio error code " << x << " at line " << __LINE__ << endl; exit(x); }

WindowsAudioStream::WindowsAudioStream() {
	// --Difficulties arise--
	// PCM CAPTURE CONTROL FLOW
	// PCM SAMPLE RATE
	// pulse audio's function that gives me pcm blocks until the amount i've asked for has been delivered
	// window's function tells me how much I get, gives me that much and then returns.
	// if I want the apis to act the same I need to wrap window's function with a loop until the amount
	// I ask for has been given. But then I have to cache the excess and use it in the next call.
    //
	// Unless windows happens to give me the amount i need every single time I ask for some data
	// also pulse allows me to ask for a certain sample rate
	// windows tells me what sample rate i'll get

	CoInitialize(nullptr);

	const int m_refTimesPerSec = 10000000;
	REFERENCE_TIME hnsRequestedDuration = m_refTimesPerSec;

	CHECK(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),(void**)&m_pEnumerator));

    CHECK(m_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_pDevice));

    CHECK(m_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,NULL, (void**)&m_pAudioClient));

	WAVEFORMATEX* m_pwfx = NULL;
	CHECK(m_pAudioClient->GetMixFormat(&m_pwfx));
	sample_rate = m_pwfx->nSamplesPerSec;
	const int bits_per_byte = 8;
	m_pwfx->wBitsPerSample = sizeof(short) * bits_per_byte;
	m_pwfx->nBlockAlign = 4;
	m_pwfx->wFormatTag = 1;
	m_pwfx->cbSize = 0;
	m_pwfx->nAvgBytesPerSec = m_pwfx->nSamplesPerSec * m_pwfx->nBlockAlign;

    CHECK(m_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK,hnsRequestedDuration,0,m_pwfx,NULL));

    CHECK(m_pAudioClient->GetService(__uuidof(IAudioCaptureClient),(void**)&m_pCaptureClient));

	// Start capturing audio
    CHECK(m_pAudioClient->Start());
}

WindowsAudioStream::~WindowsAudioStream() {
	CHECK(m_pAudioClient->Stop());
}

void WindowsAudioStream::get_next_pcm(float * buff_l, float * buff_r, int buff_size) {
    const int channels = 2;

	UINT32 packetLength = 0;

	short* pData;
	DWORD flags;
	UINT32 numFramesAvailable;

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
			buff_l[i] = frame_cache[i * channels + 0]/32768.f;
			buff_r[i] = frame_cache[i * channels + 1]/32768.f;
		}
		frame_cache_fill -= read;
		// Remove from the fifo the frames we've just read
		memcpy(frame_cache, frame_cache + channels*read, sizeof(short)*channels*frame_cache_fill);
	}

	// if i == buff_size, then we can just return. (audio_buff completely filled from frame_cache)
	// We do not lose some section of the system audio stream if we return here, because we've not called GetBuffer yet.
	// So we do not need to fill the frame_cache with anything here if we return here.
	// We only need to fill the frame_cache with the frames we don't use after calling GetBuffer

    auto start = chrono::steady_clock::now();
    while (i < buff_size) {

		// TODOFPS
        if (chrono::steady_clock::now() - start > chrono::milliseconds(144)) {
            std::fill(buff_l, buff_l + buff_size, 0.f);
            std::fill(buff_r, buff_r + buff_size, 0.f);
            break;
        }

        CHECK(m_pCaptureClient->GetNextPacketSize(&packetLength));
        if (packetLength != 0) {
            // Get the available data in the shared buffer.
            CHECK(m_pCaptureClient->GetBuffer((BYTE**)&pData, &numFramesAvailable, &flags, NULL, NULL));

            // Copy the available capture data to the audio sink.
            int j = 0; // j==1 is 1 frame
            for (; i + j < buff_size && j < numFramesAvailable; j++) {
                buff_l[i + j] = pData[j * channels + 0] / 32768.f;
                buff_r[i + j] = pData[j * channels + 1] / 32768.f;
            }
            i += j;

            // If we didn't use all the frames returned by the system
            if (j != numFramesAvailable) {
                if (frame_cache_fill + (numFramesAvailable - j) >= CACHE_SIZE) {
                    cout << "WindowsAudioStream::get_next_pcm fifo overflow" << endl;
                    exit(-1);
                }
                // Then copy what we didn't use to the frame_cache
                memcpy(frame_cache + channels * frame_cache_fill, pData + j * channels, sizeof(short)*channels*(numFramesAvailable - j));
                frame_cache_fill += numFramesAvailable - j;
            }

            CHECK(m_pCaptureClient->ReleaseBuffer(numFramesAvailable));
        }
    }
}

int WindowsAudioStream::get_sample_rate() {
	return sample_rate;
}

int WindowsAudioStream::get_max_buff_size() {
	return max_buff_size;
}
