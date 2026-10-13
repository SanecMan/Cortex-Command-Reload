[CmdletBinding()]
param(
    [string]$ServerExecutablePath,
    [string]$ClientExecutablePath,
    [string]$GameDirectory,
    [ValidatePattern('^[A-Za-z0-9_-]{1,48}$')]
    [string]$OutputPrefix = 'DedicatedServerSmoke',
    [switch]$CheckTeamFunds,
    [switch]$CheckPurchases,
    [switch]$CheckBuyMenu,
    [switch]$CheckPurchasePolicy,
    [switch]$CheckPurchasePricing,
    [switch]$CheckInventory,
    [switch]$CheckMenuConnection,
    [switch]$CheckScreenText,
    [ValidateRange(60, 600)]
    [int]$TimeoutSeconds = 360
)
$ErrorActionPreference = 'Stop'
if ($CheckPurchasePricing) { $CheckPurchasePolicy=$true }
if ($CheckPurchasePolicy) { $CheckBuyMenu=$true }
if ($CheckPurchases -and $CheckTeamFunds) { throw 'Run purchase and administrative funds fixtures separately.' }
if ($CheckBuyMenu -and ($CheckPurchases -or $CheckTeamFunds)) { throw 'Run the buy-menu fixture separately.' }
if ($CheckInventory -and ($CheckBuyMenu -or $CheckPurchases -or $CheckTeamFunds)) { throw 'Run the inventory fixture separately.' }
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
    if ($CheckScreenText) { $server.StandardInput.WriteLine('debug-screen-text'); $server.StandardInput.Flush() }
    $clients = [Collections.Generic.List[Diagnostics.Process]]::new()
    $displayedStockPriceObserved = $false
    foreach ($index in 1..2) {
		$purchaseArguments = @()
		if ($CheckPurchases) { $purchaseArguments = @('-debug-run-world-state-purchase') }
		if ($CheckBuyMenu) { $purchaseArguments = @('-debug-run-world-state-buy-menu') }
		if ($CheckPurchasePolicy) { $purchaseArguments = @('-debug-run-world-state-purchase-policy') }
		if ($CheckPurchasePricing) { $purchaseArguments = @('-debug-run-world-state-purchase-pricing') }
		if ($CheckInventory) { $purchaseArguments = @('-debug-run-world-state-inventory') }
        $client = Start-Process -FilePath $clientBinary -WorkingDirectory $gameDirectory -WindowStyle Hidden -PassThru -ArgumentList (@(
            '-debug-run','600','-debug-run-output',"$OutputPrefix-Client$index",$(if ($CheckMenuConnection) { '-debug-run-menu-connect' } else { '-world-state-client' }),'127.0.0.1',"$port"
        )+$purchaseArguments)
        $processes.Add($client)
        $clients.Add($client)
    }
    $screenTextApplied = $false
    $screenTextFixtureTimer = [Diagnostics.Stopwatch]::StartNew()
    $fundsChanged = $false
    $policyChanged = $false
    $inventoryMutated = $false
    $inventoryCreated = $false
    while (@($clients | Where-Object { !$_.HasExited }).Count -gt 0) {
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
        Check-Time
        if ($server.HasExited) { throw 'Server stopped while clients were running.' }
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
    $expectedPolicyCost = if ($CheckPurchasePricing) { 798 } else { 324 }
    if ($CheckPurchasePolicy -and (!$policyChanged -or $networkLog -notmatch "purchase id=1 accepted=1 charged=$expectedPolicyCost\.000000")) { throw 'Server stock-price policy checks failed.' }
    if ($CheckBuyMenu -and ([regex]::Matches($networkLog, 'purchase id=\d+ accepted=1').Count -ne 1)) { throw 'Expected exactly one accepted menu purchase.' }
    if ($CheckPurchases -and ($networkLog -notmatch 'purchase id=3 accepted=1 charged=500\.000000' -or
        $networkLog -notmatch 'purchase replay id=3' -or ([regex]::Matches($networkLog, 'purchase id=3 accepted=1').Count -ne 1))) { throw 'Purchase/replay server checks failed.' }
    if ($networkLog -notmatch '\(2 connected, input slot') { throw 'Two simultaneous connections not observed.' }
    foreach ($index in 1..2) {
        $clientDirectory = Join-Path $gameDirectory "ScreenShots\$OutputPrefix-Client$index"
        $clientReport = Read-Report (Join-Path $clientDirectory 'DebugRun.log')
        $clientNetwork = Read-Report (Join-Path $clientDirectory 'WorldStateClient.log')
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
        if ($CheckTeamFunds -and (!$fundsChanged -or $clientReport -notmatch '(?m)^world_state_team_funds_presentation=passed' -or
            $clientNetwork -notmatch 'funds_team0=12345\.250000 funds_team1=-17\.500000')) { throw 'Client team funds replication failed.' }
    }
    if ($CheckPurchasePolicy -and !$displayedStockPriceObserved) { throw 'Client catalog stock discount does not match the server charge.' }
    [pscustomobject]@{ Result='passed'; Clients=2; UpdatesAfterDisconnect=($latestTick-$baselineTick); Port=$port; ServerBinary=$serverBinary; ElapsedSeconds=[Math]::Round($timer.Elapsed.TotalSeconds,2) }
} finally {
    foreach ($process in $processes) {
        if (!$process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
}
