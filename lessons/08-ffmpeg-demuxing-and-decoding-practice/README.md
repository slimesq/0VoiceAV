# 08 FFmpeg Demuxing and Decoding Practice

These examples were imported from the original Qt projects and converted to
auto-discovered CMake targets.

## Targets

- `lesson_08_05_decode_audio`
- `lesson_08_06_decode_video`

New examples only need to be added as subdirectories here. If a subdirectory
contains `main.c` or `main.cpp`, CMake creates a target named from that folder:

```text
07-my-demo/main.c -> lesson_08_07_my_demo
```

After adding a new example, re-run configure:

```powershell
cmake --preset voice-av-vs2022-x64
```

## Build

```powershell
cmake --build --preset voice-av-vs2022-x64-debug --target lesson_08_05_decode_audio
cmake --build --preset voice-av-vs2022-x64-debug --target lesson_08_06_decode_video
```

## Run

Decode AAC to raw PCM:

```powershell
Push-Location .\lessons\08-ffmpeg-demuxing-and-decoding-practice\testFiles
..\..\..\build\cmake\Debug\lesson_08_05_decode_audio.exe believe.aac believe.pcm
Pop-Location
```

Decode H.264 to raw YUV420P:

```powershell
Push-Location .\lessons\08-ffmpeg-demuxing-and-decoding-practice\testFiles
..\..\..\build\cmake\Debug\lesson_08_06_decode_video.exe source.200kbps.768x320_10s.h264 source.yuv
Pop-Location
```

Input test files for this lesson are kept in `testFiles`.
The lesson 08 executables are written to the normal CMake build output
directory. Visual Studio startup/debugging is configured to use `testFiles` as
the working directory with default arguments.

The original archives also contained Qt project files and a bundled FFmpeg
4.2.1 Windows SDK. Those files are intentionally left out here because this
repository builds with CMake and links against Conan's FFmpeg 7.1.3 package.
