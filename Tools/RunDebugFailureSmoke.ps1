[CmdletBinding()]
param(
    [string]$ExecutablePath,
    [ValidatePattern('^[A-Za-z0-9_-]{1,48}$')]
    [string]$OutputPrefix = 'DebugFailureSmoke',
    [ValidateRange(15, 300)]
    [int]$TimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'
if (!$ExecutablePath) { $ExecutablePath = Join-Path $PSScriptRoot '..\Cortex Command.exe' }
$executable = (Resolve-Path -LiteralPath $ExecutablePath).Path
$gameDirectory = Split-Path -Parent $executable
$unicodeProbe = -join ([char[]]@(0x041F, 0x0440, 0x0438, 0x0432, 0x0435, 0x0442))
foreach ($probe in @('assert', 'abort', 'exception')) {
    $output = "$OutputPrefix-$probe"
    $directory = Join-Path $gameDirectory "ScreenShots\$output"
    foreach ($parent in @((Split-Path -Parent $directory), $directory)) {
        if ((Test-Path -LiteralPath $parent) -and
            ((Get-Item -LiteralPath $parent).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Diagnostic output directory must not be a reparse point: $parent"
        }
    }
    [IO.Directory]::CreateDirectory($directory) | Out-Null
    $stderrPath = Join-Path $directory 'ProcessStderr.log'
    if ((Test-Path -LiteralPath $stderrPath) -and
        ((Get-Item -LiteralPath $stderrPath).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Diagnostic stderr must not be a reparse point: $stderrPath"
    }
    # Keep the original process handle: Start-Process -PassThru can lose ExitCode
    # on Windows PowerShell 5.1 when a redirected process exits quickly.
    $process = [Diagnostics.Process]::new()
    $process.StartInfo.FileName = $executable
    $process.StartInfo.WorkingDirectory = $gameDirectory
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.StandardErrorEncoding = [Text.Encoding]::UTF8
    $process.StartInfo.Arguments = "-debug-run 60 -debug-run-preflight-only -debug-run-error-probe $probe -debug-run-output $output"
    $started = $false
    try {
        $started = $process.Start()
        if (!$started) { throw "Unable to start $probe probe." }
        $stderrRead = $process.StandardError.ReadToEndAsync()
        $timer = [Diagnostics.Stopwatch]::StartNew()
        while (!$process.WaitForExit(500)) {
            if ($timer.Elapsed.TotalSeconds -ge $TimeoutSeconds) { throw "$probe probe timed out." }
        }
        if ($process.ExitCode -ne 1) { throw "$probe probe returned $($process.ExitCode), expected 1." }
        [IO.File]::WriteAllText($stderrPath, $stderrRead.GetAwaiter().GetResult(), [Text.UTF8Encoding]::new($false))
        $failure = Get-Content -LiteralPath (Join-Path $directory 'DebugFailure.log') -Raw -Encoding UTF8
        $report = Get-Content -LiteralPath (Join-Path $directory 'DebugRun.log') -Raw -Encoding UTF8
        if (!$failure.Contains('Diagnostic failure probe: ' + $unicodeProbe) -or $report -notmatch '(?m)^result=failed\r?$') {
            throw "$probe probe did not write the expected UTF-8 failure report."
        }
        if ($probe -eq 'exception' -and $failure -notmatch 'diagnostic-probe-callstack') {
            throw 'Exception probe did not preserve its supplied callstack.'
        }
        [pscustomobject]@{ Probe = $probe; Result = 'passed'; ExpectedExitCode = $process.ExitCode; Report = $directory }
    } finally {
        if ($started -and !$process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
}
