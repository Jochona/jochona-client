//
// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
#include "thememanager.h"
#include <QColor>
#include <QStandardPaths>

#define SER_THEME "ui.theme"
#define SER_ACCENT "ui.accent"
#define SER_FONT_SCALE "ui.fontScale"
#define SER_COMPACT "ui.compactDensity"
#define SER_REDUCED_MOTION "ui.reducedMotion"

const QStringList ThemeManager::s_BuiltinThemeIds = {
    "system", "light", "dark", "oled", "highcontrast"
};

ThemeManager* ThemeManager::s_Instance = nullptr;

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
    , m_FontScale(1.0)
    , m_CompactDensity(false)
    , m_ReducedMotion(false)
{
    QSettings settings;

    m_ThemeId = settings.value(SER_THEME, "system").toString();
    if (m_ThemeId != "system" && !s_BuiltinThemeIds.contains(m_ThemeId) &&
        !(m_ThemeId.startsWith("custom:") && m_ThemeId.length() > 7)) {
        // Garbage or an id shape we never issue; resolve as system
        m_ThemeId = "system";
    }

    // Validate as a color; anything unparsable means "theme default"
    m_Accent = settings.value(SER_ACCENT, "").toString();
    if (!m_Accent.isEmpty() && !QColor(m_Accent).isValid()) {
        m_Accent.clear();
    }

    m_FontScale = settings.value(SER_FONT_SCALE, 1.0).toDouble();
    if (m_FontScale < 0.8 || m_FontScale > 1.6) {
        m_FontScale = 1.0;
    }

    m_CompactDensity = settings.value(SER_COMPACT, false).toBool();
    m_ReducedMotion = settings.value(SER_REDUCED_MOTION, false).toBool();
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
    bool ok = s_BuiltinThemeIds.contains(themeId) ||
              (themeId.startsWith("custom:") && themeId.length() > 7);
    if (!ok || themeId == m_ThemeId) {
        return;
    }

    m_ThemeId = themeId;

    QSettings settings;
    settings.setValue(SER_THEME, m_ThemeId);

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

    QSettings settings;
    settings.setValue(SER_ACCENT, m_Accent);

    emit accentChanged();
}

void
ThemeManager::setFontScale(double scale)
{
    double clamped = qBound(0.8, scale, 1.6);
    if (qFuzzyCompare(clamped, m_FontScale)) {
        return;
    }

    m_FontScale = clamped;

    QSettings settings;
    settings.setValue(SER_FONT_SCALE, m_FontScale);

    emit fontScaleChanged();
}

void
ThemeManager::setCompactDensity(bool compact)
{
    if (compact == m_CompactDensity) {
        return;
    }

    m_CompactDensity = compact;

    QSettings settings;
    settings.setValue(SER_COMPACT, m_CompactDensity);

    emit compactDensityChanged();
}

void
ThemeManager::setReducedMotion(bool reduced)
{
    if (reduced == m_ReducedMotion) {
        return;
    }

    m_ReducedMotion = reduced;

    QSettings settings;
    settings.setValue(SER_REDUCED_MOTION, m_ReducedMotion);

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
