// m_settings.cpp
#include "settings.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <algorithm>

const QString Settings::WORKING_DIR_KEY = "workingDirectory";
const QString Settings::PROGRAM_PATH_PREFIX = "programs/";
const QString Settings::WORKING_DIRS_KEY = "workingDirectories";
const QString Settings::LAST_USED_DIR_KEY = "lastUsedWorkingDirectory";
const QString Settings::VIZ_SETTINGS_PREFIX = "visualization/";
const QString Settings::USE_INVOCATION_DIR_KEY = "useInvocationDirectory";  // Claude Generated 2026

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

// Claude Generated 2026 - "Use Invocation Directory" preference
bool Settings::useInvocationDirectoryEnabled() const
{
    return m_settings.value(USE_INVOCATION_DIR_KEY, false).toBool();
}

void Settings::setUseInvocationDirectoryEnabled(bool enabled)
{
    m_settings.setValue(USE_INVOCATION_DIR_KEY, enabled);
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
    // Claude Generated - Phase 5A: Post-processing effects
    settings.ssaoEnabled = m_settings.value(VIZ_SETTINGS_PREFIX + "ssaoEnabled", true).toBool();
    settings.ssaoIntensity = m_settings.value(VIZ_SETTINGS_PREFIX + "ssaoIntensity", 1.0f).toFloat();
    settings.ssaoRadius = m_settings.value(VIZ_SETTINGS_PREFIX + "ssaoRadius", 0.05f).toFloat();
    settings.ssaoBias = m_settings.value(VIZ_SETTINGS_PREFIX + "ssaoBias", 0.025f).toFloat();
    // Claude Generated - Phase 5B: Bloom and HDR post-processing
    settings.bloomEnabled = m_settings.value(VIZ_SETTINGS_PREFIX + "bloomEnabled", true).toBool();
    settings.bloomThreshold = m_settings.value(VIZ_SETTINGS_PREFIX + "bloomThreshold", 0.8f).toFloat();
    settings.bloomIntensity = m_settings.value(VIZ_SETTINGS_PREFIX + "bloomIntensity", 1.0f).toFloat();
    settings.hdrEnabled = m_settings.value(VIZ_SETTINGS_PREFIX + "hdrEnabled", true).toBool();
    settings.exposure = m_settings.value(VIZ_SETTINGS_PREFIX + "exposure", 1.0f).toFloat();
    // Claude Generated 2026 - Interaction / Performance
    settings.rotationMode = m_settings.value(VIZ_SETTINGS_PREFIX + "rotationMode", 0).toInt();
    settings.instancingThreshold = m_settings.value(VIZ_SETTINGS_PREFIX + "instancingThreshold", 500).toInt();
    settings.wallVisible = m_settings.value(VIZ_SETTINGS_PREFIX + "wallVisible", true).toBool();
    settings.wallOpacity = m_settings.value(VIZ_SETTINGS_PREFIX + "wallOpacity", 0.6).toDouble();
    settings.centerOnLoad = m_settings.value(VIZ_SETTINGS_PREFIX + "centerOnLoad", true).toBool();

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
    // Claude Generated - Phase 5A: Post-processing effects
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "ssaoEnabled", settings.ssaoEnabled);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "ssaoIntensity", settings.ssaoIntensity);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "ssaoRadius", settings.ssaoRadius);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "ssaoBias", settings.ssaoBias);
    // Claude Generated - Phase 5B: Bloom and HDR post-processing
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "bloomEnabled", settings.bloomEnabled);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "bloomThreshold", settings.bloomThreshold);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "bloomIntensity", settings.bloomIntensity);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "hdrEnabled", settings.hdrEnabled);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "exposure", settings.exposure);
    // Claude Generated 2026 - Interaction / Performance
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "rotationMode", settings.rotationMode);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "instancingThreshold", settings.instancingThreshold);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "wallVisible", settings.wallVisible);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "wallOpacity", settings.wallOpacity);
    m_settings.setValue(VIZ_SETTINGS_PREFIX + "centerOnLoad", settings.centerOnLoad);
    m_settings.sync();
}

// Claude Generated - Visualization Preset Management
QVector<Settings::VisualizationPreset> Settings::getVisualizationPresets()
{
    QVector<VisualizationPreset> presets;

    m_settings.beginGroup(VIZ_SETTINGS_PREFIX + "presets");
    QStringList presetNames = m_settings.childGroups();

    for (const QString& presetName : presetNames) {
        m_settings.beginGroup(presetName);

        VisualizationPreset preset;
        preset.name = presetName;
        preset.settings.renderingMode = m_settings.value("renderingMode", 0).toInt();
        preset.settings.colorScheme = m_settings.value("colorScheme", 0).toInt();
        preset.settings.atomTransparency = m_settings.value("atomTransparency", 1.0f).toFloat();
        preset.settings.atomShininess = m_settings.value("atomShininess", 80.0f).toFloat();
        preset.settings.atomScaleFactor = m_settings.value("atomScaleFactor", 1.0f).toFloat();
        preset.settings.bondThickness = m_settings.value("bondThickness", 0.15f).toFloat();
        preset.settings.fogEnabled = m_settings.value("fogEnabled", false).toBool();
        preset.settings.fogIntensity = m_settings.value("fogIntensity", 0.5f).toFloat();

        presets.append(preset);
        m_settings.endGroup();
    }

    m_settings.endGroup();
    return presets;
}

void Settings::savePreset(const QString& name, const VisualizationSettings& settings)
{
    QString presetPath = VIZ_SETTINGS_PREFIX + "presets/" + name;

    m_settings.setValue(presetPath + "/renderingMode", settings.renderingMode);
    m_settings.setValue(presetPath + "/colorScheme", settings.colorScheme);
    m_settings.setValue(presetPath + "/atomTransparency", settings.atomTransparency);
    m_settings.setValue(presetPath + "/atomShininess", settings.atomShininess);
    m_settings.setValue(presetPath + "/atomScaleFactor", settings.atomScaleFactor);
    m_settings.setValue(presetPath + "/bondThickness", settings.bondThickness);
    m_settings.setValue(presetPath + "/fogEnabled", settings.fogEnabled);
    m_settings.setValue(presetPath + "/fogIntensity", settings.fogIntensity);

    m_settings.sync();
}

void Settings::deletePreset(const QString& name)
{
    m_settings.remove(VIZ_SETTINGS_PREFIX + "presets/" + name);
    m_settings.sync();
}

bool Settings::presetExists(const QString& name) const
{
    return m_settings.contains(VIZ_SETTINGS_PREFIX + "presets/" + name + "/renderingMode");
}

void Settings::initializeDefaultPresets()
{
    // Only create defaults if no presets exist
    auto presets = getVisualizationPresets();
    if (!presets.isEmpty()) {
        return;
    }

    // Publication: Professional look - CPK colors, Ball-and-stick, high shininess
    VisualizationSettings pubSettings;
    pubSettings.renderingMode = 0;      // BallAndStick
    pubSettings.colorScheme = 0;        // CPK
    pubSettings.atomTransparency = 1.0f;
    pubSettings.atomShininess = 120.0f;
    pubSettings.atomScaleFactor = 1.0f;
    pubSettings.bondThickness = 0.15f;
    pubSettings.fogEnabled = false;
    savePreset("Publication", pubSettings);

    // Analysis: Space-filling with monochrome - good for electron density
    VisualizationSettings analysisSettings;
    analysisSettings.renderingMode = 2;     // SpaceFilling
    analysisSettings.colorScheme = 1;       // Monochrome
    analysisSettings.atomTransparency = 0.8f;
    analysisSettings.atomShininess = 60.0f;
    analysisSettings.atomScaleFactor = 1.0f;
    analysisSettings.bondThickness = 0.1f;
    analysisSettings.fogEnabled = true;
    analysisSettings.fogIntensity = 0.5f;
    savePreset("Analysis", analysisSettings);

    // Presentation: Bright, high transparency, fog for depth
    VisualizationSettings presentSettings;
    presentSettings.renderingMode = 0;      // BallAndStick
    presentSettings.colorScheme = 0;        // CPK
    presentSettings.atomTransparency = 0.7f;
    presentSettings.atomShininess = 100.0f;
    presentSettings.atomScaleFactor = 1.2f;
    presentSettings.bondThickness = 0.18f;
    presentSettings.fogEnabled = true;
    presentSettings.fogIntensity = 0.3f;
    savePreset("Presentation", presentSettings);
}

// Claude Generated Phase 2 - Enhanced recent files with timestamps
QVector<Settings::RecentFileEntry> Settings::recentFilesV2() const
{
    QVector<RecentFileEntry> entries;

    // Try to load new format first
    if (m_settings.contains("recentFilesV2")) {
        // Load from JSON format (simple string parsing)
        // Format: "path1|timestamp1;path2|timestamp2;..."
        QString data = m_settings.value("recentFilesV2", "").toString();
        if (!data.isEmpty()) {
            QStringList entryStrings = data.split(";");
            for (const QString& entryStr : entryStrings) {
                if (entryStr.isEmpty()) continue;
                QStringList parts = entryStr.split("|");
                if (parts.size() == 2) {
                    RecentFileEntry entry;
                    entry.path = parts[0];
                    entry.lastAccessed = QDateTime::fromString(parts[1], Qt::ISODate);
                    if (entry.isValid()) {
                        entries.append(entry);
                    }
                }
            }
            return entries;
        }
    }

    // Migration from old format (QStringList)
    QStringList oldFiles = m_settings.value("recentFiles", QStringList()).toStringList();
    for (const QString& path : oldFiles) {
        RecentFileEntry entry;
        entry.path = path;
        entry.lastAccessed = QDateTime::currentDateTime();
        if (entry.isValid()) {
            entries.append(entry);
        }
    }

    return entries;
}

void Settings::addRecentFileV2(const QString& path)
{
    if (path.isEmpty()) return;

    QVector<RecentFileEntry> entries = recentFilesV2();

    // Remove if already exists
    entries.erase(std::remove_if(entries.begin(), entries.end(),
        [&path](const RecentFileEntry& e) { return e.path == path; }), entries.end());

    // Add to front with current timestamp
    RecentFileEntry newEntry;
    newEntry.path = path;
    newEntry.lastAccessed = QDateTime::currentDateTime();
    entries.prepend(newEntry);

    // Keep only last 10
    while (entries.size() > 10) {
        entries.removeLast();
    }

    setRecentFilesV2(entries);
}

void Settings::setRecentFilesV2(const QVector<RecentFileEntry>& files)
{
    // Save as string format: "path1|timestamp1;path2|timestamp2;..."
    QStringList parts;
    for (const auto& entry : files) {
        if (entry.isValid()) {
            parts.append(entry.path + "|" + entry.lastAccessed.toString(Qt::ISODate));
        }
    }

    m_settings.setValue("recentFilesV2", parts.join(";"));
    m_settings.sync();
}

void Settings::clearRecentFilesV2()
{
    m_settings.remove("recentFilesV2");
    m_settings.sync();
}

// Claude Generated Phase 3 - Bookmark management
QVector<Settings::BookmarkItem> Settings::bookmarks() const
{
    QVector<BookmarkItem> items;

    // Try to load new format first
    if (m_settings.contains("bookmarksV3")) {
        QString data = m_settings.value("bookmarksV3", "").toString();
        if (!data.isEmpty()) {
            // Parse JSON-like format (simplified for now)
            // Format: id|name|path|tags|color|parentId|isFolder|created;...
            QStringList itemStrings = data.split("\n");
            for (const QString& itemStr : itemStrings) {
                if (itemStr.isEmpty()) continue;
                QStringList parts = itemStr.split("|");
                if (parts.size() >= 8) {
                    BookmarkItem item;
                    item.id = parts[0];
                    item.name = parts[1];
                    item.path = parts[2];
                    item.tags = parts[3].split(",");
                    item.color = QColor(parts[4]);
                    item.parentId = parts[5];
                    item.isFolder = parts[6] == "1";
                    item.created = QDateTime::fromString(parts[7], Qt::ISODate);
                    if (item.isValid()) {
                        items.append(item);
                    }
                }
            }
            return items;
        }
    }

    // If new format doesn't exist, try to load from legacy format
    QStringList oldDirs = m_settings.value(WORKING_DIRS_KEY).toStringList();
    for (const QString& path : oldDirs) {
        if (path.isEmpty()) continue;

        BookmarkItem item;
        item.id = QUuid::createUuid().toString();
        // Extract directory name from path
        QFileInfo fi(path);
        item.name = fi.fileName().isEmpty() ? QDir(path).dirName() : fi.fileName();
        item.path = path;
        item.tags = QStringList();
        item.color = QColor();
        item.parentId = "";
        item.isFolder = false;
        item.created = QDateTime::currentDateTime();

        if (item.isValid()) {
            items.append(item);
        }
    }

    return items;
}

void Settings::setBookmarks(const QVector<BookmarkItem>& items)
{
    QStringList parts;
    for (const auto& item : items) {
        if (item.isValid()) {
            parts.append(item.id + "|" + item.name + "|" + item.path + "|" +
                        item.tags.join(",") + "|" + item.color.name() + "|" +
                        item.parentId + "|" + (item.isFolder ? "1" : "0") + "|" +
                        item.created.toString(Qt::ISODate));
        }
    }

    m_settings.setValue("bookmarksV3", parts.join("\n"));
    m_settings.sync();
}

void Settings::addBookmark(const BookmarkItem& item)
{
    QVector<BookmarkItem> items = bookmarks();

    // Check if ID already exists
    for (auto& existing : items) {
        if (existing.id == item.id) {
            existing = item;
            setBookmarks(items);
            return;
        }
    }

    // Add new
    items.append(item);
    setBookmarks(items);
}

void Settings::removeBookmark(const QString& id)
{
    QVector<BookmarkItem> items = bookmarks();
    items.erase(std::remove_if(items.begin(), items.end(),
        [&id](const BookmarkItem& item) { return item.id == id; }), items.end());
    setBookmarks(items);
}

void Settings::updateBookmark(const QString& id, const BookmarkItem& newItem)
{
    QVector<BookmarkItem> items = bookmarks();
    for (auto& item : items) {
        if (item.id == id) {
            item = newItem;
            break;
        }
    }
    setBookmarks(items);
}


// Claude Generated Phase 4 - Workspace management
QVector<Settings::Workspace> Settings::workspaces() const
{
    QVector<Workspace> workspaces;

    if (m_settings.contains("workspacesV1")) {
        QString data = m_settings.value("workspacesV1", "").toString();
        if (!data.isEmpty()) {
            // Parse format: id|name|description|workdir|calcDirs|geometry|splitterState|created|lastUsed;...
            QStringList wsStrings = data.split("\n");
            for (const QString& wsStr : wsStrings) {
                if (wsStr.isEmpty()) continue;
                QStringList parts = wsStr.split("|");
                if (parts.size() >= 9) {
                    Workspace ws;
                    ws.id = parts[0];
                    ws.name = parts[1];
                    ws.description = parts[2];
                    ws.workingDirectory = parts[3];
                    ws.openCalculations = parts[4].split(",");
                    ws.windowGeometry = QByteArray::fromHex(parts[5].toLatin1());
                    ws.dockState = QByteArray::fromHex(parts[6].toLatin1());  // Claude Generated - UI Restructuring
                    ws.created = QDateTime::fromString(parts[7], Qt::ISODate);
                    ws.lastUsed = QDateTime::fromString(parts[8], Qt::ISODate);
                    if (ws.isValid()) {
                        workspaces.append(ws);
                    }
                }
            }
        }
    }

    return workspaces;
}

void Settings::saveWorkspace(const Workspace& ws)
{
    QVector<Workspace> workspaces_list = workspaces();

    // Check if exists and update, else append
    bool found = false;
    for (auto& existing : workspaces_list) {
        if (existing.id == ws.id) {
            existing = ws;
            found = true;
            break;
        }
    }

    if (!found) {
        workspaces_list.append(ws);
    }

    // Serialize all
    QStringList parts;
    for (const auto& w : workspaces_list) {
        if (w.isValid()) {
            parts.append(w.id + "|" + w.name + "|" + w.description + "|" +
                        w.workingDirectory + "|" + w.openCalculations.join(",") + "|" +
                        QString(w.windowGeometry.toHex()) + "|" +
                        QString(w.dockState.toHex()) + "|" +  // Claude Generated - UI Restructuring
                        w.created.toString(Qt::ISODate) + "|" +
                        w.lastUsed.toString(Qt::ISODate));
        }
    }

    m_settings.setValue("workspacesV1", parts.join("\n"));
    m_settings.sync();
}

void Settings::deleteWorkspace(const QString& id)
{
    QVector<Workspace> workspaces_list = workspaces();
    workspaces_list.erase(std::remove_if(workspaces_list.begin(), workspaces_list.end(),
        [&id](const Workspace& ws) { return ws.id == id; }), workspaces_list.end());

    // Serialize remaining
    QStringList parts;
    for (const auto& w : workspaces_list) {
        if (w.isValid()) {
            parts.append(w.id + "|" + w.name + "|" + w.description + "|" +
                        w.workingDirectory + "|" + w.openCalculations.join(",") + "|" +
                        QString(w.windowGeometry.toHex()) + "|" +
                        QString(w.dockState.toHex()) + "|" +  // Claude Generated - UI Restructuring
                        w.created.toString(Qt::ISODate) + "|" +
                        w.lastUsed.toString(Qt::ISODate));
        }
    }

    if (parts.isEmpty()) {
        m_settings.remove("workspacesV1");
    } else {
        m_settings.setValue("workspacesV1", parts.join("\n"));
    }
    m_settings.sync();
}

Settings::Workspace Settings::loadWorkspace(const QString& id) const
{
    QVector<Workspace> workspaces_list = workspaces();
    for (const auto& ws : workspaces_list) {
        if (ws.id == id) {
            return ws;
        }
    }
    return Workspace();  // Return empty workspace if not found
}

void Settings::updateWorkspaceLastUsed(const QString& id)
{
    Workspace ws = loadWorkspace(id);
    if (ws.isValid()) {
        ws.lastUsed = QDateTime::currentDateTime();
        saveWorkspace(ws);
    }
}

QString Settings::lastActiveWorkspaceId() const
{
    return m_settings.value("lastActiveWorkspaceId", "").toString();
}

void Settings::setLastActiveWorkspaceId(const QString& id)
{
    m_settings.setValue("lastActiveWorkspaceId", id);
    m_settings.sync();
}

bool Settings::autoSaveWorkspaceEnabled() const
{
    return m_settings.value("autoSaveWorkspace", false).toBool();
}

void Settings::setAutoSaveWorkspace(bool enabled)
{
    m_settings.setValue("autoSaveWorkspace", enabled);
    m_settings.sync();
}

bool Settings::restoreLastWorkspaceEnabled() const
{
    return m_settings.value("restoreLastWorkspace", false).toBool();
}

void Settings::setRestoreLastWorkspace(bool enabled)
{
    m_settings.setValue("restoreLastWorkspace", enabled);
    m_settings.sync();
}

// Claude Generated - Phase SFTP Integration: Connection profile management
QVector<Settings::SftpConnectionProfile> Settings::sftpProfiles() const
{
    QVector<SftpConnectionProfile> profiles;

    if (m_settings.contains("sftpProfilesV1")) {
        QString data = m_settings.value("sftpProfilesV1", "").toString();
        if (!data.isEmpty()) {
            // Parse format: id|name|host|username|port|useSSHConfig|useKeyAuth|keyPath|created|lastUsed;...
            QStringList profileStrings = data.split("\n");
            for (const QString& profileStr : profileStrings) {
                if (profileStr.isEmpty()) continue;
                QStringList parts = profileStr.split("|");
                if (parts.size() >= 10) {
                    SftpConnectionProfile profile;
                    profile.id = parts[0];
                    profile.name = parts[1];
                    profile.host = parts[2];
                    profile.username = parts[3];
                    profile.port = parts[4].toInt();
                    profile.useSSHConfig = (parts[5] == "1");
                    profile.useKeyAuth = (parts[6] == "1");
                    profile.keyPath = parts[7];
                    profile.created = QDateTime::fromString(parts[8], Qt::ISODate);
                    profile.lastUsed = QDateTime::fromString(parts[9], Qt::ISODate);
                    if (profile.isValid()) {
                        profiles.append(profile);
                    }
                }
            }
        }
    }

    return profiles;
}

void Settings::setSftpProfiles(const QVector<SftpConnectionProfile>& profiles)
{
    QStringList parts;
    for (const auto& profile : profiles) {
        if (profile.isValid()) {
            parts.append(profile.id + "|" + profile.name + "|" + profile.host + "|" +
                        profile.username + "|" + QString::number(profile.port) + "|" +
                        (profile.useSSHConfig ? "1" : "0") + "|" +
                        (profile.useKeyAuth ? "1" : "0") + "|" +
                        profile.keyPath + "|" +
                        profile.created.toString(Qt::ISODate) + "|" +
                        profile.lastUsed.toString(Qt::ISODate));
        }
    }

    if (parts.isEmpty()) {
        m_settings.remove("sftpProfilesV1");
    } else {
        m_settings.setValue("sftpProfilesV1", parts.join("\n"));
    }
    m_settings.sync();
}

void Settings::addSftpProfile(const SftpConnectionProfile& profile)
{
    QVector<SftpConnectionProfile> profiles = sftpProfiles();

    // Check if profile with same ID already exists
    bool found = false;
    for (auto& existing : profiles) {
        if (existing.id == profile.id) {
            existing = profile;
            found = true;
            break;
        }
    }

    if (!found) {
        profiles.append(profile);
    }

    setSftpProfiles(profiles);
}

void Settings::removeSftpProfile(const QString& id)
{
    QVector<SftpConnectionProfile> profiles = sftpProfiles();
    profiles.erase(std::remove_if(profiles.begin(), profiles.end(),
        [&id](const SftpConnectionProfile& profile) { return profile.id == id; }), profiles.end());
    setSftpProfiles(profiles);
}

void Settings::updateSftpProfile(const QString& id, const SftpConnectionProfile& newProfile)
{
    QVector<SftpConnectionProfile> profiles = sftpProfiles();
    for (auto& profile : profiles) {
        if (profile.id == id) {
            profile = newProfile;
            break;
        }
    }
    setSftpProfiles(profiles);
}

void Settings::updateSftpProfileLastUsed(const QString& id)
{
    QVector<SftpConnectionProfile> profiles = sftpProfiles();
    for (auto& profile : profiles) {
        if (profile.id == id) {
            profile.lastUsed = QDateTime::currentDateTime();
            break;
        }
    }
    setSftpProfiles(profiles);
}

QVector<Settings::SftpConnectionProfile> Settings::getRecentSftpConnections(int limit) const
{
    QVector<SftpConnectionProfile> profiles = sftpProfiles();

    // Sort by lastUsed timestamp (most recent first)
    std::sort(profiles.begin(), profiles.end(),
        [](const SftpConnectionProfile& a, const SftpConnectionProfile& b) {
            return a.lastUsed > b.lastUsed;
        });

    // Return only the most recent 'limit' profiles
    if (profiles.size() > limit) {
        profiles.resize(limit);
    }

    return profiles;
}

// Claude Generated - Remote Directory Mounting: Persistent remote mounts
QVector<Settings::RemoteMountPoint> Settings::remoteMounts() const
{
    QVector<RemoteMountPoint> mounts;

    if (m_settings.contains("remoteMountsV1")) {
        QString data = m_settings.value("remoteMountsV1", "").toString();
        if (!data.isEmpty()) {
            // Parse format: id|name|profileId|remotePath|mounted|lastAccessed
            QStringList mountStrings = data.split("\n");
            for (const QString& mountStr : mountStrings) {
                if (mountStr.isEmpty()) continue;
                QStringList parts = mountStr.split("|");
                if (parts.size() >= 6) {
                    RemoteMountPoint mount;
                    mount.id = parts[0];
                    mount.name = parts[1];
                    mount.profileId = parts[2];
                    mount.remotePath = parts[3];
                    mount.mounted = QDateTime::fromString(parts[4], Qt::ISODate);
                    mount.lastAccessed = QDateTime::fromString(parts[5], Qt::ISODate);
                    if (mount.isValid()) {
                        mounts.append(mount);
                    }
                }
            }
        }
    }

    return mounts;
}

void Settings::setRemoteMounts(const QVector<RemoteMountPoint>& mounts)
{
    QStringList parts;
    for (const auto& mount : mounts) {
        if (mount.isValid()) {
            parts.append(mount.id + "|" + mount.name + "|" + mount.profileId + "|" +
                        mount.remotePath + "|" +
                        mount.mounted.toString(Qt::ISODate) + "|" +
                        mount.lastAccessed.toString(Qt::ISODate));
        }
    }

    if (parts.isEmpty()) {
        m_settings.remove("remoteMountsV1");
    } else {
        m_settings.setValue("remoteMountsV1", parts.join("\n"));
    }
    m_settings.sync();
}

void Settings::addRemoteMount(const RemoteMountPoint& mount)
{
    QVector<RemoteMountPoint> mounts = remoteMounts();

    // Check if mount with same ID already exists
    bool found = false;
    for (auto& existing : mounts) {
        if (existing.id == mount.id) {
            existing = mount;
            found = true;
            break;
        }
    }

    if (!found) {
        mounts.append(mount);
    }

    setRemoteMounts(mounts);
}

void Settings::removeRemoteMount(const QString& id)
{
    QVector<RemoteMountPoint> mounts = remoteMounts();
    mounts.erase(std::remove_if(mounts.begin(), mounts.end(),
        [&id](const RemoteMountPoint& mount) { return mount.id == id; }), mounts.end());
    setRemoteMounts(mounts);
}

void Settings::updateRemoteMountLastAccessed(const QString& id)
{
    QVector<RemoteMountPoint> mounts = remoteMounts();
    for (auto& mount : mounts) {
        if (mount.id == id) {
            mount.lastAccessed = QDateTime::currentDateTime();
            break;
        }
    }
    setRemoteMounts(mounts);
}