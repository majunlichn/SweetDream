# SweetDream

## SDL3

SDL3 support is opt-in. The project can use SDL 3.4.14 built locally or an
SDL3 CMake package supplied by the system, vcpkg, or another package manager.

Configure CMake generator with environment variables if necessary:

```powershell
$env:CMAKE_GENERATOR="Visual Studio 18 2026"
$env:CMAKE_GENERATOR_PLATFORM="x64"
```

To clone, build, and install the pinned release:

```sh
python ThirdParty/SDL/build.py
cmake -S . -B build -DSD_BUILD_GUI=ON
```

The script uses this untracked layout:

- `ThirdParty/SDL/source` - SDL Git checkout at `release-3.4.14`
- `ThirdParty/SDL/build` - CMake build tree
- `ThirdParty/SDL/installed` - local install prefix containing the SDL3 package

Pass `--clean` to rebuild from a clean build/install tree, `--config Debug` for
a debug build, or `--linkage static` for a static-only install.

If the local install is absent, the same configure command searches for an
SDL3 package on the system. Set `SDL3_DIR` to the directory containing
`SDL3Config.cmake` to select a specific installation. After discovery,
consumers can link the imported target `SDL3::SDL3`.
