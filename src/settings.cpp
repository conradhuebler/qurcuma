// m_settings.cpp
#include "settings.h"
#include <QStandardPaths>
#include <QDir>

const QString Settings::WORKING_DIR_KEY = "workingDirectory";
const QString Settings::PROGRAM_PATH_PREFIX = "programs/";
const QString Settings::WORKING_DIRS_KEY = "workingDirectories";
const QString Settings::LAST_USED_DIR_KEY = "lastUsedWorkingDirectory";
const QString Settings::VIZ_SETTINGS_PREFIX = "visualization/";

Settings::Settings(QObject* parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, "Qurcuma", "qurcuma")
{
    // Claude Generated - Fixed application name from "m_settings" to "qurcuma"
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

// Claude Generated - Quick Win: Recent files
QStringList Settings::recentFiles() const
{
    return m_settings.value("recentFiles", QStringList()).toStringList();
}

void Settings::setRecentFiles(const QStringList& files)
{
    m_settings.setValue("recentFiles", files);
    m_settings.sync();
}

// Claude Generated - Visual Polish: Dark mode
bool Settings::darkModeEnabled() const
{
    return m_settings.value("darkMode", false).toBool();
}

void Settings::setDarkMode(bool enabled)
{
    m_settings.setValue("darkMode", enabled);
    m_settings.sync();
}

// Claude Generated - Visualization Settings Persistence
Settings::VisualizationSettings Settings::getVisualizationSettings() const
{
    VisualizationSettings settings;

    settings.renderingMode = m_settings.value(VIZ_SETTINGS_PREFIX + "renderingMode", 0).toInt();
    settings.colorScheme = m_settings.value(VIZ_SETTINGS_PREFIX + "colorScheme", 0).toInt();
    settings.atomTransparency = m_settings.value(VIZ_SETTINGS_PREFIX + "atomTransparency", 1.0f).toFloat();
    settings.atomShininess = m_settings.value(VIZ_SETTINGS_PREFIX + "atomShininess", 80.0f).toFloat();
    settings.atomScaleFactor = m_settings.value(VIZ_SETTINGS_PREFIX + "atomScaleFactor", 1.0f).toFloat();
    settings.bondThickness = m_settings.value(VIZ_SETTINGS_PREFIX + "bondThickness", 0.15f).toFloat();
    settings.fogEnabled = m_settings.value(VIZ_SETTINGS_PREFIX + "fogEnabled", false).toBool();
    settings.fogIntensity = m_settings.value(VIZ_SETTINGS_PREFIX + "fogIntensity", 0.5f).toFloat();

    return settings;
}

void Settings::setVisualizationSettings(const VisualizationSettings& settings)
{
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "renderingMode", settings.renderingMode);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "colorScheme", settings.colorScheme);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "atomTransparency", settings.atomTransparency);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "atomShininess", settings.atomShininess);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "atomScaleFactor", settings.atomScaleFactor);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "bondThickness", settings.bondThickness);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "fogEnabled", settings.fogEnabled);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "fogIntensity", settings.fogIntensity);
    m_settings.sync();
}