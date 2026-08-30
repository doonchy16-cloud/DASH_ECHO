# DASH ECHO v0.5 — Death Intelligence Architecture

Status: **SOURCE IMPLEMENTED / RUNTIME UNVERIFIED BY PROJECT DECISION**

Gameplay/build verification remains deferred until the integrated v1.0 milestone.

## Intended outcome

Turn terminal failures into durable local training intelligence without making DASH ECHO a collision/death authority and without coupling analytics correctness to marker rendering.

The v0.5 authority chain is:

`PlayLayer::destroyPlayer`
→ Geometry Dash normal death logic
→ confirmed `PlayerObject::m_isDead`
→ `DeathEvent`
→ incremental `DeathCluster` + progress heatmap aggregates
→ optional `EchoDeathOverlay`

## Death confirmation boundary

DASH ECHO captures candidate context before calling the normal `PlayLayer::destroyPlayer` implementation because player/hazard transforms are most trustworthy before death effects mutate presentation.

It does **not** immediately commit a death event.

After the normal death call returns, the event is accepted only when:

- capture is enabled
- an authoritative active attempt exists
- the player existed before the call
- the player was not already dead before the call
- the player reports `m_isDead == true` afterward
- the event passes finite-value validation
- that attempt has not already produced an accepted death event

This means DASH ECHO observes the resulting death state instead of deciding whether death should occur.

## One terminal death per attempt

Attempt ID is the authoritative deduplication key.

`destroyPlayer` can potentially be reached more than once because of dual-player cleanup or chained hooks. v0.5 records one terminal death outcome per attempt rather than allowing duplicate calls to inflate analytics.

The accepted event stores:

- monotonic death event ID
- attempt ID
- attempt-relative recorder time
- progress percentage
- triggering player index (1 or 2)
- player world position
- optional hazard presence
- hazard object ID when available
- hazard world position when available

## Raw history vs aggregate authority

A source-review finding showed that rebuilding every cluster and every heatmap bucket from all retained deaths after each new death would scale poorly in long sessions.

v0.5 therefore separates two concepts:

### Recent raw detail

- bounded to 4,096 `DeathEvent` records
- oldest raw detail is discarded after the cap
- useful for later history/detail UI

### Lifetime-session aggregate intelligence

- total death count continues beyond raw-detail eviction
- cluster counts/centroids update incrementally
- heatmap counts update incrementally
- crossing the raw-event cap does not force a full-history rebuild

This prevents raw retention policy from silently redefining long-session aggregate truth.

## Death clustering

Clusters represent repeated spatial/progress death zones.

Current source heuristics:

- event-to-cluster spatial radius: 54 world units
- event-to-cluster progress window: 1.50 percentage points
- compatible-cluster merge radius: 70% of the admission radius
- compatible-cluster merge progress window: 70% of the admission window

When an event fits multiple clusters, the lowest normalized spatial/progress score wins.

Clusters maintain weighted:

- world-space centroid X/Y
- mean progress
- minimum progress
- maximum progress
- death count
- first attempt ID
- last attempt ID

### Cluster capacity

Cluster count is hard-capped at 512.

If a new death fits no existing cluster and all 512 cluster slots are already meaningful, v0.5 does **not** force the event into an unrelated cluster merely to preserve a perfect clustered count.

Instead:

- total deaths still increments
- the progress heatmap still records the death
- recent raw history still records the death
- `unclusteredDeaths` increments

This preserves semantic cluster quality under saturation.

## Progress heatmap

v0.5 maintains 100 fixed buckets:

- bucket 0 = [0%, 1%)
- ...
- bucket 99 = [99%, 100%]

Each accepted death increments exactly one bucket.

Every bucket exposes:

- begin percentage
- end percentage
- death count
- normalized intensity relative to the hottest bucket

The heatmap is aggregate session intelligence and is not truncated when recent raw death-event retention rolls over.

## Marker presentation

`EchoDeathOverlay` consumes cluster authority but does not mutate it.

Presentation rules:

- marker rendering is capped at 24 clusters
- selection prioritizes highest death count, then most recently active clusters
- selected markers render in weaker-to-stronger order
- repeated deaths increase marker radius
- heat intensity increases visual intensity
- clusters with 2+ deaths receive `xN` count labels
- marker rendering occurs in the live player's coordinate space
- marker visibility is controlled by the `Death Markers` setting

Turning marker rendering off does **not** disable death analytics.

## Dependency impact

### v0.6 attempt history

Can associate retained recent `DeathEvent` records with attempt IDs without reconstructing death truth from visual markers.

### v0.7 / v0.8 replay timeline and scrubbing

Death timestamps and attempt IDs are already explicit, allowing future death jump-points on replay timelines.

### future analytics UI

Can consume clusters and heatmap buckets directly; it must not scrape the world-space marker layer.

## Adversarial review / unresolved runtime questions

The following remain unverified until v1.0:

- whether all real death paths leave `PlayerObject::m_isDead` true after the chained hook returns
- dual-mode ordering when multiple player death callbacks occur for one terminal attempt
- whether 54 world units / 1.50% are ideal death-zone thresholds across unusual levels
- whether marker z-order is visually clear across extreme object-layer configurations
- whether `bigFont.fnt` count labels remain readable across camera zoom modes
- progress-percentage usefulness in platformer levels
- exact hazard object IDs supplied for every death class
- performance under pathological sessions with hundreds of distinct death clusters

Spatial cluster data remains meaningful even when progress heatmap semantics are weak; v1.0 platformer testing must determine whether platformer needs a separate path-distance or checkpoint-oriented heatmap.

## v1.0 verification gate

At minimum test:

- spike/block deaths
- moving-object deaths
- null/unknown hazard death paths
- cube/ship/ball/UFO/wave/robot/spider/swing deaths
- dual mode, including which player triggers terminal death
- practice mode/checkpoint deaths
- death near teleport/speed/mode transitions
- marker enable/disable behavior
- repeated deaths at one obstacle
- two nearby but distinct death obstacles
- long sessions with many distinct clusters
- platformer deaths and progress semantics
- compatibility with another mod that hooks or prevents `destroyPlayer`

Until that evidence exists, v0.5 remains **SOURCE IMPLEMENTED / 🟡 HOLD FOR RUNTIME VERIFICATION**.
