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
#include <QGridLayout>
#include <QMainWindow>
#include <QPushButton>
#include <QToolButton>
#include <QToolBar>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QMenuBar>
#include <QAction>
#include <QKeyEvent>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QStatusBar>
#include <QSplitter>
#include <QTimer>
#include <QStyle>

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
// Application-wide dark theme stylesheet
// ---------------------------------------------------------------------------
static const char *darkStyleSheet = R"(
	QMainWindow {
		background-color: #1e1e2e;
	}
	QMenuBar {
		background-color: #181825;
		color: #cdd6f4;
		border-bottom: 1px solid #313244;
		padding: 2px;
	}
	QMenuBar::item:selected {
		background-color: #45475a;
		border-radius: 4px;
	}
	QMenu {
		background-color: #1e1e2e;
		color: #cdd6f4;
		border: 1px solid #313244;
		border-radius: 6px;
		padding: 4px;
	}
	QMenu::item:selected {
		background-color: #45475a;
		border-radius: 4px;
	}
	QMenu::separator {
		height: 1px;
		background: #313244;
		margin: 4px 8px;
	}
	QToolBar {
		background-color: #181825;
		border-bottom: 1px solid #313244;
		spacing: 4px;
		padding: 4px 8px;
	}
	QPushButton {
		background-color: #313244;
		color: #cdd6f4;
		border: 1px solid #45475a;
		border-radius: 6px;
		padding: 6px 16px;
		font-weight: 500;
		min-width: 60px;
	}
	QPushButton:hover {
		background-color: #45475a;
		border-color: #585b70;
	}
	QPushButton:pressed {
		background-color: #585b70;
	}
	QPushButton:disabled {
		background-color: #1e1e2e;
		color: #585b70;
		border-color: #313244;
	}
	QPushButton:checked {
		background-color: #cba6f7;
		color: #1e1e2e;
		border-color: #cba6f7;
	}
	QPushButton#runStopBtn {
		background-color: #a6e3a1;
		color: #1e1e2e;
		border-color: #a6e3a1;
		font-weight: bold;
	}
	QPushButton#runStopBtn:hover {
		background-color: #94e2d5;
	}
	QPushButton#runStopBtn[running="true"] {
		background-color: #f38ba8;
		border-color: #f38ba8;
	}
	QPushButton#runStopBtn[running="true"]:hover {
		background-color: #eba0ac;
	}
	QPushButton#stepBtn {
		background-color: #a6e3a1;
		color: #1e1e2e;
		border-color: #a6e3a1;
		font-weight: bold;
	}
	QPushButton#stepBtn:hover {
		background-color: #94e2d5;
	}
	QPushButton#stepBtn:disabled {
		background-color: #1e1e2e;
		color: #585b70;
		border-color: #313244;
	}
	QPushButton#resetBtn {
		background-color: #f38ba8;
		color: #1e1e2e;
		border-color: #f38ba8;
		font-weight: bold;
	}
	QPushButton#resetBtn:hover {
		background-color: #eba0ac;
	}
	QPushButton#resetBtn:disabled {
		background-color: #1e1e2e;
		color: #585b70;
		border-color: #313244;
	}
	QGroupBox {
		background-color: #181825;
		border: 1px solid #313244;
		border-radius: 8px;
		margin-top: 14px;
		padding: 12px 8px 8px 8px;
		color: #cdd6f4;
	}
	QGroupBox::title {
		subcontrol-origin: margin;
		subcontrol-position: top left;
		left: 12px;
		padding: 0 6px;
		color: #89b4fa;
		font-weight: bold;
		font-size: 11px;
	}
	QLabel {
		color: #cdd6f4;
	}
	QLabel#regLabel {
		color: #cdd6f4;
		background-color: #11111b;
		border: 1px solid #313244;
		border-radius: 4px;
		padding: 3px 6px;
	}
	QLabel#regLabel[editable="true"]:hover {
		border-color: #89b4fa;
		background-color: #1e1e2e;
	}
	QLabel#regNameLabel {
		color: #89b4fa;
		font-weight: bold;
		padding: 3px 2px;
	}
	QLabel#sectionTitle {
		color: #89b4fa;
		font-weight: bold;
		font-size: 11px;
		padding: 2px 0;
	}
	QSlider::groove:horizontal {
		border: 1px solid #313244;
		height: 6px;
		background: #313244;
		border-radius: 3px;
	}
	QSlider::handle:horizontal {
		background: #89b4fa;
		border: none;
		width: 14px;
		margin: -5px 0;
		border-radius: 7px;
	}
	QSlider::handle:horizontal:hover {
		background: #b4befe;
	}
	QPlainTextEdit {
		background-color: #11111b;
		color: #a6e3a1;
		border: 1px solid #313244;
		border-radius: 6px;
		padding: 4px;
		selection-background-color: #45475a;
		selection-color: #cdd6f4;
	}
	QStatusBar {
		background-color: #181825;
		color: #a6adc8;
		border-top: 1px solid #313244;
		padding: 2px;
	}
	QStatusBar QLabel {
		padding: 0 8px;
	}
	QSplitter::handle {
		background-color: #313244;
		height: 2px;
	}
	QScrollBar:vertical {
		background: #181825;
		width: 10px;
		border-radius: 5px;
	}
	QScrollBar::handle:vertical {
		background: #45475a;
		min-height: 20px;
		border-radius: 5px;
	}
	QScrollBar::handle:vertical:hover {
		background: #585b70;
	}
	QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
		height: 0;
	}
	QScrollBar:horizontal {
		background: #181825;
		height: 10px;
		border-radius: 5px;
	}
	QScrollBar::handle:horizontal {
		background: #45475a;
		min-width: 20px;
		border-radius: 5px;
	}
	QScrollBar::handle:horizontal:hover {
		background: #585b70;
	}
	QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
		width: 0;
	}
	QToolTip {
		background-color: #313244;
		color: #cdd6f4;
		border: 1px solid #45475a;
		border-radius: 4px;
		padding: 4px;
	}
)";

// ---------------------------------------------------------------------------
// RegisterGrid — grouped register display with editable-state control
// ---------------------------------------------------------------------------
class RegisterGrid : public QGroupBox {
	QGridLayout *grid;
	QFont font;
	int nextRow = 1;  // row 0 is reserved for short registers (A, M, PSW)
	std::vector<QLabel*> editableLabels;

    public:
	RegisterGrid(const QString &title, const QFont &f, QWidget *parent = nullptr)
		: QGroupBox(title, parent), font(f)
	{
		grid = new QGridLayout;
		grid->setSpacing(4);
		grid->setContentsMargins(8, 8, 8, 8);
		setLayout(grid);
	}

	QGridLayout *Grid() { return grid; }

	void AddRegRow(const QString &name, QLabel *&label) {
		int row = nextRow++;
		auto *nameLabel = new QLabel(name);
		nameLabel->setObjectName("regNameLabel");
		label = new QLabel();
		label->setObjectName("regLabel");
		label->setFont(font);
		label->setProperty("regName", name);
		label->setCursor(Qt::PointingHandCursor);
		label->setToolTip("Click to edit " + name);
		grid->addWidget(nameLabel, row, 0);
		grid->addWidget(label, row, 1, 1, 7);
		editableLabels.push_back(label);
	}

	void SetEditable(bool editable) {
		for (auto *label : editableLabels) {
			if (editable) {
				label->setCursor(Qt::PointingHandCursor);
				QString name = label->property("regName").toString();
				label->setToolTip("Click to edit " + name);
			} else {
				label->setCursor(Qt::ArrowCursor);
				label->setToolTip(QString());
			}
			label->setProperty("editable", editable);
			label->style()->unpolish(label);
			label->style()->polish(label);
		}
	}
};

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
	QLabel *aLabel   = nullptr;
	QLabel *mLabel   = nullptr;
	QLabel *pswLabel = nullptr;
	QLabel *bcLabel  = nullptr;
	QLabel *deLabel  = nullptr;
	QLabel *hlLabel  = nullptr;
	QLabel *pcLabel  = nullptr;
	QLabel *spLabel  = nullptr;
	QSlider *speedSlider = nullptr;
	QLabel *speedValueLabel = nullptr;

	// Status bar widgets
	QLabel *interruptLabel = nullptr;
	QLabel *haltedLabel    = nullptr;
	QLabel *statusRunLabel = nullptr;

	// Register grid
	RegisterGrid *regGrid = nullptr;

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

		// ---- Apply dark theme ----
		this->setStyleSheet(darkStyleSheet);

		// ---- Menu bar ----
		auto *menuBar = this->menuBar();

		// File menu
		auto *fileMenu = menuBar->addMenu("&File");

		runStopAction = fileMenu->addAction("Run");
		runStopAction->setShortcut(QKeySequence(Qt::Key_F5));
		connect(runStopAction, &QAction::triggered, this, &MainWindow::ToggleRunStop);

		singleStepAction = fileMenu->addAction("Step");
		singleStepAction->setShortcut(QKeySequence(Qt::Key_F10));
		connect(singleStepAction, &QAction::triggered, this, &MainWindow::SingleStep);

		resetAction = fileMenu->addAction("Reset");
		resetAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
		connect(resetAction, &QAction::triggered, this, &MainWindow::ResetEmulator);

		fileMenu->addSeparator();

		loadImageAction = fileMenu->addAction("Load Image...");
		loadImageAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
		connect(loadImageAction, &QAction::triggered, this, &MainWindow::LoadImage);

		fileMenu->addSeparator();

		auto *prefsAction = fileMenu->addAction("Preferences...");
		prefsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
		connect(prefsAction, &QAction::triggered, this, &MainWindow::ShowPreferences);

		fileMenu->addSeparator();

		auto *quitAction = fileMenu->addAction("Quit");
		quitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));
		connect(quitAction, &QAction::triggered, this, &MainWindow::ConfirmQuit);

		// Help menu
		auto *helpMenu = menuBar->addMenu("&Help");

		auto *quickHelpAction = helpMenu->addAction("Quick Help");
		quickHelpAction->setShortcut(QKeySequence(Qt::Key_F1));
		connect(quickHelpAction, &QAction::triggered, this, [this]() {
			QMessageBox dlg(this);
			dlg.setWindowTitle("Quick Help");
			dlg.setStyleSheet("QMessageBox { background-color: #1e1e2e; } QLabel { color: #cdd6f4; font-size: 13px; } QPushButton { background-color: #45475a; color: #cdd6f4; padding: 6px 16px; border-radius: 4px; }");
			dlg.setTextFormat(Qt::MarkdownText);
			dlg.setText(
				"## Keyboard Shortcuts\n\n"
				"| Key | Action |\n"
				"|-----|--------|\n"
				"| **F5** | Run / Stop |\n"
				"| **F10** | Single Step |\n"
				"| **Ctrl+R** | Reset CPU |\n"
				"| **Ctrl+O** | Load Image |\n"
				"| **Ctrl+Q** | Quit |\n\n"
				"Click any register row to edit its value.\n\n"
				"Click the VDI display to give it keyboard focus."
			);
			dlg.setStandardButtons(QMessageBox::Close);
			dlg.exec();
		});

		auto *docAction = helpMenu->addAction("Documentation");
		connect(docAction, &QAction::triggered, this, []() {
			QDesktopServices::openUrl(QUrl("https://github.com/mainwindow/documentation#fixme"));
		});

		helpMenu->addSeparator();

		auto *aboutAction = helpMenu->addAction("About");
		connect(aboutAction, &QAction::triggered, this, [this]() {
			QMessageBox dlg(this);
			dlg.setWindowTitle("About Poly-88 Emulator");
			dlg.setStyleSheet("QMessageBox { background-color: #1e1e2e; } QLabel { color: #cdd6f4; font-size: 13px; } QPushButton { background-color: #45475a; color: #cdd6f4; padding: 6px 16px; border-radius: 4px; }");
			dlg.setTextFormat(Qt::MarkdownText);
			dlg.setText(
				"## Poly-88 Emulator\n\n"
				"An emulator for the Polymorphic Systems Poly-88 computer.\n\n"
				"Written by Powool"
			);
			dlg.setStandardButtons(QMessageBox::Close);
			dlg.exec();
		});

		// ---- Central widget layout ----
		auto *centralWidget = new QWidget;
		auto *mainLayout = new QVBoxLayout;
		mainLayout->setSpacing(8);
		mainLayout->setContentsMargins(12, 8, 12, 8);

		// -- Toolbar row --
		auto *toolbarRow = new QHBoxLayout;
		toolbarRow->setSpacing(6);

		runStopButton = new QPushButton("Run");
		runStopButton->setObjectName("runStopBtn");
		runStopButton->setToolTip("Start/stop CPU execution (F5)");
		connect(runStopButton, &QPushButton::clicked, this, &MainWindow::ToggleRunStop);
		toolbarRow->addWidget(runStopButton);

		singleStepButton = new QPushButton("Step");
		singleStepButton->setObjectName("stepBtn");
		singleStepButton->setToolTip("Execute one instruction (F10)");
		connect(singleStepButton, &QPushButton::clicked, this, &MainWindow::SingleStep);
		toolbarRow->addWidget(singleStepButton);

		resetButton = new QPushButton("Reset");
		resetButton->setObjectName("resetBtn");
		resetButton->setToolTip("Reset CPU (Ctrl+R)");
		connect(resetButton, &QPushButton::clicked, this, &MainWindow::ResetEmulator);
		toolbarRow->addWidget(resetButton);

		// Separator line
		auto *sep1 = new QFrame;
		sep1->setFrameShape(QFrame::VLine);
		sep1->setStyleSheet("color: #45475a;");
		toolbarRow->addWidget(sep1);

		traceButton = new QPushButton("Trace");
		traceButton->setCheckable(true);
		traceButton->setToolTip("Toggle instruction tracing");
		connect(traceButton, &QPushButton::toggled, this, [this](bool checked) {
			traceEnabled = checked;
			traceButton->setText(checked ? "Trace ON" : "Trace");
		});
		toolbarRow->addWidget(traceButton);

		toolbarRow->addStretch();

		// Speed control
		auto *speedLabel = new QLabel("Speed:");
		speedLabel->setStyleSheet("color: #a6adc8; font-size: 11px;");
		toolbarRow->addWidget(speedLabel);

		speedSlider = new QSlider(Qt::Horizontal);
		speedSlider->setRange(0, 100);
		speedSlider->setValue(cpuSpeed);
		speedSlider->setFixedWidth(140);
		speedSlider->setToolTip("CPU execution speed");
		connect(speedSlider, &QSlider::valueChanged, this, [this](int v) {
			cpuSpeed = v;
			speedValueLabel->setText(QString::number(v) + "%");
		});
		toolbarRow->addWidget(speedSlider);

		speedValueLabel = new QLabel(QString::number(cpuSpeed) + "%");
		speedValueLabel->setFixedWidth(36);
		speedValueLabel->setStyleSheet("color: #a6adc8; font-size: 11px;");
		toolbarRow->addWidget(speedValueLabel);

		mainLayout->addLayout(toolbarRow);

		// -- VDI display (fixed size, added directly) --
		polyVdi = new PolyVdi();
		mainLayout->addWidget(polyVdi, 0, Qt::AlignHCenter);

		// -- Registers panel --
		regGrid = new RegisterGrid("Registers", uiFont);

		// Row 0: A, M, PSW (short values)
		auto *aNameLabel = new QLabel("A");
		aNameLabel->setObjectName("regNameLabel");
		aLabel = new QLabel();
		aLabel->setObjectName("regLabel");
		aLabel->setFont(uiFont);
		regGrid->Grid()->addWidget(aNameLabel, 0, 0);
		regGrid->Grid()->addWidget(aLabel, 0, 1);

		auto *mNameLabel = new QLabel("M");
		mNameLabel->setObjectName("regNameLabel");
		mLabel = new QLabel();
		mLabel->setObjectName("regLabel");
		mLabel->setFont(uiFont);
		regGrid->Grid()->addWidget(mNameLabel, 0, 2);
		regGrid->Grid()->addWidget(mLabel, 0, 3);

		auto *pswNameLabel = new QLabel("PSW");
		pswNameLabel->setObjectName("regNameLabel");
		pswLabel = new QLabel();
		pswLabel->setObjectName("regLabel");
		pswLabel->setFont(uiFont);
		regGrid->Grid()->addWidget(pswNameLabel, 0, 4);
		regGrid->Grid()->addWidget(pswLabel, 0, 5, 1, 3);

		// Rows 1-5: register pairs with memory dump
		regGrid->AddRegRow("BC", bcLabel);
		regGrid->AddRegRow("DE", deLabel);
		regGrid->AddRegRow("HL", hlLabel);
		regGrid->AddRegRow("SP", spLabel);
		regGrid->AddRegRow("PC", pcLabel);

		regGrid->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

		// Make register labels clickable for editing
		bcLabel->installEventFilter(this);
		deLabel->installEventFilter(this);
		hlLabel->installEventFilter(this);
		spLabel->installEventFilter(this);
		pcLabel->installEventFilter(this);

		// -- Trace output --
		auto *traceGroup = new QGroupBox("Instruction Trace");
		auto *traceLayout = new QVBoxLayout;
		traceLayout->setContentsMargins(4, 4, 4, 4);
		traceLayout->setSpacing(0);

		traceOutput = new QPlainTextEdit();
		traceOutput->setReadOnly(true);
		traceOutput->setFont(uiFont);
		traceOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
		traceOutput->setPlaceholderText("Enable Trace and step to see instructions here...");
		traceLayout->addWidget(traceOutput, 1);
		traceGroup->setLayout(traceLayout);

		// -- Registers + Trace side by side --
		auto *regTraceRow = new QHBoxLayout;
		regTraceRow->setSpacing(8);
		regTraceRow->addWidget(regGrid, 0);
		regTraceRow->addWidget(traceGroup, 1);
		mainLayout->addLayout(regTraceRow, 1);

		centralWidget->setLayout(mainLayout);
		this->setCentralWidget(centralWidget);

		// ---- Status bar ----
		auto *sb = this->statusBar();
#if 0
		statusRunLabel = new QLabel("Stopped");
		statusRunLabel->setStyleSheet("font-weight: bold;");
		sb->addWidget(statusRunLabel);

		auto *sbSep1 = new QFrame;
		sbSep1->setFrameShape(QFrame::VLine);
		sbSep1->setStyleSheet("color: #45475a;");
		sb->addWidget(sbSep1);
#endif
		interruptLabel = new QLabel("Interrupts Enabled");
		sb->addWidget(interruptLabel);

		auto *sbSep2 = new QFrame;
		sbSep2->setFrameShape(QFrame::VLine);
		sbSep2->setStyleSheet("color: #45475a;");
		sb->addWidget(sbSep2);

		haltedLabel = new QLabel("Not Halted");
		sb->addWidget(haltedLabel);

		// ---- File dialog bridge ----
		fileDialogBridge = std::make_shared<FileDialogBridge>();

		emulator = std::make_shared<PolyMorphics88>(fileDialogBridge);

		polyVdi->setFocusPolicy(Qt::StrongFocus);
		polyVdi->installEventFilter(this);

		// Prevent the window from shrinking enough for panels to overlap.
		this->setMinimumSize(SCENE_W + 60, SCENE_H + 450);

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

		bool running = emulator->Running();

		// Menu labels & enabled state
		runStopAction->setText(running ? "Stop" : "Run");
		singleStepAction->setEnabled(!running);
		resetAction->setEnabled(!running);
		loadImageAction->setEnabled(!running);

		// Buttons
		runStopButton->setText(running ? "Running" : "Stopped");
		runStopButton->setProperty("running", !running);
		runStopButton->style()->unpolish(runStopButton);
		runStopButton->style()->polish(runStopButton);
		singleStepButton->setEnabled(!running);
		resetButton->setEnabled(!running);

		// Enable/disable register editing affordances
		regGrid->SetEditable(!running);
#if 0
		// Status bar
		if (running) {
			statusRunLabel->setText("Running");
			statusRunLabel->setStyleSheet("color: #a6e3a1; font-weight: bold;");
		} else {
			statusRunLabel->setText("Stopped");
			statusRunLabel->setStyleSheet("color: #f9e2af; font-weight: bold;");
		}
#endif
		interruptLabel->setText(emulator->InterruptEnable() ? "INT Enabled" : "INT Disabled");
		interruptLabel->setStyleSheet(emulator->InterruptEnable()
			? "color: #a6e3a1;" : "color: #585b70;");

		if (emulator->Halted()) {
			haltedLabel->setText("HALTED ");
			haltedLabel->setStyleSheet("color: #f38ba8; font-weight: bold;");
		} else {
			haltedLabel->setText("RUNNING");
			haltedLabel->setStyleSheet("color: #a6adc8;");
		}

		// Register labels
		aLabel->setText(QString::fromStdString(std::format("0x{:02x}", emulator->A())));
		mLabel->setText(QString::fromStdString(std::format("0x{:02x}", emulator->M())));
		pswLabel->setText(QString::fromStdString(std::format("{}", emulator->PSW())));

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
			emulator->RunOneInstruction();
			AppendTrace();
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
		// Intercept key presses destined for the VDI display
		if (obj == polyVdi && event->type() == QEvent::KeyPress) {
			auto *ke = static_cast<QKeyEvent*>(event);
		       QString text = ke->text();
		       if (!text.isEmpty()) {
			       uint8_t ch = static_cast<uint8_t>(text.at(0).toLatin1());
			       if (ch != 0)
				       emulator->KeyPress(ch);
			       return true;
		       }
		}

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

	void closeEvent(QCloseEvent *event) override {
		event->ignore();
		ConfirmQuit();
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
