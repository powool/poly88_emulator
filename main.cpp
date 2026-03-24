#include <cmath>
#include <array>
#include <format>
#include <chrono>
#include <thread>
#include <string>
#include <memory>
#include <filesystem>

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

#include "PolyMorphics88.hpp"
#include "Poly88VdiFont.h"
#include "Poly88Vdi.hpp"
#include "MediaQueue.hpp"
#include "MediaPicker.hpp"

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

	// Media
	std::shared_ptr<MediaQueue> mediaQueue;
	std::shared_ptr<MediaPicker> mediaPicker;

	// Menu actions
	QAction *runStopAction  = nullptr;
	QAction *singleStepAction = nullptr;
	QAction *resetAction    = nullptr;
	QAction *mediaPickerAction = nullptr;

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

		mediaPickerAction = fileMenu->addAction("Media");
		connect(mediaPickerAction, &QAction::triggered, this, &MainWindow::ToggleMediaPicker);

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
			dlg.setText("Written by Paul Anderson");
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

		pcLabel = new QLabel();
		pcLabel->setFont(QFont("Monospace", 9));
		mainLayout->addWidget(pcLabel);

		spLabel = new QLabel();
		spLabel->setFont(QFont("Monospace", 9));
		mainLayout->addWidget(spLabel);

		centralFrame->setLayout(mainLayout);
		this->setCentralWidget(centralFrame);

		// ---- Media subsystem ----
		mediaQueue = std::make_shared<MediaQueue>();
		mediaPicker = std::make_shared<MediaPicker>(mediaQueue, ".");

		emulator = std::make_shared<PolyMorphics88>(mediaQueue);

		polyVdi->setFocusPolicy(Qt::StrongFocus);

		UpdateUI();
	}

	bool Closed() { return closed; }

	std::shared_ptr<EmulatorInterface> GetEmulator() { return emulator; }

	int CpuSpeed() { return cpuSpeed; }

	void UpdateUI() {
		auto mediaWanted = mediaQueue->MediaWanted();
		if (mediaWanted) {
			mediaPicker->ShowPicker();
		}

		// Menu labels & enabled state
		runStopAction->setText(emulator->Running() ? "Stop" : "Run");
		singleStepAction->setEnabled(!emulator->Running());
		resetAction->setEnabled(!emulator->Running());

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
		pcLabel->setText(QString::fromStdString(formatRegMemRow("PC", emulator->PC(), emulator)));
		spLabel->setText(QString::fromStdString(formatRegMemRow("SP", emulator->SP(), emulator)));

		// Update VDI scene
		polyVdi->UpdateScene(emulator);
	}

	void ToggleRunStop() {
		emulator->RunStop(!emulator->Running());
		UpdateUI();
	}

	void SingleStep() {
		if (!emulator->Running()) {
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

	void ConfirmQuit() {
		QMessageBox dlg(this);
		dlg.setWindowTitle("Confirm Quit");
		dlg.setText("Are you sure you want to quit?");
		dlg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
		dlg.setDefaultButton(QMessageBox::No);
		if (dlg.exec() == QMessageBox::Yes)
			closed = true;
	}

	void ToggleMediaPicker() {
		if (mediaPicker->isVisible())
			mediaPicker->HidePicker();
		else
			mediaPicker->ShowPicker();
	}

	void keyPressEvent(QKeyEvent *event) override {
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
		if (mediaPicker)
			mediaPicker->close();
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
