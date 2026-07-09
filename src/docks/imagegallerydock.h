// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// ImageGalleryDock — bottom dock that collects exported molecule images and
// batch-trims their identical whitespace border (FlipBooQ-style), preserving the
// embedded PNG metadata. Hidden by default; auto-shown on the first export.
//
// Claude Generated 2026 - Batch border-trim gallery.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QTimer;

class ImageGalleryDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ImageGalleryDock(QWidget* parent = nullptr);

    /// Folder scanned when "Show all images in folder" is enabled.
    void setWorkingDirectory(const QString& dir);

public slots:
    /// Register an image exported during this session and surface the dock.
    void addExportedImage(const QString& path);

private slots:
    void reloadGallery();
    void analyzeBorders();
    void saveResized();
    void onContextMenu(const QPoint& pos);

private:
    void setupUI();
    /// True when the source filter shows folder files (not the session list).
    bool folderMode() const;
    /// Session images, or a filtered *.png set from the working directory.
    QStringList currentImagePaths() const;
    void addThumbnail(const QString& path);
    QStringList checkedPaths() const;
    /// Overlay the analyzed common crop rectangle onto the analyzed thumbnails.
    void drawCropOverlays();
    /// Re-render every thumbnail at the current icon size (keeps checks + overlays).
    void refreshThumbnails();
    /// Modal preview: fit-to-window image (zoom slider) + embedded-metadata table.
    void viewImage(const QString& path);

    QListWidget* m_list = nullptr;
    QComboBox* m_sourceCombo = nullptr;
    QSlider* m_toleranceSlider = nullptr;
    QSlider* m_iconSizeSlider = nullptr;
    QTimer* m_thumbRefreshTimer = nullptr;  // debounces crisp re-render while dragging
    int m_thumbSize = 132;                  // square thumbnail edge (px), zoomable
    QLabel* m_toleranceValue = nullptr;
    QPushButton* m_analyzeButton = nullptr;
    QPushButton* m_saveButton = nullptr;
    QLabel* m_status = nullptr;

    QString m_workingDir;
    QStringList m_sessionImages;   // absolute paths, arrival order, de-duplicated

    // Result of the last analyze pass (shared flip-book canvas + common crop).
    QSize m_canvasSize;            // max source width × height across the batch
    QRect m_cropRect;             // common crop in canvas coordinates
    QStringList m_resizePaths;    // images the crop applies to (checked & readable)
};
