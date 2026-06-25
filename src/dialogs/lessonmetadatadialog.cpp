// lessonmetadatadialog.cpp - Editor for lesson-level metadata
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 - OER teaching scenarios for Qurcuma.

#include "lessonmetadatadialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

LessonMetadataDialog::LessonMetadataDialog(const LessonMetadata& meta, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Lesson Metadata"));
    resize(560, 460);

    auto* form = new QFormLayout;
    m_titleEdit = new QLineEdit(meta.title, this);
    m_descEdit = new QPlainTextEdit(meta.description, this);
    m_descEdit->setMaximumHeight(80);
    m_licenseEdit = new QLineEdit(meta.license, this);
    m_languageEdit = new QLineEdit(meta.language, this);
    m_keywordsEdit = new QLineEdit(meta.keywords.join(QStringLiteral(", ")), this);
    m_keywordsEdit->setPlaceholderText(tr("comma-separated, e.g. Ammoniak, Katalyse, GFN2"));

    form->addRow(tr("Title:"), m_titleEdit);
    form->addRow(tr("Description:"), m_descEdit);
    form->addRow(tr("License:"), m_licenseEdit);
    form->addRow(tr("Language:"), m_languageEdit);
    form->addRow(tr("Keywords:"), m_keywordsEdit);

    // Authors table
    m_authorsTable = new QTableWidget(0, 4, this);
    m_authorsTable->setHorizontalHeaderLabels(
        { tr("Name"), tr("ORCID"), tr("Institution"), tr("E-mail") });
    m_authorsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_authorsTable->verticalHeader()->setVisible(false);
    for (const LessonAuthor& a : meta.authors)
        addAuthorRow(a);

    auto* addBtn = new QPushButton(tr("Add Author"), this);
    auto* removeBtn = new QPushButton(tr("Remove Selected"), this);
    connect(addBtn, &QPushButton::clicked, this, [this]() { addAuthorRow(); });
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_authorsTable->currentRow();
        if (row >= 0)
            m_authorsTable->removeRow(row);
    });
    auto* authorBtns = new QHBoxLayout;
    authorBtns->addWidget(addBtn);
    authorBtns->addWidget(removeBtn);
    authorBtns->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_authorsTable, 1);
    layout->addLayout(authorBtns);
    layout->addWidget(buttons);
}

void LessonMetadataDialog::addAuthorRow(const LessonAuthor& a)
{
    const int row = m_authorsTable->rowCount();
    m_authorsTable->insertRow(row);
    m_authorsTable->setItem(row, 0, new QTableWidgetItem(a.name));
    m_authorsTable->setItem(row, 1, new QTableWidgetItem(a.orcid));
    m_authorsTable->setItem(row, 2, new QTableWidgetItem(a.institution));
    m_authorsTable->setItem(row, 3, new QTableWidgetItem(a.email));
}

LessonMetadata LessonMetadataDialog::metadata() const
{
    LessonMetadata meta;
    meta.title = m_titleEdit->text().trimmed();
    meta.description = m_descEdit->toPlainText().trimmed();
    meta.license = m_licenseEdit->text().trimmed();
    meta.language = m_languageEdit->text().trimmed();
    const QStringList kws = m_keywordsEdit->text().split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& k : kws)
        meta.keywords << k.trimmed();

    for (int r = 0; r < m_authorsTable->rowCount(); ++r) {
        auto cell = [&](int c) {
            const QTableWidgetItem* it = m_authorsTable->item(r, c);
            return it ? it->text().trimmed() : QString();
        };
        LessonAuthor a;
        a.name = cell(0);
        a.orcid = cell(1);
        a.institution = cell(2);
        a.email = cell(3);
        if (!a.name.isEmpty() || !a.orcid.isEmpty() || !a.institution.isEmpty() || !a.email.isEmpty())
            meta.authors.push_back(a);
    }
    return meta;
}
