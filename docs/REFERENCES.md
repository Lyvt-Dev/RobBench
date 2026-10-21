# Reference research

Reviewed 2026-10-21. Sources are references; no application source was copied into RobBench.

- Blockbench: https://github.com/JannisX11/blockbench
  Reference HEAD observed: `ec0b2827104d0799c153bfb5f867579c0c44652f`.
  Inspected `js/interface/interface.js` for per-mode layout data, panel slots and timeline header sizing.
  Adopted the familiar outliner/central viewport/inspector/bottom timeline organization through native QDockWidget.
  Blockbench's modeling, graph, paint and UV workflows have not yet received a full parity study.
- Qt QVulkanWindow: https://doc.qt.io/qt-6/qvulkanwindow.html
  Qt 6.8.2 implementation reviewed when diagnosing semaphore reuse validation.
- Roblox Creator Docs, official machine-readable class definitions:
  https://github.com/Roblox/creator-docs/tree/main/content/en-us/reference/engine/classes
  Reviewed Motor6D, Pose, KeyframeSequenceProvider, Plugin, StudioService, File, Players and Humanoid.
  Pose.CFrame applies the animation Motor6D transform while preserving C0/C1; pose parentage follows part joints.
  StudioService:PromptImportFileAsync is the current non-deprecated local file picker.
  File:GetBinaryContents is PluginSecurity. Temporary registered sequence IDs are Studio-local.
  Players:GetUserIdFromNameAsync and Players:CreateHumanoidModelFromUserIdAsync provide an official Studio-side
  avatar resolution/import route. Direct native avatar-mesh API authentication has not been implemented;
  no undocumented website scraping, credential bypass or asset-access bypass is used.
- VMA: https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
- Vulkan-Hpp: https://github.com/KhronosGroup/Vulkan-Hpp

API availability, asset access and plugin capabilities must still be verified in the user's current Studio build.
