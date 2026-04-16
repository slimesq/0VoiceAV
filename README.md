# 0VoiceAVLearning

Audio/video learning examples built with CMake, Conan, and FFmpeg.

## Quick Start

### Windows

```powershell
.\pull_dependency.ps1
```

Press Enter at both prompts to use the defaults:

- FFmpeg flavor: `light`
- Build type: `Debug`

Configure and build:

```powershell
cmake --preset voice-av-vs2022-x64
cmake --build --preset voice-av-vs2022-x64-debug
```

Run the basic FFmpeg version example:

```powershell
.\build\cmake\Debug\voice_av_learning.exe
```

### Ubuntu 22 LTS

Install the common build tools:

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build python3 python3-pip git curl tar bzip2 pkg-config nasm yasm
python3 -m pip install --user conan
```

Make sure `~/.local/bin` is in `PATH` if `conan` is not found after the pip
install. The dependency script will install `gcc-13`/`g++-13` when they are
missing, because Conan Center's Linux `ffmpeg/7.1.3` binaries are published for
GCC 13. Use Conan 2.18 or newer for the Linux `full` flavor; it enables SDL,
whose current Conan Center dependency graph needs newer Conan recipe support.

Install or refresh dependencies:

```bash
bash ./pull_dependency.sh
```

Press Enter at both prompts to use the defaults:

- FFmpeg flavor: `light`
- Build type: `Debug`

Configure and build:

```bash
cmake --preset voice-av-linux-debug
cmake --build --preset voice-av-linux-debug
```

Run the basic FFmpeg version example:

```bash
./build/cmake-linux-debug/Debug/voice_av_learning
```

## Dependencies

This project uses `ffmpeg/7.1.3` from Conan Center. Conan Center does not
publish an exact `ffmpeg/7.1` recipe, so the project pins the latest available
7.1 patch release.

Dependency files are generated under `build/<platform>/generators`, and this
project uses a project-local Conan cache under `build/<platform>/conan_home`.
For example, Windows uses `build/windows`, and Linux uses `build/linux`.

Use a specific dependency flavor or build type:

```powershell
.\pull_dependency.ps1 -Flavor light -BuildType Debug
.\pull_dependency.ps1 -Flavor full -BuildType Release
```

```bash
bash ./pull_dependency.sh --flavor light --build-type Debug
bash ./pull_dependency.sh --flavor full --build-type Release
```

The `light` flavor is the fast default and uses Conan Center prebuilt packages
only. On Ubuntu 22 LTS, the script installs GCC 13 if needed so the local Conan
profile matches the available Linux FFmpeg binaries. The `full` flavor enables
more FFmpeg options and may compile FFmpeg locally.

See [FFmpeg Dependency](docs/ffmpeg-dependency.md) for flavor details, Conan
option mapping, and native FFmpeg configure flags that the Conan Center recipe
does not expose.

## Editor Navigation

`pull_dependency.ps1` on Windows and `pull_dependency.sh` on Linux prepare
FFmpeg source navigation for clangd:

- uses the managed FFmpeg source tree at `third_party/ffmpeg-src`
- downloads FFmpeg source to `third_party/ffmpeg-src` if it is missing
- writes the merged clangd database to `build/clangd/compile_commands.json`
- copies a generated root `compile_commands.json` for editor compatibility

If clangd reports `STL1000: Unexpected compiler version`, update LLVM clangd
to match your Visual Studio STL, or keep the provided `.clangd` setting that
suppresses that editor-only version check.

## Build Commands

Configure on Windows:

```powershell
cmake --preset voice-av-vs2022-x64
```

Build Debug or Release on Windows:

```powershell
cmake --build --preset voice-av-vs2022-x64-debug
cmake --build --preset voice-av-vs2022-x64-release
```

Open the generated Visual Studio solution:

```powershell
cmake --open build/cmake
```

Configure on Ubuntu 22 LTS:

```bash
cmake --preset voice-av-linux-debug
cmake --preset voice-av-linux-release
```

Build Debug or Release on Ubuntu 22 LTS:

```bash
cmake --build --preset voice-av-linux-debug
cmake --build --preset voice-av-linux-release
```

## Examples

Examples live under [voice_av](voice_av).

- `voice_av/00-ffmpeg-version`: prints linked FFmpeg library versions.
- `voice_av/08-ffmpeg-demuxing-and-decoding`: demuxing and decoding examples.
- `voice_av/09-ffmpeg-encoding-and-muxing`: prepared for encoding and muxing examples.

Build the current group 08 targets:

```powershell
cmake --build --preset voice-av-vs2022-x64-debug --target voice_av_08_05_decode_audio
cmake --build --preset voice-av-vs2022-x64-debug --target voice_av_08_06_decode_video
```

```bash
cmake --build --preset voice-av-linux-debug --target voice_av_08_05_decode_audio
cmake --build --preset voice-av-linux-debug --target voice_av_08_06_decode_video
```

Run group 08 examples from `voice_av/test_files`:

```powershell
Push-Location .\voice_av\test_files
..\..\build\cmake\Debug\voice_av_08_05_decode_audio.exe believe.aac believe.pcm
..\..\build\cmake\Debug\voice_av_08_06_decode_video.exe source.200kbps.768x320_10s.h264 source.yuv
Pop-Location
```

```bash
pushd ./voice_av/test_files
../../build/cmake-linux-debug/Debug/voice_av_08_05_decode_audio believe.aac believe.pcm
../../build/cmake-linux-debug/Debug/voice_av_08_06_decode_video source.200kbps.768x320_10s.h264 source.yuv
popd
```

See [Voice AV Examples](voice_av/README.md) for the steps to add a new example.

## Documentation

- [docs/README.md](docs/README.md): documentation index.
- [docs/ffmpeg-dependency.md](docs/ffmpeg-dependency.md): FFmpeg dependency notes.
