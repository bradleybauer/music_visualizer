#pragma once

#include <string>
#include <chrono>

#include "filesystem.h"
namespace filesys = std::experimental::filesystem;

#include "FileWatcher/FileWatcher.h"

class FileWatcher : FW::FileWatchListener {
public:
	FileWatcher(filesys::path shader_folder) : shader_folder(shader_folder), shaders_changed(false), last_event_time()
	{
		file_watcher.addWatch(shader_folder.string(), (FW::FileWatchListener*)this, false);
	}

	~FileWatcher() {
		file_watcher.removeWatch(shader_folder.string());
	}

	// More than one event can be delivered by the editor from a single save command.
	// So sleep a few millis and then process the event and set the last process time.
	// If new event is within 100 ms of last process time, then ignore it.
	// If shader.json or any frag or geom file has changed, then set shaders_changed.
	void handleFileAction(FW::WatchID watchid, const FW::String& dir, const FW::String& filename_str, FW::Action action)
	{
		if (FW::Action::Delete == action)
			return;
		if (dir != "shaders")
			return;
		std::string extension = filesys::path(filename_str).extension().string();
		if (extension != ".json" && extension != ".geom" && extension != ".frag")
			return;
		if (std::chrono::steady_clock::now() - last_event_time < std::chrono::milliseconds(100))
			return;

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		shaders_changed = true;
		last_event_time = std::chrono::steady_clock::now();
	}
	bool shaders_changed;

private:
	std::chrono::steady_clock::time_point last_event_time;

	filesys::path shader_folder;
	FW::AsyncFileWatcher file_watcher;
};
