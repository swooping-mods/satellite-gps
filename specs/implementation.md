# Satellite GPS: behavior and implementation

Describes the retained payload **0.1.0**. The behavior prose describes the retained payload; current candidate source and metadata are identified below. Gameplay qualification is a separate requirement.


<!-- source-only-sync -->
## Current source candidate

Source version **0.1.0** is saved from workshop revision `ca32bbf79ce0840d5166a232bdb3a9fdcd761254`. The retained installable payload remains **0.1.0** with its original bytes. This source update does not rebuild or requalify that payload. The current source and metadata define the candidate; gameplay, rendered behavior and co-op remain unqualified for rebuilt bytes.

The authored behavior and lifecycle descriptions below are retained from the published payload. They are not new qualification evidence for the source candidate. The integration table below follows the current candidate metadata.
<!-- /source-only-sync -->

## Behavior

Free driven guidance uses K with vanilla cruise already enabled. Captured heading or A/B positions define oriented great-circle lines, not constant compass bearings. A $750 per-owned-car module enables host-only unattended Follow line and Auto to point. Map Control Framework supplies Heading, Set A, Set B, Follow line, Auto to point and Stop. Vehicle Control Framework arbitrates a per-car lease and native unattended physics/cleaning.

## State and lifecycle

A checked ledger stores up to 64 module storage IDs; routes/orders are session-only and inert after reload. Purchase validates exact buyer/instance and confirmed debit/refund. Manual input, authority/owner/support loss, stale callbacks, full dust storage, obstacles or stalled progress stop guidance. Unattended stopping brakes before releasing the lease. Only eligible ordinary native cars can buy/use unattended mode; driver guidance does not transfer module ownership.

## Integration and configuration

Runtime dependencies: [vehicle-control-framework](https://github.com/swooping-mods/vehicle-control-framework), [map-control-framework](https://github.com/swooping-mods/map-control-framework), [shop-framework](https://github.com/swooping-mods/shop-framework), [hints-framework](https://github.com/swooping-mods/hints-framework).

The [module metadata](../source/mods/satellite-gps/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

| Service | ABI | Table size |
|---|---:|---|
| Requires `vehicle-control-framework.controls` | 1 | at least 72 bytes |
| Requires `map-control-framework.map-controls` | 1 | at least 32 bytes |
| Requires `shop-framework.shop` | 1 | at least 56 bytes |
| Requires `hints-framework.hints` | 1 | at least 24 bytes |

## Implementation sources

- [mod.c](../source/mods/satellite-gps/mod.c)
- [guidance_core.h](../source/mods/satellite-gps/guidance_core.h)
- [module_store.h](../source/mods/satellite-gps/module_store.h)
- [store.h](../source/mods/satellite-gps/store.h)

## Verification boundary

No detours, U-turns, route queue, host migration or cross-store crash transaction. Custom hover/physics cars are excluded from unattended mode. Spherical guidance math does not by itself qualify road/terrain safety or live co-op.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
