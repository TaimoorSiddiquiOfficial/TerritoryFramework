# Batch 46: data asset categories, creation, and validation

Evidence: `Saved/Verification/20260908_DataAssetAudit/` in the host project.
Started from plugin `5b87f8c`, host `9c48abe1`, on
`hoptrendy/territory-complete-audit`. Read the current declarations, implementations,
consumers, editor integration, tests, and relevant Narrative/UE source before edits.

## Confirmed defects and fixes

- Unreal generated derived Definition categories before inherited categories,
  placing `10` and `11` ahead of `01`. The existing Details customizations now
  sort the complete generated category map. The single inherited story panel and
  Place/District/City field visibility remain intact.
- Situation Profile and Conditional Dialogue Recipe had no named Territory asset
  actions or factories. Both now appear in **Territory Framework > Story & Quests**.
- Named data asset factories silently ignored an incompatible requested class.
  They now reject mismatched, abstract, deprecated, or replaced classes.
- Numeric widget clamps did not validate serialized or scripted values. The
  existing Territory validator now checks editable floating-point fields for
  finiteness and their declared ClampMin/ClampMax, including nested value structs,
  arrays, and map values. Float bounds are compared in their storage precision.
  This does not traverse object references or execute Narrative conditions/events.
- Diplomacy Dialogue Profile lacked validation of assigned dialogues. Empty slots
  preserve the NPC fallback; assigned slots must have a compiled Native dialogue
  template with a root and NPC replies. This uses
  `UDialogueBlueprintGeneratedClass::GetDialogueTemplate()`, matching Narrative's
  initialization contract, rather than assuming the CDO contains the graph.
- Dialogue Recipe accepted finite coordinates outside the editor's int32 range.
  Editor validation now rejects them before the builder creates any package or
  converts graph coordinates. The shared `ValidateRecipe` path is reused.

## Asset and implementation inventory

The registry scan includes on-disk descendants across all mounts, not just a
hardcoded content folder. It found **15 authored instances across 11 types**;
Disguise Profile has no authored instance. Tests cover all **12 concrete types**.

| Data asset | Instances | Existing implementation / consumer authority reviewed |
|---|---:|---|
| Place Definition | 2 | Definition application, physical Volume state, Control capture, child helper configuration |
| District Definition | 2 | Definition hierarchy links and aggregate reduction; physical-only fields stay hidden |
| City Definition | 1 | Definition hierarchy links and aggregate reduction |
| Counter-Attack Profile | 1 | Faction-force lookup, bounded difficulty/vehicle resolution, CounterAttackSubsystem planning |
| Guard Post Definition | 1 | Declarative post settings consumed by GuardSpawnPoint and Native NPC spawning |
| Production Profile | 2 | Recipe validation, checked quantity multiplication, state policy and Economy consumers |
| Stealth Profile | 1 | Default tags/activity and StealthSubsystem perception/investigation settings |
| Disguise Profile | 0 | Exposure configuration, tags/faction eligibility and StealthSubsystem consumer |
| Diplomacy Dialogue Profile | 1 | Existing seven-slot dialogue resolver and Native NPC fallback |
| Quest Cascade Recipe | 1 | Graph validation, tasks/events/conditions, migration, summary and Native quest generation |
| Situation Profile | 1 | Explicit context, existing hierarchy/WorldState/diplomacy/defence read model and validation |
| Conditional Dialogue Recipe | 2 | Speaker/node/reference/reachability/cycle validation and Native dialogue generation |

These are authored configurations, not competing runtime state authorities.
The Guard Post's declarative header is intentional, not an empty implementation.
Existing semantic validators remain in force alongside the numeric checks.
No new faction, capture, dialogue runner, inventory, or persistence system was added.

## Authority, compatibility, and preserved edits

TerritoryFrameworkEditor owns the changed menus, Details ordering, factories, and
asset validation. The coordinate check is WITH_EDITOR in the existing recipe
implementation. There are no gameplay authority changes, RPCs, replicated fields,
save schema changes, runtime asset renames, or GUID/tag migrations. Existing valid
assets require no resave. Existing assets with invalid authored numbers may now
fail validation and receive field-specific diagnostics.

The user had unsaved Farm Definition edits. The disk asset was backed up, those
edits were saved before closing the editor, and that saved version was preserved.
Farm now selects automatic capture. The old map integration test assumed Farm
must always use story capture; it now checks agreement with the authored mode and
mutual exclusion of story bounds and automatic progress. Blacksmith still has its
explicit story-capture regression. The modern validator test now checks all four
specific movement/deployment diagnostics instead of assuming an exact error count.

## Verification

Initial regressions reproduced the category ordering, missing menu/factory entries,
mismatched factory creation, non-finite guard settings, and dialogue validation gap.
The first green focused run passed six tests. Numeric coverage explicitly injects
NaN/infinity into **57 editable root floating-point fields**, plus nested struct,
array, map/audio bounds, and a valid float-boundary case. Native graph generation
also verifies acceptance of a valid compiled dialogue template and rejection of
out-of-range coordinates.

The first full run passed 284/286; the two failures were the stale test assumptions
described above. An initial inventory validation included four transient `/Temp/`
test fixtures; the final scan uses on-disk registry records. Project validation
then compiled **77 Blueprints** and checked **128 assets**, with **zero errors** and
four existing presentation warnings.

`Build_FinalTests.log` and `Build_Game.log` pass; UHT also succeeded after the two
new factories were introduced. `Automation_All_Final.json` records **286/286
passing**, zero failed/skipped. `CookStage.log` finishes with exit zero, zero cook
errors and 30 existing warnings. The distinct `Stage_Batch46` Game executable
matches the verified build by SHA-256 and completes the 90-second localhost
server-mode assault smoke with exit zero and no fatal, assertion, ensure, or
Blueprint runtime errors. This uses Development Game `-server`; it does not replace
a compiled TDAServer target or a physical city streaming test.

All 741 Narrative Pro source files match the installed vendor copy. HopDistrictTest,
AlMalik, handover dialogue, and owner asset hashes are unchanged; Farm matches its
preserved user-edited snapshot. After automation, the editor reloaded the saved
test map without saving test mutations. Farm's Details editor is open, and the
editor reports zero dirty packages.

## Remaining audit boundaries

This batch closes confirmed data asset authoring defects. It does not certify every
runtime consumer of every possible asset combination. Continue the existing
malformed assault-save budget/ledger audit, production/refund settlement review,
and physical AlMalik World Partition fixture. The wider AI crash, content/vehicle,
lighting/performance, and compiled TDAServer gates remain in
[the roadmap](ROADMAP_AND_REMAINING.md). This editor-only batch introduces no new
network behavior requiring a separate two-client gameplay change test.
