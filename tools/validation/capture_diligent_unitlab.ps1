[CmdletBinding()]
param(
    [ValidateSet('dev-diligent','release-diligent','dev-diligent-hybrid')][string]$Preset='dev-diligent',
    [string]$OutputDirectory=''
)
$ErrorActionPreference='Stop'
if ($null -ne (Get-Variable PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue)) {
    $PSNativeCommandUseErrorActionPreference=$false
}
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$executable=Join-Path $root "build/$Preset/apps/game/genomes_game.exe"
if (-not (Test-Path $executable)) { throw "Build genomes_game first: $executable" }
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory=Join-Path $root ('artifacts/diligent-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
}
if (Test-Path $OutputDirectory) { throw 'Use a new output directory; evidence is never overwritten.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$OutputDirectory=(Resolve-Path $OutputDirectory).Path
$cases=@(
    @{ Name='three-quarter'; Camera='3q'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='front'; Camera='front'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='side'; Camera='side'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='back'; Camera='back'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='face'; Camera='face'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='hands'; Camera='hands'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='run'; Camera='3q'; Pose='run'; Expression='neutral'; Frame='137' },
    @{ Name='crouch'; Camera='side'; Pose='crouch'; Expression='neutral'; Frame='121' },
    @{ Name='eyes-closed'; Camera='face'; Pose='idle'; Expression='eyes-closed'; Frame='90' },
    @{ Name='anger'; Camera='face'; Pose='idle'; Expression='anger'; Frame='120' },
    @{ Name='pain'; Camera='face'; Pose='idle'; Expression='pain'; Frame='120' }
)
Push-Location $root
try {
    foreach ($case in $cases) {
        $image=Join-Path $OutputDirectory ($case.Name+'.png')
        $log=Join-Path $OutputDirectory ($case.Name+'.log')
        & $executable '--unit-lab' '--unitlab-camera' $case.Camera '--unitlab-locomotion' $case.Pose `
            '--unitlab-expression' $case.Expression '--deterministic' '--frames' $case.Frame `
            '--capture' $image '--capture-frame' $case.Frame *> $log
        if ($LASTEXITCODE -ne 0) { throw "Capture failed: $($case.Name). Read $log" }
        if (-not (Test-Path $image) -or -not (Test-Path ($image+'.json'))) {
            throw "Capture is incomplete: $($case.Name)"
        }
        Write-Host "Created $image and metadata. Visual acceptance is still required."
    }
    git rev-parse HEAD | Set-Content (Join-Path $OutputDirectory 'tested-sha.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Could not record tested Git revision.' }
    git status --short | Set-Content (Join-Path $OutputDirectory 'working-tree.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Could not record working tree status.' }
} finally { Pop-Location }
