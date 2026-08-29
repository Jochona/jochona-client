# Jochona Shell — UI/UX Design Brief

Status: proposed. Supersedes the ad-hoc "ShellChrome + flag" styling.
Scope: the whole lean-back shell (home, host detail, library, pairing, settings
entry, in-stream overlays are a later appendix). Streaming surface itself is
untouched.

## 1. Subject

Jochona is a game-streaming client for a TV, a monitor, and the Steam Deck.
The user is on a couch or a handheld, holding a gamepad, ~3 m from the screen.
The shell has one job:

> Get from "controller wakes" to "inside a game on the rig" in three actions
> or fewer, while quietly proving the rig is ready.

Everything else (diagnostics, quality tuning, theming) is secondary and must
never stand between the user and Play.

## 2. Research takeaways

Sources: Android TV / tvOS HIGs, 10-foot UX writeups (Pascal Potvin, Amazon
FTV guidelines, Smashing Magazine 2025), Steam Big Picture 2023 redesign
postmortems, PS5 home behaviour, community threads on Moonlight's UI.

1. **Focus is the cursor.** One item owns focus; it must be unmistakable
   (scale 1.05–1.1×, elevation, glow). Focus must never move without user
   input. tvOS/Android TV both codify this; Big Picture lives by it.
2. **10-foot legibility floor:** body text ≥ 22–24 px at 1080p, medium/bold
   weights only, critical content inside ~90% safe area, dark environments
   win on TVs.
3. **Console shells that work (Big Picture 2023, PS5, tvOS) share one shape:**
   a small persistent nav affordance + a hero band driven by the focused item
   + horizontal shelves. Context changes because the *hero art* changes, not
   because the chrome changes.
4. **Moonlight's UI reputation** is "unbeatable engine, utilitarian shell."
   The community ask is explicitly "console-like, gamepad-first, host status
   at a glance" (r/MoonlightStreaming; forks like StreamLight exist purely to
   fix this). That is the gap this design fills.
5. **Art is scarce.** GameStream/Sunshine apps arrive as bare titles; there is
   no reliable box-art pipeline. A poster-wall design (Netflix-style) is a
   trap — it looks broken empty. The design must be beautiful *without* art
   and get better *with* it.

## 3. Thesis

**"The PC in the next room, lit by moonlight."**

Jochona is the living-room face of a machine that is somewhere else. The
shell's whole visual argument is the *link*: a dark, quiet room lit by one
cool source — and when a rig is reachable, it glows. Warm moonlight is the
affordance colour; everything not actionable stays cool and dim. You always
know what the moon is touching.

Signature element: **moonlit focus** — the focused item doesn't get a
neon border like every other dashboard; it gets a soft warm rim-light and a
slight lift, as if a lamp turned toward it. Hero backgrounds bloom from the
same light. One place we spend boldness: everything else stays disciplined.

## 4. Directions considered

| | A — Nightlink (chosen) | B — Signal Room | C — Poster Wall |
|---|---|---|---|
| Idea | Console shell: nav rail + art-driven hero + shelves | Instrument panel: monospace HUD, host telemetry grid | Netflix-style full-bleed poster grid |
| Without box art | Strong (hero carries type + glow) | Strong | **Collapses** — monogram wall |
| Gamepad ergonomics | Excellent (Big Picture lineage) | Okay, dense | Good |
| Risk | Familiarity (in a good way) | Reads like a router UI | Empty-state disaster |

**Chosen: A.** It is the shape the audience already knows (Big Picture / PS5),
it survives missing art, and the moonlight treatment gives it an identity that
is Jochona's, not a Steam clone. B's telemetry survives here as host-card
detail, not as the shell. C returns for free later: when box art exists, the
shelves simply get prettier.

## 5. Tokens

Built on `Tokens.qml`/`ThemeEngine` — extend, don't replace.

### 5.1 Palette (default theme "Moonless")

| Token | Hex | Role |
|---|---|---|
| `night` | `#0D111C` | Base. Deep blue-slate, never pure black (OLED-friendly, keeps depth) |
| `surface` | `#161C2B` | Cards, shelves |
| `surfaceLift` | `#1E2638` | Pressed/focus substrate |
| `moon` | `#F0DFAE` | **Warm moonlight — the focus/affordance colour.** Rim glow, primary CTA text-on-CTA stays dark |
| `moonDim` | `#8C86A0` | Inactive text on dark; cool silver |
| `link` | `#7FA7C4` | Cool secondary: links, chips, non-focus accents |
| `ready` `#5FBF7F` / `waking` `#D9A441` / `down` `#C4574E` | | Status dots only. Never layout colour |

Rules: at most one warm-lit element per region; status colours appear only as
8–10 px dots + label; focus glow is `moon` at 18–35 % opacity, radius ≈ 2 ×
card radius. User theme override keeps the same roles (ThemeEngine already
supports accent swap; "moon" is the new accent slot).

### 5.2 Type

Bundled faces stay: **Space Grotesk** (display), **Inter** (body/UI),
**Kenney input glyphs** (button prompts), **Mode Seven** (stream stats only).

The current scale is desktop-first; at TV scale it must clear the 10-foot
floor. Introduce viewport-adaptive `uiScale` (see §8) and:

| Role | 1080p reference | Weight |
|---|---|---|
| Hero title | 64 px | Grotesk Medium, tight tracking |
| Shelf header | 26 px | Inter Medium |
| Card title | 24 px | Inter Medium |
| Body / meta | 22 px | Inter Regular (never Light) |
| Chips / dots label | 20 px | Inter Medium, +5 % tracking |
| Micro (never load-bearing) | 17 px | Inter Medium |

All multiply by `fontScale` (user) × `uiScale` (device).

### 5.3 Metrics

Design grid 1920 × 1080 reference. Safe-area inset 4 % (54 px at 1080p) —
rail, hero text, and action rows live inside it (TV overscan). Rail 96 px
icon-only, expands to 280 px with labels when it holds focus, collapses after
2 s idle. Gutters 32 (compact 20). Card radius 14; hero radius 0 (full-bleed).
Density toggle adjusts card heights only, never type below the floor.

### 5.4 Motion

Collapse-first: every duration routes through `Tokens.motion()`.
- Focus move: 140 ms — rim-light crossfade + 1.06 scale + 6 px lift (shadow).
- Hero art change: 260 ms crossfade; scrim gradient re-tints.
- Rail expand/collapse: 180 ms.
- Screen push: content slides 32 px + fade, 200 ms; **no parallax on the
  stream surface ever.**
- Reduced motion: states appear instantly; glow still distinguishes focus.

## 6. Layout & flow

### 6.1 Home (replaces the blue-bar grid)

```
┌────┬───────────────────────────────────────────────────────────┐
│ ⌂  │        [hero: focused item's art / monogram field]        │
│ ▶  │        ░░░░░░░░░░░░░░░ scrim ░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│ ★  │        Hades II                                        │
│ ⚙  │        ●  battlestation · 4K · 120 Hz · AV1   [ ▶ Play ]  │
│    ├───────────────────────────────────────────────────────────┤
│    │  YOUR RIGS                                                │
│    │  ╭─────────╮ ╭─────────╮ ╭─────────╮ ╭ ╮                  │
│    │  │battle-  │ │steam-   │ │  +      │ │ │                  │
│    │  │station  │ │deck     │ │  Add    │ │ │                  │
│    │  │● 4K·AV1 │ │◐ waking │ │  rig    │ │ │                  │
│    │  ╰─────────╯ ╰─────────╯ ╰─────────╯ ╰ ╯                  │
│    │  CONTINUE                                                  │
│    │  [Hades II] [Balatro] [Cuphead] …                         │
└────┴───────────────────────────────────────────────────────────┘
 ◐  moon disc, bottom of rail (ambient brand; shows app state, not host state)
```

- No top bar. The header is deleted outright — ShellChrome's floating title
  and ghost actions go away; the rail is the only persistent chrome. (This
  also kills the blue-bar class of bug structurally, not by gating.)
- Focus in shelves drives the hero (PS5 rule). D-pad left/right walks the
  shelf, up jumps shelves, down from a shelf reaches its "see all".
- First action from gamepad wake: hero already shows the last-played app →
  A = resume. Three-button path to any game: rig → game → Play.

### 6.2 Host detail (replaces the per-PC app page)

```
┌────┬───────────────────────────────────────────────────────────┐
│ ⌂  │  battlestation          ● online · Sunshine 062c · AV1    │
│    │  ───────────────────────────────────────────────────────  │
│    │  QUALITY  [ Auto ▲ ] [ 4K 120 ] [ 1080p 60 ] [ Custom… ]  │
│    │           ▲ focused chip expands: "Clamped: host          │
│    │             advertises 120 Hz max" ← Negotiator reasons    │
│    │  ───────────────────────────────────────────────────────  │
│    │  GAMES            ▢▢▢▢▢▢  (monogram tiles until art)      │
│    │                   ▢▢▢▢▢▢                                   │
│    │  [ ⏻ Wake ] [ ⭳ Sleep ] [ ⚙ Host settings ]               │
└────┴───────────────────────────────────────────────────────────┘
```

- Quality chips are the Negotiator UI: `Auto` is first-class; the reason
  strings from `effectiveQualityFor().reasons` render in the focused-chip
  expander. This makes the negotiation engine visible without a settings tree.
- Wake/Sleep are secondary actions in a bottom action row, icon+label, never
  on the card itself (misclick tax on TVs).

### 6.3 Dialogs & sheets

Everything modal becomes a **bottom sheet** (Quick Sheet pattern, Big
Picture): slides up over content, content dims 40 %, first control focused,
B/back pops. Covers: pairing PIN, add-host, remove-confirm, rename, test
results. Full-screen SettingsView stays (it's a desktop-scale screen in
itself) but adopts tokens + rail.

### 6.4 Empty & error states (act as invitation, not mood)

- No rigs: hero becomes the add-rig call itself — "Add a PC running
  Sunshine" as the only focused thing.
- Offline rig: card dims to `moonDim`, moon-phase glyph hollow; focused
  expander says exactly what's wrong and offers Wake / Retry.
- Pairing failure: sheet names the failing step and the fix ("Wrong PIN —
  the PIN on the TV expires in 60 s"), never a raw error code.

### 6.5 In-stream overlay (appendix — M4+)

Guide-button opens a Quick Sheet over the stream: ping, resolution/fps/codec
readout (Mode Seven numerals), bitrate slider, disconnect. Never re-renders
the video plane; overlay is a separate surface, opacity ≤ 85 %.

## 7. Navigation model

- **Screen stack** (push/pop), not flags. `Home → HostDetail → AppDetail`.
  B pops; holding B at root does nothing scary. The `modernHomeScreen`
  feature-flag soup retires once this stack ships — one shell, no flag.
- Every screen focuses exactly one control on entry; focus restore on pop.
- Mouse coexists: hover = preview (no focus steal), click = activate + move
  focus.
- Controller guide/home buttons map: Guide → rail, Start → Quick Sheet
  (in stream), B → pop.

## 8. Device adaptation (the Deck/monitor problem)

Reference design is 1080p. `uiScale = clamp(min(w,h)/1080, 0.62, 1.0)`
applied inside Tokens: Deck (800p → 0.74) shrinks chrome but keeps type
proportions; the 10-foot floor applies only when an output ≥ 1080p is
detected *and* input mode is controller (desktop mouse users get the dense
scale they expect — Jochona is also a desktop app). Rail collapses to 64 px
on Deck. Ultrawide: hero text column caps at 1100 px; shelves use the width.

## 9. Accessibility floor

- Contrast: all text pairs ≥ 4.5:1 in every theme (ThemeEngine already
  guarantees for base pairs; new `moon`/`moonDim` pairs verified).
- Focus visible in every state incl. reduced-motion and high-brightness
  rooms (rim glow + 2 px solid ring fallback).
- `fontScale` up to 1.6 keeps grid intact (cards are container-sized, never
  pixel-fit to text).
- Colour never sole signal: every status dot carries a text label when
  focused.
- Reduced motion honored globally (existing `motion()` path).

## 10. Implementation map (what actually changes)

| Piece | Action |
|---|---|
| `main.qml` `header: ToolBar` | **Delete.** Shell chrome = rail only |
| `ShellChrome.qml` | Retire: floating title/ghost actions replaced by rail + hero |
| `HomeView.qml` | Rebuild as shelves + hero (keeps ComputerModel wiring) |
| New `NavRail.qml`, `Hero.qml`, `Shelf.qml`, `HostCard.qml`, `AppTile.qml`, `QuickSheet.qml` | New components, token-driven |
| `Negotiator` (backend, done) | Wire to QUALITY chips; `reasons` → expander text |
| `Tokens.qml` | Add palette (§5.1), `uiScale`, type scale (§5.2) |
| `ThemeEngine` | Rename accent slot → `moon`; keep user accent override |
| Kenney glyph fonts | Button prompts everywhere (Xbox/PS/Switch glyphs auto) |

Order: tokens → rail + stack + home (kills the bar) → host detail + quality
chips → sheets/empty states → polish/motion pass.

## 11. Open questions

1. "Continue" shelf needs last-played tracking per app (cheap DB column) —
   in scope now or after?
2. Moon-disc ambient: pure decoration, or app-state (idle/scanning/streaming)
   after all?
3. Box art: accept host-side art if Sunshine serves it in the future; until
   then monogram fields stay first-class citizens, not a fallback shame.
