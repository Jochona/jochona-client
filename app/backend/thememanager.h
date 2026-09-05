//
// SPDX-FileCopyrightText: Jochona Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
// Jochona theme and personalization bridge. SQLite stores the active theme,
// accent, density, text scale, and motion preference. ThemeEngine.qml owns
// the live palettes and metrics.
//
// Theme ids are stable built-in ids or custom:<id>. reloadThemes() validates
// installed data-only packages before it exposes them to QML.
//
#pragma once

#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QMap>

class ThemeManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString theme READ themeId WRITE setThemeId NOTIFY themeChanged)
    Q_PROPERTY(QString accent READ accent WRITE setAccent NOTIFY accentChanged)
    Q_PROPERTY(double fontScale READ fontScale WRITE setFontScale NOTIFY fontScaleChanged)
    Q_PROPERTY(bool compactDensity READ compactDensity WRITE setCompactDensity NOTIFY compactDensityChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY reducedMotionChanged)
    Q_PROPERTY(QStringList builtinThemeIds READ builtinThemeIds CONSTANT)
    Q_PROPERTY(QStringList availableThemeIds READ availableThemeIds
               NOTIFY themesChanged)

public:
    static const QStringList s_BuiltinThemeIds;

    Q_INVOKABLE static ThemeManager*
    get();

    QString
    themeId() const
    {
        return m_ThemeId;
    }

    void
    setThemeId(const QString& themeId);

    QString
    accent() const
    {
        return m_Accent;
    }

    void
    setAccent(const QString& accent);

    // 0.8–2.0; clamped. Scales every type token in Tokens.qml.
    double
    fontScale() const
    {
        return m_FontScale;
    }

    void
    setFontScale(double scale);

    bool
    compactDensity() const
    {
        return m_CompactDensity;
    }

    void
    setCompactDensity(bool compact);

    // Honors the user's request for less motion; Tokens.motion() collapses
    // every animation duration to 0 when set.
    bool
    reducedMotion() const
    {
        return m_ReducedMotion;
    }

    void
    setReducedMotion(bool reduced);

    QStringList
    builtinThemeIds() const
    {
        return s_BuiltinThemeIds;
    }
    QStringList availableThemeIds() const { return m_AvailableThemeIds; }
    Q_INVOKABLE QString themeName(const QString& themeId) const;
    Q_INVOKABLE void reloadThemes();

    // Well-formed built-in or custom:<id> string; anything else reads as
    // "system" at load time (garbage never reaches QML).
    Q_INVOKABLE static bool
    isCustom(const QString& themeId);

    // Directory holding installed data-only theme packages:
    // <AppDataLocation>/themes/<id>/theme.json (docs/theme-packages.md)
    Q_INVOKABLE static QString
    themesPath();

signals:
    void
    themeChanged();

    void
    accentChanged();

    void
    fontScaleChanged();

    void
    compactDensityChanged();

    void
    reducedMotionChanged();
    void themesChanged();

private:
    ThemeManager(QObject* parent = nullptr);

    Q_DISABLE_COPY(ThemeManager)

    static ThemeManager* s_Instance;

    QString m_ThemeId;
    QString m_Accent; // "" = theme default accent
    double m_FontScale;
    bool m_CompactDensity;
    bool m_ReducedMotion;
    QStringList m_AvailableThemeIds;
    QMap<QString, QString> m_CustomThemeNames;
};
