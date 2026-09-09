// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// OutputDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "outputdock.h"

#include "core/loghub.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

OutputDock::OutputDock(QWidget* parent)
    : QDockWidget(DockConfig::OutputDockTitle, parent)
{
    setObjectName(DockConfig::OutputViewDockObjectName);
    setupUI();
}

void OutputDock::setupUI()
{
    QWidget* outputWidget = new QWidget(this);
    QVBoxLayout* outputLayout = new QVBoxLayout(outputWidget);
    outputLayout->setContentsMargins(4, 4, 4, 4);

    QHBoxLayout* outputHeaderLayout = new QHBoxLayout;
    QLabel* outputLabel = new QLabel(tr("Output"));
    outputLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    outputHeaderLayout->addWidget(outputLabel);
    outputHeaderLayout->addStretch();

    QPushButton* clearOutputButton = new QPushButton;
    clearOutputButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));
    clearOutputButton->setToolTip(tr("Clear output (Ctrl+L)"));
    clearOutputButton->setMaximumWidth(30);
    connect(clearOutputButton, &QPushButton::clicked,
            this, &OutputDock::clearRequested);
    outputHeaderLayout->addWidget(clearOutputButton);
    outputLayout->addLayout(outputHeaderLayout);

    m_outputView = new QTextEdit;
    m_outputView->setPlaceholderText(tr("Output"));
    m_outputView->setReadOnly(true);
    outputLayout->addWidget(m_outputView);

    setWidget(outputWidget);
}

QTextEdit* OutputDock::outputView() const
{
    return m_outputView;
}

void OutputDock::appendOutput(const QString& text)
{
    if (m_outputView) {
        m_outputView->append(text);
        QScrollBar* bar = m_outputView->verticalScrollBar();
        if (bar)
            bar->setValue(bar->maximum());
    }
}

void OutputDock::clearOutput()
{
    m_pending.clear();
    if (m_outputView)
        m_outputView->clear();
}

void OutputDock::setText(const QString& text, bool scrollToBottom)
{
    if (!m_outputView)
        return;
    m_outputView->setPlainText(text);
    if (scrollToBottom) {
        QScrollBar* bar = m_outputView->verticalScrollBar();
        if (bar)
            bar->setValue(bar->maximum());
    }
}

// Claude Generated 2026 - LogHub subscription.
void OutputDock::followLogHub(LogHub* hub)
{
    if (!hub || !m_outputView)
        return;

    if (!m_flushTimer) {
        m_flushTimer = new QTimer(this);
        m_flushTimer->setSingleShot(true);
        m_flushTimer->setInterval(100);  // ms; one append per burst, not per line
        connect(m_flushTimer, &QTimer::timeout, this, &OutputDock::flushPending);
    }

    // Whatever the hub collected before the dock existed (startup messages).
    const QVector<LogRecord> backlog = hub->query({});
    for (const LogRecord& record : backlog)
        m_pending.append(LogHub::formatForView(record));
    flushPending();

    // Queued: records reach the hub from the simulation worker thread too.
    connect(hub, &LogHub::appended, this, [this](const LogRecord& record) {
        m_pending.append(LogHub::formatForView(record));
        if (m_flushTimer && !m_flushTimer->isActive())
            m_flushTimer->start();
    }, Qt::QueuedConnection);
}

void OutputDock::flushPending()
{
    if (m_pending.isEmpty() || !m_outputView)
        return;
    m_outputView->append(m_pending.join(QLatin1Char('\n')));
    m_pending.clear();
}
