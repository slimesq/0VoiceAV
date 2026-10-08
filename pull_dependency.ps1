param(
    [ValidateSet("light", "full")]
    [string]$Flavor,

    [ValidateSet("Debug", "Release")]
    [string]$BuildType,

    [ValidateRange(1, 256)]
    [int]$Jobs = [Environment]::ProcessorCount,

    [switch]$ForceConan
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ffmpegVersion = "7.1.3"
$ffmpegNavigationStampVersion = "2"
$compilerCppStd = "17"
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$conanHome = Join-Path $repoRoot "build/windows/conan_home"
$thirdPartyDir = Join-Path $repoRoot "third_party"
$ffmpegSourceDir = Join-Path $thirdPartyDir "ffmpeg-src"
$downloadDir = Join-Path $thirdPartyDir "_downloads"
$archivePath = Join-Path $downloadDir "ffmpeg-$ffmpegVersion.tar.bz2"
$archiveTempPath = "$archivePath.tmp"
$extractDir = Join-Path $thirdPartyDir "_ffmpeg-extract"
$ffmpegUrl = "https://ffmpeg.org/releases/ffmpeg-$ffmpegVersion.tar.bz2"
$generatorDir = Join-Path $repoRoot "build/windows/generators"
$dependencyStateDir = Join-Path $repoRoot "build/windows/dependency_state"
$dependencyStampPath = Join-Path $dependencyStateDir "conan-install.stamp"
$script:StepNumber = 0

function Write-Section {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host ""
    Write-Host "== $Message ==" -ForegroundColor Magenta
}

function Write-Step {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    $script:StepNumber++
    Write-Host ""
    Write-Host ("[{0}] {1}" -f $script:StepNumber, $Message) -ForegroundColor Cyan
}

function Write-Info {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host "  $Message" -ForegroundColor DarkGray
}

function Write-Success {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host "  OK  $Message" -ForegroundColor Green
}

function Write-Notice {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host "  NOTE  $Message" -ForegroundColor Yellow
}

function Read-ColoredPrompt {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    Write-Host -NoNewline "$Message " -ForegroundColor Cyan
    return Read-Host
}

function Test-InteractiveInput {
    try {
        return -not [Console]::IsInputRedirected
    } catch {
        return $false
    }
}

function Assert-UnderRepo {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $fullRoot = [System.IO.Path]::GetFullPath($repoRoot)
    $fullRootWithSeparator = $fullRoot
    if (-not $fullRootWithSeparator.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $fullRootWithSeparator += [System.IO.Path]::DirectorySeparatorChar
    }

    if (
        -not $fullPath.Equals($fullRoot, [System.StringComparison]::OrdinalIgnoreCase) -and
        -not $fullPath.StartsWith($fullRootWithSeparator, [System.StringComparison]::OrdinalIgnoreCase)
    ) {
        throw "Refusing to modify path outside repository: $fullPath"
    }
}

function Remove-RepoItem {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    Assert-UnderRepo -Path $Path
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Get-RelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $baseFullPath = [System.IO.Path]::GetFullPath($BasePath)
    if (-not $baseFullPath.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $baseFullPath += [System.IO.Path]::DirectorySeparatorChar
    }

    $pathFullPath = [System.IO.Path]::GetFullPath($Path)
    $baseUri = New-Object System.Uri($baseFullPath)
    $pathUri = New-Object System.Uri($pathFullPath)

    return [System.Uri]::UnescapeDataString(
        $baseUri.MakeRelativeUri($pathUri).ToString()
    ).Replace("/", [System.IO.Path]::DirectorySeparatorChar)
}

function Test-FFmpegSourceRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return (
        (Test-Path -LiteralPath (Join-Path $Path "libavformat/avformat.c")) -and
        (Test-Path -LiteralPath (Join-Path $Path "libavcodec/avcodec.c")) -and
        (Test-Path -LiteralPath (Join-Path $Path "libavutil/avutil.h"))
    )
}

function Find-FFmpegSource {
    if (Test-FFmpegSourceRoot -Path $ffmpegSourceDir) {
        return $ffmpegSourceDir
    }

    return $null
}

function Get-FFmpegNavigationStamp {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir
    )

    return @(
        "stamp_version=$ffmpegNavigationStampVersion"
        "ffmpeg_version=$ffmpegVersion"
        "source_dir=$SourceDir"
    ) -join "`n"
}

function Test-FFmpegNavigationFresh {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,

        [Parameter(Mandatory = $true)]
        [string]$ExpectedStamp
    )

    $requiredFiles = @(
        (Join-Path $SourceDir "compile_flags.txt"),
        (Join-Path $SourceDir "config.h"),
        (Join-Path $SourceDir "config_components.h"),
        (Join-Path $SourceDir "libavutil/ffversion.h"),
        (Join-Path $SourceDir "libavutil/avconfig.h"),
        (Join-Path $SourceDir "compile_commands.json")
    )

    foreach ($requiredFile in $requiredFiles) {
        if (-not (Test-Path -LiteralPath $requiredFile)) {
            return $false
        }
    }

    $stampPath = Join-Path $SourceDir ".voice-av-navigation.stamp"
    if (-not (Test-Path -LiteralPath $stampPath)) {
        return $false
    }

    $currentStamp = Get-Content -LiteralPath $stampPath -Raw -Encoding UTF8
    return ($currentStamp.Trim() -eq $ExpectedStamp.Trim())
}

function Ensure-FFmpegNavigationFiles {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir
    )

    $navigationStamp = Get-FFmpegNavigationStamp -SourceDir $SourceDir
    if (Test-FFmpegNavigationFresh -SourceDir $SourceDir -ExpectedStamp $navigationStamp) {
        Write-Success "FFmpeg navigation files are already up to date."
        return $false
    }

    @"
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
"@ | Set-Content -LiteralPath (Join-Path $SourceDir "compile_flags.txt") -Encoding ASCII

    @"
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
#define HAVE_PTHREADS 0
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
"@ | Set-Content -LiteralPath (Join-Path $SourceDir "config.h") -Encoding ASCII

    @"
#ifndef FFMPEG_NAVIGATION_CONFIG_COMPONENTS_H
#define FFMPEG_NAVIGATION_CONFIG_COMPONENTS_H

/* Navigation-only component config for clangd. Missing FFmpeg component
 * macros intentionally evaluate to 0 in preprocessor conditionals. */

#endif
"@ | Set-Content -LiteralPath (Join-Path $SourceDir "config_components.h") -Encoding ASCII

    $ffversionPath = Join-Path $SourceDir "libavutil/ffversion.h"
    @"
#ifndef AVUTIL_FFVERSION_H
#define AVUTIL_FFVERSION_H
#define FFMPEG_VERSION "$ffmpegVersion"
#endif
"@ | Set-Content -LiteralPath $ffversionPath -Encoding ASCII

    $avconfigPath = Join-Path $SourceDir "libavutil/avconfig.h"
    @"
#ifndef AVUTIL_AVCONFIG_H
#define AVUTIL_AVCONFIG_H
#define AV_HAVE_BIGENDIAN 0
#define AV_HAVE_FAST_UNALIGNED 1
#endif
"@ | Set-Content -LiteralPath $avconfigPath -Encoding ASCII

    $sourceSubdirs = @(
        "libavcodec",
        "libavdevice",
        "libavfilter",
        "libavformat",
        "libavutil",
        "libpostproc",
        "libswresample",
        "libswscale"
    )

    $compileCommands = [System.Collections.Generic.List[object]]::new()
    foreach ($sourceSubdir in $sourceSubdirs) {
        $fullSourceSubdir = Join-Path $SourceDir $sourceSubdir
        if (-not (Test-Path -LiteralPath $fullSourceSubdir)) {
            continue
        }

        $sourceFiles = Get-ChildItem -LiteralPath $fullSourceSubdir -Recurse -Filter "*.c" -File -ErrorAction SilentlyContinue |
            Sort-Object FullName

        foreach ($sourceFile in $sourceFiles) {
            $relativeSource = (Get-RelativePath -BasePath $SourceDir -Path $sourceFile.FullName).Replace("\", "/")
            $compileCommands.Add([PSCustomObject]@{
                directory = $SourceDir
                command = "clang -x c -std=c11 -fsyntax-only -DHAVE_AV_CONFIG_H -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -I. -Icompat -Ilibavcodec -Ilibavdevice -Ilibavfilter -Ilibavformat -Ilibavutil -Ilibpostproc -Ilibswresample -Ilibswscale $relativeSource"
                file = $sourceFile.FullName
            }) | Out-Null
        }
    }

    $compileCommandsJson = ConvertTo-Json -InputObject $compileCommands.ToArray() -Depth 3
    [System.IO.File]::WriteAllText((Join-Path $SourceDir "compile_commands.json"), $compileCommandsJson, $utf8NoBom)
    [System.IO.File]::WriteAllText((Join-Path $SourceDir ".voice-av-navigation.stamp"), $navigationStamp, $utf8NoBom)
    return $true
}

function Get-FileSha256 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return "<missing>"
    }

    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Get-ConanInstallStamp {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Flavor,

        [Parameter(Mandatory = $true)]
        [string]$ConanBuildType,

        [Parameter(Mandatory = $true)]
        [string]$BuildPolicy
    )

    $conanfilePath = Join-Path $repoRoot "conanfile.py"
    $profilePath = Join-Path $conanHome "profiles/default"

    return @(
        "stamp_version=2"
        "ffmpeg_version=$ffmpegVersion"
        "flavor=$Flavor"
        "conan_build_type=$ConanBuildType"
        "compiler_cppstd=$compilerCppStd"
        "build_policy=$BuildPolicy"
        "conanfile_sha256=$(Get-FileSha256 -Path $conanfilePath)"
        "profile_sha256=$(Get-FileSha256 -Path $profilePath)"
    ) -join "`n"
}

function Test-ConanInstallFresh {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExpectedStamp
    )

    if ($ForceConan) {
        return $false
    }

    $requiredGeneratorFiles = @(
        (Join-Path $generatorDir "conan_toolchain.cmake"),
        (Join-Path $generatorDir "ffmpeg-config.cmake")
    )

    foreach ($requiredFile in $requiredGeneratorFiles) {
        if (-not (Test-Path -LiteralPath $requiredFile)) {
            return $false
        }
    }

    if (-not (Test-Path -LiteralPath $dependencyStampPath)) {
        return $false
    }

    $currentStamp = Get-Content -LiteralPath $dependencyStampPath -Raw -Encoding UTF8
    return ($currentStamp.Trim() -eq $ExpectedStamp.Trim())
}

function Sync-FFmpegSource {
    Write-Step "Checking FFmpeg source for code navigation"

    $existingSource = Find-FFmpegSource
    if (-not [string]::IsNullOrWhiteSpace($existingSource)) {
        $navigationRefreshed = Ensure-FFmpegNavigationFiles -SourceDir $existingSource
        Write-Success "FFmpeg source found: $existingSource"
        if ($navigationRefreshed) {
            Write-Info "Navigation files refreshed under the source tree."
        }
        return
    }

    Write-Notice "Managed FFmpeg source was not found at $ffmpegSourceDir."
    Write-Step "Downloading FFmpeg source $ffmpegVersion for code navigation"
    New-Item -ItemType Directory -Force -Path $downloadDir | Out-Null
    New-Item -ItemType Directory -Force -Path $thirdPartyDir | Out-Null

    if (-not (Test-Path -LiteralPath $archivePath)) {
        Write-Info "Downloading: $ffmpegUrl"
        Remove-RepoItem -Path $archiveTempPath
        try {
            Invoke-WebRequest -Uri $ffmpegUrl -OutFile $archiveTempPath
            Move-Item -LiteralPath $archiveTempPath -Destination $archivePath -Force
        } catch {
            Remove-RepoItem -Path $archiveTempPath
            throw
        }
    } else {
        Write-Info "Using existing archive: $archivePath"
    }

    Remove-RepoItem -Path $extractDir
    New-Item -ItemType Directory -Force -Path $extractDir | Out-Null

    Write-Info "Extracting archive..."
    tar -xjf $archivePath -C $extractDir
    if ($LASTEXITCODE -ne 0) {
        Remove-RepoItem -Path $extractDir
        Remove-RepoItem -Path $archivePath
        throw "Failed to extract FFmpeg archive. Removed the cached archive so the next run can download it again."
    }

    $extractedSource = Join-Path $extractDir "ffmpeg-$ffmpegVersion"
    if (-not (Test-Path -LiteralPath $extractedSource)) {
        Remove-RepoItem -Path $extractDir
        Remove-RepoItem -Path $archivePath
        throw "FFmpeg archive did not extract to expected folder: $extractedSource"
    }

    Remove-RepoItem -Path $ffmpegSourceDir
    Move-Item -LiteralPath $extractedSource -Destination $ffmpegSourceDir
    Remove-RepoItem -Path $extractDir

    Ensure-FFmpegNavigationFiles -SourceDir $ffmpegSourceDir | Out-Null

    Write-Success "FFmpeg source ready: $ffmpegSourceDir"
}

Write-Section "Voice AV dependency setup"
Write-Info "Repository: $repoRoot"
Write-Info "Project Conan cache: $conanHome"

if ([string]::IsNullOrWhiteSpace($Flavor)) {
    if (Test-InteractiveInput) {
        Write-Step "Choose FFmpeg dependency flavor"
        Write-Info "light: fast path, uses Conan Center defaults and prebuilt packages when available."
        Write-Info "full : enables more FFmpeg options and may compile FFmpeg locally."
        $inputFlavor = Read-ColoredPrompt "FFmpeg flavor [light/full] (default: light):"
        $Flavor = if ([string]::IsNullOrWhiteSpace($inputFlavor)) { "light" } else { $inputFlavor.Trim().ToLowerInvariant() }
    } else {
        $Flavor = "light"
        Write-Notice "No interactive input detected. Using default FFmpeg flavor: $Flavor"
    }
}

$Flavor = $Flavor.Trim().ToLowerInvariant()

if ($Flavor -notin @("light", "full")) {
    throw "Invalid FFmpeg flavor '$Flavor'. Use 'light' or 'full'."
}

if ([string]::IsNullOrWhiteSpace($BuildType)) {
    if (Test-InteractiveInput) {
        Write-Step "Choose build type"
        Write-Info "Debug  : default for learning and local debugging."
        Write-Info "Release: optimized build type for dependency resolution."
        $inputBuildType = Read-ColoredPrompt "Build type [Debug/Release] (default: Debug):"
        $BuildType = if ([string]::IsNullOrWhiteSpace($inputBuildType)) { "Debug" } else { $inputBuildType.Trim() }
    } else {
        $BuildType = "Debug"
        Write-Notice "No interactive input detected. Using default build type: $BuildType"
    }
}

$BuildType = switch ($BuildType.ToLowerInvariant()) {
    "debug" { "Debug" }
    "release" { "Release" }
    default { $BuildType }
}

if ($BuildType -notin @("Debug", "Release")) {
    throw "Invalid build type '$BuildType'. Use 'Debug' or 'Release'."
}

$conanBuildType = $BuildType
$buildPolicy = if ($Flavor -eq "light") { "never" } else { "missing" }

if ($Flavor -eq "light" -and $BuildType -eq "Debug") {
    $conanBuildType = "Release"
    Write-Notice "Debug selected. Using Release Conan packages for light FFmpeg because Conan Center does not provide Debug binaries for this dependency set."
}

Write-Section "Selected configuration"
Write-Info "FFmpeg flavor: $Flavor"
Write-Info "Requested build type: $BuildType"
Write-Info "Conan build_type: $conanBuildType"
Write-Info "Conan build policy: $buildPolicy"
Write-Info "C++ standard: $compilerCppStd"
Write-Info "Parallel jobs: $Jobs"

Sync-FFmpegSource

Write-Step "Preparing project-local Conan cache"

$env:CONAN_HOME = $conanHome
New-Item -ItemType Directory -Force -Path $conanHome | Out-Null
Write-Info "CONAN_HOME=$conanHome"

$defaultProfile = Join-Path $conanHome "profiles/default"
if (-not (Test-Path -LiteralPath $defaultProfile)) {
    Write-Info "Default Conan profile not found. Detecting profile..."
    conan profile detect --force
    if ($LASTEXITCODE -ne 0) {
        throw "conan profile detect failed with exit code $LASTEXITCODE"
    }
    Write-Success "Conan profile detected."
} else {
    Write-Success "Conan profile found: $defaultProfile"
}

$conanInstallStamp = Get-ConanInstallStamp `
    -Flavor $Flavor `
    -ConanBuildType $conanBuildType `
    -BuildPolicy $buildPolicy

Write-Step "Checking Conan dependencies"

if (Test-ConanInstallFresh -ExpectedStamp $conanInstallStamp) {
    Write-Success "Conan dependencies are already installed for this configuration."
    Write-Notice "Skipping conan install. Use -ForceConan to refresh dependency graph and generated CMake files."
} else {
    Write-Info "Installing Conan dependencies: flavor=$Flavor, selected_build_type=$BuildType, conan_build_type=$conanBuildType, build=$buildPolicy"
    Write-Info "Adding or updating conancenter remote in project-local Conan cache..."
    conan remote add conancenter https://center2.conan.io --force
    if ($LASTEXITCODE -ne 0) {
        throw "conan remote add failed with exit code $LASTEXITCODE"
    }

    Write-Step "Running conan install"
    conan install $repoRoot `
      -r=conancenter `
      "--build=$buildPolicy" `
      -c "tools.build:jobs=$Jobs" `
      -c "tools.cmake.cmaketoolchain:user_presets=" `
      -s "build_type=$conanBuildType" `
      -s "compiler.cppstd=$compilerCppStd" `
      -o "&:ffmpeg_flavor=$Flavor"
    if ($LASTEXITCODE -ne 0) {
        throw "conan install failed with exit code $LASTEXITCODE"
    }

    New-Item -ItemType Directory -Force -Path $dependencyStateDir | Out-Null
    [System.IO.File]::WriteAllText($dependencyStampPath, $conanInstallStamp, $utf8NoBom)
    Write-Success "Conan install finished."
}

Write-Step "Generating clangd compile database"
& (Join-Path $repoRoot "update_clangd.ps1") -BuildType $BuildType
Write-Success "Dependency setup complete."
