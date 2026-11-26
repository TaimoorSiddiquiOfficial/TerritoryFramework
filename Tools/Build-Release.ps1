param(
    [Parameter(Mandatory=$true)][string]$EngineRoot,
    [Parameter(Mandatory=$true)][string]$PluginSource,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [string]$NarrativeRoot,
    [string]$SourceCommit,
    [switch]$ApplyNarrativeCompatibilityPatch,
    [ValidateRange(1,2147483)][int]$TestTimeoutSeconds = 1800,
    [ValidateRange(1,64)][int]$MaxParallelActions = 6,
    [switch]$Resume,
    [switch]$RefreshSource
)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/Invoke-BoundedProcess.ps1"
$engine = [IO.Path]::GetFullPath($EngineRoot)
$source = [IO.Path]::GetFullPath($PluginSource)
$output = [IO.Path]::GetFullPath($OutputRoot)
# This is an isolated command-line host, never a hot-reload target. Refuse
# mutation while that host is open, while allowing unrelated user editors.
$hostPrefix = ([IO.Path]::GetFullPath("$output/HostProject").TrimEnd('\') + '\')
foreach ($editorProcess in @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe'")) {
    $command = ([string]$editorProcess.CommandLine).Replace('/','\')
    if ($command.IndexOf($hostPrefix, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
        throw "Close this release host before building (PID $($editorProcess.ProcessId)): $hostPrefix"
    }
    # A relative project command line may omit the absolute host prefix.
    # An editor can exit between the CIM snapshot and module enumeration. Only
    # that verified disappearance is harmless; an unreadable live editor fails.
    try {
        $loadedEditor = Get-Process -Id $editorProcess.ProcessId -ErrorAction Stop
        $loadedModules = @($loadedEditor.Modules)
    } catch {
        $remainingEditor = Get-CimInstance Win32_Process -Filter "ProcessId=$($editorProcess.ProcessId)"
        if (-not $remainingEditor) { continue }
        throw
    }
    foreach ($module in $loadedModules) {
        if ($module.FileName.StartsWith($hostPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "This release host has loaded DLLs in PID $($editorProcess.ProcessId); close it before building."
        }
    }
}

if ($RefreshSource -and -not $Resume) { throw 'RefreshSource requires an existing -Resume host.' }
if ((Test-Path -LiteralPath $output) -and -not $Resume) { throw 'Use a new output path, or -Resume for this same build host.' }
if ($Resume -and -not (Test-Path -LiteralPath "$output/HostProject/Plugins/TerritoryFramework/TerritoryFramework.uplugin")) {
    throw 'Resume requires a previous Territory build host at this exact output path.'
}
if ($output.Equals($source, [StringComparison]::OrdinalIgnoreCase) -or
    $output.Equals($engine, [StringComparison]::OrdinalIgnoreCase) -or
    $output.StartsWith($source + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    $output.StartsWith($engine + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Output must be outside the source and engine directories.'
}
$version = Get-Content -LiteralPath "$engine/Engine/Build/Build.version" -Raw | ConvertFrom-Json
if ($version.MajorVersion -ne 5 -or $version.MinorVersion -notin @(7,8)) { throw 'This script is verified for UE 5.7 and 5.8.' }
$narrative = if ($NarrativeRoot) { Get-Item -LiteralPath "$NarrativeRoot/NarrativePro.uplugin" }
else {
    $installed = @(Get-ChildItem -LiteralPath "$engine/Engine/Plugins" -Filter NarrativePro.uplugin -File -Recurse)
    if ($installed.Count -gt 1) { throw 'Multiple Narrative installations found. Select one with -NarrativeRoot.' }
    $installed | Select-Object -First 1
}
if (-not $narrative) { throw 'Install the matching Narrative Pro package under Engine/Plugins first.' }
$nativeVersion = (Get-Content -LiteralPath $narrative.FullName -Raw | ConvertFrom-Json).VersionName
# The supported version is not a literal here: it is whichever manifest in Tools/Compatibility
# pins this Vendor source as its reviewed original. Upgrading Narrative Pro therefore means
# running Tools/Rebase-NarrativeCompatibility.py and reviewing the replacement functions.
$nativeManifest = Join-Path $source "Tools/Compatibility/narrative-$nativeVersion.json"
if (-not (Test-Path -LiteralPath $nativeManifest)) {
    throw "No reviewed compatibility manifest pins Narrative Pro $nativeVersion. Rebase it with Tools/Rebase-NarrativeCompatibility.py and review the patch before changing this check."
}
$python = (Get-Command python -ErrorAction Stop).Source
& "$source/Tools/Tests/Test-ReleaseTemplate.ps1" -BuildScript "$source/Tools/Build-Release.ps1"
$dependencyCheck = & $python "$source/Tools/Verify-NarrativeCompatibility.py" --narrative-root $narrative.Directory.FullName
if ($LASTEXITCODE -notin @(0,2)) { throw 'Narrative version/hash compatibility check failed.' }
if ($LASTEXITCODE -eq 2 -and -not $ApplyNarrativeCompatibilityPatch) {
    throw "Narrative $nativeVersion needs the reviewed save correction. Opt in with -ApplyNarrativeCompatibilityPatch; only this build host copy will change."
}
$include = @('Source','Content','Resources','Shaders','Config','Docs','Tools','README.md','TerritoryFramework.uplugin')
$sourceRows = @($include | ForEach-Object {
    $entryPath = Join-Path $source $_
    if (Test-Path -LiteralPath $entryPath) {
        Get-ChildItem -LiteralPath $entryPath -Recurse -File | Where-Object {
            $_.FullName -notmatch '[\\/]__pycache__[\\/]|\.pyc$'
        } | ForEach-Object {
            [ordered]@{ path=$_.FullName.Substring($source.Length + 1).Replace('\','/'); sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
        }
    }
} | Sort-Object { $_.path })
$sourceBytes = [Text.Encoding]::UTF8.GetBytes(($sourceRows | ConvertTo-Json -Depth 3 -Compress))
$hasher = [Security.Cryptography.SHA256]::Create()
try { $sourceHash = ([BitConverter]::ToString($hasher.ComputeHash($sourceBytes))).Replace('-','').ToLowerInvariant() }
finally { $hasher.Dispose() }
if (-not $SourceCommit) {
    $SourceCommit = & git -C $source rev-parse HEAD 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $SourceCommit) { throw 'A source export requires -SourceCommit with its original revision.' }
}
if ($Resume) {
    $previousSource = Get-Content -LiteralPath "$output/SOURCE_SNAPSHOT.json" -Raw | ConvertFrom-Json
    if (-not $RefreshSource -and ($previousSource.sha256 -ne $sourceHash -or $previousSource.commit -ne $SourceCommit)) {
        throw 'Resume source differs from this host snapshot. Use a new output path to prevent stale source or assets.'
    }
    if ($RefreshSource) {
        # A reviewed refresh can reuse compiler outputs, never stale source files.
        # No deletion or vendor reset is performed. A removed or unexpected input
        # needs a new host, while every build and test gate still runs below.
        $expected = @{}
        foreach ($row in $sourceRows) { $expected[$row.path] = $true }
        $existingPlugin = "$output/HostProject/Plugins/TerritoryFramework"
        foreach ($entry in $include) {
            $path = Join-Path $existingPlugin $entry
            if (-not (Test-Path -LiteralPath $path)) { continue }
            foreach ($file in Get-ChildItem -LiteralPath $path -Recurse -File) {
                if ($file.FullName -match '[\\/]__pycache__[\\/]|\.pyc$') { continue }
                $relative = $file.FullName.Substring($existingPlugin.Length + 1).Replace('\','/')
                if (-not $expected.ContainsKey($relative)) { throw "Refresh would retain an unexpected input: $relative. Use a new host." }
            }
        }
        $history = "$output/PreviousSourceSnapshots"
        New-Item -ItemType Directory -Path $history -Force | Out-Null
        Copy-Item -LiteralPath "$output/SOURCE_SNAPSHOT.json" -Destination "$history/$($previousSource.sha256).json" -Force
    }
}
$dotnet = Get-ChildItem -LiteralPath "$engine/Engine/Binaries/ThirdParty/DotNet" -Filter dotnet.exe -File -Recurse |
    Where-Object { $_.FullName -match 'win-x64' } | Select-Object -First 1
if (-not $dotnet) { throw 'Bundled Windows .NET runtime was not found.' }
$hostRoot = "$output/HostProject"
$hostPlugin = "$hostRoot/Plugins/TerritoryFramework"
New-Item -ItemType Directory -Path $hostPlugin -Force | Out-Null
New-Item -ItemType Directory -Path "$hostRoot/Config" -Force | Out-Null
@{commit=$SourceCommit;sha256=$sourceHash;files=$sourceRows} | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath "$output/SOURCE_SNAPSHOT.json" -Encoding utf8
# Keep licensed source and its rollback backup outside Package. Never patch Engine.
$hostNative = "$hostRoot/Plugins/NarrativePro"
if (-not $Resume) {
    New-Item -ItemType Directory -Path $hostNative -Force | Out-Null
    Get-ChildItem -LiteralPath $narrative.Directory.FullName | Where-Object {
        $_.Name -notin @('Intermediate','Saved','.git')
    } | Copy-Item -Destination $hostNative -Recurse -Force
}
$patchArgs = @('--narrative-root', $hostNative)
if ($ApplyNarrativeCompatibilityPatch) { $patchArgs += @('--apply','--backup-root',"$output/DependencyBackups") }
$dependencyJson = & $python "$source/Tools/Verify-NarrativeCompatibility.py" @patchArgs
if ($LASTEXITCODE -ne 0) { throw 'Build-host Narrative compatibility correction/verification failed.' }
$dependency = $dependencyJson | ConvertFrom-Json
# Use Narrative's own project tag setup in the temporary test host. It is not packaged.
$nativeTags = Join-Path $narrative.Directory.FullName 'Resources/ProjectTemplates/TP_Narrative/Config/DefaultGameplayTags.ini'
if (-not (Test-Path -LiteralPath $nativeTags)) { throw 'Narrative project-template tag settings were not found.' }
Copy-Item -LiteralPath $nativeTags -Destination "$hostRoot/Config/DefaultGameplayTags.ini" -Force
# Native 2.4.2 ships GE_Damage_SetByCaller with this tag, but omits it from
# its project template. Register it before any dependent effect is loaded.
Add-Content -LiteralPath "$hostRoot/Config/DefaultGameplayTags.ini" -Value @'

[/Script/GameplayTags.GameplayTagsList]
+GameplayTagList=(Tag="SetByCaller.Damage",DevComment="Required by Native GE_Damage_SetByCaller")
'@
$nativeRoadSettings = Join-Path $narrative.Directory.FullName 'Resources/ProjectTemplates/TP_Narrative/Config/DefaultPlugins.ini'
if (-not (Test-Path -LiteralPath $nativeRoadSettings)) { throw 'Narrative project-template road settings were not found.' }
Copy-Item -LiteralPath $nativeRoadSettings -Destination "$hostRoot/Config/DefaultPlugins.ini" -Force
$templateContent = Join-Path $narrative.Directory.FullName 'Resources/ProjectTemplates/TP_Narrative/Content'
if (-not (Test-Path -LiteralPath $templateContent)) { throw 'Narrative project-template content was not found.' }
if (-not $Resume) { Copy-Item -LiteralPath $templateContent -Destination "$hostRoot/Content" -Recurse -Force }
# Complete the temporary Narrative project setup, including collision and Gameplay Cue paths.
# Start test sessions in an empty Engine map instead of loading Narrative's demonstration city.
$templateConfig = Join-Path $narrative.Directory.FullName 'Resources/ProjectTemplates/TP_Narrative/Config'
foreach ($configName in @('DefaultEngine.ini','DefaultGame.ini','DefaultInput.ini','DefaultMass.ini')) {
    $configSource = Join-Path $templateConfig $configName
    if (-not (Test-Path -LiteralPath $configSource)) { throw "Narrative project config is missing: $configName" }
    $configText = Get-Content -LiteralPath $configSource -Raw
    if ($configName -eq 'DefaultEngine.ini') {
        $configText = $configText -replace '(?m)^(EditorStartupMap|GameDefaultMap|ServerDefaultMap)=.*$', '$1=/Engine/Maps/Entry.Entry'
        $configText += "`n[/Script/TerritoryFramework.TerritoryDeveloperSettings]`nRoadGuideBlueprintClass=/TerritoryFramework/Blueprints/BP_TerritoryRoadGuide.BP_TerritoryRoadGuide_C`n"
    }
    if ($configName -eq 'DefaultGame.ini') {
        # Extend Native's existing primary types rather than creating a competing
        # loader. Their SpawnedData bundles own NPC appearance/activity readiness.
        $configText = [regex]::Replace($configText,
            '(?m)^\+PrimaryAssetTypesToScan=\(PrimaryAssetType="(?:NPCDefinition|PlayerDefinition)"[^\r\n]+(?=\r?$)',
            { param($match) $match.Value.Replace('Directories=((Path="/NarrativePro"),(Path="/Game"))',
                'Directories=((Path="/NarrativePro"),(Path="/Game"),(Path="/TerritoryFramework"))') })
        if ($configText -notmatch 'Directories=\([^\r\n]+/TerritoryFramework') {
            throw 'Native primary asset scan configuration changed; review template before building a host.'
        }
    }
    $configText | Set-Content -LiteralPath "$hostRoot/Config/$configName" -Encoding utf8
}
# PluginSource must be a clean export. Do not copy a working project's ignored assets or vendor plugins.
# Preserve timestamps of matching files during an exact-snapshot resume. Copying
# every header again needlessly invalidates all precompiled target configurations.
foreach ($row in $sourceRows) {
    $inputFile = Join-Path $source $row.path
    $hostFile = Join-Path $hostPlugin $row.path
    if ($Resume -and (Test-Path -LiteralPath $hostFile) -and
        (Get-FileHash -LiteralPath $hostFile -Algorithm SHA256).Hash -eq $row.sha256) { continue }
    New-Item -ItemType Directory -Path (Split-Path -Parent $hostFile) -Force | Out-Null
    Copy-Item -LiteralPath $inputFile -Destination $hostFile -Force
}
$project = "$hostRoot/HostProject.uproject"
@{
    FileVersion=3
    Plugins=@(
        @{Name='TerritoryFramework';Enabled=$true},
        @{Name='Cargo';Enabled=$false},
        @{Name='PythonScriptPlugin';Enabled=$true;TargetAllowList=@('Editor')},
        @{Name='EditorScriptingUtilities';Enabled=$true;TargetAllowList=@('Editor')}
    )
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $project -Encoding utf8
New-Item -ItemType Directory -Path "$hostRoot/Saved" -Force | Out-Null
$manifests = @()
# Native 2.4.2's UE 5.7 Game sources rely on unity includes. Writable copied
# vendor files otherwise enter adaptive non-unity builds and fail on GEngine.
# Use normal unity for that supported dependency; never broaden the source patch.
$compilerCompatibilityArgs = @()
if ($version.MinorVersion -eq 7) { $compilerCompatibilityArgs += '-DisableAdaptiveUnity' }
# Explicitly compile the patched vendor module; an installed dependency's import
# library alone does not replace its old editor DLL.
& $dotnet.FullName "$engine/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" UnrealEditor Win64 Development `
    "-Project=$project" -Module=NarrativeSaveSystem -DisablePlugin=Cargo -NoUBTMakefiles -NoHotReload -NoHotReloadFromIDE -WaitMutex "-MaxParallelActions=$MaxParallelActions" @compilerCompatibilityArgs *> "$output/Build-NarrativeSaveSystem.log"
if ($LASTEXITCODE -ne 0) { throw 'Patched NarrativeSaveSystem failed to build; see Build-NarrativeSaveSystem.log.' }
foreach ($build in @(@('UnrealEditor','Development'),@('UnrealGame','Development'),@('UnrealGame','Shipping'))) {
    $target,$config = $build
    $manifest = "$hostRoot/Saved/Manifest-$target-Win64-$config.xml"
    $log = "$output/Build-$target-$config.log"
    Write-Output "Building UE 5.$($version.MinorVersion) $target $config"
    $targetArgs = @()
    # Editor precompilation scans all engine plugins, including disabled project plugins.
    # Game targets use the project descriptor and reject this target-level override.
    if ($target -eq 'UnrealEditor') { $targetArgs += '-DisablePlugin=Cargo' }
    # Installed UnrealGame targets need the plugin precompile path. A standalone
    # -Module build has no output item for a project dependency on that target.
    # Retain dependency outputs so the corrected Native save module is compiled
    # for BOTH game configurations instead of silently reusing a vendor binary.
    if ($target -eq 'UnrealGame') { $targetArgs += '-BuildDependantPlugins' }
    # These are the same target/configuration and manifest arguments used by UAT BuildPlugin.
    & $dotnet.FullName "$engine/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" $target Win64 $config `
        "-Project=$project" "-plugin=$hostPlugin/TerritoryFramework.uplugin" -noubtmakefiles "-manifest=$manifest" -nohotreload -NoHotReloadFromIDE -WaitMutex "-MaxParallelActions=$MaxParallelActions" @compilerCompatibilityArgs @targetArgs *> $log
    if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $log -Tail 35; throw "$target $config failed; see $log" }
    $manifests += $manifest
}
$package = "$output/Package/TerritoryFramework"
if (Test-Path -LiteralPath $package) {
    # Preserve the previous package, and assemble a fresh one without stale files.
    $previous = [IO.Path]::GetFullPath("$output/PreviousPackages/" + [guid]::NewGuid().ToString('N'))
    $packageAbsolute = (Resolve-Path -LiteralPath $package).Path
    $outputPrefix = $output.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $packageAbsolute.StartsWith($outputPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not $previous.StartsWith($outputPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Package backup paths must stay inside this build output.'
    }
    New-Item -ItemType Directory -Path (Split-Path $previous) -Force | Out-Null
    Move-Item -LiteralPath $packageAbsolute -Destination $previous
}
New-Item -ItemType Directory -Path $package -Force | Out-Null
# Match UAT's source/content/generated-header rules, and include community docs and plugin config.
foreach ($entry in $include) {
    if (Test-Path -LiteralPath "$hostPlugin/$entry") { Copy-Item -LiteralPath "$hostPlugin/$entry" -Destination $package -Recurse -Force }
}
$products = @()
foreach ($manifest in $manifests) {
    [xml]$xml = Get-Content -LiteralPath $manifest -Raw
    $products += @($xml.BuildManifest.BuildProducts.string)
}
$products += @(Get-ChildItem -LiteralPath "$hostPlugin/Intermediate/Build" -File -Recurse |
    Where-Object { $_.FullName -match '[\\/]Inc[\\/]' } | ForEach-Object FullName)
foreach ($product in $products | Sort-Object -Unique) {
    if (-not $product) { continue }
    $absolute = [IO.Path]::GetFullPath($product)
    $prefix = [IO.Path]::GetFullPath($hostPlugin) + [IO.Path]::DirectorySeparatorChar
    if (-not $absolute.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { continue }
    $relative = $absolute.Substring($prefix.Length)
    $destination = Join-Path $package $relative
    New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
    Copy-Item -LiteralPath $absolute -Destination $destination -Force
}
$descriptorPath = "$package/TerritoryFramework.uplugin"
$descriptor = Get-Content -LiteralPath $descriptorPath -Raw | ConvertFrom-Json
$descriptor | Add-Member EngineVersion "5.$($version.MinorVersion).0" -Force
$descriptor.Installed = $true
$descriptor | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $descriptorPath -Encoding utf8
# Gate the newly built host before creating a successful receipt or uploading.
foreach ($check in @('Check-OptionsExtraction.py','Verify-OptionsDoc.py','Verify-OptionsGuide.py')) {
    & $python "$hostPlugin/Tools/$check"
    if ($LASTEXITCODE -ne 0) { throw "Fresh-source documentation check failed: $check" }
}
$shellExe = (Get-Process -Id $PID).Path
& $shellExe -NoProfile -NonInteractive -File "$hostPlugin/Tools/Run-Tests.ps1" `
    -Filter TerritoryFramework -EngineRoot $engine -Project $project -TimeoutSeconds $TestTimeoutSeconds `
    -ReportPath "$output/AUTOMATION.json"
if ($LASTEXITCODE -ne 0) { throw 'Fresh release-host automation failed, timed out or completed zero tests.' }
$automation = Get-Content -LiteralPath "$output/AUTOMATION.json" -Raw | ConvertFrom-Json
if (-not $automation.passed -or -not $automation.queue_completed -or ($automation.success + $automation.success_with_warnings) -le 0) {
    throw 'Automation receipt did not pass.'
}
$oldContentReport = $env:TF_RELEASE_REPORT
try {
    $env:TF_RELEASE_REPORT = "$output/CONTENT_VALIDATION.json"
    $contentProcess = Invoke-BoundedProcess -FilePath "$engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" `
        -ArgumentList @("`"$project`"", '/Engine/Maps/Entry', '-run=pythonscript', "-script=`"$hostPlugin/Tools/Validate-Content.py`"", '-DDC=InstalledNoZenLocalFallback','-unattended','-nullrhi','-nosplash','-nop4','-stdout','-FullStdOutLogOutput') `
        -StandardOutputPath "$output/ContentValidation.log" -StandardErrorPath "$output/ContentValidation.err.log" -TimeoutSeconds $TestTimeoutSeconds
    if ($contentProcess.TimedOut -or $contentProcess.ExitCode -ne 0) { throw 'Included content validation failed or timed out.' }
} finally { $env:TF_RELEASE_REPORT = $oldContentReport }
$content = Get-Content -LiteralPath "$output/CONTENT_VALIDATION.json" -Raw | ConvertFrom-Json
if ($content.errors -ne 0 -or $content.invalid.Count -ne 0 -or $content.assets_checked -le 0 -or
    $content.assets_checked -ne $content.expected_assets -or $content.missing_packages.Count -ne 0 -or
    $content.unexpected_packages.Count -ne 0 -or $content.external_dependencies.Count -ne 0) {
    throw 'Content receipt did not pass.'
}
@{
    plugin_version=$descriptor.VersionName; source_commit=$SourceCommit; source_sha256=$sourceHash
    engine_version=$content.engine_version; narrative_version=$dependency.version; narrative_patch_sha256_lf=$dependency.sha256_lf
    platform='Win64'; builds=@('Editor Development','Game Development','Game Shipping')
    compiler_compatibility_arguments=$compilerCompatibilityArgs
    automation=$automation; content=@{assets=$content.assets_checked;blueprints=$content.blueprints_compiled;errors=$content.errors;warnings=$content.warnings}
    verified_utc=[DateTime]::UtcNow.ToString('o'); build_automation_content_passed=$true
    limitations=@('Socket multiplayer, physical streaming, performance and cooked-game scenarios require separate acceptance evidence.')
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath "$package/BUILD_INFO.json" -Encoding utf8
Copy-Item -LiteralPath "$output/AUTOMATION.json","$output/CONTENT_VALIDATION.json" -Destination $package -Force
if (Get-ChildItem -LiteralPath $package -Filter NarrativePro.uplugin -Recurse -File) { throw 'Licensed dependency leaked into the package.' }
Write-Output "BUILD AND AUTOMATION/CONTENT GATES PASSED: $package"
