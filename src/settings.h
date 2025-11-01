// settings.h
#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QDateTime>
#include <QVector>

class Settings : public QObject
{
    Q_OBJECT
public:
    explicit Settings(QObject *parent = nullptr);

    // Arbeitsverzeichnis
    QString workingDirectory() const;
    void setWorkingDirectory(const QString &path);

    // Programmpfade
    QString getProgramPath(const QString &program) const;
    void setProgramPath(const QString &program, const QString &path);

    // Spezifisch für orca
    QString orcaBinaryPath() const;
    void setOrcaBinaryPath(const QString &path);


    // Standard-Einstellungen laden/speichern
    void loadDefaults();
    void saveSettings();

    QStringList workingDirectories() const;
    void addWorkingDirectory(const QString& path);
    void removeWorkingDirectory(const QString& path);
    void setLastUsedWorkingDirectory(const QString& path);
    QString lastUsedWorkingDirectory() const;

    // Claude Generated - Quick Win: Recent files
    QStringList recentFiles() const;
    void setRecentFiles(const QStringList& files);

    // Claude Generated Phase 2 - Enhanced recent files with timestamps
    struct RecentFileEntry {
        QString path;
        QDateTime lastAccessed;

        // For JSON serialization
        bool isValid() const { return !path.isEmpty(); }
    };

    QVector<RecentFileEntry> recentFilesV2() const;
    void addRecentFileV2(const QString& path);
    void setRecentFilesV2(const QVector<RecentFileEntry>& files);
    void clearRecentFilesV2();

    // Claude Generated - Visual Polish: Dark mode
    bool darkModeEnabled() const;
    void setDarkMode(bool enabled);

    // Claude Generated - Visualization Settings Persistence
    // Structure to hold all visualization parameters
    struct VisualizationSettings {
        int renderingMode = 0;      // RenderingMode enum value
        int colorScheme = 0;        // ColorScheme enum value
        float atomTransparency = 1.0f;
        float atomShininess = 80.0f;
        float atomScaleFactor = 1.0f;
        float bondThickness = 0.15f;
        bool fogEnabled = false;
        float fogIntensity = 0.5f;
    };

    VisualizationSettings getVisualizationSettings() const;
    void setVisualizationSettings(const VisualizationSettings& settings);

    // Claude Generated - Visualization Preset Management
    struct VisualizationPreset {
        QString name;
        VisualizationSettings settings;
    };

    QVector<VisualizationPreset> getVisualizationPresets();  // Claude Generated - Note: non-const due to QSettings::beginGroup
    void savePreset(const QString& name, const VisualizationSettings& settings);
    void deletePreset(const QString& name);
    bool presetExists(const QString& name) const;
    void initializeDefaultPresets();  // Create built-in presets

private:
    QSettings m_settings;

    // Konstanten für Settings-Keys
    static const QString WORKING_DIR_KEY;
    static const QString PROGRAM_PATH_PREFIX;
    static const QString WORKING_DIRS_KEY;
    static const QString LAST_USED_DIR_KEY;
    static const QString VIZ_SETTINGS_PREFIX;
};

#endif
