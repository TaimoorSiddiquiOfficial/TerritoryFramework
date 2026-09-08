# GitHub Builds for Unreal 5.7 and 5.8

The repository workflow builds two separate Win64 plugin artifacts:

- `TerritoryFramework-UE5.7-Win64`
- `TerritoryFramework-UE5.8-Win64`

It runs for pushes to `main`, version tags such as `v0.3.0-preview.1`, and manual **Run workflow** requests.
It intentionally does not execute unreviewed pull-request code on licensed self-hosted machines.

## Why self-hosted runners are required

Unreal Engine and Narrative Pro are licensed software. A normal GitHub-hosted Windows runner does
not contain either dependency. Use two self-hosted Windows x64 runners:

| Runner | Required labels | Required software |
|---|---|---|
| Unreal 5.7 | `self-hosted`, `Windows`, `X64`, `ue-5.7` | UE 5.7 and its matching Narrative Pro build |
| Unreal 5.8 | `self-hosted`, `Windows`, `X64`, `ue-5.8` | UE 5.8 and its matching Narrative Pro build |

Set the runner environment variable `UE_ROOT` to the engine directory. Easy examples:

```text
D:\Program Files\Epic Games\UE_5.7
D:\Program Files\Epic Games\UE_5.8
```

If `UE_ROOT` is absent, the workflow also checks the usual Epic Games folders on drives `C:` and
`D:`. It reads `Engine/Build/Build.version` and rejects a runner whose actual engine version does
not match its label.

Install `NarrativePro.uplugin` somewhere below each engine's `Engine/Plugins` directory. The
workflow deliberately does not download, copy, or publish Narrative Pro. The final artifact
contains TerritoryFramework only.

## What the workflow verifies

For each engine version it:

1. verifies the real Unreal version;
2. verifies that Narrative Pro 2.4.2 and its project-template settings are installed;
3. runs `Tools/Build-Release.ps1` to compile Editor Development, Game Development,
   and Game Shipping for Win64 through Unreal Build Tool;
4. copies the plugin's build products from UBT manifests, generated headers, source,
   content, config, and documentation into the package;
5. opens the included content in the temporary project and compiles/validates it;
6. adds `BUILD_INFO.json` and `CONTENT_VALIDATION.json`;
7. uploads the packaged plugin for 30 days if compilation and content checks pass.

The temporary host uses Narrative's own project-template tags, input, collision,
road, Gameplay Cue, and other settings, with an empty Engine startup map. Those
settings and Narrative's plugin files are not included in the Territory package.
The artifact job does not prove multiplayer, full gameplay, or a cooked game.
The release's [verification record](RELEASE_VERIFICATION.md) lists those gates separately.

The two matrix jobs are independent. A 5.8 compatibility failure does not cancel the 5.7 result,
which makes engine-upgrade problems easier to diagnose.

## Downloading an artifact

Open the workflow run on GitHub and download the artifact matching the target engine. Extract it
into the consuming project's `Plugins/TerritoryFramework` directory, then regenerate project files
or reopen the Unreal project.

An artifact is produced only after compilation succeeds. The workflow does not treat uncompiled
source as a release package.

## Runner troubleshooting

The script uses the same target/configuration and manifest arguments as UAT's
`BuildPlugin`. It disables the unrelated Cargo plugin in the temporary project and
Editor precompile target because an incomplete Cargo installation on the local
machine otherwise stops compilation before Territory code is reached. It does not
edit the engine or add Cargo as a Territory dependency.

Keep the runner's other engine plugins healthy. Use a short output path when
building locally, for example `D:/TFBuild/UE58`. Run the script with a new output
directory. `-Resume` reuses only an existing Territory build host at that path.
Run builds one at a time on a shared machine: Unreal Build Tool uses a shared log.
The content check uses the installed local cache fallback so UE 5.7 and UE 5.8
do not compete for different versions of the same Zen cache service.
