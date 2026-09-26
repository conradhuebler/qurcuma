// lessoncontroller.h - OER-lesson feature controller (extracted from MainWindow).
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - WP T4. Owns the authored Lesson (data + open/save/add/
// apply-conditions workflow), the in-memory LessonStructureModel and the
// Files|Lesson browse mode. The lesson *model* (Lesson/LessonStructure + JSON) lives
// in lesson.h; this class is the workflow that binds it to the viewer, the
// simulation dock and the ProjectDock lesson widgets. Because that workflow is
// genuinely host-coupled, the controller holds its collaborators by pointer
// (injected once after the docks are built) and emits signals for the few actions
// that remain MainWindow's job (switch working dir, window title, dir refresh,
// status bar, post-load bookkeeping).

#pragma once

#include "lesson.h"  // Lesson, LessonStructure, atomsToXyz/xyzToAtoms + MoleculeViewer::Atom

#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class MoleculeViewer;
class SimulationControlWidget;
class DockManager;
class LessonStructureModel;
class QAbstractItemModel;
class QListView;
class QToolButton;
class QLineEdit;
class QLabel;
class QComboBox;
class QWidget;

class LessonController : public QObject {
    Q_OBJECT
public:
    /// @p dialogParent parents the QFileDialog/QMessageBox popups (the MainWindow).
    explicit LessonController(QWidget* dialogParent, QObject* parent = nullptr);

    // --- Collaborator injection (call once, after ProjectDock is built) ---------
    void setViewer(MoleculeViewer* v) { m_viewer = v; }
    void setSimulationWidget(SimulationControlWidget* w) { m_simWidget = w; }
    /// Saving a lesson records the open panels (restored via lessonOpened). Claude Generated 2026.
    void setDockManager(DockManager* d) { m_dockManager = d; }
    /// @p filesModel is the model to restore when leaving Lesson mode (proxy or plain).
    void setContentView(QListView* view, QAbstractItemModel* filesModel);
    void setModeButtons(QToolButton* filesBtn, QToolButton* lessonBtn);
    void setMetaWidgets(QWidget* metaWidget, QLineEdit* title, QLineEdit* desc, QLabel* authors);
    void setStructWidgets(QWidget* structWidget, QLineEdit* name, QLineEdit* desc, QComboBox* role);
    /// Wire the metadata/per-structure editor lambdas and build the structure model.
    /// Requires the widget setters + setContentView to have run first.
    void wireWidgetConnections();

    // --- Cached MainWindow config (kept in sync via these setters) --------------
    void setWorkingDirectory(const QString& dir) { m_workingDirectory = dir; }
    void setCenterOnLoad(bool on) { m_centerOnLoad = on; }

    // --- Accessors --------------------------------------------------------------
    LessonStructureModel* structureModel() const { return m_structureModel; }
    bool browseMode() const { return m_browseMode; }
    int structureCount() const { return static_cast<int>(m_lesson.structures.size()); }

    // --- Workflow (moved verbatim from MainWindow) ------------------------------
    void openLesson(const QString& path);
    void saveLesson(const QString& path);
    bool saveLessonInteractive(bool forceDialog);
    /// @p sourceFilePath supplies the default structure name (the loaded file).
    void addCurrentStructure(const QString& sourceFilePath);
    void addFile(const QString& filePath);            // single (context menu)
    void addFiles(const QStringList& paths);          // batch (drag & drop)
    void editMetadata();
    void applyConditions(const QString& filePath);
    void setBrowserMode(bool lessonMode);
    void refreshStructureView(bool autoShow = false);
    void loadStructureFromIndex(const QModelIndex& index);
    void removeStructure(int row);

signals:
    void workingDirectoryChangeRequested(const QString& dir);
    void windowTitleChangeRequested(const QString& title);
    void directoryContentRefreshRequested();
    void statusMessage(const QString& msg, int timeoutMs);
    /// An in-memory lesson structure was loaded into the viewer: MainWindow clears
    /// the current file path, resets modified state, enables Save and snapshots it.
    void inMemoryStructureLoaded(const QString& name);
    /// A lesson file was opened and unpacked. MainWindow switches to Teaching (if the
    /// preference is on) and then opens @p panels, the panels the lesson was saved with
    /// (empty for lessons that store none). Claude Generated 2026.
    void lessonOpened(const QStringList& panels);

private:
    int appendStructureFromAtoms(const QString& name, const QVector<MoleculeViewer::Atom>& atoms);
    void refreshMetaWidget();
    void showStructureDetails(int row);

    // owned lesson state
    Lesson m_lesson;
    QString m_lessonFilePath;
    int m_currentRow = -1;
    bool m_browseMode = false;
    LessonStructureModel* m_structureModel = nullptr;

    // injected collaborators (not owned)
    QWidget* m_dialogParent = nullptr;
    MoleculeViewer* m_viewer = nullptr;
    SimulationControlWidget* m_simWidget = nullptr;
    DockManager* m_dockManager = nullptr;
    QListView* m_dirView = nullptr;
    QAbstractItemModel* m_filesModel = nullptr;
    QToolButton* m_filesModeBtn = nullptr;
    QToolButton* m_lessonModeBtn = nullptr;
    QWidget* m_metaWidget = nullptr;
    QLineEdit* m_titleEdit = nullptr;
    QLineEdit* m_descEdit = nullptr;
    QLabel* m_authorsLabel = nullptr;
    QWidget* m_structWidget = nullptr;
    QLineEdit* m_structNameEdit = nullptr;
    QLineEdit* m_structDescEdit = nullptr;
    QComboBox* m_structRoleCombo = nullptr;

    // cached MainWindow config
    QString m_workingDirectory;
    bool m_centerOnLoad = true;
};
