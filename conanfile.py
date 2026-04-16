from conan import ConanFile


class VoiceAVLearningConan(ConanFile):
    name = "voice_av_learning"
    version = "0.1.0"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "ffmpeg_flavor": ["light", "full"],
    }
    default_options = {
        "ffmpeg_flavor": "light",
    }
    generators = "CMakeDeps", "CMakeToolchain", "VirtualRunEnv"

    def requirements(self):
        # Conan Center does not publish an exact ffmpeg/7.1 recipe. 7.1.3 is
        # the latest available patch release in the 7.1 series.
        self.requires("ffmpeg/7.1.3")

    def layout(self):
        platform_name = str(self.settings.os).lower()
        self.folders.build = f"build/{platform_name}"
        self.folders.generators = f"build/{platform_name}/generators"

    def configure(self):
        ffmpeg = self.options["ffmpeg"]
        flavor = str(self.options.ffmpeg_flavor)

        if flavor == "light":
            self._configure_light_ffmpeg(ffmpeg)
        else:
            self._configure_full_ffmpeg(ffmpeg)

    def _configure_light_ffmpeg(self, ffmpeg):
        # Keep Conan Center's default ffmpeg/7.1.3 options. The dependency
        # scripts choose a compiler profile that matches available binaries.
        ffmpeg.shared = False

    def _configure_full_ffmpeg(self, ffmpeg):
        self._configure_light_ffmpeg(ffmpeg)

        os_name = str(self.settings.os)
        if os_name != "Windows":
            ffmpeg.fPIC = True

        # Full is light plus extra capabilities. Do not set feature options to
        # False here, because that can remove capabilities from Conan Center's
        # default ffmpeg package configuration.

        # Core FFmpeg libraries.
        ffmpeg.avdevice = True
        ffmpeg.avcodec = True
        ffmpeg.avformat = True
        ffmpeg.avfilter = True
        ffmpeg.swresample = True
        ffmpeg.swscale = True
        ffmpeg.postproc = True
        ffmpeg.with_programs = True
        ffmpeg.with_asm = True

        # Conan Center options that map directly to requested configure flags.
        ffmpeg.with_bzip2 = True
        ffmpeg.with_fontconfig = True
        ffmpeg.with_freetype = True
        ffmpeg.with_fribidi = True
        ffmpeg.with_harfbuzz = True
        ffmpeg.with_libaom = True
        ffmpeg.with_libdav1d = True
        ffmpeg.with_libfdk_aac = True
        ffmpeg.with_libiconv = True
        ffmpeg.with_libmp3lame = True
        ffmpeg.with_libsvtav1 = True
        ffmpeg.with_libvpx = True
        ffmpeg.with_libwebp = True
        ffmpeg.with_libx264 = True
        ffmpeg.with_libx265 = True
        ffmpeg.with_libxml2 = True
        ffmpeg.with_lzma = True
        ffmpeg.with_openh264 = True
        ffmpeg.with_openjpeg = True
        ffmpeg.with_opus = True
        ffmpeg.with_sdl = True
        ffmpeg.with_vorbis = True
        # The Windows/MSVC recipe path has trouble detecting libzmq through
        # pkg-config, even when Conan has resolved the dependency.
        if os_name != "Windows":
            ffmpeg.with_zeromq = True
        ffmpeg.with_zlib = True

        # The Conan Center recipe does not expose gnutls. It supports openssl
        # or securetransport; use openssl so HTTPS/TLS protocols are enabled.
        ffmpeg.with_ssl = "openssl"
