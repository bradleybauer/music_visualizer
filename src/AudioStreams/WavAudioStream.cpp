#include <iostream>
using std::cout; using std::endl;
#include <fstream>
using std::ifstream;
using std::ios;
#include <cstdio>
#include <stdexcept>
#include <cstring> // memory stuff

#include "WavAudioStream.h"

// Create a WavStreamer backed by a CircularBuffer<short>?

// https://github.com/tkaczenko/WavReader/blob/master/WavReader/WavReader.cpp
//Wav Header
struct wav_header_t {
	char chunkID[4]; //"RIFF" = 0x46464952
	unsigned long chunkSize; //28 [+ sizeof(wExtraFormatBytes) + wExtraFormatBytes] + sum(sizeof(chunk.id) + sizeof(chunk.size) + chunk.size)
	char format[4]; //"WAVE" = 0x45564157
	char subchunk1ID[4]; //"fmt " = 0x20746D66
	unsigned long subchunk1Size; //16 [+ sizeof(wExtraFormatBytes) + wExtraFormatBytes]
	unsigned short audioFormat;
	unsigned short numChannels;
	unsigned long sampleRate;
	unsigned long byteRate;
	unsigned short blockAlign;
	unsigned short bitsPerSample;
	//[WORD wExtraFormatBytes;]
	//[Extra format bytes]
};

//Chunks
struct chunk_t {
	char ID[4]; //"data" = 0x61746164
	unsigned long size;  //Chunk data bytes
};

// Reads entire file into buffer
WavAudioStream::WavAudioStream(const filesys::path &wav_path) {
	if (!filesys::exists(wav_path))
		throw std::runtime_error("WavAudioStream: wav file not found");
	ifstream fin(wav_path.string(), std::ios::binary);
	if (!fin.is_open())
		throw std::runtime_error("WavAudioStream: file did not open" );
	fin.unsetf(ios::skipws);

	//Read WAV header
	wav_header_t header;
	fin.read((char*)&header, sizeof(header));

	//Print WAV header
	/*
	cout << "WAV File Header read:" << endl;
	cout << "File Type: " << header.chunkID << endl;
	cout << "File Size: " << header.chunkSize << endl;
	cout << "WAV Marker: " << header.format << endl;
	cout << "Format Name: " << header.subchunk1ID << endl;
	cout << "Format Length: " << header.subchunk1Size << endl;
	cout << "Format Type: " << header.audioFormat << endl;
	cout << "Number of Channels: " << header.numChannels << endl;
	cout << "Sample Rate: " << header.sampleRate << endl;
	cout << "Sample Rate * Bits/Sample * Channels / 8: " << header.byteRate << endl;
	cout << "Bits per Sample * Channels / 8.1: " << header.blockAlign << endl;
	cout << "Bits per Sample: " << header.bitsPerSample << endl;
	*/
	sample_rate = header.sampleRate;

	//Reading file
	chunk_t chunk;
	//go to data chunk
	while (true) {
		fin.read((char*)&chunk, sizeof(chunk));
		if (*(unsigned int *)&chunk.ID == 0x61746164)
			break;
		//skip chunk data bytes
		fin.seekg(chunk.size, ios::cur);
	}

	//Number of samples
	int sample_size = header.bitsPerSample / 8;
	int samples_count = chunk.size * 8 / header.bitsPerSample;

	buf_interlaced = new short[samples_count];
	memset(buf_interlaced, 0, sizeof(short) * samples_count);

	//Reading data
	for (int i = 0; i < samples_count; ++i) {
		fin.read((char*)&buf_interlaced[i], sample_size);
	}

	// TODO do not load the whole file to memory
}

WavAudioStream::~WavAudioStream() {
	if (buf_interlaced)
		delete[] buf_interlaced;
}

void WavAudioStream::get_next_pcm(float * buff_l, float * buff_r, int buff_size) {
	// TODO
	cout << "WavAudioStream.get_next_pcm not implemented yet" << endl;
	exit(1);
}

int WavAudioStream::get_sample_rate() {
	return sample_rate;
}

int WavAudioStream::get_max_buff_size() {
	return max_buff_size;
}
