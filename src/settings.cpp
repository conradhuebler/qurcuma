// settings.cpp
#include "settings.h"
#include <QStandardPaths>
#include <QDir>

const QString Settings::WORKING_DIR_KEY = "workingDirectory";
const QString Settings::PROGRAM_PATH_PREFIX = "programs/";

Settings::Settings(QObject *parent)
    : QObject(parent)
    , settings(QSettings::IniFormat, QSettings::UserScope, "Qurcuma", "settings")
{
    // Wenn keine Einstellungen vorhanden, Standardwerte laden
    if (settings.allKeys().isEmpty()) {
        loadDefaults();
    }
}

QString Settings::workingDirectory() const
{
    return settings.value(WORKING_DIR_KEY,
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/qurcuma")
        .toString();
}

void Settings::setWorkingDirectory(const QString &path)
{
    settings.setValue(WORKING_DIR_KEY, path);
    settings.sync();
}

QString Settings::getProgramPath(const QString &program) const
{
    return settings.value(PROGRAM_PATH_PREFIX + program).toString();
}

void Settings::setProgramPath(const QString &program, const QString &path)
{
    settings.setValue(PROGRAM_PATH_PREFIX + program, path);
    settings.sync();
}

QString Settings::orcaBinaryPath() const
{
    return settings.value("orca/binaryPath").toString();
}

void Settings::setOrcaBinaryPath(const QString &path)
{
    settings.setValue("orca/binaryPath", path);
    settings.sync();
}

void Settings::loadDefaults()
{
    // Standard-Arbeitsverzeichnis
    QString defaultWorkDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) 
                           + "/qurcuma";
    
    if (!settings.contains(WORKING_DIR_KEY)) {
        setWorkingDirectory(defaultWorkDir);
    }

    // Standard-Programmpfade basierend auf üblichen Installationsorten
    #ifdef Q_OS_LINUX
    const QString defaultBinPath = "/usr/local/bin/";
    #elif defined(Q_OS_WIN)
    const QString defaultBinPath = "C:/Program Files/";
    #else
    const QString defaultBinPath = "/usr/local/bin/";
    #endif

    QMap<QString, QString> defaultPaths = {
        {"curcuma", defaultBinPath + "curcuma"},
        {"orca", defaultBinPath + "orca"},
        {"xtb", defaultBinPath + "xtb"},
        {"iboview", defaultBinPath + "iboview"},
        {"avogadro", defaultBinPath + "avogadro"}
    };

    // Nur fehlende Programmpfade setzen
    for (auto it = defaultPaths.constBegin(); it != defaultPaths.constEnd(); ++it) {
        QString key = PROGRAM_PATH_PREFIX + it.key();
        if (!settings.contains(key)) {
            settings.setValue(key, it.value());
        }
    }

    settings.sync();
}

void Settings::saveSettings()
{
    settings.sync();
}
