#include "MediaPicker.hpp"

#include <QHeaderView>
#include <QToolTip>
#include <QApplication>
#include <QUrl>

// ---------------------------------------------------------------------------
// MediaListWidget
// ---------------------------------------------------------------------------
MediaListWidget::MediaListWidget(std::shared_ptr<MediaQueue> q, QWidget *parent)
	: QListWidget(parent), queue(std::move(q))
{
	setDragEnabled(true);
	setAcceptDrops(true);
	setDropIndicatorShown(true);
	setDragDropMode(QAbstractItemView::DragDrop);
	setDefaultDropAction(Qt::MoveAction);
	setSelectionMode(QAbstractItemView::SingleSelection);
	Refresh();
}

void MediaListWidget::Refresh()
{
	clear();
	auto cwd = std::filesystem::current_path();
	for (size_t i = 0; i < queue->Count(); i++) {
		auto &e = queue->At(i);
		QString label = QString::fromStdString(
			std::filesystem::relative(e.path, cwd).string());
		auto *item = new QListWidgetItem(label, this);
		item->setToolTip(QString("Size: %1 bytes").arg(e.size));
		if (!e.ready) {
			item->setForeground(Qt::gray);
		}
		item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
	}
}

Qt::DropActions MediaListWidget::supportedDropActions() const
{
	return Qt::MoveAction | Qt::CopyAction;
}

QStringList MediaListWidget::mimeTypes() const
{
	return {"text/uri-list", "application/x-qabstractitemmodeldatalist"};
}

void MediaListWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls() || event->source() == this) {
		event->acceptProposedAction();
	} else {
		QListWidget::dragEnterEvent(event);
	}
}

void MediaListWidget::dragMoveEvent(QDragMoveEvent *event)
{
	if (event->mimeData()->hasUrls() || event->source() == this) {
		event->acceptProposedAction();
	} else {
		QListWidget::dragMoveEvent(event);
	}
}

void MediaListWidget::dropEvent(QDropEvent *event)
{
	int dropRow = indexAt(event->position().toPoint()).row();
	if (dropRow < 0)
		dropRow = static_cast<int>(queue->Count());

	if (event->mimeData()->hasUrls() && event->source() != this) {
		// Drop from file tree — insert new entries
		for (auto &url : event->mimeData()->urls()) {
			QString localPath = url.toLocalFile();
			if (!localPath.isEmpty()) {
				queue->Insert(dropRow, std::filesystem::path(localPath.toStdString()));
				dropRow++;
			}
		}
		queue->SaveToFile();
		Refresh();
		emit queueChanged();
		event->acceptProposedAction();
	} else if (event->source() == this) {
		// Internal reorder
		auto selected = selectedItems();
		if (!selected.isEmpty()) {
			int fromRow = row(selected.first());
			if (fromRow >= 0 && fromRow != dropRow) {
				int targetRow = dropRow;
				if (fromRow < dropRow)
					targetRow--;
				queue->Move(fromRow, targetRow);
				queue->SaveToFile();
				Refresh();
				emit queueChanged();
			}
		}
		event->acceptProposedAction();
	} else {
		QListWidget::dropEvent(event);
	}
}

// ---------------------------------------------------------------------------
// FileTreeView
// ---------------------------------------------------------------------------
FileTreeView::FileTreeView(const QString &rootPath, const QStringList &nameFilters, QWidget *parent)
	: QTreeView(parent)
{
	fsModel = new QFileSystemModel(this);
	fsModel->setRootPath(rootPath);
	fsModel->setNameFilters(nameFilters);
	fsModel->setNameFilterDisables(false);
	fsModel->setReadOnly(true);

	setModel(fsModel);
	setRootIndex(fsModel->index(rootPath));

	// Show only name and size columns
	for (int i = 1; i < fsModel->columnCount(); i++) {
		if (i != 1) // column 1 is size
			setColumnHidden(i, true);
	}

	setDragEnabled(true);
	setDragDropMode(QAbstractItemView::DragOnly);
	setSelectionMode(QAbstractItemView::SingleSelection);

	header()->setStretchLastSection(false);
	header()->setSectionResizeMode(0, QHeaderView::Stretch);
	if (!isColumnHidden(1))
		header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
}

void FileTreeView::startDrag(Qt::DropActions supportedActions)
{
	auto indexes = selectedIndexes();
	if (indexes.isEmpty())
		return;

	QModelIndex idx = indexes.first();
	QString filePath = static_cast<QFileSystemModel*>(model())->filePath(idx);

	auto *mimeData = new QMimeData;
	QList<QUrl> urls;
	urls.append(QUrl::fromLocalFile(filePath));
	mimeData->setUrls(urls);

	auto *drag = new QDrag(this);
	drag->setMimeData(mimeData);
	drag->exec(Qt::CopyAction);
}

// ---------------------------------------------------------------------------
// MediaPickerPreferences
// ---------------------------------------------------------------------------
MediaPickerPreferences::MediaPickerPreferences(MediaPickerOptions &opts, QWidget *parent)
	: QDialog(parent, Qt::Window), options(opts)
{
	setWindowTitle("Media Picker Preferences");
	setMinimumSize(300, 200);
	setModal(false);

	auto *outerLayout = new QVBoxLayout(this);

	// Menu bar
	auto *menuBar = new QMenuBar(this);
	auto *helpMenu = menuBar->addMenu("Help");

	auto *quickHelpAction = helpMenu->addAction("Quick Help");
	connect(quickHelpAction, &QAction::triggered, this, [this]() {
		QMessageBox dlg(this);
		dlg.setWindowTitle("Quick Help");
		dlg.setTextFormat(Qt::MarkdownText);
		dlg.setText("Insert Preferences Help here");
		dlg.setStandardButtons(QMessageBox::Close);
		dlg.exec();
	});

	auto *docAction = helpMenu->addAction("Documentation");
	connect(docAction, &QAction::triggered, this, []() {
		QDesktopServices::openUrl(QUrl("https://github.com/mediapicker/documentation#fixme"));
	});

	outerLayout->setMenuBar(menuBar);

	// Checkboxes
	deleteOnUseCheck = new QCheckBox("Delete on use", this);
	deleteOnUseCheck->setChecked(options.deleteOnUse);
	connect(deleteOnUseCheck, &QCheckBox::toggled, this, [this](bool v) { options.deleteOnUse = v; });
	outerLayout->addWidget(deleteOnUseCheck);

	useOnceCheck = new QCheckBox("Use once", this);
	useOnceCheck->setChecked(options.useOnce);
	connect(useOnceCheck, &QCheckBox::toggled, this, [this](bool v) { options.useOnce = v; });
	outerLayout->addWidget(useOnceCheck);

	repeatCheck = new QCheckBox("Repeat", this);
	repeatCheck->setChecked(options.repeat);
	connect(repeatCheck, &QCheckBox::toggled, this, [this](bool v) { options.repeat = v; });
	outerLayout->addWidget(repeatCheck);

	outerLayout->addStretch();
}

// ---------------------------------------------------------------------------
// MediaPicker
// ---------------------------------------------------------------------------
MediaPicker::MediaPicker(std::shared_ptr<MediaQueue> q,
                         const QString &rootPath,
                         const QStringList &nameFilters,
                         QWidget *parent)
	: QDialog(parent), queue(std::move(q))
{
	setWindowTitle("Media Picker");
	setMinimumSize(700, 400);

	auto *mainLayout = new QVBoxLayout(this);

	// ---- Menu bar ----
	auto *menuBar = new QMenuBar(this);

	// File menu
	auto *fileMenu = menuBar->addMenu("File");

	auto *saveAction = fileMenu->addAction("Save");
	connect(saveAction, &QAction::triggered, this, &MediaPicker::OnSave);

	auto *loadAction = fileMenu->addAction("Load");
	connect(loadAction, &QAction::triggered, this, &MediaPicker::OnLoad);

	fileMenu->addSeparator();

	auto *closeAction = fileMenu->addAction("Close");
	connect(closeAction, &QAction::triggered, this, &MediaPicker::HidePicker);

	// Edit menu
	auto *editMenu = menuBar->addMenu("Edit");

	auto *clearAction = editMenu->addAction("Clear");
	connect(clearAction, &QAction::triggered, this, &MediaPicker::OnClear);

	auto *prefsAction = editMenu->addAction("Preferences");
	connect(prefsAction, &QAction::triggered, this, &MediaPicker::OnPreferences);

	// Help menu
	auto *helpMenu = menuBar->addMenu("Help");

	auto *quickHelpAction = helpMenu->addAction("Quick Help");
	connect(quickHelpAction, &QAction::triggered, this, [this]() {
		QMessageBox dlg(this);
		dlg.setWindowTitle("Quick Help");
		dlg.setTextFormat(Qt::MarkdownText);
		dlg.setText("Insert Help here");
		dlg.setStandardButtons(QMessageBox::Close);
		dlg.exec();
	});

	auto *docAction = helpMenu->addAction("Documentation");
	connect(docAction, &QAction::triggered, this, []() {
		QDesktopServices::openUrl(QUrl("https://github.com/mediapicker/documentation#fixme"));
	});

	mainLayout->setMenuBar(menuBar);

	// ---- Splitter with left/right panes ----
	auto *splitter = new QSplitter(Qt::Horizontal, this);

	// Left pane: media queue list
	auto *leftPane = new QWidget;
	auto *leftLayout = new QVBoxLayout(leftPane);
	leftLayout->setContentsMargins(0, 0, 0, 0);

	auto *leftLabel = new QLabel("Media Queue");
	leftLayout->addWidget(leftLabel);

	listWidget = new MediaListWidget(queue, this);
	leftLayout->addWidget(listWidget);

	removeButton = new QPushButton("Remove Selected");
	connect(removeButton, &QPushButton::clicked, this, &MediaPicker::OnRemoveClicked);
	leftLayout->addWidget(removeButton);

	splitter->addWidget(leftPane);

	// Right pane: file tree
	auto *rightPane = new QWidget;
	auto *rightLayout = new QVBoxLayout(rightPane);
	rightLayout->setContentsMargins(0, 0, 0, 0);

	auto *rightLabel = new QLabel("Files");
	rightLayout->addWidget(rightLabel);

	treeView = new FileTreeView(rootPath, nameFilters, this);
	rightLayout->addWidget(treeView);

	splitter->addWidget(rightPane);

	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 2);

	mainLayout->addWidget(splitter);

	connect(listWidget, &MediaListWidget::queueChanged, this, &MediaPicker::OnQueueChanged);
}

void MediaPicker::ShowPicker()
{
	listWidget->Refresh();
	show();
	raise();
	activateWindow();
}

void MediaPicker::HidePicker()
{
	hide();
}

void MediaPicker::OnRemoveClicked()
{
	int idx = listWidget->currentRow();
	if (idx >= 0 && idx < static_cast<int>(queue->Count())) {
		queue->Remove(idx);
		queue->SaveToFile();
		listWidget->Refresh();
	}
}

void MediaPicker::OnQueueChanged()
{
	// Hook for future use — queue was modified via drag-and-drop
}

void MediaPicker::OnSave()
{
	if (!queue->SaveToFile()) {
		QMessageBox::warning(this, "Save Failed", "Failed to save the media list to file.");
	}
}

void MediaPicker::OnLoad()
{
	queue->LoadFromFile();
	listWidget->Refresh();
}

void MediaPicker::OnClear()
{
	queue->Clear();
	listWidget->Refresh();
}

void MediaPicker::OnPreferences()
{
	if (!prefsWindow) {
		prefsWindow = new MediaPickerPreferences(pickerOptions, this);
	}
	prefsWindow->show();
	prefsWindow->raise();
	prefsWindow->activateWindow();
}
