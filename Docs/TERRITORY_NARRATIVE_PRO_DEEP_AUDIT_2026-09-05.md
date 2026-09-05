# Territory Framework + Narrative Pro Deep Audit

> **Date:** 2026-09-05
>
> **Audience:** game designers, Blueprint developers, C++ developers, and plugin maintainers
>
> **Installed baseline:** Territory Framework 0.2.7, Narrative Pro 2.4.2, Unreal Engine 5.7
> **Scope:** runtime and editor C++, Definition authoring, Narrative integration, gameplay HUD,
> packaging, validation, tests, and release polish

## Result in plain English

The framework has a sound authority model. A Definition describes reusable setup, the placed
Territory actor owns live local state, the Registry answers spatial questions, the subsystems own
gameplay mutations, and Narrative Pro remains the authority for factions, inventory, abilities,
quests, dialogue, POIs, notifications, and layered menus.

No new critical gameplay defect or production-code stub was found in this audit. The requested HUD
problem was real: a broad City could keep the passive Territory card visible during ordinary
travel. It is now controlled per City, District, or Place Definition.

```text
City travel volume       Show Passive Gameplay HUD Card = Off
Quiet civilian District  Show Passive Gameplay HUD Card = Off
Capturable Blacksmith    Show Passive Gameplay HUD Card = On
```

This hides only the small location/capture card. Capture, ownership, alerts, POIs, map markers,
Command Center intelligence, and management still work.

## What changed in this audit

### 1. Definition-owned passive HUD policy

Every `UTerritoryDefinition` now exposes:

```text
09 Presentation
└── Show Passive Gameplay HUD Card
```

- City default: **off**. A City is normally a large ambient travel region.
- District default: **on**.
- Place default: **on**.
- A designer can override any individual asset.

`ATerritoryVolume::ShouldShowGameplayHUD()` is a read-only Blueprint query. The passive HUD checks
the exact Territory selected by the spatial Registry. An explicit off does not fall back to a
parent card, because that would make the option feel broken.

The widget collapses before building detailed text when the option is off. It clears only its
temporary on-card assault message. It does not remove the real assault record or notification.

### 2. Story Outcome Preview

Every Definition report now includes **Presentation > Passive gameplay HUD card**. It says whether
the card may appear and lists the systems that remain available. This makes the result understandable
before PIE.

### 3. Regression protection

Automation now verifies:

- the property is exposed on the shared Definition;
- the runtime query is Blueprint-pure;
- City defaults off while District and Place default on;
- a Place actor reads live policy from its Definition;
- Story Outcome explains both the hidden result and unaffected strategic UI.

## Architecture findings

| Area | Finding | Decision |
|---|---|---|
| Definition authority | City, District, and Place share one Definition base. City/District Details already hide physical Place-only fields. Validation rejects physical capture, guards, stealth, income, and counterattack authoring on aggregate parents. | Keep. It is safer than splitting runtime authority again. |
| Runtime authority | `ATerritoryVolume` exposes read-only applied data. Control, economy, diplomacy, counterattack, and other subsystems perform mutations. | Keep. New HUD policy is static presentation data, so it does not need save or replication state. |
| Spatial selection | The Registry selects the most-specific loaded Territory. Existing visibility logic handles locked ancestors. | Keep. The new policy is evaluated after selection, so disabling one exact area is predictable. |
| Narrative UI | Territory menus use Narrative's registered `UI.Layer` containers through an activatable widget. The passive card remains a non-interactive HUD child. | Correct. Epic recommends activatable widgets for layered interaction, while passive display widgets do not all need to be activatable. |
| Narrative gameplay systems | Inventory production calls Narrative inventory APIs; diplomacy updates Narrative faction attitude; Territory abilities/effects use Narrative's ASC/GAS; POI and notification presentation use Narrative systems. | Keep Narrative as the owner. Territory should adapt, not duplicate these systems. |
| Vendor boundary | No Narrative Pro source change is required for this feature. The installed vendor plugin remains read-only. | Keep this release rule. |

Narrative Pro 2.4.2's installed `UNarrativeGameplayHUD` exposes registered layer containers,
menu opening, HUD hiding, and notification functions. Its `UNarrativeActivatableWidget` derives
from CommonUI and owns back, focus, input-mode, and block-tag behavior. This matches Epic's
[CommonUI overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-advanced-multiplatform-user-interfaces-with-common-ui-for-unreal-engine),
[design guidance](https://dev.epicgames.com/documentation/en-us/unreal-engine/design-guidelines-for-using-commonui-in-unreal-engine),
and [`UCommonActivatableWidget` API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/UCommonActivatableWidget).

## Ranked improvement plan

### High — add real validation to release CI

The GitHub workflow packages the plugin for UE 5.7 and 5.8, but `BuildPlugin` proves mainly that
source and plugin content can compile/package. It does not run the Territory automation suite,
Blueprint compilation, Data Validation, a host-project cook, or a map smoke test.

Recommended pipeline:

1. Package the plugin as today.
2. Install it into a small licensed test-host project that also has the matching Narrative Pro.
3. Run `TerritoryFramework.*` automation.
4. Run asset Data Validation and Blueprint compile.
5. Cook one representative map.
6. Upload the plugin and the validation logs together.

This is the best next reliability investment because many important contracts cross C++, Blueprint,
assets, Narrative Pro, and a real world. Unreal's
[Data Validation system](https://dev.epicgames.com/documentation/unreal-engine/data-validation-in-unreal-engine)
is intended to catch asset-specific setup mistakes, while the current workflow does not execute it.

### High — make platform support match tested support

The plugin descriptor advertises Win64, Android, and Linux. Current CI builds only Win64. Either add
Android/Linux compile and cook lanes, or document Win64 as the verified platform and treat the other
allow-list entries as provisional. A public plugin should not make a stronger support promise than
its evidence.

### Medium — define the Primary Asset packaging contract

Territory Definition, stealth, disguise, and guard-post assets return Primary Asset IDs, but the
host project's Asset Manager scan list currently registers only Map, PrimaryAssetLabel,
NPCDefinition, and PlayerDefinition. Direct references from placed actors and hierarchy assets cook
correctly, but an unreferenced asset cannot be promised as runtime-discoverable by ID.

Choose and document one policy:

- **Reference-only:** every playable Definition must be referenced by a placed actor, City hierarchy,
  or Primary Asset Label; or
- **Discoverable:** ship explicit Asset Manager scan rules for all Territory primary asset types and
  add an automated lookup/cook test.

Epic documents this distinction in
[Asset Management](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine)
and [`UPrimaryDataAsset`](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UPrimaryDataAsset).

### Medium — replace broad UI polling with revision-driven refresh

The passive info card polls every 0.5 seconds. The Command Center polls every second in addition to
its event bindings, and management/economy screens also keep fallback timers. The polling is safe and
helps late join, but it can repeatedly rebuild unchanged view models as a game grows.

A measured improvement is:

1. keep a slow late-join/desync safety poll;
2. add one replicated/read-only Territory UI revision;
3. refresh large screens only when the relevant revision changes;
4. suspend screen timers while the Narrative activatable screen is deactivated;
5. profile before and after with hundreds of Places.

Do not make every widget listen to every low-level gameplay delegate. One revision boundary keeps UI
decoupled from ownership authority.

### Medium — complete a physical World Partition fixture

Stable IDs and stream-safe registration are automated, and unloaded District directory behavior is
covered. The remaining release proof is a map that physically streams out and back in a Place, its
guard posts, and a live assault. World Partition automatically divides the world into streamable
grid cells, so this test exercises lifecycle behavior that an in-memory unit fixture cannot fully
represent. See Epic's
[World Partition overview](https://dev.epicgames.com/documentation/unreal-engine/world-partition-in-unreal-engine)
and [Data Layers guidance](https://dev.epicgames.com/documentation/unreal-engine/world-partition---data-layers-in-unreal-engine?lang=en-US).

### Medium — add UI accessibility and input verification

The Territory type hierarchy, title case, selected state, and hover state now have tests. The next UI
polish gate should cover:

- keyboard and gamepad focus order;
- back action and focus restoration through Narrative's layer stack;
- 100%, 125%, and 150% UI scale;
- 16:9, ultrawide, and safe-zone layouts;
- color-blind readability without depending only on gold/green/red;
- long translated names and right-to-left layout pressure.

Epic's
[CommonUI input guide](https://dev.epicgames.com/documentation/unreal-engine/commonui-input-technical-guide-for-unreal-engine?lang=en-US)
supports treating input routing and focus as part of the UI contract, not as final visual cleanup.

### Low — enforce the Narrative compatibility baseline in CI

The source and documentation correctly target Narrative Pro 2.4.2 on UE 5.7, while the UE 5.8 CI
lane asks only for any installed `NarrativePro.uplugin`. Record the installed Narrative version in
the artifact and fail when it is outside an explicitly supported matrix. This will turn an accidental
vendor update into a clear compatibility result.

## What should not be refactored

- Do not save or replicate `bShowGameplayHUD`; it is static project presentation policy.
- Do not hide alerts, POIs, map markers, or the Command Center when the passive card is disabled.
- Do not add a second Territory menu stack beside Narrative CommonUI.
- Do not copy Narrative inventory, faction, quest, dialogue, or ability state into a parallel owner.
- Do not move City/District to physical capture actors. Their control is derived from child Places.
- Do not edit Narrative Pro vendor assets to solve a Territory project integration problem.

## Verification performed

| Check | Result |
|---|---|
| Production C++ TODO/FIXME/stub scan | No unresolved production stub found |
| Narrative private-header or `.cpp` dependency scan | None found |
| Narrative Pro vendor changes required | None |
| UE 5.7 `TDAEditor Win64 Development` after final source changes | Passed |
| UE 5.7 `TDA Win64 Development` after final source changes | Passed, including UHT warnings-as-errors |
| `TerritoryFramework.UI.*` focused automation | 17/17 passed |
| `TerritoryFramework.Editor.StoryOutcome.*` focused automation | 6/6 passed |
| Full `TerritoryFramework.*` automation | 211/211 passed; no failed or skipped test |
| Markdown artifact structural review | Headings, tables, local index link, and descriptive source links checked; no separate rendered-page preview was available |

The full log contains no Blueprint Runtime Error, Accessed None, assertion, fatal error, or Territory
error. Its warning-producing cases are intentional invalid-input and cleanup fixtures. UE 5.8,
Android, Linux, physical World Partition streaming, and visual accessibility checks require their
matching environments and are not claimed by this local audit.

## Source ledger

Local primary evidence:

- Territory runtime/editor source and tests in this plugin working tree.
- Installed Narrative Pro 2.4.2 public source under
  `Plugins/Narrativeed3f9374a6eV6/Source`, especially `NarrativeGameplayHUD.h`,
  `NarrativeGameplayHUD.cpp`, `NarrativeActivatableWidget.h`, and Narrative player-controller
  HUD creation.
- Project `Config/DefaultGame.ini` Asset Manager scan rules.
- `.github/workflows/build-unreal-artifacts.yml`.

External primary evidence:

- Epic Games, [CommonUI overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-advanced-multiplatform-user-interfaces-with-common-ui-for-unreal-engine).
- Epic Games, [CommonUI design guidelines](https://dev.epicgames.com/documentation/en-us/unreal-engine/design-guidelines-for-using-commonui-in-unreal-engine).
- Epic Games, [`UCommonActivatableWidget`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/UCommonActivatableWidget).
- Epic Games, [CommonUI input technical guide](https://dev.epicgames.com/documentation/unreal-engine/commonui-input-technical-guide-for-unreal-engine?lang=en-US).
- Epic Games, [Gameplay Tags](https://dev.epicgames.com/documentation/unreal-engine/using-gameplay-tags-in-unreal-engine?lang=en-US).
- Epic Games, [Asset Management](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine).
- Epic Games, [`UPrimaryDataAsset`](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UPrimaryDataAsset).
- Epic Games, [Data Validation](https://dev.epicgames.com/documentation/unreal-engine/data-validation-in-unreal-engine).
- Epic Games, [World Partition](https://dev.epicgames.com/documentation/unreal-engine/world-partition-in-unreal-engine).
- Epic Games, [World Partition Data Layers](https://dev.epicgames.com/documentation/unreal-engine/world-partition---data-layers-in-unreal-engine?lang=en-US).
