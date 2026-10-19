// addcomputerdialog.cpp - see addcomputerdialog.h
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute)

#include "addcomputerdialog.h"

#include "remote/hostprobe.h"
#include "widgets/themeicon.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

#include <initializer_list>


void AddComputerDialog::fillHostCombo(QComboBox* combo, bool withLocal, const QString& select)
{
    const QSignalBlocker block(combo);
    combo->clear();
    if (withLocal)
        combo->addItem(tr("This computer"), QString());
    for (const remote::HostProfile& p : remote::HostProfiles::load()) {
        combo->addItem(p.name, p.name);
        combo->setItemData(combo->count() - 1, p.target, Qt::ToolTipRole);
    }
    const int idx = select.isEmpty() ? -1 : combo->findData(select);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
}

bool AddComputerDialog::isSavedComputer(const QString& name)
{
    return !name.isEmpty() && remote::HostProfiles::find(name).has_value();
}

QString AddComputerDialog::defaultServerCommand(const QString& host)
{
    const QString stored = QSettings().value(QStringLiteral("remote/serverCommand/") + host).toString();
    if (!stored.isEmpty())
        return stored;
    if (const auto p = remote::HostProfiles::find(host))
        return p->effectiveServerCommand();
    return QStringLiteral("qurcuma-server");
}

namespace {
const char* const kStepNames[3] = { QT_TRANSLATE_NOOP("AddComputerDialog", "Login"),
    QT_TRANSLATE_NOOP("AddComputerDialog", "Server"), QT_TRANSLATE_NOOP("AddComputerDialog", "Session") };
}

AddComputerDialog::AddComputerDialog(QWidget* parent, const std::optional<remote::HostProfile>& edit)
    : QDialog(parent)
{
    setWindowTitle(edit ? tr("Edit computer") : tr("Add computer"));
    resize(660, 480);
    setMinimumWidth(580);
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("A computer that runs the simulations for this window. qurcuma reaches it over ssh without a password "
                                "prompt (key or ssh agent), and qurcuma-server must be installed on it."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto* form = new QFormLayout;
    m_address = new QLineEdit(this);
    m_address->setPlaceholderText(tr("user@host.example.org or 192.0.2.10"));
    m_address->setToolTip(tr("Host name or IP address, optionally with the user name in front."));
    form->addRow(tr("Address:"), m_address);
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(tr("shown in the list; taken from the address if empty"));
    form->addRow(tr("Name:"), m_name);
    m_port = new QSpinBox(this);
    m_port->setRange(0, 65535);
    m_port->setSpecialValueText(tr("22 (default)"));
    form->addRow(tr("ssh port:"), m_port);
    m_key = new QLineEdit(this);
    m_key->setPlaceholderText(tr("empty: ssh agent and default keys"));
    m_browse = new QPushButton(themeIcon(QStringLiteral("document-open"), QStyle::SP_DirOpenIcon), tr("Browse..."), this);
    auto* keyRow = new QHBoxLayout;
    keyRow->addWidget(m_key);
    keyRow->addWidget(m_browse);
    form->addRow(tr("Key file:"), keyRow);
    m_serverCommand = new QLineEdit(this);
    m_serverCommand->setPlaceholderText(tr("empty: the test looks for qurcuma-server"));
    m_serverCommand->setToolTip(tr("Command that starts qurcuma-server on that computer (full path if it is not in the PATH). "
                                   "Left empty, the test searches for it and the found path is stored."));
    form->addRow(tr("Server command:"), m_serverCommand);
    layout->addLayout(form);

    // The test, in three steps that each show their own state.
    m_test = new QPushButton(themeIcon(QStringLiteral("network-connect"), QStyle::SP_DriveNetIcon), tr("Test connection"), this);
    m_problem = new QLabel(this);
    m_problem->setWordWrap(true);
    auto* testRow = new QHBoxLayout;
    testRow->addWidget(m_test);
    testRow->addStretch(1);
    layout->addLayout(testRow);
    for (int i = 0; i < 3; ++i) {
        m_stepIcons[i] = new QLabel(this);
        m_stepIcons[i]->setFixedSize(20, 20);
        m_stepLabels[i] = new QLabel(this);
        m_stepLabels[i]->setWordWrap(true);
        m_stepLabels[i]->setTextFormat(Qt::RichText);
        m_stepLabels[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto* row = new QHBoxLayout;
        row->addWidget(m_stepIcons[i], 0, Qt::AlignTop);
        row->addWidget(m_stepLabels[i], 1);
        layout->addLayout(row);
    }
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);  // busy indicator
    m_progress->setTextVisible(false);
    m_progress->setMaximumHeight(8);
    m_progress->hide();
    layout->addWidget(m_progress);
    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextFormat(Qt::RichText);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_summary);
    layout->addWidget(m_problem);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setText(edit ? tr("Save") : tr("Add"));
    layout->addWidget(buttons);

    if (edit) {
        m_originalName = edit->name;
        m_address->setText(edit->target);
        m_name->setText(edit->name);
        m_port->setValue(edit->port);
        m_key->setText(edit->identityFile);
        m_serverCommand->setText(edit->serverCommand);
    }
    for (int i = 0; i < 3; ++i)
        onStep(i, int(remote::HostProbe::Pending), QString());

    connect(m_browse, &QPushButton::clicked, this, [this] {
        const QString f = QFileDialog::getOpenFileName(this, tr("ssh key file"), QDir::homePath() + QStringLiteral("/.ssh"));
        if (!f.isEmpty())
            m_key->setText(f);
    });
    connect(m_test, &QPushButton::clicked, this, &AddComputerDialog::toggleTest);
    connect(buttons, &QDialogButtonBox::accepted, this, &AddComputerDialog::tryAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    for (QLineEdit* e : { m_address, m_name, m_key, m_serverCommand })
        connect(e, &QLineEdit::textChanged, this, [this] {
            // A result belongs to the entries it was made with.
            if (m_tested && !m_filling) {
                m_tested = m_testedOk = false;
                m_summary->clear();
                for (int i = 0; i < 3; ++i)
                    onStep(i, int(remote::HostProbe::Pending), QString());
            }
            updateButtons();
        });
    connect(m_port, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        m_tested = m_testedOk = false;
        updateButtons();
    });
    updateButtons();
}

AddComputerDialog::~AddComputerDialog()
{
    stopTest();
}

remote::HostProfile AddComputerDialog::profile() const
{
    remote::HostProfile p;
    p.target = m_address->text().trimmed();
    p.name = m_name->text().trimmed();
    if (p.name.isEmpty())
        p.name = p.target.section(QLatin1Char('@'), -1);  // the host part of the address
    p.port = m_port->value();
    p.identityFile = m_key->text().trimmed();
    p.serverCommand = m_serverCommand->text().trimmed();
    return p;
}

void AddComputerDialog::setBusy(bool busy)
{
    // Everything that would change the test while it runs is greyed out.
    for (QWidget* w : std::initializer_list<QWidget*>{ m_address, m_name, m_port, m_key, m_serverCommand, m_browse })
        w->setEnabled(!busy);
    m_progress->setVisible(busy);
    m_test->setText(busy ? tr("Cancel test") : tr("Test connection"));
    updateButtons();
}

void AddComputerDialog::updateButtons()
{
    QString why;
    const bool filled = !m_address->text().trimmed().isEmpty();
    const bool valid = filled && profile().isValid(&why);
    const bool busy = m_probe != nullptr;
    m_test->setEnabled(busy || valid);
    m_ok->setEnabled(valid && !busy);
    m_problem->setText(filled && !valid ? QStringLiteral("<span style='color:#c62828'>%1</span>").arg(why.toHtmlEscaped()) : QString());
    if (!busy && !m_tested)
        m_ok->setToolTip(valid ? tr("Not tested yet: the computer is saved as entered.") : tr("Enter a valid address first."));
    else if (!busy && m_testedOk)
        m_ok->setToolTip(tr("The test passed."));
    else if (!busy)
        m_ok->setToolTip(tr("The test found a problem (see above); the computer can still be saved."));
}

void AddComputerDialog::toggleTest()
{
    if (m_probe)
        stopTest();
    else
        startTest();
}

void AddComputerDialog::startTest()
{
    m_tested = m_testedOk = false;
    m_summary->clear();
    for (int i = 0; i < 3; ++i)
        onStep(i, int(remote::HostProbe::Pending), QString());
    m_probe = new remote::HostProbe(profile(), this);
    connect(m_probe, &remote::HostProbe::stepChanged, this, &AddComputerDialog::onStep);
    connect(m_probe, &remote::HostProbe::finished, this, &AddComputerDialog::onProbeFinished);
    setBusy(true);
    m_probe->start();
}

void AddComputerDialog::stopTest()
{
    if (!m_probe)
        return;
    delete m_probe;  // cancels the ssh processes
    m_probe = nullptr;
    for (int i = 0; i < 3; ++i)
        onStep(i, int(remote::HostProbe::Pending), QString());
    m_summary->setText(tr("Test cancelled."));
    setBusy(false);
}

void AddComputerDialog::onStep(int step, int state, const QString& text)
{
    if (step < 0 || step > 2)
        return;
    struct Look { const char* theme; QStyle::StandardPixmap fallback; const char* colour; };
    // pending, running, ok, failed, skipped
    static const Look looks[] = {
        { "dialog-question", QStyle::SP_MessageBoxQuestion, "#757575" },
        { "view-refresh", QStyle::SP_BrowserReload, "#1565c0" },
        { "dialog-ok-apply", QStyle::SP_DialogApplyButton, "#2e7d32" },
        { "dialog-error", QStyle::SP_MessageBoxCritical, "#c62828" },
        { "go-next", QStyle::SP_ArrowForward, "#757575" } };
    const Look& l = looks[qBound(0, state, 4)];
    m_stepIcons[step]->setPixmap(themeIcon(QString::fromLatin1(l.theme), l.fallback).pixmap(16, 16));
    const QString name = QCoreApplication::translate("AddComputerDialog", kStepNames[step]);
    const QString detail = state == int(remote::HostProbe::Pending) ? tr("not tried yet") : text;
    m_stepLabels[step]->setText(QStringLiteral("<span style='color:%1'><b>%2</b> &nbsp;%3</span>")
                                    .arg(QLatin1String(l.colour), name.toHtmlEscaped(), detail.toHtmlEscaped()));
}

void AddComputerDialog::onProbeFinished(const remote::ProbeResult& result)
{
    m_probe->deleteLater();
    m_probe = nullptr;
    m_tested = true;
    m_testedOk = result.sessionOk;
    if (!result.correctedCommand.isEmpty()) {
        m_filling = true;  // the completed command is part of this result, not a new entry
        m_serverCommand->setText(result.correctedCommand);
        m_filling = false;
    } else if (!result.serverPath.isEmpty() && m_serverCommand->text().trimmed().isEmpty()) {
        m_filling = true;
        m_serverCommand->setText(result.serverPath);
        m_filling = false;
    }
    m_summary->setText(QStringLiteral("<b><span style='color:%1'>%2</span></b>")
                           .arg(result.sessionOk ? QStringLiteral("#2e7d32") : QStringLiteral("#c62828"), result.message.toHtmlEscaped()));
    setBusy(false);
}

void AddComputerDialog::tryAccept()
{
    QString why;
    const remote::HostProfile p = profile();
    const bool saved = m_originalName.isEmpty() ? remote::HostProfiles::add(p, &why)
                                                : remote::HostProfiles::update(m_originalName, p, &why);
    if (!saved) {
        m_problem->setText(why);
        return;
    }
    accept();
}
