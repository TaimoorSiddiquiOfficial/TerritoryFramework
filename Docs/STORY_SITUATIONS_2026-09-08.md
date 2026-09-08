# Story situations and owner reinforcements

This batch is being verified. The published `0.3.0-preview.1` package predates it.

## Which faction does a situation check?

`Territory Situation Condition` uses its Situation Profile. The profile names
one Place and chooses the default faction:

- **Narrative Target Faction** uses the pawn passed by Tales, usually the player
  whose Tales component is running the conversation. It does not automatically
  mean the NPC who is speaking or the faction contesting the Place.
- **Controller Pawn Faction** uses the requesting controller's pawn.
- **Explicit Faction** uses the exact Narrative faction tag in the profile.

The condition's optional **Faction Override** checks another exact faction for
that one condition. Leave it empty to use the profile. For example, one node can
check Bandit holdings while another checks the player's relationship with the
owner. The override never changes the player's capture faction or the shared
profile. Graph text names the faction source being checked.

**Scope** chooses This Place Only, the Place's District, or its City. The system
follows the authored parent tags and the saved/replicated territory directory.
It does not count unloaded actors as missing holdings or invent zero defence.

**Secure Place Count** counts unlocked, Claimed Places. **Owned Place Count
(Including Contested)** also counts Contested Places that still belong to that
faction. Neither counts locked Places. Dominance and share continue to use
secure holdings. Defence power continues to describe the target owner's loaded
District defence front; it is unknown when that front is unavailable.

## Reinforcements before handover

On the existing **Territory Enemy Wave** event, choose **Owner Reinforcements
Before Handover**. Set the sending faction to the current owner, choose a stable
**Scenario ID**, and enable **Start Counterattack Immediately**. The optional
**Opposing Faction** defaults to the explicit Narrative target pawn's faction.

This is one finite story battle in `UTerritoryCounterAttackSubsystem`. The force
uses the Place's existing counterattack profile, wave strategy and approaches.
It may enter a Claimed Place or a Place contested by the stated opponent. It
requires War, available territory, allowed owner/state rules, valid Narrative
NPCs, routes and force budgets. Immediate deployment skips strategic chance,
grace, clock and player-distance delays. It never grants capture pressure to the
reinforcements and never changes ownership.

Use **Territory Enemy Wave / Assault Condition** with the same **Attacking
Faction** and **Scenario ID** to read the outcome. Require both **Latest State =
Defeated** and **Latest Resolution = Attacking Force Defeated**. A cancelled,
route-blocked or failed deployment does not satisfy those conditions. Empty
filters retain the previous behavior and accept any matching Territory assault.

Reopening the dialogue cannot duplicate an active or completed named
reinforcement encounter. A cancelled attempt can be retried. Named reinforcement
victories remain in the existing saved and replicated assault records after
ordinary history is trimmed, so a later save or late join cannot relock the
handover. Use one stable Scenario ID per authored encounter, not generated IDs
on every dialogue visit.

For a pressure condition, the existing **Territory Control Progress Condition**
now offers an optional exact **Contesting Faction**. Empty means any faction's
current progress; a supplied tag must match the Place's actual contesting
faction. Combine it with the assault outcome conditions where the story needs
both facts. Capture still goes through `UTerritoryControlSubsystem`.

## Authoring and migration

Existing enum values and default condition behavior are preserved. New scopes,
queries and the story launch mode are appended. No Narrative Pro source is
modified. The new story mode requires this plugin version when loading its save;
older plugin builds do not understand the new mode. No old save fields are
renamed. `FTerritorySituationReport` is a disposable query, not campaign state.

The editor's `ArrangeDialogue`, `AddDialogueReply`, `SetDialogueNodePosition`
and `ConnectDialogueReply` functions operate on
existing Narrative graphs through their schema and compiler. Layout writes both
the graph coordinates and Native runtime node positions. This matters because
Narrative uses position to select reply priority. Layout preserves that order;
intentional story branch changes must be reviewed separately. None of these helpers
saves assets automatically. `AddDialogueReply` registers a new node in both the
Native graph and its runtime template, rejects duplicate IDs and connects it
through the Native schema. Shared plugin content must retain UE 5.7-compatible
serialization before the next dual-engine release.

## TDA Blacksmith example

The project handover now uses the existing `NotNow` runtime ID for a connected
reinforcement branch. The previous disconnected node was absent from Narrative's
runtime node list and did not survive loading. Bandits must still own the Place,
have at least one unlocked owned Place in the City, and be at War with Heroes.
The original defenders must be cleared. This branch checks ownership including
Contested; the older Territory Ownership condition deliberately requires secure
Claimed ownership and is unsuitable for this check.

The event sends Bandits against the requesting Narrative pawn's faction, starts
immediately and names the encounter `Blacksmith_BeforeHandover`. Heroes' Claimed
state entry, the offer, the selected reply and the capture event all require that
encounter's verified defeat. Other faction state rules remain separately authored.
This is one named story encounter. Repeating it after every later loss needs a
deliberate encounter/reset policy; do not generate arbitrary new IDs per visit.

The old explicit Claimed-entry wave is removed from the Heroes override. Both
copies of `NQ_CaptureBlacksmith` skip their post-capture wave when this named
pre-handover battle is already defeated. Without that story outcome, their
existing example behavior remains. The framework's strategic counterattack
policy remains separate. Peace does not count as a battlefield victory; the
example's handover remains gated if a treaty cancels the battle.

## Verification record

Editor/runtime/UHT and Game Development compilation succeed on UE 5.8.2. All 288 automation tests
pass (269 without warnings, 19 with warnings). The new Native reinforcement test
checks admission, client-role rejection, duplicate requests, missing routes,
physical NPC admission, zero capture registration, finite death accounting,
target registry absence, save/load, story-history retention, faction/scenario
filters, and ceasefire cancellation. The final Native editor recipe regression
also passes after adding the reply-authoring helper.

The HopDistrictTest listen-server and two-client probe passes: eight physical
Native NPCs arrive through two vehicles, dismount and contribute zero capture
pressure. Reopening the dialogue does not duplicate the force. An early capture
event and both client force-capture attempts fail. One living reinforcement
keeps the handover blocked; the eighth death opens CaptureOffer, and the normal
Tales choice reaches Confirmed and transfers the Place to Heroes on both clients.
A real Narrative save followed by two loads preserves the same completed story
record and Heroes ownership on all three worlds. Deaths use Native ASC Instakill
to test finite accounting; this is not a shooting/combat-balance certification.

An earlier long probe exposed the second quest wave: it physically retook the
undefended Place before the save check. The quest event now recognizes the
completed pre-handover encounter, and the repeated probe passes with no duplicate
immediate post-handover wave. This was not an ownership-save failure.

Six project dialogue graphs, including Hashir's two, are arranged with their
texts, stable IDs, conditions, events and links compared before and after.
The final wider inventory covers 237 assets and compiles 143 Blueprints in memory.
Farm's null weapon entry is removed while preserving its WeaponUpgrades tag and
upgrade level; the benefit now has the label "Weapon upgrades". It grants no
weapon until an actual item is selected. Hashir's Native greeting tag is valid;
`NPC_Hahsir` is a redirector and its stable NPC ID is intentionally preserved.
His main dialogue currently has only its root. Act 1 remains to be authored.

TDA redirects standard Narrative UI assets to its installed RPG theme. Seven
asset-reference errors came from the project reference policy not allowing those
redirects. A narrow TDA rule allows TerritoryFramework to reference NP_RPGUITheme
in this project; no theme dependency is added to the portable plugin.
Cold TDA validation now passes with zero errors and eight presentation warnings:
both Farm dialogues retain gameplay framing and a zero blend-out time, and four
example owner definitions use the prototype mannequin. No plugin asset has a
project-content dependency. These are presentation choices, not missing captures.

The first packaged combat run found a late faction callback in Hashir's Native
`GoalGenerator_Attack` after his controller was destroyed. His original pacifist
configuration needs that generator for threat information. The existing Territory
combat generator also adds EQS combat scoring, so it is not a minimal replacement.
The project now uses `GoalGenerator_Hop_SafePerception`, a direct Native child
that overrides only `RefreshPerceivedActors` through the existing
`RefreshParentPerceivedActorsSafely` function. `AC_HashirPacifist` retains all
eight Native activities and the 0.5-second rescore interval. Hashir's ID and other
NPC settings remain. No plugin or Narrative source changes were needed for this
content fix.

Its server lifecycle probe in a three-world PIE session passes live refresh,
unpossession, controller destruction and subsequent faction broadcasts. Two
additional findings remain open. A separate real save followed by immediate
load crashed in `UNarrativeSaveSubsystem::LoadActorFromRecord`, line 788, at its
saved-component loop after actor restoration. The exact invalid object or record
and the responsible callback have not been identified. Separately, Native `BP_NarrativeNPC`
calls `RemoveAllGoals` with no local AI component when Hashir dies on clients.
Neither is covered by the new perception override. Preserve these release
blockers instead of treating the earlier Blacksmith save pass as proof of all
NPC save/death cases. Existing saves that already contain the old Native goal
also need a deliberate migration check; changing a definition default does not
rewrite saved activity instances.

UE 5.7.4 builds the Editor, Development game and Shipping game successfully.
All 288 automation tests pass there (268 without warnings, 20 with warnings).
The edited shared quest/task assets were restored onto canonical 5.7 assets
through Unreal's APIs. Their seven-node quest topology and authored settings
match the 5.8 snapshots. The four shared dialogues were arranged in 5.7 and
returned to TDA. Cold 5.7 validation loads all 118 plugin assets and compiles all
75 Blueprints with zero errors and four presentation warnings. All 10 in-scope
dialogue graphs have distinct node positions (130 nodes total).

TDA's final batch cook, stage and package pass with zero errors and 30 existing
content/tooling warnings. The matching packaged Game completes its 90-second
server-mode assault smoke with exit zero and no fatal, assertion, ensure,
Blueprint runtime error or invalid-controller script warning. This is an assault
startup/combat check with an isolated test user directory; it does not run clients
or save/load and is not a compiled TDAServer target. The two clean-engine installed-package consuming checks,
dedicated-server/streaming gates and newly recorded Native restore/client-death
findings remain open before publication. The published preview is unchanged.

### Authority and lifecycle

The Territory volume still owns ownership and state. The control subsystem still
validates handover. The counterattack subsystem owns the finite force, and the
world state stores and replicates its existing records. Narrative continues to
own faction identity, NPC spawning/combat, Tales and save operations.

| Step | Existing owner of the step | Saved or presented result |
|---|---|---|
| Request and preparation | Counterattack scheduler, Territory state rules and diplomacy | Named assault, opponent, decision, force and immediate flag |
| Grace, warning and activation | Counterattack scheduler | Existing assault state and notifications; the immediate story request skips waiting |
| Route and physical arrival | Existing approaches, vehicle deployment and Narrative spawning | Assault ID, finite deployment usage, survivor and vehicle checkpoints |
| Combat and casualties | Narrative ASC/death with Territory assault participants | Living, pending, killed and withdrawn counts; zero capture registration in this mode |
| Last attacker removed | Counterattack scheduler | Defeated plus AllAttackersRemoved, retained as the named story outcome |
| Handover | Native Tales capture event and Territory control subsystem | Verified ownership result and replicated Territory summary |
| Reload or late join | Narrative save and Territory world state | Same encounter result and owner; no new force or ownership roll |

The live probe's final recorded example is:

```text
Target: Territory.HavenReach.MarketSquare.Blacksmith
Sending faction: Narrative.Factions.Bandits (current owner before handover)
Opposing faction: Narrative.Factions.Heroes
Active / desired / maximum guards: 0 / 3 / 7
Reserve guards: 3
Attack power: 100
District defence power: 1.5
Power ratio: 66.6667
Strategic value: 1
Attack priority: 127.2783
Launch probability: 1 (explicit admitted immediate request)
Estimated success probability: 0.9852 (planning data only)
Planned / alive / pending / killed force at defeat: 8 / 0 / 0 / 8
Withdrawn force: 0
Approach: Blacksmith_WestRoad
Wave strategy: BackToBack, 4 attackers per wave
Vehicle deployments: 2 of 2
Notification: no warning sent; immediate physical deployment
Proximity / grace / strategic clock: bypassed by the explicit immediate request
Capture permission for reinforcements: false
Result: Defeated / AllAttackersRemoved; normal Tales handover then succeeds
```

The new test initially exposed an invalid synthetic Native Game State spawn;
the fixture now uses the established unsaved clock pattern. The layout test also
required an actual Tales component and a strong recipe reference across Native
Blueprint compilation. The editor builder now retains that recipe through its
post-compile checks.

See [remaining work](ROADMAP_AND_REMAINING.md) for the content handover check,
broader asset audit, Hashir preparation and older release gates.
