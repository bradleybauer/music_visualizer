#include <iostream>
using std::cout; using std::endl;

#include "LinuxAudioStream.h"

LinuxAudioStream::LinuxAudioStream() {
	std::string sink_name;
	getPulseDefaultSink((void*)&sink_name);
	sink_name += ".monitor";

	buf_interlaced = new float[sample_rate * channels];

	pa_sample_spec pulseSampleSpec;
	pulseSampleSpec.channels = channels;
	pulseSampleSpec.rate = sample_rate;
	pulseSampleSpec.format = PA_SAMPLE_FLOAT32NE;
	pa_buffer_attr pb;
	pb.fragsize = max_buff_size*channels*sizeof(float) / 2;
	pb.maxlength = max_buff_size*channels*sizeof(float);
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
	delete[] buf_interlaced;
}

void LinuxAudioStream::get_next_pcm(float * buff_l, float * buff_r, int size) {
	if (max_buff_size < size)
		cout << "get_next_pcm called with size > max_buff_size" << endl;
	if (pa_simple_read(pulseState, buf_interlaced, size*channels*sizeof(float), &pulseError) < 0) {
		cout << "pa_simple_read() failed: " << pa_strerror(pulseError) << endl;
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < size; i++) {
		buff_l[i] = buf_interlaced[i * channels + 0];
		buff_r[i] = buf_interlaced[i * channels + 1];
	}
}

int LinuxAudioStream::get_sample_rate() {
	return sample_rate;
}

int LinuxAudioStream::get_max_buff_size() {
	return max_buff_size;
}
