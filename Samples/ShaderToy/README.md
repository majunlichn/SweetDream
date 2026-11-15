# ShaderToy

A Vulkan runner for Shadertoy Image/Buffer effects.

## Usage

From the repository root:

```powershell
cmake -S . -B build -DSD_BUILD_GUI=ON
cmake --build build --config Debug --target ShaderToy

# Light Cycle: no external resources
.\build\bin\Debug\ShaderToy.exe --width 1280 --height 720

# Voxel Flythrough: download RustyMetal first
python .\build\bin\Debug\ShaderToy\download_resources.py
.\build\bin\Debug\ShaderToy.exe --config .\Samples\ShaderToy\Shaders\VoxelFlythrough\VoxelFlythrough.json
```

Defaults: Light Cycle, 1280x720. `--frames N` limits rendering; Escape exits.
`--gpu-index N` selects GPU N (default 0), with no automatic fallback.
Requires Vulkan 1.3, dynamic rendering, and synchronization2; unsupported features abort.
`--validate` checks JSON and shader compilation, not texture loading or GPU capabilities.
Resizing resets time, frame numbering, and Buffer contents.
`iFrameRate` is a roughly half-second average, initially 60, not `1.0 / iTimeDelta`.

Assets are copied to `ShaderToy` beside the executable. The downloader saves to
its adjacent `Resources/` directory, independent of the working directory.

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
  Mipmaps are generated on demand; Buffer mipmaps refresh after rendering.
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
