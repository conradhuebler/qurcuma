// snapshotswidget.h - Manual snapshot history for molecular geometries
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - Snapshot history foundation

#pragma once

#include "view.h"

#include <QDateTime>
#include <QListWidget>
#include <QToolButton>
#include <QVector>
#include <QWidget>

/**
 * @brief Snapshot entry: a named, timestamped copy of atoms and bonds.
 *
 * Claude Generated 2026 - Snapshot history foundation. Each snapshot is an
 * independent, in-memory geometry copy that can be restored to the viewer and
 * simulation dock without touching the source file.
 */
struct MoleculeSnapshot {
    QString name;
    QDateTime timestamp;
    QVector<MoleculeViewer::Atom> atoms;
    QVector<MoleculeViewer::Bond> bonds;
};

/**
 * @brief Dock widget showing the snapshot history and controls.
 *
 * Claude Generated 2026 - Snapshot history foundation. Policy is intentionally
 * manual: the user takes snapshots explicitly and restores any selected one.
 * An automatic limit/aging policy is reserved for future work.
 */
class SnapshotsWidget : public QWidget {
    Q_OBJECT

public:
    explicit SnapshotsWidget(QWidget* parent = nullptr);
    ~SnapshotsWidget() override;

    /** Add a snapshot and select it in the list. */
    void addSnapshot(const MoleculeSnapshot& snapshot);

    /** Replace the displayed list (used when loading a new molecule). */
    void clearSnapshots();

    /** Number of stored snapshots. */
    int count() const;

    /** Access snapshot by row index. */
    MoleculeSnapshot snapshotAt(int index) const;

signals:
    /** User clicked the "Take Snapshot" button. The receiver should call
     *  takeSnapshot() with the current viewer geometry. */
    void takeSnapshotRequested();

    /** User wants to restore the selected snapshot. */
    void restoreSnapshotRequested(int index);

    /** User wants to delete the selected snapshot. */
    void deleteSnapshotRequested(int index);

private slots:
    void onTakeClicked();
    void onRestoreClicked();
    void onDeleteClicked();
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void setupUI();
    void updateButtonStates();

    QListWidget* m_listWidget = nullptr;
    QToolButton* m_takeBtn = nullptr;
    QToolButton* m_restoreBtn = nullptr;
    QToolButton* m_deleteBtn = nullptr;
    QVector<MoleculeSnapshot> m_snapshots;
};
