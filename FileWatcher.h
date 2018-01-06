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
	// So process the event and set the last process time.
	// If new event is within 200 ms of last process time, then ignore it.
	//
	// If shader.json or any frag or geom file has changed, then set shaders_changed.
	void handleFileAction(FW::WatchID watchid, const FW::String& dir, const FW::String& filename_str, FW::Action action)
	{
		if (FW::Action::Delete == action)
			return;

		filesys::path filename = filesys::path(filename_str);
		std::string extension = filename.extension().string();
		std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
		float time_elapsed = (now - last_event_time).count() / 1e6;
		if (time_elapsed > 200.f)
			if (dir == "shaders")
				if (extension == ".json" || extension == ".geom" || extension == ".frag") {
					shaders_changed = true;
					last_event_time = now;
				}
	}
	bool shaders_changed;

private:
	std::chrono::steady_clock::time_point last_event_time;

	filesys::path shader_folder;
	FW::AsyncFileWatcher file_watcher;
};
