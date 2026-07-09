// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// ImageGalleryDock implementation.
//
// Claude Generated 2026 - Batch border-trim gallery.

#include "imagegallerydock.h"

#include "../imagecrop.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kPathRole = Qt::UserRole;  // absolute image path stored on each item

// Build a fixed @p size × @p size thumbnail: the aspect-scaled image centred on a
// transparent square canvas so every list item is identical in size regardless of
// the image's aspect ratio. If @p cropInImg is valid, the common crop rectangle is
// drawn (dashed red) so the operator sees what will be trimmed.
QIcon makeThumbnail(const QImage& img, const QRect& cropInImg, int size)
{
    const QImage scaled = img.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    const int ox = (size - scaled.width()) / 2;
    const int oy = (size - scaled.height()) / 2;
    p.drawImage(ox, oy, scaled);
    if (cropInImg.isValid() && img.width() > 0) {
        const double s = static_cast<double>(scaled.width()) / img.width();
        const QRect r(ox + qRound(cropInImg.x() * s), oy + qRound(cropInImg.y() * s),
            qRound(cropInImg.width() * s), qRound(cropInImg.height() * s));
        QPen pen(QColor(0xFF, 0x40, 0x40));
        pen.setWidth(2);
        pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(r.adjusted(0, 0, -1, -1));
    }
    p.end();
    return QIcon(pm);
}
} // namespace

ImageGalleryDock::ImageGalleryDock(QWidget* parent)
    : QDockWidget(DockConfig::ImageGalleryDockTitle, parent)
{
    setObjectName(DockConfig::ImageGalleryDockObjectName);
    setupUI();
}

void ImageGalleryDock::setupUI()
{
    QWidget* root = new QWidget(this);
    QVBoxLayout* outer = new QVBoxLayout(root);
    outer->setContentsMargins(4, 4, 4, 4);

    // --- header controls ---
    QHBoxLayout* header = new QHBoxLayout;

    header->addWidget(new QLabel(tr("Show:")));
    m_sourceCombo = new QComboBox;
    m_sourceCombo->addItem(tr("This session"), 0);
    m_sourceCombo->addItem(tr("Folder: all PNG"), 1);
    m_sourceCombo->addItem(tr("Folder: resized only"), 2);
    m_sourceCombo->addItem(tr("Folder: originals only"), 3);
    m_sourceCombo->setToolTip(tr("This session: images exported now.\n"
                                 "Folder: *.png in the working directory, optionally filtered "
                                 "to the resized (*.resized.png) outputs or the originals."));
    connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) { reloadGallery(); });
    header->addWidget(m_sourceCombo);

    header->addSpacing(12);
    header->addWidget(new QLabel(tr("Tolerance:")));
    m_toleranceSlider = new QSlider(Qt::Horizontal);
    m_toleranceSlider->setRange(0, 32);
    m_toleranceSlider->setValue(0);
    m_toleranceSlider->setMaximumWidth(120);
    m_toleranceSlider->setToolTip(tr("Max per-channel deviation still treated as removable border."));
    m_toleranceValue = new QLabel(QStringLiteral("0"));
    m_toleranceValue->setMinimumWidth(20);
    connect(m_toleranceSlider, &QSlider::valueChanged, this,
        [this](int v) { m_toleranceValue->setText(QString::number(v)); });
    header->addWidget(m_toleranceSlider);
    header->addWidget(m_toleranceValue);

    header->addSpacing(12);
    header->addWidget(new QLabel(tr("Size:")));
    m_iconSizeSlider = new QSlider(Qt::Horizontal);
    m_iconSizeSlider->setRange(72, 280);
    m_iconSizeSlider->setValue(m_thumbSize);
    m_iconSizeSlider->setMaximumWidth(110);
    m_iconSizeSlider->setToolTip(tr("Thumbnail / preview size."));
    header->addWidget(m_iconSizeSlider);

    header->addStretch();

    m_analyzeButton = new QPushButton(tr("Analyze borders"));
    m_analyzeButton->setToolTip(tr("Find one common crop rectangle for the checked images "
                                   "so they all end up the same size."));
    connect(m_analyzeButton, &QPushButton::clicked, this, &ImageGalleryDock::analyzeBorders);
    header->addWidget(m_analyzeButton);

    m_saveButton = new QPushButton(tr("Save resized"));
    m_saveButton->setToolTip(tr("Write <name>.resized.png copies (metadata preserved)."));
    m_saveButton->setEnabled(false);
    connect(m_saveButton, &QPushButton::clicked, this, &ImageGalleryDock::saveResized);
    header->addWidget(m_saveButton);

    outer->addLayout(header);

    // --- thumbnail grid ---
    m_list = new QListWidget;
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(m_thumbSize, m_thumbSize));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSpacing(6);
    m_list->setUniformItemSizes(true);  // square icons → all items identical size
    m_list->setWordWrap(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QWidget::customContextMenuRequested, this, &ImageGalleryDock::onContextMenu);
    connect(m_list, &QListWidget::itemDoubleClicked, this,
        [this](QListWidgetItem* it) { if (it) viewImage(it->data(kPathRole).toString()); });
    outer->addWidget(m_list);

    // Icon-size zoom: rescale existing icons instantly, then re-render crisply after
    // a short debounce so dragging the slider stays smooth.
    m_thumbRefreshTimer = new QTimer(this);
    m_thumbRefreshTimer->setSingleShot(true);
    m_thumbRefreshTimer->setInterval(150);
    connect(m_thumbRefreshTimer, &QTimer::timeout, this, &ImageGalleryDock::refreshThumbnails);
    connect(m_iconSizeSlider, &QSlider::valueChanged, this, [this](int v) {
        m_thumbSize = v;
        m_list->setIconSize(QSize(v, v));
        m_thumbRefreshTimer->start();
    });

    m_status = new QLabel(tr("No images yet — export an image to populate the gallery."));
    m_status->setWordWrap(true);
    outer->addWidget(m_status);

    setWidget(root);
}

bool ImageGalleryDock::folderMode() const
{
    return m_sourceCombo && m_sourceCombo->currentData().toInt() != 0;
}

void ImageGalleryDock::setWorkingDirectory(const QString& dir)
{
    if (m_workingDir == dir)
        return;
    m_workingDir = dir;
    if (folderMode())
        reloadGallery();
}

void ImageGalleryDock::addExportedImage(const QString& path)
{
    const QString abs = QFileInfo(path).absoluteFilePath();
    if (!m_sessionImages.contains(abs))
        m_sessionImages.append(abs);
    if (m_workingDir.isEmpty())
        m_workingDir = QFileInfo(abs).absolutePath();

    reloadGallery();

    // Hidden by default → surface it once the operator has something to trim.
    show();
    raise();
}

QStringList ImageGalleryDock::currentImagePaths() const
{
    const int mode = m_sourceCombo ? m_sourceCombo->currentData().toInt() : 0;
    if (mode == 0 || m_workingDir.isEmpty())
        return m_sessionImages;

    // Folder modes: 1 = all PNG, 2 = resized (*.resized.png) only, 3 = originals only.
    QDir dir(m_workingDir);
    QStringList out;
    const QStringList names = dir.entryList({QStringLiteral("*.png")},
        QDir::Files | QDir::Readable, QDir::Name);
    for (const QString& n : names) {
        const bool isResized = n.endsWith(QStringLiteral(".resized.png"), Qt::CaseInsensitive);
        if (mode == 2 && !isResized)
            continue;
        if (mode == 3 && isResized)
            continue;
        out << dir.absoluteFilePath(n);
    }
    return out;
}

void ImageGalleryDock::addThumbnail(const QString& path)
{
    QImage img(path);
    QFileInfo fi(path);
    QString caption = fi.fileName();
    QIcon icon;
    if (!img.isNull()) {
        caption += QStringLiteral("\n%1×%2").arg(img.width()).arg(img.height());
        icon = makeThumbnail(img, QRect(), m_thumbSize);  // no crop overlay until analyzed
    } else {
        caption += tr("\n(unreadable)");
    }

    QListWidgetItem* item = new QListWidgetItem(icon, caption, m_list);
    item->setData(kPathRole, path);
    item->setToolTip(path);
    item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
    item->setCheckState(img.isNull() ? Qt::Unchecked : Qt::Checked);
    item->setTextAlignment(Qt::AlignHCenter);
}

void ImageGalleryDock::reloadGallery()
{
    m_list->clear();
    m_resizePaths.clear();
    m_cropRect = QRect();
    m_canvasSize = QSize();
    m_saveButton->setEnabled(false);

    const QStringList paths = currentImagePaths();
    for (const QString& p : paths)
        addThumbnail(p);

    if (paths.isEmpty())
        m_status->setText(tr("No images yet — export an image to populate the gallery."));
    else
        m_status->setText(tr("%n image(s). Check the ones to trim, then Analyze borders.",
            nullptr, static_cast<int>(paths.size())));
}

QStringList ImageGalleryDock::checkedPaths() const
{
    QStringList out;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        if (item->checkState() == Qt::Checked)
            out << item->data(kPathRole).toString();
    }
    return out;
}

void ImageGalleryDock::analyzeBorders()
{
    const int tol = m_toleranceSlider->value();
    const QStringList paths = checkedPaths();
    m_resizePaths.clear();
    m_cropRect = QRect();
    m_canvasSize = QSize();
    m_saveButton->setEnabled(false);

    if (paths.isEmpty()) {
        m_status->setText(tr("Check at least one image to analyze."));
        return;
    }

    // Load the checked frames; keep the parallel path list for the save step.
    QList<QImage> imgs;
    QStringList good;
    int minW = 0, minH = 0, maxW = 0, maxH = 0;
    for (const QString& p : paths) {
        QImage img(p);
        if (img.isNull())
            continue;
        if (good.isEmpty()) {
            minW = maxW = img.width();
            minH = maxH = img.height();
        } else {
            minW = qMin(minW, img.width());  maxW = qMax(maxW, img.width());
            minH = qMin(minH, img.height()); maxH = qMax(maxH, img.height());
        }
        imgs << img;
        good << p;
    }

    if (imgs.isEmpty()) {
        m_status->setText(tr("None of the checked images could be read."));
        return;
    }

    // Flip-book: lay every frame on a common (max-sized) canvas, then the crop is
    // the union of all content — the smallest rectangle that clips no frame.
    const QSize canvas = imagecrop::canvasSize(imgs);
    const QRect crop = imagecrop::commonContentRect(imgs, canvas, tol);
    if (crop.isEmpty()) {
        m_status->setText(tr("Nothing to trim — the checked images have no content."));
        return;
    }

    m_canvasSize = canvas;
    m_cropRect = crop;
    m_resizePaths = good;

    QString msg = tr("Uniform output %1×%2 px for %n frame(s)", nullptr,
                     static_cast<int>(good.size()))
                      .arg(crop.width()).arg(crop.height());
    if (minW != maxW || minH != maxH)
        msg += tr(" — sources %1×%2…%3×%4 unified on a %5×%6 canvas")
                   .arg(minW).arg(minH).arg(maxW).arg(maxH)
                   .arg(canvas.width()).arg(canvas.height());
    msg += tr(". Content centred, minimal common border.");
    m_status->setText(msg);
    m_saveButton->setEnabled(true);

    drawCropOverlays();  // show the crop rectangle on the analyzed thumbnails
}

void ImageGalleryDock::drawCropOverlays()
{
    if (m_cropRect.isEmpty() || m_canvasSize.isEmpty())
        return;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        const QString path = item->data(kPathRole).toString();
        if (!m_resizePaths.contains(path))
            continue;
        QImage img(path);
        if (img.isNull())
            continue;
        // Map the canvas-space crop into this image's own coordinates (it was
        // centred on the canvas), then draw it on the thumbnail.
        const int offx = (m_canvasSize.width() - img.width()) / 2;
        const int offy = (m_canvasSize.height() - img.height()) / 2;
        const QRect cropInImg = m_cropRect.translated(-offx, -offy);
        item->setIcon(makeThumbnail(img, cropInImg, m_thumbSize));
    }
}

void ImageGalleryDock::refreshThumbnails()
{
    m_list->setIconSize(QSize(m_thumbSize, m_thumbSize));
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        const QString path = item->data(kPathRole).toString();
        QImage img(path);
        if (img.isNull())
            continue;
        QRect cropInImg;  // re-apply the crop overlay only on analyzed items
        if (!m_cropRect.isEmpty() && !m_canvasSize.isEmpty() && m_resizePaths.contains(path)) {
            const int offx = (m_canvasSize.width() - img.width()) / 2;
            const int offy = (m_canvasSize.height() - img.height()) / 2;
            cropInImg = m_cropRect.translated(-offx, -offy);
        }
        item->setIcon(makeThumbnail(img, cropInImg, m_thumbSize));
    }
}

void ImageGalleryDock::saveResized()
{
    if (m_resizePaths.isEmpty() || m_cropRect.isEmpty() || m_canvasSize.isEmpty())
        return;

    const int tol = m_toleranceSlider->value();
    const QString stamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    const QString software = QStringLiteral("Qurcuma %1").arg(QCoreApplication::applicationVersion());

    int ok = 0;
    int fail = 0;
    QStringList savedOutputs;
    for (const QString& src : m_resizePaths) {
        QImage img(src);
        if (img.isNull()) {
            ++fail;
            continue;
        }
        const QRgb bg = img.pixel(0, 0);
        const QRect content = imagecrop::contentRect(img, bg, tol);

        // Centre on the shared canvas, then crop to the common rectangle so every
        // output has identical X×Y (flip-book registration).
        const QImage out = imagecrop::centerOnCanvas(img, m_canvasSize).copy(m_cropRect);

        QMap<QString, QString> extra;
        extra.insert(QStringLiteral("ResizeSourceSize"),
            QStringLiteral("%1x%2").arg(img.width()).arg(img.height()));
        extra.insert(QStringLiteral("ResizeCanvasSize"),
            QStringLiteral("%1x%2").arg(m_canvasSize.width()).arg(m_canvasSize.height()));
        extra.insert(QStringLiteral("ResizeOutputSize"),
            QStringLiteral("%1x%2").arg(m_cropRect.width()).arg(m_cropRect.height()));
        extra.insert(QStringLiteral("ResizeCropRect"),
            QStringLiteral("%1,%2,%3,%4").arg(m_cropRect.x()).arg(m_cropRect.y())
                .arg(m_cropRect.width()).arg(m_cropRect.height()));
        extra.insert(QStringLiteral("ResizeContentRect"),
            QStringLiteral("%1,%2,%3,%4").arg(content.x()).arg(content.y())
                .arg(content.width()).arg(content.height()));
        extra.insert(QStringLiteral("ResizeTolerance"), QString::number(tol));
        extra.insert(QStringLiteral("ResizeBackground"),
            QStringLiteral("%1,%2,%3,%4").arg(qRed(bg)).arg(qGreen(bg)).arg(qBlue(bg)).arg(qAlpha(bg)));
        extra.insert(QStringLiteral("ResizeBatchTimestamp"), stamp);
        extra.insert(QStringLiteral("ResizeSoftware"), software);

        const QFileInfo fi(src);
        const QString outPath = fi.dir().absoluteFilePath(
            fi.completeBaseName() + QStringLiteral(".resized.png"));
        if (imagecrop::savePngWithMetadata(out, img, outPath, extra)) {
            ++ok;
            savedOutputs << QFileInfo(outPath).absoluteFilePath();
        } else {
            ++fail;
        }
    }

    // Show the resized outputs alongside the originals in "This session".
    for (const QString& outAbs : savedOutputs)
        if (!m_sessionImages.contains(outAbs))
            m_sessionImages.append(outAbs);

    QString msg = tr("Saved %n resized image(s) (metadata preserved).", nullptr, ok);
    if (fail > 0)
        msg += tr(" %n failed.", nullptr, fail);
    // Refresh so the newly written .resized.png outputs appear (session or folder).
    reloadGallery();
    m_status->setText(msg);
}

void ImageGalleryDock::onContextMenu(const QPoint& pos)
{
    QListWidgetItem* item = m_list->itemAt(pos);
    if (!item)
        return;
    const QString path = item->data(kPathRole).toString();

    QMenu menu(this);
    QAction* viewAct = menu.addAction(tr("View image + metadata…"));
    // "Remove from gallery" only makes sense for the session list; in folder mode
    // a removed item would just reappear on the next scan.
    QAction* removeAct = nullptr;
    if (!folderMode())
        removeAct = menu.addAction(tr("Remove from gallery"));
    menu.addSeparator();
    QAction* deleteAct = menu.addAction(tr("Delete file from disk…"));

    QAction* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == viewAct) {
        viewImage(path);
    } else if (removeAct && chosen == removeAct) {
        m_sessionImages.removeAll(QFileInfo(path).absoluteFilePath());
        m_resizePaths.removeAll(path);
        reloadGallery();
    } else if (chosen == deleteAct) {
        const auto btn = QMessageBox::question(this, tr("Delete Image"),
            tr("Permanently delete this file from disk?\n\n%1").arg(path),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn != QMessageBox::Yes)
            return;
        if (QFile::remove(path)) {
            m_sessionImages.removeAll(QFileInfo(path).absoluteFilePath());
            m_resizePaths.removeAll(path);
            reloadGallery();
            m_status->setText(tr("Deleted %1").arg(QFileInfo(path).fileName()));
        } else {
            QMessageBox::warning(this, tr("Delete Image"),
                tr("Could not delete the file:\n%1").arg(path));
        }
    }
}

void ImageGalleryDock::viewImage(const QString& path)
{
    QImage img(path);
    if (img.isNull()) {
        QMessageBox::warning(this, tr("View Image"), tr("Could not load:\n%1").arg(path));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QFileInfo(path).fileName());
    dlg.resize(940, 620);
    QVBoxLayout* outer = new QVBoxLayout(&dlg);

    QSplitter* split = new QSplitter(Qt::Horizontal, &dlg);

    // Left: the image, zoomable (fit-to-window by default, zoom slider below).
    const QPixmap fullPixmap = QPixmap::fromImage(img);
    QLabel* imgLabel = new QLabel;
    imgLabel->setAlignment(Qt::AlignCenter);
    QScrollArea* scroll = new QScrollArea;
    scroll->setWidget(imgLabel);
    scroll->setWidgetResizable(false);
    scroll->setAlignment(Qt::AlignCenter);
    scroll->setBackgroundRole(QPalette::Dark);
    split->addWidget(scroll);

    // Right: pixel size + a table of the embedded PNG text chunks (provenance).
    QWidget* side = new QWidget;
    QVBoxLayout* right = new QVBoxLayout(side);
    right->setContentsMargins(0, 0, 0, 0);
    right->addWidget(new QLabel(tr("<b>%1 × %2 px</b>").arg(img.width()).arg(img.height())));

    QStringList keys;
    for (const QString& k : img.textKeys())
        if (!k.isEmpty())
            keys << k;
    keys.sort(Qt::CaseInsensitive);

    if (keys.isEmpty()) {
        right->addWidget(new QLabel(tr("(no embedded metadata)")));
        right->addStretch();
    } else {
        QTableWidget* table = new QTableWidget(keys.size(), 2, side);
        table->setHorizontalHeaderLabels({tr("Key"), tr("Value")});
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setWordWrap(true);
        for (int row = 0; row < keys.size(); ++row) {
            table->setItem(row, 0, new QTableWidgetItem(keys[row]));
            table->setItem(row, 1, new QTableWidgetItem(img.text(keys[row])));
        }
        table->resizeColumnToContents(0);
        table->horizontalHeader()->setStretchLastSection(true);
        right->addWidget(table);
    }
    split->addWidget(side);
    split->setStretchFactor(0, 4);   // image area ~80 %
    split->setStretchFactor(1, 1);   // metadata ~20 %
    split->setSizes({static_cast<int>(dlg.width() * 0.8), static_cast<int>(dlg.width() * 0.2)});
    outer->addWidget(split, 1);      // let the splitter take the vertical space

    // One compact bottom row: Fit · zoom slider · percent · Close.
    QHBoxLayout* zoomRow = new QHBoxLayout;
    QPushButton* fitButton = new QPushButton(tr("Fit"));
    QSlider* zoomSlider = new QSlider(Qt::Horizontal);
    zoomSlider->setRange(10, 400);
    zoomSlider->setValue(100);
    QLabel* zoomLabel = new QLabel(QStringLiteral("100%"));
    zoomLabel->setMinimumWidth(46);
    QPushButton* closeButton = new QPushButton(tr("Close"));
    connect(closeButton, &QPushButton::clicked, &dlg, &QDialog::accept);
    zoomRow->addWidget(fitButton);
    zoomRow->addWidget(new QLabel(tr("Zoom:")));
    zoomRow->addWidget(zoomSlider, 1);
    zoomRow->addWidget(zoomLabel);
    zoomRow->addSpacing(12);
    zoomRow->addWidget(closeButton);
    outer->addLayout(zoomRow);

    auto applyZoom = [imgLabel, zoomLabel, fullPixmap](int pct) {
        const QSize target = fullPixmap.size() * pct / 100;
        imgLabel->setPixmap(fullPixmap.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        imgLabel->resize(imgLabel->pixmap().size());
        zoomLabel->setText(QStringLiteral("%1%").arg(pct));
    };
    connect(zoomSlider, &QSlider::valueChanged, &dlg, applyZoom);

    auto fitToWindow = [scroll, fullPixmap, zoomSlider, applyZoom]() {
        const QSize vp = scroll->viewport()->size();
        if (fullPixmap.width() < 1 || fullPixmap.height() < 1)
            return;
        const double sx = static_cast<double>(vp.width()) / fullPixmap.width();
        const double sy = static_cast<double>(vp.height()) / fullPixmap.height();
        int pct = static_cast<int>(qMin(sx, sy) * 100.0);
        pct = qBound(zoomSlider->minimum(), pct, zoomSlider->maximum());
        // setValue triggers applyZoom via valueChanged; call directly if unchanged.
        if (zoomSlider->value() == pct)
            applyZoom(pct);
        else
            zoomSlider->setValue(pct);
    };
    connect(fitButton, &QPushButton::clicked, &dlg, fitToWindow);

    applyZoom(100);                              // sensible initial pixmap
    QTimer::singleShot(0, &dlg, fitToWindow);    // fit once the viewport has a size
    dlg.exec();
}
