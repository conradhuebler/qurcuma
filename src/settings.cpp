// m_settings.cpp
#include "settings.h"
#include <QStandardPaths>
#include <QDir>

const QString Settings::WORKING_DIR_KEY = "workingDirectory";
const QString Settings::PROGRAM_PATH_PREFIX = "programs/";
const QString Settings::WORKING_DIRS_KEY = "workingDirectories";
const QString Settings::LAST_USED_DIR_KEY = "lastUsedWorkingDirectory";

Settings::Settings(QObject* parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, "Qurcuma", "m_settings")
{
    // Wenn keine Einstellungen vorhanden, Standardwerte laden
    if (m_settings.allKeys().isEmpty()) {
        loadDefaults();
    }
}

QString Settings::workingDirectory() const
{
    return m_settings.value(WORKING_DIR_KEY,
                         QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/qurcuma")
        .toString();
}

void Settings::setWorkingDirectory(const QString &path)
{
    m_settings.setValue(WORKING_DIR_KEY, path);
    m_settings.sync();
}

QString Settings::getProgramPath(const QString &program) const
{
    return m_settings.value(PROGRAM_PATH_PREFIX + program).toString();
}

void Settings::setProgramPath(const QString &program, const QString &path)
{
    m_settings.setValue(PROGRAM_PATH_PREFIX + program, path);
    m_settings.sync();
}

QString Settings::orcaBinaryPath() const
{
    return m_settings.value("orca/binaryPath").toString();
}

void Settings::setOrcaBinaryPath(const QString &path)
{
    m_settings.setValue("orca/binaryPath", path);
    m_settings.sync();
}

void Settings::loadDefaults()
{
    // Standard-Arbeitsverzeichnis
    QString defaultWorkDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) 
                           + "/qurcuma";

    if (!m_settings.contains(WORKING_DIR_KEY)) {
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
        if (!m_settings.contains(key)) {
            m_settings.setValue(key, it.value());
        }
    }

    m_settings.sync();
}

void Settings::saveSettings()
{
    m_settings.sync();
}

QStringList Settings::workingDirectories() const
{
    return m_settings.value(WORKING_DIRS_KEY).toStringList();
}

void Settings::addWorkingDirectory(const QString& path)
{
    QStringList dirs = workingDirectories();
    if (!dirs.contains(path)) {
        dirs.append(path);
        m_settings.setValue(WORKING_DIRS_KEY, dirs);
    }
    setLastUsedWorkingDirectory(path);
}

void Settings::removeWorkingDirectory(const QString& path)
{
    QStringList dirs = workingDirectories();
    dirs.removeAll(path);
    m_settings.setValue(WORKING_DIRS_KEY, dirs);
}

void Settings::setLastUsedWorkingDirectory(const QString& path)
{
    m_settings.setValue(LAST_USED_DIR_KEY, path);
}

QString Settings::lastUsedWorkingDirectory() const
{
    return m_settings.value(LAST_USED_DIR_KEY).toString();
}