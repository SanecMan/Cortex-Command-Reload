[CmdletBinding()]
param(
    [string]$ExecutablePath,
    [string]$GameDirectory,
    [ValidateSet('Transition', 'Equipment', 'Control')]
    [string]$Scenario = 'Transition',
    [ValidatePattern('^[A-Za-z0-9_-]{1,48}$')]
    [string]$OutputPrefix = 'WorldStateSmoke',
    [ValidateRange(0, 65535)]
    [int]$Port = 0,
    [ValidateRange(60, 1800)]
    [int]$TimeoutSeconds = 600
)

$ErrorActionPreference = 'Stop'
if (!$ExecutablePath) { $ExecutablePath = Join-Path $PSScriptRoot '..\Cortex Command.exe' }
$executable = (Resolve-Path -LiteralPath $ExecutablePath).Path
if (!$GameDirectory) { $GameDirectory = Split-Path -Parent $executable }
$gameDirectory = (Resolve-Path -LiteralPath $GameDirectory).Path
$hostOutput = "$OutputPrefix-Host"
$clientOutput = "$OutputPrefix-Client"
$hostReportPath = Join-Path $gameDirectory "ScreenShots\$hostOutput\DebugRun.log"
$clientReportPath = Join-Path $gameDirectory "ScreenShots\$clientOutput\DebugRun.log"
$processes = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
$timer = [System.Diagnostics.Stopwatch]::StartNew()

function Read-FreshReport([string]$Path) {
    if (!(Test-Path -LiteralPath $Path)) { return '' }
    return Get-Content -LiteralPath $Path -Raw
}

function Assert-TimeRemaining {
    if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
        throw "Smoke timed out after $TimeoutSeconds seconds. Reports: $hostReportPath and $clientReportPath"
    }
}

# The engine replaces these owned reports too, but an open Windows file can
# retain the previous LastWriteTime until it closes. Remove the two old reports
# before launching so readiness can never be read from a previous invocation.
foreach ($reportPath in @($hostReportPath, $clientReportPath)) {
    $reportDirectory = Split-Path -Parent $reportPath
    $screenshotsDirectory = Split-Path -Parent $reportDirectory
    foreach ($directory in @($screenshotsDirectory, $reportDirectory)) {
        if ((Test-Path -LiteralPath $directory) -and
            ((Get-Item -LiteralPath $directory).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw "Diagnostic output directory must not be a reparse point: $directory"
        }
    }
    if (Test-Path -LiteralPath $reportPath) { Remove-Item -LiteralPath $reportPath }
}

if ($Port -eq 0) {
    $portProbe = [System.Net.Sockets.UdpClient]::new([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Loopback, 0))
    try { $Port = $portProbe.Client.LocalEndPoint.Port }
    finally { $portProbe.Dispose() }
}

$hostUpdates = if ($Scenario -eq 'Equipment') { '3600' } else { '900' }
$clientUpdates = if ($Scenario -eq 'Equipment') { '600' } else { '900' }
$hostCheck = if ($Scenario -eq 'Equipment') { '-debug-run-world-state-equipment-mutation' } else { '-debug-run-world-state-transition' }
$clientCheck = if ($Scenario -eq 'Equipment') { '-debug-run-require-world-state-equipment-mutation' } else { '-debug-run-require-world-state-transition' }
if ($Scenario -eq 'Control') {
    $hostCheck = '-debug-run-world-state-control'
    $clientCheck = '-debug-run-require-world-state-control'
}

try {
    $hostProcess = Start-Process -FilePath $executable -WorkingDirectory $gameDirectory -WindowStyle Hidden -PassThru -ArgumentList @(
        '-debug-run', $hostUpdates, '-debug-run-output', $hostOutput, $hostCheck, '-world-state-server', "$Port"
    )
    $processes.Add($hostProcess)
    Write-Host "Waiting for host readiness on UDP port $Port..."
    while ($true) {
        Assert-TimeRemaining
        if ($hostProcess.HasExited) { throw 'Host exited before its readiness checks passed.' }
        $hostReport = Read-FreshReport $hostReportPath
        if ($hostReport -match '(?m)^world_state_host_session_smoke=passed' -and
            $hostReport -match '(?m)^screenshot=.*debug-run-initial\.png result=0') { break }
        Start-Sleep -Milliseconds 500
    }

    $clientProcess = Start-Process -FilePath $executable -WorkingDirectory $gameDirectory -WindowStyle Hidden -PassThru -ArgumentList @(
        '-debug-run', $clientUpdates, '-debug-run-output', $clientOutput, $clientCheck, '-world-state-client', '127.0.0.1', "$Port"
    )
    $processes.Add($clientProcess)
    Write-Host 'Client started; waiting for both diagnostic runs to finish...'
    while (!$hostProcess.HasExited -or !$clientProcess.HasExited) {
        Assert-TimeRemaining
        foreach ($process in $processes) {
            if ($process.HasExited) {
                $process.WaitForExit()
                if ($process.ExitCode -ne 0) { throw "Diagnostic process $($process.Id) exited with code $($process.ExitCode)." }
            }
        }
        Start-Sleep -Milliseconds 500
    }
    foreach ($process in $processes) {
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "Diagnostic process $($process.Id) exited with code $($process.ExitCode)." }
    }
    $hostReport = Read-FreshReport $hostReportPath
    $clientReport = Read-FreshReport $clientReportPath
    if ($hostReport -notmatch '(?m)^result=passed\r?$' -or $clientReport -notmatch '(?m)^result=passed\r?$') {
        throw 'One or both engine reports did not confirm result=passed.'
    }
    [pscustomobject]@{
        Scenario = $Scenario
        Result = 'passed'
        Port = $Port
        ElapsedSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 2)
        HostReport = $hostReportPath
        ClientReport = $clientReportPath
    }
} finally {
    foreach ($process in $processes) {
        if (!$process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            $process.WaitForExit()
        }
        $process.Dispose()
    }
}
