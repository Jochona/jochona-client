//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
#include "controllerprofilestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>
#include <QDebug>

// --- ControllerCalibration --------------------------------------------------

QVariantMap
ControllerCalibration::toVariantMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("deadzoneLeftStick"), deadzoneLeftStick);
    map.insert(QStringLiteral("deadzoneRightStick"), deadzoneRightStick);
    map.insert(QStringLiteral("deadzoneLeftTrigger"), deadzoneLeftTrigger);
    map.insert(QStringLiteral("deadzoneRightTrigger"), deadzoneRightTrigger);
    map.insert(QStringLiteral("curveLeftStick"), curveLeftStick);
    map.insert(QStringLiteral("curveRightStick"), curveRightStick);
    map.insert(QStringLiteral("curveLeftTrigger"), curveLeftTrigger);
    map.insert(QStringLiteral("curveRightTrigger"), curveRightTrigger);
    return map;
}

ControllerCalibration
ControllerCalibration::fromVariantMap(const QVariantMap& map)
{
    ControllerCalibration cal;
    cal.deadzoneLeftStick = map.value(QStringLiteral("deadzoneLeftStick"), cal.deadzoneLeftStick).toDouble();
    cal.deadzoneRightStick = map.value(QStringLiteral("deadzoneRightStick"), cal.deadzoneRightStick).toDouble();
    cal.deadzoneLeftTrigger = map.value(QStringLiteral("deadzoneLeftTrigger"), cal.deadzoneLeftTrigger).toDouble();
    cal.deadzoneRightTrigger = map.value(QStringLiteral("deadzoneRightTrigger"), cal.deadzoneRightTrigger).toDouble();
    cal.curveLeftStick = map.value(QStringLiteral("curveLeftStick"), cal.curveLeftStick).toDouble();
    cal.curveRightStick = map.value(QStringLiteral("curveRightStick"), cal.curveRightStick).toDouble();
    cal.curveLeftTrigger = map.value(QStringLiteral("curveLeftTrigger"), cal.curveLeftTrigger).toDouble();
    cal.curveRightTrigger = map.value(QStringLiteral("curveRightTrigger"), cal.curveRightTrigger).toDouble();
    return cal;
}

// --- ControllerProfile -------------------------------------------------------

QVariantMap
ControllerProfile::toVariantMap() const
{
    QVariantMap map;
    map.insert(QStringLiteral("controllerPath"), controllerPath);
    map.insert(QStringLiteral("appId"), appId);
    map.insert(QStringLiteral("calibration"), calibration.toVariantMap());

    QVariantMap remap;
    for (auto it = buttonRemap.constBegin(); it != buttonRemap.constEnd(); ++it) {
        remap.insert(it.key(), it.value());
    }
    map.insert(QStringLiteral("buttonRemap"), remap);

    return map;
}

ControllerProfile
ControllerProfile::fromVariantMap(const QString& controllerPath, const QString& appId, const QVariantMap& map)
{
    ControllerProfile profile;
    profile.controllerPath = controllerPath;
    profile.appId = appId;
    profile.calibration = ControllerCalibration::fromVariantMap(map.value(QStringLiteral("calibration")).toMap());

    QVariantMap remap = map.value(QStringLiteral("buttonRemap")).toMap();
    for (auto it = remap.constBegin(); it != remap.constEnd(); ++it) {
        profile.buttonRemap.insert(it.key(), it.value().toString());
    }

    return profile;
}

// --- JsonControllerProfileBackend --------------------------------------------

namespace {
    const QString kControllerPrefix = QStringLiteral("controller:");
    const QString kGamePrefix = QStringLiteral("game:");
}

JsonControllerProfileBackend::JsonControllerProfileBackend(const QString& filePath)
    : m_FilePath(filePath.isEmpty() ? defaultFilePath() : filePath)
{
}

QString
JsonControllerProfileBackend::defaultFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + QStringLiteral("/controller-profiles.json");
}

QString
JsonControllerProfileBackend::key(const QString& controllerPath, const QString& appId)
{
    if (appId.isEmpty()) {
        return kControllerPrefix + controllerPath;
    }

    return kGamePrefix + appId + QStringLiteral("|") + controllerPath;
}

QVariantMap
JsonControllerProfileBackend::readAll() const
{
    QFile file(m_FilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QVariantMap();
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (doc.isNull() || !doc.isObject()) {
        if (error.error != QJsonParseError::NoError) {
            qWarning() << "ControllerProfileStore: malformed profile store" << m_FilePath
                       << "-" << error.errorString() << "; treating as empty";
        }
        return QVariantMap();
    }

    return doc.object().toVariantMap();
}

void
JsonControllerProfileBackend::writeAll(const QVariantMap& root) const
{
    QFileInfo info(m_FilePath);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qWarning() << "ControllerProfileStore: failed to create" << dir.absolutePath();
        return;
    }

    QFile file(m_FilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "ControllerProfileStore: failed to open" << m_FilePath << "for writing";
        return;
    }

    file.write(QJsonDocument(QJsonObject::fromVariantMap(root)).toJson(QJsonDocument::Indented));
    file.close();
}

QVariantMap
JsonControllerProfileBackend::load(const QString& controllerPath, const QString& appId) const
{
    QVariantMap profiles = readAll().value(QStringLiteral("profiles")).toMap();
    return profiles.value(key(controllerPath, appId)).toMap();
}

void
JsonControllerProfileBackend::save(const QString& controllerPath, const QString& appId, const QVariantMap& config)
{
    QVariantMap root = readAll();
    QVariantMap profiles = root.value(QStringLiteral("profiles")).toMap();
    profiles.insert(key(controllerPath, appId), config);
    root.insert(QStringLiteral("profiles"), profiles);
    root.insert(QStringLiteral("version"), 1);
    writeAll(root);
}

void
JsonControllerProfileBackend::remove(const QString& controllerPath, const QString& appId)
{
    QVariantMap root = readAll();
    QVariantMap profiles = root.value(QStringLiteral("profiles")).toMap();
    profiles.remove(key(controllerPath, appId));
    root.insert(QStringLiteral("profiles"), profiles);
    writeAll(root);
}

QStringList
JsonControllerProfileBackend::knownControllerPaths() const
{
    QVariantMap profiles = readAll().value(QStringLiteral("profiles")).toMap();

    QStringList paths;
    for (auto it = profiles.constBegin(); it != profiles.constEnd(); ++it) {
        if (it.key().startsWith(kControllerPrefix)) {
            paths.append(it.key().mid(kControllerPrefix.length()));
        }
    }

    return paths;
}

// --- ControllerProfileStore ---------------------------------------------------

ControllerProfileStore* ControllerProfileStore::s_Instance = nullptr;

ControllerProfileStore*
ControllerProfileStore::get()
{
    if (s_Instance == nullptr) {
        s_Instance = new ControllerProfileStore(new JsonControllerProfileBackend());
    }

    return s_Instance;
}

ControllerProfileStore::ControllerProfileStore(IControllerProfileBackend* backend, QObject* parent)
    : QObject(parent)
    , m_Backend(backend)
{
    Q_ASSERT(m_Backend != nullptr);
}

ControllerProfileStore::~ControllerProfileStore()
{
    delete m_Backend;
}

QVariantMap
ControllerProfileStore::profileFor(const QString& controllerPath, const QString& appId) const
{
    ControllerProfile base = ControllerProfile::fromVariantMap(controllerPath, QString(),
                                                                m_Backend->load(controllerPath, QString()));

    if (appId.isEmpty()) {
        return base.toVariantMap();
    }

    QVariantMap overrideMap = m_Backend->load(controllerPath, appId);
    if (overrideMap.isEmpty()) {
        // No per-game override yet: report the controller-wide profile
        // under this appId so callers can save a delta on top of it later.
        ControllerProfile forGame = base;
        forGame.appId = appId;
        return forGame.toVariantMap();
    }

    // Layer the per-game override on top of the controller-wide profile:
    // any field the override doesn't specify keeps the controller-wide
    // value rather than falling back to ControllerCalibration's defaults.
    QVariantMap mergedCalibration = base.calibration.toVariantMap();
    QVariantMap overrideCalibration = overrideMap.value(QStringLiteral("calibration")).toMap();
    for (auto it = overrideCalibration.constBegin(); it != overrideCalibration.constEnd(); ++it) {
        mergedCalibration.insert(it.key(), it.value());
    }

    QMap<QString, QString> mergedRemap = base.buttonRemap;
    QVariantMap overrideRemap = overrideMap.value(QStringLiteral("buttonRemap")).toMap();
    for (auto it = overrideRemap.constBegin(); it != overrideRemap.constEnd(); ++it) {
        mergedRemap.insert(it.key(), it.value().toString());
    }

    ControllerProfile merged;
    merged.controllerPath = controllerPath;
    merged.appId = appId;
    merged.calibration = ControllerCalibration::fromVariantMap(mergedCalibration);
    merged.buttonRemap = mergedRemap;
    return merged.toVariantMap();
}

void
ControllerProfileStore::saveProfile(const QString& controllerPath, const QVariantMap& config, const QString& appId)
{
    ControllerProfile profile = ControllerProfile::fromVariantMap(controllerPath, appId, config);
    m_Backend->save(controllerPath, appId, profile.toVariantMap());
    emit profileChanged(controllerPath, appId);
}

void
ControllerProfileStore::setDeadzone(const QString& controllerPath, const QString& stick, double value, const QString& appId)
{
    QString field;
    if (stick == QStringLiteral("leftStick")) {
        field = QStringLiteral("deadzoneLeftStick");
    } else if (stick == QStringLiteral("rightStick")) {
        field = QStringLiteral("deadzoneRightStick");
    } else if (stick == QStringLiteral("leftTrigger")) {
        field = QStringLiteral("deadzoneLeftTrigger");
    } else if (stick == QStringLiteral("rightTrigger")) {
        field = QStringLiteral("deadzoneRightTrigger");
    } else {
        qWarning() << "ControllerProfileStore::setDeadzone: unknown stick" << stick;
        return;
    }

    QVariantMap current = profileFor(controllerPath, appId);
    QVariantMap calibration = current.value(QStringLiteral("calibration")).toMap();
    calibration.insert(field, qBound(0.0, value, 0.9));
    current.insert(QStringLiteral("calibration"), calibration);

    saveProfile(controllerPath, current, appId);
}

void
ControllerProfileStore::resetProfile(const QString& controllerPath, const QString& appId)
{
    m_Backend->remove(controllerPath, appId);
    emit profileChanged(controllerPath, appId);
}

QStringList
ControllerProfileStore::knownControllerPaths() const
{
    return m_Backend->knownControllerPaths();
}
