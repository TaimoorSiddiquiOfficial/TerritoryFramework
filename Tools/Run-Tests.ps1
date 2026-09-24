#Runs one automation-test filter headless against a host project, and reports what actually
#happened. Two things this exists to stop:
#
#  * A stale binary. UnrealEditor-Cmd loads the compiled DLL from disk, so a run after a failed
#    build silently re-tests the *previous* code and reports green. The staleness check below
#    refuses to run in that case instead.
#  * A defect that only an allocator that poisons freed memory can see. -Stomp passes
#    -stompmalloc, which turns a read of a destroyed temporary into an access violation.
#    TerritoryFramework.Tales.Tasks.FloorGarrisonObjectives and
#    TerritoryFramework.Guards.Floors.ProgressReadsItsOwnFloorSnapshot both cover dangling
#    reads of ATerritoryVolume::GetGarrisonSnapshot(), which returns its read model by value.
#    Run the same filter both ways: the default allocator is expected to pass, Stomp is
#    expected to catch the hazard. A filter that only ever runs under the default allocator
#    is not evidence about that class of defect.
#
#Usage:
#  Tools/Run-Tests.ps1 -Filter TerritoryFramework.Guards.Floors
#  Tools/Run-Tests.ps1 -Filter TerritoryFramework.Guards.Floors -Stomp
param(
    [Parameter(Mandatory=$true)][string]$Filter,
    [switch]$Stomp,
    [string]$EngineRoot,
    [string]$Project,
    [int]$TimeoutSeconds = 1800
)
$ErrorActionPreference = 'Stop'

$plugin = [IO.Path]::GetFullPath("$PSScriptRoot/..")
$project = if ($Project) { [IO.Path]::GetFullPath($Project) }
    else { [IO.Path]::GetFullPath("$plugin/../../TDA.uproject") }
if (-not (Test-Path -LiteralPath $project)) { throw "Host project not found: $project" }
$projectRoot = Split-Path -Parent $project

if (-not $EngineRoot) {
    $association = (Get-Content -LiteralPath $project -Raw | ConvertFrom-Json).EngineAssociation
    if (-not $association) { throw 'The host project declares no EngineAssociation; pass -EngineRoot.' }
    if ($association.StartsWith('{')) {
        # A GUID association is a custom or source install. UnrealBuildTool resolves it through
        # this per-user key rather than through the launcher's install folder, so a version string
        # would not name the same engine. This host project uses one.
        $EngineRoot = (Get-ItemProperty -Path 'HKCU:/SOFTWARE/Epic Games/Unreal Engine/Builds' `
            -ErrorAction SilentlyContinue).$association
        if (-not $EngineRoot) { throw "No engine is registered for $association; pass -EngineRoot." }
    }
    else { $EngineRoot = "C:/Program Files/Epic Games/UE_$association" }
}
$editor = "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
if (-not (Test-Path -LiteralPath $editor)) { throw "UnrealEditor-Cmd not found: $editor" }

#Refuse to test a binary the current sources were not built into.
$dll = Get-Item -LiteralPath "$plugin/Binaries/Win64/UnrealEditor-TerritoryFramework.dll"
$newestSource = Get-ChildItem -LiteralPath "$plugin/Source" -Recurse -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if ($newestSource.LastWriteTime -gt $dll.LastWriteTime) {
    throw ("UnrealEditor-TerritoryFramework.dll is older than $($newestSource.Name). " +
        "Build before running tests, or this run tests the previous code. " +
        "A host editor holding the DLLs blocks the link; close it first.")
}

$label = if ($Stomp) { 'stomp' } else { 'default' }
$log = "$projectRoot/Saved/Tests/$($Filter -replace '[^A-Za-z0-9_.]','_').$label.log"
New-Item -ItemType Directory -Path (Split-Path -Parent $log) -Force | Out-Null

$arguments = @(
    "`"$project`"",
    "-ExecCmds=`"Automation RunTests $Filter; Quit`"",
    '-unattended', '-nopause', '-nosplash', '-nullrhi', '-NoSound',
    '-log', '-stdout', '-FullStdOutLogOutput',
    '-TestExit="Automation Test Queue Empty"'
)
if ($Stomp) { $arguments += '-stompmalloc' }

Write-Host "Running $Filter under the $label allocator. Log: $log"
$process = Start-Process -FilePath $editor -ArgumentList $arguments -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput $log -RedirectStandardError "$log.err"
$text = (Get-Content -LiteralPath $log -Raw) + (Get-Content -LiteralPath "$log.err" -Raw)

$success = ([regex]::Matches($text, 'Result=\{Success\}')).Count
$warned = ([regex]::Matches($text, 'Result=\{SuccessWithWarnings\}')).Count
$failed = ([regex]::Matches($text, 'Result=\{Fail\}')).Count
$crash = $text -match 'EXCEPTION_ACCESS_VIOLATION|Fatal error|Assertion failed'

Write-Host ("exit=$($process.ExitCode)  success=$success  successWithWarnings=$warned  " +
    "fail=$failed  crash=$crash")
if ($crash) {
    $frames = [regex]::Matches($text, 'TerritoryFramework[^\r\n]*\.cpp:\d+')
    if ($frames.Count) { Write-Host "First frames: $($frames[0].Value)" }
}
#A run that never reached the queue is not a pass, however clean the counts look.
if ($process.ExitCode -notin @(0, 2) -or $failed -gt 0 -or $crash -or ($success + $warned) -eq 0) {
    Write-Host "FAILED - see $log"
    exit 1
}
Write-Host 'PASSED'
