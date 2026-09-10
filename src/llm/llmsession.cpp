// llmsession.cpp - The agent loop.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "llmsession.h"

#include "core/loghub.h"
#include "core/tooldispatcher.h"
#include "core/toolregistry.h"
#include "llmclient.h"

#include <QSet>
#include <QThread>
#include <QElapsedTimer>
#include <QJsonDocument>

namespace {

/// Read and Display run unattended; everything else has to be approved. This is
/// the operator's policy, expressed once.
bool needsApproval(ToolEffect effect)
{
    return effect != ToolEffect::Read && effect != ToolEffect::Display;
}

QJsonObject message(const QString& role, const QString& content)
{
    QJsonObject o;
    o.insert(QStringLiteral("role"), role);
    o.insert(QStringLiteral("content"), content);
    return o;
}

}  // namespace

LlmSession::LlmSession(LlmClient* client, ToolRegistry* registry, ToolDispatcher* dispatcher,
                       LogHub* hub, QObject* parent)
    : QObject(parent)
    , m_client(client)
    , m_registry(registry)
    , m_dispatcher(dispatcher)
    , m_hub(hub)
{
    if (m_client) {
        connect(m_client, &LlmClient::finished, this, &LlmSession::onClientFinished);
        connect(m_client, &LlmClient::failed, this, &LlmSession::onClientFailed);
        connect(m_client, &LlmClient::contentChunk, this, &LlmSession::assistantChunk);
        connect(m_client, &LlmClient::reasoningChunk, this, &LlmSession::reasoningChunk);
    }
}

void LlmSession::setApprovalPolicy(ApprovalFn approve)
{
    m_approve = std::move(approve);
}

void LlmSession::setSystemPrompt(const QString& prompt)
{
    m_systemPrompt = prompt;
}

void LlmSession::setMaxIterations(int iterations)
{
    m_maxIterations = qMax(1, iterations);
}

void LlmSession::reset()
{
    m_messages = QJsonArray();
    m_iteration = 0;
}

void LlmSession::note(LogLevel level, const QString& text)
{
    if (m_hub)
        m_hub->append(QStringLiteral("llm"), level, text);
}

void LlmSession::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(busy);
}

QJsonArray LlmSession::toolCatalogue() const
{
    QJsonArray catalogue;
    if (!m_registry)
        return catalogue;

    // Which tools can do anything right now. The predicates may look at the viewer,
    // so they are evaluated on the thread that owns it; this loop is on the agent
    // loop's. Blocking is safe here for the same reason the approval dialog is --
    // nothing on the GUI thread ever waits on this one.
    QSet<QString> unavailable;
    const auto collect = [this, &unavailable] {
        for (const ToolSpec& spec : m_registry->all()) {
            if (spec.available && !spec.available())
                unavailable.insert(spec.name);
        }
    };
    if (m_dispatcher && QThread::currentThread() != m_dispatcher->thread())
        QMetaObject::invokeMethod(m_dispatcher, collect, Qt::BlockingQueuedConnection);
    else
        collect();

    for (const ToolSpec& spec : m_registry->all()) {
        if (unavailable.contains(spec.name))
            continue;
        QJsonObject function;
        function.insert(QStringLiteral("name"), spec.name);
        function.insert(QStringLiteral("description"), spec.description);
        function.insert(QStringLiteral("parameters"), spec.paramSchema);

        QJsonObject entry;
        entry.insert(QStringLiteral("type"), QStringLiteral("function"));
        entry.insert(QStringLiteral("function"), function);
        catalogue.append(entry);
    }
    return catalogue;
}

void LlmSession::ask(const QString& userMessage)
{
    if (m_busy) {
        emit failed(tr("the assistant is still working on the previous question"));
        return;
    }
    if (!m_client) {
        emit failed(tr("no endpoint client"));
        return;
    }

    if (m_messages.isEmpty() && !m_systemPrompt.isEmpty())
        m_messages.append(message(QStringLiteral("system"), m_systemPrompt));
    m_messages.append(message(QStringLiteral("user"), userMessage));

    m_iteration = 0;
    m_freeRounds = 0;
    m_finalRound = false;
    if (m_dispatcher)
        m_dispatcher->clearInterrupt();
    setBusy(true);
    sendRound();
}

void LlmSession::sendRound()
{
    ++m_iteration;
    // The closing round goes out without a catalogue, which is what makes it
    // closing: with no tools on offer the model has to write its answer.
    m_client->send(m_messages, m_finalRound ? QJsonArray() : toolCatalogue());
}

void LlmSession::cancel()
{
    if (!m_busy)
        return;
    // Set first, and unconditionally: a tool may be waiting right now, and the
    // queued cancel below cannot reach a loop that is itself blocked inside one.
    if (m_dispatcher)
        m_dispatcher->requestInterrupt();
    if (m_client)
        m_client->cancel();
    note(LogLevel::Info, tr("cancelled by the user"));
    endTurn();
}

void LlmSession::endTurn()
{
    setBusy(false);
    emit finished();
}

void LlmSession::onClientFailed(const QString& error)
{
    note(LogLevel::Warning, tr("endpoint error: %1").arg(error));
    setBusy(false);
    emit failed(error);
}

void LlmSession::onClientFinished(const QJsonObject& assistantMessage)
{
    // The assistant's turn goes into the history verbatim, tool_calls included:
    // the protocol requires that every tool result refer back to a call the
    // history actually contains.
    m_messages.append(assistantMessage);

    const QJsonArray toolCalls = assistantMessage.value(QStringLiteral("tool_calls")).toArray();
    if (toolCalls.isEmpty()) {
        const QString text = assistantMessage.value(QStringLiteral("content")).toString();
        emit this->assistantMessage(text);
        endTurn();
        return;
    }

    if (m_finalRound) {
        // It was asked to answer without a catalogue and called a tool anyway.
        // Take whatever prose came with it rather than opening another round.
        const QString text = assistantMessage.value(QStringLiteral("content")).toString();
        emit this->assistantMessage(text.isEmpty()
                ? tr("The round budget ran out before an answer was written.")
                : text);
        endTurn();
        return;
    }

    // A round that only waited does not count. Watching a simulation is a dozen
    // calls that each hand back a number and cost nothing but time, and charging
    // them against the budget meant a run was cut off for being patient.
    // The ceiling below still holds, so a loop that only ever reads still ends.
    const int workRounds = m_iteration - m_freeRounds;
    const bool exhausted = workRounds >= m_maxIterations
        || m_iteration >= m_maxIterations * 3;
    if (exhausted) {
        // Not a dead end. The results gathered so far are in the history, and the
        // model is asked once more with no tools, so the turn ends with the answer
        // they support instead of with the work discarded.
        //
        // Every pending call still has to be answered first: the protocol requires
        // a tool result for each tool_call in the history, and a dangling one makes
        // the next request invalid.
        declineRemainingCalls(toolCalls,
            tr("the round budget for this turn is used up; no further tool will run"));
        const QString reason = tr("%1 rounds of tool calls used (%2 of them spent only "
                                  "waiting, which does not count). Answer now from the results "
                                  "already gathered; no further tools will run.")
                                   .arg(m_iteration).arg(m_freeRounds);
        m_messages.append(message(QStringLiteral("system"), reason));
        note(LogLevel::Warning, reason);
        m_finalRound = true;
        sendRound();
        return;
    }

    m_roundDidWork = false;
    m_roundWaitedMs = 0;
    runToolCalls(toolCalls);
    if (!m_roundDidWork && m_roundWaitedMs >= m_waitRoundThresholdMs)
        ++m_freeRounds;
    if (m_busy)
        sendRound();
}

void LlmSession::declineRemainingCalls(const QJsonArray& toolCalls, const QString& reason)
{
    for (const QJsonValue& value : toolCalls) {
        const QJsonObject call = value.toObject();
        const QString name = call.value(QStringLiteral("function")).toObject()
                                 .value(QStringLiteral("name")).toString();
        QJsonObject toolMessage;
        toolMessage.insert(QStringLiteral("role"), QStringLiteral("tool"));
        toolMessage.insert(QStringLiteral("tool_call_id"), call.value(QStringLiteral("id")).toString());
        toolMessage.insert(QStringLiteral("name"), name);
        toolMessage.insert(QStringLiteral("content"), tr("refused: %1").arg(reason));
        m_messages.append(toolMessage);
        emit toolRefused(name, reason);
    }
}

void LlmSession::runToolCalls(const QJsonArray& toolCalls)
{
    for (const QJsonValue& value : toolCalls) {
        const QJsonObject call = value.toObject();
        const QString id = call.value(QStringLiteral("id")).toString();
        const QJsonObject function = call.value(QStringLiteral("function")).toObject();
        const QString name = function.value(QStringLiteral("name")).toString();

        // Arguments arrive as a JSON *string*, not an object. A model that emits
        // something unparseable has to be told, not silently given an empty object.
        const QByteArray raw = function.value(QStringLiteral("arguments")).toString().toUtf8();
        QJsonObject args;
        QString argumentError;
        if (!raw.isEmpty()) {
            QJsonParseError parseError {};
            const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseError);
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
                argumentError = tr("the arguments were not a JSON object: %1")
                                    .arg(parseError.errorString());
            } else {
                args = doc.object();
            }
        }

        const auto reply = [this, &id, &name](bool ok, const QString& payload) {
            QJsonObject toolMessage;
            toolMessage.insert(QStringLiteral("role"), QStringLiteral("tool"));
            toolMessage.insert(QStringLiteral("tool_call_id"), id);
            toolMessage.insert(QStringLiteral("name"), name);
            toolMessage.insert(QStringLiteral("content"), payload);
            m_messages.append(toolMessage);
            Q_UNUSED(ok)
        };

        if (!argumentError.isEmpty()) {
            reply(false, tr("error: %1").arg(argumentError));
            emit toolRefused(name, argumentError);
            continue;
        }

        ToolSpec spec;
        if (!m_registry || !m_registry->tool(name, spec)) {
            const QString error = tr("there is no tool called \"%1\"").arg(name);
            reply(false, tr("error: %1").arg(error));
            emit toolRefused(name, error);
            continue;
        }

        if (needsApproval(spec.effect)) {
            const bool approved = m_approve && m_approve(spec, args);
            if (!approved) {
                const QString reason = m_approve
                    ? tr("the operator declined this call")
                    : tr("%1 tools need approval and no approval policy is set")
                          .arg(toolEffectName(spec.effect));
                reply(false, tr("refused: %1").arg(reason));
                note(LogLevel::Warning, tr("refused %1: %2").arg(name, reason));
                emit toolRefused(name, reason);
                continue;
            }
        }

        emit toolStarted(name, args);
        QElapsedTimer clock;
        clock.start();
        const ToolResult result = m_dispatcher
            ? m_dispatcher->dispatch(name, args)
            : ToolResult::failure(tr("no dispatcher"));
        if (spec.effect == ToolEffect::Read)
            m_roundWaitedMs += clock.elapsed();
        else
            m_roundDidWork = true;   // anything that is not a read is real work
        emit toolFinished(name, result);

        if (!result.ok) {
            reply(false, tr("error: %1").arg(result.error));
            continue;
        }

        QString payload = result.data.isEmpty()
            ? result.text
            : QString::fromUtf8(QJsonDocument(result.data).toJson(QJsonDocument::Compact));
        if (!result.text.isEmpty() && !result.data.isEmpty())
            payload = result.text + QLatin1Char('\n') + payload;

        // A rendered image is not part of the tool result in this protocol: tool
        // messages are text. It has to arrive as its own user message, and only if
        // the model can look at one -- otherwise say so, so it works from the
        // numbers instead of waiting for a picture that never comes.
        if (!result.image.isEmpty()) {
            if (m_visionCapable) {
                payload += tr("\n(the image follows as the next message)");
            } else {
                payload += tr("\n(%1 cannot be shown: the selected model does not take images. "
                              "Work from the coordinates and the measurements instead.)")
                               .arg(name);
            }
        }

        // The history is resent in full every round, so one oversized result is
        // paid for again on every subsequent call.
        if (payload.size() > m_maxToolResultChars) {
            payload = payload.left(m_maxToolResultChars)
                + tr("\n… cut after %1 characters; narrow the query or ask for the next page")
                      .arg(m_maxToolResultChars);
        } else if (result.truncated) {
            payload += tr("\n(the tool had more to give; ask for the next page)");
        }
        reply(true, payload);
        if (!result.image.isEmpty() && m_visionCapable)
            showImage(result.image, result.imageMimeType, name);
    }
}

void LlmSession::showImage(const QByteArray& image, const QString& mimeType,
                           const QString& toolName)
{
    // Only the newest picture stays. The whole history goes out again every round,
    // so an image left in it is paid for on every subsequent one.
    for (int i = m_messages.size() - 1; i >= 0; --i) {
        const QJsonObject entry = m_messages.at(i).toObject();
        if (entry.value(QStringLiteral("role")).toString() != QLatin1String("user"))
            continue;
        const QJsonArray parts = entry.value(QStringLiteral("content")).toArray();
        bool carriesImage = false;
        for (const QJsonValue& part : parts) {
            if (part.toObject().value(QStringLiteral("type")).toString()
                == QLatin1String("image_url")) {
                carriesImage = true;
                break;
            }
        }
        if (carriesImage)
            m_messages.removeAt(i);
    }

    const QString mime = mimeType.isEmpty() ? QStringLiteral("image/png") : mimeType;
    QJsonObject text;
    text.insert(QStringLiteral("type"), QStringLiteral("text"));
    text.insert(QStringLiteral("text"), tr("The image %1 returned.").arg(toolName));
    QJsonObject url;
    url.insert(QStringLiteral("url"), QStringLiteral("data:%1;base64,%2")
                                          .arg(mime, QString::fromLatin1(image.toBase64())));
    QJsonObject picture;
    picture.insert(QStringLiteral("type"), QStringLiteral("image_url"));
    picture.insert(QStringLiteral("image_url"), url);

    QJsonObject entry;
    entry.insert(QStringLiteral("role"), QStringLiteral("user"));
    entry.insert(QStringLiteral("content"), QJsonArray { text, picture });
    m_messages.append(entry);

    note(LogLevel::Info, tr("sent a %1 image (%2 kB) to the model")
                             .arg(mime).arg(image.size() / 1024));
}
