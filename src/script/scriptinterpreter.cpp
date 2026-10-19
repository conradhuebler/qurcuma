// scriptinterpreter.cpp - The mechanics: one engine per run, limits, stop, errors.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - The content of what a script can compute lives in
// scriptbuiltins.cpp; this file is only the enclosure. What it has to get right:
//
//  - A run ends. The deadline is a wall clock rather than an instruction budget,
//    because the engine is a black box: a thread watches it and interrupts it.
//    QJSEngine::setInterrupted is documented thread-safe for exactly this, and the
//    test proves it stops a `while (true) {}` rather than promising it.
//  - A refusal names the place. Line and column come from the engine's error object
//    and its stack frames.
//  - A result is bounded. Lists, objects, strings and the value as a whole are cut
//    at the limits, and `truncated` says so rather than the answer looking complete.
#include "scriptinterpreter.h"

#include "scriptbridge.h"
#include "scriptbuiltins.h"

#include <QElapsedTimer>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJSValue>
#include <QRegularExpression>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {

/// The watch interval. The stop and the deadline are checked this often while the
/// engine holds the thread; small enough that a stop feels immediate, large enough
/// that the watcher costs nothing.
constexpr int kWatchSliceMs = 20;

/// A binding must be a name JavaScript accepts, or the program built from it would be
/// a syntax error instead of a statement about the argument.
bool isIdentifier(const QString& name)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z_$][A-Za-z0-9_$]*$"));
    return pattern.match(name).hasMatch();
}

/// A JavaScript literal for @p value. JSON is a subset of JavaScript, so objects and
/// arrays go out as they are; a scalar needs the array wrapper because QJsonDocument
/// serialises only containers, and stripping the wrapper is cheaper than a second
/// escaping rule that could disagree with the first.
QString jsLiteral(const QJsonValue& value)
{
    switch (value.type()) {
    case QJsonValue::Null:
        return QStringLiteral("null");
    case QJsonValue::Bool:
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Double: {
        const double number = value.toDouble();
        if (qIsNaN(number) || qIsInf(number))
            return QStringLiteral("null");  // JSON cannot carry them either
        return QString::number(number, 'g', 17);  // enough digits to round-trip
    }
    case QJsonValue::String: {
        const QByteArray wrapped = QJsonDocument(QJsonArray { value }).toJson(QJsonDocument::Compact);
        return QString::fromUtf8(wrapped.mid(1, wrapped.size() - 2));
    }
    case QJsonValue::Array:
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    case QJsonValue::Object:
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    default:
        return QStringLiteral("null");
    }
}

QString bindingProgram(const QVariantMap& bindings)
{
    QStringList statements;
    for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
        statements.append(QStringLiteral("var %1 = %2;")
                              .arg(it.key(), jsLiteral(QJsonValue::fromVariant(it.value()))));
    }
    return statements.join(QLatin1Char('\n'));
}

/// "line 3, column 12", "line 3", or nothing when the engine gave no position
/// (a builtin that throws carries a message but no place).
QString positionText(const ScriptError& error)
{
    if (error.line <= 0)
        return QString();
    if (error.column <= 0)
        return QStringLiteral("line %1").arg(error.line);
    return QStringLiteral("line %1, column %2").arg(error.line).arg(error.column);
}

/// Whether a stack frame belongs to the script the caller wrote rather than to a
/// program the interpreter ran first (the builtin table). File names arrive
/// URL-encoded, hence the two spellings.
bool frameInScript(const QString& entry)
{
    return entry.contains(QLatin1String("<script>")) || entry.contains(QLatin1String("%3Cscript%3E"));
}

bool failureInScript(const QJSValue& value)
{
    const QString file = value.property(QStringLiteral("fileName")).toString();
    if (file.isEmpty())
        return true;  // no file named: the engine's own complaint, about the script
    return file == QLatin1String("<script>") || file.contains(QLatin1String("%3Cscript%3E"));
}

/// Where the engine says the failure is. The frames read
/// "function:line:column:file" and the error object carries a line and a file name.
///
/// Claude Generated 2026 - Measured (14.09.2026), and it decided this code: a builtin
/// that throws reports its own line inside the builtin table, which is meaningless to
/// the script's author (a one-line script got "line 57"). The caller's line is in the
/// frame that belongs to the script, so that frame wins and the builtin's own position
/// is dropped. A syntax error reports a real column, a runtime error -1, so a column is
/// taken only when the engine actually has one.
void applyPosition(ScriptError& error, const QJSValue& value, const QStringList& frames)
{
    static const QRegularExpression frame(QStringLiteral(":(-?\\d+):(-?\\d+):"));

    for (const QString& entry : frames) {
        if (!frameInScript(entry))
            continue;
        const auto match = frame.match(entry);
        if (!match.hasMatch())
            continue;
        error.line = match.captured(1).toInt();
        const int column = match.captured(2).toInt();
        if (column > 0)
            error.column = column;
        return;
    }

    if (failureInScript(value))
        error.line = value.property(QStringLiteral("lineNumber")).toInt();
}

QString errorText(const QJSValue& value)
{
    const QString message = value.property(QStringLiteral("message")).toString();
    const QString name = value.property(QStringLiteral("name")).toString();
    QString text = message.isEmpty() ? value.toString() : message;
    if (!name.isEmpty() && name != QLatin1String("Error"))
        text = QStringLiteral("%1: %2").arg(name, text);
    return text;
}

/// A script may throw something that is not an Error, in which case there is no
/// message and no name: `toString()` gives "[object Object]" for an object, which says
/// nothing. Rendering it as JSON at least shows what was thrown.
QString thrownText(const QJSValue& value)
{
    const QVariant thrown = value.toVariant();
    const auto type = static_cast<QMetaType::Type>(thrown.typeId());
    if (type == QMetaType::QVariantMap || type == QMetaType::QVariantList)
        return QString::fromUtf8(QJsonDocument::fromVariant(thrown).toJson(QJsonDocument::Compact));
    return value.toString();
}

}  // namespace

ScriptInterpreter::ScriptInterpreter(ScriptLimits limits)
    : m_limits(limits)
{
}

ScriptInterpreter::~ScriptInterpreter() = default;

ScriptResult ScriptInterpreter::run(const QString& source, const QVariantMap& bindings,
    const ScriptHost& host)
{
    ScriptResult result;
    m_stop.store(false);

    QElapsedTimer clock;
    clock.start();

    if (source.trimmed().isEmpty()) {
        result.error.line = 1;
        result.error.column = 1;
        result.error.message = QStringLiteral("line 1: the script is empty");
        return result;
    }

    for (auto it = bindings.constBegin(); it != bindings.constEnd(); ++it) {
        if (!isIdentifier(it.key())) {
            result.error.message = QStringLiteral(
                "the name \"%1\" cannot be bound to the script: a name must start with a letter "
                "or _ and continue with letters, digits or _")
                                       .arg(it.key());
            return result;
        }
    }

    QJSEngine engine;

    // Bindings first, then the builtins, so a name a caller passes cannot shadow a
    // builtin function.
    if (!bindings.isEmpty()) {
        const QJSValue bound = engine.evaluate(bindingProgram(bindings),
            QStringLiteral("<bindings>"), 1);
        if (bound.isError()) {
            result.error.message = QStringLiteral("the values handed to the script could not be bound: %1")
                                       .arg(bound.toString());
            return result;
        }
    }

    // The crossing into qurcuma exists only when the caller asked for it. The engine
    // owns no C++ object unless this is set, which is what keeps the assistant's tool
    // a calculator.
    ScriptBridge bridge(host);
    if (host.call) {
        QJSValue wrapper = engine.newQObject(&bridge);
        // The bridge lives on this stack frame for the length of the run; letting
        // JavaScript own it would mean the collector freeing it out from under us.
        engine.setObjectOwnership(&bridge, QJSEngine::CppOwnership);
        engine.globalObject().setProperty(QStringLiteral("__host"), wrapper);
    }

    const QJSValue builtinProblem = engine.evaluate(QString::fromUtf8(script::builtinsSource()),
        QStringLiteral("<builtins>"), 1);
    if (builtinProblem.isError()) {
        // Our own program, so this is a fault of the interpreter and not of the script.
        result.error.message = QStringLiteral("the builtin table could not be loaded: %1")
                                   .arg(errorText(builtinProblem));
        return result;
    }

    // The watcher: the deadline and the stop button both end up in setInterrupted.
    std::mutex gate;
    std::condition_variable wake;
    bool finished = false;
    std::atomic<bool> deadlineHit { false };
    std::atomic<bool> stopHit { false };
    QJSEngine* const raw = &engine;
    const int deadlineMs = m_limits.deadlineMs;

    std::thread watcher([&] {
        std::unique_lock<std::mutex> lock(gate);
        int waited = 0;
        while (!finished) {
            if (wake.wait_for(lock, std::chrono::milliseconds(kWatchSliceMs),
                    [&] { return finished; }))
                return;
            if (m_stop.load() || (m_stopPoll && m_stopPoll())) {
                stopHit.store(true);
                raw->setInterrupted(true);
                return;
            }
            waited += kWatchSliceMs;
            if (deadlineMs > 0 && waited >= deadlineMs) {
                deadlineHit.store(true);
                raw->setInterrupted(true);
                return;
            }
        }
    });

    QStringList frames;
    const QJSValue value = engine.evaluate(source, QStringLiteral("<script>"), 1, &frames);

    // Measured: `throw "boom"` leaves isError() false and only shows up as a stack
    // frame, so the frames are what distinguishes a thrown value from a returned one
    // (Qt's own advice: an empty frame list means a normal return).
    const bool exceptional = value.isError() || !frames.isEmpty();

    {
        std::lock_guard<std::mutex> lock(gate);
        finished = true;
    }
    wake.notify_all();
    watcher.join();

    result.elapsedMs = clock.elapsed();

    if (exceptional) {
        ScriptError error;
        applyPosition(error, value, frames);
        const QString where = positionText(error);

        if (deadlineHit.load()) {
            // A script that produced no value and ran out of time: the loop the model
            // wrote has no end, or computes more than a line of arithmetic needs.
            // The interrupt carries no position (measured), so this names the reason
            // and not the line.
            error.message = QStringLiteral("the script ran longer than %1 ms and was stopped%2; "
                                           "make the loop end or compute less")
                                .arg(deadlineMs)
                                .arg(where.isEmpty() ? QString()
                                                     : QStringLiteral(" in %1").arg(where));
        } else if (stopHit.load()) {
            error.message = QStringLiteral("stopped at the operator's request%1")
                                .arg(where.isEmpty() ? QString() : QStringLiteral(" in %1").arg(where));
        } else {
            const QString text = value.isError() ? errorText(value) : thrownText(value);
            error.message = where.isEmpty() ? text : QStringLiteral("%1: %2").arg(where, text);
        }

        result.stopped = deadlineHit.load() || stopHit.load();
        result.error = error;
        return result;
    }

    bool cut = false;
    result.value = script::cappedValue(value.toVariant(), m_limits, &cut);
    result.prints = script::readPrints(engine, m_limits.maxPrints, &cut);
    result.truncated = cut;

    const QJsonObject payload { { QStringLiteral("value"), QJsonValue::fromVariant(result.value) } };
    const QByteArray compact = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    if (m_limits.maxResultChars > 0 && compact.size() > m_limits.maxResultChars) {
        // The honest answer to a big result is to aggregate it inside the script, and
        // the message says so rather than pretending the value was small.
        result.value = QStringLiteral("(the result is %1 characters, more than the %2 reported here: "
                                      "reduce it inside the script, for example to its mean or its extremes)")
                           .arg(compact.size())
                           .arg(m_limits.maxResultChars);
        result.truncated = true;
    }

    result.ok = true;
    return result;
}
