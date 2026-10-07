[CmdletBinding()]
param(
    [string]$ExecutablePath,
    [ValidatePattern('^[A-Za-z0-9_-]{1,48}$')]
    [string]$OutputPrefix = 'HeadlessWorldStateSmoke',
    [ValidateRange(1, 2)]
    [int]$Clients = 2,
    [ValidateRange(600, 18000)]
    [int]$HostUpdates = 3600,
    [string]$ServerConfigPath,
    [ValidateRange(1, 64)]
    [int]$PlayerLimit = 8,
    [ValidateRange(60, 600)]
    [int]$TimeoutSeconds = 360
)

$ErrorActionPreference = 'Stop'
if (!$ExecutablePath) { $ExecutablePath = Join-Path $PSScriptRoot '..\Cortex Command.exe' }
$executable = (Resolve-Path -LiteralPath $ExecutablePath).Path
$gameDirectory = Split-Path -Parent $executable
$hostOutput = "$OutputPrefix-Host"
$hostReportPath = Join-Path $gameDirectory "ScreenShots\$hostOutput\DebugRun.log"
$processes = [System.Collections.Generic.List[System.Diagnostics.Process]]::new()
$clientReports = [System.Collections.Generic.List[string]]::new()
$timer = [System.Diagnostics.Stopwatch]::StartNew()

function Read-Report([string]$Path) {
    if (!(Test-Path -LiteralPath $Path)) { return '' }
    return Get-Content -LiteralPath $Path -Raw
}

function Assert-TimeRemaining {
    if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "Headless smoke timed out after $TimeoutSeconds seconds." }
}

# Remove only these owned reports so readiness cannot come from an older run.
foreach ($outputName in @($hostOutput) + @(1..$Clients | ForEach-Object { "$OutputPrefix-Client$_" })) {
    $reportDirectory = Join-Path $gameDirectory "ScreenShots\$outputName"
    foreach ($directory in @((Join-Path $gameDirectory 'ScreenShots'), $reportDirectory)) {
        if ((Test-Path -LiteralPath $directory) -and
            ((Get-Item -LiteralPath $directory).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw "Diagnostic directory must not be a reparse point: $directory"
        }
    }
    $reportPath = Join-Path $reportDirectory 'DebugRun.log'
    if (Test-Path -LiteralPath $reportPath) { Remove-Item -LiteralPath $reportPath }
}

try {
    $hostProcess = [System.Diagnostics.Process]::new()
    $hostProcess.StartInfo.FileName = $executable
    $hostProcess.StartInfo.WorkingDirectory = $gameDirectory
    $hostProcess.StartInfo.Arguments = "-debug-run $HostUpdates -debug-run-headless-server -debug-run-output $hostOutput"
    if ($ServerConfigPath) {
        $configPath = (Resolve-Path -LiteralPath $ServerConfigPath).Path
        $hostProcess.StartInfo.Arguments += ' --config "' + $configPath + '"'
    }
    # Keep automated connections local and let the OS allocate an unused port.
    $hostProcess.StartInfo.Arguments += " --listen 127.0.0.1 --port 0 --players $PlayerLimit"
    $hostProcess.StartInfo.UseShellExecute = $false
    $hostProcess.StartInfo.CreateNoWindow = $true
    $hostProcess.StartInfo.EnvironmentVariables['SDL_VIDEODRIVER'] = 'invalid-driver-for-headless-test'
    if (!$hostProcess.Start()) { throw 'Headless host start failed.' }
    $processes.Add($hostProcess)
    while ($true) {
        Assert-TimeRemaining
        if ($hostProcess.HasExited) { throw 'Headless host exited before readiness.' }
        $hostReport = Read-Report $hostReportPath
        if ($hostReport -match '(?m)^headless_server_ready=1 port=(\d+)') { $port = [int]$Matches[1]; break }
        Start-Sleep -Milliseconds 250
    }
    Write-Host "Headless server ready on UDP port $port; launching $Clients client(s)."
    foreach ($clientIndex in 1..$Clients) {
        $clientOutput = "$OutputPrefix-Client$clientIndex"
        $client = Start-Process -FilePath $executable -WorkingDirectory $gameDirectory -WindowStyle Hidden -PassThru -ArgumentList @(
            '-debug-run', '600', '-debug-run-output', $clientOutput, '-world-state-client', '127.0.0.1', "$port"
        )
        $processes.Add($client)
        $clientReports.Add((Join-Path $gameDirectory "ScreenShots\$clientOutput\DebugRun.log"))
    }
    while (@($processes | Where-Object { !$_.HasExited }).Count -gt 0) {
        Assert-TimeRemaining
        Start-Sleep -Milliseconds 500
    }
    foreach ($process in $processes) {
        $process.WaitForExit()
        $process.Refresh()
        if ($process.ExitCode -ne 0) { throw "Process $($process.Id) exited with $($process.ExitCode)." }
    }
    $hostReport = Read-Report $hostReportPath
    $serverLog = Read-Report (Join-Path (Split-Path -Parent $hostReportPath) 'WorldStateServer.log')
    if ($Clients -eq 2 -and $serverLog -notmatch '\(2 connected, input slot') {
        throw 'Two clients were not observed connected simultaneously.'
    }
    if ($hostReport -notmatch '(?m)^result=passed' -or
        $hostReport -notmatch '(?m)^headless_server_network=passed' -or
        $hostReport -notmatch '(?m)^headless_simulation_preflight=passed.*video_initialized=0') {
        throw "Headless host checks failed: $hostReportPath"
    }
    foreach ($clientReportPath in $clientReports) {
        $clientReport = Read-Report $clientReportPath
        $clientLogPath = Join-Path (Split-Path -Parent $clientReportPath) 'WorldStateClient.log'
        $clientLog = Read-Report $clientLogPath
        if ($clientReport -notmatch '(?m)^result=passed' -or
            $clientLog -notmatch '(?m)^snapshot=\d+.*missing_presets=0' -or
            $clientLog -match 'missing_presets=[1-9]' -or
            $clientLog -notmatch '(?m)^client_local_simulation_ticks=0') {
            throw "Authoritative client checks failed: $clientReportPath"
        }
    }
    [pscustomobject]@{ Result = 'passed'; Clients = $Clients; Port = $port; HostReport = $hostReportPath; ElapsedSeconds = [Math]::Round($timer.Elapsed.TotalSeconds, 2) }
} finally {
    foreach ($process in $processes) {
        if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
}
