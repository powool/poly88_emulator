#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <atomic>

class FileDialogBridge {
	std::mutex mutex;
	std::condition_variable cv;
	std::atomic<bool> requested = false;
	bool responded = false;
	std::string selectedPath;
	std::string dialogTitle;
	std::string lastDirectory = ".";
	bool saveMode = false;

public:
	// Called from emulator thread: blocks until the UI thread provides a path (open dialog).
	std::string RequestFile(const std::string &title = "Open Tape File") {
		{
			std::lock_guard lock(mutex);
			dialogTitle = title;
			selectedPath.clear();
			responded = false;
			saveMode = false;
		}
		requested = true;

		std::unique_lock lock(mutex);
		cv.wait(lock, [this] { return responded; });
		return selectedPath;
	}

	// Called from emulator thread: blocks until the UI thread provides a path (save dialog).
	std::string RequestSaveFile(const std::string &title = "Save Tape File") {
		{
			std::lock_guard lock(mutex);
			dialogTitle = title;
			selectedPath.clear();
			responded = false;
			saveMode = true;
		}
		requested = true;

		std::unique_lock lock(mutex);
		cv.wait(lock, [this] { return responded; });
		return selectedPath;
	}

	// Called from UI thread: checks if the emulator wants a file dialog.
	bool IsRequested() const {
		return requested;
	}

	// Called from UI thread: gets the title for the dialog.
	std::string GetTitle() {
		std::lock_guard lock(mutex);
		return dialogTitle;
	}

	// Called from UI thread: true if the emulator requested a save dialog.
	bool IsSaveMode() {
		std::lock_guard lock(mutex);
		return saveMode;
	}

	// Called from UI thread: gets the starting directory for the dialog.
	std::string GetLastDirectory() {
		std::lock_guard lock(mutex);
		return lastDirectory;
	}

	// Called from UI thread: provides the result and unblocks the emulator thread.
	void Respond(const std::string &path) {
		{
			std::lock_guard lock(mutex);
			selectedPath = path;
			if (!path.empty()) {
				auto pos = path.find_last_of('/');
				if (pos != std::string::npos)
					lastDirectory = path.substr(0, pos);
			}
			responded = true;
			requested = false;
		}
		cv.notify_one();
	}
};
