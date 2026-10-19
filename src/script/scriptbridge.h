// scriptbridge.h - The one crossing from a script back into qurcuma.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - A script that may call tools needs a C++ function it can
// reach. QJSEngine in Qt 6.11 cannot create one from C++ (there is no newFunction,
// neither in the installed header nor in the installed documentation), so the call
// goes through a QObject with a single Q_INVOKABLE method: the JS engine converts the
// arguments and the returned QVariant on its own.
//
// The bridge is installed exactly when the caller passes a ScriptHost, which is what
// keeps the two paths apart: an interpreter for a dock script has one, the one behind
// the assistant's calculate tool is built without.
#pragma once

#include "scriptinterpreter.h"

#include <QObject>
#include <QVariantMap>

/// Wraps a ScriptHost for JavaScript. Lives only as long as the run that installed it.
class ScriptBridge : public QObject {
    Q_OBJECT

public:
    explicit ScriptBridge(ScriptHost host, QObject* parent = nullptr)
        : QObject(parent)
        , m_host(std::move(host))
    {
    }

    /// What a script reaches as tool(name, args). The host's answer is returned
    /// unchanged, so a script branches on its "error" member rather than on an
    /// exception; that is what a refused call has to be able to do.
    Q_INVOKABLE QVariant tool(const QString& name, const QVariantMap& args)
    {
        if (!m_host.call) {
            return QVariantMap {
                { QStringLiteral("error"),
                    QStringLiteral("this script cannot call tools") }
            };
        }
        if (name.isEmpty()) {
            return QVariantMap {
                { QStringLiteral("error"), QStringLiteral("tool needs a name") }
            };
        }
        return m_host.call(name, args);
    }

private:
    ScriptHost m_host;
};
