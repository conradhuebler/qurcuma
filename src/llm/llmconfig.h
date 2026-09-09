// llmconfig.h - Endpoint profiles for the LLM client.
// Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 - A plain JSON file at ~/.config/qurcuma/llm.json, never
// QSettings: this is configuration a user edits by hand and copies between
// machines, and burying it in a settings backend makes both awkward.
//
// The file never holds a key. A profile names the ENVIRONMENT VARIABLE the key
// comes from, so the config can be shared, committed to a dotfiles repo, or
// attached to a bug report without leaking anything.
#pragma once

#include <QString>
#include <QVector>

struct LlmProfile {
    QString name;            ///< how the profile is referred to, e.g. "local"
    QString baseUrl;         ///< e.g. "http://localhost:11434/v1"
    QString model;           ///< e.g. "qwen2.5:14b"
    QString apiKeyEnv;       ///< NAME of the env var holding the key; never the key
    bool supportsVision = false;  ///< may be sent images (render_view)
    int maxToolIterations = 12;   ///< stop an agent loop that will not converge
    int requestTimeoutMs = 120000;

    bool isValid() const { return !name.isEmpty() && !baseUrl.isEmpty() && !model.isEmpty(); }
    /// Full endpoint for a chat completion, with exactly one slash at the join.
    QString chatCompletionsUrl() const;
};

class LlmConfig {
public:
    /// ~/.config/qurcuma/llm.json (XDG config location).
    static QString defaultPath();

    /// Read @p path. Returns false and sets @p error on a missing or malformed
    /// file; a missing file is not a crime, so the caller can offer to write an
    /// example instead of failing hard.
    bool load(const QString& path, QString* error = nullptr);
    /// Parse from memory. Used by load() and by the tests.
    bool loadFromJson(const QByteArray& json, QString* error = nullptr);

    /// Write a commented example with one local and one hosted profile. Does
    /// nothing if @p path already exists.
    static bool writeExampleIfMissing(const QString& path, QString* error = nullptr);

    QVector<LlmProfile> profiles() const { return m_profiles; }
    QString activeProfileName() const { return m_active; }
    bool profile(const QString& name, LlmProfile& out) const;
    /// The profile named by "active_profile", or the first one.
    bool activeProfile(LlmProfile& out) const;
    bool isEmpty() const { return m_profiles.isEmpty(); }

    /// Key for @p profile, read from the environment. Empty when the variable is
    /// unset or the profile names none (a local endpoint usually needs no key).
    static QString apiKeyFor(const LlmProfile& profile);

private:
    QVector<LlmProfile> m_profiles;
    QString m_active;
};
