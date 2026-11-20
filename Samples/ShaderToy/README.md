# ShaderToy

A Vulkan runner for Shadertoy Image/Buffer effects.

## Usage

Command examples use Bash syntax.

Build from the repository root (multi-config Debug example):

```bash
cmake -S . -B build -DSD_BUILD_GUI=ON
cmake --build build --config Debug --target ShaderToy
```

Run from the **runtime output directory**, not the repository root, to keep logs,
temporary files, and other test outputs out of the source tree. For a single-config
build, use `build/bin` instead.

```bash
cd build/bin/Debug
# Light Cycle: no external resources
./ShaderToy --width 1280 --height 720

# Voxel Flythrough: download RustyMetal first
python ShaderToy/download_resources.py
./ShaderToy --config ShaderToy/Shaders/VoxelFlythrough/VoxelFlythrough.json

# Mipmap test: inspect every level of a 133x73 BufferA
./ShaderToy --config ShaderToy/Shaders/MipmapTest/MipmapTest-133x73.json
```

Defaults: Light Cycle, 1280x720. `--frames N` limits rendering; Escape exits.
`--gpu-index N` selects GPU N (default 0), with no automatic fallback.
Requires Vulkan 1.3, dynamic rendering, and synchronization2; unsupported features abort.
`--validate` checks JSON and shader compilation, not texture loading or GPU capabilities.
Resizing resets time, frame numbering, and Buffer contents.
`iFrameRate` is a roughly half-second average, initially 60, not `1.0 / iTimeDelta`.

Assets are copied to `ShaderToy` beside the executable. The downloader saves to
its adjacent `Resources/` directory, independent of the working directory.

### Mipmap test

[MipmapTest-BufferA.glsl](./Shaders/MipmapTest/MipmapTest-BufferA.glsl) draws fine
and coarse colored checks over a gradient in BufferA. Use one of these configs,
or change `BufferA.size` to try another size.

| Config | Mip levels | What it exercises |
| --- | ---: | --- |
| [MipmapTest-133x73.json](./Shaders/MipmapTest/MipmapTest-133x73.json) | 8 | Odd, non-power-of-two dimensions on both axes |
| [MipmapTest-256x128.json](./Shaders/MipmapTest/MipmapTest-256x128.json) | 9 | Even, power-of-two dimensions; height reaches 1 before width |
| [MipmapTest-255x257.json](./Shaders/MipmapTest/MipmapTest-255x257.json) | 9 | Dimensions just below and above 256; width reaches 1 before height |
| [MipmapTest-257x1.json](./Shaders/MipmapTest/MipmapTest-257x1.json) | 9 | Single row; only width shrinks |
| [MipmapTest-1x115.json](./Shaders/MipmapTest/MipmapTest-1x115.json) | 7 | Single column; only height shrinks |
| [MipmapTest-1x1.json](./Shaders/MipmapTest/MipmapTest-1x1.json) | 1 | Only the base level; nothing to downsample |

[MipmapTest.glsl](./Shaders/MipmapTest/MipmapTest.glsl) sizes its grid to fit
the expected mip chain, with up to four tiles per row.
Fine checks should disappear before coarse ones, and the last tile should be a
single gray texel. Each tile has a green border if its size
and center sample match the expected values, including a frame-dependent alpha
check for stale data; red marks a failure.

## Effect JSON

Each effect lives under `Shaders/<effect>/`. Shader and optional `common` paths
are relative to the JSON; texture paths are relative to the runtime `ShaderToy`
asset directory. Absolute paths are accepted.

```json
{
  "passes": [
    {
      "name": "Image",
      "shader": "VoxelFlythrough.glsl",
      "inputs": [
        { "name": "Resources\\RustyMetal.jpg",
          "filter": "mipmap", "wrap": "repeat", "vflip": true }
      ]
    }
  ]
}
```

- Array order is execution order. The last pass is Image; preceding passes are
  Buffers. Names must be unique and do not determine pass type.
  Buffers use RGBA32F and preserve alpha; final Image alpha is 1.
- Input `name` matching a Buffer pass samples its output; otherwise it is a
  texture path. Array index selects `iChannelN`; `null` leaves that slot unbound.
  Each pass accepts at most four slots (`iChannel0..3`), including null slots.
  The final Image cannot be sampled; the former `channel` field is not accepted.
  For example, `"inputs": [null, {"name": "BufferA"}]` binds BufferA to `iChannel1`.
- Earlier Buffer inputs read the current frame; self/later inputs read the
  previous frame. Buffers start cleared; missing channels sample zero.
- `filter`: `nearest`, `linear` (default), or `mipmap` (trilinear).
  File mipmaps are generated during upload; Buffer mipmaps refresh after rendering
  into each ping-pong target. Both use `VulkanMipmap` and require a compute-capable
  graphics queue and storage image support for the respective format. Aligned
  images can generate up to six levels per dispatch on devices with full
  32-lane compute subgroups and shuffle support; otherwise they use the
  general path.
- `wrap`: `clamp` (default) or `repeat`.
- `vflip`: file textures only; false by default. True flips decoded rows before upload.
- Optional Buffer `size: [width, height]` overrides screen resolution.
  Optional top-level `common` names shared GLSL code.

## Compatibility

Vulkan only, adapting Shadertoy's WebGL/OpenGL shader conventions.
Provide `mainImage(out vec4, in vec2)` without `#version`, uniforms, or `main`.

- **Entry and bindings:** [ShaderToy.frag](ShaderToy.frag) supplies GLSL 460,
  `main()`, a std140 uniform buffer, and explicit sampler `set/binding`.
- **Coordinates:** With this sample's positive-height viewport, framebuffer Y
  grows downward. Buffer row 0 stores the logical bottom; Image flips coordinate
  Y and corrects Y derivatives to preserve Shadertoy's upward Y.
  Adapted `gl_FragCoord` follows the same convention; Buffer UVs need no flip.
- **Texture orientation:** `vflip` reverses image rows before upload, like WebGL's
  `UNPACK_FLIP_Y_WEBGL`. It is independent of the Image coordinate flip.
- **Color:** File textures use RGBA8 UNORM without automatic sRGB decoding.
  UNORM attachments store shader RGB directly; sRGB attachments first decode it
  to cancel hardware encoding and preserve the original output values.

Supports one Image, up to four Buffers, and four 2D input channels per pass.
Unbound slots sample zero.
Cubemaps, 3D textures, keyboard/audio/video inputs, Sound/VR passes, and the
Shadertoy website API/JSON are unsupported.

## Verified effects

- [Light Cycle by Noztol](https://www.shadertoy.com/view/7XVSWG)
- [Voxel Flythrough](https://www.shadertoy.com/view/MdGXWG)
- [Rainforest](https://www.shadertoy.com/view/4ttSWf)
