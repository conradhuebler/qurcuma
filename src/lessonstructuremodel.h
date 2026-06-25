// lessonstructuremodel.h - Read-only list model over a lesson's in-memory structures
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - OER teaching scenarios for Qurcuma.
//
// Backs the optional "Lesson structures" list in the Project dock so the
// in-memory (being-authored or just-opened) lesson is visible and clickable,
// independent of the on-disk file browser. The model holds a pointer to the
// lesson's structure vector (owned by MainWindow::m_lesson) and is refreshed
// explicitly after structures are added/opened.

#pragma once

#include "lesson.h"

#include <QAbstractListModel>

class LessonStructureModel : public QAbstractListModel {
    Q_OBJECT
public:
    explicit LessonStructureModel(const QVector<LessonStructure>* structures,
        QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    /** @brief Re-read the backing vector (begin/endResetModel). Call after the
     *  lesson's structures change. */
    void refresh();

    /** @brief Structure at @p row, or nullptr if out of range. */
    const LessonStructure* at(int row) const;

private:
    const QVector<LessonStructure>* m_structures;  // not owned
};
