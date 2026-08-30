# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is being built as a local Geometry Dash replay and training studio: attempt ghosts, replay comparison, death intelligence, history, timelines, and cinematic playback.

## v0.6 — Attempt History

DASH ECHO now keeps immutable finalized-attempt summaries independently from heavy replay-frame retention. History records outcome, progress, duration, capture completeness, personal-best improvement, and copied death/hazard context while resolving current replay availability dynamically by attempt ID.

The history ledger is bounded to 4,096 entries, pins the current personal-best summary, and does not intentionally modify Geometry Dash saves, account state, physics, or collision authority.

Runtime verification remains intentionally deferred until v1.0.
