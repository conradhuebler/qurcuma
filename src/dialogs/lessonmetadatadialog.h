// lessonmetadatadialog.h - Editor for lesson-level metadata (title, authors, ...)
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - OER teaching scenarios for Qurcuma.

#pragma once

#include "../lesson.h"

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;
class QTableWidget;

/**
 * @brief Modal editor for LessonMetadata: title/description/license/language/
 * keywords plus an authors table (name / ORCID / institution / e-mail).
 */
class LessonMetadataDialog : public QDialog {
    Q_OBJECT
public:
    explicit LessonMetadataDialog(const LessonMetadata& meta, QWidget* parent = nullptr);

    /** @brief Metadata as edited by the user (read after exec() == Accepted). */
    LessonMetadata metadata() const;

private:
    void addAuthorRow(const LessonAuthor& a = {});

    QLineEdit* m_titleEdit = nullptr;
    QPlainTextEdit* m_descEdit = nullptr;
    QLineEdit* m_licenseEdit = nullptr;
    QLineEdit* m_languageEdit = nullptr;
    QLineEdit* m_keywordsEdit = nullptr;   // comma-separated
    QTableWidget* m_authorsTable = nullptr;
};
