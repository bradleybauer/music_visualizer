#pragma once

#include "AudioStream.h"

#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <audiopolicy.h>
#include <mmreg.h>

class WindowsAudioStream : public AudioStream {
public:
	WindowsAudioStream();
	~WindowsAudioStream();
	void get_next_pcm(float* buff_l, float* buff_r, int buff_size);
	int get_sample_rate();

private:
	int sample_rate;

	IAudioClient* m_pAudioClient = NULL;
	IAudioCaptureClient* m_pCaptureClient = NULL;
	IMMDeviceEnumerator* m_pEnumerator = NULL;
	IMMDevice* m_pDevice = NULL;

	static const int m_refTimesPerMS = 10000;
	static const int CACHE_SIZE = 10000;

	int frame_cache_fill = 0;
	short frame_cache[WindowsAudioStream::CACHE_SIZE];
};
