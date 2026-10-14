[CmdletBinding()]
param(
    [string]$ServerExecutablePath,
    [string]$ClientExecutablePath,
    [string]$GameDirectory,
    [ValidatePattern('^[^"\r\n]+$')]
    [string]$ActivityClass = 'GATutorial',
    [ValidatePattern('^[^"\r\n]+$')]
    [string]$ActivityName = 'Tutorial Mission',
    [ValidatePattern('^[^"\r\n]+$')]
    [string]$SceneName = 'Tutorial Bunker',
    [ValidateRange(0, 2)]
    [int]$RequireActivePlayers = 0,
    [switch]$RequireAttachmentMatch,
    [switch]$RestartWithConnectedPlayers,
    [switch]$RequireControlledActor,
    [switch]$CheckLatePlayerJoin,
    [switch]$CheckConstruction,
    [ValidatePattern('^[A-Za-z0-9_-]{1,48}$')]
    [string]$OutputPrefix = 'DedicatedServerSmoke',
    [switch]$CheckTeamFunds,
    [switch]$CheckPurchases,
    [switch]$CheckBuyMenu,
    [switch]$CheckPurchasePolicy,
    [switch]$CheckPurchasePricing,
    [switch]$CheckInventory,
    [switch]$CheckReloadHud,
    [switch]$CheckRecentShot,
    [switch]$CheckUnexpectedDisconnect,
    [switch]$CheckMenuConnection,
    [switch]$CheckScreenText,
    [switch]$CheckObjectives,
    [switch]$CheckSound,
    [switch]$CheckSoundMix,
    [switch]$CheckLateJoinSound,
    [switch]$CheckStaggeredSound,
    [switch]$CheckFinishLoops,
    [switch]$CheckFinishingLateJoin,
    [switch]$CheckFadingLateJoin,
    [switch]$EnableAudio,
    [ValidateRange(60, 600)]
    [int]$TimeoutSeconds = 360
)
$ErrorActionPreference = 'Stop'
if ($CheckConstruction) {
    if ($RestartWithConnectedPlayers -or $CheckLatePlayerJoin) { throw 'Run construction as a separate two-player editing fixture.' }
    $RequireControlledActor = $true; $RequireActivePlayers = 2
}
if ($CheckLatePlayerJoin) {
    if ($RestartWithConnectedPlayers) { throw 'Late player admission must be checked without restart.' }
    $RequireControlledActor = $true
    $RequireActivePlayers = 2
}
if ($CheckFadingLateJoin) {
    if ($CheckFinishLoops -or $CheckFinishingLateJoin) { throw 'Run fading and finishing fixtures separately.' }
    $CheckStaggeredSound=$true; $CheckSound=$true
}
if ($CheckFinishingLateJoin) { $CheckStaggeredSound=$true; $CheckFinishLoops=$true }
if ($CheckFinishLoops) { $CheckSound=$true }
if ($CheckFinishLoops -and $CheckSoundMix) { throw 'Run loop completion separately from mix changes.' }
if ($CheckSoundMix) { $CheckSound=$true }
if ($CheckLateJoinSound) { $CheckSound=$true }
if ($CheckStaggeredSound) { $CheckSound=$true }
if ($CheckStaggeredSound -and ($CheckLateJoinSound -or $CheckSoundMix)) { throw 'Run staggered sound independently from other sound fixtures.' }
if ($CheckLateJoinSound -and $CheckSoundMix) { throw 'Run late-join and mix-change fixtures separately.' }
if ($CheckPurchasePricing) { $CheckPurchasePolicy=$true }
if ($CheckPurchasePolicy) { $CheckBuyMenu=$true }
if ($CheckPurchases -and $CheckTeamFunds) { throw 'Run purchase and administrative funds fixtures separately.' }
if ($CheckBuyMenu -and ($CheckPurchases -or $CheckTeamFunds)) { throw 'Run the buy-menu fixture separately.' }
if ($CheckInventory -and ($CheckBuyMenu -or $CheckPurchases -or $CheckTeamFunds)) { throw 'Run the inventory fixture separately.' }
if ($CheckReloadHud -and ($CheckInventory -or $CheckBuyMenu -or $CheckPurchases -or $CheckTeamFunds)) { throw 'Run reload HUD independently from other gameplay fixtures.' }
if ($CheckRecentShot -and ($CheckReloadHud -or $CheckInventory -or $CheckBuyMenu -or $CheckPurchases -or $CheckTeamFunds)) { throw 'Run recent-shot fixture independently.' }
if ($CheckUnexpectedDisconnect -and ($CheckRecentShot -or $CheckReloadHud -or $CheckInventory -or $CheckBuyMenu -or $CheckPurchases -or $CheckTeamFunds -or $CheckSound)) { throw 'Run unexpected-disconnect fixture independently.' }
if (!$ServerExecutablePath) { $ServerExecutablePath = Join-Path $PSScriptRoot '..\CortexCommandServer.exe' }
if (!$ClientExecutablePath) { $ClientExecutablePath = Join-Path $PSScriptRoot '..\Cortex Command.exe' }
$serverBinary = (Resolve-Path -LiteralPath $ServerExecutablePath).Path
$clientBinary = (Resolve-Path -LiteralPath $ClientExecutablePath).Path
if (!$GameDirectory) { $GameDirectory = Split-Path -Parent $serverBinary }
$gameDirectory = (Resolve-Path -LiteralPath $GameDirectory).Path
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
function Start-PausedSoundFixture {
    $server.StandardInput.WriteLine('debug-sound'); $server.StandardInput.Flush()
    while ((Read-Report $serverLogPath) -notmatch 'INFO debug_sound=playing') {
        Check-Time; if ($server.HasExited) { throw 'Server exited before late-join sound.' }
        Start-Sleep -Milliseconds 100
    }
    Start-Sleep -Milliseconds 100
    $server.StandardInput.WriteLine('debug-sound-mix-pause'); $server.StandardInput.Flush()
    while ((Read-Report $serverLogPath) -notmatch 'INFO debug_sound_mix=paused') {
        Check-Time; if ($server.HasExited) { throw 'Server exited before late-join pause.' }
        Start-Sleep -Milliseconds 100
    }
}
foreach ($directory in @((Join-Path $gameDirectory 'ScreenShots'), $logDirectory)) {
    if ((Test-Path -LiteralPath $directory) -and
        ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Reparse diagnostic path.' }
}
foreach ($index in 1..2) {
    # Client startup also clears its output, but readiness must not read the
    # preceding run's reports while the new process is still initializing.
    $clientDirectory = [IO.Path]::GetFullPath((Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index"))
    $screenshotsRoot = [IO.Path]::GetFullPath((Join-Path $gameDirectory 'ScreenShots')) + [IO.Path]::DirectorySeparatorChar
    if (!$clientDirectory.StartsWith($screenshotsRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Diagnostic client path escaped screenshots root.' }
    if (Test-Path -LiteralPath $clientDirectory) {
        if ((Get-Item -LiteralPath $clientDirectory).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse client diagnostic path.' }
        foreach ($name in @('DebugRun.log','WorldStateClient.log')) {
            $reportPath = Join-Path $clientDirectory $name
            if (Test-Path -LiteralPath $reportPath) { Remove-Item -LiteralPath $reportPath }
        }
    }
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
    $server.StartInfo.Arguments += ' --activity-class "' + $ActivityClass + '" --activity "' + $ActivityName + '" --scene "' + $SceneName + '"'
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
    if ($CheckPurchases -or $CheckBuyMenu) {
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
    if ($CheckLateJoinSound) {
        Start-PausedSoundFixture
    }
    if ($CheckScreenText) { $server.StandardInput.WriteLine('debug-screen-text'); $server.StandardInput.Flush() }
    if ($CheckObjectives) { $server.StandardInput.WriteLine('debug-objectives'); $server.StandardInput.Flush() }
    $audioArguments = @(); if ($EnableAudio) { $audioArguments += '-debug-run-audio' }
    $clients = [Collections.Generic.List[Diagnostics.Process]]::new()
    $displayedStockPriceObserved = $false
    foreach ($index in 1..2) {
		$purchaseArguments = @()
		if ($CheckPurchases) { $purchaseArguments = @('-debug-run-world-state-purchase') }
		if ($CheckBuyMenu) { $purchaseArguments = @('-debug-run-world-state-buy-menu') }
		if ($CheckPurchasePolicy) { $purchaseArguments = @('-debug-run-world-state-purchase-policy') }
		if ($CheckPurchasePricing) { $purchaseArguments = @('-debug-run-world-state-purchase-pricing') }
		if ($CheckInventory) { $purchaseArguments = @('-debug-run-world-state-inventory') }
        if ($CheckConstruction) { $purchaseArguments = @('-debug-run-world-state-construction') }
        $frameCount = if ($CheckStaggeredSound -and $index -eq 1) { '6000' } else { '600' }
        if ($CheckConstruction) { $frameCount = '1800' }
        if ($CheckLatePlayerJoin -and $index -eq 1) { $frameCount = '1800' }
        $client = Start-Process -FilePath $clientBinary -WorkingDirectory $gameDirectory -WindowStyle Hidden -PassThru -ArgumentList (@(
            '-debug-run',$frameCount,'-debug-run-output',"$OutputPrefix-Client$index",$(if ($CheckMenuConnection) { '-debug-run-menu-connect' } else { '-world-state-client' }),'127.0.0.1',"$port"
        )+$purchaseArguments+$audioArguments)
        $processes.Add($client)
        $clients.Add($client)
        if ($CheckLatePlayerJoin -and $index -eq 1) {
            $firstNetwork = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client1\WorldStateClient.log"
            while ((Read-Report $firstNetwork) -notmatch '(?m)^snapshot=.*scene_revision=1 .*controlled_actor=[1-9]') {
                Check-Time; if ($client.HasExited -or $server.HasExited) { throw 'First player never became controllable before the late join.' }
                Start-Sleep -Milliseconds 100
            }
        }
        if ($CheckStaggeredSound -and $index -eq 1) {
            $firstNetwork = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client1\WorldStateClient.log"
            while ((Read-Report $firstNetwork) -notmatch '(?m)^snapshot=') {
                Check-Time; if ($client.HasExited -or $server.HasExited) { throw 'First peer exited before its world loaded.' }
                Start-Sleep -Milliseconds 100
            }
            Start-PausedSoundFixture
            $firstReport = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client1\DebugRun.log"
            while ((Read-Report $firstReport) -notmatch '(?m)^world_state_sound_bus_pause=1') {
                Check-Time; if ($client.HasExited -or $server.HasExited) { throw 'First peer exited before sound pause.' }
                Start-Sleep -Milliseconds 100
            }
            if ($CheckFinishingLateJoin) {
                $server.StandardInput.WriteLine('debug-sound-finish'); $server.StandardInput.Flush()
                while ((Read-Report $firstReport) -notmatch '(?m)^world_state_sound_finish_received=passed') {
                    Check-Time; if ($client.HasExited -or $server.HasExited) { throw 'First peer exited before loop finish.' }
                    Start-Sleep -Milliseconds 100
                }
            }
            if ($CheckFadingLateJoin) {
                $server.StandardInput.WriteLine('debug-sound-mix-resume'); $server.StandardInput.WriteLine('debug-sound-fade'); $server.StandardInput.Flush()
                while ((Read-Report $firstReport) -notmatch '(?m)^world_state_sound_fade_midpoint=passed') {
                    Check-Time; if ($client.HasExited -or $server.HasExited) { throw 'First peer exited before fading midpoint.' }
                    Start-Sleep -Milliseconds 100
                }
                $server.StandardInput.WriteLine('debug-sound-mix-pause'); $server.StandardInput.Flush()
                while ((Read-Report $firstReport) -notmatch '(?m)^world_state_sound_fade_frozen=passed') {
                    Check-Time; if ($client.HasExited -or $server.HasExited) { throw 'First peer exited before fade freeze.' }
                    Start-Sleep -Milliseconds 100
                }
            }
        }
    }
    if ($CheckUnexpectedDisconnect) {
        foreach ($index in 1..2) {
            $networkReport=Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\WorldStateClient.log"
            while ((Read-Report $networkReport) -notmatch '(?m)^snapshot=') {
                Check-Time
                if ($clients[$index-1].HasExited -or $server.HasExited) { throw 'Peer exited before disconnect fixture was ready.' }
                Start-Sleep -Milliseconds 100
            }
        }
        $server.StandardInput.WriteLine('quit'); $server.StandardInput.Flush()
        while (!$server.HasExited) { Check-Time; Start-Sleep -Milliseconds 100 }
        if ($server.ExitCode -ne 0) { throw 'Disconnect fixture server shutdown failed.' }
        foreach ($index in 1..2) {
            $client=$clients[$index-1]
            while (!$client.HasExited) { Check-Time; Start-Sleep -Milliseconds 100 }
            $report=Read-Report (Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\DebugRun.log")
            if ($client.ExitCode -ne 1 -or $report -notmatch '(?m)^unexpected_network_disconnect=' -or $report -notmatch '(?m)^result=failed') {
                throw 'Debug client did not report and exit after unexpected disconnect.'
            }
        }
        [pscustomobject]@{Result='passed'; Clients=2; ExpectedClientExitCode=1; ServerExitCode=0; ElapsedSeconds=[Math]::Round($timer.Elapsed.TotalSeconds,2)}
        return
    }
    $mixPaused = $false; $mixResumed = $false
    $soundStarted = [bool]($CheckLateJoinSound -or $CheckStaggeredSound)
    $soundStopped = $false
    $fadeVolumeChanged = $false
    $screenTextApplied = $false
    $screenTextFixtureTimer = [Diagnostics.Stopwatch]::StartNew()
    $fundsChanged = $false
    $policyChanged = $false
    $inventoryMutated = $false
    $inventoryCreated = $false
    $reloadHudCreated = $false; $reloadHudChanged = $false
    $shotCreated = $false; $shotFired = $false
    $objectiveFixtureCleared = $false
    $activityRestarted = $false
    while (@($clients | Where-Object { !$_.HasExited }).Count -gt 0) {
        if ($RestartWithConnectedPlayers -and !$activityRestarted) {
            $rosterReady = $true
            foreach ($index in 1..2) {
                $rosterReport = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\WorldStateClient.log"
                $rosterReady = $rosterReady -and ((Read-Report $rosterReport) -match '(?m)^snapshot=')
            }
            if ($rosterReady) { $server.StandardInput.WriteLine('restart'); $server.StandardInput.Flush(); $activityRestarted = $true }
        }
        if ($CheckScreenText) {
            # Activity updates may clear ordinary messages while clients load.
            # Refresh only this disposable fixture until the GUI check completes.
            if ($screenTextFixtureTimer.ElapsedMilliseconds -ge 500) {
                $server.StandardInput.WriteLine('debug-screen-text'); $server.StandardInput.Flush()
                $screenTextFixtureTimer.Restart()
            }
            foreach ($index in 1..2) {
                $screenTextReport = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\DebugRun.log"
                $screenTextApplied = $screenTextApplied -or (Read-Report $screenTextReport) -match '(?m)^world_state_screen_text_received=passed'
            }
        }
        if ($CheckObjectives -and !$objectiveFixtureCleared) {
            $objectiveStressCaptured = $true
            foreach ($index in 1..2) {
                $objectiveDirectory = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index"
                $objectiveStressCaptured = $objectiveStressCaptured -and (Test-Path (Join-Path $objectiveDirectory 'debug-run-stress.png')) -and
                    ((Read-Report (Join-Path $objectiveDirectory 'DebugRun.log')) -match '(?m)^world_state_objectives_received=passed')
            }
            if ($objectiveStressCaptured) {
                $server.StandardInput.WriteLine('debug-objectives-clear'); $server.StandardInput.Flush()
                $objectiveFixtureCleared = $true
            }
        }
        if ($CheckSound) {
            $soundReady = $true; $soundPlayed = $true
            foreach ($index in 1..2) {
                $clientOutput = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index"
                $soundReady = $soundReady -and ((Read-Report (Join-Path $clientOutput 'WorldStateClient.log')) -match '(?m)^snapshot=')
                $soundPlayed = $soundPlayed -and ((Read-Report (Join-Path $clientOutput 'DebugRun.log')) -match '(?m)^world_state_sound_play=passed')
            }
            if (!$soundStarted -and $soundReady) {
                $server.StandardInput.WriteLine('debug-sound'); $server.StandardInput.Flush(); $soundStarted=$true
            }
            $mixReadyToStop = !$CheckSoundMix
            if ($CheckSoundMix -and $soundPlayed) {
                $pausedSeen = $true; $resumedSeen = $true
                foreach ($index in 1..2) {
                    $mixReport = Read-Report (Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\DebugRun.log")
                    $pausedSeen = $pausedSeen -and ($mixReport -match '(?m)^world_state_sound_bus_pause=1')
                    $resumedSeen = $resumedSeen -and ($mixReport -match '(?s)world_state_sound_bus_pause=1.*world_state_sound_bus_pause=0')
                }
                if (!$mixPaused) { $server.StandardInput.WriteLine('debug-sound-mix-pause'); $server.StandardInput.Flush(); $mixPaused=$true }
                elseif (!$mixResumed -and $pausedSeen) { $server.StandardInput.WriteLine('debug-sound-mix-resume'); $server.StandardInput.Flush(); $mixResumed=$true }
                $mixReadyToStop = $mixResumed -and $resumedSeen
            }
            if ($soundStarted -and !$soundStopped -and $soundPlayed -and $mixReadyToStop) {
                if ($CheckStaggeredSound -and $clients[0].HasExited) { throw 'Existing peer exited before the later peer was ready.' }
                $completionIssued = $true
                if ($CheckFadingLateJoin) {
                    $frozenSeen=$true; $volumeSeen=$true
                    foreach ($index in 1..2) {
                        $fadeReport = Read-Report (Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\DebugRun.log")
                        $frozenSeen=$frozenSeen -and ($fadeReport -match '(?m)^world_state_sound_fade_frozen=passed')
                        $volumeSeen=$volumeSeen -and ($fadeReport -match '(?m)^world_state_sound_fade_volume=passed')
                    }
                    if (!$fadeVolumeChanged -and $frozenSeen) { $server.StandardInput.WriteLine('debug-sound-volume-half'); $server.StandardInput.Flush(); $fadeVolumeChanged=$true }
                    if ($fadeVolumeChanged -and $volumeSeen) { $server.StandardInput.WriteLine('debug-sound-mix-resume') }
                    else { $completionIssued=$false }
                } elseif ($CheckFinishLoops) {
                    $server.StandardInput.WriteLine('debug-sound-mix-resume')
                    if (!$CheckFinishingLateJoin) { $server.StandardInput.WriteLine('debug-sound-finish') }
                } else { $server.StandardInput.WriteLine('debug-sound-stop') }
                if ($completionIssued) { $server.StandardInput.Flush(); $soundStopped=$true }
            }
        }
        Check-Time
        if ($server.HasExited) { throw 'Server stopped while clients were running.' }
        if ($CheckRecentShot) {
            $shotClientsReady=$true; $shotActorReady=$false
            foreach ($index in 1..2) {
                $shotDirectory=Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index"
                $shotClientsReady=$shotClientsReady -and ((Read-Report (Join-Path $shotDirectory 'WorldStateClient.log')) -match '(?m)^snapshot=')
                $shotActorReady=$shotActorReady -or ((Read-Report (Join-Path $shotDirectory 'DebugRun.log')) -match '(?m)^world_state_shot_actor_ready=passed')
            }
            if (!$shotCreated -and $shotClientsReady) { $server.StandardInput.WriteLine('debug-shot-create'); $server.StandardInput.Flush(); $shotCreated=$true }
            if ($shotCreated -and !$shotFired -and $shotActorReady) { $server.StandardInput.WriteLine('debug-shot-fire'); $server.StandardInput.Flush(); $shotFired=$true }
        }
        if ($CheckReloadHud) {
            $hudClientsReady=$true; $hudInitial=$false
            foreach ($index in 1..2) {
                $hudDirectory=Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index"
                $hudClientsReady=$hudClientsReady -and ((Read-Report (Join-Path $hudDirectory 'WorldStateClient.log')) -match '(?m)^snapshot=')
                $hudInitial=$hudInitial -or ((Read-Report (Join-Path $hudDirectory 'DebugRun.log')) -match '(?m)^world_state_reload_hud_initial=passed')
            }
            if (!$reloadHudCreated -and $hudClientsReady) { $server.StandardInput.WriteLine('debug-reload-hud-create'); $server.StandardInput.Flush(); $reloadHudCreated=$true }
            if ($reloadHudCreated -and !$reloadHudChanged -and $hudInitial) { $server.StandardInput.WriteLine('debug-reload-hud-mutate'); $server.StandardInput.Flush(); $reloadHudChanged=$true }
        }
        if ($CheckInventory -and !$inventoryCreated) {
            $inventoryClientsReady=$true
            foreach ($index in 1..2) {
                $inventoryNetwork=Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\WorldStateClient.log"
                $inventoryClientsReady=$inventoryClientsReady -and (Read-Report $inventoryNetwork) -match '(?m)^snapshot='
            }
            if ($inventoryClientsReady) { $server.StandardInput.WriteLine('debug-inventory-create'); $server.StandardInput.Flush(); $inventoryCreated=$true }
        }
        if ($CheckInventory -and $inventoryCreated -and !$inventoryMutated) {
            $initialInventorySeen=$true
            foreach ($index in 1..2) {
                $inventoryReport=Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\DebugRun.log"
                $initialInventorySeen=$initialInventorySeen -and (Read-Report $inventoryReport) -match '(?m)^world_state_inventory_initial=passed'
            }
            if ($initialInventorySeen) { $server.StandardInput.WriteLine('debug-inventory-mutate'); $server.StandardInput.Flush(); $inventoryMutated=$true }
        }
        if ($CheckPurchasePolicy -and !$policyChanged) {
            $catalogReady=$false
            foreach ($index in 1..2) {
                $reportPath=Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\DebugRun.log"
                $catalogReady=$catalogReady -or (Read-Report $reportPath) -match 'debug-run-buy-catalog\.png result=0'
            }
            if ($catalogReady) {
                $policyCommand = if ($CheckPurchasePricing) { 'debug-purchase-pricing' } else { 'debug-purchase-policy' }
                $server.StandardInput.WriteLine($policyCommand); $server.StandardInput.Flush(); $policyChanged=$true
            }
        }
        if ($CheckTeamFunds -and !$fundsChanged) {
            $clientsReady = $true
            foreach ($index in 1..2) {
                $networkPath = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\WorldStateClient.log"
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
    if ($CheckScreenText) {
        foreach ($index in 1..2) {
            $report = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index\DebugRun.log"
            $screenTextApplied = $screenTextApplied -or (Read-Report $report) -match '(?m)^world_state_screen_text_received=passed'
        }
        if (!$screenTextApplied) { throw 'No assigned client applied the server screen text.' }
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
    if ($RestartWithConnectedPlayers -and (!$activityRestarted -or (Read-Report $serverLogPath) -notmatch 'INFO activity_restart=completed humans=2')) { throw 'Connected roster restart did not complete.' }
    $activeSlots = @([regex]::Matches($networkLog, 'client assignment updated for .* to player slot (\d+)') | ForEach-Object { [int]$_.Groups[1].Value } | Sort-Object -Unique)
    if ($activeSlots.Count -lt $RequireActivePlayers) { throw 'Not enough distinct active server player assignments.' }
    $expectedPolicyCost = if ($CheckPurchasePricing) { 798 } else { 324 }
    $controlledActorIDs = [Collections.Generic.List[string]]::new()
    if ($CheckPurchasePolicy -and (!$policyChanged -or $networkLog -notmatch "purchase id=1 accepted=1 charged=$expectedPolicyCost\.000000")) { throw 'Server stock-price policy checks failed.' }
    if ($CheckBuyMenu -and ([regex]::Matches($networkLog, 'purchase id=\d+ accepted=1').Count -ne 1)) { throw 'Expected exactly one accepted menu purchase.' }
    if ($CheckPurchases -and ($networkLog -notmatch 'purchase id=3 accepted=1 charged=500\.000000' -or
        $networkLog -notmatch 'purchase replay id=3' -or ([regex]::Matches($networkLog, 'purchase id=3 accepted=1').Count -ne 1))) { throw 'Purchase/replay server checks failed.' }
    if ($networkLog -notmatch '\(2 connected, input slot') { throw 'Two simultaneous connections not observed.' }
    $constructionPurchased = $false
    foreach ($index in 1..2) {
        $clientDirectory = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index"
        $clientReport = Read-Report (Join-Path $clientDirectory 'DebugRun.log')
        if ($CheckObjectives -and (!$objectiveFixtureCleared -or $clientReport -notmatch '(?m)^world_state_objectives_received=passed' -or
            $clientReport -notmatch '(?m)^world_state_objectives_cleared=passed')) { throw 'Client objective create/clear checks failed.' }
        if ($EnableAudio -and $clientReport -notmatch '(?m)^audio_shutdown_complete\r?$') { throw 'Client audio shutdown was not verified.' }
        $clientNetwork = Read-Report (Join-Path $clientDirectory 'WorldStateClient.log')
        if ($CheckConstruction -and ($clientReport -notmatch '(?m)^world_state_construction_ui=passed' -or $clientReport -notmatch '(?m)^construction_preview_capture=passed' -or $clientReport -notmatch '(?m)^construction_catalog_capture=passed' -or $clientReport -notmatch '(?m)^construction_object_ui=passed')) { throw 'Construction UI/preview/catalog/purchase/start did not pass.' }
        if ($CheckConstruction -and $clientReport -notmatch '(?m)^construction_object_preview_capture=passed') { throw 'Object preview was not captured.' }
        $constructionPurchased = $constructionPurchased -or $clientReport -match '(?m)^construction_object_ui=passed purchased=1'
        if ($RequireControlledActor -and $clientNetwork -notmatch 'controlled_actor=[1-9]') { throw 'Client never received a controlled actor.' }
        if ($CheckLatePlayerJoin -and ($clientNetwork -notmatch 'scene_revision=1 .*controlled_actor=[1-9]' -or $clientNetwork -match 'scene_revision=([2-9]|[1-9][0-9]+) ')) { throw 'Late admission did not preserve the running scene revision.' }
        if ($RequireControlledActor -and $RestartWithConnectedPlayers -and $clientNetwork -notmatch 'scene_revision=[2-9].*controlled_actor=[1-9]') { throw 'Client has no controlled actor after roster restart.' }
        if ($RequireControlledActor) {
            $controlPattern = if ($RestartWithConnectedPlayers) { 'scene_revision=[2-9].*?controlled_actor=([1-9][0-9]*)' } else { 'controlled_actor=([1-9][0-9]*)' }
            $controlMatches = [regex]::Matches($clientNetwork, $controlPattern)
            $controlledActorIDs.Add($controlMatches[$controlMatches.Count - 1].Groups[1].Value)
        }
        if ($RestartWithConnectedPlayers -and $clientNetwork -notmatch 'scene_revision=[2-9]') { throw 'Client did not apply the restarted scene.' }
        if ($clientNetwork -notmatch ('(?m)^activity=' + [regex]::Escape($ActivityName) + '\r?$') -or
            $clientNetwork -notmatch ('(?m)^scene=' + [regex]::Escape($SceneName) + '\r?$')) { throw 'Client loaded a different Activity or scene.' }
        if ($CheckMenuConnection -and ($clientReport -notmatch '(?m)^menu_connection=passed' -or
            $clientReport -notmatch '(?m)^menu_validation_and_cancel=passed' -or
            $clientNetwork -notmatch '(?m)^connection_source=main_menu' -or
            !(Test-Path (Join-Path $clientDirectory 'main-menu.png')) -or
            !(Test-Path (Join-Path $clientDirectory 'multiplayer-menu.png')))) { throw 'Menu connection checks failed.' }
        if ($CheckPurchases -and $clientReport -notmatch '(?m)^world_state_purchase=passed') { throw 'Client purchase checks failed.' }
        if ($CheckBuyMenu -and $clientReport -notmatch '(?m)^world_state_buy_menu=passed') { throw 'Client buy menu checks failed.' }
        if ($CheckPurchasePolicy -and $clientReport -notmatch '(?m)^world_state_purchase_policy=passed') { throw 'Client live policy checks failed.' }
        if ($CheckInventory -and (!$inventoryMutated -or $clientReport -notmatch '(?m)^world_state_inventory=passed' -or $clientNetwork -match 'inventory_matches=0')) { throw 'Client inventory hierarchy/reuse checks failed.' }
        if ($CheckInventory -and ($clientNetwork -notmatch 'attachment_states_checked=[1-9][0-9]*' -or $clientNetwork -match 'attachment_mismatches=[1-9]')) { throw 'Client native attachment/pose checks failed.' }
        if ($clientReport -match "(?m)^buy_menu_displayed_cost=$expectedPolicyCost\r?`$") { $displayedStockPriceObserved = $true }
        if ($clientReport -notmatch '(?m)^result=passed' -or $clientNetwork -notmatch '(?m)^client_local_simulation_ticks=0' -or
            $clientNetwork -notmatch '(?m)^snapshot=\d+.*missing_presets=0' -or $clientNetwork -match 'missing_presets=[1-9]') { throw 'Client replica checks failed.' }
        if ($RequireAttachmentMatch -and ($clientNetwork -notmatch 'attachment_states_checked=[1-9]' -or $clientNetwork -match 'attachment_mismatches=[1-9]')) { throw 'Native attachment pose mismatch observed.' }
        if ($CheckSoundMix -and ($clientReport -notmatch '(?m)^world_state_sound_bus_pause=1' -or
            $clientReport -notmatch '(?s)world_state_sound_bus_pause=1.*world_state_sound_bus_pause=0')) { throw 'Network sound group pause/resume failed.' }
        if ($CheckSound -and (!$soundStarted -or !$soundStopped -or $clientReport -notmatch '(?m)^world_state_sound_play=passed' -or
            (!$CheckFinishLoops -and !$CheckFadingLateJoin -and $clientReport -notmatch '(?m)^world_state_sound_stop=passed'))) { throw 'Network sound playback/lifecycle fixture failed.' }
        if ($CheckFadingLateJoin -and (!$fadeVolumeChanged -or $clientReport -notmatch '(?m)^world_state_sound_fade_frozen=passed' -or
            $clientReport -notmatch '(?m)^world_state_sound_fade_volume=passed' -or $clientReport -notmatch '(?m)^world_state_sound_fade_completed=passed' -or
            $clientReport -match '(?m)^world_state_sound_stop=passed')) { throw 'Fade did not freeze, preserve volume controls or naturally expire.' }
        if ($CheckFadingLateJoin -and $index -eq 2 -and $clientReport -notmatch '(?m)^world_state_sound_fade_restored=passed') { throw 'Late-join fade gain was not restored.' }
        if ($CheckFinishLoops -and ($clientReport -notmatch '(?m)^world_state_sound_finish_received=passed' -or
            $clientReport -notmatch '(?m)^world_state_sound_finish_completed=passed' -or $clientReport -match '(?m)^world_state_sound_stop=passed')) { throw 'Loop did not finish its cycle without Stop.' }
        if ($CheckFinishingLateJoin -and $index -eq 2 -and $clientReport -notmatch '(?m)^world_state_sound_finish_received=passed source=baseline') { throw 'Finishing state missing from late-join baseline.' }
        if ($EnableAudio -and $CheckSound -and ($clientReport -notmatch '(?m)^world_state_sound_play=passed audio_enabled=1' -or $clientReport -notmatch '(?m)^audio_owner_lifetime=passed')) { throw 'FMOD playback was not enabled.' }
        if ($CheckLateJoinSound -and $clientReport -notmatch '(?m)^world_state_sound_late_join=passed cursor_ms=[1-9][0-9]* paused=1') { throw 'Client did not restore the paused pre-existing loop.' }
        if ($CheckStaggeredSound) {
            if ([regex]::Matches($clientReport, '(?m)^world_state_sound_fixture_event ').Count -ne 1) { throw 'Persistent fixture Play was repeated or missing.' }
            if ($index -eq 1 -and ($clientReport -match '(?m)^world_state_sound_late_join=' -or $clientReport -notmatch '(?m)^world_state_sound_bus_pause=1')) { throw 'Existing peer sound state changed unexpectedly.' }
            if ($index -eq 2 -and $clientReport -notmatch '(?m)^world_state_sound_late_join=passed cursor_ms=[1-9][0-9]* paused=1') { throw 'Second peer did not restore the pre-existing loop.' }
        }
        if ($CheckTeamFunds -and (!$fundsChanged -or $clientReport -notmatch '(?m)^world_state_team_funds_presentation=passed' -or
            $clientNetwork -notmatch 'funds_team0=12345\.250000 funds_team1=-17\.500000')) { throw 'Client team funds replication failed.' }
    }
    if ($RequireControlledActor -and @($controlledActorIDs | Sort-Object -Unique).Count -ne 2) { throw 'Clients share the same controlled actor.' }
    if ($CheckConstruction -and !$constructionPurchased) { throw 'No client completed a paid object placement.' }
    if ($CheckPurchasePolicy -and !$displayedStockPriceObserved) { throw 'Client catalog stock discount does not match the server charge.' }
    if ($CheckRecentShot) {
        $shotVerified=$false
        foreach ($index in 1..2) {
            $shotDirectory=Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index"
            $shotReport=Read-Report (Join-Path $shotDirectory 'DebugRun.log')
            $shotImage=Join-Path $shotDirectory 'debug-run-recent-shot.png'
            $expiredImage=Join-Path $shotDirectory 'debug-run-recent-shot-expired.png'
            if ((Test-Path $shotImage) -and (Test-Path $expiredImage) -and
                (Get-FileHash -LiteralPath $shotImage).Hash -eq (Get-FileHash -LiteralPath $expiredImage).Hash) { throw 'Recent-shot screenshots are identical; visual expiry was not captured.' }
            $shotVerified=$shotVerified -or ($shotReport -match '(?m)^world_state_recent_shot=passed' -and
                $shotReport -match '(?m)^world_state_recent_shot_expired=passed' -and
                (Test-Path (Join-Path $shotDirectory 'debug-run-recent-shot.png')) -and
                (Test-Path (Join-Path $shotDirectory 'debug-run-recent-shot-expired.png')))
        }
        if (!$shotFired -or !$shotVerified -or (Read-Report $serverLogPath) -notmatch 'INFO debug_shot=fired_between_snapshots') { throw 'Recent-shot visual fixture failed.' }
    }
    if ($CheckReloadHud) {
        $hudVerified=$false
        foreach ($index in 1..2) {
            $hudDirectory=Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index"
            $hudReport=Read-Report (Join-Path $hudDirectory 'DebugRun.log')
            $hudVerified=$hudVerified -or ($hudReport -match '(?m)^world_state_reload_hud_initial=passed' -and
                $hudReport -match '(?m)^world_state_reload_hud_changed=passed' -and
                (Test-Path (Join-Path $hudDirectory 'debug-run-reload-hud-35.png')) -and (Test-Path (Join-Path $hudDirectory 'debug-run-reload-hud-75.png')))
        }
        if (!$reloadHudCreated -or !$reloadHudChanged -or !$hudVerified) { throw 'Controlled player reload HUD fixture failed.' }
    }
    [pscustomobject]@{ Result='passed'; Clients=2; UpdatesAfterDisconnect=($latestTick-$baselineTick); Port=$port; ServerBinary=$serverBinary; ElapsedSeconds=[Math]::Round($timer.Elapsed.TotalSeconds,2) }
} finally {
    foreach ($process in $processes) {
        if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
}
