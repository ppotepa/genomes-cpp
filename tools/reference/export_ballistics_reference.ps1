[CmdletBinding()]
param(
    [Parameter()]
    [string]$SourceCommit = 'da885ca68b2ae63154a004574fed00eb9dfeb458',

    [Parameter()]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$fixtureDirectory = Join-Path $repoRoot 'reference/fixtures/ballistics'
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = $fixtureDirectory
}
if ($SourceCommit -notmatch '^[0-9a-f]{40}$') {
    throw 'SourceCommit must be a full 40-character SOURCE commit SHA.'
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$fixtures = Get-ChildItem -LiteralPath $fixtureDirectory -Filter '*.json' -File
if ($fixtures.Count -eq 0) {
    throw "No ballistics fixtures found in $fixtureDirectory."
}

foreach ($fixture in $fixtures) {
    $document = Get-Content -LiteralPath $fixture.FullName -Raw | ConvertFrom-Json
    if ($document.schema -ne 'ballistics-fixtures-1' -or
        [string]::IsNullOrWhiteSpace($document.case)) {
        throw "Unsupported ballistics fixture schema in $($fixture.Name)."
    }
    $document.sourceCommit = $SourceCommit
    $json = $document | ConvertTo-Json -Depth 16
    Set-Content -LiteralPath (Join-Path $OutputDirectory $fixture.Name) -Value $json -Encoding utf8
}

$manifest = @(
    'BALLISTICS FIXTURE MANIFEST',
    'schema=ballistics-fixtures-1',
    "sourceCommit=$SourceCommit",
    "fixtureCount=$($fixtures.Count)"
)
Set-Content -LiteralPath (Join-Path $OutputDirectory 'MANIFEST.txt') -Value $manifest -Encoding utf8
Write-Host "Exported $($fixtures.Count) ballistics fixtures to $OutputDirectory"
