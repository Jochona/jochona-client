# Client-driven adaptive bitrate for low-latency interactive GameStream

Research note for `proposal.md` §6.12 (Adaptive streaming) and the post-1.0 **Runtime ABR** milestone (ADR-0009, CONTEXT.md). Compiled 2026-08-31.

**Scope discipline stated up front:** this document is about *runtime*, in-Session bitrate adaptation for interactive, low-latency game streaming — not buffered VOD adaptive bitrate (DASH/HLS-style manifest-switching), and not a general congestion-control literature survey. VOD ABR research (segment-based, buffer-occupancy-driven, multi-second lookahead) is deliberately out of scope: GameStream has no segments, no client buffer to protect, and a latency budget an order of magnitude tighter than streaming video. Runtime bitrate changes are available **only** when the paired host advertises the `runtime_bitrate` feature (today: Vibepollo only — proposal.md:420, CONTEXT.md "Runtime ABR"). Apollo and Sunshine expose no actuator; Launch Adaptation (pre-Session, restart-class) remains the only adaptation path there (ADR-0009).

Every external factual claim below carries a direct source URL. Claims about this repository cite file paths and line numbers from a direct read of the source at the time of writing. Passages with no direct source are marked **[INFERENCE]**.

---

## 1. What the actuator actually is (primary source: Vibepollo source code)

This is the ground truth the rest of the document designs against. It corrects and sharpens `docs/research/moonlight-ecosystem-facts.md`'s summary of the endpoint.

### 1.1 The `/bitrate` endpoint

Vibepollo's HTTPS handler `setBitrate` (`src/nvhttp.cpp`):

- The query parameter is **`bitrate`** in Kbps — `get_arg(args, "bitrate", "0")`, so the literal request is `GET /bitrate?bitrate=N`.
- Requires `PERM::_allow_view` on the pairing cert; returns HTTP 403 otherwise.
- Rejects `bitrate <= 0` with HTTP 400.
- **Server-side clamping, not negotiation:** the applied value is `min(requested, config::video.max_bitrate, 500_000 kbps)` (the 500 Mbps ceiling is a hardcoded absolute safety bound to keep `kbps * 1000` inside the encoder's 32-bit rate-control fields; `max_bitrate == 0` means host-configured "unlimited"). The response body reports the **applied** value, which the client MUST read back rather than assume the request was honored verbatim.
- Returns HTTP 404 if no active session matches the client's UUID (session ended or client mismatch).
- Source: <https://raw.githubusercontent.com/Nonary/Vibepollo/master/src/nvhttp.cpp> (handler `setBitrate`, route registration `https_server.resource["^/bitrate$"]["GET"] = setBitrate;`).

### 1.2 The `/api/abr/capabilities` endpoint

```
GET /api/abr/capabilities → {"supported":false,"version":1,"features":["runtime_bitrate"]}
```

`supported:false` is deliberate — the source comment states server-side ABR decisioning is intentionally unimplemented so that "Foundation-compatible clients … drive their own local ABR controller … through the runtime `/bitrate` endpoint above." This is the strongest possible primary-source confirmation that the *entire* control-loop responsibility (signal collection, decision, hysteresis, floors) belongs to the client. Source: same file, handler `getAbrCapabilities`.

### 1.3 How the host applies a change (`src/stream.cpp`, `src/video.cpp`)

- `set_bitrate_for_sessions()` writes the new value into session metadata and raises a `bitrate_events` event consumed by the encoder thread — **no server-side rate limiting or debouncing exists**. A client that calls `/bitrate` in a tight loop will drive the encoder thread at that rate. Cadence and hysteresis are entirely the client's problem. (`src/stream.cpp:741-777`)
- The encoder loop **coalesces bursts**: it drains all pending `bitrate_events` and applies only the latest value once per encode iteration, so "a burst of requests causes at most one reconfigure/rebuild" (code comment, `src/video.cpp:4806-4809,4810-4833`).
- **Reconfigure cost is encoder-dependent and asymmetric:** NVENC reconfigures the live encoder in place ("live" log message). avcodec-based (software/non-NVENC) encoders cannot reconfigure bitrate live; `session->set_bitrate()` returns false and the encoder session is torn down and rebuilt, which the code explicitly flags as expensive ("Rebuilding encoder to apply runtime bitrate…", `src/video.cpp:4823-4830`). A client-side controller that does not know the host's encoder backend cannot assume cheap, glitch-free bitrate steps; it must treat every requested change as potentially visible and price that into hysteresis, not just assume NVENC.
- Launch parameters separately expose a static `bitrate` at connection time and `framenogen` (frame-generation toggle), distinct from the runtime endpoint (`docs/research/moonlight-ecosystem-facts.md:128`).

**Implication for the controller design (§4):** the actuator is a coarse, HTTP-request/response, best-effort quality knob applied to an already-running encoder — not a packet-scheduler or congestion window like TCP/QUIC congestion control acts on. Any borrowed algorithm that assumes continuous per-packet ACK-clocked control (BBR, Copa, raw GCC) is adapting a mismatched actuator model and must be adapted, not ported (see §3).

---

## 2. What signals the client already has for free (primary source: this repository + moonlight-common-c)

Jochona does not need new host cooperation to get a competent signal set; moonlight-qt/moonlight-common-c already compute everything a delay-based, receiver-side controller needs, because Jochona *is* the video receiver.

### 2.1 `VIDEO_STATS` (`app/streaming/video/decoder.h:11-34`)

Per-frame-flow counters and high-resolution (1 µs) cumulative timers, refreshed on a rolling window (the client flips the active/last window roughly once per second — `app/streaming/video/ffmpeg.cpp:2121-2122`):

| Field | What it measures |
|---|---|
| `receivedFrames` / `totalFrames` / `networkDroppedFrames` | frames lost in transit (RTP/FEC layer), derivable as a loss ratio |
| `pacerDroppedFrames` | frames the client itself dropped because they arrived too late to render — a client-side congestion symptom distinct from network loss |
| `minHostProcessingLatency` / `maxHostProcessingLatency` / `totalHostProcessingLatency` | host encode latency, carried low-resolution over RTP |
| `totalReassemblyTimeUs` | time spent reassembling FEC-protected RTP into a frame — rises under packet loss/reordering |
| `totalDecodeTimeUs` | client decode latency |
| `totalPacerTimeUs` | time frames spend in the client's pacing/render queue — the closest available proxy to "queue depth" / standing delay, the signal SCReAM and Copa both center on (§3.2, §3.3) |
| `totalRenderTimeUs` | presentation latency |
| `lastRtt` / `lastRttVariance` | round-trip time and its variance, sourced from ENet (the reliable control channel), 1 ms resolution |
| `videoMegabitsPerSec` | current measured video bitrate (excludes FEC overhead) |

### 2.2 `LiGetEstimatedRttInfo()` (`moonlight-common-c/moonlight-common-c/src/Limelight.h:569-570`)

A direct, always-available RTT + RTT-variance estimate independent of `VIDEO_STATS`, callable any time between `LiStartConnection()`/`LiStopConnection()`.

### 2.3 `ConnListenerConnectionStatusUpdate` (`Limelight.h:454-457`)

A coarse binary `CONN_STATUS_OKAY` / `CONN_STATUS_POOR` callback moonlight-common-c already raises when it judges the connection degraded; the existing client already consumes it to show an overlay hint (`app/streaming/session.cpp:213-223`) but does not currently drive bitrate from it.

### 2.4 `BandwidthTracker` (`app/streaming/bandwidth.cpp:1-104`, `app/streaming/video/ffmpeg.cpp:230`)

A bucketed average/peak Mbps tracker already instantiated with a **10-second window, 250 ms buckets**, fed every received frame (`m_BwTracker.AddBytes(du->fullLength)`). This sets a natural existing cadence: the client already samples network throughput 4×/second and reports smoothed values roughly every second. A runtime ABR controller should piggyback on this cadence rather than invent a faster polling loop, both because finer-grained samples are noisier and because the host-side coalescing behavior (§1.3) means sub-second correction requests are wasted.

**Conclusion:** every signal a receiver-side delay-based congestion controller needs (arrival jitter proxy via reassembly/pacer timing, RTT and RTT variance, explicit loss ratio split by cause, a coarse host-independent "bad" flag) already exists client-side with no host protocol change. This maps closely onto the GCC draft's own recommended fallback mode for receivers that lack extended feedback support (§3.1).

---

## 3. Prior art survey: what is directly reusable, and what is not

### 3.1 WebRTC Google Congestion Control (GCC) — **partially reusable (mechanism, not wire protocol)**

Primary source: `draft-ietf-rmcat-gcc-02`, "A Google Congestion Control Algorithm for Real-Time Communication" (Holmer, Lundin, Carlucci, De Cicco, Mascolo; expired IETF Internet-Draft, last published 2016-07-08). <https://datatracker.ietf.org/doc/html/draft-ietf-rmcat-gcc-02> It was never advanced to RFC status but is the de facto specification implemented in libwebrtc.

Mechanism (delay-based controller, §5 of the draft):
1. Group packets arriving within a `burst_time` (5 ms recommended) window; compute inter-group delay variation `d(i)`.
2. Feed `d(i)` through a Kalman filter to estimate the trend `m(i)` (state noise `q = 10⁻³`, measurement-noise variance tracked with an exponential filter, coefficient `χ ∈ [0.001, 0.1]`).
3. Compare `m(i)` against an **adaptive** threshold `del_var_th`, not a fixed one — a fixed threshold was shown to let a concurrent TCP flow starve the GCC flow (draft cites [Pv13]). The threshold grows faster than it shrinks (`K_u = 0.01 > K_d = 0.00018`, initial value 12.5 ms, clamped to `[6, 600]` ms).
4. A 3-state rate controller (Increase / Decrease / Hold): multiplicative increase up to **8%/second** (`η = 1.08^Δt_s`) when far from convergence, additive increase of at most half a packet per `RTT + 100ms` response interval when close to convergence, and on over-use, **decrease to `β × current_incoming_rate`, β = 0.85 (RECOMMENDED)**.
5. A parallel loss-based controller: hold the estimate for 2–10% loss, increase 5%/update below 2% loss (`A(i) = 1.05·A(i-1)`), and multiplicatively cut by `(1 − 0.5p)` above 10% loss. The final bitrate is `min(delay-based, loss-based)`.
6. **§7 Interoperability Considerations is the single most load-bearing paragraph for Jochona:** it explicitly describes a fallback mode for a sender that talks to a receiver lacking the RTCP extensions — "the sender monitors RTCP receiver reports and uses the fraction of lost packets and the round-trip time as input to the loss-based controller. The delay-based controller should be left disabled." GameStream's actual topology inverts sender/receiver relative to typical WebRTC (the *client* is the passive video receiver and the *host* is the sender, and the client — not the host — is the one instructed to run the ABR loop per Vibepollo's design, §1.2), but the structural lesson holds: **run the delay-based estimator where the packet-arrival observations naturally occur (the client), and keep the loss-based bound as a second, independent floor**, exactly mirroring the two already-available signal families in §2 (pacer/reassembly timing vs. `networkDroppedFrames`).

What does **not** port directly: GCC's wire protocol (REMB/transport-wide feedback RTCP messages, abs-send-time header extension) has no equivalent in the GameStream/moonlight-common-c protocol, and building one is out of this proposal's scope (`proposal.md` explicitly excludes protocol changes to Vibepollo/Apollo/Sunshine required to ship the first client release, §2.2). The reusable asset is the **algorithm shape** (Kalman-filtered delay trend + adaptive threshold + AIMD-with-different-rates + loss floor), evaluated against the client's own locally-observed timers instead of a wire feedback message.

### 3.2 SCReAM — **partially reusable (delay-target concept), not a port**

Primary source: RFC 8298, "Self-Clocked Rate Adaptation for Multimedia" (Johansson & Sarker, Ericsson AB, December 2017, Experimental). <https://datatracker.ietf.org/doc/html/rfc8298>

Key ideas:
- Explicitly designed for "conversational media services such as interactive video" over variable-throughput links including LTE handovers (§1, §1.1) — the closest prior-art domain match to GameStream of anything surveyed here.
- Uses a LEDBAT-style (RFC 6817) queuing-delay estimate (`qdelay`) as the primary congestion signal, with a **target of 50–100 ms** (§3.1): the congestion window is allowed to grow while `qdelay` stays under that target and shrinks otherwise, with an *instant* reduction on loss/ECN.
- Separately maintains a **media rate control** loop, decoupled from the network congestion window, that reduces media bitrate specifically when the *sender's own RTP transmission queue* backs up, and can additionally recommend frame-skipping under rapid queue growth such as a radio handover (§3.3, §4.1.3) — i.e., SCReAM already treats "the encoder is producing faster than the network can drain" as a distinct signal from "the network path is congested," which is the same distinction `networkDroppedFrames` vs. `pacerDroppedFrames` draws in Jochona's existing telemetry (§2.1).
- "Fast increase mode" ramps the bitrate up within 5–10 seconds when no congestion is detected, similar in spirit (not formula) to TCP slow start, and specifically re-enables itself if congestion clears rather than staying permanently conservative.

What does **not** port directly: SCReAM's congestion window and RTP-queue-size signals are computed **sender-side** (i.e., host-side in the GameStream mapping), and Vibepollo exposes neither an RTP send-queue-depth metric nor an ECN channel to the client. Porting SCReAM verbatim would require host protocol changes, which is out of scope. The reusable asset is the **qdelay target philosophy** (hold a small, explicit standing-delay budget rather than only reacting to loss) — directly implementable today using the client's own `totalPacerTimeUs`/reassembly-timing trend and RTT-variance signals as a qdelay proxy, without any host change.

### 3.3 Copa — **not directly reusable; one transferable idea (delay-target formula), rejected as the base algorithm**

Primary source: Arun & Balakrishnan, "Copa: Practical Delay-Based Congestion Control for the Internet," NSDI 2018. <https://www.usenix.org/system/files/conference/nsdi18/nsdi18-arun.pdf> (also <https://www.usenix.org/conference/nsdi18/presentation/arun>)

Copa computes a **target rate** `λ = 1/(δ·d_q)` where `d_q` is measured queuing delay and `δ` is a tunable delay/throughput trade-off parameter (recommended default `δ = 0.5`), and moves its congestion window toward that target using an AIAD ("additive-increase/additive-decrease") rule with a velocity parameter that accelerates convergence, switching to AIMD-on-`1/δ` only when it detects a competing buffer-filling flow (queue not emptying at least once every 5 RTTs) (§2 of the paper).

This is rejected as Jochona's base algorithm for three concrete, source-grounded reasons:
1. Copa's update rule operates **every ACK**, adjusting a congestion window that gates individual packet transmission. Jochona's actuator is a coarse `/bitrate` HTTP call against an encoder that, on non-NVENC hosts, must tear down and rebuild to apply a change (§1.3) — applying Copa's per-RTT oscillation directly onto that actuator would cause visible, frequent re-encodes, which is exactly the behavior `proposal.md`'s §6.12 requirement ("avoid constant visible oscillation") forbids.
2. Copa's mode-switch detector needs to observe whether the queue empties on a roughly-5-RTT cadence — that requires either kernel-level packet-timing precision or a dedicated probe stream; Jochona has neither, only frame-granularity timers.
3. Copa's paper itself demonstrates its convergence properties on Mahimahi-emulated dumbbell links with many concurrent flows (§5.1) — a topology assumption (shared bottleneck with TCP Cubic) that does not describe a single dedicated GameStream session on a LAN or Tailscale path, Jochona's actual production topology (`proposal.md` §4.3, "Direct by default").

The one transferable idea: **express the controller's target as an explicit function of measured standing delay above a floor**, rather than a fixed step table — i.e., borrow the *shape* of "hold delay near a small budget, and only fight harder when a competing flow is detected" from both Copa and SCReAM, without adopting either's window-update math.

### 3.4 BBR — **rejected as a base algorithm; one transferable principle (don't conflate loss with congestion)**

Primary sources: Cardwell, Cheng, Gunn, Hassas Yeganeh, Jacobson, "BBR: Congestion-Based Congestion Control," ACM Queue 14(5), 2016 (indexed at <https://research.google/pubs/bbr-congestion-based-congestion-control-2/>; DOI record at <https://queue.acm.org/detail.cfm?id=3022184>), and the current BBRv2 IETF specification draft, <https://www.ietf.org/archive/id/draft-cardwell-iccrg-bbr-congestion-control-02.html>.

BBR builds an explicit two-parameter path model (bottleneck bandwidth `BtlBw`, round-trip propagation time `RTprop`) and cycles through Startup/Drain/ProbeBW/ProbeRTT phases, periodically re-probing `min_rtt` over a 10-second window (`MinRTTFilterLen`) by deliberately capping in-flight data to ~50% of BDP for 200 ms at least every 5 seconds (`ProbeRTTDuration`, `ProbeRTTInterval`, BBRv2 draft §2.14.2), and treats packet loss up to a 2% threshold (`BBRLossThresh`) as tolerable before backing off by a 0.7 multiplicative factor (`BBRBeta`) with 0.85 headroom (`BBRHeadroom`) below the last known-safe operating point.

Rejected as a base algorithm because BBR is fundamentally a **sender-side, continuous-feedback, packet-pacing** algorithm requiring per-ACK delivery-rate sampling (`draft-cheng-iccrg-delivery-rate-estimation`) that assumes the controller *is* the entity pacing packets onto the wire. Jochona is the receiver of a UDP/RTP media stream from a host it does not control at the transport layer, and its only actuator is the coarse `/bitrate` endpoint — there is no cwnd or pacing rate for a client-side BBR to compute. Deliberately capping "in-flight data" for a `ProbeRTT`-style phase has no analog when the client cannot throttle the host's send behavior directly. The transferable principle, consistent with GCC and Copa above, is BBR's founding critique: **loss is not a reliable primary congestion signal** on shallow-buffer or lossy wireless links — a principle Jochona should apply by weighting `pacerDroppedFrames`/pacer-timing trend (delay) above `networkDroppedFrames` (loss) when they disagree, rather than by adopting BBR's model-based state machine.

### 3.5 Low-latency interactive-video QoE research

- Sabet, Schmidt, Zadtootaghaj, Griwodz, Möller, "Delay Sensitivity Classification of Cloud Gaming Content," arXiv:2004.05609, 2020. <https://arxiv.org/abs/2004.05609> — establishes, from expert ratings validated against subjective test data, that delay sensitivity is **game-content-dependent**, not a single global threshold; a decision tree built on content features reached 86.6% accuracy classifying games into delay-sensitivity classes. This directly supports `proposal.md`'s existing per-game Settings Patch / Streaming Profile model (CONTEXT.md) as the right place to carry a **per-game quality floor and adaptation aggressiveness**, rather than one global tuning.
- Baena, Peñaherrera-Pulla, Barco, Fortes, "Measuring and Estimating Key Quality Indicators in Cloud Gaming Services," arXiv:2212.14073, 2022. <https://arxiv.org/pdf/2212.14073> — notable because the authors' measurement framework is built directly **on top of Moonlight** ("a controlled environment has been created over Moonlight Client… an open-source implementation of NVIDIA's GameStream protocol," §4.1). Their dataset (3,840 samples over a real LTE testbed, Table 1) quantifies three end-user-visible KQIs directly relevant to a benchmark scoring model (§6): `CGlatency` (input-to-display lag; 50th-percentile 30.6–498.6 ms, mean 87.4 ms across their scenarios), `FreezePercent` (0–100%, mean 8.7%), and effective frame rate (`EFPS`, 0.1–116.2 fps). Their mutual-information analysis (Fig. 4) found configured resolution, configured frame rate, and network RTT (`PING_avg`) as the dominant predictors of `CGlatency`, ahead of radio-layer signal-quality metrics — supporting bitrate/resolution/frame-rate as the correct primary adaptation levers rather than, e.g., FEC-only tuning.
- General cloud-gaming QoE literature synthesis (secondary, aggregated via search rather than a single primary paper for this specific claim): interactivity/latency and its variance ("jitter") dominate subjective QoE ratings more than raw bitrate/visual fidelity once a baseline visual quality is met, and fast-paced genres (competitive FPS) are far more latency-sensitive than turn-based/strategy content. This is consistent with, and reinforces, the Sabet et al. per-genre classification above. **[INFERENCE]** — treat the specific numeric genre thresholds circulating in secondary sources (e.g., "<20 ms for competitive FPS") as illustrative, not verified figures; only the Sabet et al. paper is source-verified here for the genre-dependence claim itself.

### 3.6 Reproducible network-condition testing

- Netravali, Sivaraman, Das, Goyal, Balakrishnan, Winstein, Mickens, "Mahimahi: Accurate Record-and-Replay for HTTP," USENIX ATC 2015. <https://www.usenix.org/conference/atc15/technical-session/presentation/netravali> / PDF: <http://mahimahi.mit.edu/mahimahi_atc.pdf> — a composable-shell network emulator (`mm-delay`, `mm-loss`, `mm-link`) that replays recorded packet-delivery traces deterministically; it is also the emulator Copa's own evaluation used to produce its cwnd/RTT oscillation figure (Copa paper, Figure 2 caption: "12 Mbit/s Mahimahi emulated link"), establishing it as the standard tool in this exact research area.
- Yan, Ayers, Zhu, Fouladi, Hong, Zhang, Levis, Winstein, "Pantheon: the Training Ground for Internet Congestion-Control Research," USENIX ATC 2018 (best paper). <https://www.usenix.org/conference/atc18/presentation/yan-francis>, code/traces at <https://github.com/StanfordSNR/pantheon> — a multi-country, cellular-and-wired measurement corpus and Mahimahi-based replay harness used to evaluate Copa, Vivace, and other schemes reproducibly; the project's archived per-packet traces are a ready-made source of realistic cellular/Wi-Fi bandwidth-variation traces for a Jochona benchmark corpus rather than inventing synthetic ones from scratch.

---

## 4. Recommended controller architecture

This is a synthesis grounded in §1–§3; it is a design recommendation for Jochona's implementation, **not** a claim that any cited source specifies GameStream ABR — no such source exists.

### 4.1 Where it runs and against what

A single client-side controller per Session, active only when the paired Host's normalized capability model reports `runtime_bitrate` (per §1.2 and `proposal.md` §4.4's capability-driven-compatibility principle) — no controller instance, no actuator, no UI surface on hosts that don't advertise it. This matches ADR-0009 exactly: Launch Adaptation remains the only adaptation path everywhere else.

### 4.2 Candidate control signals (all already available per §2 — no host protocol change needed)

| Signal | Source | Role |
|---|---|---|
| RTT and RTT variance | `LiGetEstimatedRttInfo()` / `VIDEO_STATS.lastRtt`, `lastRttVariance` | primary delay-trend input (GCC/SCReAM/Copa all center delay) |
| Pacer queue time (`totalPacerTimeUs` / decoded frame) | `VIDEO_STATS` | client-side standing-delay proxy (SCReAM's qdelay analog, computed locally since no host RTP-queue telemetry exists) |
| Reassembly time (`totalReassemblyTimeUs`) | `VIDEO_STATS` | rises under loss/reordering before frames are fully lost — an early-warning delay signal |
| `networkDroppedFrames` ratio | `VIDEO_STATS` | loss-based floor, mirrors GCC's loss controller thresholds |
| `pacerDroppedFrames` ratio | `VIDEO_STATS` | client-can't-keep-up signal, distinct from network loss (SCReAM's queue-growth distinction) |
| `CONN_STATUS_POOR` callback | moonlight-common-c | coarse independent corroborating signal, cheap sanity check against the derived signals above |
| Network-interface change events | existing Tailscale/route-classification heuristics (`proposal.md` §6.6) | triggers a full re-probe, not incremental adaptation (§4.6) |
| Measured achieved video Mbps | `BandwidthTracker` (10 s window / 250 ms buckets) | convergence check, analogous to GCC's `R_hat` used to detect "close to convergence" |

Deliberately **not** a signal: host-side encoder-internal state. Vibepollo exposes none, and reading it would require a protocol change out of scope.

### 4.3 Update cadence

Recommend evaluating the controller once per second, aligned with the client's existing `VIDEO_STATS` window-flip cadence (§2.1) and `BandwidthTracker`'s natural reporting rate (§2.4), **not** the finer 250 ms bucket granularity. Rationale, grounded in §1.3: the host coalesces bursts of `/bitrate` calls to the latest value and, on non-NVENC encoders, must rebuild the encoder session per applied change — sub-second correction requests are wasted work at best and a source of visible re-encode glitches at worst. A 1 Hz cadence is also consistent with GCC's own recommendation to run its rate-control routine "at least once every `response_time` interval" (≈ RTT + 100 ms, typically well under 1 s on LAN/Tailscale paths) as a *minimum* bound, not a target.

### 4.4 Reaction asymmetry (fast-decrease / slow-increase)

Directly adopt the GCC/SCReAM/Copa consensus shape — fast, immediate reaction to congestion signals; slow, cautious recovery — rather than inventing new constants:

- **Decrease:** on sustained over-use (delay-trend signal above an adaptive threshold for a minimum dwell time, mirroring GCC's `overuse_time_th`) or loss above a hard ceiling, cut the requested bitrate by a multiplicative factor in the GCC-recommended range **[0.80, 0.95]**, defaulting to **0.85** (`draft-ietf-rmcat-gcc-02` §5.6, Table 1) applied to the last *measured* achieved bitrate (`BandwidthTracker.GetAverageMbps()`), not the last *requested* value — this avoids ratcheting down from a value the host never actually delivered.
- **Increase:** additive, not multiplicative, once near a recent known-good ceiling (tracked as an exponential moving average of the pre-decrease rate, mirroring GCC's convergence-tracking mechanism, §5.5) — step by a small fixed fraction of headroom to the host-advertised ceiling per evaluation tick, never by a multiplicative factor, so a return to full bandwidth takes several seconds rather than one jump. This directly satisfies `proposal.md` §6.12's explicit requirement to avoid "constant visible oscillation."
- **Loss floor, independent of the delay-based path:** below 2% loss, no loss-driven action; 2–10% loss, hold; above 10% loss, apply an immediate multiplicative cut independent of the delay-based controller's current state (GCC §6 thresholds, reused as-is — they are dimensionless and protocol-agnostic). Take `min(delay-based target, loss-based target)` each tick, exactly as GCC does.

### 4.5 Hysteresis and dead zone

- Maintain a small delay budget (SCReAM's 50–100 ms qdelay target, §3.2, is a reasonable starting point given it was tuned for the same class of problem — interactive real-time media over variable-quality links) computed as pacer-time-trend and RTT-variance growth **above a rolling baseline**, not an absolute number, since baseline RTT varies enormously between a LAN and a Tailscale-relayed path.
- Require the delay-trend signal to stay above threshold for a minimum dwell time (GCC's `overuse_time_th` = 10 ms is too tight for frame-granularity client timers; a multi-second dwell, calibrated during benchmark tuning per §6, is more appropriate given the ~1 Hz evaluation cadence).
- Never issue a new `/bitrate` request within a minimum inter-request interval regardless of signal state, to respect the host's coalescing behavior (§1.3) and avoid needless encoder rebuild churn on non-NVENC hosts.

### 4.6 Quality floors and route-change handling

- Respect the user's **Quality Floor** (CONTEXT.md: "if capability safety requires a lower value, Jochona asks before launch rather than silently violating it or permanently blocking access") as a hard lower bound on the controller's output; if sustained congestion would require going below the floor, surface the diagnostic ("Wi-Fi jitter is causing dropped frames," `proposal.md` §6.14 example) and let the user decide, rather than silently violating the floor.
- Per-game floors and adaptation aggressiveness should live in the existing Streaming Profile / Settings Patch mechanism (CONTEXT.md), keyed off Library Entry, consistent with Sabet et al.'s finding that delay sensitivity is game-content-dependent (§3.5) — a fast-paced game and a turn-based game should not share one hysteresis/floor configuration.
- **Route change (interface transition, Tailscale reconnect) invalidates the controller's learned state.** On a detected interface change (`proposal.md` §6.6, §6.10), reset the convergence-tracking average and the adaptive delay threshold to their initial values and restart from a conservative probe rather than trusting stale baselines from the old path — mirroring Copa's explicit RTTmin-reset-on-route-change design (bounded to "10 seconds or time since flow start," Copa paper §2.1) rather than letting a stale low-RTT LAN baseline misclassify a new, legitimately-higher-RTT Tailscale path as congested indefinitely.

### 4.7 Safety invariants

1. Never request outside the Client's configured controller limits. Vibepollo does not advertise its policy ceiling, so treat a lower applied value in the `/bitrate` response as a discovered Session ceiling and as ground truth for subsequent decisions (§1.1).
2. Never issue overlapping/racing requests; serialize controller decisions against in-flight HTTP calls.
3. Restart-class settings (resolution, FPS, codec, HDR) remain out of the runtime controller's authority entirely — they stay behind explicit Apply & Reconnect per ADR-0009; the runtime controller's only lever is bitrate.
4. On any host response other than success (403/404/400, §1.1), fall back to Launch-Adaptation-selected values and surface a diagnostic; do not retry in a tight loop.
5. Controller decisions and their triggering signal values are logged into the Support Bundle facility already specified (`proposal.md` §6.14), so a user-reported "kept dropping quality" complaint is diagnosable without new plumbing.

---

## 5. Rejected / alternative approaches

Summarized from §3, with the specific tradeoff that motivated rejection:

1. **Port WebRTC GCC's algorithm and wire protocol wholesale.** Rejected: the wire protocol (transport-wide feedback RTCP, abs-send-time extension) has no GameStream equivalent and would require host-side protocol changes explicitly out of scope for the first client release (`proposal.md` §2.2). *What we keep instead:* the algorithm shape (Kalman-filtered delay trend, adaptive threshold, AIMD-with-different-rates, independent loss floor), evaluated purely against client-local timers (§4.4).
2. **Port SCReAM (RFC 8298) as specified.** Rejected: its congestion window and RTP-send-queue signals are computed sender-side, which in the GameStream topology is the *host* — a quantity Vibepollo does not expose to the client, and adding it is a host protocol change out of scope. *What we keep instead:* the explicit standing-delay-target philosophy (§4.5) and the two-tier "network congestion" vs. "local queue growth" distinction, both already expressible with existing client telemetry.
3. **Adopt a full delay-based window-update algorithm (Copa) or a model-based sender algorithm (BBR).** Rejected on the same fundamental mismatch: both assume the controller directly paces or windows packet transmission on a per-ACK/per-RTT basis; Jochona's only actuator is a coarse, HTTP-request/response, best-effort quality parameter on an already-running host encoder that may require a full rebuild to apply (§1.3), and adapting a per-RTT-oscillation algorithm onto that actuator would violate `proposal.md` §6.12's explicit anti-oscillation requirement. *What we keep instead:* Copa's "target a small explicit delay budget above baseline" framing and BBR's "loss is not a trustworthy primary congestion signal on lossy/shallow-buffer links" critique — as design principles, not algorithms.
4. **Server-side/host-side ABR.** Rejected as *unavailable*, not merely undesirable: Vibepollo's own capability endpoint reports `"supported":false` for exactly this and states server-side ABR is "intentionally unimplemented" so that clients drive their own controller (§1.2). There is nothing to integrate with even if it were architecturally preferred.
5. **Reconnect-based adaptation (tear down and restart the Session with new settings).** Rejected per the already-recorded ADR-0009 decision: "repeated Session teardown is visibly disruptive and inherently oscillation-prone." Restart-class fields (resolution, FPS, codec, HDR) stay behind explicit Apply & Reconnect; only bitrate gets a live-adjustment path, and only against hosts that advertise the actuator.
6. **A fixed (non-adaptive) step table keyed purely on discrete "poor/okay" state, ignoring magnitude.** Considered and rejected as too coarse: `CONN_STATUS_POOR` alone (§2.3) collapses a wide range of severities into one bit, which cannot support a monotonic, hysteresis-respecting decrease/increase path; it is retained only as a cheap corroborating signal (§4.2), not the primary decision input.

---

## 6. Benchmark: deterministic network-trace workload and scoring model

### 6.1 Method

Use deterministic packet-trace replay against a real Jochona↔Vibepollo pair rather than live/uncontrolled networks, following the established methodology in this exact research area: Mahimahi's composable shells (`mm-delay`, `mm-loss`, `mm-link`) replay a recorded or synthesized trace with byte-for-byte reproducibility (Netravali et al., USENIX ATC 2015, §6.1); this is the same tool Copa's own evaluation used (Copa paper, Fig. 2 caption). On non-Linux client platforms (macOS is a first-class Jochona target per `proposal.md` §1) an equivalent trace-driven shaper is required — **[INFERENCE]**: `dummynet`/`pfctl` on macOS and Linux `tc netem` are the standard platform-native substitutes for reproducing the same class of bandwidth/delay/loss trace, but this repository has not verified a specific cross-platform trace-replay harness; selecting and vendoring one is implementation work, not covered by this research pass.

Realistic bandwidth-variation traces should be drawn from, or modeled on, the Pantheon corpus (multi-country cellular and wired measurements, Yan et al., USENIX ATC 2018) rather than hand-authored synthetic curves, to avoid tuning the controller against an unrealistic trace shape it was also designed around.

### 6.2 Scenario matrix

| Scenario | Trace shape | Tests |
|---|---|---|
| Clean baseline | Fixed high bandwidth, near-zero loss, LAN-like RTT (<5 ms) | No adaptation should ever trigger; establishes a control run |
| Step-down / step-up | Bandwidth drops sharply (e.g., 40 Mbps → 8 Mbps) for a sustained interval, then returns | Fast-decrease responsiveness (time-to-react, target overshoot) and slow-increase recovery shape (time-to-recover, oscillation count on the way back up) |
| Sawtooth Wi-Fi contention | Repeated smaller oscillations around a mean, high-frequency, moderate amplitude | Hysteresis/dead-zone correctness — must **not** chase every ripple (`proposal.md` §6.12's anti-oscillation requirement is directly testable here) |
| Cellular-style handover | Brief (1–3 s) near-total outage, then abrupt bandwidth restoration, modeled on the LTE handover characteristics SCReAM §1.1 describes | Recovery behavior distinct from a sustained step-down; tests whether the controller correctly distinguishes a transient glitch from a persistent congestion event |
| Competing bulk flow | A concurrent greedy TCP Cubic (or similar loss-based) flow sharing the emulated bottleneck, following the same evaluation pattern GCC (§7), Copa (§5.1, §5.6), and BBR's own papers all use to test fairness | Whether the controller degrades gracefully (keeps interactivity) rather than either starving entirely or hogging the link against a competing flow — directly relevant since Jochona sessions commonly share a home or Tailscale link with other traffic |
| Route change / interface transition | Mid-session RTT/bandwidth-profile discontinuity simulating a Tailscale reconnect or Wi-Fi→wired transition | Baseline-reset correctness (§4.6) — old-path statistics must not misclassify the new path |
| Floor-violation pressure | Sustained bandwidth below the configured Quality Floor for the active Library Entry | Correct floor-respecting behavior and diagnostic surfacing rather than silent floor violation (CONTEXT.md "Quality Floor") |

### 6.3 Scoring model

Composite score built from metrics the client already computes (§2), plus the interactivity-weighted structure the QoE literature supports (§3.5):

- **Interactivity terms (weighted highest, per Sabet et al.'s finding that delay sensitivity dominates and is genre-dependent, §3.5):**
  - Input-to-display latency distribution (median and p95), directly analogous to Baena et al.'s `CGlatency` KQI (§3.5) — reproducible from client-side host-processing-latency + decode + pacer + render timers already in `VIDEO_STATS`.
  - `pacerDroppedFrames` ratio (client-side freeze proxy, analogous to Baena et al.'s `FreezePercent`).
- **Visual-quality terms (weighted lower, consistent with the QoE literature's "diminishing returns above a sufficient threshold" finding, §3.5):**
  - Achieved video Mbps relative to the scenario's available bandwidth (utilization efficiency).
  - `networkDroppedFrames` ratio (encode-artifact proxy).
- **Stability term (directly testable against `proposal.md` §6.12's explicit anti-oscillation requirement):**
  - Count and amplitude of bitrate direction reversals per minute of steady-state operation in the sawtooth scenario; a controller that reverses direction on every sample fails this term regardless of its other scores.
- **Floor-respect term (binary/pass-fail):** any tick where the applied bitrate falls below the active Quality Floor without the required user prompt (§4.6, §4.7) is an automatic scenario failure, independent of the weighted score above — this is a safety invariant, not a quality trade-off.

Report per-scenario scores rather than one aggregate number: a controller that trades slightly worse steady-state utilization for zero floor violations and low oscillation is the correct trade per `proposal.md`'s own principles (excellent defaults, recover rather than fail), and a single blended score would obscure that trade.

---

## 7. Open product questions

These are decisions this research pass surfaces but does not resolve; they should become ADRs before Runtime ABR implementation begins, per the CONTEXT.md/ADR discipline this repository already follows.

1. **Per-game vs. per-Host-Application vs. global tuning granularity.** Sabet et al. (§3.5) establishes that delay sensitivity is genre-dependent, and the existing Settings Patch specificity order (Host Application > Client Device > Display > Host > Global, `proposal.md` §6.8) already supports per-game overrides — but should the *default* controller constants (dwell time, decrease factor, increase step) differ by inferred genre out of the box, or should Jochona ship one conservative default and let per-game tuning be an explicit, discoverable user action? The former needs a genre-classification signal Jochona does not currently have; the latter risks most users never discovering it.
2. **How aggressively should the controller trust `CONN_STATUS_POOR`** relative to the richer `VIDEO_STATS`-derived signals? It is cheap and independently computed by moonlight-common-c, but its exact trigger logic is not part of the public API surface this research reviewed — is it redundant with, a leading indicator of, or sometimes contradictory to the pacer/reassembly-timing trend?
3. **What is the right behavior when the host's applied bitrate (the `/bitrate` response value, §1.1) is clamped well below the requested value** because of a host-side `max_bitrate` policy ceiling the client cannot see in advance? Should the controller treat the ceiling as a discovered fact and stop probing above it for the remainder of the Session, or re-probe periodically in case the host administrator raises the ceiling mid-session?
4. **Apollo's `/action/bitrates` endpoint** (`docs/research/moonlight-ecosystem-facts.md:111`) is a distinct, unverified-semantics endpoint on a different host family; should Runtime ABR's actuator abstraction be designed now to accommodate a second, differently-shaped adapter later, or should that wait until Apollo's endpoint semantics are actually verified (the existing facts doc explicitly flags this as unconfirmed)?
5. **Cross-platform deterministic trace replay tooling** for the benchmark (§6.1) is unresolved: Mahimahi is Linux-only; a decision on the macOS/Windows equivalent (or a decision to run the trace-replay benchmark only in Linux CI and treat other platforms as smoke-tested against live networks) is needed before the benchmark in §6 can be automated.
6. **Should the controller ever request a bitrate *increase* opportunistically during a Quality-Floor-violation prompt's pending user decision**, or must it hold at the floor-violating-but-best-available value until the user responds? CONTEXT.md specifies the prompt-before-violate behavior but not the controller's behavior *during* the prompt.
