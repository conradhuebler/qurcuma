// settings.h
#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QSettings>
#include <QString>

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

private:
    QSettings m_settings;

    // Konstanten für Settings-Keys
    static const QString WORKING_DIR_KEY;
    static const QString PROGRAM_PATH_PREFIX;
    static const QString WORKING_DIRS_KEY;
    static const QString LAST_USED_DIR_KEY;
};

#endif
