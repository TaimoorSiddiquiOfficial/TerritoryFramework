# Install and update

## Before installing

Install the Unreal version you use and its matching **Narrative Pro 2.4.2** package. Territory Framework extends Narrative Pro and needs its public APIs. Narrative Pro is not included in these downloads.

Start with Narrative Pro's project template, or complete Narrative's setup for an existing project. This supplies its input, gameplay tags, collision settings, and other project configuration. Enabling the Narrative plugin alone does not apply those settings. Territory Framework registers its own five example Territory tags automatically.

The release offers separate **Win64** packages for UE 5.7 and UE 5.8. See [Release checks](RELEASE_VERIFICATION.md) for exact tested versions and known limits. Other platforms need their own source build and tests.

## Install

1. Close Unreal Editor.
2. Download the ZIP whose name matches your Unreal version.
3. Extract its `TerritoryFramework` folder into `YourProject/Plugins/`.
4. Check the path is `YourProject/Plugins/TerritoryFramework/TerritoryFramework.uplugin`. Avoid an extra nested folder.
5. Open the project and enable **Territory Framework** in the Plugins window if needed. Allow Unreal to enable its required plugins, then restart.
6. In the Content Browser settings, enable **Show Plugin Content** to see the included UI, AI, Definitions, dialogue, and Tales tasks.
7. Read [Included content](INCLUDED_CONTENT.md), then follow [Quick Start](01_Quick_Start.md). The examples help you set up your own map. The full TDA map is not part of the download.

If Unreal asks to rebuild modules, check that you downloaded the correct engine package and installed the matching Narrative Pro build. A different engine patch or source engine may require Visual Studio with Unreal C++ build tools. Regenerate the project's files and build its Editor target.

## Update an existing project

Back up your project and save games. Close Unreal. Move your old TerritoryFramework folder to a backup location outside the project's Plugins folder, then install the new folder. Do not merge new binaries over old files.

Keep custom game assets in your project's Content folder or a separate project plugin. Files added inside TerritoryFramework may be lost when you replace the folder.

Open the project, compile your game Blueprints, run Data Validation, and test capture, income, saving, loading, and multiplayer flows used by your game. Read [Version rules](VERSIONING.md) before moving between releases.

## Check the download

Download `SHA256SUMS.txt` from the same GitHub release. In PowerShell, run:

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'TerritoryFramework-0.3.0-preview.1-UE5.8-Win64.zip'
```

Compare the hash with the matching line in the checksum file. A different hash means the file is not the verified release download.
