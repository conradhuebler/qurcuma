// remotelog.cpp - Diagnostic log of the remote compute code
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute)

#include "remotelog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>

namespace remote {

QString logFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + QStringLiteral("/remote.log");
}

QString appendLog(const QString& source, const QString& message)
{
    static QMutex mutex;
    QMutexLocker lock(&mutex);
    const QString path = logFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (QFileInfo(path).size() > 1024 * 1024) {
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
    }
    const QString line = QStringLiteral("[%1] %2").arg(source, message);
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << ' ' << line << '\n';
    }
    return line;
}

} // namespace remote
