#!/usr/bin/env bash
set -euo pipefail

ffmpeg_version="7.1.3"
ffmpeg_navigation_stamp_version="1"
compiler_cppstd="17"
conan_compiler_version="13"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
conan_home="$repo_root/build/linux/conan_home"
third_party_dir="$repo_root/third_party"
ffmpeg_source_dir="$third_party_dir/ffmpeg-src"
download_dir="$third_party_dir/_downloads"
archive_path="$download_dir/ffmpeg-$ffmpeg_version.tar.bz2"
extract_dir="$third_party_dir/_ffmpeg-extract"
ffmpeg_url="https://ffmpeg.org/releases/ffmpeg-$ffmpeg_version.tar.bz2"
generator_dir="$repo_root/build/linux/generators"
clangd_build_dir="$repo_root/build/clangd"
dependency_state_dir="$repo_root/build/linux/dependency_state"
dependency_stamp_path="$dependency_state_dir/conan-install.stamp"
step_number=0
resolved_ffmpeg_source_dir=""

flavor=""
build_type=""
jobs="$(nproc 2>/dev/null || echo 1)"
force_conan=0
conan_system_package_manager_conf=()

if [[ -t 1 ]]; then
    color_magenta=$'\033[35m'
    color_cyan=$'\033[36m'
    color_green=$'\033[32m'
    color_yellow=$'\033[33m'
    color_gray=$'\033[90m'
    color_reset=$'\033[0m'
else
    color_magenta=""
    color_cyan=""
    color_green=""
    color_yellow=""
    color_gray=""
    color_reset=""
fi

usage() {
    cat <<'EOF'
Usage: ./pull_dependency.sh [options]

Options:
  -f, --flavor light|full       FFmpeg dependency flavor. Default: light
  -b, --build-type Debug|Release
                                Project build type. Default: Debug
  -j, --jobs N                  Parallel build jobs. Default: CPU count
      --force-conan             Refresh Conan dependency graph
  -h, --help                    Show this help
EOF
}

write_section() {
    printf '\n%s== %s ==%s\n' "$color_magenta" "$1" "$color_reset"
}

write_step() {
    step_number=$((step_number + 1))
    printf '\n%s[%d] %s%s\n' "$color_cyan" "$step_number" "$1" "$color_reset"
}

write_info() {
    printf '%s  %s%s\n' "$color_gray" "$1" "$color_reset"
}

write_success() {
    printf '%s  OK  %s%s\n' "$color_green" "$1" "$color_reset"
}

write_notice() {
    printf '%s  NOTE  %s%s\n' "$color_yellow" "$1" "$color_reset"
}

die() {
    printf 'ERROR: %s\n' "$1" >&2
    exit 1
}

require_option_value() {
    local option_name="$1"
    local option_value="${2:-}"
    [[ -n "$option_value" && "$option_value" != -* ]] ||
        die "Missing value for $option_name"
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "Required command not found: $1"
}

run_as_root() {
    if [[ "$(id -u)" -eq 0 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        die "Root permission is required to install missing packages. Install sudo or run this script as root."
    fi
}

apt_package_has_candidate() {
    local package_name="$1"
    apt-cache policy "$package_name" 2>/dev/null |
        awk '/Candidate:/ && $2 != "(none)" { found = 1 } END { exit !found }'
}

ensure_gcc13_toolchain() {
    if command -v gcc-13 >/dev/null 2>&1 && command -v g++-13 >/dev/null 2>&1; then
        write_success "GCC 13 toolchain found."
        return
    fi

    write_step "Installing GCC 13 toolchain for Conan Center Linux binaries"
    write_notice "Conan Center currently publishes ffmpeg/7.1.3 Linux x86_64 binaries for GCC 13."

    command -v apt-get >/dev/null 2>&1 ||
        die "Automatic GCC 13 installation currently supports apt-based Ubuntu systems only."

    if ! apt_package_has_candidate gcc-13 || ! apt_package_has_candidate g++-13; then
        write_info "GCC 13 is not available in the current apt sources. Adding ubuntu-toolchain-r/test PPA..."
        run_as_root apt-get update
        run_as_root apt-get install -y software-properties-common
        run_as_root add-apt-repository -y ppa:ubuntu-toolchain-r/test
    fi

    run_as_root apt-get update
    run_as_root apt-get install -y gcc-13 g++-13

    command -v gcc-13 >/dev/null 2>&1 && command -v g++-13 >/dev/null 2>&1 ||
        die "GCC 13 installation finished, but gcc-13/g++-13 were not found in PATH."

    write_success "GCC 13 toolchain installed."
}

sha256_file() {
    local path="$1"
    if [[ ! -f "$path" ]]; then
        printf '<missing>'
        return
    fi

    sha256sum "$path" | awk '{print $1}'
}

assert_under_repo() {
    local path="$1"
    local full_path
    local full_root
    full_path="$(realpath -m "$path")"
    full_root="$(realpath -m "$repo_root")"

    case "$full_path" in
        "$full_root"|"$full_root"/*) ;;
        *) die "Refusing to modify path outside repository: $full_path" ;;
    esac
}

remove_repo_item() {
    local path="$1"
    assert_under_repo "$path"
    if [[ -e "$path" ]]; then
        rm -rf -- "$path"
    fi
}

is_ffmpeg_source_root() {
    local path="$1"
    [[ -f "$path/libavformat/avformat.c" &&
       -f "$path/libavcodec/avcodec.c" &&
       -f "$path/libavutil/avutil.h" ]]
}

find_ffmpeg_source() {
    if [[ ! -d "$third_party_dir" ]]; then
        return 1
    fi

    if is_ffmpeg_source_root "$ffmpeg_source_dir"; then
        printf '%s\n' "$ffmpeg_source_dir"
        return 0
    fi

    while IFS= read -r candidate; do
        case "$candidate" in
            "$download_dir"|"$download_dir"/*|"$extract_dir"|"$extract_dir"/*)
                continue
                ;;
        esac

        if is_ffmpeg_source_root "$candidate"; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done < <(find "$third_party_dir" -type d 2>/dev/null | sort)

    return 1
}

ffmpeg_navigation_stamp() {
    local source_dir="$1"
    printf 'stamp_version=%s\nffmpeg_version=%s\nsource_dir=%s\n' \
        "$ffmpeg_navigation_stamp_version" \
        "$ffmpeg_version" \
        "$source_dir"
}

ffmpeg_navigation_fresh() {
    local source_dir="$1"
    local expected_stamp="$2"
    local stamp_path="$source_dir/.voice-av-navigation.stamp"
    local required_files=(
        "$source_dir/compile_flags.txt"
        "$source_dir/config.h"
        "$source_dir/config_components.h"
        "$source_dir/libavutil/ffversion.h"
        "$source_dir/libavutil/avconfig.h"
        "$source_dir/compile_commands.json"
    )

    for required_file in "${required_files[@]}"; do
        [[ -f "$required_file" ]] || return 1
    done

    [[ -f "$stamp_path" ]] || return 1
    [[ "$(cat "$stamp_path")" == "$expected_stamp" ]]
}

ensure_ffmpeg_navigation_files() {
    local source_dir="$1"
    local navigation_stamp
    navigation_stamp="$(ffmpeg_navigation_stamp "$source_dir")"

    if ffmpeg_navigation_fresh "$source_dir" "$navigation_stamp"; then
        write_success "FFmpeg navigation files are already up to date."
        return 1
    fi

    cat >"$source_dir/compile_flags.txt" <<'EOF'
-I.
-Ilibavcodec
-Ilibavdevice
-Ilibavfilter
-Ilibavformat
-Ilibavutil
-Ilibswresample
-Ilibswscale
-Ilibpostproc
-DHAVE_AV_CONFIG_H
-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH
EOF

    cat >"$source_dir/config.h" <<'EOF'
#ifndef FFMPEG_NAVIGATION_CONFIG_H
#define FFMPEG_NAVIGATION_CONFIG_H

/* Navigation-only config for clangd. FFmpeg's real config.h is generated by
 * configure; this file is not used by the project build. */
#define FFMPEG_CONFIGURATION "navigation-only"
#define FFMPEG_LICENSE "LGPL version 2.1 or later"

#define ARCH_X86 1
#define ARCH_X86_32 0
#define ARCH_X86_64 1
#define HAVE_BIGENDIAN 0
#define HAVE_FAST_UNALIGNED 1
#define HAVE_THREADS 1
#define HAVE_W32THREADS 0
#define HAVE_PTHREADS 1
#define HAVE_OS2THREADS 0
#define HAVE_ATOMICS_NATIVE 1
#define HAVE_SYNC_VAL_COMPARE_AND_SWAP 0
#define HAVE_INTRINSICS_NEON 0
#define HAVE_MMX 1
#define HAVE_MMXEXT 1
#define HAVE_SSE 1
#define HAVE_SSE2 1
#define HAVE_SSE3 1
#define HAVE_SSSE3 1
#define HAVE_SSE4 1
#define HAVE_SSE42 1
#define HAVE_AVX 1
#define HAVE_AVX2 1
#define HAVE_AVX512 0
#define HAVE_INLINE_ASM 0
#define HAVE_X86ASM 0

#define CONFIG_SMALL 0
#define CONFIG_RUNTIME_CPUDETECT 1
#define CONFIG_GRAY 0
#define CONFIG_FRAME_THREAD_ENCODER 1
#define CONFIG_GPL 1
#define CONFIG_VERSION3 1
#define CONFIG_AVCODEC 1
#define CONFIG_AVDEVICE 1
#define CONFIG_AVFILTER 1
#define CONFIG_AVFORMAT 1
#define CONFIG_AVUTIL 1
#define CONFIG_POSTPROC 1
#define CONFIG_SWRESAMPLE 1
#define CONFIG_SWSCALE 1

#endif
EOF

    cat >"$source_dir/config_components.h" <<'EOF'
#ifndef FFMPEG_NAVIGATION_CONFIG_COMPONENTS_H
#define FFMPEG_NAVIGATION_CONFIG_COMPONENTS_H

/* Navigation-only component config for clangd. Missing FFmpeg component
 * macros intentionally evaluate to 0 in preprocessor conditionals. */

#endif
EOF

    cat >"$source_dir/libavutil/ffversion.h" <<EOF
#ifndef AVUTIL_FFVERSION_H
#define AVUTIL_FFVERSION_H
#define FFMPEG_VERSION "$ffmpeg_version"
#endif
EOF

    cat >"$source_dir/libavutil/avconfig.h" <<'EOF'
#ifndef AVUTIL_AVCONFIG_H
#define AVUTIL_AVCONFIG_H
#define AV_HAVE_BIGENDIAN 0
#define AV_HAVE_FAST_UNALIGNED 1
#endif
EOF

    SOURCE_DIR="$source_dir" python3 - <<'PY'
import json
import os
from pathlib import Path

source_dir = Path(os.environ["SOURCE_DIR"]).resolve()
source_subdirs = [
    "libavcodec",
    "libavdevice",
    "libavfilter",
    "libavformat",
    "libavutil",
    "libpostproc",
    "libswresample",
    "libswscale",
]

commands = []
for source_subdir in source_subdirs:
    full_source_subdir = source_dir / source_subdir
    if not full_source_subdir.exists():
        continue

    for source_file in sorted(full_source_subdir.rglob("*.c")):
        relative_source = source_file.relative_to(source_dir).as_posix()
        commands.append(
            {
                "directory": str(source_dir),
                "command": (
                    "clang -x c -std=c11 -fsyntax-only -DHAVE_AV_CONFIG_H "
                    "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH "
                    "-I. -Icompat -Ilibavcodec -Ilibavdevice -Ilibavfilter "
                    "-Ilibavformat -Ilibavutil -Ilibpostproc -Ilibswresample "
                    f"-Ilibswscale {relative_source}"
                ),
                "file": str(source_file),
            }
        )

(source_dir / "compile_commands.json").write_text(
    json.dumps(commands, indent=4),
    encoding="ascii",
)
PY

    printf '%s' "$navigation_stamp" >"$source_dir/.voice-av-navigation.stamp"
    return 0
}

get_ffmpeg_package_include_dir() {
    if [[ -d "$generator_dir" ]]; then
        local data_file
        while IFS= read -r data_file; do
            local package_folder
            package_folder="$(
                sed -n 's/.*set(ffmpeg_PACKAGE_FOLDER_[A-Z0-9_]* "\(.*\)").*/\1/p' "$data_file" |
                    head -n 1
            )"
            if [[ -n "$package_folder" ]]; then
                package_folder="${package_folder//\$\{CMAKE_CURRENT_LIST_DIR\}/$generator_dir}"
                if [[ -d "$package_folder/include" ]]; then
                    realpath "$package_folder/include"
                    return 0
                fi
            fi
        done < <(find "$generator_dir" -maxdepth 1 -type f -name 'ffmpeg-*-data.cmake' | sort)
    fi

    if [[ -d "$conan_home" ]]; then
        local include_dir
        include_dir="$(
            find "$conan_home" -type f -path '*/libavcodec/avcodec.h' -print -quit 2>/dev/null |
                xargs -r dirname |
                xargs -r dirname
        )"
        if [[ -n "$include_dir" && -d "$include_dir" ]]; then
            realpath "$include_dir"
            return 0
        fi
    fi

    return 1
}

sync_clangd_compilation_database() {
    local ffmpeg_source="$1"
    local ffmpeg_include_dir=""
    local ffmpeg_include_arg=""

    if ffmpeg_include_dir="$(get_ffmpeg_package_include_dir)"; then
        ffmpeg_include_arg="-isystem\"${ffmpeg_include_dir}\" "
    fi

    REPO_ROOT="$repo_root" \
    FFMPEG_SOURCE_DIR="$ffmpeg_source" \
    FFMPEG_INCLUDE_ARG="$ffmpeg_include_arg" \
    CLANGD_BUILD_DIR="$clangd_build_dir" \
    python3 - <<'PY'
import json
import os
from pathlib import Path

repo_root = Path(os.environ["REPO_ROOT"]).resolve()
ffmpeg_source_dir = Path(os.environ["FFMPEG_SOURCE_DIR"]).resolve()
ffmpeg_include_arg = os.environ["FFMPEG_INCLUDE_ARG"]
clangd_build_dir = Path(os.environ["CLANGD_BUILD_DIR"]).resolve()

project_commands = []
example_root = repo_root / "voice_av"
if example_root.exists():
    source_files = sorted(
        [
            *example_root.rglob("*.c"),
            *example_root.rglob("*.cpp"),
        ]
    )
else:
    source_files = []

relative_ffmpeg_source = ffmpeg_source_dir.relative_to(repo_root).as_posix()
for source_file in source_files:
    relative_source = source_file.relative_to(repo_root).as_posix()
    is_cxx = source_file.suffix in {".cc", ".cpp", ".cxx"}
    compiler = "clang++" if is_cxx else "clang"
    language = "c++" if is_cxx else "c"
    standard = "c++17" if is_cxx else "c11"
    project_commands.append(
        {
            "directory": str(repo_root),
            "command": (
                f"{compiler} -x {language} -std={standard} -fsyntax-only "
                "-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH "
                f"-I{relative_ffmpeg_source} {ffmpeg_include_arg}{relative_source}"
            ),
            "file": str(source_file),
        }
    )

ffmpeg_commands_path = ffmpeg_source_dir / "compile_commands.json"
if ffmpeg_commands_path.exists():
    ffmpeg_commands = json.loads(ffmpeg_commands_path.read_text(encoding="ascii"))
else:
    ffmpeg_commands = []

merged_commands = project_commands + ffmpeg_commands
if not merged_commands:
    raise SystemExit(0)

clangd_build_dir.mkdir(parents=True, exist_ok=True)
payload = json.dumps(merged_commands, indent=4)
(clangd_build_dir / "compile_commands.json").write_text(payload, encoding="ascii")
(repo_root / "compile_commands.json").write_text(payload, encoding="ascii")
PY

    write_success "clangd compile database ready: $clangd_build_dir"
}

conan_install_stamp() {
    local selected_flavor="$1"
    local selected_build_type="$2"
    local conan_build_type="$3"
    local conan_compiler_version="$4"
    local build_policy="$5"
    local conanfile_path="$repo_root/conanfile.py"
    local profile_path="$conan_home/profiles/default"

    cat <<EOF
ffmpeg_version=$ffmpeg_version
flavor=$selected_flavor
selected_build_type=$selected_build_type
conan_build_type=$conan_build_type
conan_compiler_version=$conan_compiler_version
compiler_cppstd=$compiler_cppstd
build_policy=$build_policy
conanfile_sha256=$(sha256_file "$conanfile_path")
profile_sha256=$(sha256_file "$profile_path")
EOF
}

conan_install_fresh() {
    local expected_stamp="$1"

    if [[ "$force_conan" -eq 1 ]]; then
        return 1
    fi

    [[ -f "$generator_dir/conan_toolchain.cmake" ]] || return 1
    [[ -f "$generator_dir/ffmpeg-config.cmake" ]] || return 1
    [[ -f "$dependency_stamp_path" ]] || return 1
    [[ "$(cat "$dependency_stamp_path")" == "$expected_stamp" ]]
}

download_file() {
    local url="$1"
    local output="$2"

    if command -v curl >/dev/null 2>&1; then
        curl -L "$url" -o "$output"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$output" "$url"
    else
        die "curl or wget is required to download FFmpeg source."
    fi
}

sync_ffmpeg_source() {
    write_step "Checking FFmpeg source for code navigation"

    local existing_source=""
    if existing_source="$(find_ffmpeg_source)"; then
        if ensure_ffmpeg_navigation_files "$existing_source"; then
            write_success "FFmpeg source found: $existing_source"
            write_info "Navigation files refreshed under the source tree."
        else
            write_success "FFmpeg source found: $existing_source"
        fi
        resolved_ffmpeg_source_dir="$existing_source"
        return 0
    fi

    write_notice "FFmpeg source was not found under third_party."
    write_step "Downloading FFmpeg source $ffmpeg_version for code navigation"
    mkdir -p "$download_dir" "$third_party_dir"

    if [[ ! -f "$archive_path" ]]; then
        write_info "Downloading: $ffmpeg_url"
        download_file "$ffmpeg_url" "$archive_path"
    else
        write_info "Using existing archive: $archive_path"
    fi

    remove_repo_item "$extract_dir"
    mkdir -p "$extract_dir"

    write_info "Extracting archive..."
    tar -xjf "$archive_path" -C "$extract_dir"

    local extracted_source="$extract_dir/ffmpeg-$ffmpeg_version"
    [[ -d "$extracted_source" ]] ||
        die "FFmpeg archive did not extract to expected folder: $extracted_source"

    remove_repo_item "$ffmpeg_source_dir"
    mv "$extracted_source" "$ffmpeg_source_dir"
    remove_repo_item "$extract_dir"

    ensure_ffmpeg_navigation_files "$ffmpeg_source_dir" >/dev/null
    write_success "FFmpeg source ready: $ffmpeg_source_dir"
    resolved_ffmpeg_source_dir="$ffmpeg_source_dir"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -f|--flavor)
            require_option_value "$1" "${2:-}"
            flavor="${2:-}"
            shift 2
            ;;
        -b|--build-type)
            require_option_value "$1" "${2:-}"
            build_type="${2:-}"
            shift 2
            ;;
        -j|--jobs)
            require_option_value "$1" "${2:-}"
            jobs="${2:-}"
            shift 2
            ;;
        --force-conan)
            force_conan=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            die "Unknown argument: $1"
            ;;
    esac
done

require_command conan
require_command bzip2
require_command python3
require_command realpath
require_command sha256sum
require_command tar

write_section "Voice AV dependency setup"
write_info "Repository: $repo_root"
write_info "Project Conan cache: $conan_home"

if [[ -z "$flavor" ]]; then
    if [[ -t 0 ]]; then
        write_step "Choose FFmpeg dependency flavor"
        write_info "light: fast path, uses Conan Center defaults and prebuilt packages when available."
        write_info "full : enables more FFmpeg options and may compile FFmpeg locally."
        read -r -p "FFmpeg flavor [light/full] (default: light): " flavor
        flavor="${flavor:-light}"
    else
        flavor="light"
        write_notice "No interactive input detected. Using default FFmpeg flavor: $flavor"
    fi
fi

flavor="$(printf '%s' "$flavor" | tr '[:upper:]' '[:lower:]')"
[[ "$flavor" == "light" || "$flavor" == "full" ]] ||
    die "Invalid FFmpeg flavor '$flavor'. Use 'light' or 'full'."

if [[ -z "$build_type" ]]; then
    if [[ -t 0 ]]; then
        write_step "Choose build type"
        write_info "Debug  : default for learning and local debugging."
        write_info "Release: optimized build type for dependency resolution."
        read -r -p "Build type [Debug/Release] (default: Debug): " build_type
        build_type="${build_type:-Debug}"
    else
        build_type="Debug"
        write_notice "No interactive input detected. Using default build type: $build_type"
    fi
fi

case "$(printf '%s' "$build_type" | tr '[:upper:]' '[:lower:]')" in
    debug) build_type="Debug" ;;
    release) build_type="Release" ;;
    *) die "Invalid build type '$build_type'. Use 'Debug' or 'Release'." ;;
esac

[[ "$jobs" =~ ^[0-9]+$ && "$jobs" -ge 1 ]] ||
    die "Invalid jobs value '$jobs'. Use a positive integer."

conan_build_type="$build_type"
build_policy="missing"

if [[ "$flavor" == "light" && "$build_type" == "Debug" ]]; then
    conan_build_type="Release"
    write_notice "Debug selected. Using Release Conan packages for light FFmpeg when binaries are available."
fi

if [[ "$flavor" == "light" ]]; then
    build_policy="never"
    write_notice "Using Conan Center prebuilt packages only for light FFmpeg."
fi

ensure_gcc13_toolchain

conan_system_package_manager_conf=(
    -c "tools.system.package_manager:mode=install"
)
if [[ "$(id -u)" -ne 0 ]]; then
    conan_system_package_manager_conf+=(
        -c "tools.system.package_manager:sudo=True"
    )
fi

write_section "Selected configuration"
write_info "FFmpeg flavor: $flavor"
write_info "Requested build type: $build_type"
write_info "Conan build_type: $conan_build_type"
write_info "Conan compiler.version: $conan_compiler_version"
write_info "Conan build policy: $build_policy"
write_info "C++ standard: $compiler_cppstd"
write_info "Parallel jobs: $jobs"

sync_ffmpeg_source

write_step "Preparing project-local Conan cache"
export CONAN_HOME="$conan_home"
mkdir -p "$conan_home"
write_info "CONAN_HOME=$CONAN_HOME"

default_profile="$conan_home/profiles/default"
if [[ ! -f "$default_profile" ]]; then
    write_info "Default Conan profile not found. Detecting profile..."
    conan profile detect --force
    write_success "Conan profile detected."
else
    write_success "Conan profile found: $default_profile"
fi

expected_stamp="$(
    conan_install_stamp \
        "$flavor" \
        "$build_type" \
        "$conan_build_type" \
        "$conan_compiler_version" \
        "$build_policy"
)"

write_step "Checking Conan dependencies"

if conan_install_fresh "$expected_stamp"; then
    write_success "Conan dependencies are already installed for this configuration."
    write_notice "Skipping conan install. Use --force-conan to refresh dependency graph and generated CMake files."
else
    write_info "Installing Conan dependencies: flavor=$flavor, selected_build_type=$build_type, conan_build_type=$conan_build_type, compiler.version=$conan_compiler_version, build=$build_policy"
    write_info "Adding or updating conancenter remote in project-local Conan cache..."
    conan remote add conancenter https://center2.conan.io --force

    write_step "Running conan install"
    DEBIAN_FRONTEND=noninteractive conan install . \
        -r=conancenter \
        "--build=$build_policy" \
        -c "tools.build:jobs=$jobs" \
        "${conan_system_package_manager_conf[@]}" \
        -s "build_type=$conan_build_type" \
        -s "compiler=gcc" \
        -s "compiler.version=$conan_compiler_version" \
        -s "compiler.cppstd=$compiler_cppstd" \
        -o "&:ffmpeg_flavor=$flavor"

    mkdir -p "$dependency_state_dir"
    printf '%s' "$expected_stamp" >"$dependency_stamp_path"
    write_success "Conan install finished."
fi

remove_repo_item "$repo_root/CMakeUserPresets.json"

write_step "Generating clangd compile database"
sync_clangd_compilation_database "$resolved_ffmpeg_source_dir"
write_success "Dependency setup complete."
