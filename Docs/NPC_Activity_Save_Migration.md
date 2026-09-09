# NPC activity saves

`BP_TerritoryNPCController` is a thin child of Narrative's controller Blueprint.
Unreal's native component class override installs `UTerritoryNPCActivityComponent`
in the existing `NPCActivityComponent` slot. Native Blueprint casts, inherited
graphs and future parent changes are preserved. No Native graphs are copied.
`ATerritoryNPCController` provides the same component choice for direct C++ use.
Narrative still owns goal generation, activity selection, combat and actor records.
There is no new save system, replicated state, faction identity or AI controller logic.

The adapter fixes three generator save problems in Narrative Pro 2.4.2:

- Repeated saves appended old snapshots. A new save now contains only the current
  saved generators. Removing a generator no longer leaves it in the next save.
- Old snapshots can repeat a class. Loading keeps its last occurrence, in saved
  order. A missing, abstract or retired class is not created.
- A generator already created by the NPC's configuration was skipped during load.
  The adapter restores its saved fields on the same object, before Narrative
  selects activities. It does not initialize or bind that object again.
  Native saves omit fields equal to their class defaults. The adapter expands
  these values on a temporary, uninitialized object before applying the full saved
  state, so saved zero values and empty lists replace later changes. Transient
  state and the live generator's event bindings remain intact.

Loading uses the actual owning Narrative controller, including before `BeginPlay`.
Save and load callbacks only run on the authority. The component name and Native
record fields are unchanged, so existing records can load into the adapter after
an actor is recreated. This does not certify a complete World Partition map test.

## Controller setup

Territory's C++ guard and assault characters use this controller by default.
The portable guard and assault Blueprints select `BP_TerritoryNPCController`.
TDA's corresponding character Blueprints use that same
selection; changing the C++ default cannot replace a saved Blueprint override.
A direct C++ controller does not supply Narrative's Blueprint behavior; use the
provided Blueprint for NPC definitions that depend on those Native assets.
The older `BP_TerritoryGuardNarrativeNPCController` asset is unchanged.

For a project controller that already inherits Native Blueprint behavior, use
the same native component class override rather than changing its parent to a
C++ class. UE 5.7 and 5.8 enable this feature by default under
`[Kismet] bAllowNativeComponentClassOverrides=true`. Keep it enabled when using
this Blueprint. Hashir's generic Native controller still needs a deliberate
project integration and old-save test; this fix does not silently change it.

## Retiring an old generator

On the controller's activity component, **Old Saved Goal Generators To Ignore**
lists exact classes that should not return from an older save. First remove the
old class from the NPC's activity configuration and add its replacement there.
The portable Territory controller lists Native `GoalGenerator_Attack`, because
its current configurations use `GoalGenerator_TerritoryAttack`.

This setting does not stop a generator still used by the current configuration.
It does not discard subclasses or unrelated quest generators. It does not copy
private saved fields into a different class. The replacement uses its own current
configuration. Migration settings are editor defaults, so an old save cannot
overwrite them. Custom projects should list only classes they have deliberately
retired; no global redirect changes Narrative assets.

This change concerns generator records. Generic Native NPC client death, Hashir's
controller migration, saved attack targets, and the observed EQS/decal warnings
remain separate checks.
