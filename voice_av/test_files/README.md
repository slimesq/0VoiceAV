# Test Files

Place local media inputs and generated outputs for the examples in this
directory. Media files are ignored by git.

Course fixtures are grouped by chapter. Keep the relative paths inside each
chapter, including the `concat/`, `filter/`, and `img2video/` folders in
`03-05-ffmpeg-cli/`. For quick runs, this directory also has one top-level copy
per fixture filename. An existing top-level file takes precedence. For new
copies, use chapter order, then path depth and alphabetical path within the
chapter.

For a lesson with its own version of a sample, run from its chapter directory
or pass the chapter path explicitly. For example, the root `believe.mp4` is
the Chapter 02 sample, and `08-ffmpeg-demuxing-and-decoding/believe.mp4` is the
Chapter 08 sample.

Expected sample names used by the root README:

- `believe.aac`: input for `voice_av_08_05_decode_audio`
- `believe.pcm`: generated PCM output
- `source.200kbps.768x320_10s.h264`: input for `voice_av_08_06_decode_video`
- `source.yuv`: generated YUV output

Run examples from this directory when using relative paths, or pass absolute
paths from any working directory.

Use the copies in `10-ffmpeg-filtering/` for the [Chapter 10 input formats and
run commands](../10-ffmpeg-filtering/README.md#输入素材与运行).

These media files were extracted locally from the original course archives.
A new checkout needs its own copies of the inputs.

## Publisher Test Inputs

The RTMP and RTSP publishers read `buweishui_48000_2_s16le.pcm` and
`720x480_25fps_420p.yuv` from the program's working directory. Run these commands
from `15-rtmp-streaming/` or `19-21-rtsp-streaming/` to prepare missing inputs.
Reuse the PCM file when it already exists.

```powershell
ffmpeg -i buweishui.mp3 -ar 48000 -ac 2 -f s16le buweishui_48000_2_s16le.pcm
ffmpeg -i 720x480_25fps.mp4 -an -c:v rawvideo -pix_fmt yuv420p 720x480_25fps_420p.yuv
```
