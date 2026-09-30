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
$preset = if ($Configuration -eq 'Debug') { 'dev-debug' } else { 'dev-release' }
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDirectory = Join-Path $repoRoot "build/$preset"
$executable = Join-Path $buildDirectory 'bin/genomes_game.exe'

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter()][string[]]$Arguments = @()
    )
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

Push-Location $repoRoot
try {
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw 'cmake was not found in PATH.'
    }
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        throw 'ninja was not found in PATH.'
    }

    $requiredDependencies = @(
        (Join-Path $repoRoot 'external/DiligentEngine/CMakeLists.txt'),
        (Join-Path $repoRoot 'external/SDL/CMakeLists.txt'),
        (Join-Path $repoRoot 'external/RmlUi/CMakeLists.txt'),
        (Join-Path $repoRoot 'external/freetype/CMakeLists.txt'),
        (Join-Path $repoRoot 'external/json/single_include/nlohmann/json.hpp')
    )
    $missingDependency = $requiredDependencies |
        Where-Object { -not (Test-Path $_) } | Select-Object -First 1
    if ($null -ne $missingDependency) {
        throw "Missing dependency: $missingDependency. Initialize the pinned submodules."
    }

    if (-not $NoBuild) {
        $cache = Join-Path $buildDirectory 'CMakeCache.txt'
        $cacheMatchesDiligentPreset = $false
        if (Test-Path $cache) {
            $cacheContents = Get-Content -LiteralPath $cache -Raw
            $cacheMatchesDiligentPreset =
                $cacheContents -match '(?m)^GENOMES_RENDER_BACKEND:STRING=DILIGENT$' -and
                $cacheContents -match '(?m)^GENOMES_ENABLE_DILIGENT:BOOL=ON$' -and
                $cacheContents -match '(?m)^GENOMES_ENABLE_SDL:BOOL=ON$'
        }
        if ($Reconfigure -or -not $cacheMatchesDiligentPreset) {
            Write-Host "Configuring Genomes Diligent/D3D12 ($Configuration) with preset $preset"
            Invoke-NativeCommand -FilePath 'cmake' -Arguments @('--preset', $preset)
        }
        Write-Host "Building genomes_game with preset $preset"
        Invoke-NativeCommand -FilePath 'cmake' -Arguments @('--build', '--preset', $preset, '--target', 'genomes_game')
    }

    if (-not (Test-Path $executable)) {
        throw "Executable not found: $executable. Remove -NoBuild or build the genomes_game target."
    }
    if ($BuildOnly) {
        Write-Host 'Build complete; skipping application launch (-BuildOnly).'
        exit 0
    }

    Write-Host "Launching: $executable"
    & $executable @ApplicationArguments
    exit $LASTEXITCODE
}
finally {
    Pop-Location
}
