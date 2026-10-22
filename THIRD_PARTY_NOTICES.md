# Third-party notices

RobBench's original code is MIT licensed. No Blockbench code, assets or branding are bundled.
Blockbench is a GPL-3.0 project used as a UX reference only. Roblox and Blockbench names belong to their owners;
RobBench is independent and is not endorsed by either project/company.

Build dependencies retain their licenses. This source tree does not vendor their implementations.
Dependency copyright files from a vcpkg installation are included by packaging where available.
Distributors must audit the final binary package and include all transitive notices; packaging is not yet validated.

| Dependency | Upstream | License |
|---|---|---|
| Qt 6 | https://www.qt.io/licensing/ | LGPL-3.0 or GPL/commercial, depending on component; this project uses LGPL-eligible Core/Gui/Widgets/Network |
| Vulkan loader/headers | https://github.com/KhronosGroup/Vulkan-Loader | Apache-2.0 / MIT for relevant headers; retain installed notices |
| Vulkan-Hpp | https://github.com/KhronosGroup/Vulkan-Hpp | Apache-2.0 |
| Vulkan Memory Allocator | https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator | MIT |
| GLM | https://github.com/g-truc/glm | MIT or Happy Bunny License |
| nlohmann/json | https://github.com/nlohmann/json | MIT |
| spdlog | https://github.com/gabime/spdlog | MIT |
| fmt (spdlog dependency) | https://github.com/fmtlib/fmt | MIT |
| GoogleTest (tests) | https://github.com/google/googletest | BSD-3-Clause |
| glslang / SPIRV-Tools (build tools) | https://github.com/KhronosGroup/glslang | BSD-style / Apache-2.0; retain supplied notices |
| Assimp | https://github.com/assimp/assimp | BSD-3-Clause and bundled component notices |
| tinygltf (provisioned; not integrated yet) | https://github.com/syoyo/tinygltf | MIT |
| stb (provisioned; not integrated yet) | https://github.com/nothings/stb | MIT or public-domain alternative |

Qt is dynamically linked in the Windows vcpkg configuration. Users must be allowed to replace compatible Qt
libraries; do not lock or prohibit modified libraries. Preserve the LGPL/GPL texts and Qt's third-party notices.
Matching Qt source is available from https://download.qt.io/archive/qt/ and should be identified by the exact
version in each released package. Review LGPL distribution obligations before releasing a verified binary.
Vulkan validation layers and compiler tools are development dependencies, not required application runtimes.
