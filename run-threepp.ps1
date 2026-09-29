[CmdletBinding()]
param(
    [ValidateSet('dev-threepp','release-threepp')]
    [string]$Preset = 'dev-threepp',
    [switch]$BuildOnly,
    [switch]$NoBuild,
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$AppArguments
)
$ErrorActionPreference = 'Stop'
if ($BuildOnly -and $NoBuild) { throw '-BuildOnly and -NoBuild are mutually exclusive.' }
Push-Location $PSScriptRoot
try {
    Write-Host "Backend: THREEPP_GL / bootstrap G02. The Infantry Lab is not switched yet."
    & git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot determine the checkout revision.' }
    & git -C external/threepp rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Initialize external/threepp with git submodule update --init --recursive external/threepp.' }
    if (-not $NoBuild) {
        & cmake --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
        & cmake --build --preset $Preset --target genomes_threepp_bootstrap
        if ($LASTEXITCODE -ne 0) { throw 'CMake build failed.' }
    } else {
        Write-Warning '-NoBuild may run a different revision; the executable logs its compiled SHA.'
    }
    if (-not $BuildOnly) {
        $exe = Join-Path $PSScriptRoot "build/$Preset/bin/genomes_threepp_bootstrap.exe"
        if (-not (Test-Path $exe)) { throw "Missing executable: $exe" }
        & $exe @AppArguments
        if ($LASTEXITCODE -ne 0) { throw "threepp bootstrap failed with exit code $LASTEXITCODE" }
    }
} finally { Pop-Location }
