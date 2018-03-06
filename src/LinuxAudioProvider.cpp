#include <iostream>
using std::cout;
using std::endl;

#include "LinuxAudioProvider.h"

LinuxAudioStream::LinuxAudioStream() {
	std::string sink_name;
	getPulseDefaultSink((void*)&sink_name);
	sink_name += ".monitor";

	// TODO is this released?
	buf_interlaced = new float[sample_rate * channels];

	pa_sample_spec pulseSampleSpec;
	pulseSampleSpec.channels = channels;
	pulseSampleSpec.rate = sample_rate;
	pulseSampleSpec.format = PA_SAMPLE_FLOAT32NE;
	pa_buffer_attr pb;
	pb.fragsize = buff_size*channels*sizeof(float) / 2;
	pb.maxlength = buff_size*channels*sizeof(float);
	pulseState = pa_simple_new(NULL, "Music Visualizer", PA_STREAM_RECORD, sink_name.c_str(), "Music Visualizer", &pulseSampleSpec, NULL,
	                  &pb, &pulseError);
	if (!pulseState) {
		cout << "Could not open pulseaudio source: " << sink_name.c_str() << " " << pa_strerror(pulseError)
		     << ". To find a list of your pulseaudio sources run 'pacmd list-sources'" << endl;
		exit(EXIT_FAILURE);
	}
}

LinuxAudioStream::~LinuxAudioStream() {
	pa_simple_free(pulseState);
}

void LinuxAudioStream::get_next_pcm(float * buff_l, float * buff_r, int buff_size) {
	if (pa_simple_read(pulseState, buf_interlaced, buff_size*channels*sizeof(float), &pulseError) < 0) {
		cout << "pa_simple_read() failed: " << pa_strerror(pulseError) << endl;
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < buff_size; i++) {
		buff_l[i] = buf_interlaced[i * channels + 0];
		buff_r[i] = buf_interlaced[i * channels + 1];
	}
}

void LinuxAudioStream::get_sample_rate() {
	return sample_rate;
}
