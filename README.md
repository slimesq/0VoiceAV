# 0VoiceAVLearning

Audio/video learning examples built with CMake, Conan, and FFmpeg.

## Quick Start

### Windows

Prepare Conan, CMake, and the desktop SDKs using the
[Windows desktop build guide](docs/ffmpeg-dependency.md#windows-desktop-dependencies).

```powershell
.\pull_dependency.ps1
```

Press Enter at both prompts to use the defaults:

- FFmpeg flavor: `light`
- Build type: `Debug`

Configure and build the main and course examples in `build/cmake`.

```powershell
cmake --preset voice-av-windows-all
cmake --build --preset voice-av-windows-all-debug
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
install. The [dependency guide](docs/ffmpeg-dependency.md#flavor-selection)
explains GCC and Conan version requirements for each flavor.

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

This project pins `ffmpeg/7.1.3` from Conan Center. See
[FFmpeg Dependency](docs/ffmpeg-dependency.md) for optional setup commands,
flavor selection, the Conan cache, and FFmpeg configure flag mapping.

## Editor Navigation

`pull_dependency.ps1` on Windows and `pull_dependency.sh` on Linux prepare
FFmpeg source navigation for clangd:

- uses the managed FFmpeg source tree at `third_party/ffmpeg-src`
- downloads FFmpeg source to `third_party/ffmpeg-src` if it is missing
- writes the merged clangd database to `build/clangd/compile_commands.json`
- copies a generated root `compile_commands.json` for editor compatibility

On Windows, refresh the database after adding source files or changing target
include directories or compile options:

```powershell
.\update_clangd.ps1 -BuildType Debug
```

This initializes the Visual Studio 2022 x64 compiler environment and configures
a separate Ninja build in `build/clangd-cmake` with
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`. It indexes the same targets as the Windows
desktop preset, generates the Qt UI headers, and merges the target compile
commands with FFmpeg navigation entries. It reads any SDK path
overrides from `build/cmake/CMakeCache.txt` when available. Ninja must be on PATH
or available through Visual Studio's CMake tools.

The Windows dependency script attempts this refresh automatically. If editor
configuration is incomplete, set up the
[desktop SDKs](docs/ffmpeg-dependency.md#windows-desktop-dependencies) and rerun
`update_clangd.ps1`. The dependency installation and any existing index remain
available in the meantime.

On Ubuntu, `pull_dependency.sh` configures the Ninja preset for the selected
build type and merges its CMake compilation database with the FFmpeg source
entries. Run the dependency script again after changing target compile options
or adding sources. If indexing fails, the installed dependencies and existing
index remain available.

After refreshing, run `clangd: Restart language server` in VS Code.

If clangd reports `STL1000: Unexpected compiler version`, update LLVM clangd
to match your Visual Studio STL, or keep the provided `.clangd` setting that
suppresses that editor-only version check.

## 代码格式

各机器统一使用 LLVM **19.1.7** 的 clangd。可以把对应的 `bin` 目录放在
`PATH` 前面，或在 VS Code 的用户设置中把 `clangd.path` 指向本机的
clangd 可执行文件。使用 `PATH` 时可以运行 `clangd --version` 核对版本。

安装工作区推荐的 VS Code 扩展后，clangd 负责 C/C++ 保存时的自动格式化。

根目录 `.clang-format` 统一规定四空格缩进、100 列、左侧指针符号和大括号换行。
源码使用 UTF-8 和 LF。
clangd 使用内置的格式化引擎，各机器使用同一版本可获得一致的格式化结果。

## Additional Build Commands

The Quick Start covers Debug builds. Build Release on Windows:

```powershell
cmake --build --preset voice-av-windows-all-release
```

Open the generated Visual Studio solution:

```powershell
cmake --open build/cmake
```

Configure and build Release on Ubuntu 22 LTS:

```bash
cmake --preset voice-av-linux-release
cmake --build --preset voice-av-linux-release
```

## Examples

Examples live under [voice_av](voice_av).

- [0Voice 课程讲义索引](docs/lecture_notes/README.md) maps the numbered course chapters
  to the imported source trees, including RTSP, WebRTC, and Android NDK.
- `voice_av/00-ffmpeg-version`: prints linked FFmpeg library versions.
- `voice_av/08-ffmpeg-demuxing-and-decoding`: demuxing and decoding examples.
- `voice_av/09-ffmpeg-encoding-and-muxing`: encoding, muxing, transcoding, image extraction,
  watermarking, and RTMP publishing examples.
- [voice_av/10-ffmpeg-filtering](voice_av/10-ffmpeg-filtering/README.md): filter graphs,
  audio mixing, crop/flip, JPEG watermarking, and video logo examples.

Build the current group 08 targets:

```powershell
cmake --build --preset voice-av-windows-all-debug --target voice_av_08_05_decode_audio
cmake --build --preset voice-av-windows-all-debug --target voice_av_08_06_decode_video
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

See [Adding an example](docs/README.md#examples) for the directory layout and
target naming rules.

## Documentation

- [docs/README.md](docs/README.md): documentation index.
- [Chapter 10 lecture notes](docs/lecture_notes/10-ffmpeg-filtering/README.md): original
  filter lectures and diagrams, with links to the migrated examples.
