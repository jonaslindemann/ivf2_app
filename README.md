# ivf2 standalone application

This repository is a standalone example application for the `ivf2` framework. It
expects the `ivf2` source tree and prebuilt libraries to be available next to
this directory:

```text
parent/
  ivf2/
  ivf2_app/
```

The default CMake presets set `IVF2_ROOT` to `../ivf2`. The app adds that source
tree as an include/library dependency and links against a local `ivf2::ivf2`
interface target. Release builds use `build-release`, and debug builds use
`build-debug`. vcpkg runs in manifest mode using `vcpkg.json`, with both build
directories sharing `vcpkg_installed`. The prebuilt ivf2 libraries must exist in:

```text
ivf2/lib/Release
ivf2/lib/Debug
```

## Configure and build on Windows

```bash
cmake --preset windows
cmake --build --preset release
```

For a debug build:

```bash
cmake --preset windows-debug
cmake --build --preset debug
```

You can also use the helper script:

```bash
build_windows.cmd [debug|release] [path\to\ivf2]
```

If no ivf2 path is specified, the script uses `..\ivf2`.

## Configure and build on Linux

```bash
cmake --preset linux
cmake --build --preset linux-release
```

For a debug build:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
```

## Configure and build on macOS

```bash
cmake --preset macos
cmake --build --preset macos-release
```

For a debug build:

```bash
cmake --preset macos-debug
cmake --build --preset macos-debug
```
