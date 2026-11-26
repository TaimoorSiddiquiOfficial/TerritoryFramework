param([string]$BuildScript = "$PSScriptRoot/../Build-Release.ps1")
$ErrorActionPreference = 'Stop'
$tokens = $null
$errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile(
    [IO.Path]::GetFullPath($BuildScript), [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw $errors[0] }
$branch = @($ast.FindAll({ param($node)
    $node -is [Management.Automation.Language.IfStatementAst] -and
    $node.Clauses[0].Item1.Extent.Text -eq "`$configName -eq 'DefaultGame.ini'"
}, $true))
if ($branch.Count -ne 1) { throw 'Expected one actual Native primary scan configuration block.' }
$body = $branch[0].Clauses[0].Item2.Extent.Text
$configure = [scriptblock]::Create($body.Substring(1, $body.Length - 2))
$lines = @(
    '[/Script/Engine.AssetManagerSettings]',
    '+PrimaryAssetTypesToScan=(PrimaryAssetType="NPCDefinition",AssetBaseClass="/Script/NarrativeArsenal.NPCDefinition",Directories=((Path="/NarrativePro"),(Path="/Game")),Rules=(CookRule=AlwaysCook))',
    '+PrimaryAssetTypesToScan=(PrimaryAssetType="PlayerDefinition",AssetBaseClass="/Script/NarrativeArsenal.PlayerDefinition",Directories=((Path="/NarrativePro"),(Path="/Game")),Rules=(CookRule=AlwaysCook))',
    '+PrimaryAssetTypesToScan=(PrimaryAssetType="Unrelated",Directories=((Path="/Game")))'
)
foreach ($newline in @("`n", "`r`n")) {
    $configText = ($lines -join $newline) + $newline
    . $configure
    if ([regex]::Matches($configText, '\(Path="/TerritoryFramework"\)').Count -ne 2) {
        throw 'Both Native primary types must include the Territory directory.'
    }
    if (-not $configText.Contains($lines[3] + $newline)) { throw 'Unrelated scan or line endings changed.' }
}
$configText = $lines[1].Replace('/NarrativePro', '/UnexpectedVendorLayout')
$rejected = $false
try { . $configure } catch { $rejected = $true }
if (-not $rejected) { throw 'An unexpected template must fail instead of silently omitting plugin assets.' }
Write-Output 'PASS: actual release configuration handles LF and CRLF, preserves unrelated scans, and rejects unknown layouts.'

# Exercise the actual release preflight with deterministic process lifecycle cases.
$processLoop = @($ast.FindAll({ param($node)
    $node -is [Management.Automation.Language.ForEachStatementAst] -and
    $node.Variable.VariablePath.UserPath -eq 'editorProcess'
}, $true))
if ($processLoop.Count -ne 1) { throw 'Expected one release editor preflight loop.' }
$preflight = [scriptblock]::Create($processLoop[0].Extent.Text)
$hostPrefix = 'C:\IsolatedBuild\HostProject\'
function Get-CimInstance {
    param([string]$ClassName, [string]$Filter)
    if ($Filter -like 'ProcessId=*') {
        if ($script:ProcessCase -eq 'gone') { return $null }
        return [pscustomobject]@{ ProcessId=42; CommandLine='UnrealEditor.exe Relative.uproject' }
    }
    return [pscustomobject]@{ ProcessId=42; CommandLine='UnrealEditor.exe Relative.uproject' }
}
function Get-Process {
    param([int]$Id, [string]$ErrorAction)
    if ($script:ProcessCase -in @('gone','unreadable')) { throw 'Process lookup/module access failed' }
    $path = if ($script:ProcessCase -eq 'loaded') { $hostPrefix + 'Plugins\TerritoryFramework\Binaries\Win64\UnrealEditor-TerritoryFramework.dll' } else { 'C:\OtherProject\UnrealEditor-Other.dll' }
    return [pscustomobject]@{ Modules=@([pscustomobject]@{ FileName=$path }) }
}
foreach ($script:ProcessCase in @('gone','unrelated','unreadable','loaded')) {
    $rejected = $false
    try { & $preflight } catch { $rejected = $true }
    $expectedRejection = $script:ProcessCase -in @('unreadable','loaded')
    if ($rejected -ne $expectedRejection) { throw "Incorrect release preflight for $script:ProcessCase" }
}
Write-Output 'PASS: exited/unrelated editor admitted; unreadable live editor and loaded release host rejected.'
