# FFmpeg Conan options

This project uses `ffmpeg/7.1.3` from Conan Center because Conan Center does
not publish an exact `ffmpeg/7.1` recipe.

## Flavor selection

The project exposes a local Conan option named `ffmpeg_flavor`:

- `light`: the default. Keeps Conan Center's default `ffmpeg/7.1.3` options and
  is intended to reuse prebuilt binaries.
- `full`: enables the Conan options that correspond to the requested FFmpeg
  configure flags below. This may require local source builds.

Install dependencies interactively:

```powershell
.\pull_dependency.ps1
```

Or skip prompts:

```powershell
.\pull_dependency.ps1 -Flavor light -BuildType Debug
.\pull_dependency.ps1 -Flavor full -BuildType Release
```

For `light + Debug`, the script installs Release Conan packages and the CMake
project maps Debug builds to those packages. This keeps the default path on
prebuilt FFmpeg binaries.

## Requested flags mapped to Conan options

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

## Requested flags not exposed by the Conan Center recipe

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
