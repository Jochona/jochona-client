---
name: Bug report
about: Rule out host/setup problems before reporting a bug

---
**READ ME FIRST!**
If something basic is not working (gamepad input, video, connection), it is usually host or network setup. Check the upstream troubleshooting guide first — the streaming stack is shared: https://github.com/moonlight-stream/moonlight-docs/wiki/Troubleshooting

If the problem persists specifically in Jochona, file it here.

**Describe the bug**
A clear and concise description of what the bug is.

**Steps to reproduce**
Any special steps required for the bug to appear.

**Screenshots**
If applicable. For video glitching or quality issues, please include screenshots.

**Affected games**
List games that exhibit the issue. To check for game-specific behavior, stream the host desktop and test there.

**Jochona settings**
- Have any settings been adjusted from defaults? Which ones?
- Does the problem persist after reverting to defaults?

**Gamepad-related issues (if applicable)**
- Is a gamepad connected to the host PC directly?
- Does the problem remain when streaming the desktop and testing via https://html5gamepad.com ?
  (Desktop-streaming setup: https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide)

**Client PC details**
- OS: [e.g. Windows 11 23H2]
- Jochona version: [e.g. v0.1.0]
- GPU: [e.g. Intel Arc A770]
- Linux package type, if applicable: [e.g. Flatpak]

**Host PC details**
- OS: [e.g. Windows 11]
- Host software and version: [e.g. Vibepollo x.y.z / Apollo x.y.z / Sunshine v2025.x / GeForce Experience]
- GPU and driver: [e.g. RX 7900 XT / driver 25.x]

**Logs (please attach)**
Log files named `Moonlight-###.log` live in `%TEMP%` on Windows and `/tmp` on macOS/Linux (the filename prefix is inherited from upstream and will change with a later code pass). Attach the file covering the failing session; Jochona's future in-app diagnostic export will replace this step.
