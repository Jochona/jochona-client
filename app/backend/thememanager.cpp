//
// SPDX-FileCopyrightText: Jochona Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
#include "thememanager.h"
#include "core/settingsdatabase.h"
#include <QSettings>
#include <QColor>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#define SER_THEME "ui.theme"
#define SER_ACCENT "ui.accent"
#define SER_FONT_SCALE "ui.fontScale"
#define SER_COMPACT "ui.compactDensity"
#define SER_REDUCED_MOTION "ui.reducedMotion"

const QStringList ThemeManager::s_BuiltinThemeIds = {
    "system", "light", "dark", "oled", "highcontrast"
};

ThemeManager* ThemeManager::s_Instance = nullptr;

static void persistThemeSetting(const QString& key, const QVariant& value)
{
    SettingsDatabase* database = SettingsDatabase::get();
    if (database != nullptr && database->isOpen()) {
        database->setSetting(key, value);
    }
}

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
    , m_FontScale(1.0)
    , m_CompactDensity(false)
    , m_ReducedMotion(false)
{
    SettingsDatabase* database = SettingsDatabase::get();
    QSettings legacy;
    if (database != nullptr && database->isOpen()) {
        QVariantMap legacyValues;
        const QStringList keys = {
            QStringLiteral(SER_THEME), QStringLiteral(SER_ACCENT),
            QStringLiteral(SER_FONT_SCALE), QStringLiteral(SER_COMPACT),
            QStringLiteral(SER_REDUCED_MOTION)
        };
        for (const QString& key : keys) {
            if (legacy.contains(key)) {
                legacyValues.insert(key, legacy.value(key));
            }
        }
        database->importLegacySettings(
                    legacyValues,
                    QStringLiteral("migration.qsettings_theme_imported_v2"));
    }
    auto value = [database, &legacy](const QString& key,
                                     const QVariant& defaultValue) {
        return database != nullptr && database->isOpen()
               ? database->setting(key, defaultValue)
               : legacy.value(key, defaultValue);
    };

    m_ThemeId = value(QStringLiteral(SER_THEME), QStringLiteral("system")).toString();
    if (m_ThemeId != "system" && !s_BuiltinThemeIds.contains(m_ThemeId) &&
        !(m_ThemeId.startsWith("custom:") && m_ThemeId.length() > 7)) {
        // Garbage or an id shape we never issue; resolve as system
        m_ThemeId = "system";
    }

    // Validate as a color; anything unparsable means "theme default"
    m_Accent = value(QStringLiteral(SER_ACCENT), QString()).toString();
    if (!m_Accent.isEmpty() && !QColor(m_Accent).isValid()) {
        m_Accent.clear();
    }

    m_FontScale = value(QStringLiteral(SER_FONT_SCALE), 1.0).toDouble();
    if (m_FontScale < 0.8 || m_FontScale > 2.0) {
        m_FontScale = 1.0;
    }

    m_CompactDensity = value(QStringLiteral(SER_COMPACT), false).toBool();
    m_ReducedMotion = value(QStringLiteral(SER_REDUCED_MOTION), false).toBool();
    reloadThemes();
    if (!m_AvailableThemeIds.contains(m_ThemeId)) {
        m_ThemeId = QStringLiteral("system");
    }
}

ThemeManager*
ThemeManager::get()
{
    if (s_Instance == nullptr) {
        s_Instance = new ThemeManager();
    }

    return s_Instance;
}

void
ThemeManager::setThemeId(const QString& themeId)
{
    bool ok = m_AvailableThemeIds.contains(themeId);
    if (!ok || themeId == m_ThemeId) {
        return;
    }

    m_ThemeId = themeId;

    persistThemeSetting(QStringLiteral(SER_THEME), m_ThemeId);

    emit themeChanged();
}

void
ThemeManager::setAccent(const QString& accent)
{
    QString normalized = accent.trimmed();
    if (!normalized.isEmpty() && !QColor(normalized).isValid()) {
        // Never persist garbage; empty string means "use theme accent"
        normalized.clear();
    }

    if (normalized == m_Accent) {
        return;
    }

    m_Accent = normalized;

    persistThemeSetting(QStringLiteral(SER_ACCENT), m_Accent);

    emit accentChanged();
}

void
ThemeManager::setFontScale(double scale)
{
    double clamped = qBound(0.8, scale, 2.0);
    if (qFuzzyCompare(clamped, m_FontScale)) {
        return;
    }

    m_FontScale = clamped;

    persistThemeSetting(QStringLiteral(SER_FONT_SCALE), m_FontScale);

    emit fontScaleChanged();
}

void
ThemeManager::setCompactDensity(bool compact)
{
    if (compact == m_CompactDensity) {
        return;
    }

    m_CompactDensity = compact;

    persistThemeSetting(QStringLiteral(SER_COMPACT), m_CompactDensity);

    emit compactDensityChanged();
}

void
ThemeManager::setReducedMotion(bool reduced)
{
    if (reduced == m_ReducedMotion) {
        return;
    }

    m_ReducedMotion = reduced;

    persistThemeSetting(QStringLiteral(SER_REDUCED_MOTION), m_ReducedMotion);

    emit reducedMotionChanged();
}

bool
ThemeManager::isCustom(const QString& themeId)
{
    return themeId.startsWith("custom:") && themeId.length() > 7;
}

QString
ThemeManager::themesPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/themes";
}

QString ThemeManager::themeName(const QString& themeId) const
{
    if (themeId == QStringLiteral("system")) return tr("System");
    if (themeId == QStringLiteral("light")) return tr("Light");
    if (themeId == QStringLiteral("dark")) return tr("Dark");
    if (themeId == QStringLiteral("oled")) return tr("OLED");
    if (themeId == QStringLiteral("highcontrast")) {
        return tr("High contrast");
    }
    return m_CustomThemeNames.value(
        themeId, themeId.mid(QStringLiteral("custom:").size()));
}

void ThemeManager::reloadThemes()
{
    QStringList available = s_BuiltinThemeIds;
    QMap<QString, QString> names;
    const QSet<QString> allowedTopLevel{
        QStringLiteral("schema"), QStringLiteral("id"),
        QStringLiteral("name"), QStringLiteral("extends"),
        QStringLiteral("colors"),
    };
    const QSet<QString> allowedRoles{
        QStringLiteral("surface"), QStringLiteral("surfaceFocus"),
        QStringLiteral("border"), QStringLiteral("borderFocus"),
        QStringLiteral("textPrimary"), QStringLiteral("textSecondary"),
        QStringLiteral("accent"), QStringLiteral("accentFocus"),
        QStringLiteral("statusOnline"), QStringLiteral("statusPairing"),
        QStringLiteral("statusOffline"), QStringLiteral("statusUnknown"),
        QStringLiteral("night"), QStringLiteral("moon"),
        QStringLiteral("moonDim"), QStringLiteral("link"),
        QStringLiteral("scrim"),
    };
    QDir root(themesPath());
    const QStringList directories = root.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& directory : directories) {
        QFile file(root.filePath(
            directory + QStringLiteral("/theme.json")));
        if (!file.open(QIODevice::ReadOnly)) continue;
        QJsonParseError error {};
        const QJsonDocument document =
            QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError
                || !document.isObject()) {
            continue;
        }
        const QJsonObject object = document.object();
        bool valid = object.value(QStringLiteral("schema")).toInt() == 1
            && object.value(QStringLiteral("id")).toString() == directory
            && !object.value(QStringLiteral("name")).toString().isEmpty()
            && object.value(QStringLiteral("name")).toString().size() <= 64
            && s_BuiltinThemeIds.contains(
                object.value(QStringLiteral("extends")).toString())
            && object.value(QStringLiteral("extends")).toString()
                   != QStringLiteral("system")
            && object.value(QStringLiteral("colors")).isObject();
        for (auto it = object.constBegin();
             valid && it != object.constEnd(); ++it) {
            valid = allowedTopLevel.contains(it.key());
        }
        const QJsonObject colors =
            object.value(QStringLiteral("colors")).toObject();
        for (auto it = colors.constBegin();
             valid && it != colors.constEnd(); ++it) {
            const QString value = it.value().toString();
            valid = allowedRoles.contains(it.key())
                && it.value().isString()
                && (value.size() == 7 || value.size() == 9)
                && value.startsWith(QLatin1Char('#'))
                && QColor(value).isValid();
        }
        if (!valid) continue;
        const QString id = QStringLiteral("custom:") + directory;
        available.append(id);
        names.insert(id, object.value(QStringLiteral("name")).toString());
    }

    const bool changed =
        available != m_AvailableThemeIds || names != m_CustomThemeNames;
    m_AvailableThemeIds = available;
    m_CustomThemeNames = names;
    if (!m_AvailableThemeIds.contains(m_ThemeId)) {
        setThemeId(QStringLiteral("system"));
    }
    if (changed) emit themesChanged();
}
