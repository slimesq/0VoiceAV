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
        self.folders.build = "build"
        self.folders.generators = "build/generators"

    def configure(self):
        ffmpeg = self.options["ffmpeg"]

        if self.options.ffmpeg_flavor == "light":
            self._configure_light_ffmpeg(ffmpeg)
        else:
            self._configure_full_ffmpeg(ffmpeg)

    def _configure_light_ffmpeg(self, ffmpeg):
        # Keep Conan Center's default ffmpeg/7.1.3 options. This currently has
        # a prebuilt Windows/MSVC package on Conan Center and is the fast path.
        ffmpeg.shared = False

    def _configure_full_ffmpeg(self, ffmpeg):
        # Static FFmpeg libraries, matching --enable-static.
        ffmpeg.shared = False

        # Core FFmpeg libraries.
        ffmpeg.avdevice = True
        ffmpeg.avcodec = True
        ffmpeg.avformat = True
        ffmpeg.avfilter = True
        ffmpeg.swresample = True
        ffmpeg.swscale = True
        ffmpeg.postproc = True
        ffmpeg.with_programs = True

        # Conan Center options that map directly to requested configure flags.
        ffmpeg.with_bzip2 = True
        ffmpeg.with_fontconfig = True
        ffmpeg.with_freetype = True
        ffmpeg.with_fribidi = True
        ffmpeg.with_harfbuzz = True
        ffmpeg.with_libaom = True
        ffmpeg.with_libdav1d = False
        ffmpeg.with_libfdk_aac = False
        ffmpeg.with_libiconv = True
        ffmpeg.with_libmp3lame = True
        ffmpeg.with_libsvtav1 = False
        ffmpeg.with_libvpx = True
        ffmpeg.with_libwebp = True
        ffmpeg.with_libx264 = True
        ffmpeg.with_libx265 = True
        ffmpeg.with_libxml2 = True
        ffmpeg.with_lzma = True
        ffmpeg.with_openh264 = False
        ffmpeg.with_openjpeg = True
        ffmpeg.with_opus = True
        ffmpeg.with_sdl = True
        ffmpeg.with_vorbis = True
        # The Windows/MSVC recipe path has trouble detecting libzmq through
        # pkg-config, even when Conan has resolved the dependency.
        ffmpeg.with_zeromq = self.settings.os != "Windows"
        ffmpeg.with_zlib = True

        # The Conan Center recipe does not expose gnutls. It supports openssl
        # or securetransport; use openssl so HTTPS/TLS protocols are enabled.
        ffmpeg.with_ssl = "openssl"

        # Linux/FreeBSD-only hardware and desktop integrations. Only VAAPI is
        # requested; the rest are set false to avoid recipe defaults adding
        # unrelated desktop/audio integrations.
        if self.settings.os in ("Linux", "FreeBSD"):
            ffmpeg.with_libalsa = False
            ffmpeg.with_libdrm = False
            ffmpeg.with_pulse = False
            ffmpeg.with_vaapi = True
            ffmpeg.with_vdpau = False
            ffmpeg.with_vulkan = False
            ffmpeg.with_xcb = False
            ffmpeg.with_xlib = False

        # Apple-only integrations.
        if self.settings.os == "Macos":
            ffmpeg.with_appkit = False
            ffmpeg.with_avfoundation = False
            ffmpeg.with_audiotoolbox = False
            ffmpeg.with_coreimage = False
            ffmpeg.with_videotoolbox = False
