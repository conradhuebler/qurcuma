// settingsdialog.cpp - Settings dialog: external program paths and operator metadata
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "settingsdialog.h"

#include "settings.h"
#include "widgets/themeicon.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>

using programpaths::Check;
using programpaths::Kind;
using programpaths::ProgramSpec;

namespace {

enum class Tone { Ok, Error, Neutral };

/// Puts a 16 px icon and one coloured line into a status pair (green / red / grey).
/// Claude Generated 2026
void showStatus(QLabel* icon, QLabel* label, Tone tone, const QString& text)
{
    struct Look { const char* theme; QStyle::StandardPixmap fallback; const char* colour; };
    static const Look looks[] = {
        { "dialog-ok-apply", QStyle::SP_DialogApplyButton, "#2e7d32" },
        { "dialog-error", QStyle::SP_MessageBoxCritical, "#c62828" },
        { "dialog-information", QStyle::SP_MessageBoxInformation, "#757575" } };
    const Look& l = looks[int(tone)];
    icon->setPixmap(themeIcon(QString::fromLatin1(l.theme), l.fallback).pixmap(16, 16));
    label->setText(QStringLiteral("<span style='color:%1'>%2</span>").arg(QLatin1String(l.colour), text.toHtmlEscaped()));
    label->setToolTip(text);
}

/// An icon label plus a text label in one row; the text is clipped instead of widening the dialog.
/// Claude Generated 2026
QHBoxLayout* makeStatusRow(QWidget* parent, QLabel*& icon, QLabel*& text)
{
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    icon = new QLabel(parent);
    icon->setFixedSize(16, 16);
    text = new QLabel(parent);
    text->setTextFormat(Qt::RichText);
    text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    row->addWidget(icon);
    row->addWidget(text, 1);
    return row;
}

} // namespace

SettingsDialog::SettingsDialog(Settings& settings, QWidget* parent, Tab tab)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(tr("Settings"));
    resize(720, 420);
    setMinimumWidth(640);

    auto* layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildProgramsTab(), themeIcon(QStringLiteral("application-x-executable"), QStyle::SP_FileIcon), tr("Programs"));
    m_tabs->addTab(buildOperatorTab(), themeIcon(QStringLiteral("user-identity"), QStyle::SP_DirHomeIcon), tr("Operator"));
    m_tabs->setTabToolTip(ProgramsTab, tr("Paths to the external programs qurcuma starts"));
    m_tabs->setTabToolTip(OperatorTab, tr("Author and license metadata embedded into exported images"));
    m_tabs->setCurrentIndex(tab);
    layout->addWidget(m_tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setToolTip(tr("Save the settings of both tabs and close"));
    buttons->button(QDialogButtonBox::Cancel)->setToolTip(tr("Close without saving any change"));
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons);
}

QWidget* SettingsDialog::buildProgramsTab()
{
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    auto* intro = new QLabel(tr("Programs that qurcuma starts as separate processes. Leave a field empty when the "
                                "program is not installed. Changes are saved with OK."), page);
    intro->setWordWrap(true);
    outer->addWidget(intro);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    const QIcon browseIcon = themeIcon(QStringLiteral("document-open"), QStyle::SP_DirOpenIcon);
    const QIcon detectIcon = themeIcon(QStringLiteral("edit-find"), QStyle::SP_FileDialogContentsView);

    for (const ProgramSpec& spec : programpaths::specs()) {
        const int i = m_rows.size();
        ProgramRow row;
        row.spec = spec;
        row.detected = programpaths::detect(spec);

        row.edit = new QLineEdit(QDir::toNativeSeparators(m_settings.getProgramPath(spec.name)), page);
        row.edit->setClearButtonEnabled(true);
        row.edit->setPlaceholderText(row.detected.isEmpty() ? tr("not set") : QDir::toNativeSeparators(row.detected));
        row.edit->setToolTip(spec.kind == Kind::Directory
                ? tr("%1\nThe path of the folder. A grey path is what Detect found; it is not used until you fill it in.").arg(spec.help)
                : tr("%1\nThe full path of the executable, or a bare name that is looked up in PATH. A grey path is what "
                     "Detect found; it is not used until you fill it in.").arg(spec.help));

        auto* label = new QLabel(spec.label + QLatin1Char(':'), page);
        label->setBuddy(row.edit);
        label->setToolTip(spec.help);

        auto* browseBtn = new QPushButton(browseIcon, tr("Browse..."), page);
        browseBtn->setToolTip(spec.kind == Kind::Directory ? tr("Choose the folder in a file dialog")
                                                           : tr("Choose the executable in a file dialog"));
        auto* detectBtn = new QPushButton(detectIcon, tr("Detect"), page);
        detectBtn->setToolTip(tr("Search PATH and the usual install folders for %1 and fill in the first match").arg(spec.label));

        QHBoxLayout* statusRow = makeStatusRow(page, row.icon, row.status);

        grid->addWidget(label, 2 * i, 0);
        grid->addWidget(row.edit, 2 * i, 1);
        grid->addWidget(browseBtn, 2 * i, 2);
        grid->addWidget(detectBtn, 2 * i, 3);
        grid->addLayout(statusRow, 2 * i + 1, 1, 1, 3);

        m_rows.append(row);
        connect(row.edit, &QLineEdit::textChanged, this, [this, i] { updateProgramStatus(i); });
        connect(browseBtn, &QPushButton::clicked, this, [this, i] { browse(i); });
        connect(detectBtn, &QPushButton::clicked, this, [this, i] { detect(i); });
        updateProgramStatus(i);
    }
    outer->addLayout(grid);
    outer->addStretch(1);
    return page;
}

QWidget* SettingsDialog::buildOperatorTab()
{
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    auto* intro = new QLabel(tr("Embedded as author and license metadata into exported images. All fields are "
                                "optional. Changes are saved with OK."), page);
    intro->setWordWrap(true);
    outer->addWidget(intro);

    auto* form = new QFormLayout;
    m_name = new QLineEdit(m_settings.operatorName(), page);
    m_name->setPlaceholderText(tr("Your name"));
    m_name->setClearButtonEnabled(true);
    m_name->setToolTip(tr("Author name written into exported images"));
    form->addRow(tr("Name:"), m_name);

    m_orcid = new QLineEdit(m_settings.operatorOrcid(), page);
    m_orcid->setPlaceholderText(QStringLiteral("0000-0000-0000-0000"));
    m_orcid->setClearButtonEnabled(true);
    m_orcid->setToolTip(tr("ORCID iD in the form 0000-0000-0000-000X. Format and check digit are checked; "
                           "whether the iD is registered is not."));
    form->addRow(tr("ORCID:"), m_orcid);
    auto* orcidStatus = new QWidget(page);
    orcidStatus->setLayout(makeStatusRow(orcidStatus, m_orcidIcon, m_orcidStatus));
    form->addRow(QString(), orcidStatus);

    m_institution = new QLineEdit(m_settings.operatorInstitution(), page);
    m_institution->setPlaceholderText(tr("Institution / affiliation"));
    m_institution->setClearButtonEnabled(true);
    m_institution->setToolTip(tr("Affiliation written into exported images"));
    form->addRow(tr("Institution:"), m_institution);

    m_license = new QComboBox(page);
    m_license->setEditable(true);
    m_license->addItems({ QStringLiteral("CC-BY-4.0"), QStringLiteral("CC0-1.0"), QStringLiteral("CC-BY-SA-4.0"),
        QStringLiteral("CC-BY-ND-4.0"), QStringLiteral("CC-BY-NC-4.0") });
    const QString currentLicense = m_settings.operatorLicense();
    const int licIdx = m_license->findText(currentLicense);
    if (licIdx >= 0)
        m_license->setCurrentIndex(licIdx);
    else if (!currentLicense.isEmpty())
        m_license->setEditText(currentLicense);
    m_license->setToolTip(tr("Default license embedded into exported images. Pick one or type an SPDX identifier."));
    form->addRow(tr("License:"), m_license);

    outer->addLayout(form);
    outer->addStretch(1);

    connect(m_orcid, &QLineEdit::textChanged, this, &SettingsDialog::updateOrcidStatus);
    updateOrcidStatus();
    return page;
}

void SettingsDialog::browse(int i)
{
    ProgramRow& r = m_rows[i];
    QString start = QDir::homePath();
    const QFileInfo current(r.edit->text().trimmed());
    if (!r.edit->text().trimmed().isEmpty()) {
        const QString dir = current.isDir() ? current.absoluteFilePath() : current.absolutePath();
        if (QFileInfo(dir).isDir())
            start = dir;
    }
    const QString chosen = r.spec.kind == Kind::Directory
        ? QFileDialog::getExistingDirectory(this, tr("Select the %1").arg(r.spec.label), start)
        : QFileDialog::getOpenFileName(this, tr("Select the %1 executable").arg(r.spec.label), start);
    if (!chosen.isEmpty())
        r.edit->setText(QDir::toNativeSeparators(chosen));
}

void SettingsDialog::detect(int i)
{
    ProgramRow& r = m_rows[i];
    const QString found = programpaths::detect(r.spec);
    if (found.isEmpty()) {
        showStatus(r.icon, r.status, Tone::Error, tr("Not found in PATH or the usual install folders"));
        return;
    }
    r.detected = found;
    r.edit->setText(QDir::toNativeSeparators(found));
    updateProgramStatus(i);  // also when the text did not change
}

void SettingsDialog::updateProgramStatus(int i)
{
    const ProgramRow& r = m_rows[i];
    const Check check = programpaths::validate(r.spec, r.edit->text());
    if (check.state == Check::Ok) {
        showStatus(r.icon, r.status, Tone::Ok, check.text);
    } else if (check.state == Check::NotSet) {
        showStatus(r.icon, r.status, Tone::Neutral,
            r.detected.isEmpty() ? tr("Not set; Detect found nothing when the dialog opened")
                                 : tr("Not set; Detect fills in the grey path shown in the field"));
    } else {
        showStatus(r.icon, r.status, Tone::Error, check.text);
    }
}

void SettingsDialog::updateOrcidStatus()
{
    switch (programpaths::checkOrcid(m_orcid->text())) {
    case programpaths::OrcidState::Empty:
        showStatus(m_orcidIcon, m_orcidStatus, Tone::Neutral, tr("Optional"));
        break;
    case programpaths::OrcidState::BadFormat:
        showStatus(m_orcidIcon, m_orcidStatus, Tone::Error,
            tr("Expected 0000-0000-0000-000X: four groups of four digits, the last character may be X"));
        break;
    case programpaths::OrcidState::BadChecksum:
        showStatus(m_orcidIcon, m_orcidStatus, Tone::Error, tr("The check digit does not match; probably a typo"));
        break;
    case programpaths::OrcidState::Ok:
        showStatus(m_orcidIcon, m_orcidStatus, Tone::Ok, tr("Format and check digit are valid"));
        break;
    }
}

void SettingsDialog::accept()
{
    for (const ProgramRow& r : m_rows)
        m_settings.setProgramPath(r.spec.name, QDir::fromNativeSeparators(r.edit->text().trimmed()));
    m_settings.setOperatorName(m_name->text().trimmed());
    m_settings.setOperatorOrcid(m_orcid->text().trimmed());
    m_settings.setOperatorInstitution(m_institution->text().trimmed());
    m_settings.setOperatorLicense(m_license->currentText().trimmed());
    QDialog::accept();
}
