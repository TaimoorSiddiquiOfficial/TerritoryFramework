# Release checks: 0.3.0-preview.1

These checks cover the community help and content-import batch on 8 September
2026. Both downloads use the same source commit and the same 118 content assets.
`BUILD_INFO.json` in each download records the exact commit and build results.

## Tested versions

| Check | UE 5.7.4 | UE 5.8.2 |
|---|---|---|
| Narrative Pro | 2.4.2 | 2.4.2 |
| Runtime, Editor, and Unreal Header Tool | Pass | Pass |
| Game Development and Shipping compilation | Pass | Pass |
| Included assets checked | 118 | 118 |
| Included Blueprints compiled | 75 | 75 |
| Content validation | 0 errors, 4 warnings | 0 errors, 4 warnings |
| Territory automation | 287 pass, 0 fail | 287 pass, 0 fail |
| Automation tests with warnings | 19 | 20 |
| Installed-package consuming Game build | Pass | Pass |
| Windows cook | 0 errors, 48 warnings | 0 errors, 48 warnings |
| Game staging and packaging | Pass | Pass |
| Packaged Entry map startup | Exit 0, no runtime errors | Exit 0, no runtime errors |

These are local checks on Windows. The GitHub workflow is provided for licensed
self-hosted runners; it is not the source of these recorded results. The startup
check loads an empty Entry map with TerritoryFramework mounted. It does not
exercise a multiplayer battle or a complete game campaign.

## Content and help checks

- All 108 original project asset hashes still match the backup taken before import.
- Both packages contain identical copies of the 118 plugin assets. They have no
  direct dependency on TDA content or the separate RPG UI theme.
- The shared content uses UE 5.7's format. Nine current data assets and 52 actual
  retake dialogue nodes were compared after restoration. The only difference is
  the existing Farm story-capture normalization explained in [Included content](INCLUDED_CONTENT.md).
- The import uses Unreal's Asset Tools and editor APIs. Asset version headers
  were not patched, and the original project maps were not migrated to the copies.
- Unreal Header Tool reports help on all 2,051 category-bearing properties and
  functions. This pass filled 991 previously empty tooltips.
- Links in the new community guides resolve. Version names and package engine
  versions are checked by the release script.
- All 741 local Narrative Pro source files match the installed 2.4.2 package.

Unreal's Asset Tools owns content copying. The existing Definition editor rules
own capture-mode normalization. Unreal's Gameplay Tags manager owns the included
example identities. Narrative retains factions, combat, inventory, and time of day.
This batch introduces no save schema, ownership authority, or replicated fields.
Existing project UI settings still override the new default example button styles.

## Warnings and limits

The four content warnings are the Farm handover's missing camera shot and zero
blend-out time, plus the Blacksmith and Farm owners' prototype mannequin appearance.
Choose your own cameras and characters when using those examples.

The cook warnings come from the installed Engine/Narrative dependency set and
template setup. They include old demo World Partition actors, tag loading, demo
subobjects, input, and Gameplay Cue setup. The cook also reports missing optional
MetaHuman references. The first cook command used `-SkipEditorContent`
and failed because Narrative's projectile and Landmass assets reference Engine
editor resources. The passing cook includes those required Engine assets; no
missing-content errors were suppressed and no vendor assets were edited.

The clean hosts do not contain UDS. Their UDS test verifies missing-dependency
failure and preserves the existing scene. A separate TDA test with UDS installed
passes the lighting/exposure ownership checks, including an empty sky-class pin
and rejection of an explicit incompatible class. This is functional verification,
not visual or performance approval. UDS and Narrative's UDS integration are installed
separately and are not included in the downloads.

The [roadmap](ROADMAP_AND_REMAINING.md) remains the current list of framework work.
Open items include malformed positive assault budgets and duplicate approach
records, production/refund settlement, a real AlMalik World Partition fixture,
the unsymbolized combat/shutdown reports, and visual/performance review of UDS.

A Game executable is not a compiled dedicated-server target. The compiled
TDAServer gate and wider multiplayer certification remain open. This is a
community preview, not a fully certified production release.

## Reproduce the checks

Use `Tools/Build-Release.ps1` with the matching installed Unreal and Narrative Pro
2.4.2 packages. Run `Tools/Validate-Content.py` through Unreal's Python commandlet,
then run the `TerritoryFramework` automation group. See [GitHub builds](31_CI_Artifacts.md).

For a consuming-game check, copy the package into a fresh Narrative-configured
project, build its Game target, cook the Entry map and plugin content, then stage,
package, and start the resulting executable. Keep required Engine content enabled.

The release process checks ZIP integrity, records SHA-256 hashes, compares them
with GitHub's uploaded-asset digests, and verifies the public downloads. Use
`SHA256SUMS.txt` from the same release to check your download. Narrative Pro and
the cooked test games are not included in the Territory ZIPs.
