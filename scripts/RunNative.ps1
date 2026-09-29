[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration,

    [switch]$NoBuild,

    [switch]$BuildOnly,

    [switch]$Reconfigure,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ApplicationArguments
)

$ErrorActionPreference = 'Stop'

# PowerShell 7 can promote stderr from native tools to terminating errors when
# ErrorActionPreference is Stop. CMake (and the compilers it invokes) commonly
# writes non-fatal warnings to stderr, so let the native exit code determine
# success while still failing on a non-zero exit code below.
if ($null -ne (Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue)) {
    $PSNativeCommandUseErrorActionPreference = $false
}

if ($NoBuild -and $BuildOnly) {
    throw 'Use either -NoBuild or -BuildOnly, not both.'
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDirectory = Join-Path $repoRoot ('build/run-' + $Configuration.ToLowerInvariant())
$executable = Join-Path $buildDirectory 'apps/game/genomes_game.exe'
$fingerprintPath = Join-Path $buildDirectory 'genomes-config.fingerprint'

if ($NoBuild) {
    if (-not (Test-Path $executable)) {
        throw "Executable not found: $executable. Build the genomes_game target first."
    }
    Write-Host "Launching existing executable: $executable"
    Push-Location $repoRoot
    try {
        & $executable @ApplicationArguments
        $applicationExitCode = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    exit $applicationExitCode
}

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [Parameter()]
        [string[]]$Arguments = @()
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'cmake was not found in PATH.'
}

if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw 'ninja was not found in PATH. These scripts use the Ninja generator.'
}

$requiredDependencies = @(
    (Join-Path $repoRoot 'external/threepp/CMakeLists.txt'),
    (Join-Path $repoRoot 'external/SDL/CMakeLists.txt'),
    (Join-Path $repoRoot 'external/RmlUi/CMakeLists.txt'),
    (Join-Path $repoRoot 'external/freetype/CMakeLists.txt'),
    (Join-Path $repoRoot 'external/json/single_include/nlohmann/json.hpp')
)

$missingDependency = $requiredDependencies | Where-Object { -not (Test-Path $_) } | Select-Object -First 1
if ($null -ne $missingDependency) {
    throw "Missing dependency: $missingDependency. Run: git submodule update --init --recursive"
}

$configureArguments = @(
    '-S', $repoRoot,
    '-B', $buildDirectory,
    '-G', 'Ninja',
    ('-DCMAKE_BUILD_TYPE=' + $Configuration),
    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
    '-DGENOMES_RENDER_BACKEND=THREEPP_GL',
    '-DGENOMES_ENABLE_THREEPP=ON',
    '-DGENOMES_ENABLE_SDL=ON',
    '-DGENOMES_ENABLE_DILIGENT=OFF',
    '-DGENOMES_ENABLE_RMLUI=ON',
    '-DGENOMES_ENABLE_CUDA=OFF',
    '-DGENOMES_BUILD_TESTS=OFF',
    '-DGENOMES_BUILD_BENCHMARKS=OFF',
    '-DGENOMES_WARNINGS_AS_ERRORS=ON'
)

function Get-ToolIdentity {
    param([Parameter(Mandatory = $true)][string]$Name)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $command) { return "$Name=<missing>" }
    $version = & $command.Source '--version' 2>$null | Select-Object -First 1
    if ($null -eq $version) { $version = '' }
    return "$Name=$($command.Source)|$version"
}

$fingerprintInputs = @(
    (Join-Path $repoRoot 'CMakeLists.txt'),
    (Join-Path $repoRoot 'CMakePresets.json'),
    (Join-Path $repoRoot 'cmake'),
    (Join-Path $repoRoot 'config'),
    (Join-Path $repoRoot 'external/threepp'),
    (Join-Path $repoRoot 'external/SDL'),
    (Join-Path $repoRoot 'external/RmlUi')
)
function Get-Sha256Hex {
    param([Parameter(Mandatory = $true)][string]$Path)
    $bytes = [IO.File]::ReadAllBytes($Path)
    return ([BitConverter]::ToString(
        [Security.Cryptography.SHA256]::Create().ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
}
$fingerprintFiles = foreach ($input in $fingerprintInputs) {
    if (Test-Path $input -PathType Leaf) { Get-Item -LiteralPath $input }
    elseif (Test-Path $input -PathType Container) {
        Get-ChildItem -LiteralPath $input -Recurse -File |
            Where-Object { $_.Extension -in @('.cmake', '.txt', '.json', '.in') }
    }
}
$fingerprintPayload = @(
    "configuration=$Configuration"
    'generator=Ninja'
    "cmake=$((Get-Command cmake).Source)"
    "ninja=$((Get-Command ninja).Source)"
    'GENOMES_RENDER_BACKEND=THREEPP_GL'
    'GENOMES_ENABLE_THREEPP=ON'
    'GENOMES_ENABLE_SDL=ON'
    'GENOMES_ENABLE_DILIGENT=OFF'
    'GENOMES_ENABLE_RMLUI=ON'
    'GENOMES_ENABLE_CUDA=OFF'
    'GENOMES_BUILD_TESTS=OFF'
    'GENOMES_BUILD_BENCHMARKS=OFF'
    'GENOMES_WARNINGS_AS_ERRORS=ON'
    (Get-ToolIdentity 'cmake')
    (Get-ToolIdentity 'ninja')
    (Get-ToolIdentity 'clang')
    (Get-ToolIdentity 'clang++')
)
foreach ($file in ($fingerprintFiles | Sort-Object FullName -Unique)) {
    $digest = Get-Sha256Hex -Path $file.FullName
    $relative = $file.FullName.Substring($repoRoot.Length) -replace '^[\\/]+', ''
    $fingerprintPayload += "$relative=$digest"
}
$fingerprintText = (($fingerprintPayload -join "`n") + "`n")
$currentFingerprint = ([BitConverter]::ToString(
    [Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($fingerprintText)))).Replace('-', '').ToLowerInvariant()
$cacheExists = (Test-Path (Join-Path $buildDirectory 'CMakeCache.txt')) -and
               (Test-Path (Join-Path $buildDirectory 'build.ninja')) -and
               (Test-Path $fingerprintPath)
$storedFingerprint = if (Test-Path $fingerprintPath) { (Get-Content -Raw $fingerprintPath).Trim() } else { '' }
$needsConfigure = $Reconfigure -or -not $cacheExists -or $storedFingerprint -ne $currentFingerprint
if ($needsConfigure) {
    Write-Host "Configuring Genomes THREEPP_GL ($Configuration): $buildDirectory"
    Invoke-NativeCommand -FilePath 'cmake' -Arguments $configureArguments
    New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
    Set-Content -LiteralPath $fingerprintPath -Value $currentFingerprint -NoNewline
} else {
    Write-Host "Configuration is current; skipping CMake configure: $buildDirectory"
}

if (-not $NoBuild) {
    Write-Host 'Building genomes_game (CMake performs only required incremental rebuilds)...'
    Invoke-NativeCommand -FilePath 'cmake' -Arguments @('--build', $buildDirectory, '--target', 'genomes_game', '--parallel')
}

if (-not (Test-Path $executable)) {
    throw "Executable not found: $executable. Remove -NoBuild or build the genomes_game target."
}

if ($BuildOnly) {
    Write-Host 'Build complete; skipping application launch (-BuildOnly).'
    exit 0
}

Write-Host "Launching: $executable"
Push-Location $repoRoot
try {
    & $executable @ApplicationArguments
    $applicationExitCode = $LASTEXITCODE
}
finally {
    Pop-Location
}

exit $applicationExitCode
