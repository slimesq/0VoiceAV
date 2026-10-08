param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = $PSScriptRoot
$generatorDir = Join-Path $repoRoot "build/windows/generators"
$databaseBuildDir = Join-Path $repoRoot "build/clangd-cmake"
$clangdBuildDir = Join-Path $repoRoot "build/clangd"
$ffmpegSourceDatabase = Join-Path $repoRoot "third_party/ffmpeg-src/compile_commands.json"

if (-not (Test-Path -LiteralPath (Join-Path $generatorDir "ffmpeg-config.cmake"))) {
    throw "FFmpeg dependencies are missing. Run ./pull_dependency.ps1 first."
}

$vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
if (-not (Test-Path -LiteralPath $vswherePath)) {
    throw "vswhere.exe was not found. Install Visual Studio 2022 with C++ tools."
}

$vsPath = & $vswherePath -latest -version "[17.0,18.0)" -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($vsPath)) {
    throw "Visual Studio 2022 with x64 C++ tools was not found."
}

# Initialize MSVC for Ninja, including compiler and Windows SDK search paths.
$originalLocation = Get-Location
try {
    & (Join-Path $vsPath "Common7/Tools/Launch-VsDevShell.ps1") -Arch amd64 -HostArch amd64
} finally {
    Set-Location -LiteralPath $originalLocation.Path
}

$ninjaCommand = Get-Command ninja -ErrorAction SilentlyContinue
if ($null -ne $ninjaCommand) {
    $ninjaPath = $ninjaCommand.Source
} else {
    $ninjaPath = Join-Path $vsPath "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
}
if (-not (Test-Path -LiteralPath $ninjaPath)) {
    throw "Ninja was not found. Install Ninja or the Visual Studio CMake tools."
}

# Load Conan package configs directly: its VS toolchain specifies a platform/toolset
# that Ninja does not support. The project configures its own compiler options.
$configureArguments = @(
    "-S", $repoRoot,
    "-B", $databaseBuildDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_C_COMPILER=cl",
    "-DCMAKE_CXX_COMPILER=cl",
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=$ninjaPath",
    "-DVOICE_AV_CONAN_GENERATORS_DIR:PATH=$generatorDir",
    "-DCMAKE_PREFIX_PATH:PATH=$generatorDir",
    "-Dffmpeg_DIR:PATH=$generatorDir",
    "-U", "CMAKE_MAP_IMPORTED_CONFIG_DEBUG",
    "-U", "CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO",
    "-U", "CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)

& cmake @configureArguments
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed. The existing clangd database was not updated."
}

$projectDatabase = Join-Path $databaseBuildDir "compile_commands.json"
$mergedCommands = [System.Collections.Generic.List[object]]::new()
$projectCommands = Get-Content -LiteralPath $projectDatabase -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($entry in $projectCommands) {
    $mergedCommands.Add($entry)
}
if (Test-Path -LiteralPath $ffmpegSourceDatabase) {
    $ffmpegCommands = Get-Content -LiteralPath $ffmpegSourceDatabase -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($entry in $ffmpegCommands) {
        $mergedCommands.Add($entry)
    }
}

# A compilation database must be a flat array of command objects.
foreach ($entry in $mergedCommands) {
    if ($null -eq $entry -or $entry -is [System.Array] `
        -or $null -eq $entry.PSObject.Properties["file"] `
        -or $null -eq $entry.PSObject.Properties["directory"] `
        -or $entry.file -isnot [string] -or [string]::IsNullOrWhiteSpace($entry.file) `
        -or $entry.directory -isnot [string] -or [string]::IsNullOrWhiteSpace($entry.directory)) {
        throw "Invalid compilation database entry. The existing database was not updated."
    }

    $commandProperty = $entry.PSObject.Properties["command"]
    $argumentsProperty = $entry.PSObject.Properties["arguments"]
    $hasCommand = $null -ne $commandProperty -and $commandProperty.Value -is [string] `
        -and -not [string]::IsNullOrWhiteSpace($commandProperty.Value)
    $hasArguments = $null -ne $argumentsProperty -and $argumentsProperty.Value -is [System.Array] `
        -and $argumentsProperty.Value.Count -gt 0
    if ($hasArguments) {
        foreach ($argument in $argumentsProperty.Value) {
            if ($argument -isnot [string]) {
                $hasArguments = $false
                break
            }
        }
    }
    if ((-not $hasCommand -and -not $hasArguments) `
        -or ($null -ne $argumentsProperty -and -not $hasArguments)) {
        throw "Invalid compilation database entry. The existing database was not updated."
    }
}

New-Item -ItemType Directory -Force -Path $clangdBuildDir | Out-Null
$databasePath = Join-Path $clangdBuildDir "compile_commands.json"
$payload = ConvertTo-Json -InputObject $mergedCommands.ToArray() -Depth 10
# Write each payload completely before replacing the previous database.
foreach ($outputPath in @($databasePath, (Join-Path $repoRoot "compile_commands.json"))) {
    $temporaryPath = "$outputPath.$([Guid]::NewGuid().ToString('N')).tmp"
    try {
        [System.IO.File]::WriteAllText($temporaryPath, $payload, [System.Text.UTF8Encoding]::new($false))
        if (Test-Path -LiteralPath $outputPath) {
            [System.IO.File]::Replace($temporaryPath, $outputPath, [NullString]::Value)
        } else {
            [System.IO.File]::Move($temporaryPath, $outputPath)
        }
    } finally {
        if (Test-Path -LiteralPath $temporaryPath) {
            Remove-Item -LiteralPath $temporaryPath -Force
        }
    }
}
Write-Host "clangd compile database ready: $databasePath"
