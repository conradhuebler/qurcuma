// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// OutputDock — bottom dock holding the read-only calculation/output log.
// Replaces the inline output QTextEdit previously created in MainWindow.
//
// Claude Generated 2026 - Dock system restructuring.

#pragma once

#include "dockconfig.h"

#include <QDockWidget>

class QTextEdit;

class OutputDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit OutputDock(QWidget* parent = nullptr);

    /// Raw access to the log view for callers that already append text directly.
    QTextEdit* outputView() const;

public slots:
    void appendOutput(const QString& text);
    void clearOutput();

signals:
    /// Emitted when the user clicks the clear button.
    void clearRequested();

private:
    void setupUI();

    QTextEdit* m_outputView = nullptr;
};
