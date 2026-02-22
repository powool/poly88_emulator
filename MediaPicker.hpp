#pragma once

#include <memory>
#include <string>
#include <filesystem>

#include <QDialog>
#include <QListWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QCheckBox>

#include "MediaQueue.hpp"

// ---------------------------------------------------------------------------
// MediaPickerOptions — preferences state
// ---------------------------------------------------------------------------
struct MediaPickerOptions {
	bool deleteOnUse = false;
	bool useOnce = false;
	bool repeat = false;
};

// ---------------------------------------------------------------------------
// MediaListWidget — left pane: shows MediaQueue entries, supports reorder
// via drag-and-drop and drop from the file tree
// ---------------------------------------------------------------------------
class MediaListWidget : public QListWidget {
	Q_OBJECT
	std::shared_ptr<MediaQueue> queue;
public:
	explicit MediaListWidget(std::shared_ptr<MediaQueue> q, QWidget *parent = nullptr);
	void Refresh();

protected:
	void dropEvent(QDropEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	Qt::DropActions supportedDropActions() const override;
	QStringList mimeTypes() const override;

signals:
	void queueChanged();
};

// ---------------------------------------------------------------------------
// FileTreeView — right pane: hierarchical directory browser filtered by
// a wildcard pattern, with size tooltip on hover
// ---------------------------------------------------------------------------
class FileTreeView : public QTreeView {
	Q_OBJECT
	QFileSystemModel *fsModel = nullptr;
public:
	explicit FileTreeView(const QString &rootPath, const QStringList &nameFilters, QWidget *parent = nullptr);

protected:
	void startDrag(Qt::DropActions supportedActions) override;
};

// ---------------------------------------------------------------------------
// MediaPickerPreferences — non-modal preferences window
// ---------------------------------------------------------------------------
class MediaPickerPreferences : public QDialog {
	Q_OBJECT
	MediaPickerOptions &options;
	QCheckBox *deleteOnUseCheck = nullptr;
	QCheckBox *useOnceCheck = nullptr;
	QCheckBox *repeatCheck = nullptr;
public:
	explicit MediaPickerPreferences(MediaPickerOptions &opts, QWidget *parent = nullptr);
};

// ---------------------------------------------------------------------------
// MediaPicker — the dialog containing both panes
// ---------------------------------------------------------------------------
class MediaPicker : public QDialog {
	Q_OBJECT
	std::shared_ptr<MediaQueue> queue;
	MediaListWidget *listWidget = nullptr;
	FileTreeView *treeView = nullptr;
	QPushButton *removeButton = nullptr;
	MediaPickerOptions pickerOptions;
	MediaPickerPreferences *prefsWindow = nullptr;

public:
	explicit MediaPicker(std::shared_ptr<MediaQueue> q,
	                     const QString &rootPath = ".",
	                     const QStringList &nameFilters = {"*.cas", "*.bin", "*.hex", "*.rom"},
	                     QWidget *parent = nullptr);

	void ShowPicker();
	void HidePicker();

private slots:
	void OnRemoveClicked();
	void OnQueueChanged();
	void OnSave();
	void OnLoad();
	void OnClear();
	void OnPreferences();
};
