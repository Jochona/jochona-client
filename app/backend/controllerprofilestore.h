//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
// Jochona: per-controller calibration + button-remap persistence (proposal
// §6.7). ControllerManager owns live SDL state; this store owns durable
// config keyed by controller identity (ControllerManager::controllerPath(),
// generally an SDL_GameControllerPath()-derived string) and, optionally, a
// per-game app id for a per-title override layered on top of the
// controller-wide profile.
//
// --- SQLite seam -------------------------------------------------------
// app/backend/database.{h,cpp} (Database class, migration runner) is being
// built concurrently and isn't finalized. Rather than block on it or touch
// its files, ControllerProfileStore talks only to the abstract
// IControllerProfileBackend interface below; JsonControllerProfileBackend
// is the M2 fallback (one JSON file under QStandardPaths::AppConfigLocation)
// so this ships end-to-end today.
//
// Once Database lands, add a DatabaseControllerProfileBackend implementing
// IControllerProfileBackend, backed by:
//
//   CREATE TABLE controller_profiles(
//       controller_path TEXT PRIMARY KEY,
//       config           JSON NOT NULL
//   );
//   CREATE TABLE game_profiles(
//       app_id           TEXT NOT NULL,
//       controller_path  TEXT NOT NULL,
//       config           JSON NOT NULL,
//       PRIMARY KEY (app_id, controller_path)
//   );
//
// `config` is the same JSON shape ControllerProfile::toVariantMap() already
// produces (calibration + buttonRemap), so the swap is confined to
// ControllerProfileStore::get() constructing the new backend - no QML- or
// caller-facing API changes required.
// -------------------------------------------------------------------------
#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QStringList>
#include <QVariantMap>

// One stick/trigger's calibration. The response curve is an exponent
// applied to the normalized, deadzone-excluded magnitude
// (output = input^curve); 1.0 is linear (no reshaping).
struct ControllerCalibration
{
    double deadzoneLeftStick = 0.10;
    double deadzoneRightStick = 0.10;
    double deadzoneLeftTrigger = 0.02;
    double deadzoneRightTrigger = 0.02;
    double curveLeftStick = 1.0;
    double curveRightStick = 1.0;
    double curveLeftTrigger = 1.0;
    double curveRightTrigger = 1.0;

    QVariantMap
    toVariantMap() const;

    static ControllerCalibration
    fromVariantMap(const QVariantMap& map);
};

struct ControllerProfile
{
    QString controllerPath;
    QString appId; // empty => controller-wide profile, not a per-game override
    ControllerCalibration calibration;
    QMap<QString, QString> buttonRemap; // logical name -> logical name, e.g. "a" -> "b"

    QVariantMap
    toVariantMap() const;

    static ControllerProfile
    fromVariantMap(const QString& controllerPath, const QString& appId, const QVariantMap& map);
};

// Persistence seam: ControllerProfileStore never touches storage directly,
// only through this interface. See the SQLite seam note above.
class IControllerProfileBackend
{
public:
    virtual ~IControllerProfileBackend() = default;

    // Returns an empty map if no profile is stored for this key.
    virtual QVariantMap
    load(const QString& controllerPath, const QString& appId) const = 0;

    virtual void
    save(const QString& controllerPath, const QString& appId, const QVariantMap& config) = 0;

    virtual void
    remove(const QString& controllerPath, const QString& appId) = 0;

    virtual QStringList
    knownControllerPaths() const = 0;
};

// M2 fallback backend: a single JSON file under
// QStandardPaths::AppConfigLocation. The whole file is read/rewritten on
// each save; the profile count is small (at most a handful of controllers
// and games), so this is not a performance concern.
class JsonControllerProfileBackend : public IControllerProfileBackend
{
public:
    explicit JsonControllerProfileBackend(const QString& filePath = QString());

    QVariantMap
    load(const QString& controllerPath, const QString& appId) const override;

    void
    save(const QString& controllerPath, const QString& appId, const QVariantMap& config) override;

    void
    remove(const QString& controllerPath, const QString& appId) override;

    QStringList
    knownControllerPaths() const override;

    static QString
    defaultFilePath();

private:
    static QString
    key(const QString& controllerPath, const QString& appId);

    QVariantMap
    readAll() const;

    void
    writeAll(const QVariantMap& root) const;

    QString m_FilePath;
};

class ControllerProfileStore : public QObject
{
    Q_OBJECT

public:
    // Production entry point. Owns a JsonControllerProfileBackend until the
    // SQLite-backed one is wired in (see the seam note above).
    Q_INVOKABLE static ControllerProfileStore*
    get();

    // Seam constructor: takes ownership of `backend`. Used by get() today
    // and by the future Database-backed swap-in / unit tests.
    explicit ControllerProfileStore(IControllerProfileBackend* backend, QObject* parent = nullptr);

    ~ControllerProfileStore() override;

    // Returns calibration+remap for controllerPath, layering a per-game
    // override (if any and if appId is non-empty) on top of the
    // controller-wide profile. Unset fields fall back to
    // ControllerCalibration's defaults.
    Q_INVOKABLE QVariantMap
    profileFor(const QString& controllerPath, const QString& appId = QString()) const;

    Q_INVOKABLE void
    saveProfile(const QString& controllerPath, const QVariantMap& config, const QString& appId = QString());

    // Convenience for the M2 calibration panel: read-modify-write a single
    // deadzone field. `stick` is one of "leftStick" | "rightStick" |
    // "leftTrigger" | "rightTrigger".
    Q_INVOKABLE void
    setDeadzone(const QString& controllerPath, const QString& stick, double value, const QString& appId = QString());

    Q_INVOKABLE void
    resetProfile(const QString& controllerPath, const QString& appId = QString());

    Q_INVOKABLE QStringList
    knownControllerPaths() const;

signals:
    void
    profileChanged(const QString& controllerPath, const QString& appId);

private:
    Q_DISABLE_COPY(ControllerProfileStore)

    static ControllerProfileStore* s_Instance;

    IControllerProfileBackend* m_Backend;
};
