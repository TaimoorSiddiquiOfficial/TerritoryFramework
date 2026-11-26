# Territory Attitude State Machine + Narrative Tagged Dialogue

Status: **implemented and verified in the editor** (2026-10-12). This document describes exactly what
exists, what is still manual, and which constraints apply. Everything below was confirmed against the
running editor or the compiled module, not from documentation.

> **Decision (2026-10-13): the vendor route won — no state machine is used.** The ambient layer is
> driven by Narrative's **own** NPC state tags, which the vendor's own assets already maintain:
> `Narrative.State.NPC.Aggressive` (set in `BP_NarrativeNPC`), `Narrative.State.NPC.Activity.Attacking`
> (`BPA_Attack`), `Narrative.State.NPC.IsBusy` (`BPA_Attack`, interaction seats), `Narrative.State.IsDead`
> (ASC) — plus Narrative's own triggers (`GreetCheck` → Greet, `BTS_Attack` → Taunt, `BPA_Attack` →
> BeginAttacking, `BPA_Attack_Investigate` → `Investigate.*`, `BTS_PlayTaggedDialogue` → Flee,
> `AN_PlayTaggedDialogue` → Carjacked). Playback reaches the player through the NPC Blueprint's
> `ExecutePlayTaggedDialogue` override → `UTerritoryBlueprintLibrary::PlayTaggedDialogueLine`; the
> full chain is in `24_Stealth_Infiltration.md`.
>
> Consequences: `SM_NPC_Attitude` (`/Game/HOPTRENDY/AI/StateMachines/`) is **unused/optional** — its 14
> transition conditions, the per-state `PlayTaggedDialogue` + loose-tag bodies and the delegate → `Send
> Event` bindings are no longer the plan, and no Logic Driver component is added to any NPC (the scripted
> add never reached the generated class anyway). The Territory state tags stay in use, but only where
> Territory itself is the authority: `Wary` while a holstered-weapon warning is being delivered and
> `Hostile` while a faction at war with the owner is inside the Place, both driven from
> `ATerritoryVolume::SyncDefenderDialogueStates` through the defender's own
> `UTerritoryDiplomacyDialogueComponent`. The sections below describe the state-machine variant; they are
> kept as reference, not as the active path.

## 1. Why this exists

Narrative Pro ships a *tagged dialogue* system: an NPC can play an ambient line for a gameplay tag
(`Narrative.TaggedDialogue.Greet`, `.Taunt`, `.Flee`, `.AimedAt`, …). The tag is resolved against
`FTaggedDialogue` entries in a `UTaggedDialogueSet` data asset, gated by `RequiredTags` / `BlockedTags`
/ `Cooldown`, and played through the Tales component.

What was missing **before this work**: nothing in that pipeline read faction attitude or a treaty state,
so every NPC greeted (or taunted) identically regardless of war, alliance, or ceasefire. The talk/interaction
dialogue already respected diplomacy through `UTerritoryDiplomacyDialogueComponent` /
`UTerritoryDiplomacyInteractable` (`TerritoryGuardCharacter.cpp:92,98,259`); the *ambient* layer did not.

## 2. Territory dialogue-state tags (plugin API)

Declared as native tags, following the existing `Territory*Tags.h/.cpp` pattern:

- `Plugins/TerritoryFramework/Source/TerritoryFramework/Public/Core/TerritoryDialogueTags.h`
- `Plugins/TerritoryFramework/Source/TerritoryFramework/Private/Core/TerritoryDialogueTags.cpp`

| Tag | Meaning |
|---|---|
| `Territory.DialogueState.Ally` | alliance, or the interactor shares the NPC's exact faction |
| `Territory.DialogueState.Neutral` | no rich treaty, or a trade agreement |
| `Territory.DialogueState.Wary` | ceasefire: temporary peace after hostility |
| `Territory.DialogueState.Hostile` | the factions are at war (war always wins, even for a shared faction) |
| `Territory.DialogueState.Conversing` | the NPC is inside a running dialogue; ambient lines stay quiet |

Mapping helper (C++ and Blueprint):

- `TerritoryDialogueTags::GetDialogueStateTag(EDiplomacyState, bool bSameFaction)`
- `TerritoryDialogueTags::GetDialogueStateTagForState(ETerritoryDialogueState)`
- `ETerritoryDialogueState { Ally, Neutral, Wary, Hostile, Conversing }` (BlueprintType)
- Blueprint wrapper for the relationship mapping: `UTerritoryBlueprintLibrary::GetDialogueStateTag`
  (BlueprintPure, `Territory|Dialogue`)

Writing the state to an NPC — this lives on the **existing** `UTerritoryDiplomacyDialogueComponent`
(the relationship → dialogue resolver that guards and assault characters already use). No second
"dialogue" component exists; the ambient state is a projection of the same resolved relationship:

| API (`UTerritoryDiplomacyDialogueComponent`, `Territory|Dialogue`) | Effect |
|---|---|---|
| `Apply Dialogue State From Interactor(const APawn*)` | server-only; runs the existing `ResolveRelationshipForInteractor` (multi-faction, disguise-aware perceived factions, war wins) → tag → owner's Ability System. Returns the state |
| `Apply Dialogue State(ETerritoryDialogueState)` | server-only; replaces the previous state tag on the owner's Ability System |
| `Apply Dialogue State From Relationship` | *(removed with the duplicate component — use From Interactor or Apply Dialogue State)* |
| `Clear Dialogue State()` | removes the applied tag (also on `EndPlay`) |
| `Get Dialogue State()` / `Get Active Dialogue State Tag()` | BlueprintPure read-model |
| `Request Tagged Dialogue(Tag, Instigator)` | forwards to the owning `ANarrativeNPCCharacter::PlayTaggedDialogue`; the Narrative gate still applies |

Tests: `TerritoryFramework.Dialogue.StateTagMapping` and `TerritoryFramework.Dialogue.StateEnumMapping`
in `Private/Tests/TerritoryDialogueStateTests.cpp` (relationship↔enum agreement, tag round-trip, unknown
tag → Neutral) — **both passing** (verified via `run_automation_tests`; the whole
`TerritoryFramework.Dialogue` prefix is green, 6/6).

Authority: `UTerritoryDiplomacySubsystem` still owns the relationship and `ANarrativeGameState` still owns
AI attitude. These tags are a read-model; the state machine never writes a treaty or an attitude.

## 3. State machine `SM_NPC_Attitude`

- Asset: `/Game/HOPTRENDY/AI/StateMachines/SM_NPC_Attitude` (spec source: `Docs/specs/SPEC_SM_NPC_Attitude.json`)
- Engine: **Logic Driver Lite** (`SMSystem`, v1.7.4, project copy at `Plugins/SMSystem`, git-ignored)
- Structure: 7 states, 14 transitions, entry state `Unaware` — **compiles, validates** (only the
  informational "no end states" notice remains, which is correct for a long-lived attitude machine)

States: `Unaware · Noticed · Greeting · Wary · Conversing · Hostile · Fleeing`

| Transition | Condition (implemented) | Event to send at runtime |
|---|---|---|
| Unaware → Noticed | event | `PlayerEnteredRange` |
| Noticed → Greeting | event | `GreetReady` |
| Greeting → Noticed | event | `GreetDone` |
| Noticed → Wary | event | `RelationshipChanged` |
| Wary → Noticed | event | `RelationshipChanged` |
| Noticed → Hostile | event | `AttitudeChanged` |
| Wary → Hostile | event | `AttitudeChanged` |
| Hostile → Fleeing | event | `HealthLow` |
| Fleeing → Hostile | event | `ThreatGone` |
| Conversing → Noticed | event | `DialogueEnded` |
| Noticed/Greeting/Wary/Hostile → Conversing | tag check | — (fires on `Territory.DialogueState.Conversing`) |

Events are deliberate: the attitude/treaty sources are delegates
(`ANarrativeGameState::OnFactionAttitudeChanged`, `UTerritoryDiplomacySubsystem` state changes,
`ASC::OnDamagedBy`), so the NPC pushes a transition instead of polling every tick.

## 4. Component wiring (verified)

`USMStateMachineComponent` added to `/Game/HOPTRENDY/Character/Hashir/BP_Hashir` as `AttitudeSM`,
`StateMachineClass = SM_NPC_Attitude_C`, with:

| Property | Value | Why |
|---|---|---|
| `bInitializeOnBeginPlay` | true | build the instance on spawn |
| `bStartOnBeginPlay` | true | start the machine automatically |
| `bStopOnEndPlay` | true | clean shutdown on despawn/destroy |
| `StateChangeAuthority` | `SM_Server` | server-authoritative state decisions |
| `NetworkTickConfiguration` | `SM_Server` | only the server ticks the machine |

## 5. Still manual: the SM's state actions

The machine has structure, conditions, and an event contract, but the *actions* inside its state
graphs are not authored yet. **Why**: Logic Driver Lite does not expose an owning-actor accessor that
Monolith's node authoring can call blind (`GetOwningActor` is not BlueprintCallable on
`USMStateInstance`/`SMInstance`/`SMStateMachineComponent`), so the action graphs are an editor job.
The chain below **does** resolve (verified with `resolve_node`), so this is mechanical work:

```
Self (state instance)
  └─ Get Outer Object        (KismetSystemLibrary)
       └─ Get Component Owner (State Machine Instance)
            └─ Get Owner      (Actor Component)      → the NPC actor
                 └─ cast to your NPC class
                      └─ Get Component by Class (TerritoryDialogueStateComponent)
```

With that reference in hand:

| State | Action to author |
|---|---|
| `Greeting` | `TerritoryDialogueStateComponent.Request Tagged Dialogue(Tag=Narrative.TaggedDialogue.Greet, Instigator=<the same NPC actor>)` — the plugin's own BT service passes the NPC as the instigator too |
| `Wary` / `Hostile` / `Ally` / `Neutral` | `Apply Dialogue State(<matching enum value>)` on entry; `Clear Dialogue State()` is not needed on exit (the next state overwrites it) |
| `Hostile` | one `PlayTaggedDialogue(Narrative.TaggedDialogue.Taunt)`; `Fleeing` → `Flee`; while hostile also `AimedAt` (that tag has dialogues but no trigger anywhere else) |
| `Conversing` | `Apply Dialogue State(Conversing)` on entry, `Clear Dialogue State()` on exit |

Alternatively, drive the same component from your own Blueprint/controller and leave the SM as the
documented state authority — both paths write the same tags, so the data variant selection is identical.

Also still open:

1. **Event senders.** Bind `OnFactionAttitudeChanged` / diplomacy state changes / `ASC::OnDamagedBy` and
   call `Send Event(<name>)` on the SM component (names in §3), or `runtime_send_event` when testing.
2. **`GreetCheck` override — done.** An empty override now exists in `BP_HashirController`, so the
   inherited plugin greet no longer fires and the state machine is the single authority for Greet.
   The graph is intentionally empty; document it as such if you keep it.
3. **Data variants — one shipped, more are designer work.** The Hashir Greet entry now carries
   `BlockedTags = [Territory.DialogueState.Hostile]`, so the NPC stops greeting while at war. Add more
   entries that share a tag with mutually exclusive `RequiredTags` for wary/ally variants — the C++
   gate evaluates every matching entry, so the sets must stay exclusive.
4. **Other NPCs.** Repeat §4 on the guard/assault NPC Blueprints (or their project parents) if ambient
   lines should react there too.

## 6. Constraints and known limits

- **Logic Driver Lite**: no nested state machines, no construction scripts on state graphs, **no
  gameplay tags on state nodes** (Monolith's `set_state_tags` fails with *"No FGameplayTagContainer
  property found on node class 'SMGraphNode_StateNode'"* — that is a Pro feature). Use the ASC tags from
  §2; that is also the only form the tagged-dialogue gate reads.
- **`tag_check` transition conditions do not exist in Lite either** (`set_transition_condition` →
  *"condition_type 'tag_check' accepted but no matching properties found on node class
  SMGraphNode_TransitionEdge"*), so the `Conversing` transitions are driven by `/DialogueBegan` /
  `DialogueEnded` events instead of a tag query.
- **Monolith's `set_transition_condition` only sets reflection flags** — it cannot author a condition.
  `MonolithLogicDriverNodeActions.cpp:495-602` sets `bCanEvaluateFromEvent` for `event_based` and *tries*
  to write the event name into an enum (`EventTriggerType`); the name string is never stored, and LD has no
  condition class property at all. Transitions authored this way compile but stay unconditioned. LD
  conditions are Blueprint graphs ending in `SMGraphK2Node_TransitionResultNode`, so they must be authored
  in the SM editor: **step-by-step runbook in `Docs/SM_NPC_ATTITUDE_COMPLETE_INTEGRATION.md`** (project root
  `Docs/`), which lists every transition GUID with the condition to author, plus the state actions.
- **Monolith 0.23.0 LD-bridge compile fix (local)**: enabling LogicDriver exposed a latent build error in
  `Plugins/Monolith/Source/MonolithLogicDriver/Private/MonolithLogicDriverGraphActions.cpp` — the JSON key
  type is `UE::TSharedString<TCHAR>` in UE 5.8 and the code assigned it straight to `FString`. Fixed locally
  (iterate the `TMap` and convert with `*Key`). A Monolith update will overwrite this; worth reporting upstream.
- **Logic Driver location**: the Fab install sits in a mangled engine folder (`LogicDri…V14`), which
  Monolith's `LogicDriver*` probe does not match. A project copy at `Plugins/SMSystem` is used instead
  (`.gitignore` keeps `/Plugins/*` out of the repository). The engine install stays untouched.
- **Vendor content**: no Narrative Pro source file was modified. Note that `git status` cannot prove this —
  `/Plugins/*` is git-ignored (`Plugins/TerritoryFramework` and `Plugins/HopUtilityForge` are submodules).
  Use file modification times instead.
- **Tagged dialogue cooldowns are not saved** (`FTaggedDialogue::LastPlayTime` is neither `SaveGame` nor
  replicated), and `FTaggedDialogue::MaxDistance` is only enforced by the NPC Blueprint gate, not by C++.
