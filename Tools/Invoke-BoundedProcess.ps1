# Kept separate so the automation runner's timeout can be exercised without launching Unreal.
function Invoke-BoundedProcess {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$FilePath,
        [Parameter(Mandatory=$true)][string[]]$ArgumentList,
        [Parameter(Mandatory=$true)][string]$StandardOutputPath,
        [Parameter(Mandatory=$true)][string]$StandardErrorPath,
        [Parameter(Mandatory=$true)][ValidateRange(1, 2147483)][int]$TimeoutSeconds
    )

    $childProcess = $null
    try {
        $childProcess = Start-Process -FilePath $FilePath -ArgumentList $ArgumentList `
            -WindowStyle Hidden -PassThru -RedirectStandardOutput $StandardOutputPath `
            -RedirectStandardError $StandardErrorPath
        # Retain the process handle before it exits. This also keeps the exit code available
        # when Start-Process returned before a very short-lived command completed.
        $null = $childProcess.Handle
        $timedOut = -not $childProcess.WaitForExit($TimeoutSeconds * 1000)
        if ($timedOut) {
            # Kill through the process object we started, never an executable-name search:
            # another editor session or another automation run must survive this timeout.
            if (-not $childProcess.HasExited) { $childProcess.Kill() }
            if (-not $childProcess.WaitForExit(10000)) {
                throw "Timed-out child process $($childProcess.Id) did not exit after termination."
            }
        }
        return [pscustomobject]@{
            ProcessId = $childProcess.Id
            ExitCode = $childProcess.ExitCode
            TimedOut = $timedOut
        }
    }
    finally {
        if ($null -ne $childProcess) {
            try {
                # Also release our own child if waiting was interrupted by an exception.
                if (-not $childProcess.HasExited) {
                    $childProcess.Kill()
                    $null = $childProcess.WaitForExit(10000)
                }
            }
            finally { $childProcess.Dispose() }
        }
    }
}
