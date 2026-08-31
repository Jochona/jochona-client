# Jochona theme packages

Custom themes are **versioned, data-only packages**. A theme may change
colors only; it can never contain QML, JavaScript, native libraries, shell
commands, or any other executable content (proposal §6.3). An invalid theme
fails safe: Jochona ignores it, logs a warning, and renders the built-in
`dark` palette.

## Installation layout

```text
<AppDataLocation>/themes/<id>/theme.json
```

`<AppDataLocation>` is Qt's per-user app data directory
(`~/Library/Application Support/Jochona` on macOS, `%APPDATA%\Jochona` on
Windows, `~/.local/share/Jochona` on Linux — the exact leaf segment follows
Qt's `QStandardPaths::AppDataLocation`, which is organization/application-
name-dependent; these are Jochona's current org/app identifiers, not a
hardcoded Qt default). Additional files (backgrounds, badges) may sit next
to `theme.json`; the client loads only `theme.json` today.

## theme.json — schema version 1

```json
{
  "schema": 1,
  "id": "neon-midnight",
  "name": "Neon Midnight",
  "extends": "dark",
  "colors": {
    "surface": "#101323",
    "surfaceFocus": "#1b2140",
    "accent": "#5B8CFF",
    "accentFocus": "#7FA6FF"
  }
}
```

| Field | Type | Rule |
| --- | --- | --- |
| `schema` | number | Exactly `1`. Anything else voids the package. |
| `id` | string | Must equal the containing directory name. |
| `name` | string | 1–64 characters; shown in the picker. |
| `extends` | string | One of the built-ins: `light`, `dark`, `oled`, `highcontrast`. (`system` is not a valid base.) |
| `colors` | object | Zero or more role overrides below. |

Overridable roles (all values `#rrggbb` or `#rrggbbaa` hex strings):
`surface`, `surfaceFocus`, `border`, `borderFocus`, `textPrimary`,
`textSecondary`, `accent`, `accentFocus`, `statusOnline`, `statusPairing`,
`statusOffline`, `statusUnknown`, `night`, `moon`, `moonDim`, `link`,
`scrim`.

## Validation (all must pass, or the package is ignored)

1. `theme.json` parses as JSON.
2. Every rule in the table above holds.
3. `colors` contains no key outside the overridable roles — one unknown
   role voids the whole package rather than half-theming the shell.
4. Every color matches the hex pattern; color names, `rgb()`, and other
   CSS syntax are rejected.

Validation lives in `app/gui/style/ThemeEngine.qml` (`loadCustom`). The
selection itself is persisted by `app/backend/thememanager.{h,cpp}` as the
string `custom:<id>` in the SQLite `settings` table (key `ui.theme`,
`core/settingsdatabase.{h,cpp}`); a one-time legacy import reads a prior
`QSettings` value on first launch and never dual-writes afterward.

## Notes for authors

- Contrast is the author's responsibility; `light` and `highcontrast` are
  the safest bases for bright rooms.
- Status colors must stay distinguishable without relying on color alone —
  the UI always pairs a text label with a status color.
- The user accent setting overrides `colors.accent` after the theme loads.
