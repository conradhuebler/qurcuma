// scriptdock.cpp - The script dock.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - Structure: a row of controls, the editor, and the output
// below it. The run happens on its own thread, because a script may take up to the
// deadline and the window has to keep turning (the same reason the MD worker exists);
// Stop reaches into the running engine from this thread, which is what
// QJSEngine::setInterrupted is for.
#include "scriptdock.h"

#include "dockconfig.h"

#include "core/loghub.h"
#include "core/tooldispatcher.h"
#include "core/toolregistry.h"
#include "script/scriptbuiltins.h"
#include "script/scriptinterpreter.h"
#include "script/scriptsyntax.h"

#include <QAction>
#include <QDateTime>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantMap>

namespace {

const QLatin1String kEditorKey("script/editor");

/// The worked examples. Each one is something an operator would otherwise reach for a
/// calculator to do, and the last one shows what the dock has that the assistant's
/// tool does not: it may call tools.
struct Example {
    const char* title;
    const char* source;
};

const Example kExamples[] = {
    { "Energy difference in kJ/mol",
        "// An energy difference, converted here rather than in anyone's head.\n"
        "var e1 = -40.123456;   // Hartree\n"
        "var e2 = -40.118;\n"
        "print(\"dE in Hartree\", e2 - e1);\n"
        "print(\"dE in kJ/mol\", ha_to_kjmol(e2 - e1));\n" },
    { "Mean and spread of measured values",
        "var d = [2.31, 2.28, 2.35, 2.29, 2.33];   // Angstrom\n"
        "print(\"mean\", mean(d), \"spread\", sd(d), \"n\", len(d));\n"
        "print(\"standard error\", sem(d));\n" },
    { "A line through measured points",
        "var c = [0.001, 0.002, 0.003, 0.004];   // mol/L\n"
        "var r = [0.012, 0.025, 0.037, 0.049];   // measured\n"
        "print(\"slope\", slope(c, r), \"intercept\", intercept(c, r));\n"
        "print(\"r2\", r2(c, r));\n" },
    { "Through a tool: the loaded structure",
        "// The dock may call tools; the assistant's calculate may not.\n"
        "var s = tool(\"get_structure_summary\", {});\n"
        "if (s.error) { print(\"failed:\", s.error); }\n"
        "else {\n"
        "    print(\"formula\", s.formula, \"atoms\", s.atom_count);\n"
        "    print(\"bonds per atom\", s.bond_count / s.atom_count);\n"
        "}\n" },
};

/// The value of a run as one line of output. Numbers keep their full precision in the
/// tool result; here they are rendered the way the interpreter renders them elsewhere.
QString renderValue(const QVariant& value)
{
    const auto type = static_cast<QMetaType::Type>(value.typeId());
    if (type == QMetaType::Double || type == QMetaType::Float || type == QMetaType::Int)
        return script::formatNumber(value.toDouble());
    if (type == QMetaType::QVariantMap || type == QMetaType::QVariantList)
        return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
    return value.toString();
}

}  // namespace

ScriptDock::ScriptDock(QWidget* parent)
    : QDockWidget(DockConfig::ScriptDockTitle, parent)
{
    setObjectName(DockConfig::ScriptDockObjectName);
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);

    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    auto* controls = new QHBoxLayout;
    m_runButton = new QPushButton(tr("Run"), content);
    m_runButton->setToolTip(tr("Run the script (Ctrl+Return)"));
    m_stopButton = new QPushButton(tr("Stop"), content);
    m_stopButton->setEnabled(false);
    auto* examples = new QPushButton(tr("Examples"), content);
    auto* examplesMenu = new QMenu(examples);
    const int exampleCount = int(sizeof(kExamples) / sizeof(kExamples[0]));
    for (int i = 0; i < exampleCount; ++i)
        examplesMenu->addAction(QString::fromUtf8(kExamples[i].title), this, [this, i] { insertExample(i); });
    examples->setMenu(examplesMenu);
    examples->setToolTip(tr("Put a worked example into the editor"));

    controls->addWidget(m_runButton);
    controls->addWidget(m_stopButton);
    controls->addWidget(examples);
    controls->addStretch(1);
    auto* hint = new QLabel(tr("JavaScript; the value of the last expression is the result"), content);
    hint->setEnabled(false);
    controls->addWidget(hint);
    layout->addLayout(controls);

    m_editor = new QPlainTextEdit(content);
    m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_editor->setPlaceholderText(tr("var d = [2.31, 2.28, 2.35];\n"
                                    "print(\"mean\", mean(d), \"spread\", sd(d));"));
    m_editor->setTabChangesFocus(false);

    m_output = new QPlainTextEdit(content);
    m_output->setReadOnly(true);
    m_output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_output->setMaximumHeight(160);
    m_output->setPlaceholderText(tr("Output appears here."));

    auto* splitter = new QSplitter(Qt::Vertical, content);
    splitter->addWidget(m_editor);
    splitter->addWidget(m_output);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);
    setWidget(content);

    auto* runAction = new QAction(this);
    runAction->setShortcuts({ QKeySequence(Qt::CTRL | Qt::Key_Return),
        QKeySequence(Qt::CTRL | Qt::Key_Enter) });
    runAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(runAction);
    connect(runAction, &QAction::triggered, this, &ScriptDock::run);
    connect(m_runButton, &QPushButton::clicked, this, &ScriptDock::run);
    connect(m_stopButton, &QPushButton::clicked, this, &ScriptDock::stop);

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(1000);
    connect(m_saveTimer, &QTimer::timeout, this, &ScriptDock::saveEditor);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &ScriptDock::saveEditorSoon);

    loadEditor();
}

ScriptDock::~ScriptDock()
{
    saveEditor();
    // A running script calls back into this object, so it is stopped and waited for
    // rather than left running; the thread itself is deleted by the finished handler.
    // The wait matches the dispatcher's own timeout, because a script that is inside a
    // tool call cannot be stopped until that call returns or gives up.
    if (m_interpreter)
        m_interpreter->requestStop();
    if (m_thread)
        m_thread->wait(ToolDispatcher::kDefaultTimeoutMs);
}

void ScriptDock::setToolLayer(ToolRegistry* registry, ToolDispatcher* dispatcher)
{
    m_registry = registry;
    m_dispatcher = dispatcher;
}

void ScriptDock::setApprovalPolicy(std::function<bool(const ToolSpec&, const QJsonObject&)> policy)
{
    m_approval = std::move(policy);
}

QString ScriptDock::source() const
{
    return m_editor->toPlainText();
}

void ScriptDock::setSource(const QString& source)
{
    m_editor->setPlainText(source);
}

void ScriptDock::insertExample(int index)
{
    const int count = int(sizeof(kExamples) / sizeof(kExamples[0]));
    if (index < 0 || index >= count)
        return;
    m_editor->setPlainText(QString::fromUtf8(kExamples[index].source));
    m_output->clear();
}

void ScriptDock::appendLine(const QString& line)
{
    m_output->appendPlainText(line);
}

void ScriptDock::loadEditor()
{
    const QString stored = QSettings().value(kEditorKey).toString();
    if (!stored.isEmpty())
        m_editor->setPlainText(stored);
}

void ScriptDock::saveEditorSoon()
{
    m_saveTimer->start();
}

void ScriptDock::saveEditor()
{
    QSettings().setValue(kEditorKey, m_editor->toPlainText());
}

bool ScriptDock::confirmToolCalls(const QString& source)
{
    const QStringList names = script::namedToolsIn(source);
    if (names.isEmpty())
        return true;

    QStringList lines;
    bool beyondReading = false;
    for (const QString& name : names) {
        ToolSpec spec;
        if (m_registry && m_registry->tool(name, spec)) {
            lines.append(QStringLiteral("%1 (%2)").arg(name, toolEffectName(spec.effect)));
            if (spec.effect != ToolEffect::Read && spec.effect != ToolEffect::Display)
                beyondReading = true;
        } else {
            lines.append(QStringLiteral("%1 (no such tool)").arg(name));
            beyondReading = true;
        }
    }

    // Reading and displaying need no announcement; the operator already pressed Run.
    if (!beyondReading)
        return true;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Run this script?"));
    box.setText(tr("The script calls these tools:"));
    box.setInformativeText(lines.join(QLatin1Char('\n')));
    box.setDetailedText(tr("Each call is made by the script, not by the assistant. A call that "
                           "needs permission is still put to you when it happens."));
    QPushButton* runButton = box.addButton(tr("Run"), QMessageBox::AcceptRole);
    box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(runButton);
    box.exec();
    return box.clickedButton() == runButton;
}

void ScriptDock::run()
{
    if (m_running)
        return;

    const QString source = m_editor->toPlainText();
    if (source.trimmed().isEmpty()) {
        appendLine(tr("nothing to run"));
        return;
    }
    if (!confirmToolCalls(source))
        return;

    saveEditor();
    m_running = true;
    m_runButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    appendLine(tr("--- %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));

    m_interpreter = std::make_shared<ScriptInterpreter>();
    // The dispatcher's Stop path: a waiting job and a running script end the same way.
    ToolDispatcher* const dispatcher = m_dispatcher;
    if (dispatcher)
        m_interpreter->setStopPoll([dispatcher] { return dispatcher->isInterrupted(); });

    ScriptHost host;
    if (m_dispatcher) {
        ToolRegistry* const registry = m_registry;
        auto approval = m_approval;
        host.call = [this, registry, dispatcher, approval](const QString& name, const QVariantMap& args) -> QVariant {
            // On the script's thread. The policy is the assistant's, so a script is not
            // the softer path, and a refusal comes back as a value the script can
            // branch on instead of an exception it cannot catch.
            const QJsonObject json = QJsonObject::fromVariantMap(args);
            ToolSpec spec;
            if (registry && !registry->tool(name, spec)) {
                return QVariantMap {
                    { QStringLiteral("error"), QStringLiteral("no tool named \"%1\"").arg(name) }
                };
            }
            if (approval && !approval(spec, json)) {
                const QString refused = QStringLiteral("the operator did not allow \"%1\" just now").arg(name);
                QMetaObject::invokeMethod(this, [this, name] {
                    appendLine(QStringLiteral("  tool %1 refused").arg(name));
                }, Qt::QueuedConnection);
                return QVariantMap { { QStringLiteral("error"), refused } };
            }

            const ToolResult result = dispatcher->dispatch(name, json);
            QVariantMap out = result.data.toVariantMap();
            if (!result.ok)
                out.insert(QStringLiteral("error"), result.error);
            if (!result.text.isEmpty())
                out.insert(QStringLiteral("text"), result.text);

            const bool ok = result.ok;
            const QString summary = result.ok ? result.text : result.error;
            QMetaObject::invokeMethod(this, [this, name, ok, summary] {
                appendLine(QStringLiteral("  tool %1 %2%3")
                               .arg(name, ok ? QStringLiteral("ok") : QStringLiteral("failed"),
                                   summary.isEmpty() ? QString() : QStringLiteral(" - %1").arg(summary)));
            }, Qt::QueuedConnection);
            return out;
        };
    }

    auto interpreter = m_interpreter;
    m_thread = QThread::create([this, interpreter, source, host] {
        const ScriptResult result = interpreter->run(source, {}, host);
        QStringList lines;
        for (const QString& printed : result.prints)
            lines.append(printed);
        if (result.value.isValid())
            lines.append(QStringLiteral("= %1").arg(renderValue(result.value)));
        if (!result.ok)
            lines.append(result.stopped ? QStringLiteral("stopped: %1").arg(result.error.message)
                                        : QStringLiteral("error: %1").arg(result.error.message));
        lines.append(QStringLiteral("(%1 ms)").arg(result.elapsedMs));

        const bool ok = result.ok;
        const QString summary = result.ok ? lines.value(0) : result.error.message;
        QMetaObject::invokeMethod(this, [this, ok, lines, summary, source] {
            m_running = false;
            m_runButton->setEnabled(true);
            m_stopButton->setEnabled(false);
            for (const QString& line : lines)
                appendLine(line);

            LogHub::instance().append(QStringLiteral("script"), ok ? LogLevel::Info : LogLevel::Warning,
                QStringLiteral("%1 -> %2")
                    .arg(source.split(QLatin1Char('\n')).first().trimmed().left(120),
                        ok ? QStringLiteral("ok") : QStringLiteral("failed")));
            emit scriptRan(source, ok, summary);
        }, Qt::QueuedConnection);
    });
    connect(m_thread, &QThread::finished, this, [this] {
        if (m_thread) {
            m_thread->deleteLater();
            m_thread = nullptr;
        }
    });
    m_thread->start();
}

void ScriptDock::stop()
{
    if (m_interpreter)
        m_interpreter->requestStop();
}
