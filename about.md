# ECHO_DASH 👻🔥

**Every attempt leaves a trace.**

ECHO_DASH turns your past Geometry Dash attempts into a replayable multiverse: historical ghosts, persistent attempt history, death intelligence, Replay Studio controls, and cinematic cameras.

## v1.1.1 — Runtime UX & Persistence Repair

v1.1.1 is the first corrective patch driven directly by v1.1 gameplay evidence.

- **Replay Studio entry moved to the Geometry Dash pause menu.** Nothing from ECHO_DASH needs to float over ordinary gameplay.
- **Priority aura/halo rendering was removed completely.** Last Attempt and Best Recorded identity now comes from the cleaner colored trails.
- **Last Attempt trail:** electric blue.
- **Best Recorded Echo trail:** gold.
- **Attempt #1 now starts through the same explicit lifecycle path as every later attempt** and receives an immediate initial sample.
- **Every successful finalization is persisted before the next attempt starts.**
- Replay retention now defaults to a very high all-run safety ceiling and is primarily bounded by the user's configured storage budget.
- Replay Studio UI is reorganized around clearer attempt, timeline, transport, speed, camera, settings, and close controls.

ECHO_DASH still separates Geometry Dash's actual saved PB from the best replay ECHO_DASH owns. A historical replay is never called the real PB unless the evidence actually matches.

ECHO_DASH remains an observer: it does not intentionally control player input, physics, collision, death, or completion authority.

**Build verification and in-game runtime verification are separate gates.**