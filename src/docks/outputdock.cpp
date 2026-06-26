// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// OutputDock implementation.
//
// Claude Generated 2026 - Dock system restructuring.

#include "outputdock.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
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
    if (m_outputView)
        m_outputView->clear();
}
