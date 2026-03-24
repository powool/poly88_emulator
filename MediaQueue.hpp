#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

struct MediaEntry {
	std::filesystem::path path;
	bool ready = false;
	std::uintmax_t size = 0;
};

class MediaQueue {
	std::mutex mutex;
	std::vector<MediaEntry> entries;
	static constexpr const char *MEDIA_LIST_FILE = ".poly88_media_files";
	std::atomic<bool> mediaWanted = false;
	int currentMediaIndex = -1;

public:
	MediaQueue() {
		LoadFromFile();
	}

	void MediaRequest() {
		mediaWanted = true;
	}

	bool MediaWanted() {
		// once we've notified the UI, just reset our flag
		bool result = mediaWanted;
		mediaWanted = false;
		return result;
	}

	void LoadFromFile() {
		std::lock_guard<std::mutex> lock(mutex);

		entries.clear();
		std::ifstream ifs(MEDIA_LIST_FILE);
		if (!ifs.is_open())
			return;
		std::string line;
		while (std::getline(ifs, line)) {
			if (line.empty())
				continue;
			MediaEntry e;
			e.path = line;
			std::error_code ec;
			auto sz = std::filesystem::file_size(e.path, ec);
			if (!ec) {
				e.size = sz;
				e.ready = true;
			} else {
				e.size = 0;
				e.ready = false;
			}
			entries.push_back(std::move(e));
		}

		currentMediaIndex = entries.size() ? 0 : -1;
	}

	bool SaveToFile() {
		std::lock_guard<std::mutex> lock(mutex);
		std::ofstream ofs(MEDIA_LIST_FILE);
		if (!ofs.is_open())
			return false;
		for (auto &e : entries)
			ofs << e.path.string() << "\n";
		return ofs.good();
	}

	static const char *GetMediaListFilePath() { return MEDIA_LIST_FILE; }

	size_t Count() const { return entries.size(); }

	const MediaEntry& At(size_t index) const { return entries.at(index); }
	MediaEntry& At(size_t index) { return entries.at(index); }

	void Add(const std::filesystem::path &p) {
		std::lock_guard<std::mutex> lock(mutex);

		MediaEntry e;
		e.path = p;
		std::error_code ec;
		auto sz = std::filesystem::file_size(p, ec);
		if (!ec) {
			e.size = sz;
			e.ready = true;
		} else {
			e.size = 0;
			e.ready = false;
		}
		entries.push_back(std::move(e));

		if (entries.size() == 1) {
			currentMediaIndex = 0;
		}
	}

	void Insert(size_t index, const std::filesystem::path &p) {
		std::lock_guard<std::mutex> lock(mutex);

		MediaEntry e;
		e.path = p;
		std::error_code ec;
		auto sz = std::filesystem::file_size(p, ec);
		if (!ec) {
			e.size = sz;
			e.ready = true;
		} else {
			e.size = 0;
			e.ready = false;
		}
		if (index > entries.size()) index = entries.size();
		entries.insert(entries.begin() + index, std::move(e));
		if (entries.size() == 1) {
			currentMediaIndex = 0;
		} else {
			if (currentMediaIndex >= index) {
				currentMediaIndex += 1;
			}
		}
	}

	void Remove(size_t index) {
		std::lock_guard<std::mutex> lock(mutex);

		if (index < entries.size()) {
			entries.erase(entries.begin() + index);
		}
		if(entries.size() == 0) {
			currentMediaIndex = -1;
		} else {
			if (currentMediaIndex >= index) {
				currentMediaIndex -= 1;
			}
		}
	}

	void Move(size_t fromIndex, size_t toIndex) {
		std::lock_guard<std::mutex> lock(mutex);

		if (fromIndex >= entries.size() || toIndex >= entries.size())
			return;
		if (fromIndex == toIndex)
			return;
		MediaEntry tmp = std::move(entries[fromIndex]);
		entries.erase(entries.begin() + fromIndex);

		if (toIndex > entries.size()) toIndex = entries.size();
		entries.insert(entries.begin() + toIndex, std::move(tmp));

		if (currentMediaIndex == fromIndex) {
			currentMediaIndex = toIndex;
		}
	}

	void Replace(size_t index, const std::filesystem::path &p) {
		std::lock_guard<std::mutex> lock(mutex);

		if (index >= entries.size())
			return;
		MediaEntry e;
		e.path = p;
		std::error_code ec;
		auto sz = std::filesystem::file_size(p, ec);
		if (!ec) {
			e.size = sz;
			e.ready = true;
		} else {
			e.size = 0;
			e.ready = false;
		}
		entries[index] = std::move(e);
	}

	void Clear() {
		std::lock_guard<std::mutex> lock(mutex);

		entries.clear();
		currentMediaIndex = -1;
	}

	const std::vector<MediaEntry>& Entries() const { return entries; }

	std::string GetNextMediaPath() {
		std::lock_guard<std::mutex> lock(mutex);
		if (currentMediaIndex == -1 || currentMediaIndex >= entries.size()) {
			currentMediaIndex = -1;
			return "";
		}

		std::string result = entries[currentMediaIndex].path;
		entries[currentMediaIndex++].ready = false;
		if (currentMediaIndex >= entries.size()) {
			currentMediaIndex = 0;
		}
		return result;
	}
};
