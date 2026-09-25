// m_settings.cpp
#include "settings.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

const QString Settings::WORKING_DIR_KEY = "workingDirectory";
const QString Settings::PROGRAM_PATH_PREFIX = "programs/";
const QString Settings::WORKING_DIRS_KEY = "workingDirectories";
const QString Settings::LAST_USED_DIR_KEY = "lastUsedWorkingDirectory";
const QString Settings::VIZ_SETTINGS_PREFIX = "visualization/";
const QString Settings::USE_INVOCATION_DIR_KEY = "useInvocationDirectory";  // Claude Generated 2026
const QString Settings::VIEW_PRESETS_PREFIX = "viewPresets/";  // Claude Generated 2026
// Claude Generated 2026 - exact legacy key strings (see settings.h note).
const QString Settings::ORCA_BINARY_KEY = "orca/binaryPath";
const QString Settings::DARK_MODE_KEY = "darkMode";
const QString Settings::RECENT_FILES_LEGACY_KEY = "recentFiles";
const QString Settings::RECENT_FILES_V2_KEY = "recentFilesV2";
const QString Settings::BOOKMARKS_KEY = "bookmarksV3";
const QString Settings::WORKSPACES_KEY = "workspacesV1";
const QString Settings::LAST_ACTIVE_WORKSPACE_KEY = "lastActiveWorkspaceId";
const QString Settings::AUTO_SAVE_WORKSPACE_KEY = "autoSaveWorkspace";
const QString Settings::RESTORE_LAST_WORKSPACE_KEY = "restoreLastWorkspace";
const QString Settings::OPERATOR_NAME_KEY = "operator/name";
const QString Settings::OPERATOR_ORCID_KEY = "operator/orcid";
const QString Settings::OPERATOR_INSTITUTION_KEY = "operator/institution";
const QString Settings::OPERATOR_LICENSE_KEY = "operator/license";
const QString Settings::SFTP_PROFILES_KEY = "sftpProfilesV1";
const QString Settings::REMOTE_MOUNTS_KEY = "remoteMountsV1";

// Claude Generated 2026 - Structured (JSON) persistence for the record lists
// (bookmarks / workspaces / SFTP profiles / remote mounts). Replaces the old
// '|'- and '\n'-delimited encoding, which silently corrupted any field that
// contained a delimiter and broke on positional-index drift. Each list is stored
// as a compact JSON array string under its existing QSettings key. There is
// intentionally NO backward read of the legacy delimited format (a one-time
// settings reset for these lists is acceptable).
namespace {

QJsonArray readJsonArray(const QSettings& settings, const QString& key)
{
    return QJsonDocument::fromJson(settings.value(key).toString().toUtf8()).array();
}

void writeJsonArray(QSettings& settings, const QString& key, const QJsonArray& arr)
{
    if (arr.isEmpty())
        settings.remove(key);
    else
        settings.setValue(key, QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

QJsonObject bookmarkToJson(const Settings::BookmarkItem& b)
{
    QJsonObject o;
    o["id"] = b.id;
    o["name"] = b.name;
    o["path"] = b.path;
    o["tags"] = QJsonArray::fromStringList(b.tags);
    o["color"] = b.color.isValid() ? b.color.name() : QString();
    o["parentId"] = b.parentId;
    o["isFolder"] = b.isFolder;
    o["created"] = b.created.toString(Qt::ISODate);
    return o;
}

Settings::BookmarkItem bookmarkFromJson(const QJsonObject& o)
{
    Settings::BookmarkItem b;
    b.id = o["id"].toString();
    b.name = o["name"].toString();
    b.path = o["path"].toString();
    QStringList tags;
    for (const QJsonValue& v : o["tags"].toArray())
        tags << v.toString();
    b.tags = tags;
    b.color = QColor(o["color"].toString());
    b.parentId = o["parentId"].toString();
    b.isFolder = o["isFolder"].toBool();
    b.created = QDateTime::fromString(o["created"].toString(), Qt::ISODate);
    return b;
}

QJsonObject workspaceToJson(const Settings::Workspace& w)
{
    QJsonObject o;
    o["id"] = w.id;
    o["name"] = w.name;
    o["description"] = w.description;
    o["workingDirectory"] = w.workingDirectory;
    o["openCalculations"] = QJsonArray::fromStringList(w.openCalculations);
    o["windowGeometry"] = QString::fromLatin1(w.windowGeometry.toBase64());
    o["dockState"] = QString::fromLatin1(w.dockState.toBase64());
    o["created"] = w.created.toString(Qt::ISODate);
    o["lastUsed"] = w.lastUsed.toString(Qt::ISODate);
    return o;
}

Settings::Workspace workspaceFromJson(const QJsonObject& o)
{
    Settings::Workspace w;
    w.id = o["id"].toString();
    w.name = o["name"].toString();
    w.description = o["description"].toString();
    w.workingDirectory = o["workingDirectory"].toString();
    QStringList calcs;
    for (const QJsonValue& v : o["openCalculations"].toArray())
        calcs << v.toString();
    w.openCalculations = calcs;
    w.windowGeometry = QByteArray::fromBase64(o["windowGeometry"].toString().toLatin1());
    w.dockState = QByteArray::fromBase64(o["dockState"].toString().toLatin1());
    w.created = QDateTime::fromString(o["created"].toString(), Qt::ISODate);
    w.lastUsed = QDateTime::fromString(o["lastUsed"].toString(), Qt::ISODate);
    return w;
}

QJsonObject recentFileToJson(const Settings::RecentFileEntry& e)
{
    QJsonObject o;
    o["path"] = e.path;
    o["lastAccessed"] = e.lastAccessed.toString(Qt::ISODate);
    return o;
}

Settings::RecentFileEntry recentFileFromJson(const QJsonObject& o)
{
    Settings::RecentFileEntry e;
    e.path = o["path"].toString();
    e.lastAccessed = QDateTime::fromString(o["lastAccessed"].toString(), Qt::ISODate);
    return e;
}

#ifdef USE_SFTP
QJsonObject sftpProfileToJson(const Settings::SftpConnectionProfile& p)
{
    QJsonObject o;
    o["id"] = p.id;
    o["name"] = p.name;
    o["host"] = p.host;
    o["username"] = p.username;
    o["port"] = p.port;
    o["useSSHConfig"] = p.useSSHConfig;
    o["useKeyAuth"] = p.useKeyAuth;
    o["keyPath"] = p.keyPath;
    o["created"] = p.created.toString(Qt::ISODate);
    o["lastUsed"] = p.lastUsed.toString(Qt::ISODate);
    return o;
}

Settings::SftpConnectionProfile sftpProfileFromJson(const QJsonObject& o)
{
    Settings::SftpConnectionProfile p;
    p.id = o["id"].toString();
    p.name = o["name"].toString();
    p.host = o["host"].toString();
    p.username = o["username"].toString();
    p.port = o["port"].toInt(22);
    p.useSSHConfig = o["useSSHConfig"].toBool();
    p.useKeyAuth = o["useKeyAuth"].toBool();
    p.keyPath = o["keyPath"].toString();
    p.created = QDateTime::fromString(o["created"].toString(), Qt::ISODate);
    p.lastUsed = QDateTime::fromString(o["lastUsed"].toString(), Qt::ISODate);
    return p;
}

QJsonObject mountToJson(const Settings::RemoteMountPoint& m)
{
    QJsonObject o;
    o["id"] = m.id;
    o["name"] = m.name;
    o["profileId"] = m.profileId;
    o["remotePath"] = m.remotePath;
    o["mounted"] = m.mounted.toString(Qt::ISODate);
    o["lastAccessed"] = m.lastAccessed.toString(Qt::ISODate);
    return o;
}

Settings::RemoteMountPoint mountFromJson(const QJsonObject& o)
{
    Settings::RemoteMountPoint m;
    m.id = o["id"].toString();
    m.name = o["name"].toString();
    m.profileId = o["profileId"].toString();
    m.remotePath = o["remotePath"].toString();
    m.mounted = QDateTime::fromString(o["mounted"].toString(), Qt::ISODate);
    m.lastAccessed = QDateTime::fromString(o["lastAccessed"].toString(), Qt::ISODate);
    return m;
}
#endif // USE_SFTP

} // namespace

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
    return m_settings.value(ORCA_BINARY_KEY).toString();
}

void Settings::setOrcaBinaryPath(const QString &path)
{
    m_settings.setValue(ORCA_BINARY_KEY, path);
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

// Note: the legacy "recentFiles" QStringList key is still read once by
// recentFilesV2() for one-time migration; the old V1 accessors were unused
// and removed. Claude Generated 2026.

// Claude Generated - Visual Polish: Dark mode
bool Settings::darkModeEnabled() const
{
    return m_settings.value(DARK_MODE_KEY, false).toBool();
}

void Settings::setDarkMode(bool enabled)
{
    m_settings.setValue(DARK_MODE_KEY, enabled);
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
namespace {
// Claude Generated 2026 - One read/writer for a full VisualizationSettings block
// under an arbitrary key prefix. The live settings (prefix "visualization/") and
// every named preset ("visualization/presets/<name>/") now persist the SAME
// complete field set; presets previously saved only 8 fields and silently dropped
// SSAO/bloom/HDR/rotation/walls/... The reader falls back to the struct's own
// default-initialised values, so defaults live only in DisplaySettings.
void writeVizSettings(QSettings& s, const QString& prefix, const Settings::VisualizationSettings& v)
{
    s.setValue(prefix + "renderingMode", v.renderingMode);
    s.setValue(prefix + "colorScheme", v.colorScheme);
    s.setValue(prefix + "atomTransparency", v.atomTransparency);
    s.setValue(prefix + "atomShininess", v.atomShininess);
    s.setValue(prefix + "atomScaleFactor", v.atomScaleFactor);
    s.setValue(prefix + "bondThickness", v.bondThickness);
    s.setValue(prefix + "fogEnabled", v.fogEnabled);
    s.setValue(prefix + "fogIntensity", v.fogIntensity);
    s.setValue(prefix + "ssaoEnabled", v.ssaoEnabled);
    s.setValue(prefix + "ssaoIntensity", v.ssaoIntensity);
    s.setValue(prefix + "ssaoRadius", v.ssaoRadius);
    s.setValue(prefix + "ssaoBias", v.ssaoBias);
    s.setValue(prefix + "bloomEnabled", v.bloomEnabled);
    s.setValue(prefix + "bloomThreshold", v.bloomThreshold);
    s.setValue(prefix + "bloomIntensity", v.bloomIntensity);
    s.setValue(prefix + "hdrEnabled", v.hdrEnabled);
    s.setValue(prefix + "exposure", v.exposure);
    s.setValue(prefix + "rotationMode", v.rotationMode);
    s.setValue(prefix + "instancingThreshold", v.instancingThreshold);
    s.setValue(prefix + "wallVisible", v.wallVisible);
    s.setValue(prefix + "wallOpacity", v.wallOpacity);
    s.setValue(prefix + "centerOnLoad", v.centerOnLoad);
    s.setValue(prefix + "nciSource", v.nciSource);
    s.setValue(prefix + "nciHydrogenBonds", v.nciHydrogenBonds);
    s.setValue(prefix + "nciHalogenBonds", v.nciHalogenBonds);
    s.setValue(prefix + "nciPiStacking", v.nciPiStacking);
    s.setValue(prefix + "nciCloseContacts", v.nciCloseContacts);
    s.setValue(prefix + "nciElectrostatics", v.nciElectrostatics);
    s.setValue(prefix + "nciDispersion", v.nciDispersion);
    s.setValue(prefix + "nciHbDistance", v.nciHbDistance);
    s.setValue(prefix + "nciHbAngle", v.nciHbAngle);
    s.setValue(prefix + "nciLabels", v.nciLabels);
    s.setValue(prefix + "nciLiveMd", v.nciLiveMd);
    s.setValue(prefix + "fragmentTint", v.fragmentTint);
    s.setValue(prefix + "fragmentTintStrength", v.fragmentTintStrength);
    s.setValue(prefix + "fragmentScale", v.fragmentScale);
    s.setValue(prefix + "buildDockPreview", v.buildDockPreview);
    s.setValue(prefix + "hydrogenDisplay", v.hydrogenDisplay);
}

Settings::VisualizationSettings readVizSettings(const QSettings& s, const QString& prefix)
{
    Settings::VisualizationSettings v;  // default-initialised = canonical defaults
    v.renderingMode = s.value(prefix + "renderingMode", v.renderingMode).toInt();
    v.colorScheme = s.value(prefix + "colorScheme", v.colorScheme).toInt();
    v.atomTransparency = s.value(prefix + "atomTransparency", v.atomTransparency).toFloat();
    v.atomShininess = s.value(prefix + "atomShininess", v.atomShininess).toFloat();
    v.atomScaleFactor = s.value(prefix + "atomScaleFactor", v.atomScaleFactor).toFloat();
    v.bondThickness = s.value(prefix + "bondThickness", v.bondThickness).toFloat();
    v.fogEnabled = s.value(prefix + "fogEnabled", v.fogEnabled).toBool();
    v.fogIntensity = s.value(prefix + "fogIntensity", v.fogIntensity).toFloat();
    v.ssaoEnabled = s.value(prefix + "ssaoEnabled", v.ssaoEnabled).toBool();
    v.ssaoIntensity = s.value(prefix + "ssaoIntensity", v.ssaoIntensity).toFloat();
    v.ssaoRadius = s.value(prefix + "ssaoRadius", v.ssaoRadius).toFloat();
    v.ssaoBias = s.value(prefix + "ssaoBias", v.ssaoBias).toFloat();
    v.bloomEnabled = s.value(prefix + "bloomEnabled", v.bloomEnabled).toBool();
    v.bloomThreshold = s.value(prefix + "bloomThreshold", v.bloomThreshold).toFloat();
    v.bloomIntensity = s.value(prefix + "bloomIntensity", v.bloomIntensity).toFloat();
    v.hdrEnabled = s.value(prefix + "hdrEnabled", v.hdrEnabled).toBool();
    v.exposure = s.value(prefix + "exposure", v.exposure).toFloat();
    v.rotationMode = s.value(prefix + "rotationMode", v.rotationMode).toInt();
    v.instancingThreshold = s.value(prefix + "instancingThreshold", v.instancingThreshold).toInt();
    v.wallVisible = s.value(prefix + "wallVisible", v.wallVisible).toBool();
    v.wallOpacity = s.value(prefix + "wallOpacity", v.wallOpacity).toDouble();
    v.centerOnLoad = s.value(prefix + "centerOnLoad", v.centerOnLoad).toBool();
    v.nciSource = s.value(prefix + "nciSource", v.nciSource).toInt();
    v.nciHydrogenBonds = s.value(prefix + "nciHydrogenBonds", v.nciHydrogenBonds).toBool();
    v.nciHalogenBonds = s.value(prefix + "nciHalogenBonds", v.nciHalogenBonds).toBool();
    v.nciPiStacking = s.value(prefix + "nciPiStacking", v.nciPiStacking).toBool();
    v.nciCloseContacts = s.value(prefix + "nciCloseContacts", v.nciCloseContacts).toBool();
    v.nciElectrostatics = s.value(prefix + "nciElectrostatics", v.nciElectrostatics).toBool();
    v.nciDispersion = s.value(prefix + "nciDispersion", v.nciDispersion).toBool();
    v.nciHbDistance = s.value(prefix + "nciHbDistance", v.nciHbDistance).toFloat();
    v.nciHbAngle = s.value(prefix + "nciHbAngle", v.nciHbAngle).toFloat();
    v.nciLabels = s.value(prefix + "nciLabels", v.nciLabels).toBool();
    v.nciLiveMd = s.value(prefix + "nciLiveMd", v.nciLiveMd).toBool();
    v.fragmentTint = s.value(prefix + "fragmentTint", v.fragmentTint).toBool();
    v.fragmentTintStrength = s.value(prefix + "fragmentTintStrength", v.fragmentTintStrength).toFloat();
    v.fragmentScale = s.value(prefix + "fragmentScale", v.fragmentScale).toFloat();
    v.buildDockPreview = s.value(prefix + "buildDockPreview", v.buildDockPreview).toBool();
    v.hydrogenDisplay = s.value(prefix + "hydrogenDisplay", v.hydrogenDisplay).toInt();
    return v;
}
}  // namespace

Settings::VisualizationSettings Settings::getVisualizationSettings() const
{
    return readVizSettings(m_settings, VIZ_SETTINGS_PREFIX);
}

void Settings::setVisualizationSettings(const VisualizationSettings& settings)
{
    writeVizSettings(m_settings, VIZ_SETTINGS_PREFIX, settings);
    m_settings.sync();
}

// Claude Generated 2026 - Bead-type and interaction colours. Both maps are keyed
// by content (a VTF type label, an interaction class) rather than by a fixed
// field, so they get their own QSettings groups instead of a slot in
// VisualizationSettings. Bead type labels recur across files of the same system,
// which is why the colours are remembered globally rather than per file.
QHash<QString, QColor> Settings::beadTypeColors()
{
    QHash<QString, QColor> colors;
    m_settings.beginGroup(VIZ_SETTINGS_PREFIX + "beadColors");
    for (const QString& key : m_settings.childKeys()) {
        const QColor c = m_settings.value(key).value<QColor>();
        if (c.isValid())
            colors.insert(key, c);
    }
    m_settings.endGroup();
    return colors;
}

void Settings::setBeadTypeColors(const QHash<QString, QColor>& colors)
{
    m_settings.beginGroup(VIZ_SETTINGS_PREFIX + "beadColors");
    m_settings.remove(QString());   // drop entries the user reset
    for (auto it = colors.constBegin(); it != colors.constEnd(); ++it) {
        if (it.value().isValid())
            m_settings.setValue(it.key(), it.value());
    }
    m_settings.endGroup();
    m_settings.sync();
}

QHash<int, QColor> Settings::nciPalette()
{
    QHash<int, QColor> palette;
    m_settings.beginGroup(VIZ_SETTINGS_PREFIX + "nciColors");
    for (const QString& key : m_settings.childKeys()) {
        bool ok = false;
        const int paletteKey = key.toInt(&ok);
        const QColor c = m_settings.value(key).value<QColor>();
        if (ok && c.isValid())
            palette.insert(paletteKey, c);
    }
    m_settings.endGroup();
    return palette;
}

void Settings::setNciPalette(const QHash<int, QColor>& palette)
{
    m_settings.beginGroup(VIZ_SETTINGS_PREFIX + "nciColors");
    m_settings.remove(QString());
    for (auto it = palette.constBegin(); it != palette.constEnd(); ++it) {
        if (it.value().isValid())
            m_settings.setValue(QString::number(it.key()), it.value());
    }
    m_settings.endGroup();
    m_settings.sync();
}

// Claude Generated - Visualization Preset Management
QVector<Settings::VisualizationPreset> Settings::getVisualizationPresets()
{
    QVector<VisualizationPreset> presets;

    m_settings.beginGroup(VIZ_SETTINGS_PREFIX + "presets");
    const QStringList presetNames = m_settings.childGroups();
    m_settings.endGroup();

    for (const QString& presetName : presetNames) {
        VisualizationPreset preset;
        preset.name = presetName;
        preset.settings = readVizSettings(m_settings, VIZ_SETTINGS_PREFIX + "presets/" + presetName + "/");
        presets.append(preset);
    }
    return presets;
}

void Settings::savePreset(const QString& name, const VisualizationSettings& settings)
{
    writeVizSettings(m_settings, VIZ_SETTINGS_PREFIX + "presets/" + name + "/", settings);
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

namespace {
// Helpers to serialize Qt value types that QSettings cannot store directly.
QString vec3ToString(const QVector3D& v)
{
    return QStringLiteral("%1,%2,%3").arg(v.x()).arg(v.y()).arg(v.z());
}
QVector3D vec3FromString(const QString& s)
{
    const QStringList parts = s.split(QLatin1Char(','));
    if (parts.size() >= 3)
        return QVector3D(parts[0].toFloat(), parts[1].toFloat(), parts[2].toFloat());
    return QVector3D();
}
QString quatToString(const QQuaternion& q)
{
    return QStringLiteral("%1,%2,%3,%4").arg(q.scalar()).arg(q.x()).arg(q.y()).arg(q.z());
}
QQuaternion quatFromString(const QString& s)
{
    const QStringList parts = s.split(QLatin1Char(','));
    if (parts.size() >= 4)
        return QQuaternion(parts[0].toFloat(), parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat());
    return QQuaternion();
}
QString colorToString(const QColor& c)
{
    return QStringLiteral("%1,%2,%3,%4").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}
QColor colorFromString(const QString& s)
{
    const QStringList parts = s.split(QLatin1Char(','));
    if (parts.size() >= 3)
        return QColor(parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts.value(3, QStringLiteral("255")).toInt());
    return QColor(32, 36, 44);
}
}

// Claude Generated 2026 - Reproducible camera + display view presets
QVector<ViewPreset> Settings::viewPresets()
{
    QVector<ViewPreset> presets;
    m_settings.beginGroup(VIEW_PRESETS_PREFIX);
    const QStringList groups = m_settings.childGroups();
    for (const QString& name : groups) {
        m_settings.beginGroup(name);
        ViewPreset p;
        p.name = name;
        p.rootRotation = quatFromString(m_settings.value(QStringLiteral("rootRotation")).toString());
        p.cameraDistance = m_settings.value(QStringLiteral("cameraDistance"), 0.0f).toFloat();
        p.pan = vec3FromString(m_settings.value(QStringLiteral("pan")).toString());
        p.fieldOfView = m_settings.value(QStringLiteral("fieldOfView"), 45.0f).toFloat();
        p.sceneExtent = m_settings.value(QStringLiteral("sceneExtent"), 0.0f).toFloat();
        p.zoomFactor = m_settings.value(QStringLiteral("zoomFactor"), 3.0f).toFloat();
        p.zoomMode = static_cast<ZoomMode>(m_settings.value(QStringLiteral("zoomMode"), 0).toInt());

        p.renderingMode = m_settings.value(QStringLiteral("renderingMode"), 0).toInt();
        p.colorScheme = m_settings.value(QStringLiteral("colorScheme"), 0).toInt();
        p.atomTransparency = m_settings.value(QStringLiteral("atomTransparency"), 1.0f).toFloat();
        p.atomShininess = m_settings.value(QStringLiteral("atomShininess"), 80.0f).toFloat();
        p.atomScaleFactor = m_settings.value(QStringLiteral("atomScaleFactor"), 1.0f).toFloat();
        p.bondThickness = m_settings.value(QStringLiteral("bondThickness"), 0.15f).toFloat();
        p.fogEnabled = m_settings.value(QStringLiteral("fogEnabled"), false).toBool();
        p.fogIntensity = m_settings.value(QStringLiteral("fogIntensity"), 0.5f).toFloat();
        p.fogDistance = m_settings.value(QStringLiteral("fogDistance"), 0.2f).toFloat();
        p.ssaoEnabled = m_settings.value(QStringLiteral("ssaoEnabled"), true).toBool();
        p.ssaoIntensity = m_settings.value(QStringLiteral("ssaoIntensity"), 1.0f).toFloat();
        p.ssaoRadius = m_settings.value(QStringLiteral("ssaoRadius"), 0.05f).toFloat();
        p.ssaoBias = m_settings.value(QStringLiteral("ssaoBias"), 0.025f).toFloat();
        p.bloomEnabled = m_settings.value(QStringLiteral("bloomEnabled"), true).toBool();
        p.bloomThreshold = m_settings.value(QStringLiteral("bloomThreshold"), 0.8f).toFloat();
        p.bloomIntensity = m_settings.value(QStringLiteral("bloomIntensity"), 1.0f).toFloat();
        p.hdrEnabled = m_settings.value(QStringLiteral("hdrEnabled"), true).toBool();
        p.exposure = m_settings.value(QStringLiteral("exposure"), 1.0f).toFloat();
        p.rotationMode = m_settings.value(QStringLiteral("rotationMode"), 0).toInt();
        p.wallVisible = m_settings.value(QStringLiteral("wallVisible"), true).toBool();
        p.wallOpacity = m_settings.value(QStringLiteral("wallOpacity"), 0.6).toDouble();
        p.backgroundColor = colorFromString(m_settings.value(QStringLiteral("backgroundColor")).toString());
        for (int i = 0; i < 4; ++i)
            p.cornerLightEnabled[i] = m_settings.value(QStringLiteral("cornerLight%1").arg(i), i < 2).toBool();

        presets.append(p);
        m_settings.endGroup();
    }
    m_settings.endGroup();
    return presets;
}

void Settings::saveViewPreset(const ViewPreset& preset)
{
    if (preset.name.isEmpty())
        return;
    const QString path = VIEW_PRESETS_PREFIX + preset.name + QLatin1Char('/');
    m_settings.setValue(path + QStringLiteral("rootRotation"), quatToString(preset.rootRotation));
    m_settings.setValue(path + QStringLiteral("cameraDistance"), preset.cameraDistance);
    m_settings.setValue(path + QStringLiteral("pan"), vec3ToString(preset.pan));
    m_settings.setValue(path + QStringLiteral("fieldOfView"), preset.fieldOfView);
    m_settings.setValue(path + QStringLiteral("sceneExtent"), preset.sceneExtent);
    m_settings.setValue(path + QStringLiteral("zoomFactor"), preset.zoomFactor);
    m_settings.setValue(path + QStringLiteral("zoomMode"), static_cast<int>(preset.zoomMode));

    m_settings.setValue(path + QStringLiteral("renderingMode"), preset.renderingMode);
    m_settings.setValue(path + QStringLiteral("colorScheme"), preset.colorScheme);
    m_settings.setValue(path + QStringLiteral("atomTransparency"), preset.atomTransparency);
    m_settings.setValue(path + QStringLiteral("atomShininess"), preset.atomShininess);
    m_settings.setValue(path + QStringLiteral("atomScaleFactor"), preset.atomScaleFactor);
    m_settings.setValue(path + QStringLiteral("bondThickness"), preset.bondThickness);
    m_settings.setValue(path + QStringLiteral("fogEnabled"), preset.fogEnabled);
    m_settings.setValue(path + QStringLiteral("fogIntensity"), preset.fogIntensity);
    m_settings.setValue(path + QStringLiteral("fogDistance"), preset.fogDistance);
    m_settings.setValue(path + QStringLiteral("ssaoEnabled"), preset.ssaoEnabled);
    m_settings.setValue(path + QStringLiteral("ssaoIntensity"), preset.ssaoIntensity);
    m_settings.setValue(path + QStringLiteral("ssaoRadius"), preset.ssaoRadius);
    m_settings.setValue(path + QStringLiteral("ssaoBias"), preset.ssaoBias);
    m_settings.setValue(path + QStringLiteral("bloomEnabled"), preset.bloomEnabled);
    m_settings.setValue(path + QStringLiteral("bloomThreshold"), preset.bloomThreshold);
    m_settings.setValue(path + QStringLiteral("bloomIntensity"), preset.bloomIntensity);
    m_settings.setValue(path + QStringLiteral("hdrEnabled"), preset.hdrEnabled);
    m_settings.setValue(path + QStringLiteral("exposure"), preset.exposure);
    m_settings.setValue(path + QStringLiteral("rotationMode"), preset.rotationMode);
    m_settings.setValue(path + QStringLiteral("wallVisible"), preset.wallVisible);
    m_settings.setValue(path + QStringLiteral("wallOpacity"), preset.wallOpacity);
    m_settings.setValue(path + QStringLiteral("backgroundColor"), colorToString(preset.backgroundColor));
    for (int i = 0; i < 4; ++i)
        m_settings.setValue(path + QStringLiteral("cornerLight%1").arg(i), preset.cornerLightEnabled[i]);

    m_settings.sync();
}

void Settings::deleteViewPreset(const QString& name)
{
    m_settings.remove(VIEW_PRESETS_PREFIX + name);
    m_settings.sync();
}

bool Settings::viewPresetExists(const QString& name) const
{
    return m_settings.contains(VIEW_PRESETS_PREFIX + name + QStringLiteral("/cameraDistance"));
}

// Claude Generated 2026 - Operator metadata (name/ORCID/institution/license)
QString Settings::operatorName() const
{
    return m_settings.value(OPERATOR_NAME_KEY).toString();
}
void Settings::setOperatorName(const QString& name)
{
    m_settings.setValue(OPERATOR_NAME_KEY, name);
    m_settings.sync();
}
QString Settings::operatorOrcid() const
{
    return m_settings.value(OPERATOR_ORCID_KEY).toString();
}
void Settings::setOperatorOrcid(const QString& orcid)
{
    m_settings.setValue(OPERATOR_ORCID_KEY, orcid);
    m_settings.sync();
}
QString Settings::operatorInstitution() const
{
    return m_settings.value(OPERATOR_INSTITUTION_KEY).toString();
}
void Settings::setOperatorInstitution(const QString& institution)
{
    m_settings.setValue(OPERATOR_INSTITUTION_KEY, institution);
    m_settings.sync();
}
QString Settings::operatorLicense() const
{
    return m_settings.value(OPERATOR_LICENSE_KEY).toString();
}
void Settings::setOperatorLicense(const QString& license)
{
    m_settings.setValue(OPERATOR_LICENSE_KEY, license);
    m_settings.sync();
}

// Claude Generated Phase 2 - Enhanced recent files with timestamps
QVector<Settings::RecentFileEntry> Settings::recentFilesV2() const
{
    QVector<RecentFileEntry> entries;

    // Load the JSON array format; fall through to the legacy QStringList
    // migration below only when the V2 key is absent.
    const QJsonArray arr = readJsonArray(m_settings, RECENT_FILES_V2_KEY);
    if (!arr.isEmpty()) {
        for (const QJsonValue& v : arr) {
            RecentFileEntry entry = recentFileFromJson(v.toObject());
            if (entry.isValid())
                entries.append(entry);
        }
        return entries;
    }

    // Migration from old format (QStringList)
    QStringList oldFiles = m_settings.value(RECENT_FILES_LEGACY_KEY, QStringList()).toStringList();
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
    QJsonArray arr;
    for (const auto& entry : files)
        if (entry.isValid())
            arr.append(recentFileToJson(entry));
    writeJsonArray(m_settings, RECENT_FILES_V2_KEY, arr);
    m_settings.sync();
}

void Settings::clearRecentFilesV2()
{
    m_settings.remove(RECENT_FILES_V2_KEY);
    m_settings.sync();
}

// Claude Generated Phase 3 - Bookmark management
QVector<Settings::BookmarkItem> Settings::bookmarks() const
{
    QVector<BookmarkItem> items;
    const QJsonArray arr = readJsonArray(m_settings, BOOKMARKS_KEY);
    for (const QJsonValue& v : arr) {
        BookmarkItem item = bookmarkFromJson(v.toObject());
        if (item.isValid())
            items.append(item);
    }
    return items;
}

void Settings::setBookmarks(const QVector<BookmarkItem>& items)
{
    QJsonArray arr;
    for (const auto& item : items)
        if (item.isValid())
            arr.append(bookmarkToJson(item));
    writeJsonArray(m_settings, BOOKMARKS_KEY, arr);
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
    const QJsonArray arr = readJsonArray(m_settings, WORKSPACES_KEY);
    for (const QJsonValue& v : arr) {
        Workspace ws = workspaceFromJson(v.toObject());
        if (ws.isValid())
            workspaces.append(ws);
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
    QJsonArray arr;
    for (const auto& w : workspaces_list)
        if (w.isValid())
            arr.append(workspaceToJson(w));
    writeJsonArray(m_settings, WORKSPACES_KEY, arr);
    m_settings.sync();
}

void Settings::deleteWorkspace(const QString& id)
{
    QVector<Workspace> workspaces_list = workspaces();
    workspaces_list.erase(std::remove_if(workspaces_list.begin(), workspaces_list.end(),
        [&id](const Workspace& ws) { return ws.id == id; }), workspaces_list.end());

    // Serialize remaining (writeJsonArray removes the key when the list is empty)
    QJsonArray arr;
    for (const auto& w : workspaces_list)
        if (w.isValid())
            arr.append(workspaceToJson(w));
    writeJsonArray(m_settings, WORKSPACES_KEY, arr);
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
    return m_settings.value(LAST_ACTIVE_WORKSPACE_KEY, "").toString();
}

void Settings::setLastActiveWorkspaceId(const QString& id)
{
    m_settings.setValue(LAST_ACTIVE_WORKSPACE_KEY, id);
    m_settings.sync();
}

bool Settings::autoSaveWorkspaceEnabled() const
{
    return m_settings.value(AUTO_SAVE_WORKSPACE_KEY, false).toBool();
}

void Settings::setAutoSaveWorkspace(bool enabled)
{
    m_settings.setValue(AUTO_SAVE_WORKSPACE_KEY, enabled);
    m_settings.sync();
}

bool Settings::restoreLastWorkspaceEnabled() const
{
    return m_settings.value(RESTORE_LAST_WORKSPACE_KEY, false).toBool();
}

void Settings::setRestoreLastWorkspace(bool enabled)
{
    m_settings.setValue(RESTORE_LAST_WORKSPACE_KEY, enabled);
    m_settings.sync();
}

#ifdef USE_SFTP
// Claude Generated - Phase SFTP Integration: Connection profile management
QVector<Settings::SftpConnectionProfile> Settings::sftpProfiles() const
{
    QVector<SftpConnectionProfile> profiles;
    const QJsonArray arr = readJsonArray(m_settings, SFTP_PROFILES_KEY);
    for (const QJsonValue& v : arr) {
        SftpConnectionProfile profile = sftpProfileFromJson(v.toObject());
        if (profile.isValid())
            profiles.append(profile);
    }
    return profiles;
}

void Settings::setSftpProfiles(const QVector<SftpConnectionProfile>& profiles)
{
    QJsonArray arr;
    for (const auto& profile : profiles)
        if (profile.isValid())
            arr.append(sftpProfileToJson(profile));
    writeJsonArray(m_settings, SFTP_PROFILES_KEY, arr);
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
    const QJsonArray arr = readJsonArray(m_settings, REMOTE_MOUNTS_KEY);
    for (const QJsonValue& v : arr) {
        RemoteMountPoint mount = mountFromJson(v.toObject());
        if (mount.isValid())
            mounts.append(mount);
    }
    return mounts;
}

void Settings::setRemoteMounts(const QVector<RemoteMountPoint>& mounts)
{
    QJsonArray arr;
    for (const auto& mount : mounts)
        if (mount.isValid())
            arr.append(mountToJson(mount));
    writeJsonArray(m_settings, REMOTE_MOUNTS_KEY, arr);
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
#endif // USE_SFTP