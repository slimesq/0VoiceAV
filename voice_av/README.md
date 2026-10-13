# Voice AV Examples

This directory contains course examples grouped by topic.

Each topic directory can auto-discover child projects that contain a
`CMakeLists.txt`. For example:

```text
voice_av/
  08-ffmpeg-demuxing-and-decoding/
    05-decode-audio/
      CMakeLists.txt
      main.c
```

## Current Groups

- `00-ffmpeg-version`: prints linked FFmpeg library versions.
- `08-ffmpeg-demuxing-and-decoding`: demuxing and decoding examples.
- `09-ffmpeg-encoding-and-muxing`: encoding, muxing, transcoding, image extraction,
  watermarking, and RTMP publishing examples.

## Add an Example

1. Create a project directory under the matching topic directory.

```text
voice_av/
  09-ffmpeg-encoding-and-muxing/
    01-encode-audio/
      CMakeLists.txt
      include/
        encoder.h
      src/
        main.c
        encoder.c
```

2. Use the standard example layout.

```text
01-encode-audio/
  CMakeLists.txt
  include/
    encoder.h
  src/
    main.c
    encoder.c
```

Examples with local headers follow `08-ffmpeg-demuxing-and-decoding/10-2-audio-resample`:
keep `.h` and `.hpp` files in `include/`, and implementation files plus the entry
point in `src/`. A single-file example without local headers can keep `main.c` or
`main.cpp` beside `CMakeLists.txt`. A per-example README is optional and can be
added later when the run steps are not obvious.

3. Put the project's files in the matching directories.

The project can contain multiple `.c`, `.cpp`, `.cc`, `.cxx`, `.h`, `.hpp`,
`.hh`, and `.hxx` files.

4. Add the target to the project `CMakeLists.txt`:

```cmake
voice_av_add_current_example_executable()
```

For examples with an `include/` directory, also add its include path:

```cmake
voice_av_add_current_example_executable()
voice_av_get_current_example_target_name(targetName)
target_include_directories(${targetName} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
```

The helper collects source files recursively, including those in `src/`.

The target name is calculated from the directory names:

```text
voice_av/09-ffmpeg-encoding-and-muxing/01-encode-audio
```

becomes:

```text
voice_av_09_01_encode_audio
```

5. Re-run CMake configure after adding the directory:

```powershell
cmake --preset voice-av-vs2022-x64
```

6. Build the new target:

```powershell
cmake --build --preset voice-av-vs2022-x64-debug --target voice_av_09_01_encode_audio
```

## Test Files

Put local media files in `voice_av/test_files` when an example needs input or
output files. Its media contents are ignored by git, while the directory is kept
with `.gitkeep` and documented in `test_files/README.md`.

For examples that use relative input paths, run the executable from the matching
`voice_av/test_files` directory, or pass absolute paths.
