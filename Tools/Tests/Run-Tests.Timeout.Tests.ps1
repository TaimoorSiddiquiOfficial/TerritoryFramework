# Exercises the real bounded launcher with short PowerShell children; no Unreal instance needed.
param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/../Invoke-BoundedProcess.ps1"

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$powershellExecutable = (Get-Process -Id $PID).Path
$checks = @()
$completedText = "Result={Success}`nTEST COMPLETE. EXIT CODE: 0"
foreach ($case in @(
    @{ Name='completed'; Text=$completedText; Code=0; Timeout=$false; Passed=$true },
    @{ Name='zero'; Text='TEST COMPLETE. EXIT CODE: 0'; Code=0; Timeout=$false; Passed=$false },
    @{ Name='partial'; Text='Result={Success}'; Code=0; Timeout=$false; Passed=$false },
    @{ Name='failed'; Text="$completedText`nResult={Fail}"; Code=0; Timeout=$false; Passed=$false },
    @{ Name='crash'; Text="$completedText`nFatal error"; Code=0; Timeout=$false; Passed=$false },
    @{ Name='timeout'; Text=$completedText; Code=0; Timeout=$true; Passed=$false },
    @{ Name='process failure'; Text=$completedText; Code=7; Timeout=$false; Passed=$false }
)) {
    $result = Get-TerritoryAutomationResult -Text $case.Text -ExitCode $case.Code -TimedOut $case.Timeout
    if ($result.passed -ne $case.Passed) { throw "Incorrect release admission: $($case.Name)" }
}
$checks += 'Release gate rejects zero, partial, failed, crashed, timed-out and nonzero runs'

function Test-Child {
    param([string]$Name, [string]$Command, [int]$TimeoutSeconds = 15)
    $encodedCommand = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($Command))
    Invoke-BoundedProcess -FilePath $powershellExecutable `
        -ArgumentList @('-NoProfile', '-NonInteractive', '-EncodedCommand', $encodedCommand) `
        -StandardOutputPath "$OutputDirectory/$Name.out.log" `
        -StandardErrorPath "$OutputDirectory/$Name.err.log" -TimeoutSeconds $TimeoutSeconds
}

$successful = Test-Child 'success' '[Console]::Out.WriteLine("Result={Success}"); [Console]::Error.WriteLine("stderr retained"); exit 0'
if ($successful.TimedOut -or $successful.ExitCode -ne 0) { throw 'Successful child did not retain exit code 0.' }
if ((Get-Content -LiteralPath "$OutputDirectory/success.out.log" -Raw) -notmatch 'Result=\{Success\}') {
    throw 'Successful child stdout was not retained.'
}
if ((Get-Content -LiteralPath "$OutputDirectory/success.err.log" -Raw) -notmatch 'stderr retained') {
    throw 'Successful child stderr was not retained.'
}
$checks += 'Normal exit and both redirected logs retained'

$failed = Test-Child 'failure' 'exit 7'
if ($failed.TimedOut -or $failed.ExitCode -ne 7) { throw 'Nonzero child exit code was lost.' }
$checks += 'Nonzero exit code retained'

# Keep an independent process alive to prove timeout cleanup targets only its own child.
$sentinelCommand = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes('Start-Sleep -Seconds 60'))
$sentinel = Start-Process -FilePath $powershellExecutable -WindowStyle Hidden -PassThru `
    -ArgumentList @('-NoProfile', '-NonInteractive', '-EncodedCommand', $sentinelCommand)
try {
    $null = $sentinel.Handle
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $hung = Test-Child 'timeout' '[Console]::Out.WriteLine("Result={Success}"); Start-Sleep -Seconds 60' 3
    $watch.Stop()
    if (-not $hung.TimedOut) { throw 'Hung child with a success log was not marked timed out.' }
    if ($watch.Elapsed.TotalSeconds -gt 15) { throw 'Timeout did not bound the child lifetime.' }
    if (Get-Process -Id $hung.ProcessId -ErrorAction SilentlyContinue) { throw 'Timed-out child is still alive.' }
    if ($sentinel.HasExited) { throw 'Timeout terminated an unrelated process.' }
    if ((Get-Content -LiteralPath "$OutputDirectory/timeout.out.log" -Raw) -notmatch 'Result=\{Success\}') {
        throw 'Timeout discarded the child log.'
    }
    $checks += 'Timeout overrides success-looking output, keeps logs and terminates only its own child'
}
finally {
    if (-not $sentinel.HasExited) { $sentinel.Kill(); $null = $sentinel.WaitForExit(10000) }
    $sentinel.Dispose()
}

$rejected = $false
try { Test-Child 'invalid-timeout' 'exit 0' 0 | Out-Null }
catch {
    if ($_.FullyQualifiedErrorId -notlike 'ParameterArgumentValidationError,*') { throw }
    $rejected = $true
}
if (-not $rejected) { throw 'A nonpositive timeout was accepted.' }
$checks += 'Nonpositive timeout rejected before launch'

[pscustomobject]@{ Passed = $true; Checks = $checks } | ConvertTo-Json -Depth 3 |
    Tee-Object -FilePath "$OutputDirectory/result.json"
