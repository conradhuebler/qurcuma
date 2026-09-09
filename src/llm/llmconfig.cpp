// llmconfig.cpp - Endpoint profiles for the LLM client.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026

#include "llmconfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

QString LlmProfile::chatCompletionsUrl() const
{
    QString base = baseUrl;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    return base + QStringLiteral("/chat/completions");
}

QString LlmConfig::defaultPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(dir).filePath(QStringLiteral("llm.json"));
}

bool LlmConfig::loadFromJson(const QByteArray& json, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };

    QJsonParseError parseError {};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(QStringLiteral("not valid JSON: %1").arg(parseError.errorString()));
    if (!doc.isObject())
        return fail(QStringLiteral("the top level must be an object"));

    const QJsonObject root = doc.object();
    const QJsonArray profiles = root.value(QStringLiteral("profiles")).toArray();
    if (profiles.isEmpty())
        return fail(QStringLiteral("no \"profiles\" array, or it is empty"));

    QVector<LlmProfile> parsed;
    for (const QJsonValue& value : profiles) {
        const QJsonObject o = value.toObject();
        LlmProfile p;
        p.name = o.value(QStringLiteral("name")).toString();
        p.baseUrl = o.value(QStringLiteral("base_url")).toString();
        p.model = o.value(QStringLiteral("model")).toString();
        p.apiKeyEnv = o.value(QStringLiteral("api_key_env")).toString();
        p.supportsVision = o.value(QStringLiteral("supports_vision")).toBool(false);
        p.maxToolIterations = o.value(QStringLiteral("max_tool_iterations")).toInt(12);
        p.requestTimeoutMs = o.value(QStringLiteral("request_timeout_ms")).toInt(120000);

        // A key in the file is the one mistake worth refusing outright rather than
        // warning about: the file gets shared, and a warning gets missed.
        if (o.contains(QStringLiteral("api_key"))) {
            return fail(QStringLiteral(
                "profile \"%1\" carries an \"api_key\". Keys never belong in this file -- "
                "use \"api_key_env\" to name the environment variable instead.")
                            .arg(p.name.isEmpty() ? QStringLiteral("<unnamed>") : p.name));
        }
        if (!p.isValid()) {
            return fail(QStringLiteral("profile \"%1\" needs name, base_url and model")
                            .arg(p.name.isEmpty() ? QStringLiteral("<unnamed>") : p.name));
        }
        parsed.append(p);
    }

    m_profiles = parsed;
    m_active = root.value(QStringLiteral("active_profile")).toString();
    if (error)
        error->clear();
    return true;
}

bool LlmConfig::load(const QString& path, QString* error)
{
    QFile file(path);
    if (!file.exists()) {
        if (error)
            *error = QStringLiteral("%1 does not exist").arg(path);
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("cannot read %1: %2").arg(path, file.errorString());
        return false;
    }
    return loadFromJson(file.readAll(), error);
}

bool LlmConfig::writeExampleIfMissing(const QString& path, QString* error)
{
    if (QFileInfo::exists(path))
        return true;

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("cannot write %1: %2").arg(path, file.errorString());
        return false;
    }

    // JSON has no comments, so the note is a field. Better an odd-looking key than
    // a file nobody knows how to fill in.
    static const char* kExample = R"JSON({
  "_note": [
    "Endpoint profiles for qurcuma's LLM support.",
    "No API key belongs in this file. 'api_key_env' names the environment variable",
    "the key is read from, so this file can be shared or committed safely.",
    "Any OpenAI-compatible endpoint works; base_url is the part before /chat/completions.",
    "The model must actually exist at that endpoint -- for Ollama, check 'ollama list'.",
    "Tool calling depends on the model, not on the endpoint: a model that cannot call tools",
    "will answer in prose and never touch the structure."
  ],
  "active_profile": "local",
  "profiles": [
    {
      "name": "local",
      "base_url": "http://localhost:11434/v1",
      "model": "qwen2.5:14b",
      "api_key_env": "",
      "supports_vision": false,
      "max_tool_iterations": 12
    },
    {
      "name": "hosted",
      "base_url": "https://api.openai.com/v1",
      "model": "gpt-4o-mini",
      "api_key_env": "OPENAI_API_KEY",
      "supports_vision": true,
      "max_tool_iterations": 12
    }
  ]
}
)JSON";
    file.write(kExample);
    file.close();
    if (error)
        error->clear();
    return true;
}

bool LlmConfig::profile(const QString& name, LlmProfile& out) const
{
    for (const LlmProfile& p : m_profiles) {
        if (p.name == name) {
            out = p;
            return true;
        }
    }
    return false;
}

bool LlmConfig::activeProfile(LlmProfile& out) const
{
    if (m_profiles.isEmpty())
        return false;
    if (!m_active.isEmpty() && profile(m_active, out))
        return true;
    out = m_profiles.first();
    return true;
}

QString LlmConfig::apiKeyFor(const LlmProfile& profile)
{
    if (profile.apiKeyEnv.isEmpty())
        return QString();
    return qEnvironmentVariable(profile.apiKeyEnv.toLocal8Bit().constData());
}
