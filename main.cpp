#include <cmath>
#include <array>
#include <format>
#include <chrono>
#include <thread>
#include <string>
#include <memory>
#include <filesystem>
#include <fstream>

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMainWindow>
#include <QPushButton>
#include <QFrame>
#include <QLabel>
#include <QSlider>
#include <QMenuBar>
#include <QAction>
#include <QKeyEvent>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

#include <QLineEdit>
#include <QMouseEvent>
#include <QFileDialog>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QFontDialog>
#include <QSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>

#include "PolyMorphics88.hpp"
#include "Poly88VdiFont.h"
#include "Poly88Vdi.hpp"
#include "FileDialogBridge.hpp"

// ---------------------------------------------------------------------------
// Helper: format a register + 16 memory bytes + ASCII representation
// ---------------------------------------------------------------------------
static std::string formatRegMemRow(
	const char *regName,
	uint16_t regVal,
	std::shared_ptr<EmulatorInterface> emulator)
{
	std::string s = std::format("{}: {:04x}", regName, regVal);
	std::string ascii;
	for (int i = -7; i < 8; i++) {
		uint8_t b = emulator->GetMemoryByte(regVal + i);
		s += std::format(" {:02x}", b);
		if (i == -1) s += " ";
		ascii += (b >= 0x20 && b < 0x7f) ? static_cast<char>(b) : '.';
	}
	s += " " + ascii;
	return s;
}

// ---------------------------------------------------------------------------
// MainWindow
// ---------------------------------------------------------------------------
class MainWindow : public QMainWindow
{
	bool closed = false;
	int  cpuSpeed = 50;

	std::shared_ptr<PolyMorphics88> emulator;
	PolyVdi *polyVdi = nullptr;

	// File dialog bridge for emulator <-> UI thread communication
	std::shared_ptr<FileDialogBridge> fileDialogBridge;

	// Menu actions
	QAction *runStopAction  = nullptr;
	QAction *singleStepAction = nullptr;
	QAction *resetAction    = nullptr;
	QAction *loadImageAction = nullptr;

	// Toolbar-area widgets
	QPushButton *runStopButton    = nullptr;
	QPushButton *singleStepButton = nullptr;
	QPushButton *resetButton      = nullptr;
	QLabel *interruptLabel = nullptr;
	QLabel *haltedLabel    = nullptr;
	QLabel *aLabel   = nullptr;
	QLabel *mLabel   = nullptr;
	QLabel *pswLabel = nullptr;
	QLabel *bcLabel  = nullptr;
	QLabel *deLabel  = nullptr;
	QLabel *hlLabel  = nullptr;
	QLabel *pcLabel  = nullptr;
	QLabel *spLabel  = nullptr;
	QSlider *speedSlider = nullptr;

	// Trace output
	QPlainTextEdit *traceOutput = nullptr;
	QPushButton *traceButton = nullptr;
	bool traceEnabled = false;

	// Preferences
	int maxTraceRows = 1000;
	QFont uiFont = QFont("Monospace", 9);

    public:
	MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
		this->setWindowTitle("Poly-88 Emulator");

		// ---- Menu bar ----
		auto *menuBar = this->menuBar();

		// File menu
		auto *fileMenu = menuBar->addMenu("File");

		runStopAction = fileMenu->addAction("Run");
		connect(runStopAction, &QAction::triggered, this, &MainWindow::ToggleRunStop);

		singleStepAction = fileMenu->addAction("Step");
		connect(singleStepAction, &QAction::triggered, this, &MainWindow::SingleStep);

		resetAction = fileMenu->addAction("Reset");
		connect(resetAction, &QAction::triggered, this, &MainWindow::ResetEmulator);

		loadImageAction = fileMenu->addAction("Load Image");
		connect(loadImageAction, &QAction::triggered, this, &MainWindow::LoadImage);

		auto *prefsAction = fileMenu->addAction("Preferences...");
		connect(prefsAction, &QAction::triggered, this, &MainWindow::ShowPreferences);

		fileMenu->addSeparator();

		auto *quitAction = fileMenu->addAction("Quit");
		connect(quitAction, &QAction::triggered, this, &MainWindow::ConfirmQuit);

		// Help menu
		auto *helpMenu = menuBar->addMenu("Help");

		auto *quickHelpAction = helpMenu->addAction("Quick Help");
		connect(quickHelpAction, &QAction::triggered, this, [this]() {
			QMessageBox dlg(this);
			dlg.setWindowTitle("Quick Help");
			dlg.setTextFormat(Qt::MarkdownText);
			dlg.setText("Insert MainWindow Help here");
			dlg.setStandardButtons(QMessageBox::Close);
			dlg.exec();
		});

		auto *docAction = helpMenu->addAction("Documentation");
		connect(docAction, &QAction::triggered, this, []() {
			QDesktopServices::openUrl(QUrl("https://github.com/mainwindow/documentation#fixme"));
		});

		auto *aboutAction = helpMenu->addAction("About");
		connect(aboutAction, &QAction::triggered, this, [this]() {
			QMessageBox dlg(this);
			dlg.setWindowTitle("About");
			dlg.setTextFormat(Qt::MarkdownText);
			dlg.setText("Written by Powool");
			dlg.setStandardButtons(QMessageBox::Close);
			dlg.exec();
		});

		// ---- Central widget layout ----
		auto *centralFrame = new QFrame;
		auto *mainLayout = new QVBoxLayout;

		// VDI display
		polyVdi = new PolyVdi();
		mainLayout->addWidget(polyVdi, 0, Qt::AlignHCenter);

		// Row 1: Run/Stop, SingleStep, Interrupts, Halted, Reset
		auto *row1 = new QHBoxLayout;
		runStopButton = new QPushButton("Run");
		connect(runStopButton, &QPushButton::clicked, this, &MainWindow::ToggleRunStop);
		row1->addWidget(runStopButton);

		singleStepButton = new QPushButton("Step");
		connect(singleStepButton, &QPushButton::clicked, this, &MainWindow::SingleStep);
		row1->addWidget(singleStepButton);

		traceButton = new QPushButton("Trace");
		traceButton->setCheckable(true);
		connect(traceButton, &QPushButton::toggled, this, [this](bool checked) {
			traceEnabled = checked;
			traceButton->setText(checked ? "Trace On" : "Trace");
		});
		row1->addWidget(traceButton);

		interruptLabel = new QLabel("Interrupts Enabled");
		interruptLabel->setFrameShape(QFrame::Box);
		interruptLabel->setLineWidth(1);
		row1->addWidget(interruptLabel);

		haltedLabel = new QLabel("Halted");
		haltedLabel->setFrameShape(QFrame::Box);
		haltedLabel->setLineWidth(1);
		row1->addWidget(haltedLabel);

		resetButton = new QPushButton("Reset");
		connect(resetButton, &QPushButton::clicked, this, &MainWindow::ResetEmulator);
		row1->addWidget(resetButton);

		row1->addStretch();
		mainLayout->addLayout(row1);

		// Row 2: A, M, PSW, speed slider
		auto *row2 = new QHBoxLayout;
		aLabel   = new QLabel();
		aLabel->setFont(QFont("Monospace", 9));
		mLabel   = new QLabel();
		mLabel->setFont(QFont("Monospace", 9));
		pswLabel = new QLabel();
		pswLabel->setFont(QFont("Monospace", 9));
		row2->addWidget(aLabel);
		row2->addWidget(mLabel);
		row2->addWidget(pswLabel);
		row2->addStretch();

		speedSlider = new QSlider(Qt::Horizontal);
		speedSlider->setRange(0, 100);
		speedSlider->setValue(cpuSpeed);
		connect(speedSlider, &QSlider::valueChanged, this, [this](int v){ cpuSpeed = v; });
		row2->addWidget(speedSlider);
		mainLayout->addLayout(row2);

		// Row 3-5: BC, DE, HL + memory dump
		bcLabel = new QLabel();
		bcLabel->setFont(QFont("Monospace", 9));
		mainLayout->addWidget(bcLabel);

		deLabel = new QLabel();
		deLabel->setFont(QFont("Monospace", 9));
		mainLayout->addWidget(deLabel);

		hlLabel = new QLabel();
		hlLabel->setFont(QFont("Monospace", 9));
		mainLayout->addWidget(hlLabel);

		spLabel = new QLabel();
		spLabel->setFont(QFont("Monospace", 9));
		mainLayout->addWidget(spLabel);

		pcLabel = new QLabel();
		pcLabel->setFont(QFont("Monospace", 9));
		mainLayout->addWidget(pcLabel);

		// Make register labels clickable for editing
		bcLabel->installEventFilter(this);
		deLabel->installEventFilter(this);
		hlLabel->installEventFilter(this);
		spLabel->installEventFilter(this);
		pcLabel->installEventFilter(this);

		// Trace output (scrollable, monospace, grows vertically)
		traceOutput = new QPlainTextEdit();
		traceOutput->setReadOnly(true);
		traceOutput->setFont(uiFont);
		traceOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
		mainLayout->addWidget(traceOutput, 1);

		centralFrame->setLayout(mainLayout);
		this->setCentralWidget(centralFrame);

		// ---- File dialog bridge ----
		fileDialogBridge = std::make_shared<FileDialogBridge>();

		emulator = std::make_shared<PolyMorphics88>(fileDialogBridge);

		polyVdi->setFocusPolicy(Qt::StrongFocus);

		UpdateUI();
	}

	bool Closed() { return closed; }

	std::shared_ptr<EmulatorInterface> GetEmulator() { return emulator; }

	int CpuSpeed() { return cpuSpeed; }

	void UpdateUI() {
		if (fileDialogBridge->IsRequested()) {
			QString title = QString::fromStdString(fileDialogBridge->GetTitle());
			QString dir = QString::fromStdString(fileDialogBridge->GetLastDirectory());
			QString filter = "Cassette Files (*.cas *.CAS);;All Files (*)";
			QString path;
			if (fileDialogBridge->IsSaveMode())
				path = QFileDialog::getSaveFileName(this, title, dir, filter);
			else
				path = QFileDialog::getOpenFileName(this, title, dir, filter);
			fileDialogBridge->Respond(path.toStdString());
		}

		// Menu labels & enabled state
		runStopAction->setText(emulator->Running() ? "Stop" : "Run");
		singleStepAction->setEnabled(!emulator->Running());
		resetAction->setEnabled(!emulator->Running());
		loadImageAction->setEnabled(!emulator->Running());

		// Buttons
		runStopButton->setText(emulator->Running() ? "Stop" : "Run");
		singleStepButton->setEnabled(!emulator->Running());
		resetButton->setEnabled(!emulator->Running());

			// Status labels
		interruptLabel->setText(emulator->InterruptEnable() ? "Interrupts  Enabled" : "Interrupts Disabled");

		if (emulator->Halted()) {
			haltedLabel->setText("Halted");
			haltedLabel->setStyleSheet("QLabel { color: red; border: 1px solid red; padding: 2px; }");
		} else {
			haltedLabel->setText("Not Halted");
			haltedLabel->setStyleSheet("QLabel { color: green; border: 1px solid green; padding: 2px; }");
		}

		// Register labels
		aLabel->setText(QString::fromStdString(std::format("A: 0x{:02x}", emulator->A())));
		mLabel->setText(QString::fromStdString(std::format("M: 0x{:02x}", emulator->M())));
		pswLabel->setText(QString::fromStdString(std::format("PSW: {}", emulator->PSW())));

		// Register + memory rows
		bcLabel->setText(QString::fromStdString(formatRegMemRow("BC", emulator->BC(), emulator)));
		deLabel->setText(QString::fromStdString(formatRegMemRow("DE", emulator->DE(), emulator)));
		hlLabel->setText(QString::fromStdString(formatRegMemRow("HL", emulator->HL(), emulator)));
		spLabel->setText(QString::fromStdString(formatRegMemRow("SP", emulator->SP(), emulator)));
		pcLabel->setText(QString::fromStdString(formatRegMemRow("PC", emulator->PC(), emulator)));

		// Update VDI scene
		polyVdi->UpdateScene(emulator);
	}

	void ToggleRunStop() {
		emulator->RunStop(!emulator->Running());
		UpdateUI();
	}

	void AppendTrace() {
		QString line = QString::fromStdString(emulator->Disassemble(emulator->PC()));
		traceOutput->appendPlainText(line);
		// Enforce max rows
		if (traceOutput->document()->blockCount() > maxTraceRows) {
			QTextCursor cursor = traceOutput->textCursor();
			cursor.movePosition(QTextCursor::Start);
			cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor,
				traceOutput->document()->blockCount() - maxTraceRows);
			cursor.removeSelectedText();
			cursor.deleteChar(); // remove leftover newline
		}
	}

	void SingleStep() {
		if (!emulator->Running()) {
			AppendTrace();
			emulator->RunOneInstruction();
			UpdateUI();
		}
	}

	void ResetEmulator() {
		if (!emulator->Running()) {
			emulator->Reset();
			UpdateUI();
		}
	}

	void ApplyFont() {
		// Apply uiFont to all relevant widgets
		QFont mono = uiFont;
		aLabel->setFont(mono);
		mLabel->setFont(mono);
		pswLabel->setFont(mono);
		bcLabel->setFont(mono);
		deLabel->setFont(mono);
		hlLabel->setFont(mono);
		spLabel->setFont(mono);
		pcLabel->setFont(mono);
		traceOutput->setFont(mono);
		menuBar()->setFont(mono);
	}

	void ShowPreferences() {
		QDialog dlg(this);
		dlg.setWindowTitle("Preferences");
		auto *form = new QFormLayout(&dlg);

		// Max trace rows
		auto *rowsSpin = new QSpinBox();
		rowsSpin->setRange(100, 1000000);
		rowsSpin->setValue(maxTraceRows);
		form->addRow("Max trace rows:", rowsSpin);

		// Font picker
		auto *fontButton = new QPushButton(
			QString("%1, %2pt").arg(uiFont.family()).arg(uiFont.pointSize()));
		QFont chosenFont = uiFont;
		connect(fontButton, &QPushButton::clicked, &dlg, [&]() {
			bool ok;
			QFont f = QFontDialog::getFont(&ok, chosenFont, &dlg, "Select UI Font");
			if (ok) {
				chosenFont = f;
				fontButton->setText(QString("%1, %2pt").arg(f.family()).arg(f.pointSize()));
			}
		});
		form->addRow("UI Font:", fontButton);

		auto *buttons = new QDialogButtonBox(
			QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
		form->addRow(buttons);

		if (dlg.exec() == QDialog::Accepted) {
			maxTraceRows = rowsSpin->value();
			uiFont = chosenFont;
			ApplyFont();
		}
	}

	void LoadImage() {
		if (emulator->Running()) return;
		QString path = QFileDialog::getOpenFileName(this, "Load Image", ".",
			"Image Files (*.img *.IMG);;All Files (*)");
		if (path.isEmpty()) return;

		std::ifstream file(path.toStdString(), std::ios::binary);
		if (!file) {
			QMessageBox::warning(this, "Load Image", "Could not open file.");
			return;
		}

		constexpr uint16_t baseAddress = 0x2000;
		constexpr size_t maxSize = 55296;
		uint16_t address = baseAddress;
		size_t count = 0;
		char byte;
		while (file.get(byte) && count < maxSize) {
			emulator->PutMemoryByte(address, static_cast<uint8_t>(byte));
			address++;
			count++;
		}

		UpdateUI();
	}

	void ConfirmQuit() {
		QMessageBox dlg(this);
		dlg.setWindowTitle("Confirm Quit");
		dlg.setText("Are you sure you want to quit?");
		dlg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		dlg.setDefaultButton(QMessageBox::No);
		if (dlg.exec() == QMessageBox::Yes)
			closed = true;
	}

	void EditRegister(const QString &name, uint16_t currentValue,
		std::function<void(uint16_t)> setter) {
		if (emulator->Running()) return;
		QDialog dlg(this);
		dlg.setWindowTitle("Edit " + name);
		auto *form = new QFormLayout(&dlg);

		auto *edit = new QLineEdit(QString::asprintf("%04X", currentValue));
		edit->setFont(uiFont);
		form->addRow(name + ":", edit);

		auto *buttons = new QDialogButtonBox(
			QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
		connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
		form->addRow(buttons);

		edit->selectAll();
		edit->setFocus();

		if (dlg.exec() == QDialog::Accepted) {
			bool ok;
			uint16_t val = edit->text().toUShort(&ok, 16);
			if (ok) {
				setter(val);
				UpdateUI();
			}
		}
	}

	bool eventFilter(QObject *obj, QEvent *event) override {
		if (event->type() == QEvent::MouseButtonPress) {
			auto *me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::LeftButton) {
				if (obj == bcLabel) {
					EditRegister("BC", emulator->BC(),
						[this](uint16_t v){ emulator->BC(v); });
					return true;
				} else if (obj == deLabel) {
					EditRegister("DE", emulator->DE(),
						[this](uint16_t v){ emulator->DE(v); });
					return true;
				} else if (obj == hlLabel) {
					EditRegister("HL", emulator->HL(),
						[this](uint16_t v){ emulator->HL(v); });
					return true;
				} else if (obj == spLabel) {
					EditRegister("SP", emulator->SP(),
						[this](uint16_t v){ emulator->SP(v); });
					return true;
				} else if (obj == pcLabel) {
					EditRegister("PC", emulator->PC(),
						[this](uint16_t v){ emulator->PC(v); });
					return true;
				}
			}
		}
		return QMainWindow::eventFilter(obj, event);
	}

	void keyPressEvent(QKeyEvent *event) override {
//		if (polyVdi && polyVdi->underMouse()) {
		if (polyVdi && polyVdi->hasFocus()) {
			QString text = event->text();
			if (!text.isEmpty()) {
				uint8_t ch = static_cast<uint8_t>(text.at(0).toLatin1());
				if (ch != 0)
					emulator->KeyPress(ch);
			}
		} else {
			QMainWindow::keyPressEvent(event);
		}
	}

	void closeEvent(QCloseEvent *event) override {
		closed = true;
	}
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
		win->UpdateUI();
		// win->CpuSpeed() is a work in progress - do we care enough to add this?
	}

	qtApplication.quit();
	return 0;
}
