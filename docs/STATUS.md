# Development status

Version 0.1.0 is a buildable, functional foundation. **The complete master specification and Studio end-to-end
acceptance are not complete.** No final parity, performance, avatar accuracy or cross-platform acceptance is claimed.

## Implemented

- C++20/CMake/vcpkg setup; native Qt Widgets shell and Vulkan 1.3 integration.
- Dockable dark panels, Animate/Edit/Display panel presets, saved custom layout and geometry.
- Real shaded block viewport, grid/world ground axes, camera orbit/pan/zoom, front/right/top,
  orthographic/perspective, selection tint, focus, ray-box picking, resize lifecycle, optional 4x MSAA.
- W/E/R move/rotate/resize handles, local/world joint axes, Shift/toolbar snapping, Esc/focus-loss cancellation,
  isolated previews and one undo command per drag. Geometry resizing uses part-local axes without scale keyframes.
- R6/R15 connected block templates; custom hierarchy data, exact arbitrary rigid C0/C1, validation.
- Inspector joint position/quaternion rotation authoring; part sizes, visibility, locking.
- Multiple clips: new/rename/duplicate/delete; duration/loop; add/replace/delete/move keys; frame snapping;
  playback with elapsed time, timeline zoom/scroll, SLERP and five interpolation modes.
- Snapshot command undo/redo for document operations; add/resize/delete block parts.
- Versioned project save/load with atomic writes, autosave/recovery, recent projects.
- Baked animation export and desktop reimport; original timestamps/endpoints retained; unsupported scale/world-root motion rejected.
- Studio companion: avatar import through official APIs, exact rig JSON export, strict animation import,
  matching block rig construction, sequence preview APIs, Studio history entries and diagnostics.
- Automated mathematical/core/file/export tests and native editor/Vulkan smoke tests.
- Successful Windows Debug/Release builds, core tests and ZIP/NSIS generation in CI; Ubuntu Qt 6.4 compatibility.
- Extracted Windows package runtime-check step and tagged-release workflow. No tagged release has been published.

## Exact remaining work, by roadmap phase

### Foundation/rendering

- Hardware and Windows validation; macOS/MoltenVK integration; Vulkan failure/device-loss recovery tests.
- Proper linear/sRGB material pipeline, textured mesh buffers, index buffers, descriptors, uniforms/storage,
  texture images/samplers/staging, transparency, additional quality settings, shadow maps, outlines, bone display.
- Instancing, frustum culling, buffer/texture resource caches, disk pipeline caching, asynchronous assets,
  measurement-driven multithreading, CPU/GPU profiling, GTX 1060 memory/frame-time measurements.
- Configurable shortcuts and all six standard views; current gizmos support axis-constrained joint move/rotation
  and part-local block resizing.
- Professional icon set, project dialog, all workspace layouts, full persistent settings UI and customization.

### Rigging and first Studio acceptance

- Execute docs/STUDIO_WORKFLOW.md for R6/R15 on Studio, including pivot mismatch, pose hierarchy and looping checks.
- Validate authoritative default R6/R15 attachment frames/dimensions against exported Studio rigs.
  Current templates are editable block approximations, not accurate avatar packages.
- Mesh bodies, body proportions, accessories/clothing/layered clothing, generic custom bone/skinning import.
- Rig repair UI, reparent/group/naming UI, arbitrary joint creation, pivot controls and retargeting.
- Native username/user-ID avatar downloader with verified supported endpoint/authentication requirements,
  if feasible. Current supported path uses Studio and transfers joint data plus bounding boxes only.

### Animation/export

- Timeline multi-selection, clipboard/copy/paste/duplicate/mirror/time-scale keys, markers, events, loop regions,
  custom FPS UI, speed UI, multiple-track controls, clip-name conflict UX.
- Cubic Bézier curve representation, tangent semantics and full editable graph editor with proper quaternion handling.
- Two-bone IK, poles, IK/FK, pose clipboard/mirroring/library; editable physics trajectories and motion baking.
- R6/R15 editable animation presets; accurate joint-specific limits; animation layers/blending if required.
- Studio runtime verification and publishing guidance on current APIs; stricter stress/transaction-failure testing;
  optional root-motion gameplay export; explicit retargeting rather than strict-pivot rejection.

### Modeling/texturing

- Cube duplication/mirror/pivot/group tools, snapping and object-versus-component modes.
- Editable indexed mesh topology, vertex/edge/face selection and transforms; extrusion/inset/subdivide/delete;
  merge/split/normals; mesh-specific undo commands.
- Assimp OBJ/FBX/glTF/GLB import, model export, dedicated RBXM/RBXMX parsing. No model format is supported yet.
- PNG import/export, texture painting/fill/erase/picker/palette/brush/opacity/resizing and stroke undo.
- UV island editing, unwrap/pack/transform/snap/mirror, texture assignment, 3D painting and viewport synchronization.
- Extend project schema with meshes, textures, UVs, materials, events and per-project workspace metadata.

### Distribution/polish

- Inspect both Windows installer and portable package on a clean machine without Visual Studio, including actual GPU rendering.
- Verify deployment of MSVC runtime/Qt plugins/non-Qt DLLs and dependency licenses; signing is not configured.
- Run sanitizers, fuzz corrupted files, large-scene/long-clip stress tests, UI automation and crash recovery tests.
- Harden memory use: history currently copies whole documents; track lookup is linear; rig traversal is recursive.
- Plugin infrastructure/ABI, settings migration, accessibility, localization and a complete Blockbench workflow parity audit.

Next acceptance gate: run both Studio workflows and fix any API/runtime issues, then verify Windows installation
and GPU rendering on a clean machine. Existing tests and the authoring/export path must remain intact.
