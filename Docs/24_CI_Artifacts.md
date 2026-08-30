# GitHub Builds for Unreal 5.7 and 5.8

The repository workflow builds two separate Win64 plugin artifacts:

- `TerritoryFramework-UE5.7-Win64`
- `TerritoryFramework-UE5.8-Win64`

It runs for pushes to `main`, version tags such as `v0.2.8`, and manual **Run workflow** requests.
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
2. verifies that Narrative Pro is installed;
3. runs Unreal Automation Tool `BuildPlugin` for Win64;
4. checks that the packaged `.uplugin` exists;
5. adds `BUILD_INFO.txt` with the engine version and commit;
6. uploads the packaged plugin for 30 days.

The two matrix jobs are independent. A 5.8 compatibility failure does not cancel the 5.7 result,
which makes engine-upgrade problems easier to diagnose.

## Downloading an artifact

Open the workflow run on GitHub and download the artifact matching the target engine. Extract it
into the consuming project's `Plugins/TerritoryFramework` directory, then regenerate project files
or reopen the Unreal project.

An artifact is produced only after compilation succeeds. The workflow does not treat uncompiled
source as a release package.

## Runner troubleshooting

`BuildPlugin` creates a clean host project, but Unreal still loads engine plugins marked as enabled
by default. Keep each runner's engine installation healthy and minimal. For example, an incomplete
KitBash Cargo installation can fail with `Could not find definition for module KitBash3dUsdTools`
before TerritoryFramework compilation begins. Repair or uninstall that engine plugin on the runner;
do not add a false TerritoryFramework dependency merely to hide the runner problem.
