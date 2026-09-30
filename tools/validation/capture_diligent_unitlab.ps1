[CmdletBinding()]
param(
    [ValidateSet('dev-debug','dev-release')][string]$Preset='dev-debug',
    [string]$OutputDirectory=''
)
$ErrorActionPreference='Stop'
if ($null -ne (Get-Variable PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue)) {
    $PSNativeCommandUseErrorActionPreference=$false
}
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$executable=Join-Path $root "build/$Preset/bin/genomes_game.exe"
if (-not (Test-Path $executable)) { throw "Build genomes_game first: $executable" }
$runId=Get-Date -Format 'yyyyMMdd-HHmmssfff'
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory=Join-Path $root ('artifacts/diligent-'+$Preset+'-'+$runId)
}
if (Test-Path $OutputDirectory) { throw 'Use a new output directory; evidence is never overwritten.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$OutputDirectory=(Resolve-Path $OutputDirectory).Path
$manifest=[System.Collections.Generic.List[object]]::new()
$cases=@(
    @{ Name='three-quarter'; Camera='3q'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='front'; Camera='front'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='side'; Camera='side'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='back'; Camera='back'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='face'; Camera='face'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='hands'; Camera='hands'; Pose='idle'; Expression='neutral'; Frame='180' },
    @{ Name='walk'; Camera='3q'; Pose='walk'; Expression='neutral'; Frame='137' },
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
        $stdout=Join-Path $OutputDirectory ($case.Name+'.stdout.log')
        $stderr=Join-Path $OutputDirectory ($case.Name+'.stderr.log')
        $exitFile=Join-Path $OutputDirectory ($case.Name+'.exit-code.txt')
        & $executable '--unit-lab' '--unitlab-camera' $case.Camera '--unitlab-locomotion' $case.Pose `
            '--unitlab-expression' $case.Expression '--deterministic' '--frames' $case.Frame `
            '--capture' $image '--capture-frame' $case.Frame 1> $stdout 2> $stderr
        $exitCode=[int]$LASTEXITCODE
        $exitCode | Set-Content $exitFile
        $metadataPath=$image+'.json'
        $manifest.Add([ordered]@{ name=$case.Name; image=$image; metadata=$metadataPath;
            stdout=$stdout; stderr=$stderr; exit_code=$exitCode; frame=[int]$case.Frame })
        if ($exitCode -ne 0) { throw "Capture failed: $($case.Name). Read $stdout and $stderr" }
        if (-not (Test-Path $image) -or -not (Test-Path ($image+'.json'))) {
            throw "Capture is incomplete: $($case.Name)"
        }
        if ((Get-Item -LiteralPath $image).Length -le 0) {
            throw "Capture produced an empty image: $($case.Name)"
        }
        try {
            $metadata = Get-Content -Raw -LiteralPath ($image+'.json') | ConvertFrom-Json
        } catch {
            throw "Capture produced invalid metadata JSON: $($case.Name)"
        }
        if ($null -eq $metadata) {
            throw "Capture produced empty metadata: $($case.Name)"
        }
        Write-Host "Created $image and metadata. Visual acceptance is still required."
    }
    git rev-parse HEAD | Set-Content (Join-Path $OutputDirectory 'tested-sha.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Could not record tested Git revision.' }
    git status --short | Set-Content (Join-Path $OutputDirectory 'working-tree.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Could not record working tree status.' }
    [ordered]@{ run_id=$runId; preset=$Preset; executable=$executable;
        git_sha=(Get-Content (Join-Path $OutputDirectory 'tested-sha.txt'));
        captures=$manifest } | ConvertTo-Json -Depth 6 |
        Set-Content (Join-Path $OutputDirectory 'capture-manifest.json')
} finally { Pop-Location }
