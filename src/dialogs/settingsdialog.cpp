// settingsdialog.cpp - Settings dialog: programs, general, display, computers, operator metadata
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "settingsdialog.h"

#include "settings.h"
#include "widgets/themeicon.h"

#ifdef QURCUMA_REMOTE
#include "dialogs/addcomputerdialog.h"
#include "remote/hostprofiles.h"

#include <QHeaderView>
#include <QSettings>
#include <QTreeWidget>
#endif

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
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

/// The word-wrapped intro line at the top of a tab. Claude Generated 2026
QLabel* makeIntro(QWidget* page, const QString& text)
{
    auto* intro = new QLabel(text, page);
    intro->setWordWrap(true);
    return intro;
}

} // namespace

SettingsDialog::SettingsDialog(Settings& settings, QWidget* parent, Tab tab)
    : QDialog(parent)
    , m_settings(settings)
    , m_currentWorkDir(settings.lastUsedWorkingDirectory())
{
    setWindowTitle(tr("Settings"));
    setMinimumWidth(640);

    auto* layout = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);
    m_pages[ProgramsTab] = buildProgramsTab();
    m_pages[GeneralTab] = buildGeneralTab();
    m_pages[DisplayTab] = buildDisplayTab();
#ifdef QURCUMA_REMOTE
    m_pages[ComputersTab] = buildComputersTab();
#endif
    m_pages[OperatorTab] = buildOperatorTab();

    struct TabLook { Tab tab; const char* theme; QStyle::StandardPixmap fallback; QString title; QString tip; };
    const TabLook looks[] = {
        { ProgramsTab, "application-x-executable", QStyle::SP_FileIcon, tr("Programs"),
          tr("Paths to the external programs qurcuma starts") },
        { GeneralTab, "preferences-system", QStyle::SP_ComputerIcon, tr("General"),
          tr("Working directory, dark mode and the recent-files list") },
        { DisplayTab, "video-display", QStyle::SP_DesktopIcon, tr("Display"),
          tr("How the viewer places a structure when it is loaded") },
        { ComputersTab, "network-server", QStyle::SP_DriveNetIcon, tr("Computers"),
          tr("Computers for remote runs (Simulation panel, Compute on)") },
        { OperatorTab, "user-identity", QStyle::SP_DirHomeIcon, tr("Operator"),
          tr("Author and license metadata embedded into exported images") } };
    for (const TabLook& l : looks) {
        if (!m_pages[l.tab])
            continue;
        const int index = m_tabs->addTab(m_pages[l.tab], themeIcon(QString::fromLatin1(l.theme), l.fallback), l.title);
        m_tabs->setTabToolTip(index, l.tip);
    }
    const int wanted = static_cast<int>(tab);
    const bool known = wanted >= 0 && wanted < TabCount && m_pages[wanted];
    m_tabs->setCurrentWidget(known ? m_pages[wanted] : m_pages[ProgramsTab]);
    layout->addWidget(m_tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setToolTip(tr("Save and apply the changes of all tabs, then close"));
#ifdef QURCUMA_REMOTE
    buttons->button(QDialogButtonBox::Cancel)->setToolTip(
        tr("Close without saving the changes; computers added, edited or removed are already saved"));
#else
    buttons->button(QDialogButtonBox::Cancel)->setToolTip(tr("Close without saving any change"));
#endif
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    layout->addWidget(buttons);

    updateWorkingDirectoryView();
    resize(QSize(720, 420).expandedTo(sizeHint()));
}

QWidget* SettingsDialog::buildProgramsTab()
{
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    outer->addWidget(makeIntro(page, tr("Programs that qurcuma starts as separate processes. Leave a field empty when the "
                                        "program is not installed. Changes are saved with OK.")));

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

QWidget* SettingsDialog::buildGeneralTab()
{
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    outer->addWidget(makeIntro(page, tr("Where qurcuma works and how it starts. Changes are applied with OK.")));

    // Working directory: the one in use, the known ones, the invocation-directory switch.
    auto* dirGroup = new QGroupBox(tr("Working directory"), page);
    auto* dirGrid = new QGridLayout(dirGroup);
    dirGrid->setColumnStretch(1, 1);
    m_workDirEdit = new QLineEdit(dirGroup);
    m_workDirEdit->setReadOnly(true);
    m_workDirEdit->setToolTip(tr("The folder shown in the Project panel"));
    auto* dirLabel = new QLabel(tr("Directory:"), dirGroup);
    dirLabel->setBuddy(m_workDirEdit);
    dirLabel->setToolTip(m_workDirEdit->toolTip());
    auto* browseBtn = new QPushButton(themeIcon(QStringLiteral("document-open"), QStyle::SP_DirOpenIcon), tr("Browse..."), dirGroup);
    browseBtn->setToolTip(tr("Choose another working directory in a folder dialog; qurcuma switches to it when you press OK"));
    m_workDirNote = new QLabel(dirGroup);
    m_workDirNote->setWordWrap(true);
    m_workDirNote->setToolTip(tr("What happens to the working directory when you press OK"));

    auto* listLabel = new QLabel(tr("Known directories:"), dirGroup);
    m_workDirList = new QListWidget(dirGroup);
    m_workDirList->setToolTip(tr("Directories that were set as working directory (with Browse here or with Set as Working "
                                 "Directory in the Project panel). Double-click one to use it."));
    listLabel->setBuddy(m_workDirList);
    listLabel->setToolTip(m_workDirList->toolTip());
    const QColor missingColour = m_workDirList->palette().color(QPalette::Disabled, QPalette::Text);
    for (const QString& dir : m_settings.workingDirectories()) {
        auto* item = new QListWidgetItem(QDir::toNativeSeparators(dir), m_workDirList);
        item->setData(Qt::UserRole, dir);
        if (QFileInfo(dir).isDir()) {
            item->setToolTip(QDir::toNativeSeparators(dir));
        } else {
            item->setForeground(missingColour);
            item->setToolTip(tr("%1\nThis folder does not exist").arg(QDir::toNativeSeparators(dir)));
        }
    }
    m_useWorkDirBtn = new QPushButton(themeIcon(QStringLiteral("go-jump"), QStyle::SP_ArrowRight), tr("Use"), dirGroup);
    m_useWorkDirBtn->setToolTip(tr("Make the selected directory the working directory when you press OK"));
    m_removeWorkDirBtn = new QPushButton(themeIcon(QStringLiteral("list-remove"), QStyle::SP_TrashIcon), tr("Remove"), dirGroup);
    m_removeWorkDirBtn->setToolTip(tr("Remove the selected directory from this list when you press OK; the folder itself is not touched"));
    auto* listButtons = new QVBoxLayout;
    listButtons->addWidget(m_useWorkDirBtn);
    listButtons->addWidget(m_removeWorkDirBtn);
    listButtons->addStretch(1);

    m_useInvocationDirCheck = new QCheckBox(tr("Use the directory qurcuma was started from as working directory"), dirGroup);
    m_useInvocationDirCheck->setChecked(m_settings.useInvocationDirectoryEnabled());
    m_useInvocationDirCheck->setToolTip(tr("When on, the directory qurcuma was launched from becomes the working directory at "
                                           "every start, and at once when you press OK."));

    dirGrid->addWidget(dirLabel, 0, 0);
    dirGrid->addWidget(m_workDirEdit, 0, 1);
    dirGrid->addWidget(browseBtn, 0, 2);
    dirGrid->addWidget(m_workDirNote, 1, 1, 1, 2);
    dirGrid->addWidget(listLabel, 2, 0, Qt::AlignTop);
    dirGrid->addWidget(m_workDirList, 2, 1);
    dirGrid->addLayout(listButtons, 2, 2);
    dirGrid->addWidget(m_useInvocationDirCheck, 3, 0, 1, 3);
    dirGrid->setRowStretch(2, 1);
    outer->addWidget(dirGroup, 1);

    auto* lookGroup = new QGroupBox(tr("Appearance"), page);
    auto* lookLayout = new QVBoxLayout(lookGroup);
    m_darkModeCheck = new QCheckBox(tr("Dark mode"), lookGroup);
    m_darkModeCheck->setChecked(m_settings.darkModeEnabled());
    m_darkModeCheck->setToolTip(tr("Stored as a preference only: the window colours follow the system theme, "
                                   "qurcuma applies no theme of its own."));
    lookLayout->addWidget(m_darkModeCheck);
    outer->addWidget(lookGroup);

    auto* recentGroup = new QGroupBox(tr("Recent files"), page);
    auto* recentLayout = new QHBoxLayout(recentGroup);
    m_recentLabel = new QLabel(recentGroup);
    m_recentLabel->setToolTip(tr("Entries in File > Recent Files"));
    m_clearRecentBtn = new QPushButton(themeIcon(QStringLiteral("edit-clear-history"), QStyle::SP_DialogResetButton),
        tr("Clear list"), recentGroup);
    m_clearRecentBtn->setToolTip(tr("Empty File > Recent Files when you press OK; the files themselves are not touched"));
    recentLayout->addWidget(m_recentLabel, 1);
    recentLayout->addWidget(m_clearRecentBtn);
    const int recentCount = m_settings.recentFilesV2().size();
    if (recentCount == 0)
        m_recentLabel->setText(tr("File > Recent Files is empty"));
    else if (recentCount == 1)
        m_recentLabel->setText(tr("1 entry in File > Recent Files"));
    else
        m_recentLabel->setText(tr("%1 entries in File > Recent Files").arg(recentCount));
    m_clearRecentBtn->setEnabled(recentCount > 0);
    outer->addWidget(recentGroup);

    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::browseWorkingDirectory);
    connect(m_useWorkDirBtn, &QPushButton::clicked, this, &SettingsDialog::useSelectedWorkingDirectory);
    connect(m_removeWorkDirBtn, &QPushButton::clicked, this, &SettingsDialog::removeSelectedWorkingDirectory);
    connect(m_workDirList, &QListWidget::itemDoubleClicked, this, &SettingsDialog::useSelectedWorkingDirectory);
    connect(m_workDirList, &QListWidget::currentRowChanged, this, &SettingsDialog::updateWorkingDirectoryView);
    connect(m_clearRecentBtn, &QPushButton::clicked, this, &SettingsDialog::clearRecentFiles);
    return page;
}

QWidget* SettingsDialog::buildDisplayTab()
{
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    outer->addWidget(makeIntro(page, tr("How the viewer places a structure when it is loaded. Changes are applied with OK.")));

    m_centerOnLoadCheck = new QCheckBox(tr("Move the structure's center of mass to the origin after loading"), page);
    m_centerOnLoadCheck->setChecked(m_settings.getVisualizationSettings().centerOnLoad);
    m_centerOnLoadCheck->setToolTip(tr("When a file or a lesson structure is opened, every frame is shifted so that its "
                                       "mass-weighted center of mass lies at the origin."));
    outer->addWidget(m_centerOnLoadCheck);
    outer->addStretch(1);
    return page;
}

#ifdef QURCUMA_REMOTE
QWidget* SettingsDialog::buildComputersTab()
{
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    outer->addWidget(makeIntro(page, tr("Computers for remote runs, offered under Compute on in the Simulation panel. Add, "
                                        "Edit and Remove take effect at once and are not undone by Cancel.")));

    auto* row = new QHBoxLayout;
    m_computerTree = new QTreeWidget(page);
    m_computerTree->setColumnCount(3);
    m_computerTree->setHeaderLabels({ tr("Name"), tr("Address"), tr("Key file") });
    m_computerTree->setRootIsDecorated(false);
    m_computerTree->setAllColumnsShowFocus(true);
    m_computerTree->header()->setStretchLastSection(true);
    m_computerTree->setToolTip(tr("Computers saved in qurcuma (~/.ssh/config is not read). Double-click one to edit it."));
    row->addWidget(m_computerTree, 1);

    auto* addBtn = new QPushButton(themeIcon(QStringLiteral("list-add"), QStyle::SP_FileDialogNewFolder), tr("Add..."), page);
    addBtn->setToolTip(tr("Add a computer: address, port, key, and a test of the connection and of qurcuma-server"));
    m_editComputerBtn = new QPushButton(themeIcon(QStringLiteral("document-edit"), QStyle::SP_FileDialogContentsView),
        tr("Edit..."), page);
    m_editComputerBtn->setToolTip(tr("Change the selected computer and test it again"));
    m_removeComputerBtn = new QPushButton(themeIcon(QStringLiteral("list-remove"), QStyle::SP_TrashIcon), tr("Remove"), page);
    m_removeComputerBtn->setToolTip(tr("Remove the selected computer from the list"));
    auto* buttons = new QVBoxLayout;
    buttons->addWidget(addBtn);
    buttons->addWidget(m_editComputerBtn);
    buttons->addWidget(m_removeComputerBtn);
    buttons->addStretch(1);
    row->addLayout(buttons);
    outer->addLayout(row, 1);

    connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::addComputer);
    connect(m_editComputerBtn, &QPushButton::clicked, this, &SettingsDialog::editComputer);
    connect(m_removeComputerBtn, &QPushButton::clicked, this, &SettingsDialog::removeComputer);
    connect(m_computerTree, &QTreeWidget::itemDoubleClicked, this, &SettingsDialog::editComputer);
    connect(m_computerTree, &QTreeWidget::currentItemChanged, this, [this] {
        const bool selected = m_computerTree->currentItem() != nullptr;
        m_editComputerBtn->setEnabled(selected);
        m_removeComputerBtn->setEnabled(selected);
    });
    fillComputerList();
    return page;
}
#endif

QWidget* SettingsDialog::buildOperatorTab()
{
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    outer->addWidget(makeIntro(page, tr("Embedded as author and license metadata into exported images. All fields are "
                                        "optional. Changes are saved with OK.")));

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

void SettingsDialog::setCurrentWorkingDirectory(const QString& path)
{
    m_currentWorkDir = path;
    if (m_chosenWorkDir == path)
        m_chosenWorkDir.clear();
    updateWorkingDirectoryView();
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

void SettingsDialog::browseWorkingDirectory()
{
    const QString shown = m_chosenWorkDir.isEmpty() ? m_currentWorkDir : m_chosenWorkDir;
    const QString start = QFileInfo(shown).isDir() ? shown : QDir::homePath();
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Select the working directory"), start);
    if (chosen.isEmpty())
        return;
    m_chosenWorkDir = QDir::fromNativeSeparators(chosen) == m_currentWorkDir ? QString() : QDir::fromNativeSeparators(chosen);
    updateWorkingDirectoryView();
}

void SettingsDialog::useSelectedWorkingDirectory()
{
    const QListWidgetItem* item = m_workDirList->currentItem();
    if (!item)
        return;
    const QString dir = item->data(Qt::UserRole).toString();
    if (!QFileInfo(dir).isDir())
        return;
    m_chosenWorkDir = dir == m_currentWorkDir ? QString() : dir;
    updateWorkingDirectoryView();
}

void SettingsDialog::removeSelectedWorkingDirectory()
{
    const int row = m_workDirList->currentRow();
    if (row < 0)
        return;
    QListWidgetItem* item = m_workDirList->takeItem(row);
    m_removedWorkDirs.append(item->data(Qt::UserRole).toString());
    delete item;
    updateWorkingDirectoryView();
}

void SettingsDialog::updateWorkingDirectoryView()
{
    if (!m_workDirEdit)
        return;
    const bool changing = !m_chosenWorkDir.isEmpty();
    m_workDirEdit->setText(QDir::toNativeSeparators(changing ? m_chosenWorkDir : m_currentWorkDir));
    QString note;
    if (changing)
        note = tr("qurcuma switches to this directory when you press OK (now: %1).").arg(QDir::toNativeSeparators(m_currentWorkDir));
    if (!m_removedWorkDirs.isEmpty()) {
        const QString removed = m_removedWorkDirs.size() == 1 ? tr("1 directory is removed from the list when you press OK.")
                                                              : tr("%1 directories are removed from the list when you press OK.")
                                                                    .arg(m_removedWorkDirs.size());
        note = note.isEmpty() ? removed : note + QLatin1Char(' ') + removed;
    }
    m_workDirNote->setText(note);
    m_workDirNote->setVisible(!note.isEmpty());

    const QListWidgetItem* item = m_workDirList->currentItem();
    m_useWorkDirBtn->setEnabled(item && QFileInfo(item->data(Qt::UserRole).toString()).isDir());
    m_removeWorkDirBtn->setEnabled(item != nullptr);
}

void SettingsDialog::clearRecentFiles()
{
    const int count = m_settings.recentFilesV2().size();
    if (count == 0 || m_clearRecent)
        return;
    const QString question = count == 1 ? tr("Remove the 1 entry from File > Recent Files?")
                                        : tr("Remove all %1 entries from File > Recent Files?").arg(count);
    if (QMessageBox::question(this, tr("Clear recent files"),
            question + QLatin1Char('\n') + tr("The list is cleared when you press OK in the Settings dialog."))
        != QMessageBox::Yes)
        return;
    m_clearRecent = true;
    m_recentLabel->setText(tr("File > Recent Files is cleared when you press OK"));
    m_clearRecentBtn->setEnabled(false);
}

#ifdef QURCUMA_REMOTE
void SettingsDialog::fillComputerList(const QString& select)
{
    m_computerTree->clear();
    QTreeWidgetItem* selected = nullptr;
    for (const remote::HostProfile& p : remote::HostProfiles::load()) {
        const QString address = p.port > 0 ? QStringLiteral("%1:%2").arg(p.target).arg(p.port) : p.target;
        const QString key = p.identityFile.isEmpty() ? tr("ssh agent / default keys") : QDir::toNativeSeparators(p.identityFile);
        auto* item = new QTreeWidgetItem(m_computerTree, { p.name, address, key });
        item->setData(0, Qt::UserRole, p.name);
        item->setToolTip(0, tr("Server command: %1").arg(p.effectiveServerCommand()));
        item->setToolTip(1, p.port > 0 ? tr("ssh address and port") : tr("ssh address (default port)"));
        item->setToolTip(2, key);
        if (p.name == select)
            selected = item;
    }
    m_computerTree->resizeColumnToContents(0);
    m_computerTree->resizeColumnToContents(1);
    m_computerTree->setCurrentItem(selected);
    m_editComputerBtn->setEnabled(selected != nullptr);
    m_removeComputerBtn->setEnabled(selected != nullptr);
}

void SettingsDialog::addComputer()
{
    AddComputerDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const remote::HostProfile p = dialog.profile();
    // as in the Simulation panel: the server command found by the test is the one stored per computer
    QSettings().setValue(QStringLiteral("remote/serverCommand/") + p.name, p.effectiveServerCommand());
    fillComputerList(p.name);
    emit computersChanged();
}

void SettingsDialog::editComputer()
{
    const QTreeWidgetItem* item = m_computerTree->currentItem();
    if (!item)
        return;
    const auto current = remote::HostProfiles::find(item->data(0, Qt::UserRole).toString());
    if (!current)
        return;
    AddComputerDialog dialog(this, current);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const remote::HostProfile p = dialog.profile();
    QSettings().setValue(QStringLiteral("remote/serverCommand/") + p.name, p.effectiveServerCommand());
    fillComputerList(p.name);
    emit computersChanged();
}

void SettingsDialog::removeComputer()
{
    const QTreeWidgetItem* item = m_computerTree->currentItem();
    if (!item)
        return;
    const QString name = item->data(0, Qt::UserRole).toString();
    if (QMessageBox::question(this, tr("Remove computer"), tr("Remove '%1' from the list?").arg(name)) != QMessageBox::Yes)
        return;
    remote::HostProfiles::remove(name);
    QSettings().remove(QStringLiteral("remote/serverCommand/") + name);
    fillComputerList();
    emit computersChanged();
}
#endif

void SettingsDialog::accept()
{
    for (const ProgramRow& r : m_rows)
        m_settings.setProgramPath(r.spec.name, QDir::fromNativeSeparators(r.edit->text().trimmed()));
    m_settings.setOperatorName(m_name->text().trimmed());
    m_settings.setOperatorOrcid(m_orcid->text().trimmed());
    m_settings.setOperatorInstitution(m_institution->text().trimmed());
    m_settings.setOperatorLicense(m_license->currentText().trimmed());

    const bool darkMode = m_darkModeCheck->isChecked();
    const bool darkModeChangedNow = darkMode != m_settings.darkModeEnabled();
    m_settings.setDarkMode(darkMode);
    const bool useInvocation = m_useInvocationDirCheck->isChecked();
    const bool useInvocationChangedNow = useInvocation != m_settings.useInvocationDirectoryEnabled();
    m_settings.setUseInvocationDirectoryEnabled(useInvocation);
    for (const QString& dir : m_removedWorkDirs)
        m_settings.removeWorkingDirectory(dir);
    if (m_clearRecent)
        m_settings.clearRecentFilesV2();

    // read-modify-write: the other fields belong to the Display panel and the last session
    Settings::VisualizationSettings vis = m_settings.getVisualizationSettings();
    const bool centerOnLoad = m_centerOnLoadCheck->isChecked();
    const bool centerOnLoadChangedNow = centerOnLoad != vis.centerOnLoad;
    if (centerOnLoadChangedNow) {
        vis.centerOnLoad = centerOnLoad;
        m_settings.setVisualizationSettings(vis);
    }

    // An explicit directory choice is emitted after the invocation-directory switch, so that it
    // wins over the switch to the invocation directory that enabling the check box causes.
    if (darkModeChangedNow)
        emit darkModeChanged(darkMode);
    if (centerOnLoadChangedNow)
        emit centerOnLoadChanged(centerOnLoad);
    if (useInvocationChangedNow)
        emit useInvocationDirectoryChanged(useInvocation);
    if (!m_chosenWorkDir.isEmpty())
        emit workingDirectoryChosen(m_chosenWorkDir);
    if (m_clearRecent)
        emit recentFilesCleared();
    QDialog::accept();
}
