#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <cstdint>

struct MediaEntry {
	std::filesystem::path path;
	bool ready = false;
	std::uintmax_t size = 0;
};

class MediaQueue {
	std::vector<MediaEntry> entries;
	static constexpr const char *MEDIA_LIST_FILE = ".poly88_media_files";

public:
	MediaQueue() {
		LoadFromFile();
	}

	void LoadFromFile() {
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
	}

	bool SaveToFile() const {
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
	}

	void Insert(size_t index, const std::filesystem::path &p) {
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
	}

	void Remove(size_t index) {
		if (index < entries.size())
			entries.erase(entries.begin() + index);
	}

	void Move(size_t fromIndex, size_t toIndex) {
		if (fromIndex >= entries.size() || toIndex >= entries.size())
			return;
		if (fromIndex == toIndex)
			return;
		MediaEntry tmp = std::move(entries[fromIndex]);
		entries.erase(entries.begin() + fromIndex);
		if (toIndex > entries.size()) toIndex = entries.size();
		entries.insert(entries.begin() + toIndex, std::move(tmp));
	}

	void Replace(size_t index, const std::filesystem::path &p) {
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

	void Clear() { entries.clear(); }

	const std::vector<MediaEntry>& Entries() const { return entries; }
};
