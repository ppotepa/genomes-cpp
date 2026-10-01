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
$logPath = Join-Path $repoRoot 'run.log'
$buildErrorLogPath = Join-Path $repoRoot 'build-error.log'
$buildAttempted = $false

function Write-RunEvent {
    param(
        [Parameter(Mandatory = $true)][ValidateSet('INFO', 'WARN', 'ERROR')][string]$Level,
        [Parameter(Mandatory = $true)][string]$Message
    )
    $line = "[{0:yyyy-MM-dd HH:mm:ss}] [{1}] {2}" -f (Get-Date), $Level, $Message
    Write-Host $line
    Add-Content -LiteralPath $logPath -Value $line
}

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter()][string[]]$Arguments = @(),
        [Parameter()][string]$FailureLogPath,
        [switch]$AllowFailure
    )
    $quoteArgument = {
        param([string]$Value)
        if ($Value -notmatch '[\s"]') { return $Value }
        return '"' + ($Value -replace '(\\*)"', '$1$1\"' -replace '(\\+)$', '$1$1') + '"'
    }
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = (($Arguments | ForEach-Object { & $quoteArgument $_ }) -join ' ')
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Could not start native command: $FilePath"
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    if (-not [string]::IsNullOrEmpty($stdout)) {
        Write-Host $stdout -NoNewline
        Add-Content -LiteralPath $logPath -Value $stdout -NoNewline
    }
    if (-not [string]::IsNullOrEmpty($stderr)) {
        Write-Host $stderr -NoNewline -ForegroundColor Yellow
        Add-Content -LiteralPath $logPath -Value $stderr -NoNewline
    }
    $nativeExitCode = $process.ExitCode
    $process.Dispose()
    if ($nativeExitCode -ne 0 -and -not [string]::IsNullOrEmpty($FailureLogPath)) {
        $failureHeader = @(
            "Build command failed with exit code ${nativeExitCode}.",
            "Command: $FilePath $($Arguments -join ' ')",
            ''
        ) -join [Environment]::NewLine
        Set-Content -LiteralPath $FailureLogPath -Value $failureHeader -NoNewline
        if (-not [string]::IsNullOrEmpty($stdout)) {
            Add-Content -LiteralPath $FailureLogPath -Value $stdout -NoNewline
        }
        if (-not [string]::IsNullOrEmpty($stderr)) {
            Add-Content -LiteralPath $FailureLogPath -Value $stderr -NoNewline
        }
    }
    if ($nativeExitCode -ne 0 -and -not $AllowFailure) {
        throw "Command failed with exit code ${nativeExitCode}: $FilePath $($Arguments -join ' ')"
    }
    if ($AllowFailure) { return $nativeExitCode }
}

Push-Location $repoRoot
try {
    Set-Content -LiteralPath $logPath -Value ''
    Write-RunEvent -Level INFO -Message "Starting Genomes $Configuration ($preset)."
    Write-RunEvent -Level INFO -Message "Log: $logPath"
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
        (Join-Path $repoRoot 'external/json/single_include/nlohmann/json.hpp'),
        (Join-Path $repoRoot 'external/meshoptimizer/CMakeLists.txt'),
        (Join-Path $repoRoot 'external/mikktspace/mikktspace.c'),
        (Join-Path $repoRoot 'external/earcut/include/mapbox/earcut.hpp')
    )
    $missingDependency = $requiredDependencies |
        Where-Object { -not (Test-Path $_) } | Select-Object -First 1
    if ($null -ne $missingDependency) {
        throw "Missing dependency: $missingDependency. Initialize the pinned submodules."
    }

    if (-not $NoBuild) {
        # This file always describes the most recent build attempt. Compiler
        # diagnostics can be emitted on either stdout or stderr, so a failure
        # records both streams in their captured order groups.
        Set-Content -LiteralPath $buildErrorLogPath -Value '' -NoNewline
        $buildAttempted = $true
        Write-RunEvent -Level INFO -Message "Build errors: $buildErrorLogPath"
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
            Write-RunEvent -Level INFO -Message "Configuring Diligent/D3D12 with preset $preset."
            Invoke-NativeCommand -FilePath 'cmake' -Arguments @('--preset', $preset) `
                -FailureLogPath $buildErrorLogPath
        }
        Write-RunEvent -Level INFO -Message "Building genomes_game with preset $preset."
        Invoke-NativeCommand -FilePath 'cmake' `
            -Arguments @('--build', '--preset', $preset, '--target', 'genomes_game') `
            -FailureLogPath $buildErrorLogPath
    }

    if (-not (Test-Path $executable)) {
        throw "Executable not found: $executable. Remove -NoBuild or build the genomes_game target."
    }
    if ($BuildOnly) {
        Write-RunEvent -Level INFO -Message 'Build complete; skipping application launch (-BuildOnly).'
        exit 0
    }

    Write-RunEvent -Level INFO -Message "Launching $executable."
    $applicationExitCode = Invoke-NativeCommand -FilePath $executable `
        -Arguments $ApplicationArguments -AllowFailure
    if ($applicationExitCode -eq 0) {
        Write-RunEvent -Level INFO -Message 'Application exited successfully.'
    } else {
        Write-RunEvent -Level ERROR -Message "Application exited with code $applicationExitCode."
    }
    exit $applicationExitCode
}
catch {
    if ($buildAttempted -and
        (-not (Test-Path -LiteralPath $buildErrorLogPath) -or
         (Get-Item -LiteralPath $buildErrorLogPath).Length -eq 0)) {
        Set-Content -LiteralPath $buildErrorLogPath -Value $_.Exception.Message
    }
    Write-RunEvent -Level ERROR -Message $_.Exception.Message
    exit 1
}
finally {
    Pop-Location
}
