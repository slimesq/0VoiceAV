# Documentation

This directory is the project map for build, dependency, and maintenance notes.
Keep the root README focused on quick start commands, and put longer background
or troubleshooting material here.

## Build And Dependencies

- [FFmpeg Dependency](ffmpeg-dependency.md): FFmpeg flavor selection, Conan
  option mapping, and native FFmpeg configure flags that the Conan Center recipe
  does not expose.
- [Root README](../README.md): quick start commands for Windows and Ubuntu,
  preset names, and common build targets.

## Examples

The root build discovers example directories with a `CMakeLists.txt` in
chapters 08, 09, and 10. Use [05-decode-audio](../voice_av/08-ffmpeg-demuxing-and-decoding/05-decode-audio/CMakeLists.txt)
as a template. The shared helper derives `voice_av_08_05_decode_audio` from the
directory name, collects files in the example directory, `src/`, and `inc/`,
and adds `inc/` to the include path when present.

After setting up dependencies, you can configure and build an individual
example from the repository root on Windows:

```powershell
cmake -S voice_av/08-ffmpeg-demuxing-and-decoding/05-decode-audio -B build/example-08-05 -G "Visual Studio 17 2022" -A x64
cmake --build build/example-08-05 --config Debug
```

- [Test Files](../voice_av/test_files/README.md): local media file names and
  where to place sample inputs and generated outputs.

## 课程讲义

在[讲义索引](lecture_notes/README.md)中按章号查找课程讲义、AI 补充讲义和对应的源码目录。课程配图和课堂抓包随原讲义归入 `0Voice/`。

## Maintenance

- [CMake Helpers](../cmake/VoiceAVHelpers.cmake): helper functions used by
  example `CMakeLists.txt` files.
- [Dependency Scripts](../pull_dependency.ps1) and
  [Linux Dependency Script](../pull_dependency.sh): project-local Conan setup,
  FFmpeg source navigation files, and clangd database generation.
