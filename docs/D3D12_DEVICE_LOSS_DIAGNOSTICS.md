# D3D12 device-loss diagnostics

The Windows Diligent backend records the native device-removal HRESULT and
stops submitting GPU work after it detects device loss. This prevents repeated
fence and `Present` failures from obscuring the first failure; it does not
recover or recreate the removed device.

For a diagnostic run, enable D3D12 DRED before launching the application from
the same PowerShell session:

```powershell
$env:GENOMES_D3D12_DRED = "1"
# From the repository root, after the owner-run dev-release build:
$dredRun = Join-Path 'artifacts' ('dred-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $dredRun | Out-Null
rtk proxy git rev-parse HEAD | Out-File (Join-Path $dredRun 'revision.txt')
rtk proxy git status --short | Out-File (Join-Path $dredRun 'worktree.txt')
rtk proxy .\build\dev-release\bin\genomes_game.exe --battlefield 2>&1 |
    Tee-Object -FilePath (Join-Path $dredRun 'battlefield.log')
$LASTEXITCODE | Out-File (Join-Path $dredRun 'battlefield-exit.txt')
# Separate process/log for the crowd scene and HUD/weapon visual review:
rtk proxy .\build\dev-release\bin\genomes_game.exe --infantry-mass-battle 2>&1 |
    Tee-Object -FilePath (Join-Path $dredRun 'mass-battle.log')
$LASTEXITCODE | Out-File (Join-Path $dredRun 'mass-battle-exit.txt')
```

DRED tracking is also enabled when renderer validation is enabled. With DRED
active, a device-loss report may include command-queue/list breadcrumbs, the
last completed breadcrumb count, the next operation, and page-fault allocation
names. Capture the first device-loss report together with the GPU errors just
before it. Missing DRED records do not prove that the failure was not GPU work;
the driver or runtime may not provide extended data.

When logs show `GetCompletedFenceValue()` returning `UINT64_MAX` or `Present`
failing before a later resource-creation error, treat the later allocation as a
possible consequence of device removal, not as the cause by itself. The
`Genomes persistent mesh vertices` buffer is only about 810 KiB in the current
reported failure, so its size alone is not evidence of an oversized-buffer
problem. Preserve the log from process startup: Diligent reports the selected
adapter during device creation, and that line is needed to correlate the
failure with the active GPU and its driver. The first device-loss report also
includes the selected D3D12 adapter LUID from the live device.

Keep the complete logs rather than extracting only the final resource error.
The first DRED report is still required to choose a renderer/resource fix;
the cause is not established by the source changes. CPU tests and JS fixtures
cannot verify GPU device-loss recovery or posture/weapon visual behavior.

Unset the variable after diagnosis to avoid breadcrumb-tracking overhead in
normal runs:

```powershell
Remove-Item Env:GENOMES_D3D12_DRED
```
