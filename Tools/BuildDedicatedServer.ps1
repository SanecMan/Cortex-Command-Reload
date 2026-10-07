[CmdletBinding()]
param(
    [ValidateSet('Final', 'Debug Minimal')]
    [string]$Configuration = 'Final',
    [switch]$SkipCoreBuild
)
$ErrorActionPreference = 'Stop'
$gameDirectory = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Visual Studio C++ x64 toolchain is required.' }
$msbuild = Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
Push-Location $gameDirectory
$savedEnvironment = @{}
try {
    if (!$SkipCoreBuild) {
        & $msbuild RTEA.sln /m "/p:Configuration=$Configuration" /p:Platform=x64 /verbosity:minimal
        if ($LASTEXITCODE) { throw 'Engine build failed.' }
    }
    $vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
    $environmentLines = & $env:ComSpec /d /c ('call "' + $vcvars + '" >nul && set')
    if ($LASTEXITCODE) { throw 'Cannot initialize the x64 compiler environment.' }
    foreach ($line in $environmentLines) {
        if ($line -match '^([^=]+)=(.*)$') {
            $name = $Matches[1]
            $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
            [Environment]::SetEnvironmentVariable($name, $Matches[2], 'Process')
        }
    }
    $intermediate = Join-Path $gameDirectory "_Bin\x64\$Configuration"
    $linkLogPath = Join-Path $intermediate 'RTEA.tlog\link.command.1.tlog'
    $linkLines = [IO.File]::ReadAllLines($linkLogPath, [Text.Encoding]::Unicode)
    $coreCommand = @($linkLines | Where-Object { $_ -match '/OUT:' })
    if ($coreCommand.Count -ne 1) { throw 'Expected one engine link command; rebuild the selected configuration.' }
    $entryObject = Join-Path $intermediate 'ServerEntry.obj'
    $entrySource = Join-Path $gameDirectory 'Source\ServerEntry.cpp'
    & cl.exe /nologo /std:c++20 /EHsc /utf-8 /O2 /GL /MD /c $entrySource "/Fo$entryObject"
    if ($LASTEXITCODE) { throw 'Server console entry compilation failed.' }
    $serverName = if ($Configuration -eq 'Final') { 'CortexCommandServer' } else { 'CortexCommandServer.debug.minimal' }
    $serverBinary = Join-Path $gameDirectory "$serverName.exe"
    $serverSymbols = Join-Path $gameDirectory "$serverName.pdb"
    # Use the engine's exact object/library list rather than duplicating it or
    # rebuilding a second simulation implementation. A response file avoids
    # Windows command-line length limits.
    $serverCommand = [regex]::Replace($coreCommand[0], '/OUT:("[^"]+"|\S+)', '/OUT:"' + $serverBinary + '"')
    $serverCommand = [regex]::Replace($serverCommand, '/PDB:("[^"]+"|\S+)', '/PDB:"' + $serverSymbols + '"')
    $serverCommand = [regex]::Replace($serverCommand, '/ENTRY:("[^"]+"|\S+)', '')
    $serverCommand = [regex]::Replace($serverCommand, '/SUBSYSTEM:WINDOWS(?:,[0-9.]+)?', '/SUBSYSTEM:CONSOLE /ENTRY:wmainCRTStartup')
    if ($serverCommand -notmatch '/ENTRY:wmainCRTStartup') { throw 'Engine link command has no Windows subsystem option.' }
    $sourceLines = @($linkLines | Where-Object { $_ -and !$_.StartsWith('^') -and $_ -notmatch '/OUT:' })
    if ($sourceLines.Count -eq 0) { throw 'Engine link object list is empty.' }
    $serverCommand += "`n" + ($sourceLines -join "`n")
    $serverCommand += ' "' + $entryObject + '"'
    $responsePath = Join-Path $intermediate 'DedicatedServerLink.rsp'
    [IO.File]::WriteAllText($responsePath, $serverCommand, [Text.Encoding]::Unicode)
    & link.exe "@$responsePath"
    if ($LASTEXITCODE) { throw 'Dedicated server link failed.' }
    Get-Item -LiteralPath $serverBinary | Select-Object FullName, Length, LastWriteTime
} finally {
    foreach ($name in $savedEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process') }
    Pop-Location
}
