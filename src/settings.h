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

private:
    QSettings settings;

    // Konstanten für Settings-Keys
    static const QString WORKING_DIR_KEY;
    static const QString PROGRAM_PATH_PREFIX;
};

#endif
