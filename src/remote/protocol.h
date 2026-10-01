// protocol.h - Wire format between a qurcuma client (A) and qurcuma-server (B)
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// Claude Generated 2026 (WP remote compute R1, docs/WP-remote-compute-vr.md).
// Qt Core/Gui value types only, no widgets, so the server builds without a display.
//
// Transport: one WebSocket connection. Control messages are JSON text frames
// ({"type": ...}); binary frames start with a one-byte kind (MsgKind). Binary payloads
// are little-endian (the encoder refuses to compile on a big-endian host).

#pragma once

#include "../moleculetypes.h"
#include "../simulationframe.h"
#include "../simulationworker.h"  // SimulationConfig

#include <QByteArray>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace remote {

constexpr int kProtocolVersion = 1;
constexpr qint64 kMaxFileBytes = 64LL * 1024 * 1024;      ///< per uploaded file
constexpr qint64 kMaxSessionBytes = 256LL * 1024 * 1024;  ///< per session

/// First byte of every binary WebSocket message.
enum class MsgKind : quint8 {
    Frame = 1,  ///< server -> client: one SimulationFrame
    File = 2,   ///< client -> server: one uploaded file
    FileChunk = 3  ///< server -> client: part of a downloaded file (after a fileBegin message)
};

// --- control messages (JSON) -------------------------------------------------

QJsonObject atomsToJson(const QVector<MolAtom>& atoms);
bool atomsFromJson(const QJsonValue& v, QVector<MolAtom>& atoms, QString* error = nullptr);
QJsonObject bondsToJson(const QVector<MolBond>& bonds);
bool bondsFromJson(const QJsonValue& v, QVector<MolBond>& bonds, QString* error = nullptr);

// --- run configuration ---------------------------------------------------------

/// simConfigToJson (lesson.h) plus the one transient field lessons do not store,
/// optSingleShot (without it a remote clean-up would run the endless keep-alive loop).
QJsonObject configToJson(const SimulationConfig& cfg);
SimulationConfig configFromJson(const QJsonObject& obj);

// --- frames (binary) ---------------------------------------------------------

/// Positions are sent as float32 (about 1e-7 relative, below the display resolution),
/// energies and temperatures as double. Bonds, events and NCI contacts follow only when
/// the frame carries them.
QByteArray encodeFrame(const SimulationFrame& frame);
bool decodeFrame(const QByteArray& message, SimulationFrame& frame, QString* error = nullptr);

// --- uploaded files (binary) -------------------------------------------------

QByteArray encodeFile(const QString& name, const QByteArray& data);
/// Checks the name (isSafeFileName) and the SHA-256 carried in the message.
bool decodeFile(const QByteArray& message, QString& name, QByteArray& data, QString* error = nullptr);

// --- file download (server -> client, chunked) -----------------------------------

constexpr int kChunkBytes = 256 * 1024;

QByteArray encodeChunk(quint32 transferId, const QByteArray& data);
bool decodeChunk(const QByteArray& message, quint32& transferId, QByteArray& data);

/// Resolves a path a client asked to list or fetch. @p requested is absolute or relative to
/// the first root; the result is the canonical existing path and lies inside one of
/// @p roots (symlinks that leave a root are refused). Empty on failure, @p error says why.
QString resolveBrowsePath(const QString& requested, const QStringList& roots, QString* error = nullptr);

// --- file policy -------------------------------------------------------------

/// A plain file name: no directory part, no "..", not empty, not hidden, at most 128
/// characters of [A-Za-z0-9._+-].
bool isSafeFileName(const QString& name);

/// curcuma `simplemd` parameters whose value is a file the run reads (uploaded by the client).
QStringList uploadParamKeys();
/// Parameters the server never takes from a client (programs, arbitrary force-field files).
QStringList blockedParamKeys();

/// Applies the file policy to a start configuration (SimulationConfig as JSON, see
/// simConfigToJson): every file parameter must name an uploaded file (@p uploaded) and is
/// rewritten to its path under @p inDir; blocked keys and other path-like strings are
/// rejected. Returns false and fills @p error on the first violation.
bool applyFilePolicy(QJsonObject& configJson, const QSet<QString>& uploaded, const QString& inDir,
    QString* error = nullptr);

} // namespace remote
