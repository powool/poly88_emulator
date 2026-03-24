#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

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

	TapeIndex GetEndIndex() {
		if (data.size() && csData.length > 0) {
			return csData.startIndex + csData.length;
		}
		if (csHeader.length > 0) {
			return csHeader.startIndex + csHeader.length;
		}
		return GetStartIndex();
	}

	uint16_t GetRecordNumber() {
		if (rcdL.value && rcdH.value)
			return static_cast<uint16_t>(*(rcdL.value)) |
			       (static_cast<uint16_t>(*(rcdH.value)) << 8);
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

	// Return all TapeBytes in order for tick-mark rendering
	std::vector<const TapeByte *> GetAllBytes() const {
		std::vector<const TapeByte *> result;
		for (auto &b : leader) result.push_back(&b);
		result.push_back(&soh);
		for (auto &b : name) result.push_back(&b);
		result.push_back(&rcdL);
		result.push_back(&rcdH);
		result.push_back(&ln);
		result.push_back(&addrL);
		result.push_back(&addrH);
		result.push_back(&type);
		result.push_back(&csHeader);
		for (auto &b : data) result.push_back(&b);
		result.push_back(&csData);
		return result;
	}

	// Check if a sample index falls within this record
	bool ContainsIndex(TapeIndex idx) {
		return idx >= GetStartIndex() && idx < GetEndIndex();
	}

	// Find which TapeByte (field name) a sample index corresponds to
	std::string FieldNameAtIndex(TapeIndex idx) {
		for (auto &b : leader)
			if (idx >= b.startIndex && idx < b.startIndex + b.length) return "leader";
		if (soh.length > 0 && idx >= soh.startIndex && idx < soh.startIndex + soh.length) return "soh";
		for (int i = 0; i < 8; i++)
			if (name[i].length > 0 && idx >= name[i].startIndex && idx < name[i].startIndex + name[i].length)
				return std::string("name[") + std::to_string(i) + "]";
		if (rcdL.length > 0 && idx >= rcdL.startIndex && idx < rcdL.startIndex + rcdL.length) return "rcdL";
		if (rcdH.length > 0 && idx >= rcdH.startIndex && idx < rcdH.startIndex + rcdH.length) return "rcdH";
		if (ln.length > 0 && idx >= ln.startIndex && idx < ln.startIndex + ln.length) return "ln";
		if (addrL.length > 0 && idx >= addrL.startIndex && idx < addrL.startIndex + addrL.length) return "addrL";
		if (addrH.length > 0 && idx >= addrH.startIndex && idx < addrH.startIndex + addrH.length) return "addrH";
		if (type.length > 0 && idx >= type.startIndex && idx < type.startIndex + type.length) return "type";
		if (csHeader.length > 0 && idx >= csHeader.startIndex && idx < csHeader.startIndex + csHeader.length) return "csHeader";
		for (size_t i = 0; i < data.size(); i++)
			if (data[i].length > 0 && idx >= data[i].startIndex && idx < data[i].startIndex + data[i].length)
				return std::string("data[") + std::to_string(i) + "]";
		if (csData.length > 0 && idx >= csData.startIndex && idx < csData.startIndex + csData.length) return "csData";
		return "";
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

	TapeIndex GetEndIndex() {
		if (records.size()) {
			return records.back().GetEndIndex();
		}
		return 0;
	}

	std::vector<Record> &GetRecords() { return records; }
	const std::vector<Record> &GetRecords() const { return records; }

	bool ContainsIndex(TapeIndex idx) {
		return idx >= GetStartIndex() && idx < GetEndIndex();
	}
};

class Tape {
	std::vector<File> tapeFiles;
    public:
	std::vector<File> &GetFiles() { return tapeFiles; }
	const std::vector<File> &GetFiles() const { return tapeFiles; }
};

// ---------------------------------------------------------------------------
// MainWindowSettings - holds user-configurable settings
// ---------------------------------------------------------------------------
struct MainWindowSettings {
	bool booleanPlaceholder = false;
};

// ---------------------------------------------------------------------------
// SettingsDialog - modal dialog bound to a MainWindowSettings object
// ---------------------------------------------------------------------------
class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	SettingsDialog(MainWindowSettings &settings, QWidget *parent = nullptr)
		: QDialog(parent), settings_(settings)
	{
		setWindowTitle("Settings");
		auto *layout = new QFormLayout(this);

		booleanCheckBox_ = new QCheckBox(this);
		booleanCheckBox_->setChecked(settings_.booleanPlaceholder);
		layout->addRow("Boolean", booleanCheckBox_);

		auto *buttons = new QDialogButtonBox(
			QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
		layout->addRow(buttons);

		connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
	}

	void accept() override {
		settings_.booleanPlaceholder = booleanCheckBox_->isChecked();
		QDialog::accept();
	}

private:
	MainWindowSettings &settings_;
	QCheckBox *booleanCheckBox_;
};

// ---------------------------------------------------------------------------
// WaveformView - custom widget for rendering audio waveform
// ---------------------------------------------------------------------------
class WaveformView : public QWidget {
	Q_OBJECT
public:
	WaveformView(QWidget *parent = nullptr)
		: QWidget(parent)
	{
		setMouseTracking(true);
		setFocusPolicy(Qt::StrongFocus);
		setContextMenuPolicy(Qt::CustomContextMenu);

		connect(this, &QWidget::customContextMenuRequested,
			this, &WaveformView::showContextMenu);
	}

	void setAudio(AudioPtr audio) {
		audio_ = audio;
		scrollOffset_ = 0;
		update();
		emit scrollChanged();
	}

	void setTape(Tape *tape) {
		tape_ = tape;
		update();
	}

	AudioPtr getAudio() const { return audio_; }

	// Convert a widget-local X pixel to a sample index
	double pixelToSample(int px) const {
		return scrollOffset_ + static_cast<double>(px) / xScale_;
	}

	// Convert a sample index to a widget-local X pixel
	double sampleToPixel(double sampleIdx) const {
		return (sampleIdx - scrollOffset_) * xScale_;
	}

	double getScrollOffset() const { return scrollOffset_; }
	void setScrollOffset(double offset) {
		if (!audio_) return;
		double maxOff = std::max(0.0, static_cast<double>(audio_->SampleCount()) -
			width() / xScale_);
		scrollOffset_ = std::clamp(offset, 0.0, maxOff);
		update();
		emit scrollChanged();
	}

	// Number of samples visible in the viewport
	double visibleSamples() const {
		return width() / xScale_;
	}

	double totalSamples() const {
		if (!audio_) return 1.0;
		return static_cast<double>(audio_->SampleCount());
	}

	double getXScale() const { return xScale_; }
	double getYScale() const { return yScale_; }

signals:
	void mouseSampleChanged(double sampleIndex);
	void scrollChanged();

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.fillRect(rect(), Qt::black);

		if (!audio_ || audio_->SampleCount() == 0) {
			p.setPen(Qt::gray);
			p.drawText(rect(), Qt::AlignCenter, "No audio loaded");
			return;
		}

		int w = width();
		int h = height();
		int midY = h / 2;

		// Draw zero line
		p.setPen(QColor(60, 60, 60));
		p.drawLine(0, midY, w, midY);

		// Draw waveform
		p.setPen(QColor(0, 200, 0));

		int prevScreenY = midY;
		for (int px = 0; px < w; px++) {
			double sampleIdx = pixelToSample(px);
			int idx = static_cast<int>(sampleIdx);
			if (idx < 0 || idx >= audio_->SampleCount()) continue;

			double val = audio_->Value(idx);
			int screenY = midY - static_cast<int>(val * yScale_ * midY / 32768.0);
			screenY = std::clamp(screenY, 0, h - 1);

			if (px > 0) {
				p.drawLine(px - 1, prevScreenY, px, screenY);
			}
			prevScreenY = screenY;
		}

		// Draw TapeByte tick marks on the X axis
		drawByteTickMarks(p, w, h);
	}

	void drawByteTickMarks(QPainter &p, int w, int h) {
		if (!tape_) return;

		p.setPen(QColor(100, 100, 255));
		int tickTop = h - 20;
		int tickBot = h - 5;

		for (auto &file : tape_->GetFiles()) {
			for (auto &record : file.GetRecords()) {
				auto allBytes = record.GetAllBytes();
				for (auto *tb : allBytes) {
					if (tb->length <= 0) continue;
					double px = sampleToPixel(tb->startIndex);
					if (px < -1 || px > w + 1) continue;
					int ipx = static_cast<int>(px);
					p.drawLine(ipx, tickTop, ipx, tickBot);
				}
			}
		}
	}

	void mouseMoveEvent(QMouseEvent *event) override {
		if (dragging_) {
			double dx = event->position().x() - dragLastX_;
			setScrollOffset(scrollOffset_ - dx / xScale_);
			dragLastX_ = event->position().x();
		}

		double sampleIdx = pixelToSample(static_cast<int>(event->position().x()));
		emit mouseSampleChanged(sampleIdx);

		QWidget::mouseMoveEvent(event);
	}

	void mousePressEvent(QMouseEvent *event) override {
		if (event->button() == Qt::LeftButton) {
			dragging_ = true;
			dragLastX_ = event->position().x();
			setCursor(Qt::ClosedHandCursor);
		}
		QWidget::mousePressEvent(event);
	}

	void mouseReleaseEvent(QMouseEvent *event) override {
		if (event->button() == Qt::LeftButton) {
			dragging_ = false;
			setCursor(Qt::ArrowCursor);
		}
		QWidget::mouseReleaseEvent(event);
	}

	void wheelEvent(QWheelEvent *event) override {
		double degrees = event->angleDelta().y() / 8.0;
		double steps = degrees / 15.0;

		if (event->modifiers() & Qt::ControlModifier) {
			// Vertical scale: linear with mouse wheel
			yScale_ = std::clamp(yScale_ + steps * 0.1, 0.1, 50.0);
		} else {
			// Horizontal scale: exponential / proportional
			// Larger wheel movement -> exponentially larger scale change
			double factor = std::pow(1.2, steps);
			double mouseX = event->position().x();
			double sampleAtMouse = pixelToSample(static_cast<int>(mouseX));

			xScale_ = std::clamp(xScale_ * factor, 0.0001, 200.0);

			// Keep the sample under the mouse cursor stationary
			scrollOffset_ = sampleAtMouse - mouseX / xScale_;
			double maxOff = std::max(0.0, totalSamples() - visibleSamples());
			scrollOffset_ = std::clamp(scrollOffset_, 0.0, maxOff);
		}
		update();
		emit scrollChanged();
		event->accept();
	}

private slots:
	void showContextMenu(const QPoint &pos) {
		QMenu contextMenu(this);

		QAction *dataAction = contextMenu.addAction("Data");
		QAction *waveformAction = contextMenu.addAction("Waveform");

		connect(dataAction, &QAction::triggered, this, &WaveformView::onContextData);
		connect(waveformAction, &QAction::triggered, this, &WaveformView::onContextWaveform);

		contextMenu.exec(mapToGlobal(pos));
	}

	void onContextData() {
		// placeholder - will be implemented later
	}

	void onContextWaveform() {
		// placeholder - will be implemented later
	}

private:
	AudioPtr audio_;
	Tape *tape_ = nullptr;

	double xScale_ = 0.05;   // pixels per sample (start zoomed out)
	double yScale_ = 1.0;
	double scrollOffset_ = 0; // first sample visible at left edge

	bool dragging_ = false;
	double dragLastX_ = 0;
};

// ---------------------------------------------------------------------------
// MainWindow
// ---------------------------------------------------------------------------
class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
		setWindowTitle("PolyWaveToImage");
		resize(1200, 800);

		buildMenus();
		buildUI();

		statusBar()->showMessage("Ready");
	}

	AudioPtr audioPtr;

private:
	MainWindowSettings settings_;
	Tape tape_;

	// UI components
	WaveformView *waveformView_ = nullptr;
	QScrollBar *hScrollBar_ = nullptr;
	QLabel *tapeFileLabel_ = nullptr;
	QLabel *recordNumberLabel_ = nullptr;
	QLabel *byteLabel_ = nullptr;
	QLabel *validityLabel_ = nullptr;
	QWidget *bottomPlaceholder_ = nullptr;

	bool updatingScrollBar_ = false;

	void buildMenus() {
		// --- File menu ---
		QMenu *fileMenu = menuBar()->addMenu("&File");

		QAction *loadAction = fileMenu->addAction("&Load");
		connect(loadAction, &QAction::triggered, this, &MainWindow::onLoad);

		QAction *saveAction = fileMenu->addAction("&Save");
		connect(saveAction, &QAction::triggered, this, &MainWindow::onSave);

		fileMenu->addSeparator();

		QAction *settingsAction = fileMenu->addAction("Se&ttings");
		connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettings);

		fileMenu->addSeparator();

		QAction *quitAction = fileMenu->addAction("&Quit");
		connect(quitAction, &QAction::triggered, this, &MainWindow::onQuit);

		// --- Help menu ---
		QMenu *helpMenu = menuBar()->addMenu("&Help");

		QAction *quickHelpAction = helpMenu->addAction("Quick &Help");
		connect(quickHelpAction, &QAction::triggered, this, &MainWindow::onQuickHelp);

		QAction *documentationAction = helpMenu->addAction("&Documentation");
		connect(documentationAction, &QAction::triggered, this, &MainWindow::onDocumentation);

		helpMenu->addSeparator();

		QAction *aboutAction = helpMenu->addAction("&About");
		connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
	}

	void buildUI() {
		auto *centralWidget = new QWidget(this);
		auto *mainLayout = new QVBoxLayout(centralWidget);
		mainLayout->setContentsMargins(0, 0, 0, 0);
		mainLayout->setSpacing(0);

		auto *splitter = new QSplitter(Qt::Vertical, centralWidget);

		// --- Top pane: waveform + scrollbar ---
		auto *topWidget = new QWidget(splitter);
		auto *topLayout = new QVBoxLayout(topWidget);
		topLayout->setContentsMargins(0, 0, 0, 0);
		topLayout->setSpacing(0);

		waveformView_ = new WaveformView(topWidget);
		waveformView_->setMinimumHeight(200);
		topLayout->addWidget(waveformView_, 1);

		hScrollBar_ = new QScrollBar(Qt::Horizontal, topWidget);
		topLayout->addWidget(hScrollBar_);

		connect(waveformView_, &WaveformView::scrollChanged,
			this, &MainWindow::syncScrollBar);
		connect(hScrollBar_, &QScrollBar::valueChanged,
			this, &MainWindow::onScrollBarChanged);
		connect(waveformView_, &WaveformView::mouseSampleChanged,
			this, &MainWindow::onMouseSampleChanged);

		// --- Middle pane: status labels ---
		auto *middleWidget = new QWidget(splitter);
		auto *middleLayout = new QHBoxLayout(middleWidget);
		middleLayout->setContentsMargins(8, 4, 8, 4);

		auto makeStatusPair = [&](const QString &labelText) -> QLabel * {
			auto *nameLabel = new QLabel(labelText + ":", middleWidget);
			nameLabel->setStyleSheet("font-weight: bold;");
			auto *valueLabel = new QLabel("—", middleWidget);
			middleLayout->addWidget(nameLabel);
			middleLayout->addWidget(valueLabel);
			middleLayout->addSpacing(16);
			return valueLabel;
		};

		tapeFileLabel_ = makeStatusPair("Tape File");
		recordNumberLabel_ = makeStatusPair("Record Number");
		byteLabel_ = makeStatusPair("Byte");

		validityLabel_ = new QLabel("—", middleWidget);
		auto *validTitleLabel = new QLabel("Status:", middleWidget);
		validTitleLabel->setStyleSheet("font-weight: bold;");
		middleLayout->addWidget(validTitleLabel);
		middleLayout->addWidget(validityLabel_);

		middleLayout->addStretch();
		middleWidget->setMaximumHeight(40);

		// --- Bottom pane: placeholder ---
		bottomPlaceholder_ = new QWidget(splitter);
		auto *bottomLayout = new QVBoxLayout(bottomPlaceholder_);
		auto *placeholderLabel = new QLabel(
			"Tape files / records / byte layout will appear here",
			bottomPlaceholder_);
		placeholderLabel->setAlignment(Qt::AlignCenter);
		placeholderLabel->setStyleSheet("color: gray;");
		bottomLayout->addWidget(placeholderLabel);
		bottomPlaceholder_->setMinimumHeight(100);

		// Set splitter proportions
		splitter->addWidget(topWidget);
		splitter->addWidget(middleWidget);
		splitter->addWidget(bottomPlaceholder_);
		splitter->setStretchFactor(0, 5);
		splitter->setStretchFactor(1, 0);
		splitter->setStretchFactor(2, 2);

		mainLayout->addWidget(splitter);
		setCentralWidget(centralWidget);
	}

	// Sync the horizontal scrollbar to match the WaveformView state.
	// Uses an adaptive logarithmic mapping for large data sets.
	void syncScrollBar() {
		if (!waveformView_ || updatingScrollBar_) return;
		updatingScrollBar_ = true;

		double total = waveformView_->totalSamples();
		double visible = waveformView_->visibleSamples();
		double offset = waveformView_->getScrollOffset();

		// Use a logarithmic mapping: scrollbar position is proportional
		// to log(1 + offset) / log(1 + total).
		// This gives fine-grained control near the current position
		// and coarser jumps at the extremes.
		const int scrollRange = 100000;

		if (total <= visible) {
			hScrollBar_->setRange(0, 0);
			hScrollBar_->setValue(0);
		} else {
			double logTotal = std::log1p(total);
			int pos = static_cast<int>(std::log1p(offset) / logTotal * scrollRange);
			int pageStep = static_cast<int>(std::log1p(visible) / logTotal * scrollRange);
			pageStep = std::max(pageStep, 1);

			hScrollBar_->setRange(0, scrollRange);
			hScrollBar_->setPageStep(pageStep);
			hScrollBar_->setValue(pos);
		}
		updatingScrollBar_ = false;
	}

private slots:
	void onScrollBarChanged(int value) {
		if (updatingScrollBar_ || !waveformView_) return;
		updatingScrollBar_ = true;

		double total = waveformView_->totalSamples();
		const int scrollRange = 100000;

		// Inverse of log mapping: offset = exp(value / scrollRange * log(1+total)) - 1
		double logTotal = std::log1p(total);
		double offset = std::expm1(static_cast<double>(value) / scrollRange * logTotal);

		waveformView_->setScrollOffset(offset);
		updatingScrollBar_ = false;
	}

	void onMouseSampleChanged(double sampleIndex) {
		if (!audioPtr || sampleIndex < 0 ||
			sampleIndex >= audioPtr->SampleCount()) {
			tapeFileLabel_->setText("—");
			recordNumberLabel_->setText("—");
			byteLabel_->setText("—");
			validityLabel_->setText("—");
			validityLabel_->setStyleSheet("");
			return;
		}

		bool found = false;
		int fileIdx = 0;
		for (auto &file : tape_.GetFiles()) {
			int recIdx = 0;
			for (auto &record : file.GetRecords()) {
				if (record.ContainsIndex(sampleIndex)) {
					tapeFileLabel_->setText(
						QString::fromStdString(record.GetName()).trimmed());
					recordNumberLabel_->setText(
						QString::number(record.GetRecordNumber()));
					std::string fieldName = record.FieldNameAtIndex(sampleIndex);
					byteLabel_->setText(QString::fromStdString(fieldName));

					if (record.RecordIsValid()) {
						validityLabel_->setText("valid");
						validityLabel_->setStyleSheet(
							"color: green; font-weight: bold;");
					} else {
						validityLabel_->setText("invalid");
						validityLabel_->setStyleSheet(
							"color: red; font-weight: bold;");
					}
					found = true;
					break;
				}
				recIdx++;
			}
			if (found) break;
			fileIdx++;
		}

		if (!found) {
			tapeFileLabel_->setText("—");
			recordNumberLabel_->setText("—");
			byteLabel_->setText("—");
			validityLabel_->setText("—");
			validityLabel_->setStyleSheet("");
		}
	}

	void onLoad() {
		QString fileName = QFileDialog::getOpenFileName(
			this, "Open WAV File", QString(),
			"WAV files (*.wav);;All files (*)");
		if (fileName.isEmpty()) return;

		try {
			audioPtr = std::make_shared<Audio>(fileName.toStdString());
			waveformView_->setAudio(audioPtr);
			waveformView_->setTape(&tape_);
			statusBar()->showMessage(
				"Loaded: " + fileName +
				" (" + QString::number(audioPtr->SampleCount()) + " samples)");
		} catch (const std::exception &e) {
			QMessageBox::critical(this, "Error loading file",
				QString::fromStdString(e.what()));
		}
	}

	void onSave() {
		// placeholder - will be implemented later
		statusBar()->showMessage("Save not yet implemented");
	}

	void onSettings() {
		SettingsDialog dlg(settings_, this);
		dlg.exec();
	}

	void onQuit() {
		auto reply = QMessageBox::question(
			this, "Quit", "Are you sure you want to quit?",
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (reply == QMessageBox::Yes) {
			QApplication::quit();
		}
	}

	void onQuickHelp() {
		QDialog helpDlg(this);
		helpDlg.setWindowTitle("Quick Help");
		helpDlg.resize(500, 400);
		auto *layout = new QVBoxLayout(&helpDlg);
		auto *browser = new QTextBrowser(&helpDlg);
		browser->setMarkdown("insert help here");
		layout->addWidget(browser);
		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &helpDlg);
		layout->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::accepted, &helpDlg, &QDialog::accept);
		helpDlg.exec();
	}

	void onDocumentation() {
		QDesktopServices::openUrl(QUrl("http://help.me/fast"));
	}

	void onAbout() {
		QMessageBox::about(this, "About PolyWaveToImage",
			"Written by Powool");
	}
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	MainWindow win;
	win.show();

	return app.exec();
}

#include "PolyWaveToImage.moc"
