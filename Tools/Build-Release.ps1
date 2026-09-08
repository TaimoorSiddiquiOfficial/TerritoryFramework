param(
    [Parameter(Mandatory=$true)][string]$EngineRoot,
    [Parameter(Mandatory=$true)][string]$PluginSource,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [switch]$Resume
)
$ErrorActionPreference = 'Stop'
$engine = [IO.Path]::GetFullPath($EngineRoot)
$source = [IO.Path]::GetFullPath($PluginSource)
$output = [IO.Path]::GetFullPath($OutputRoot)
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
$narrative = Get-ChildItem -LiteralPath "$engine/Engine/Plugins" -Filter NarrativePro.uplugin -File -Recurse | Select-Object -First 1
if (-not $narrative) { throw 'Install the matching Narrative Pro package under Engine/Plugins first.' }
$nativeVersion = (Get-Content -LiteralPath $narrative.FullName -Raw | ConvertFrom-Json).VersionName
if ($nativeVersion -ne '2.4.2') { throw "Expected Narrative Pro 2.4.2; found $nativeVersion. Verify compatibility before changing this check." }
$dotnet = Get-ChildItem -LiteralPath "$engine/Engine/Binaries/ThirdParty/DotNet" -Filter dotnet.exe -File -Recurse |
    Where-Object { $_.FullName -match 'win-x64' } | Select-Object -First 1
if (-not $dotnet) { throw 'Bundled Windows .NET runtime was not found.' }
$hostRoot = "$output/HostProject"
$hostPlugin = "$hostRoot/Plugins/TerritoryFramework"
New-Item -ItemType Directory -Path $hostPlugin -Force | Out-Null
New-Item -ItemType Directory -Path "$hostRoot/Config" -Force | Out-Null
# Use Narrative's own project tag setup in the temporary test host. It is not packaged.
$nativeTags = Join-Path $narrative.Directory.FullName 'Resources/ProjectTemplates/TP_Narrative/Config/DefaultGameplayTags.ini'
if (-not (Test-Path -LiteralPath $nativeTags)) { throw 'Narrative project-template tag settings were not found.' }
Copy-Item -LiteralPath $nativeTags -Destination "$hostRoot/Config/DefaultGameplayTags.ini" -Force
$nativeRoadSettings = Join-Path $narrative.Directory.FullName 'Resources/ProjectTemplates/TP_Narrative/Config/DefaultPlugins.ini'
if (-not (Test-Path -LiteralPath $nativeRoadSettings)) { throw 'Narrative project-template road settings were not found.' }
Copy-Item -LiteralPath $nativeRoadSettings -Destination "$hostRoot/Config/DefaultPlugins.ini" -Force
# Complete the temporary Narrative project setup, including collision and Gameplay Cue paths.
# Start test sessions in an empty Engine map instead of loading Narrative's demonstration city.
$templateConfig = Join-Path $narrative.Directory.FullName 'Resources/ProjectTemplates/TP_Narrative/Config'
foreach ($configName in @('DefaultEngine.ini','DefaultGame.ini','DefaultInput.ini','DefaultMass.ini')) {
    $configSource = Join-Path $templateConfig $configName
    if (-not (Test-Path -LiteralPath $configSource)) { throw "Narrative project config is missing: $configName" }
    $configText = Get-Content -LiteralPath $configSource -Raw
    if ($configName -eq 'DefaultEngine.ini') {
        $configText = $configText -replace '(?m)^(EditorStartupMap|GameDefaultMap|ServerDefaultMap)=.*$', '$1=/Engine/Maps/Entry.Entry'
    }
    $configText | Set-Content -LiteralPath "$hostRoot/Config/$configName" -Encoding utf8
}
# PluginSource must be a clean export. Do not copy a working project's ignored assets or vendor plugins.
Get-ChildItem -LiteralPath $source | Where-Object { $_.Name -notin @('.git','Binaries','Intermediate','Saved') } |
    Copy-Item -Destination $hostPlugin -Recurse -Force
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
foreach ($build in @(@('UnrealEditor','Development'),@('UnrealGame','Development'),@('UnrealGame','Shipping'))) {
    $target,$config = $build
    $manifest = "$hostRoot/Saved/Manifest-$target-Win64-$config.xml"
    $log = "$output/Build-$target-$config.log"
    Write-Output "Building UE 5.$($version.MinorVersion) $target $config"
    $targetArgs = @()
    # Editor precompilation scans all engine plugins, including disabled project plugins.
    # Game targets use the project descriptor and reject this target-level override.
    if ($target -eq 'UnrealEditor') { $targetArgs += '-DisablePlugin=Cargo' }
    # These are the same target/configuration and manifest arguments used by UAT BuildPlugin.
    & $dotnet.FullName "$engine/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" $target Win64 $config `
        "-Project=$project" "-plugin=$hostPlugin/TerritoryFramework.uplugin" -noubtmakefiles "-manifest=$manifest" -nohotreload @targetArgs *> $log
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
$include = @('Source','Content','Resources','Shaders','Config','Docs','Tools','README.md','TerritoryFramework.uplugin')
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
Write-Output "BUILD PASSED: $package"
