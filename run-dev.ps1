[CmdletBinding()]
param(
    [switch]$NoBuild,

    [switch]$BuildOnly,

    [switch]$Reconfigure,

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ApplicationArguments
)

$scriptArguments = @{
    Configuration = 'Debug'
    NoBuild = $NoBuild
    BuildOnly = $BuildOnly
}
if ($Reconfigure) {
    $scriptArguments.Reconfigure = $true
}
if ($null -ne $ApplicationArguments) {
    $scriptArguments.ApplicationArguments = $ApplicationArguments
}

& (Join-Path $PSScriptRoot 'scripts/RunNative.ps1') @scriptArguments
exit $LASTEXITCODE
