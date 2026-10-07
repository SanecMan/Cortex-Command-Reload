[CmdletBinding()]
param(
    [string]$ServerExecutablePath,
    [string]$ClientExecutablePath,
    [ValidatePattern('^[A-Za-z0-9_-]{1,48}$')]
    [string]$OutputPrefix = 'DedicatedServerSmoke',
    [switch]$CheckTeamFunds,
    [switch]$CheckPurchases,
    [ValidateRange(60, 600)]
    [int]$TimeoutSeconds = 360
)
$ErrorActionPreference = 'Stop'
if ($CheckPurchases -and $CheckTeamFunds) { throw 'Run purchase and administrative funds fixtures separately.' }
if (!$ServerExecutablePath) { $ServerExecutablePath = Join-Path $PSScriptRoot '..\CortexCommandServer.exe' }
if (!$ClientExecutablePath) { $ClientExecutablePath = Join-Path $PSScriptRoot '..\Cortex Command.exe' }
$serverBinary = (Resolve-Path -LiteralPath $ServerExecutablePath).Path
$clientBinary = (Resolve-Path -LiteralPath $ClientExecutablePath).Path
$gameDirectory = Split-Path -Parent $serverBinary
$logDirectory = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Host"
$timer = [Diagnostics.Stopwatch]::StartNew()
$processes = [Collections.Generic.List[Diagnostics.Process]]::new()
function Read-Report([string]$Path) {
    if (!(Test-Path -LiteralPath $Path)) { return '' }
    return Get-Content -LiteralPath $Path -Raw
}
function Check-Time {
    if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw 'Dedicated server smoke timeout.' }
}
foreach ($directory in @((Join-Path $gameDirectory 'ScreenShots'), $logDirectory)) {
    if ((Test-Path -LiteralPath $directory) -and
        ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Reparse diagnostic path.' }
}
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
foreach ($name in @('Server.log','ServerFailure.log','WorldStateServer.log','LogLoading.txt','LogConsole.txt','StdOut.log','StdErr.log')) {
    $path = Join-Path $logDirectory $name
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
}
try {
    $server = [Diagnostics.Process]::new()
    $server.StartInfo.FileName = $serverBinary
    $server.StartInfo.WorkingDirectory = $gameDirectory
    $server.StartInfo.Arguments = '--listen 127.0.0.1 --port 0 --players 4 --log-directory "' + $logDirectory + '"'
    $server.StartInfo.UseShellExecute = $false
    $server.StartInfo.CreateNoWindow = $true
    $server.StartInfo.RedirectStandardInput = $true
    $server.StartInfo.RedirectStandardOutput = $true
    $server.StartInfo.RedirectStandardError = $true
    $server.StartInfo.EnvironmentVariables['SDL_VIDEODRIVER'] = 'invalid-driver-for-headless-test'
    if (!$server.Start()) { throw 'Server process start failed.' }
    $processes.Add($server)
    $outputTask = $server.StandardOutput.ReadToEndAsync()
    $errorTask = $server.StandardError.ReadToEndAsync()
    $serverLogPath = Join-Path $logDirectory 'Server.log'
    while ($true) {
        Check-Time
        if ($server.HasExited) { throw 'Server exited before readiness.' }
        if ((Read-Report $serverLogPath) -match 'INFO ready port=(\d+) video_initialized=0') { $port = [int]$Matches[1]; break }
        Start-Sleep -Milliseconds 250
    }
    $server.Refresh()
    if ($server.MainWindowHandle -ne 0) { throw 'Server created a native window.' }
    if ($CheckPurchases) {
        # Tutorial begins with zero funds. Fund this disposable server fixture before clients sample it.
        $server.StandardInput.WriteLine('funds 0 10000')
        $server.StandardInput.Flush()
        while ((Read-Report $serverLogPath) -notmatch 'INFO funds team=0 value=10000\.000000') {
            Check-Time
            if ($server.HasExited) { throw 'Server exited while funding purchase fixture.' }
            Start-Sleep -Milliseconds 100
        }
    }
    Write-Host "Dedicated server ready on $port; connecting two separate clients."
    $clients = [Collections.Generic.List[Diagnostics.Process]]::new()
    foreach ($index in 1..2) {
		$purchaseArguments = @()
		if ($CheckPurchases) { $purchaseArguments = @('-debug-run-world-state-purchase') }
        $client = Start-Process -FilePath $clientBinary -WorkingDirectory (Split-Path -Parent $clientBinary) -WindowStyle Hidden -PassThru -ArgumentList (@(
            '-debug-run','600','-debug-run-output',"$OutputPrefix-Client$index",'-world-state-client','127.0.0.1',"$port"
        )+$purchaseArguments)
        $processes.Add($client)
        $clients.Add($client)
    }
    $fundsChanged = $false
    while (@($clients | Where-Object { !$_.HasExited }).Count -gt 0) {
        Check-Time
        if ($server.HasExited) { throw 'Server stopped while clients were running.' }
        if ($CheckTeamFunds -and !$fundsChanged) {
            $clientsReady = $true
            foreach ($index in 1..2) {
                $networkPath = Join-Path (Split-Path -Parent $clientBinary) "ScreenShots\$OutputPrefix-Client$index\WorldStateClient.log"
                $clientsReady = $clientsReady -and (Read-Report $networkPath) -match '(?m)^snapshot='
            }
            if ($clientsReady) {
                $server.StandardInput.WriteLine('funds 0 12345.25')
                $server.StandardInput.WriteLine('funds 1 -17.5')
                $server.StandardInput.Flush()
                $fundsChanged = $true
            }
        }
        Start-Sleep -Milliseconds 500
    }
    foreach ($client in $clients) {
        $client.WaitForExit(); $client.Refresh()
        if ($client.ExitCode -ne 0) { throw 'Client failed.' }
    }
    # Ask for status until disconnects have propagated, then prove ticking
    # continues for at least 60 more updates without connected clients.
    $baselineTick = -1
    while ($true) {
        Check-Time
        if ($server.HasExited) { throw 'Server stopped after client disconnect.' }
        $server.StandardInput.WriteLine('status')
        $server.StandardInput.Flush()
        Start-Sleep -Milliseconds 500
        $statuses = [regex]::Matches((Read-Report $serverLogPath), 'status tick=(\d+) clients=0')
        if ($statuses.Count -eq 0) { continue }
        $latestTick = [int]$statuses[$statuses.Count - 1].Groups[1].Value
        if ($baselineTick -lt 0) { $baselineTick = $latestTick }
        if ($latestTick - $baselineTick -ge 60) { break }
    }
    $server.StandardInput.WriteLine('quit')
    $server.StandardInput.Flush()
    while (!$server.HasExited) { Check-Time; Start-Sleep -Milliseconds 250 }
    $server.WaitForExit()
    if ($server.ExitCode -ne 0) { throw 'Server shutdown failed.' }
    [IO.File]::WriteAllText((Join-Path $logDirectory 'StdOut.log'), $outputTask.GetAwaiter().GetResult(), [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $logDirectory 'StdErr.log'), $errorTask.GetAwaiter().GetResult(), [Text.UTF8Encoding]::new($false))
    $networkLog = Read-Report (Join-Path $logDirectory 'WorldStateServer.log')
    if ($CheckPurchases -and ($networkLog -notmatch 'purchase id=3 accepted=1 charged=500\.000000' -or
        $networkLog -notmatch 'purchase replay id=3' -or ([regex]::Matches($networkLog, 'purchase id=3 accepted=1').Count -ne 1))) { throw 'Purchase/replay server checks failed.' }
    if ($networkLog -notmatch '\(2 connected, input slot') { throw 'Two simultaneous connections not observed.' }
    foreach ($index in 1..2) {
        $clientDirectory = Join-Path (Split-Path -Parent $clientBinary) "ScreenShots\$OutputPrefix-Client$index"
        $clientReport = Read-Report (Join-Path $clientDirectory 'DebugRun.log')
        $clientNetwork = Read-Report (Join-Path $clientDirectory 'WorldStateClient.log')
        if ($CheckPurchases -and $clientReport -notmatch '(?m)^world_state_purchase=passed') { throw 'Client purchase checks failed.' }
        if ($clientReport -notmatch '(?m)^result=passed' -or $clientNetwork -notmatch '(?m)^client_local_simulation_ticks=0' -or
            $clientNetwork -notmatch '(?m)^snapshot=\d+.*missing_presets=0' -or $clientNetwork -match 'missing_presets=[1-9]') { throw 'Client replica checks failed.' }
        if ($CheckTeamFunds -and (!$fundsChanged -or $clientReport -notmatch '(?m)^world_state_team_funds_presentation=passed' -or
            $clientNetwork -notmatch 'funds_team0=12345\.250000 funds_team1=-17\.500000')) { throw 'Client team funds replication failed.' }
    }
    [pscustomobject]@{ Result='passed'; Clients=2; UpdatesAfterDisconnect=($latestTick-$baselineTick); Port=$port; ServerBinary=$serverBinary; ElapsedSeconds=[Math]::Round($timer.Elapsed.TotalSeconds,2) }
} finally {
    foreach ($process in $processes) {
        if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
}
