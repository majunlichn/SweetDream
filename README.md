# SweetDream

A C++20 application development framework, exposed as the CMake target
`SweetDream::SweetDream`.

## Goals

- Provide reusable building blocks for desktop applications and interactive tools.
- Integrate application lifecycle, windows, GPU resources, shaders, and user interfaces.
- Keep APIs clear and resource ownership explicit.

## Style

- C++20 APIs in the `sd` namespace, with PascalCase types and functions.
- RAII and reference-counted ownership for managed resources.
- Build on radcpp, Vulkan, SDL3, Dear ImGui, and ImPlot rather than duplicate their functionality.

## Features

- **Core**: shared strings and logging utilities, independent of GPU and GUI code.
- **GPU/Vulkan**: devices, memory allocation, resources, commands, pipelines, synchronization,
  mipmap generation, and image readback with PNG and Radiance HDR export.
- **Shader**: runtime GLSL compilation; SPIR-V validation, assembly, and disassembly.
- **Scene**: editable node hierarchies and transforms, shared point, line, and polygon meshes,
  PBR materials, textures, cameras, and lights.
- **Platform**: SDL3 application lifecycle, event dispatch, and frame processing.
- **Gui**: windows, Vulkan presentation, and Dear ImGui/ImPlot integration.

Modules live under [Source/SweetDream](Source/SweetDream) and belong to one static library.
The public include root is `Source`, e.g. `<SweetDream/GPU/Vulkan/VulkanDevice.h>`.

## Build

- Requires CMake 3.25+, a C++20 compiler, and the dependencies in [vcpkg.json](vcpkg.json).
- Install the Vulkan SDK, including `glslc`, shaderc, and SPIRV-Tools; set `VULKAN_SDK`.
- Set `VCPKG_ROOT` to use its toolchain automatically; the first configure installs dependencies.
- GUI support requires SDL3 3.4.14 or newer and the Dear ImGui/ImPlot submodules.

Command examples use Bash syntax.

From the repository root, with a **multi-config** generator
(Visual Studio or Ninja Multi-Config):

```bash
git submodule update --init --recursive
cmake -S . -B build
cmake --build build --config Debug
```

For a **single-config** generator (Ninja or Unix Makefiles), configure with
`-DCMAKE_BUILD_TYPE=Debug` and omit `--config Debug` when building.
Use the same configuration throughout.

- `SD_BUILD_GUI` defaults to `ON`; missing GUI dependencies disable it with a warning.
  Set `-DSD_BUILD_GUI=OFF` to build without Platform and Gui.
- `SD_BUILD_SAMPLES` defaults to `ON`; samples require GUI support.
- `SD_BUILD_TESTS` defaults to `OFF`. Set it to `ON` to build `SweetDreamUnitTests`.
  After a Debug build, run `ctest --test-dir build -C Debug --output-on-failure`.

### SDL3

Use an existing SDL3 CMake package or build the pinned release locally before configuring:

```bash
python ThirdParty/SDL/build.py --config Debug
```

The script requires Python 3 and Git. It keeps the checkout, build tree, and installation
under `ThirdParty/SDL/source`, `ThirdParty/SDL/build`, and `ThirdParty/SDL/installed`.
Use `--clean` to rebuild from a clean build/install tree.

Set `SDL3_DIR` to the directory containing `SDL3Config.cmake` to select an installation.
Otherwise, CMake searches the local install first, then system/package-manager locations.

## Samples

- [HelloTriangle](Samples/HelloTriangle): Vulkan triangle with Dear ImGui/ImPlot.
- [ShaderToy](Samples/ShaderToy/README.md): Shadertoy effects with JSON-configured passes,
  textures, and feedback.

**Run samples and tests from the runtime output directory, not the repository root**, to keep
logs, temporary files, and other test outputs out of the source tree:

- **Multi-config**: `build/bin/<Config>/`, matching the built configuration.
- **Single-config**: `build/bin/`.

For a multi-config Debug build:

```bash
cd build/bin/Debug
./HelloTriangle
./ShaderToy --frames 60
```
