#pragma once
#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

// Setup pulse audio connection.
// code from github: karlstav/cava
// thanks !

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

