# Version rules

The plugin version and Unreal version are different numbers.

| Example | Meaning |
|---|---|
| `0.3.0-preview.1` | Territory Framework release version. |
| `v0.3.0-preview.1` | Git tag for that exact source commit. |
| `UE5.7` / `UE5.8` | Unreal version used to build a download. |
| `Win64` | Platform supported by that binary download. |

Both Unreal downloads use the same plugin version and source commit. Do not use the UE 5.8 binaries in UE 5.7.

## Changes between releases

- A patch release, such as 0.3.1, is for compatible fixes and help improvements.
- A minor release, such as 0.4.0, can add features and change setup during the 0.x development period. Read its migration notes.
- A major release, such as 1.0.0, marks a new stable compatibility line.
- `preview.1`, `preview.2`, and later previews may still have open release checks. They are marked as prereleases on GitHub.

`VersionName` in `TerritoryFramework.uplugin` matches the release version. The integer `Version` increases for each release; this preview is 6. The release script writes the matching `EngineVersion` into each binary package.

Published tags and downloads must not be silently replaced. A correction receives a new version. `BUILD_INFO.json` records the exact commit, Unreal patch version, Narrative Pro version, and build checks. `SHA256SUMS.txt` lets users check the downloads.

## Moving from 0.2.7

Back up the project and save games before updating. Replace the plugin folder as described in [Install and update](RELEASE_INSTALL.md). Reopen the project, compile your Blueprints, and run Data Validation on your Definitions and profiles.

This release adds authoring checks, faction and quest rules, retake dialogue tools, and fixes described in the [changelog](CHANGELOG.md) and [roadmap](ROADMAP_AND_REMAINING.md). Invalid values that were previously accepted may now fail validation. Fix the reported field in the asset.

The tooltip pass changes no Blueprint names, save fields, gameplay rules, or replication. Upgrading Unreal itself is a separate project migration: keep a copy of the older project. Do not open assets saved by a newer Unreal version in an older version.
