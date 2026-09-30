# UPG-02: coherent Diligent restoration

Date: 2026-09-30. Baseline: `7183c5c0c907277431ca0fe122e8c23dd174841e`.
Status: source package A-E prepared; runtime/build acceptance belongs to the user.
This supersedes the previously delivered, unapplied UPG-02A ZIP. Do not layer that
installer over this commit. See `STATUS.txt` and `RESTORATION_ACCEPTANCE.txt`.

## Scope and ownership

Diligent is the production GPU target. The existing threepp CPU primitive adapter
and optional meshoptimizer preparation remain separate from renderer selection.
No domain anatomy, reference fixtures, ECS, physics, navigation or job-system
rewrite is part of this commit. The existing threepp comparison renderer remains
available. Existing default shortcuts are NOT silently repointed before acceptance.

This replaces the active monolithic Diligent source with a single implementation,
not a second competing backend. Obsolete private scene/shader headers are removed.
The public `genomes::render_diligent` target and compatibility backend spelling
`DILIGENT_LEGACY` remain so application composition can select it without a new API.

## Source map

- `DiligentBackend.cpp`: native factory/window setup, frame entry/exit, resize,
  shutdown and small exception-to-Result boundaries.
- `DiligentBackendImpl.hpp`: private state, resource ownership and math utilities.
- `DiligentResources.cpp`: pipeline variants, reflected constant/resource binding,
  transient buffer writes and primary depth/shadow resources.
- `DiligentMeshUpload.cpp`: validation and persistent geometry caches; a new VB/IB
  pair is published only after both allocations succeed.
- `DiligentScenePasses.cpp`: one camera for the scene, prototype/pose lookup,
  shadow setup, opaque/transparent ordering and optional capture metadata.
- `DiligentDraw.cpp`: material draw ranges, regular instancing, skin constants.
- `DiligentUiPass.cpp`: indexed UI streaming, texture revision cache, clipping,
  premultiplied-alpha handling, debug lines and a minimal legacy text fallback.
- `DiligentCapture.cpp`: requested-frame staging readback, row-pitch-aware copy,
  PNG encoding through DiligentTools, paired JSON and failure reporting.
- `DiligentSceneRenderer.*`: presentation facade, frame transaction and neutral
  orbit state; no copied whole PresentationSnapshot just to change the camera.
- `DiligentGpuContracts.hpp`: plain CPU ABI structures with static layout checks.
- `DrawMaterialPlan.hpp`: renderer-neutral group/material validation.
- `RenderFrameTransaction.hpp`: close/abort-on-failure lifecycle, original-error retention.
- `camera/CameraController.hpp`: native Y-up orbit/fly/RTS control consuming filtered input.

## Fixed contracts

Regular vertices retain the full 52-byte CPU stride even though material regions
are not all shader inputs. Skinned vertices remain 180 bytes. The shader and CPU
share a 4640-byte skin constant layout, including CameraPosition at offset 96,
morph weights at 208 and 69 bone matrices starting at 224. The old active source
omitted CameraPosition and therefore supplied a shifted palette.

Constants are bound at both vertex and pixel stages when reflected. Heatmap
selection is -1 by default and uses the requested bone index rather than a fixed
value. Non-finite data, malformed material partitions and incomplete palettes
are rejected explicitly.

Generated geometry owns persistent USAGE_DEFAULT storage. Transient instance,
pose, material and UI data use DISCARD maps when written; their allocations are
not reused across frames. A pass-local skin upload is reused across consecutive
materials of the same instance. The telemetry field `palette_updates` measures
actual constant writes, not changed pose revisions. This distinction matters
when the character is paused but still rendered in both shadow and color passes.

Surface OFF leaves an unused prototype, which is legal. Failed scene submissions
are aborted without presenting a partial image. Cleanup does not replace the
original error. Debug rendering is independent of whether a regular batch exists.

## Materials and rendering

MaterialDescriptor plus explicit index groups supply base color, vertex-color
usage, roughness, metalness, opacity, alpha mode, cutoff, sidedness and opt-in
instance tint. The shader no longer assigns material classes from infantry-specific
region numbers. Group validation does not change indices or semantic vertex IDs.

The implementation is a compact direct-light GGX material pass with hemisphere
ambient, a main and fill light, one 2048x2048 PCF shadow map, and a fitted ACES-style
tone mapping curve into an sRGB swap-chain target. It is NOT the complete
DiligentFX PBR_Renderer, IBL or an HDR postprocess pipeline. No new external
renderer has been added. Existing DiligentTools is used for PNG encoding.

Opaque/masked ordinary meshes are instanced by mesh/range and compatible draw
state. Independent skeletal instances retain separate pose data. Blends are
stable-sorted by object-center distance and range; their triangle order is
preserved. Blended geometry does not enter the shadow pass. There is no OIT or
per-triangle sorting. Advanced GPU culling, indirect draws and meshlets are not
claimed by this restoration.

Material texture IDs are currently rejected unless zero: there is not yet a
connected neutral texture catalogue. This is an intentional explicit limit,
not a silent fallback to an unrelated material. Current infantry presentation
uses baked vertex colors and parameter-only material descriptors.

## UI and capture

Pinned RmlUi vertices and generated texture pixels use premultiplied RGBA. The
UI pass accounts for that separately from straight-alpha legacy rectangles and
encodes the final result only at the sRGB target. The inspector occupies the
same left 40% rectangle reserved by the camera. RML binds the owned `ui` model's
status/selected strings; these are updated through DirtyVariable instead of
rebinding temporary pointers. Existing per-event input filtering is retained.
Advanced RmlUi layers/filters/clip masks remain outside this pass.

Captures are queued before the selected frame and read before Present. Waiting
for staging is deliberate in this evidence-only path, not normal gameplay.
The writer accepts PNG paths, refuses existing destinations, handles RGBA/BGRA
formats and row pitch, and writes JSON with the configured Git SHA, backend,
frame, size, camera, revisions, skeleton identity and morph weights. The paired
files are not reported complete when metadata publication fails. The user must
also retain working-tree status and startup logs: configure-time SHA alone does
not certify a clean tree or a rebuilt executable.

## Checked upstream contracts

Source inspection used the submodule revisions already pinned in this repo:

- DiligentEngine `5301bfd3bb36fe231fee31ae2a28a4e5175a7551`.
- DiligentCore `b036337d68be2353c9950a85929acf796b9a6d50`:
  InputLayout.h, DeviceContext.h, RasterizerState.h and resource interfaces.
- DiligentTools `809313db98843b6fbff2f67299282672979a8532`:
  TextureLoader/interface/Image.h and TextureLoader/CMakeLists.txt.
- DiligentSamples `e670fbd11947cd90ae713f7197cc9d06d4d07b09`:
  Tutorial13_ShadowMap source and NDC-to-shadow-UV conventions.
- RmlUi `ba95ffe8bfb6370efb2cdcca927eaad4710c5413`:
  Vertex.h, RenderInterface.h and DataModelHandle.h.

This source inspection is not compiler, driver, numerical-parity or image evidence.
The agent has not executed CMake, C++, HLSL compilation, CTest or a GPU capture.

## Remaining acceptance and safe promotion

Use the dedicated Diligent presets and run script first. Windows uses D3D12;
Linux window integration currently requires X11/Vulkan. The pinned Windows
Diligent build can still need Microsoft FXC; it is not interchangeable with DXC.
The script locates installed SDK tools but does not install them or alter global PATH.

After the user's Debug/Release, contract tests, captures and input/resize review
pass, switch the ordinary default presets and RunNative in a small promotion
commit. Keep visual comparison and rollback evidence. Do not interpret this
package's 5/5 source-stage completion as all infantry modelling or the old
88-task/122-file programme being complete.
