# Validation record

Date: 2026-10-21. Debian 13 x64; GCC 14.2; CMake 3.31.6; Qt 6.8.2; Vulkan headers/loader 1.4.309;
VMA 3.2.1; Mesa llvmpipe Vulkan software device (LLVM 19.1.7). No physical GPU or Studio was available.
Development dependencies were extracted to a local prefix because the session user cannot install system packages.

Executed:

- Debug and Release CMake configuration and native application builds.
- GoogleTest core/integration tests: mathematics, topology, Motor6D equation, invalid parents/cycles,
  SLERP/antipodal quaternions, deterministic step boundaries, key insertion/replacement/deletion,
  corrupted format rejection, R6/R15 project save/reopen/export, animation import, scale/root export rejection,
  off-frame/step samples and atomic-save protection.
- Vulkan editor smoke: actual rasterized block rig/grid, inspector edits, undo/redo, interpolated properties,
  save/reopen, baked export, synthetic mouse orbit, ray picking, elapsed-time playback and native-window resize.
- Debug validation layers enabled. An initial Qt 6.8 acquire-semaphore reuse error was reproduced and fixed
  with bounded per-slot completion fences. Clean reruns are required after future renderer changes.
- `luau-compile plugins/roblox-studio/RobBench.plugin.luau`: successful syntax/bytecode compilation.

Final Debug CTest run: **15/15 passed** (1.33 seconds). Final Release CTest run: **15/15 passed** (1.08 seconds).
Both include R6 and R15 native Vulkan editor workflows.
Debug and Release Linux CPack TGZ generation succeeded; the archive is a development artifact, not a verified standalone distribution.
Windows build and installer generation have not been run. The vcpkg Linux dependency-resolution dry run succeeded;
a Windows-target plan could not complete on this Linux host without a Windows compiler, so it is not treated as a successful Windows dependency/build check.
Test source remains authoritative.
Generated captures and interchange examples are under `build/local/smoke-R6` and `build/local/smoke-R15`
when the optional Vulkan tests are enabled. Build products are deliberately gitignored.

Not executed: Roblox Studio plugin installation or runtime, actual avatar API requests, Roblox animation playback,
Windows compilation, Windows clean-machine packages, tagged GitHub release, macOS, hardware performance or VRAM tests.
Luau compilation checks syntax, not availability or runtime behavior of Roblox services.
Do not interpret software-Vulkan smoke success as a 60 FPS or GTX 1060 acceptance result.

## CI follow-up — 2026-10-22

[Run 39689916426](https://github.com/Lyvt-Dev/RobBench/actions/runs/39689916426) built and tested
Windows Debug and Release successfully and produced the Windows ZIP/NSIS artifact. This verifies compilation,
core tests and package generation, not GPU rendering or installation on a clean Windows machine.
The Ubuntu 24.04 job failed at configure because Qt 6.4 uses `FILENAME_VARIABLE` for the deployment script
output; Qt 6.5+ uses `OUTPUT_SCRIPT`. CMake now chooses the appropriate spelling.
