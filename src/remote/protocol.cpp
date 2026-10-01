// protocol.cpp - Wire format between qurcuma and qurcuma-server
// Copyright (C) 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
// Claude Generated 2026 (WP remote compute R1)

#include "protocol.h"

#include "../lesson.h"  // simConfigToJson / simConfigFromJson

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>

#include <cstring>

static_assert(Q_BYTE_ORDER == Q_LITTLE_ENDIAN, "the binary wire format is little-endian");

namespace remote {

namespace {

// ---- minimal little-endian writer / reader over a QByteArray ----------------

struct Writer {
    QByteArray buf;
    template <typename T> void put(T v)
    {
        const int n = buf.size();
        buf.resize(n + int(sizeof(T)));
        std::memcpy(buf.data() + n, &v, sizeof(T));
    }
};

struct Reader {
    const QByteArray& buf;
    int pos = 0;
    bool ok = true;
    explicit Reader(const QByteArray& b) : buf(b) {}
    template <typename T> T get()
    {
        T v{};
        if (!ok || pos + int(sizeof(T)) > buf.size()) {
            ok = false;
            return v;
        }
        std::memcpy(&v, buf.constData() + pos, sizeof(T));
        pos += int(sizeof(T));
        return v;
    }
    bool enough(qint64 bytes) const { return ok && bytes >= 0 && pos + bytes <= buf.size(); }
};

bool fail(QString* error, const QString& msg)
{
    if (error)
        *error = msg;
    return false;
}

constexpr quint8 kFlagBonds = 1;
constexpr quint8 kFlagEvents = 2;
constexpr quint8 kFlagNci = 4;
constexpr qint32 kMaxAtoms = 50'000'000;  // sanity bound against corrupt counts

} // namespace

// --- JSON ----------------------------------------------------------------------

QJsonObject atomsToJson(const QVector<MolAtom>& atoms)
{
    // Column-wise: compact and fast to parse. Element/type strings repeat, which is fine.
    QJsonArray el, x, y, z, q, r, t;
    for (const MolAtom& a : atoms) {
        el.append(a.element);
        x.append(double(a.position.x()));
        y.append(double(a.position.y()));
        z.append(double(a.position.z()));
        q.append(double(a.charge));
        r.append(double(a.radius));
        t.append(a.type);
    }
    return QJsonObject{ { "element", el }, { "x", x }, { "y", y }, { "z", z },
        { "charge", q }, { "radius", r }, { "type", t } };
}

bool atomsFromJson(const QJsonValue& v, QVector<MolAtom>& atoms, QString* error)
{
    atoms.clear();
    const QJsonObject o = v.toObject();
    const QJsonArray el = o.value("element").toArray(), x = o.value("x").toArray(),
                     y = o.value("y").toArray(), z = o.value("z").toArray(),
                     q = o.value("charge").toArray(), r = o.value("radius").toArray(),
                     t = o.value("type").toArray();
    const int n = el.size();
    if (x.size() != n || y.size() != n || z.size() != n || q.size() != n || r.size() != n || t.size() != n)
        return fail(error, QStringLiteral("atom columns differ in length"));
    atoms.reserve(n);
    for (int i = 0; i < n; ++i) {
        MolAtom a;
        a.element = el[i].toString();
        a.position = QVector3D(float(x[i].toDouble()), float(y[i].toDouble()), float(z[i].toDouble()));
        a.charge = float(q[i].toDouble());
        a.radius = float(r[i].toDouble());
        a.type = t[i].toString();
        atoms.append(a);
    }
    return true;
}

QJsonObject bondsToJson(const QVector<MolBond>& bonds)
{
    QJsonArray a, b, o;
    for (const MolBond& bd : bonds) {
        a.append(bd.atom1);
        b.append(bd.atom2);
        o.append(bd.bondOrder);
    }
    return QJsonObject{ { "a", a }, { "b", b }, { "order", o } };
}

bool bondsFromJson(const QJsonValue& v, QVector<MolBond>& bonds, QString* error)
{
    bonds.clear();
    const QJsonObject o = v.toObject();
    const QJsonArray a = o.value("a").toArray(), b = o.value("b").toArray(), ord = o.value("order").toArray();
    if (a.size() != b.size() || a.size() != ord.size())
        return fail(error, QStringLiteral("bond columns differ in length"));
    bonds.reserve(a.size());
    for (int i = 0; i < a.size(); ++i)
        bonds.append(MolBond{ a[i].toInt(), b[i].toInt(), ord[i].toInt() });
    return true;
}

// --- run configuration ---------------------------------------------------------

QJsonObject configToJson(const SimulationConfig& cfg)
{
    QJsonObject o = simConfigToJson(cfg);
    o[QStringLiteral("optSingleShot")] = cfg.optSingleShot;
    return o;
}

SimulationConfig configFromJson(const QJsonObject& obj)
{
    SimulationConfig cfg = simConfigFromJson(obj);
    cfg.optSingleShot = obj.value(QStringLiteral("optSingleShot")).toBool(false);
    return cfg;
}

// --- frames --------------------------------------------------------------------

QByteArray encodeFrame(const SimulationFrame& f)
{
    Writer w;
    w.put<quint8>(quint8(MsgKind::Frame));
    quint8 flags = 0;
    if (!f.bonds.empty() || f.topologyVersion >= 0)
        flags |= kFlagBonds;
    if (!f.events.isEmpty())
        flags |= kFlagEvents;
    if (!f.nciContacts.isEmpty())
        flags |= kFlagNci;
    w.put<quint8>(flags);
    w.put<qint32>(f.step);
    w.put<double>(f.energy);
    w.put<double>(f.ekin);
    w.put<double>(f.temperature);
    w.put<double>(f.targetTemperature);
    w.put<qint32>(qint32(f.positions.size()));
    for (const QVector3D& p : f.positions) {
        w.put<float>(p.x());
        w.put<float>(p.y());
        w.put<float>(p.z());
    }
    if (flags & kFlagBonds) {
        w.put<qint32>(f.topologyVersion);
        w.put<qint32>(qint32(f.bonds.size()));
        for (const FrameBond& b : f.bonds) {
            w.put<qint32>(b.a);
            w.put<qint32>(b.b);
            w.put<qint32>(b.order);
        }
    }
    if (flags & kFlagEvents) {
        w.put<qint32>(qint32(f.events.size()));
        for (const ReactEventView& e : f.events) {
            w.put<qint32>(e.step);
            w.put<double>(e.deJumpKJmol);
            w.put<qint32>(qint32(e.formed.size()));
            for (const auto& p : e.formed) { w.put<qint32>(p.first); w.put<qint32>(p.second); }
            w.put<qint32>(qint32(e.broken.size()));
            for (const auto& p : e.broken) { w.put<qint32>(p.first); w.put<qint32>(p.second); }
        }
    }
    if (flags & kFlagNci) {
        w.put<qint32>(qint32(f.nciContacts.size()));
        for (const nci::Contact& c : f.nciContacts) {
            w.put<qint32>(int(c.kind));
            w.put<qint32>(c.donor);
            w.put<qint32>(c.bridge);
            w.put<qint32>(c.acceptor);
            w.put<qint32>(qint32(c.ringA.size()));
            for (int i : c.ringA) w.put<qint32>(i);
            w.put<qint32>(qint32(c.ringB.size()));
            for (int i : c.ringB) w.put<qint32>(i);
            w.put<float>(c.distance);
            w.put<float>(c.angle);
            w.put<float>(c.offset);
            w.put<float>(c.score);
            w.put<double>(c.energy);
            w.put<quint8>(c.hasEnergy ? 1 : 0);
            w.put<qint32>(c.motif);
        }
    }
    return w.buf;
}

bool decodeFrame(const QByteArray& m, SimulationFrame& f, QString* error)
{
    f = SimulationFrame();
    Reader r(m);
    if (r.get<quint8>() != quint8(MsgKind::Frame))
        return fail(error, QStringLiteral("not a frame message"));
    const quint8 flags = r.get<quint8>();
    f.step = r.get<qint32>();
    f.energy = r.get<double>();
    f.ekin = r.get<double>();
    f.temperature = r.get<double>();
    f.targetTemperature = r.get<double>();
    const qint32 n = r.get<qint32>();
    if (!r.ok || n < 0 || n > kMaxAtoms || !r.enough(qint64(n) * 12))
        return fail(error, QStringLiteral("frame truncated or atom count invalid"));
    f.positions.resize(size_t(n));
    for (QVector3D& p : f.positions) {
        const float x = r.get<float>(), y = r.get<float>(), z = r.get<float>();
        p = QVector3D(x, y, z);
    }
    // A count read from the wire must fit into what is left of the message.
    auto count = [&](qint64 bytesEach) -> qint32 {
        const qint32 c = r.get<qint32>();
        if (!r.ok || c < 0 || !r.enough(qint64(c) * bytesEach)) {
            r.ok = false;
            return 0;
        }
        return c;
    };
    if (flags & kFlagBonds) {
        f.topologyVersion = r.get<qint32>();
        const qint32 nb = count(12);
        f.bonds.resize(size_t(nb));
        for (FrameBond& b : f.bonds) {
            b.a = r.get<qint32>();
            b.b = r.get<qint32>();
            b.order = r.get<qint32>();
        }
    }
    if (flags & kFlagEvents) {
        const qint32 ne = count(20);
        for (qint32 i = 0; i < ne && r.ok; ++i) {
            ReactEventView e;
            e.step = r.get<qint32>();
            e.deJumpKJmol = r.get<double>();
            const qint32 nf = count(8);
            for (qint32 k = 0; k < nf; ++k) { const int a = r.get<qint32>(), b = r.get<qint32>(); e.formed.append({ a, b }); }
            const qint32 nbk = count(8);
            for (qint32 k = 0; k < nbk; ++k) { const int a = r.get<qint32>(), b = r.get<qint32>(); e.broken.append({ a, b }); }
            f.events.append(e);
        }
    }
    if (flags & kFlagNci) {
        const qint32 nc = count(57);
        for (qint32 i = 0; i < nc && r.ok; ++i) {
            nci::Contact c;
            c.kind = nci::Kind(r.get<qint32>());
            c.donor = r.get<qint32>();
            c.bridge = r.get<qint32>();
            c.acceptor = r.get<qint32>();
            const qint32 na = count(4);
            for (qint32 k = 0; k < na; ++k) c.ringA.append(r.get<qint32>());
            const qint32 nb = count(4);
            for (qint32 k = 0; k < nb; ++k) c.ringB.append(r.get<qint32>());
            c.distance = r.get<float>();
            c.angle = r.get<float>();
            c.offset = r.get<float>();
            c.score = r.get<float>();
            c.energy = r.get<double>();
            c.hasEnergy = r.get<quint8>() != 0;
            c.motif = r.get<qint32>();
            f.nciContacts.append(c);
        }
    }
    if (!r.ok || r.pos != m.size())
        return fail(error, QStringLiteral("frame truncated or has trailing bytes"));
    return true;
}

// --- files ---------------------------------------------------------------------

QByteArray encodeFile(const QString& name, const QByteArray& data)
{
    const QByteArray nm = name.toUtf8();
    Writer w;
    w.put<quint8>(quint8(MsgKind::File));
    w.put<quint32>(quint32(nm.size()));
    w.buf.append(nm);
    w.buf.append(QCryptographicHash::hash(data, QCryptographicHash::Sha256));
    w.buf.append(data);
    return w.buf;
}

bool decodeFile(const QByteArray& m, QString& name, QByteArray& data, QString* error)
{
    Reader r(m);
    if (r.get<quint8>() != quint8(MsgKind::File))
        return fail(error, QStringLiteral("not a file message"));
    const quint32 nl = r.get<quint32>();
    if (!r.ok || nl == 0 || nl > 256 || !r.enough(qint64(nl) + 32))
        return fail(error, QStringLiteral("file message truncated or name length invalid"));
    name = QString::fromUtf8(m.constData() + r.pos, int(nl));
    r.pos += int(nl);
    const QByteArray sha = m.mid(r.pos, 32);
    r.pos += 32;
    data = m.mid(r.pos);
    if (!isSafeFileName(name))
        return fail(error, QStringLiteral("unsafe file name"));
    if (data.size() > kMaxFileBytes)
        return fail(error, QStringLiteral("file larger than the per-file limit"));
    if (QCryptographicHash::hash(data, QCryptographicHash::Sha256) != sha)
        return fail(error, QStringLiteral("file checksum mismatch"));
    return true;
}

// --- file policy ----------------------------------------------------------------

bool isSafeFileName(const QString& name)
{
    static const QRegularExpression re(QStringLiteral("\\A[A-Za-z0-9_+-][A-Za-z0-9._+-]{0,127}\\z"));
    return re.match(name).hasMatch() && !name.contains(QStringLiteral(".."));
}

QStringList uploadParamKeys()
{
    return { QStringLiteral("rmsd_mtd_ref_file"), QStringLiteral("restart_file"), QStringLiteral("plumed_file") };
}

QStringList blockedParamKeys()
{
    return { QStringLiteral("load_ff_json"), QStringLiteral("orca_executable"), QStringLiteral("orca_basename"),
        QStringLiteral("orca_keywords"), QStringLiteral("orca_extra_keywords"), QStringLiteral("molalign_bin") };
}

namespace {

bool looksLikePath(const QString& s)
{
    static const QRegularExpression ext(QStringLiteral("\\.(xyz|json|dat|trj|sdf|mol2|pdb|vtf)$"), QRegularExpression::CaseInsensitiveOption);
    return s.contains(QLatin1Char('/')) || s.contains(QLatin1Char('\\')) || s.startsWith(QLatin1Char('~'))
        || ext.match(s).hasMatch();
}

// One file-valued entry: "none"/empty = unset; otherwise it must be an uploaded name.
bool resolveUpload(QJsonValue& v, const QString& key, const QSet<QString>& uploaded, const QString& inDir, QString* error)
{
    const QString s = v.toString();
    if (s.isEmpty() || s == QLatin1String("none"))
        return true;
    if (!isSafeFileName(s))
        return fail(error, QStringLiteral("%1: not a plain file name").arg(key));
    if (!uploaded.contains(s))
        return fail(error, QStringLiteral("%1: file '%2' was not uploaded").arg(key, s));
    v = inDir + QLatin1Char('/') + s;
    return true;
}

} // namespace

bool applyFilePolicy(QJsonObject& cfg, const QSet<QString>& uploaded, const QString& inDir, QString* error)
{
    // SimulationConfig field that mirrors rmsd_mtd_ref_file.
    if (cfg.contains(QStringLiteral("rmsdMtdRefFile"))) {
        QJsonValue v = cfg.value(QStringLiteral("rmsdMtdRefFile"));
        if (!resolveUpload(v, QStringLiteral("rmsd_mtd_ref_file"), uploaded, inDir, error))
            return false;
        cfg[QStringLiteral("rmsdMtdRefFile")] = v;
    }
    QJsonObject extra = cfg.value(QStringLiteral("mdExtraParams")).toObject();
    const QStringList upload = uploadParamKeys(), blocked = blockedParamKeys();
    for (auto it = extra.begin(); it != extra.end(); ++it) {
        const QString key = it.key();
        if (blocked.contains(key))
            return fail(error, QStringLiteral("%1: parameter not accepted by the server").arg(key));
        QJsonValue v = it.value();
        if (upload.contains(key)) {
            if (!resolveUpload(v, key, uploaded, inDir, error))
                return false;
            it.value() = v;
        } else if (v.isString() && looksLikePath(v.toString())) {
            return fail(error, QStringLiteral("%1: path-like value '%2' is not a known upload parameter").arg(key, v.toString()));
        }
    }
    cfg[QStringLiteral("mdExtraParams")] = extra;
    return true;
}

} // namespace remote
