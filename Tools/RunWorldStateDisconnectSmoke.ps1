[CmdletBinding()]
param(
    [string]$ServerExecutablePath = "$PSScriptRoot\..\_Bin\MesonPortableEntry\CortexCommandServer.exe",
    [string]$ClientExecutablePath = "$PSScriptRoot\..\_Bin\MesonPortableEntry\Cortex Command.exe",
    [string]$GameDirectory = "$PSScriptRoot\..",
    [ValidatePattern('^[A-Za-z0-9_-]{1,48}$')][string]$OutputPrefix = 'DisconnectMenuSmoke',
    [switch]$CheckReconnect,
    [int]$TimeoutSeconds = 240
)
$ErrorActionPreference = 'Stop'
$gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path
$hostLogs = Join-Path $gameRoot "ScreenShots\$OutputPrefix-Host"
foreach ($path in @((Join-Path $gameRoot 'ScreenShots'),$hostLogs)) {
    if ((Test-Path -LiteralPath $path) -and ((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Reparse diagnostic path.' }
}
New-Item -ItemType Directory -Path $hostLogs -Force | Out-Null
foreach ($name in @('Server.log','ServerFailure.log','WorldStateServer.log','LogLoading.txt','LogConsole.txt')) {
    $path = Join-Path $hostLogs $name
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
}
$server = [Diagnostics.Process]::new()
$client = $null
$watch = [Diagnostics.Stopwatch]::StartNew()
function Check-Deadline { if ($watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Disconnect menu test timed out.' } }
function Read-Log([string]$path) { if (Test-Path -LiteralPath $path) { Get-Content -LiteralPath $path -Raw } else { '' } }
# Readiness is observed only in the fresh log for this diagnostic run.
$serverLog = Join-Path $hostLogs 'Server.log'
try {
    $server.StartInfo.FileName = (Resolve-Path -LiteralPath $ServerExecutablePath).Path
    $server.StartInfo.WorkingDirectory = $gameRoot
    $server.StartInfo.Arguments = '--listen 127.0.0.1 --port 0 --players 2 --log-directory "' + $hostLogs + '"'
    $server.StartInfo.UseShellExecute = $false
    $server.StartInfo.CreateNoWindow = $true
    $server.StartInfo.RedirectStandardInput = $true
    $server.StartInfo.EnvironmentVariables['SDL_VIDEODRIVER'] = 'invalid-driver-for-headless-test'
    if (!$server.Start()) { throw 'Server start failed.' }
    do {
        Check-Deadline
        if ($server.HasExited) { throw 'Server exited before readiness.' }
        $text = Read-Log $serverLog
        if ($text -match 'INFO ready port=(\d+) video_initialized=0') { $port = [int]$Matches[1]; break }
        Start-Sleep -Milliseconds 200
    } while ($true)
    $clientLogs = Join-Path $gameRoot "ScreenShots\$OutputPrefix-Client"
    if ((Test-Path -LiteralPath $clientLogs) -and ((Get-Item -LiteralPath $clientLogs).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Reparse client diagnostic path.' }
    $report = Join-Path $clientLogs 'DebugRun.log'
    if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report }
    $client = Start-Process -FilePath (Resolve-Path -LiteralPath $ClientExecutablePath).Path -WorkingDirectory $gameRoot -WindowStyle Hidden -PassThru -ArgumentList '-debug-run','6000','-debug-run-output',"$OutputPrefix-Client",$(if ($CheckReconnect) { '-debug-run-reconnect-menu' } else { '-debug-run-disconnect-menu' }),'-world-state-client','127.0.0.1',"$port"
    $report = Join-Path $gameRoot "ScreenShots\$OutputPrefix-Client\DebugRun.log"
    do {
        Check-Deadline
        if ($client.HasExited) { throw 'Client exited before server shutdown.' }
        if ((Read-Log $report) -match '(?m)^simulation_update=60 ') { break }
        Start-Sleep -Milliseconds 200
    } while ($true)
    $server.StandardInput.WriteLine('quit'); $server.StandardInput.Flush()
    if ($CheckReconnect) {
        while (!$server.HasExited -or (Read-Log $report) -notmatch '(?m)^disconnected_server_menu=passed') {
            Check-Deadline
            if ($client.HasExited) { throw 'Client exited before reconnect.' }
            Start-Sleep -Milliseconds 200
        }
        $server.WaitForExit(); $server.Refresh()
        if ($server.ExitCode -ne 0) { throw 'Initial server shutdown failed.' }
        Remove-Item -LiteralPath $serverLog
        $server.StartInfo.Arguments = '--listen 127.0.0.1 --port ' + $port + ' --players 2 --activity-class GAScripted --activity "Skirmish Defense" --scene "Ketanot Hills" --log-directory "' + $hostLogs + '"'
        if (!$server.Start()) { throw 'Server restart failed.' }
        do {
            Check-Deadline
            if ($server.HasExited -or $client.HasExited) { throw 'A process exited before restart readiness.' }
            if ((Read-Log $serverLog) -match 'INFO ready port=(\d+) video_initialized=0') { break }
            Start-Sleep -Milliseconds 200
        } while ($true)
        $server.StandardInput.WriteLine('debug-inventory-create'); $server.StandardInput.Flush()
        while ((Read-Log $serverLog) -notmatch 'INFO debug_inventory=create_passed') {
            Check-Deadline
            if ($server.HasExited -or $client.HasExited -or (Read-Log $serverLog) -match 'ERROR debug_inventory=create_failed') { throw 'Restarted server fixture failed.' }
            Start-Sleep -Milliseconds 200
        }
        [IO.File]::WriteAllText((Join-Path $clientLogs 'Reconnect.ready'),'ready')
        while (!$client.HasExited) { Check-Deadline; if ($server.HasExited) { throw 'Restarted server stopped early.' }; Start-Sleep -Milliseconds 200 }
        $server.StandardInput.WriteLine('quit'); $server.StandardInput.Flush()
    }
    while (!$client.HasExited -or !$server.HasExited) { Check-Deadline; Start-Sleep -Milliseconds 200 }
    $client.WaitForExit(); $client.Refresh(); $server.WaitForExit(); $server.Refresh()
    if ($client.ExitCode -ne 0 -or $server.ExitCode -ne 0) { throw 'A process failed.' }
    $text = Read-Log $report
    $network = Read-Log (Join-Path $gameRoot "ScreenShots\$OutputPrefix-Client\WorldStateClient.log")
    if ($text -notmatch '(?m)^disconnected_server_menu=passed' -or $text -notmatch '(?m)^result=passed' -or
        $network -notmatch '(?m)^client_local_simulation_ticks=0' -or
        !(Test-Path -LiteralPath (Join-Path $gameRoot "ScreenShots\$OutputPrefix-Client\connection-lost-menu.png"))) { throw 'Disconnect menu checks failed.' }
    if ($CheckReconnect -and ($text -notmatch '(?m)^world_state_menu_reconnect=passed' -or $network -notmatch '(?m)^connection_source=main_menu' -or $network -notmatch '(?m)^scene=Ketanot Hills' -or $network -notmatch 'inventory_matches=1' -or $network -match 'inventory_matches=0' -or $network -notmatch 'attachment_states_checked=[1-9]' -or $network -match 'attachment_mismatches=[1-9]')) { throw 'Restarted-world menu reconnect failed.' }
    [pscustomobject]@{ Result='passed'; Reconnected=[bool]$CheckReconnect; Port=$port; ElapsedSeconds=[Math]::Round($watch.Elapsed.TotalSeconds,2) }
} finally {
    foreach ($process in @($client,$server)) { if ($process) { if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }; $process.Dispose() } }
}
