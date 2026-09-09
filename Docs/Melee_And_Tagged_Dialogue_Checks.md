# Melee finishers and tagged dialogue

Narrative Pro owns melee combos, damage, executions and dialogue playback.
Territory does not replace those systems.

## A healthy guard is executed on the first hit

Check the attack ability's **Default Attack Damage**, the player's **Attack Rating**,
and the target's **Health** and **Armor**. Narrative's finishing-blow check includes
Attack Rating and Armor. A normal attack can therefore finish a target at full
health when the player's equipment is much stronger than the target.

In TDA's reported setup, the demo sword ability has 55 base damage. The starter
clothes add 122 Attack Rating: boots add 2, the Chief chestplate adds 100, and the
Chief platelegs add 20. The resulting light hit is 122.1 against 100 health and
zero Armor. Narrative correctly chooses an execution for that lethal hit.

TDA's `IC_TaimoorPlayerItems` now grants the existing project Territory sword,
whose combo ability has 18 base damage. With the same clothes, its base light hit
is 39.96 and its base heavy hit is 59.94 before animation-specific multipliers.
The normal combo can run before a finishing execution. Other items and Narrative's
combat formulas are unchanged. The same ability and damage rules apply to defender
and assault NPCs.

This changes a new character's starting item collection. An existing save still
owns its saved items. A saved demo sword remains the stronger demo sword; grant
the project sword through Narrative inventory if an existing character should use
the new starter balance. No automatic inventory replacement or save migration runs.

## A tagged greeting never starts

The **Dialogue** field in a tagged dialogue set must refer to a generated dialogue
class. For example, `DBP_Hahsir_Greet.DBP_Hahsir_Greet_C` is the class;
`DBP_Hahsir_Greet.DBP_Hahsir_Greet` is the Blueprint asset. Narrative's async class
loader cannot start a dialogue from the asset reference.

TDA's Hashir greeting had the asset reference. It now points to the generated
class. The existing dialogue text, reply, speaker IDs and cooldown are preserved.

Territory's editor validator now checks tagged dialogue sets under `/Game` and
`/TerritoryFramework`. It reports empty row references, Blueprint asset references,
wrong class types and abstract/deprecated classes. An intentionally empty set is
valid. This is an editor check; it does not add a runtime dialogue authority or
change Narrative Pro content.

There are no new replicated fields, SaveGame fields, actor IDs or streaming rules.
Generic NPC client death, Hashir's activity-component migration, saved AI targets,
and the separate EQS/decal findings remain on the main resolution plan.

## Verified behavior

The HopDistrictTest listen-server/two-client check plays Hashir's greeting and
reply in all three worlds. A remote player performs normal sword combos followed
by an execution against both a defender and an assault guard. The assault guard
comes from the real reinforcement scheduler and completes its vehicle arrival.
Both deaths replicate; the assault loses one living participant and records one
kill. Target distance, facing and AI are controlled for this check.

All 297 automation tests pass on UE 5.8 and UE 5.7. TDA's current asset validation
has zero errors, and the UE 5.8 package/90-second combat smoke pass. See the
[verification ledger](Finding_Resolution_Plan_2026-09-08.md) for warnings and the
remaining release gates.
