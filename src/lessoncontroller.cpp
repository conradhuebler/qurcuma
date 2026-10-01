// lessoncontroller.cpp - OER-lesson feature controller (extracted from MainWindow).
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - see lessoncontroller.h. Behaviour is preserved 1:1 from
// the former MainWindow lesson methods; MainWindow-global side effects (working
// directory, window title, dir refresh, status bar, post-load bookkeeping) are now
// emitted as signals.

#include "lessoncontroller.h"

#include "docks/dockmanager.h"

#include "dialogs/lessonmetadatadialog.h"
#include "lessonstructuremodel.h"
#include "moleculefileloader.h"
#include "simulationcontrolwidget.h"
#include "view.h"

#include <QAbstractItemModel>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QToolButton>

LessonController::LessonController(QWidget* dialogParent, QObject* parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
{
}

void LessonController::setLessonView(QListView* view)
{
    m_lessonView = view;
}

void LessonController::setMetaWidgets(QWidget* metaWidget, QLineEdit* title, QLineEdit* desc, QLabel* authors)
{
    m_metaWidget = metaWidget;
    m_titleEdit = title;
    m_descEdit = desc;
    m_authorsLabel = authors;
}

void LessonController::setStructWidgets(QWidget* structWidget, QLineEdit* name, QLineEdit* desc, QComboBox* role)
{
    m_structWidget = structWidget;
    m_structNameEdit = name;
    m_structDescEdit = desc;
    m_structRoleCombo = role;
}

// Build the in-memory structure model (over m_lesson.structures) and wire the inline
// metadata + per-structure editors, which write straight back into m_lesson.
void LessonController::wireWidgetConnections()
{
    m_structureModel = new LessonStructureModel(&m_lesson.structures, this);

    // Claude Generated 2026 - The lesson list is its own view (Lesson section), so the
    // file browser keeps showing files in every mode.
    if (m_lessonView) {
        m_lessonView->setModel(m_structureModel);
        connect(m_lessonView, &QListView::clicked, this,
            [this](const QModelIndex& index) { loadStructureFromIndex(index); });
        connect(m_lessonView, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
            const QModelIndex index = m_lessonView->indexAt(pos);
            if (!index.isValid())
                return;
            QMenu menu(m_lessonView);
            QAction* loadAct = menu.addAction(tr("Load Structure"));
            QAction* removeAct = menu.addAction(tr("Remove from Lesson"));
            QAction* chosen = menu.exec(m_lessonView->viewport()->mapToGlobal(pos));
            if (chosen == loadAct)
                loadStructureFromIndex(index);
            else if (chosen == removeAct)
                removeStructure(index.row());
        });
    }

    if (m_titleEdit)
        connect(m_titleEdit, &QLineEdit::textEdited, this,
            [this](const QString& t) { m_lesson.meta.title = t.trimmed(); });
    if (m_descEdit)
        connect(m_descEdit, &QLineEdit::textEdited, this,
            [this](const QString& t) { m_lesson.meta.description = t.trimmed(); });

    if (m_structNameEdit)
        connect(m_structNameEdit, &QLineEdit::textEdited, this, [this](const QString& t) {
            if (m_currentRow >= 0 && m_currentRow < m_lesson.structures.size()) {
                m_lesson.structures[m_currentRow].name = t;
                m_structureModel->refresh();
            }
        });
    if (m_structDescEdit)
        connect(m_structDescEdit, &QLineEdit::textEdited, this, [this](const QString& t) {
            if (m_currentRow >= 0 && m_currentRow < m_lesson.structures.size())
                m_lesson.structures[m_currentRow].description = t;
        });
    if (m_structRoleCombo)
        connect(m_structRoleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int idx) {
                if (m_currentRow >= 0 && m_currentRow < m_lesson.structures.size()) {
                    m_lesson.structures[m_currentRow].role =
                        (idx <= 0) ? QString() : m_structRoleCombo->currentText();
                    m_structureModel->refresh();
                }
            });
}

void LessonController::openLesson(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(m_dialogParent, tr("Open Lesson"), tr("Could not read %1").arg(path));
        return;
    }
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    f.close();
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(m_dialogParent, tr("Open Lesson"),
            tr("Invalid lesson JSON: %1").arg(perr.errorString()));
        return;
    }
    QString err;
    Lesson lesson = lessonFromJson(doc.object(), &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(m_dialogParent, tr("Open Lesson"), err);
        return;
    }
    if (lesson.structures.isEmpty()) {
        QMessageBox::information(m_dialogParent, tr("Open Lesson"), tr("This lesson contains no structures."));
        return;
    }

    // Unpack into <file-dir>/<stem>/ so the structures show up in the browser.
    const QFileInfo fi(path);
    QString stem = fi.fileName();
    stem.remove(QStringLiteral(".qlesson.json"), Qt::CaseInsensitive);
    stem.remove(QStringLiteral(".json"), Qt::CaseInsensitive);
    if (stem.isEmpty()) stem = QStringLiteral("lesson");
    const QString targetDir = fi.absoluteDir().filePath(stem);

    QString extractErr;
    if (!extractLesson(lesson, targetDir, &extractErr)) {
        QMessageBox::warning(m_dialogParent, tr("Open Lesson"), extractErr);
        return;
    }

    m_lesson = lesson;  // adopt so further edits / re-save work
    m_lessonFilePath = path;  // remember source so "Save Lesson" can overwrite it
    emit workingDirectoryChangeRequested(targetDir);
    // Claude Generated 2026 - Mode and panels are MainWindow's: it switches to Teaching
    // first and then opens the lesson's panels, so the switch cannot overwrite them.
    emit lessonOpened(lesson.panels);
    // Refresh the lesson list, its count and the metadata; the extracted .xyz also show
    // in the file browser of the lesson's directory.
    refreshStructureView(/*autoShow=*/false);
    refreshMetaWidget();
    showStructureDetails(-1);

    const QString title = lesson.meta.title.isEmpty() ? fi.fileName() : lesson.meta.title;
    const QString author = lesson.meta.authors.isEmpty() ? QString() : lesson.meta.authors.first().name;
    emit windowTitleChangeRequested(QStringLiteral("Qurcuma — %1%2").arg(title,
        author.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(author)));
    emit statusMessage(tr("Lesson '%1' loaded: %2 structure(s) in %3")
        .arg(title).arg(lesson.structures.size()).arg(targetDir), 5000);
}

// Write the in-memory lesson as a self-contained *.qlesson.json (inline XYZ).
void LessonController::saveLesson(const QString& path)
{
    if (m_lesson.structures.isEmpty()) {
        QMessageBox::information(m_dialogParent, tr("Save Lesson"),
            tr("The lesson is empty. Use 'Add Current Structure to Lesson…' first."));
        return;
    }
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (m_lesson.meta.created.isEmpty())
        m_lesson.meta.created = now;
    m_lesson.meta.modified = now;
    m_lesson.meta.qurcumaVersion = QCoreApplication::applicationVersion();
    if (m_dockManager)
        m_lesson.panels = m_dockManager->openPanels();  // what the author sees now

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(m_dialogParent, tr("Save Lesson"), tr("Could not write %1").arg(path));
        return;
    }
    f.write(QJsonDocument(lessonToJson(m_lesson, /*inlineXyz=*/true)).toJson(QJsonDocument::Indented));
    f.close();
    m_lessonFilePath = path;  // remember target so a follow-up "Save Lesson" overwrites it
    emit statusMessage(
        tr("Lesson saved: %1 (%2 structure(s))").arg(path).arg(m_lesson.structures.size()), 4000);
}

// Resolve the save target and call saveLesson(). With forceDialog==false, a known
// current lesson path (from openLesson/saveLesson) is overwritten silently — the
// "Save Lesson" behaviour the user expects. Otherwise (Save As, or no known path)
// a Save dialog is shown, defaulting to the current path.
bool LessonController::saveLessonInteractive(bool forceDialog)
{
    if (m_lesson.structures.isEmpty()) {
        QMessageBox::information(m_dialogParent, tr("Save Lesson"),
            tr("The lesson is empty. Use 'Add Current Structure to Lesson…' first."));
        return false;
    }

    QString path = m_lessonFilePath;
    if (forceDialog || path.isEmpty()) {
        const QString startDir = !m_lessonFilePath.isEmpty()
            ? QFileInfo(m_lessonFilePath).absolutePath()
            : (m_workingDirectory.isEmpty() ? QDir::homePath() : m_workingDirectory);
        const QString suggestion = !m_lessonFilePath.isEmpty()
            ? QFileInfo(m_lessonFilePath).fileName()
            : QStringLiteral("lesson.qlesson.json");
        path = QFileDialog::getSaveFileName(m_dialogParent, tr("Save Lesson"),
            QDir(startDir).filePath(suggestion),
            tr("Qurcuma Lesson (*.qlesson.json);;All Files (*)"));
        if (path.isEmpty())
            return false;  // user cancelled
        if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
            path += QStringLiteral(".qlesson.json");
    }
    saveLesson(path);
    return true;
}

// Build a LessonStructure from atoms + the dock's current simulation conditions,
// append it, and return its row. No UI changes — callers decide what to reveal.
int LessonController::appendStructureFromAtoms(const QString& name,
    const QVector<MolAtom>& atoms)
{
    LessonStructure s;
    s.name = name.isEmpty()
        ? tr("Structure %1").arg(m_lesson.structures.size() + 1) : name;
    s.xyz = atomsToXyz(atoms, s.name);
    s.sim = m_simWidget ? m_simWidget->currentConfig() : SimulationConfig{};
    m_lesson.structures.push_back(s);
    return static_cast<int>(m_lesson.structures.size()) - 1;
}

// Capture the currently displayed structure as a new lesson entry. No dialogs: it
// is added with a default name, then selected so the user fills in name/notes/role
// in the inline detail editor.
void LessonController::addCurrentStructure(const QString& sourceFilePath)
{
    if (!m_viewer)
        return;
    const QVector<MolAtom> atoms = m_viewer->getCurrentFrameAtoms();
    if (atoms.isEmpty()) {
        emit statusMessage(tr("No structure to add"), 3000);
        return;
    }
    const QString defaultName = QFileInfo(sourceFilePath).completeBaseName();
    const int row = appendStructureFromAtoms(defaultName, atoms);

    refreshStructureView(/*autoShow=*/true);  // open the Lesson section + count
    if (m_lessonView && m_structureModel)
        m_lessonView->setCurrentIndex(m_structureModel->index(row, 0));
    showStructureDetails(row);
    if (m_structNameEdit) {
        m_structNameEdit->setFocus();
        m_structNameEdit->selectAll();  // ready to rename immediately
    }
    emit statusMessage(
        tr("Added structure — edit name/notes/role below, then Save Lesson"), 5000);
}

// Add a structure straight from the file browser (context menu) without loading it
// into the viewer. Stays in the current browser mode (just bumps the count).
void LessonController::addFile(const QString& filePath)
{
    const MoleculeFileLoader::Result r = MoleculeFileLoader::load(filePath);
    const QVector<MolAtom> atoms = r.frames.isEmpty()
        ? QVector<MolAtom>() : r.frames.first();
    if (atoms.isEmpty()) {
        emit statusMessage(
            tr("Could not read structure: %1").arg(QFileInfo(filePath).fileName()), 3000);
        return;
    }
    appendStructureFromAtoms(QFileInfo(filePath).completeBaseName(), atoms);
    refreshStructureView(/*autoShow=*/false);  // bump count
    emit statusMessage(
        tr("Added '%1' to the lesson (%2 structures).")
            .arg(QFileInfo(filePath).completeBaseName()).arg(m_lesson.structures.size()), 4000);
}

// Add several files at once (drag & drop). Filters to structure files, parses the
// first frame of each, and refreshes once.
void LessonController::addFiles(const QStringList& paths)
{
    int added = 0;
    for (const QString& path : paths) {
        const QString suf = QFileInfo(path).suffix().toLower();
        if (suf != QLatin1String("xyz") && suf != QLatin1String("vtf")
            && suf != QLatin1String("pdb") && suf != QLatin1String("mol2"))
            continue;
        const MoleculeFileLoader::Result r = MoleculeFileLoader::load(path);
        const QVector<MolAtom> atoms = r.frames.isEmpty()
            ? QVector<MolAtom>() : r.frames.first();
        if (atoms.isEmpty())
            continue;
        appendStructureFromAtoms(QFileInfo(path).completeBaseName(), atoms);
        ++added;
    }
    if (added > 0) {
        refreshStructureView(/*autoShow=*/false);
        emit statusMessage(
            tr("Added %1 structure(s) to lesson (%2 total)")
                .arg(added).arg(m_lesson.structures.size()), 4000);
    }
}

// Edit the lesson-level metadata (title, authors with ORCID/institution, ...).
void LessonController::editMetadata()
{
    LessonMetadataDialog dlg(m_lesson.meta, m_dialogParent);
    if (dlg.exec() == QDialog::Accepted) {
        m_lesson.meta = dlg.metadata();
        refreshMetaWidget();  // reflect changes in the inline widget
    }
}

// If the just-loaded file belongs to an unpacked lesson (a lesson.json sidecar in
// its directory references it by name), restore that structure's stored simulation
// conditions into the dock. No-op for ordinary files.
void LessonController::applyConditions(const QString& filePath)
{
    if (!m_simWidget)
        return;
    const QFileInfo fi(filePath);
    const QString sidecar = fi.absoluteDir().filePath(QStringLiteral("lesson.json"));
    if (!QFile::exists(sidecar))
        return;
    QFile f(sidecar);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;
    const Lesson lesson = lessonFromJson(doc.object());
    const QString fname = fi.fileName();
    for (const LessonStructure& s : lesson.structures) {
        if (s.file == fname) {
            m_simWidget->applyConfig(s.sim);
            QString msg = tr("Lesson conditions applied: %1").arg(s.name);
            if (!s.description.isEmpty())
                msg += QStringLiteral(" — ") + s.description;
            emit statusMessage(msg, 5000);
            return;
        }
    }
}

// Refresh the in-memory lesson-structure model and report the count (Lesson section
// title and visibility). With autoShow, ask for the section to be opened so the user
// sees a just-added structure.
void LessonController::refreshStructureView(bool autoShow)
{
    if (m_structureModel)
        m_structureModel->refresh();
    const int n = static_cast<int>(m_lesson.structures.size());
    emit structureCountChanged(n);
    if (autoShow && n > 0)
        emit revealRequested();
}

// Mirror the lesson-level metadata into the inline metadata widget.
void LessonController::refreshMetaWidget()
{
    if (m_titleEdit) m_titleEdit->setText(m_lesson.meta.title);
    if (m_descEdit) m_descEdit->setText(m_lesson.meta.description);
    if (m_authorsLabel) {
        QStringList names;
        for (const LessonAuthor& a : m_lesson.meta.authors)
            names << (a.name.isEmpty() ? a.orcid : a.name);
        m_authorsLabel->setText(names.isEmpty() ? tr("(none)") : names.join(QStringLiteral(", ")));
    }
}

// Populate the per-structure detail editor from structure @p row (or hide it when
// the row is invalid). The role combo is signal-blocked so populating it doesn't
// write back.
void LessonController::showStructureDetails(int row)
{
    m_currentRow = row;
    const bool valid = (row >= 0 && row < m_lesson.structures.size());
    if (m_structWidget)
        m_structWidget->setVisible(valid);
    if (!valid)
        return;
    const LessonStructure& s = m_lesson.structures.at(row);
    if (m_structNameEdit) m_structNameEdit->setText(s.name);     // setText: no textEdited
    if (m_structDescEdit) m_structDescEdit->setText(s.description);
    if (m_structRoleCombo) {
        QSignalBlocker blk(m_structRoleCombo);
        int idx = 0;
        if (s.role == QLatin1String("start")) idx = 1;
        else if (s.role == QLatin1String("intermediate")) idx = 2;
        else if (s.role == QLatin1String("target")) idx = 3;
        m_structRoleCombo->setCurrentIndex(idx);
    }
}

// Load an in-memory lesson structure: parse its embedded XYZ into the viewer and
// restore its stored simulation conditions. No working-directory switch and no
// source path (it is in-memory authored geometry), so a later Save prompts.
void LessonController::loadStructureFromIndex(const QModelIndex& index)
{
    if (!m_structureModel || !m_viewer)
        return;
    const LessonStructure* s = m_structureModel->at(index.row());
    if (!s)
        return;
    QVector<MolAtom> atoms;
    if (!xyzToAtoms(s->xyz, atoms)) {
        emit statusMessage(tr("Could not parse lesson structure '%1'").arg(s->name), 3000);
        return;
    }

    QVector<QVector<MolAtom>> allAtoms { atoms };
    QVector<QVector<MolBond>> allBonds;  // empty => viewer auto-detects bonds
    m_viewer->clearScenePublic();
    m_viewer->setTrajectoryData(allAtoms, allBonds);
    if (m_centerOnLoad)
        m_viewer->centerAtOrigin();

    if (m_simWidget) {
        m_simWidget->setMolecule(m_viewer->getCurrentFrameAtoms(),
            m_viewer->getCurrentFrameBonds());
        m_simWidget->applyConfig(s->sim);  // restore stored conditions
        m_simWidget->setStructureModified(false);
    }
    // MainWindow bookkeeping: clear file path, reset modified, enable Save, snapshot.
    emit inMemoryStructureLoaded(s->name);
    showStructureDetails(index.row());  // bind the inline detail editor to it
    emit statusMessage(tr("Loaded lesson structure: %1").arg(s->name), 4000);
}

// Remove the lesson structure at @p row (context-menu "Remove from Lesson").
void LessonController::removeStructure(int row)
{
    if (row < 0 || row >= m_lesson.structures.size())
        return;
    const QString name = m_lesson.structures.at(row).name;
    m_lesson.structures.remove(row);
    refreshStructureView();
    showStructureDetails(-1);  // rows shifted; reset the editor
    emit statusMessage(tr("Removed '%1' from lesson").arg(name), 3000);
}
