# Test Files

Place local media inputs and generated outputs for the examples in this
directory. Media files are ignored by git.

Expected sample names used by the root README:

- `believe.aac`: input for `voice_av_08_05_decode_audio`
- `believe.pcm`: generated PCM output
- `source.200kbps.768x320_10s.h264`: input for `voice_av_08_06_decode_video`
- `source.yuv`: generated YUV output

Run examples from this directory when using relative paths, or pass absolute
paths from any working directory.

Chapter 10 inputs are kept in `10-ffmpeg-filtering/` to avoid overwriting media
used by other chapters:

- `768x320.yuv`: YUV420P input shared by examples 02 and 05 (768x320, 25 fps).
- `48000_2_f32le.pcm`, `48000_2_s16le.pcm`: packed stereo 48 kHz inputs for example 03.
- `main.jpg`, `logo.jpg`: inputs for the JPEG overlay example 04.
- `time1.flv`, `logo.jpg`: inputs for the reference video-logo example.

These media files were extracted locally from the original course archives and
remain ignored by git. A new checkout needs its own copies of the inputs.
See [Chapter 10](../10-ffmpeg-filtering/README.md) for commands and output formats.
