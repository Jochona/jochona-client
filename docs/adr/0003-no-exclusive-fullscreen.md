# Drop exclusive (flip-model) fullscreen; windowed + borderless only

The session overlay must render independently of the streamed frame and stay available when decoding stalls, but exclusive fullscreen hands the display to the video swap chain, where a Qt-rendered overlay cannot reliably appear. We decided to offer only windowed and borderless fullscreen: on DWM/Wayland/macOS composition, borderless is functionally equivalent for streaming latency, so the overlay works unconditionally and one platform-specific code path disappears.

## Consequences

- Proposal §6.9's "exclusive/fullscreen presentation modes" is amended to "windowed and borderless fullscreen".
- If a future latency study proves a real borderless-vs-exclusive gap, revisiting this means solving the overlay problem first, not the presentation problem.
