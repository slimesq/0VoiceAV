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

- [Voice AV Examples](../voice_av/README.md): example group layout, target naming
  rules, and steps for adding a new example.
- [Test Files](../voice_av/test_files/README.md): local media file names and
  where to place sample inputs and generated outputs.
- [Chapter 10 Filter Lectures](lecture_notes/10-ffmpeg-filtering/README.md): original
  PDFs and diagrams, with a source-to-example map and run instructions.
- [Chapter 43 Supplement](lecture_notes/43-ffmpeg-hevc-hwaccel-and-mixing/README.md):
  HEVC bitstreams, software encoding, NVENC, hardware decoding, and audio mixing
  lectures that build on chapters 08, 09, and 10.

## Maintenance

- [CMake Helpers](../cmake/VoiceAVHelpers.cmake): helper functions used by
  example `CMakeLists.txt` files.
- [Dependency Scripts](../pull_dependency.ps1) and
  [Linux Dependency Script](../pull_dependency.sh): project-local Conan setup,
  FFmpeg source navigation files, and clangd database generation.
