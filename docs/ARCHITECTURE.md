# Architecture

`robbench_core` holds Qt-UI-independent data/evaluation/validation. Its file I/O layer uses Qt Core's
cross-platform Unicode paths and atomic QSaveFile; it does not depend on Widgets or a GPU.
`Project` owns parts and clips by value. Parent indices define hierarchy, names identify animation tracks.
Scene evaluation applies the Motor6D equation and detects cycles even when evaluating unsorted hierarchies.

`ui/MainWindow` coordinates a single document, QUndoStack snapshot commands, properties, outliner,
playback timer, workspace visibility, settings, autosave and dialogs. Mutations validate a candidate
project before entering history. Full-document history is correct for this foundation but memory-heavy;
replace it with targeted commands before large models or painting are added.
`ui/Timeline` paints real tracks and keys; scrubbing and drags call document mutations through callbacks.
Evaluation is stateless and deterministic; looping and elapsed-time playback are UI/controller concerns.

`renderer/Viewport` is a QVulkanWindow in QWidget::createWindowContainer. Qt owns instance integration,
physical/logical device selection, queues, surface/swapchain, command pool/buffers, depth resources,
render pass and presentation synchronization. RobBench requests Vulkan 1.3 and independently checks
physical-device support. The renderer creates RAII-lifetime-scoped VMA resources, one persistent cube
vertex buffer, a pipeline layout, SPIR-V modules, graphics pipeline and in-memory pipeline cache.
A shared cube is transformed for every part/grid segment using push constants; no buffers are allocated
per frame. Normals use joint-world orientation. This renderer currently supports block parts and simple lighting.
No instancing, culling, shadows, image descriptors/textures, GPU timestamps or GPU picking are claimed.

The renderer has Qt resource lifecycle release hooks. Handles start null and partial initialization can
be released. Pipeline objects are rebuilt on swapchain recreation. Debug builds enable validation when
installed. Device loss reports an error; users should save and restart. Qt <6.9 receives additional
per-slot completion fences to ensure binary acquire semaphore waits complete before slot reuse;
this fixes a validation failure observed with Qt 6.8.2 and software Vulkan without globally idling each frame.
A headless/native smoke mode tests frames, UI controller operations, picking, playback and resizing.
It is not a substitute for driver/hardware performance testing.

`io/Serialization` separates project persistence from baked Roblox interchange. The Studio Luau plugin
validates exact source/target pivots and constructs part-named Pose hierarchies. Existing characters enter
through Studio's official Players APIs and explicit rig JSON exchange. Desktop HTTP avatar downloads,
custom mesh transfer and automatic retargeting are not implemented.

Future boundaries: modeling mesh topology, materials/textures, UVs, graph curves, IK, physics baking,
asset import jobs and a safe versioned plugin ABI should be separate libraries rather than expanding the UI controller.
