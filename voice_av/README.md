# Voice AV Examples

Course examples live under this directory. Each example group can have its own
subdirectory, and groups such as `08-ffmpeg-demuxing-and-decoding`
and `09-ffmpeg-encoding-and-muxing` automatically include child projects that
contain a `CMakeLists.txt`.

## Add a New Project

1. Create a new project directory under the matching example group.

```text
voice_av/
  09-ffmpeg-encoding-and-muxing/
    01-encode-audio/
      CMakeLists.txt
      main.c
      encoder.c
      encoder.h
```

2. Put the project's source files in that directory.

The project can contain multiple `.c`, `.cpp`, `.h`, and `.hpp` files.

3. Add a `CMakeLists.txt` in the project directory.

The target name is calculated from the directory names. For example:

```text
voice_av/09-ffmpeg-encoding-and-muxing/01-encode-audio
```

becomes:

```text
lesson_09_01_encode_audio
```

The project `CMakeLists.txt` only needs one line:

```cmake
voice_av_add_current_example_executable()
```

4. Re-run CMake configure after adding the new directory.

```powershell
cmake --preset voice-av-vs2022-x64
```

5. Build the new target.

```powershell
cmake --build --preset voice-av-vs2022-x64-debug --target lesson_09_01_encode_audio
```

## Test Files

Put local media files in a `testFiles` directory when a lesson needs input or
output files. `testFiles` is ignored by git.
