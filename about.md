# DASH ECHO 👻

**Every attempt leaves a trace.**

DASH ECHO is being built as a local Geometry Dash replay and training studio: attempt ghosts, replay comparison, death intelligence, timelines, and cinematic playback.

## v0.5 — Death Intelligence

DASH ECHO now has source architecture for verified terminal-death observation, bounded raw death history, incremental death-zone clustering, 1% progress heatmap buckets, and clustered in-level death markers with repeat-count intensity.

Death analytics observe Geometry Dash outcomes after the normal `destroyPlayer` path; they do not intentionally control collision, physics, or whether the player dies. Marker visibility is optional and independent from analytics collection.

Runtime verification remains intentionally deferred until v1.0.
