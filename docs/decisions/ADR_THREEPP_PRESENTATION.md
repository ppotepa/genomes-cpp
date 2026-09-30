# ADR: threepp presentation on the native Genomes engine

Status: SUPERSEDED by `ADR_LIGHTWEIGHT_TOOLKIT.md` (2026-09-30).
Do not treat the earlier GLRenderer migration as the accepted final stack.
The owner clarified Diligent for GPU rendering plus selected CPU geometry tools.

## Historical record

The earlier implementation introduced an SDL/OpenGL threepp bootstrap and later
an experimental scene/skin/UI adapter. Historical baseline:
`188dc2aac34821f2a21cec3eb66ac5049f3ccf60`; first code commit:
`25a661f343509f676e60d67be69846b40e3cc339`.
The pinned threepp revision remains
`ad9571cbcbb5e27c4dd582d4810fb0b235534f60`.

That code is retained for comparison and to avoid a destructive rollback.
It is not authorization to remove Diligent, import scene-graph types into domain
modules or replace the procedural infantry generator with authored assets.

The original historical text is retained in Git at
`66843b5eae186b416c140c5e6157c1ea64477fc8`.
See `ADR_LIGHTWEIGHT_TOOLKIT.md` and
`docs/migration/lightweight-toolkit/POSTEP.txt` for active work. No part of this
historical record normatively recommends GLRenderer or threepp.
