# Replace Moonlight Qt's existing QML UI incrementally, not via a framework migration

moonlight-qt master is already a Qt 6.11 Qt Quick Controls 2 application (`app/app.pro`: `QT += core quick quickcontrols2 svg`; CI pins Qt 6.11.1), so no Widgets-to-QML or Qt5-to-Qt6 migration exists as work. We decided Jochona's shell will be built as new QML screens, controls, and design tokens inside the existing application, replacing Moonlight's UI incrementally behind feature flags, because that keeps upstream merges cheap while the shell is replaced screen by screen.

## Considered Options

- Framework rewrite (Widgets→QML, Qt5→Qt6): based on a false premise about upstream; explicitly retracted.
- Keep Moonlight layout and only skin it: cannot deliver deterministic focus, glyph rendering, or theme tokens; abandons the product premise.

## Consequences

- "The shell is a rewrite" in earlier planning notes means *new QML screens replacing old QML screens*, not a technology migration.
- Upstream UI files stay in-tree until each replacement passes controller-focus and theme tests; deletions land per-screen, not in one big-bang commit.
