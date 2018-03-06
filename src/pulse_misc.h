#pragma once

#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

// pulseaudio code from github: karlstav/cava

static pa_mainloop* m_pulseaudio_mainloop;
static void cb(pa_context* pulseaudio_context, const pa_server_info* i, void* data) {
	std::string* sink_name = (std::string*) data;
	*sink_name = std::string(i->default_sink_name);
	pa_mainloop_quit(m_pulseaudio_mainloop, 0);
}
static void pulseaudio_context_state_callback(pa_context* pulseaudio_context, void* data) {
	switch (pa_context_get_state(pulseaudio_context)) {
	case PA_CONTEXT_UNCONNECTED:
		// std::cout << "UNCONNECTED" << std::endl;
		break;
	case PA_CONTEXT_CONNECTING:
		// std::cout << "CONNECTING" << std::endl;
		break;
	case PA_CONTEXT_AUTHORIZING:
		// std::cout << "AUTHORIZING" << std::endl;
		break;
	case PA_CONTEXT_SETTING_NAME:
		// std::cout << "SETTING_NAME" << std::endl;
		break;
	case PA_CONTEXT_READY: // extract default sink name
		// std::cout << "READY" << std::endl;
		pa_operation_unref(pa_context_get_server_info(pulseaudio_context, cb, data));
		break;
	case PA_CONTEXT_FAILED:
		// std::cout << "FAILED" << std::endl;
		break;
	case PA_CONTEXT_TERMINATED:
		// std::cout << "TERMINATED" << std::endl;
		pa_mainloop_quit(m_pulseaudio_mainloop, 0);
		break;
	}
}
static void getPulseDefaultSink(void* data) {
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
		std::cout << "Could not open pulseaudio mainloop to find default device name: %d" << ret << std::endl;
	}
}
