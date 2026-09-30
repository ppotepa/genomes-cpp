[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration='Debug',
    [switch]$BuildOnly,
    [switch]$NoBuild,
    [Parameter(ValueFromRemainingArguments=$true)][string[]]$ApplicationArguments
)
$ErrorActionPreference='Stop'
if ($BuildOnly -and $NoBuild) { throw 'Use either -BuildOnly or -NoBuild.' }
if ($null -ne (Get-Variable PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue)) {
    $PSNativeCommandUseErrorActionPreference=$false
}
$root=$PSScriptRoot
$preset=if ($Configuration -eq 'Release') { 'dev-release' } else { 'dev-debug' }
$binary=Join-Path $root "build/$preset/bin/genomes_game.exe"
$cache=Join-Path $root "build/$preset/CMakeCache.txt"
$oldPath=$env:PATH
function Invoke-Checked([string]$Tool,[string[]]$Arguments) {
    & $Tool @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed with exit code $LASTEXITCODE" }
}
Push-Location $root
try {
    if (-not $NoBuild) {
        foreach ($tool in @('cmake','ninja','git')) {
            if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) { throw "$tool is missing from PATH. Use a configured C++ developer shell." }
        }
        $required=@('external/DiligentEngine/CMakeLists.txt',
                    'external/DiligentEngine/DiligentTools/CMakeLists.txt',
                    'external/SDL/CMakeLists.txt','external/RmlUi/CMakeLists.txt',
                    'external/freetype/CMakeLists.txt','external/json/single_include/nlohmann/json.hpp')
        foreach ($file in $required) {
            if (-not (Test-Path (Join-Path $root $file))) {
                throw "Missing $file. Initialize the pinned submodules; do not substitute another dependency checkout."
            }
        }
        # FXC and DXC are different tools. Do not rename dxc.exe to fxc.exe.
        if (-not (Get-Command fxc -ErrorAction SilentlyContinue)) {
            $sdkRoots=@()
            if ($env:WindowsSdkDir) { $sdkRoots+=(Join-Path $env:WindowsSdkDir 'bin') }
            if (${env:ProgramFiles(x86)}) { $sdkRoots+=(Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin') }
            $candidates=foreach ($sdk in ($sdkRoots | Select-Object -Unique)) {
                if (Test-Path $sdk) {
                    Get-ChildItem $sdk -Directory -ErrorAction SilentlyContinue |
                        Sort-Object Name -Descending | ForEach-Object {
                            Join-Path $_.FullName 'x64/fxc.exe'
                        }
                }
            }
            $fxc=$candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
            if (-not $fxc) { throw 'Microsoft FXC was not found. Install the Windows SDK shader tools or add its bin/version/x64 directory to PATH.' }
            $env:PATH=(Split-Path $fxc -Parent)+[IO.Path]::PathSeparator+$env:PATH
        }
        Write-Host "Diligent profile: $preset; compiler/GPU acceptance has not yet been performed by this script."
        Invoke-Checked 'git' @('rev-parse','HEAD')
        if ($Reconfigure -or -not (Test-Path $cache)) {
            Invoke-Checked 'cmake' @('--preset',$preset)
        }
        Invoke-Checked 'cmake' @('--build','--preset',$preset,'--target','genomes_game')
    }
    if (-not (Test-Path $binary)) { throw "Executable is missing: $binary" }
    if (-not $BuildOnly) {
        & $binary @ApplicationArguments
        $result=$LASTEXITCODE
        if ($result -ne 0) { throw "genomes_game exited with code $result" }
    }
} finally {
    $env:PATH=$oldPath
    Pop-Location
}
