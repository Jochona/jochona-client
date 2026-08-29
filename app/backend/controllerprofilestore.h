//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
// Jochona Controller Maps. SQLite is authoritative; the JSON backend only
// reads the pre-M2 file during the transactional one-time import.
#pragma once
#include "SDL_compat.h"

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

struct ControllerMap
{
    QString controllerPath;
    QString appId; // empty => controller-wide profile, not a per-game override
    ControllerCalibration calibration;
    QMap<QString, QString> buttonRemap; // logical name -> logical name, e.g. "a" -> "b"

    QVariantMap
    toVariantMap() const;

    static ControllerMap
    fromVariantMap(const QString& controllerPath, const QString& appId, const QVariantMap& map);
};

// Persistence seam: ControllerMapStore never touches storage directly,
// only through this interface. See the SQLite seam note above.
class IControllerMapBackend
{
public:
    virtual ~IControllerMapBackend() = default;

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
class JsonControllerMapBackend : public IControllerMapBackend
{
public:
    explicit JsonControllerMapBackend(const QString& filePath = QString());

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
    QVariantMap allProfiles() const;

private:
    static QString
    key(const QString& controllerPath, const QString& appId);

    QVariantMap
    readAll() const;

    void
    writeAll(const QVariantMap& root) const;

    QString m_FilePath;
};

class SqliteControllerMapBackend : public IControllerMapBackend
{
public:
    QVariantMap load(const QString& controllerId,
                     const QString& contextKey) const override;
    void save(const QString& controllerId,
              const QString& contextKey,
              const QVariantMap& config) override;
    void remove(const QString& controllerId,
                const QString& contextKey) override;
    QStringList knownControllerPaths() const override;

private:
    static QString scopeForContext(const QString& contextKey);
    static QString keyWithoutScope(const QString& contextKey);
};


class ControllerMapStore : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE static ControllerMapStore* get();

    explicit ControllerMapStore(IControllerMapBackend* backend,
                                QObject* parent = nullptr);
    static QString controllerId(SDL_GameController* controller);
    ~ControllerMapStore() override;

    Q_INVOKABLE QVariantMap mapFor(
            const QString& controllerId,
            const QString& libraryEntryId = QString(),
            const QString& hostApplicationKey = QString()) const;
    Q_INVOKABLE void saveMap(const QString& controllerId,
                             const QString& scope,
                             const QString& contextKey,
                             const QVariantMap& patch);
    Q_INVOKABLE void resetMap(const QString& controllerId,
                              const QString& scope,
                              const QString& contextKey = QString());
    Q_INVOKABLE QStringList knownControllerIds() const;
    Q_INVOKABLE int playerSlot(const QString& controllerId) const;
    Q_INVOKABLE bool setPlayerSlot(const QString& controllerId, int slot);

signals:
    void mapChanged(const QString& controllerId,
                    const QString& scope,
                    const QString& contextKey);

private:
    Q_DISABLE_COPY(ControllerMapStore)

    static ControllerMapStore* s_Instance;
    IControllerMapBackend* m_Backend;
};
