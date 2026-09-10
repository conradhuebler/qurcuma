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

#include <QMetaType>
#include <QString>
#include <QVector>

/// What an endpoint says about one of its models.
///
/// Claude Generated 2026 - The OpenAI /v1/models listing is only a list of names.
/// Ollama additionally answers /api/show with the context length and, more
/// usefully, a capability list: whether the model can call tools at all and
/// whether it can see images. Both are asked lazily for the selected model, and
/// both degrade to "unknown" against an endpoint that does not offer them.
struct LlmModelInfo {
    QString id;
    int contextLength = 0;        ///< tokens; 0 = not reported
    bool supportsTools = false;
    bool supportsVision = false;
    bool detailsKnown = false;    ///< false when the endpoint told us nothing
};

struct LlmProfile {
    QString name;            ///< how the profile is referred to, e.g. "local"
    QString baseUrl;         ///< e.g. "http://localhost:11434/v1"
    /// Preferred model. May be empty: which models exist is a property of the
    /// endpoint, not of this file, and guessing one produces a first run that
    /// fails for no reason. The client asks the endpoint and the user picks.
    QString model;
    QString apiKeyEnv;       ///< NAME of the env var holding the key; never the key
    bool supportsVision = false;  ///< may be sent images (render_view)
    bool stream = true;           ///< SSE; off for an endpoint that cannot do it
    int maxToolIterations = 12;   ///< stop an agent loop that will not converge
    int requestTimeoutMs = 120000;

    bool isValid() const { return !name.isEmpty() && !baseUrl.isEmpty(); }
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

    /// Point @p profileName at another endpoint and write the file back.
    ///
    /// Claude Generated 2026 - Surgical on purpose: the file is re-read, only that
    /// one profile's "base_url" is replaced, and everything else is written back as
    /// it was -- the explanatory notes, the other profiles, and above all the
    /// api_key_env spelling, which must never be turned into a key by a round trip
    /// through this program. Moving an Ollama server to another host is a one-line
    /// change and should not need a text editor.
    static bool updateBaseUrl(const QString& path, const QString& profileName,
                              const QString& baseUrl, QString* error = nullptr);

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

// Claude Generated 2026 - Crosses from the client's thread to the GUI.
Q_DECLARE_METATYPE(LlmModelInfo)
