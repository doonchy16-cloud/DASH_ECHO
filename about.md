# ECHO_DASH 👻🔥

**Every attempt leaves a trace.**

ECHO_DASH turns your past Geometry Dash attempts into a replayable multiverse: historical ghosts, persistent attempt history, death intelligence, Replay Studio controls, and cinematic cameras.

## v1.1.2 — Runtime Reliability Hardening

v1.1.2 strengthens archive durability and replay trust beneath the Unified Ghost Engine.

- **Known-good backup retention:** each successful archive save keeps the previous validated generation as a `.bak` recovery source.
- **Automatic backup recovery:** if the primary archive cannot be trusted, ECHO_DASH attempts the retained backup before falling back to an empty safe archive.
- **Semantic replay validation:** replay timing, sequence, progress, player transforms, and camera transforms are validated before playback authority is granted.
- **Replay quarantine:** a structurally readable but semantically invalid replay is omitted while unrelated valid replays remain usable.
- **Recovery diagnostics:** ECHO_DASH tracks whether the current archive was recovered from backup and how many replay records were quarantined.
- **Replay Studio settings remain live:** settings polling is preserved before the Replay Studio early-return.
- **One ghost engine remains authoritative:** Last, Best, Last+Best, and Older are presentation identities only; they do not receive different playback engines.

## v1.1.1 — Runtime UX & Persistence Repair

- **Replay Studio entry moved to the Geometry Dash pause menu.** Nothing from ECHO_DASH needs to float over ordinary gameplay.
- **Priority aura/halo rendering was removed completely.** Last Attempt and Best Recorded identity comes from colored trails.
- **Last Attempt trail:** electric blue.
- **Best Recorded Echo trail:** gold.
- **Attempt #1 starts through the same explicit lifecycle path as every later attempt** and receives an immediate initial sample.
- **Every successful finalization is persisted before the next attempt starts.**
- Replay retention defaults to a high all-run safety ceiling and is primarily bounded by the user's configured storage budget.
- Replay Studio UI is organized around clearer attempt, timeline, transport, speed, camera, settings, and close controls.

ECHO_DASH still separates Geometry Dash's actual saved PB from the best replay ECHO_DASH owns. A historical replay is never called the real PB unless the evidence actually matches.

ECHO_DASH remains an observer: it does not intentionally control player input, physics, collision, death, or completion authority. Recorded level speed is game reality; Replay Studio playback speed is viewer control only.

**Source, build/package, installer, and in-game runtime verification are separate gates.**