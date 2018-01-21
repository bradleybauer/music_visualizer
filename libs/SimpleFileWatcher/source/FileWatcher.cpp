/**
	Copyright (c) 2009 James Wynn (james@jameswynn.com)

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/

#include <FileWatcher/FileWatcher.h>
#include <FileWatcher/FileWatcherImpl.h>

#if FILEWATCHER_PLATFORM == FILEWATCHER_PLATFORM_WIN32
#	include <FileWatcher/FileWatcherWin32.h>
#	define FILEWATCHER_IMPL FileWatcherWin32
#elif FILEWATCHER_PLATFORM == FILEWATCHER_PLATFORM_KQUEUE
#	include <FileWatcher/FileWatcherOSX.h>
#	define FILEWATCHER_IMPL FileWatcherOSX
#elif FILEWATCHER_PLATFORM == FILEWATCHER_PLATFORM_LINUX
#	include <FileWatcher/FileWatcherLinux.h>
#	define FILEWATCHER_IMPL FileWatcherLinux
#endif

namespace FW
{

	//--------
	FileWatcher::FileWatcher()
	{
		mImpl = new FILEWATCHER_IMPL();
	}

	//--------
	FileWatcher::~FileWatcher()
	{
		delete mImpl;
		mImpl = 0;
	}

	//--------
	WatchID FileWatcher::addWatch(const String& directory, FileWatchListener* watcher)
	{
		return mImpl->addWatch(directory, watcher, false);
	}

	//--------
	WatchID FileWatcher::addWatch(const String& directory, FileWatchListener* watcher, bool recursive)
	{
		return mImpl->addWatch(directory, watcher, recursive);
	}

	//--------
	void FileWatcher::removeWatch(const String& directory)
	{
		mImpl->removeWatch(directory);
	}

	//--------
	void FileWatcher::removeWatch(WatchID watchid)
	{
		mImpl->removeWatch(watchid);
	}

	//--------
	void FileWatcher::update()
	{
		mImpl->update();
	}

	void async_filewatcher_thread(AsyncFileWatcher* arg)
	{
		AsyncFileWatcher& watcher_handle = *arg;
		BufferedFileWatcher& watcher = watcher_handle.m_watch;

		while (watcher_handle.m_running)
		{
			watcher.update();

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}

	BufferedFileWatcher::BufferedFileWatcher()
	{
	}

	BufferedFileWatcher::~BufferedFileWatcher()
	{
	}

	void BufferedFileWatcher::addWatch(const String & directory, FileWatchListener * watcher, WatchID* target)
	{
		addWatch(directory, watcher, false, target);
	}

	void BufferedFileWatcher::addWatch(const String & directory, FileWatchListener * watcher, bool recursive, WatchID* target)
	{
		command_struct str;
		str.Type = AddWatch;
		str.path = directory;
		str.Add.watcher = watcher;
		str.Add.recursive = recursive;
		str.Add.target = target;

		std::lock_guard<std::mutex> lock(m_mutex);
		m_commands.push(str);
	}

	void BufferedFileWatcher::removeWatch(const String & directory)
	{
		command_struct str;
		str.Type = RemoveWatchStr;
		str.path = directory;
		
		std::lock_guard<std::mutex> lock(m_mutex);
		m_commands.push(str);
	}

	void BufferedFileWatcher::removeWatch(WatchID watchid)
	{
		command_struct str;
		str.Type = RemoveWatchID;
		str.RemoveID.id = watchid;
		
		std::lock_guard<std::mutex> lock(m_mutex);
		m_commands.push(str);
	}

	void BufferedFileWatcher::update()
	{
		if (!m_commands.empty())
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			while (!m_commands.empty())
			{
				auto cmd = m_commands.front();
				m_commands.pop();

				switch (cmd.Type)
				{
				case AddWatch:
				{
					auto ret = m_watcher.addWatch(cmd.path, cmd.Add.watcher, cmd.Add.recursive);
					if (cmd.Add.target != NULL)
						*cmd.Add.target = ret;
					break;
				}
				case RemoveWatchID:
					m_watcher.removeWatch(cmd.RemoveID.id);
					break;
				case RemoveWatchStr:
					m_watcher.removeWatch(cmd.path);
					break;
				}
			}
		}

		m_watcher.update();
	}

	AsyncFileWatcher::AsyncFileWatcher() : m_running(true)
	{
		m_thr = std::thread(async_filewatcher_thread, this);
	}

	AsyncFileWatcher::~AsyncFileWatcher()
	{
		m_running = false;
		m_thr.join();
	}

	void AsyncFileWatcher::addWatch(const String & directory, FileWatchListener * watcher, WatchID * target)
	{
		m_watch.addWatch(directory, watcher, target);
	}

	void AsyncFileWatcher::addWatch(const String & directory, FileWatchListener * watcher, bool recursive, WatchID * target)
	{
		m_watch.addWatch(directory, watcher, recursive, target);
	}

	void AsyncFileWatcher::removeWatch(const String & directory)
	{
		m_watch.removeWatch(directory);
	}

	void AsyncFileWatcher::removeWatch(WatchID watchid)
	{
		m_watch.removeWatch(watchid);
	}

	void AsyncFileWatcher::update()
	{
		// no-op, handled by our thread
	}

};//namespace FW
