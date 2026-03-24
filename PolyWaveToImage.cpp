#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <QApplication>
#include <QMainWindow>

#include "audio.h"

// Wave file handler
// bit finder
// E6 finder
// record finder/verifier
// record spans a set of wave file indeces, allow interfactive editing or auto correction

// TapeIndex is a double to allow for the fact that bits
// won't always start at an integral sample index.
using TapeIndex = double;

struct TapeByte {
	// WAV file index and length, in units of samples.
	TapeIndex startIndex, length;
	// no value means exactly that - it is unknown
	std::optional<uint8_t> value;
	// override == true -> user or system overrode a value as
	// a placeholder. Note: we might want to automatically consider
	// them to be 0xE6, like the header.
	bool override;
};

class Record {
	// defined in Poly_88_Operation_Software.pdf page 85
	std::vector<TapeByte> leader;
	TapeByte soh;
	std::array<TapeByte, 8> name;
	TapeByte rcdL;
	TapeByte rcdH;
	TapeByte ln;
	TapeByte addrL;
	TapeByte addrH;
	TapeByte type;
	TapeByte csHeader;

	std::vector<TapeByte> data;
	TapeByte csData;

	enum TapeType {
		AbsoluteBinary = 0x00,
		Comment = 0x01,
		End = 0x02,
		AutoExecute = 0x03,
		Data = 0x04
	};

    public:
	TapeIndex GetStartIndex() {
		if (leader.size() && leader[0].value) {
			return leader[0].startIndex;
		}
		return 0;
	}

	bool RecordIsValid() {
		if (leader.size() == 0) return false;
		if (!soh.value) return false;
		if (*(soh.value) != 0x01) return false;
		if (!HeaderChecksumIsValid()) {
			return false;
		}
		if (!type.value) return false;
		switch(*(type.value)) {
			case AbsoluteBinary:
			case Data:
				return DataChecksumIsValid();
			case Comment:
			case End:
			case AutoExecute:
				return true;
			default:
				return false;
		}
		// not reached:
		return true;
	}

	std::string GetName() {
		if (!NameIsValid()) return "";
		std::string result;
		for (int i = 0; i < 8; i++)
			result += *(name[i].value);
		return result;
	}

	bool NameIsValid() {
		for (int i = 0; i < 8; i++)
			if (!name[i].value) return false;
		return true;
	}

	bool HeaderChecksumIsValid() {
		if (!NameIsValid()) return false;
		if (!rcdL.value) return false;
		if (!rcdL.value) return false;
		if (!ln.value) return false;
		if (!addrL.value) return false;
		if (!addrH.value) return false;
		if (!type.value) return false;
		if (!csHeader.value) return false;

		uint8_t sum = 0;
		for (int i = 0; i < 8; i++)
			sum += *(name[i].value);
		sum += *(rcdL.value);
		sum += *(rcdL.value);
		sum += *(ln.value);
		sum += *(addrL.value);
		sum += *(addrH.value);
		sum += *(type.value);
		sum += *(csHeader.value);
		return sum == 0;
	}

	bool DataChecksumIsValid() {
		for (int i = 0; i < data.size(); i++)
			if (!data[i].value) return false;;
		uint8_t sum = 0;
		for (int i = 0; i < data.size(); i++)
			sum += *(data[i].value);
		sum += *(csData.value);
		return sum == 0;
	}
};

class File {
	std::vector<Record> records;
    public:
	TapeIndex GetStartIndex() {
		if (records.size()) {
			return records[0].GetStartIndex();
		}
		return 0;
	}
};

class Tape {
	std::vector<File> tapeFiles;
};

class MainWindow : public QMainWindow
{
	AudioPtr	audioPtr;
};

int main(int argc, char* argv[])
{
	// create the Qt application
	QApplication qtApplication(argc, argv);
	auto win = std::make_unique<MainWindow>();
	win->show();

	uint64_t cycle = 0;
	while (!win->Closed()) {
		qtApplication.processEvents(QEventLoop::ProcessEventsFlag::AllEvents);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	qtApplication.quit();
	return 0;
}

