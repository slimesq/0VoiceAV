# FFmpeg Dependency

This project uses `ffmpeg/7.1.3` from Conan Center because Conan Center does
not publish an exact `ffmpeg/7.1` recipe.

## Windows Desktop Dependencies

Install Visual Studio 2022 with the Desktop development with C++ workload and
x64 MSVC tools. Make Conan 2 and CMake 3.22 or newer available on `PATH`, and
check them with `conan --version` and `cmake --version`. Run the Windows
dependency script from the repository root to install FFmpeg 7.1.3. Before
configuring the desktop preset, prepare the SDL2 and Qt development packages
used by the course examples.

- Place the SDL2 development SDK in `build/course-deps/SDL2-2.0.10/`, with
  `include/SDL.h` and the x64 libraries under `lib/x64/`. For a different
  location, set the CMake cache variable `VOICE_AV_COURSE_SDL2_ROOT` to the SDK
  root.
- Install Qt 6 Widgets for MSVC x64. Make the Qt 6 `qmake6` or `qmake` available
  on `PATH`, or set `VOICE_AV_COURSE_QT_ROOT` to the Qt SDK root containing
  `lib/cmake/Qt6/Qt6Config.cmake`.

When running SDL2 or Qt examples, include the directory containing
`SDL2.dll` (`lib/x64/` in the SDK) and the Qt SDK's `bin/` directory on
`PATH` as needed. See the [Windows quick start](../README.md#windows) for the
configure and build commands.

## Flavor Selection

The project exposes a local Conan option named `ffmpeg_flavor`:

- `light`: the default. Keeps Conan Center's default `ffmpeg/7.1.3` options and
  uses prebuilt binaries only. On Windows x64 with MSVC, the setup script uses
  [the Light lockfile](../conan/windows-light.lock) to select the tested recipe
  revisions and transitive dependency versions. The Windows binaries were
  verified with MSVC 19.4x and a dynamic Release runtime. On Ubuntu 22 LTS, the
  dependency script installs GCC 13 (`gcc-13` and `g++-13`) when needed because
  Conan Center's Linux x86_64 FFmpeg binaries are published for GCC 13.
- `full`: enables the Conan options that correspond to the requested FFmpeg
  configure flags below. This may require local source builds. On Linux, use
  Conan 2.18 or newer because the SDL dependency graph pulls recipes that need
  newer Conan support.

See the [quick start](../README.md#quick-start) for interactive setup. Pass
the flavor and build type explicitly to skip the prompts.

```powershell
.\pull_dependency.ps1 -Flavor light -BuildType Debug
.\pull_dependency.ps1 -Flavor full -BuildType Release
```

```bash
bash ./pull_dependency.sh --flavor light --build-type Debug
bash ./pull_dependency.sh --flavor full --build-type Release
```

In `light` mode, Debug and Release use the same prebuilt Release Conan
packages. With `VOICE_AV_USE_RELEASE_CONAN_DEPS` enabled, the main build,
standalone examples, and clangd configuration map Debug to Release when only
Release packages are available. On Windows, FFmpeg-based Qt examples select
the matching MSVC runtime and Qt configuration. When Debug packages are
available, the compiler runtime follows the toolchain.

The dependency scripts keep the `light` and `full` CMake metadata in separate
subdirectories of `build/<platform>/generators/` and select the requested flavor
after a successful setup. To switch flavors, rerun the dependency script with
the intended flavor, then configure and build with the same CMake preset.
Previously generated configurations for each flavor can be reused, along with
the packages in `build/<platform>/conan_home`.
On Windows, changing the Light lockfile invalidates the corresponding setup
stamp so the next run verifies the updated dependency graph.
The setup scripts check that cached packages and the files listed in their
Conan manifests are present. If the check fails, setup runs `conan install`
and checks the files again. If the second check still finds missing files,
use the reported path to identify the affected package, remove it from the
project-local Conan cache, and rerun setup with the same flavor and build type.
To force a fresh `conan install`, pass `-ForceConan` in
PowerShell or `--force-conan` in Bash.
Both dependency scripts preserve `CMakeUserPresets.json`. For an older
checkout, rerun the dependency script once to generate the flavor-specific
files using the existing package cache.

## Mapped Configure Flags

- `--enable-static`: `ffmpeg/*:shared=False`
- `--enable-bzlib`: `ffmpeg/*:with_bzip2=True`
- `--enable-fontconfig`: `ffmpeg/*:with_fontconfig=True`
- `--enable-iconv`: `ffmpeg/*:with_libiconv=True`
- `--enable-libxml2`: `ffmpeg/*:with_libxml2=True`
- `--enable-lzma`: `ffmpeg/*:with_lzma=True`
- `--enable-zlib`: `ffmpeg/*:with_zlib=True`
- `--enable-sdl2`: `ffmpeg/*:with_sdl=True`
- `--enable-libwebp`: `ffmpeg/*:with_libwebp=True`
- `--enable-libx264`: `ffmpeg/*:with_libx264=True`
- `--enable-libx265`: `ffmpeg/*:with_libx265=True`
- `--enable-libaom`: `ffmpeg/*:with_libaom=True`
- `--enable-libopenjpeg`: `ffmpeg/*:with_openjpeg=True`
- `--enable-libvpx`: `ffmpeg/*:with_libvpx=True`
- `--enable-libfreetype`: `ffmpeg/*:with_freetype=True`
- `--enable-libfribidi`: `ffmpeg/*:with_fribidi=True`
- `--enable-libharfbuzz`: `ffmpeg/*:with_harfbuzz=True`
- `--enable-libmp3lame`: `ffmpeg/*:with_libmp3lame=True`
- `--enable-libopus`: `ffmpeg/*:with_opus=True`
- `--enable-libvorbis`: `ffmpeg/*:with_vorbis=True`
- `--enable-libzmq`: `ffmpeg/*:with_zeromq=True` on non-Windows platforms.
  It is disabled on Windows because the Conan Center recipe's MSVC path fails
  to detect libzmq through pkg-config.
- `--enable-vaapi`: enabled only on Linux/FreeBSD with `ffmpeg/*:with_vaapi=True`

The recipe automatically enables GPL when GPL dependencies such as x264/x265 or
postproc are enabled. Because the recipe supports OpenSSL instead of GnuTLS,
using TLS together with GPL dependencies may also make the FFmpeg build
`nonfree` in FFmpeg's configure logic.

## Unmapped Configure Flags

These flags are not configurable through the official `ffmpeg` Conan Center
recipe at the moment:

- `--enable-version3`
- `--disable-w32threads`
- `--disable-autodetect`
- `--enable-cairo`
- `--enable-gnutls`
- `--enable-gmp`
- `--enable-libsrt`
- `--enable-libssh`
- `--enable-avisynth`
- `--enable-libxvid`
- `--enable-mediafoundation`
- `--enable-libass`
- `--enable-libvidstab`
- `--enable-libvmaf`
- `--enable-libzimg`
- `--enable-amf`
- `--enable-cuda-llvm`
- `--enable-cuvid`
- `--enable-dxva2`
- `--enable-d3d11va`
- `--enable-d3d12va`
- `--enable-ffnvcodec`
- `--enable-libvpl`
- `--enable-nvdec`
- `--enable-nvenc`
- `--enable-openal`
- `--enable-libgme`
- `--enable-libopenmpt`
- `--enable-libopencore-amrwb`
- `--enable-libtheora`
- `--enable-libvo-amrwbenc`
- `--enable-libgsm`
- `--enable-libopencore-amrnb`
- `--enable-libspeex`
- `--enable-librubberband`

Use a custom Conan recipe if the build must match those native FFmpeg configure
flags exactly.
