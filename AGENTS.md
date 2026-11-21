# Agent guide

SweetDream is a C++20 application development framework, exposed as `SweetDream::SweetDream`.
See [README.md](README.md) for project goals, features, and build requirements.

## Source layout

- `Source/SweetDream/`: framework code, grouped by module.
  - `Core/`: shared utilities; keep independent of GPU, platform, and GUI code.
  - `GPU/Vulkan/`: Vulkan resource and execution wrappers.
  - `Scene/`: CPU scene data and hierarchy; keep public types independent of import and GPU APIs.
  - `Shader/`: GLSL compilation and SPIR-V tools.
  - `Platform/`: application lifecycle and event dispatch.
  - `Gui/`: windows and UI integration, connecting platform and GPU code.
  - Keep public `Foo.h` and optional `Foo.cpp` together.
  - The public include root is `Source`, e.g. `<SweetDream/GPU/Vulkan/VulkanDevice.h>`.
- `Samples/`: applications demonstrating framework features; keep sample-specific logic here.
- `Tests/`: focused framework tests, enabled by `SD_BUILD_TESTS`.
- `cmake/`: build support, including SDL3 discovery.
- `ThirdParty/`: submodules and dependency build helpers. Declare other dependencies in
  [vcpkg.json](vcpkg.json); avoid unrelated changes to vendored code.
- Register files explicitly in the corresponding source lists or targets in
  [CMakeLists.txt](CMakeLists.txt) and sample CMake files. Do not use source globbing.
- Keep Platform/Gui sources and dependencies conditional on `SD_BUILD_GUI`, and samples
  conditional on both `SD_BUILD_GUI` and `SD_BUILD_SAMPLES`.

## C++ style

- Follow adjacent code: Allman braces, four-space indentation, and existing include order.
- Use the `sd` namespace, PascalCase types/functions, `m_` members, `#pragma once`,
  `[[nodiscard]]` where appropriate, and closing namespace/class comments.
- Preserve RAII and explicit ownership; follow existing `rad::Ref`/`rad::RefCounted` patterns
  for shared framework resources.
- Reuse radcpp utilities and existing Vulkan/shader helpers before adding new abstractions.
- Preserve error propagation and diagnostics; do not silently ignore failures.
- Keep compiler flags and platform defines in CMake rather than per-file workarounds.

## Build and validation

- Follow the [README build instructions](README.md#build).
- Build affected targets and run focused validation for behavior changes. Use CTest with
  `SD_BUILD_TESTS=ON` for SweetDream tests; bundled radcpp tests remain disabled.
- Use ShaderToy's `--validate` for JSON and shader compilation checks. It does not validate
  texture loading or GPU capabilities; use `--frames N` for bounded rendering smoke tests.
- **When running samples, test executables, or validation commands, use the
  runtime output directory as the working directory, not the repository root, to keep logs,
  temporary files, and other test outputs out of the source tree.**
  - **Multi-config**: **`build/bin/<Config>/`**, matching the built configuration.
  - **Single-config**: **`build/bin/`**.
  - For a custom build tree, use its corresponding runtime output directory.
- Use the copied runtime assets beside the executable; do not write generated outputs into
  `Source/` or `Samples/`.
