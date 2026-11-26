# 44 — Stealth: Vendor Reality vs Territory Reality, and the Modular Seams

Status: investigation complete (source + Blueprint graphs + live sense configs + tests).
Scope: Narrative Pro stealth/perception as shipped, TerritoryFramework's stealth stack, verified gaps, and the modular structure needed for multi-system integration loops.

Everything below was verified from code, Blueprint graphs, asset defaults, or tests — not from comments or documentation.

---

## 1. Vendor (Narrative Pro) — what actually exists

### 1.1 The attribute

`UNarrativeAttributeSetBase::StealthRating` (0–100, replicated).

Writer chain:

| Writer | Where |
|---|---|
| `GE_CrouchStealth` | crouch toggle ability applies it |
| `GE_FeatherQuiet` | Sneak skill-tree perk |
| `GE_EquipmentModifier` | `UEquippableItem::StealthRating` → `SetByCaller.StealthRating` |
| `GE_DefaultPlayerAttributes` / `GE_DefaultNPCAttributes` | baseline |

The header comment claims "100 Stealth rating = not perceptible, 50 = 50% less alert".

### 1.2 The only consumer: footsteps

`BP_AnimNotify_FoleyEvent` (footstep AnimNotify):

```
Received_Notify
  → GetAINoiseEventLoudness()  = Clamp( (1 − StealthRating/100) × AINoiseEventLoudness )
  → GetAINoiseEventRange()     = same scaling for range
  → ReportNoiseEvent(Location, Loudness, Instigator, MaxRange, Tag = "Footstep")
  → PlaySoundAtLocation + SpawnSystemAtLocation(FootstepFX)
```

So in shipped content `StealthRating` scales **footstep noise only**. Sight, AI goals and hostility never read it.

### 1.3 Sight visibility model

`ANarrativeCharacter` implements the perceivable interface used by `UAISense_Sight`:

```
CanBeSeenFrom() → PerformSightTrace()  (ECC_Visibility LOS, ignores own actors)
                → OutSightStrength = CalcSightStrength(Start, End, Looker)
```

`CalcSightStrength` C++ default (`NarrativeCharacter.cpp:428`):

```
DistanceSeenPct = MapRangeClamped(distance, 300 → 5000 cm, 1 → 0)
× (IsCrouching ? 0.5 : 1)
```

(`Dot` is computed and unused.)

Overrides:

| Class | Override |
|---|---|
| `BP_NarrativeNPC` | **empty** — NPCs use the C++ default |
| `BP_NarrativePlayerCharacter` | distance Curve Float × angle (`Acos` deg → MapRange 0–100° → 1–0) × crouch Select × head/body Select (0.25) × `DotDistanceMultiplier` |

**No StealthRating anywhere in the sight model.**

### 1.4 Perception configuration (authored in BP controllers)

`BP_NarrativeNPCController` → component `AIPerception`:

| Sense | Values |
|---|---|
| `AISenseConfig_Sight` | Radius **6000**, LoseSight **7000**, Peripheral **180°**, MaxAge **0**, detects enemies/neutrals/friendlies |
| `AISenseConfig_Hearing` | Range **10000**, MaxAge **0**, detects all |
| `AISenseConfig_Damage` | MaxAge 20 |

`BP_TerritoryGuardNarrativeNPCController` inherits the identical values.

### 1.5 AI reaction

`GoalGeneratorBase_Stimulus` binds the controller's `On Perception Updated` and runs:

```
Can Create Goal From Perceived Actor?
   GoalItemMap contains?           (no duplicate)
   GetAttitude ∈ AllowedAttitudes
   Sense class ∈ AllowedSenses
   ActorClassToSense matches
→ Create Goal Item From Stimulus → AddGoalItem
```

**Perception strength is never consulted.** Goals used: `BPA_Attack`, `BPA_Attack_Ranged`, `GoalGenerator_Flee`, with `S_SightStrengthByDistance`, `S_SightAngleMultiplier`, `S_SoundFalloff`, `S_SoundMaxDistance` and EQS for approach quality.

### 1.6 What the vendor does not have

Hiding spots, stealth zones, disguise, suspicion/awareness meter, investigation, evidence taxonomy, exposure states, escalation scopes, contest weapon gate, break-stealth GAS event. `BP_Cover` is combat cover, not hiding.

---

## 2. Territory — what actually exists

### 2.1 Data

`UTerritoryStealthProfile : UPrimaryDataAsset` — sections:

```
01 Infiltration   bAllowStealthInfiltration, EscalationScope
02 Sight          ImmediateSightExposureThreshold, MinimumSightEvidence (0.2),
                  GuardDetectionMultiplier, SightSuspicionGainPerSecond,
                  SuspicionDecayPerSecond, MaximumStealthRating,
                  bRespectNarrativeInvisibleTag, bPointBlankSightAlwaysExposes + distance
03 Evidence       fire-seen, fire-unseen, damage, seen kill, unseen death,
                  anonymous-can-expose, outside-threat evidence,
                  Gunshot/BulletImpact/Corpse/ThrowableDistraction suspicion
04 Investigation  MaximumInvestigators, radius, duration, acceptance radius,
                  ShotCorrelationWindow, InvestigationActivityClass
05 Ability        BreakStealthGameplayEventTag, cancel stealth ability tags,
                  remove stealth GEs (e.g. GE_CrouchStealth)
06 Disguise       owning-faction uniform, minimum quality, clearance tags,
                  compromise-on-failed-check
07 Contest Gate   wielded-weapon requirement, war handling, warning tag + cooldown
```

Selected per Territory Definition and overridable per State Config (`FTerritoryStateConfig::StealthProfileOverride`).

`FTerritoryInfiltrationSnapshot` — ExposureState, LastEvidence, Suspicion, LastEvidenceLocation, EstimatedSourceDirection, ConfirmingObserverCount, bInsideTerritory.

### 2.2 Runtime flow

```
ATerritoryGuardCharacter (extends ANarrativeNPCCharacter)
 └ UTerritoryStealthObserverComponent   ← thin adapter, server-only
      binds controller AIPerception (same component the vendor's goals use)
      timer refresh → GetCurrentlyPerceivedActors(Sight)
      sight:  raw × GuardDetectionMultiplier × (1 − StealthRating/MaximumStealthRating)
              invisible tag → 0; point-blank override; IsExpired/WasSuccessfullySensed guards
      hearing: tag "DistractionThrowable" → ThrowableDistraction evidence
               tag "Gunshot"            → FireSeen / Gunshot
               empty tag + recent shot  → BulletImpact (ShotCorrelationWindow 0.3 s)
      → UTerritoryControlSubsystem::ReportStealthEvidence(...)   [authority]
           suspicion 0–1, gain/decay, thresholds,
           Undetected → Suspicious → Exposed,
           EscalationScope → LocalAlarm | TerritoryConflict | FactionWar,
           OnStealthEvidenceReported + snapshot for UI/Tales
      → investigation: UTerritoryInvestigationGoal/Activity
           (≤ MaximumInvestigators, radius 5000, duration 12 s, acceptance 150)
      → exposure: break-stealth GAS event, cancel stealth abilities, remove stealth GEs
```

Distraction chain:

```
GA_TerritoryDistraction (ServerOnly, montage-driven, hand_l socket, camera aim + trace)
 → ATerritoryDistractionProjectile
 → UTerritoryDistractionComponent (Loudness 1.0, MaximumRange 3000)
 → UAISense_Hearing::ReportNoiseEvent(tag = DistractionThrowable)
 + GAS DistractionImpactEvent
```

Tales: `UTerritoryStealthPolicyCondition`, `UTerritoryStealthEvidenceCondition`,
`UTerritorySetStealthOverrideEvent`, `UTerritoryRevealInfiltratorEvent`,
`UTerritoryClearExposureEvent`, `UTerritoryReportDistractionEvent`.

Tests: `FTFStealthProfileDefaults`, `FTFStealthSightFormula`, `FTFStealthAuthoringContract`,
`FTFStealthNarrativeIntegrationContract`, `FTFDisguisePolicy`, `FTFDisguiseAuthoringContract`,
`FTFStealthContestWeaponGate` + `TerritoryDistractionLifecycleTests`.

---

## 2A. Contest formula vs attack goal — the chase loop (fixed)

### The formula as it was

```
Story bounds (ATerritoryVolume::ReconcileStoryBoundsContesters, per player inside):
  ShouldAdmitStoryBoundsContest(Pawn, Faction)
    bAggressor = IsAggressiveStealthEvidence(Snapshot.LastEvidence)     ← no age check
    → DoesPawnAdmitStoryContest(Pawn, Place, Profile, bAggressor)
         = !RequireWieldedWeapon || War || Wielding || bAggressor
  admitted → TryRegisterStoryBoundsContester → TryRegisterAttackerInternal(StoryBounds)
         → CommitCaptureReadModel(Contested)                           ← this is the "AI alert"

Guard combat (ATerritoryGuardCharacter::CanAttackTarget):
  IsInfiltratorExposed (latched) && escalation != LocalAlarm
  → DoesPawnAdmitStoryContest(...)   → false = "investigation only"

Attack goal creation (GoalGenerator_Hop_Attack):  ← no admission check at all
```

Two defects fell out of that:

1. **Stale aggression.** `IsAggressiveStealthEvidence(LastEvidence)` has no time bound, so one
   gunshot/bullet impact kept the player an "aggressor" forever. The Place then committed
   **Contested** on the next entry and granted combat permission from a clue that could be minutes
   old — the "AI directly alerts" symptom.
2. **Goal outlived permission.** The guard's combat check refused to attack an exposed but holstered,
   non-war player, yet the vendor attack goal was still created. The attack activity walked the guard
   to the player, could not fire, lost the goal when sight broke, returned to patrol, saw the player
   again and repeated — the reported follow-out-and-return loop.

### Fix

- `UTerritoryStealthProfile::AggressionEvidenceWindow` (default **20 s**, `0` = legacy).
- `UTerritoryBlueprintLibrary::IsRecentAggressiveStealthEvidence(WorldContext, Snapshot, Window)`
  and its deterministic half `...AtTime(Snapshot, NowSeconds, Window)`.
- The story-bounds contest (`ATerritoryVolume::ShouldAdmitStoryBoundsContest`) and the guard combat
  check (`ATerritoryGuardCharacter::CanAttackTarget`) now use the fresh-aggression answer.
- `UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState` gained
  `bAdmittedToCombat`: the attack goal is suppressed whenever the shared contest rule does not admit
  the target, so goal creation can never exceed combat permission. Point-blank no longer bypasses the
  admission rule, matching `CanAttackTarget`.
- Tests: `TerritoryFramework.Stealth.AggressionFreshness` plus new chase-loop cases in
  `AttackGoalGate`; suite is 16/16.

### Resulting behaviour

| Situation | Guard |
|---|---|
| Hidden, below sight threshold | no attack goal, patrol/investigate |
| Exposed, holstered, no war, no recent aggression | no attack goal, investigate + warning line (no chase loop) |
| Exposed + wielding / at war / recent aggression | attack goal, combat permitted |
| Firing while seen, or point-blank with a weapon | attack goal, combat permitted |

### Still needs PIE

Get exposed → holster → leave the district: the guard should stop at the bounds and return to patrol,
not follow out and re-engage. Fire a shot, wait past `Aggression Evidence Window`, holster: the guard
must fall back to investigation instead of attacking.

### G1 — Vendor AI goals ignore stealth (critical)

`Can Create Goal From Perceived Actor?` filters on **attitude, sense and class only**.
Consequence: a crouched player at StealthRating 100 is still handed an `Attack` goal the moment
any sight stimulus arrives. Territory's reduction exists **only inside our observer** — it never
reaches the vendor's goal generator, so "hidden" players get attacked.

Options:

| Option | Mechanism | Verdict |
|---|---|---|
| A. Override `CalcSightStrength` on a player-character child | multiplies sight strength | **insufficient** — goals never read strength |
| B. Territory child of `GoalGenerator_Attack` / `GoalGeneratorBase_Stimulus` | override `Can Create Goal From Perceived Actor?` to consult exposure state + strength, create an **Investigate** goal instead of Attack | **recommended** — vendor pattern, project-side, no vendor edits |
| C. Temporarily neutral attitude while Undetected | uses `AllowedAttitudes` filter | works but global per faction pair; affects diplomacy/combat everywhere |

### G2 — Footstep noise is dropped

Vendor reports `Tag = "Footstep"` with loudness/range already scaled by StealthRating.
Our hearing handler has branches for `DistractionThrowable`, `Gunshot` and empty-tag impacts —
**no branch for `Footstep`** → the vendor's only real use of StealthRating is invisible to us.
Fix: add a footstep evidence type + weight/radius/cooldown, handle the tag in the observer.

### G3 — No guard perception tiers

All guards inherit 6000 cm / 180° / MaxAge 0 / hearing 10000. Rookie/veteran/elite variation
requires editing shared BP controller values. Fix: `UTerritoryGuardPerceptionProfile` applied at
spawn (radius, lose radius, angle, max age, hearing range) — the controller component is the
vendor's, we only configure it.

### G4 — Evidence weights are hard-coded properties

Four floats + a C++ enum (`ETerritoryStealthEvidence`). New evidence (footstep, vehicle engine,
mount gallop, spell, pet, alarm bell) needs C++ edits and enum growth.
Fix: tag-keyed `FTerritoryEvidenceRule` array + `ReportStealthEvidenceByTag`, enum kept for
backward compatibility.

### G5 — No hiding spots or stealth zones (both systems)

Greenfield, but must be a Narrative interaction + stealth-profile hook, never a second subsystem.

### G6 — No alarm escalation beyond investigation

Investigation exists; there is no Calm → Suspicious → Searching → Alarmed → Lockdown data
progression, and evidence is not propagated to guards that did not observe it (only assigned
investigators move).

### G7 — Disguise has no passive suspicion

Identity checks exist (clearance, quality, compromise). Wrong uniform in a restricted zone or
loitering behaviour produces no suspicion growth.

### G8 — No shared noise profile

Distraction has `Loudness`/`MaximumRange`; weapons, footsteps and vehicles each report their own
numbers. One `UTerritoryNoiseProfile` asset would make all noise sources consistent and tunable.

---

## 4. Modular structure for multi-integration loops

Proposed seams (all additive, vendor untouched):

```
                       ┌──────────────────────────────────────────────┐
                       │ UTerritoryStealthProfile (policy, per state)  │
                       └──────────────────────────────────────────────┘
                                          ▲
  Evidence sources (BP interfaces)        │        Consumers
  ┌───────────────────────┐               │        ┌──────────────────────────┐
  │ Sight   (observer)    │               │        │ Guard AI goal gate (G1)  │
  │ Hearing (observer)    │──ReportEvidence(Tag, Strength, Location)──▶ │ Investigation goals      │
  │ Damage  (ASC)         │               │        │ Tagged dialogue barks    │
  │ Script  (Tales)       │               │        │ Tales events/conditions  │
  │ Custom  (any system)  │               │        │ HUD snapshot             │
  └───────────────────────┘               │        │ Counterattack / contest  │
                                          ▼        └──────────────────────────┘
                    UTerritoryControlSubsystem (single authority)
```

| Seam | Asset / API | Why |
|---|---|---|
| Evidence rules | `FTerritoryEvidenceRule` (Tag, Weight, Escalation, AnonymousAllowed, RequiresSight, Radius, Cooldown, InvestigationActivity) | add evidence types without C++ |
| Evidence sources | `ITerritoryStealthEvidenceSource` (BP interface) + per-source `bEnabled` | replace/disable one sense without losing the rest |
| Noise | `UTerritoryNoiseProfile` (Loudness, MaxRange, Tag, attenuation, cooldown) | distraction, weapons, footsteps, vehicles share one truth |
| Guard tiers | `UTerritoryGuardPerceptionProfile` | applies to the vendor `AIPerception` component at spawn |
| Alarm | `FTerritoryAlertState` chain + events | investigation → search → alarm → lockdown, consumed by Tales |
| Identity | `ITerritoryIdentityProvider` | disguise, mount, vehicle, pet override perceived identity |
| Goal gate | Territory `GoalGenerator_Attack` child (G1 option B) | the missing link to vendor AI |

Integration loops this unlocks: stealth↔combat (goal gate), stealth↔dialogue (warning barks),
stealth↔quests (Tales evidence conditions), stealth↔counterattack (exposure → contest),
stealth↔economy (bribes/bounty), stealth↔vehicles/mounts (noise + identity), stealth↔PCG
(hiding spots as authored interaction points).

---

## 5. Recommended order

1. **G1 goal gate** — without it the stealth layer cannot stop AI aggression; everything else is cosmetic.
2. **G2 footstep evidence** — makes the vendor's StealthRating meaningful for hearing.
3. **G4 tag-keyed evidence rules** — the modularity unlock for every later evidence type.
4. **G3 guard perception profiles** — guard quality tiers.
5. **G6 alarm escalation**, then G7/G8 as design needs them.
6. Then asset hygiene (D) and the AGENTS.md test gates (E).

---

## 6. Implementation status

### Done — G2 footstep evidence

- `ETerritoryStealthEvidence::Footstep` (appended, existing values unchanged).
- `UTerritoryStealthProfile`: `bFootstepsRaiseSuspicion` (true), `FootstepSuspicion` (0.06),
  `FootstepEvidenceCooldown` (1.5 s).
- `UTerritoryStealthObserverComponent` hears Narrative's `"Footstep"` tag, scales the weight by the
  stimulus loudness Narrative already reduced for StealthRating, rate-limits per guard and target,
  and reports anonymous evidence (never exposure).
- Tests: `ProfileDefaults` + `HolsteredWeaponContestGate` extended; the new
  `TerritoryFramework.Stealth.AttackGoalGate` pins both the suppress and the allow paths;
  the full `TerritoryFramework.Stealth` suite is 13/13 passing.

### Done — G1 goal gate

- `UTerritoryStealthObserverComponent::ShouldSuppressAttackGoal(Target)` — BlueprintPure, fails open,
  respects point-blank, exposure state, invisibility, fire-while-seen, and the profile's
  `MinimumSightEvidence`.
- `UTerritoryBlueprintLibrary::EvaluateAttackGoalGate(Generator, PerceivedActor, bParentAllows)` —
  BlueprintPure seam that resolves the generator's owning guard pawn and asks its observer.
- Wired in the **project's** attack generator (`/Game/HOPTRENDY/AI/Activities/Attack/Goal/GoalGenerator_Hop_Attack`,
  function `Try Add Attack Goal From Actor`):

```
Function Entry ─▶ Branch( Evaluate Territory Attack Goal Gate(self, Actor) )
                    then  ─▶ Parent: Try Add Attack Goal From Actor ─▶ Return
                    else  ─▶ (ends; returns null goal, Succeeded=false)
```

- The same generator class is referenced by **both** `/Game/TerritoryFramework/AI/AC_TerritoryGuard`
  and `AC_TerritoryAssault`, so guard posts and counterattack assaults share one gate.
- Compile 0 errors / 0 warnings, `validate_blueprint` clean, asset saved.

### Done — G3 guard perception tiers

- New `UTerritoryGuardPerceptionProfile : UPrimaryDataAsset` — sight radius / lose radius /
  peripheral angle / sight memory / auto-success range / hearing range / hearing and damage memory /
  affiliation flags, defaults equal to Narrative's shipped `BP_NarrativeNPCController` values so an
  untouched tier is a no-op.
- `ApplyTo(AIPerceptionComponent*)` writes the tier onto the vendor controller's existing
  `AISenseConfig_Sight/Hearing/Damage` and calls `ConfigureSense` + `RequestStimuliListenerUpdate`.
- `UTerritoryStealthObserverComponent::PerceptionProfile` applies it once per bound perception, so
  reserve spawns, late definitions and controller replacement all receive the tier.
- Registered in the Stealth asset menu with a factory (`UTerritoryGuardPerceptionProfileFactory`);
  `TerritoryFramework.Editor.Authoring` is 9/9.

### Done — G4 data-driven evidence rules

- `FTerritoryEvidenceRule { Tag, Evidence, Suspicion, bForcesExposure }` on
  `UTerritoryStealthProfile::EvidenceRules`.
- `FindEvidenceRule`, `FindEvidenceRuleForTag`, `GetEvidenceSuspicion`, `IsEvidenceForcedExposure` —
  all BlueprintPure, so rules can override any channel's weight and force exposure without C++.
- `UTerritoryControlSubsystem::ReportStealthEvidenceByTag(...)` — server-only tag entry point for
  quests, abilities, vehicles and pets. Native distraction tag maps directly; other tags resolve
  through an authored rule; unknown tags are rejected instead of silently doing nothing.
- The suspicion switch and the forced-exposure test now consult the rules, so both the built-in
  channels and tag-addressed sources share one policy.
- Tests: `TerritoryFramework.Stealth.EvidenceRules`; full stealth suite is 15/15.

### Done — crouch keeps stealth (contest/warning no longer breaks it)

- `GE_CrouchStealth` is an infinite **+50 StealthRating** modifier; the observer already halves
  effective sight for it. The break came from the **contest gate running before stealth**: a holstered
  visitor inside a stealth-enabled Place was warned by the nearest guard and never registered as an
  infiltrator.
- `ATerritoryVolume::ReconcileStoryBoundsContesters` now registers the infiltrator **first**. While
  hidden (or while escalation is `LocalAlarm`) it dispatches no warning, no contest and no Contested
  read model — investigation only. The warning line survives only for **non-stealth** Places.
- `UTerritoryStealthProfile::InvestigationSuspicionThreshold` (0.15) stops a single footstep from
  dispatching guards to the player's exact position; distractions, gunshots, impacts, corpses and
  real sight still pass it.

### Done — G5 hiding spots

- `UTerritoryHidingSpotComponent : USphereComponent` — designer-placed cover (cart, haystack,
  wardrobe). Registers crouched overlapping players as hidden in the Place's stealth authority,
  resolves the Territory from the world location (streaming-safe), server-only, clears on teardown.
- `UTerritoryControlSubsystem::SetTargetHidden` (authority) / `IsTargetHidden` (pure).
- The observer reports **zero effective sight** and **no attack goal** while a hiding spot holds the
  target; noise, damage and disguise evidence still work.

### Done — G6 alert ladder

- `ETerritoryAlertLevel` Calm → Suspicious → Searching → Alarmed → Lockdown in
  `FTerritoryInfiltrationSnapshot::AlertLevel`, computed by
  `UTerritoryControlSubsystem::ComputeAlertLevel` from suspicion + exposure and the profile's three
  thresholds (`SearchingSuspicionThreshold` 0.3 / `AlarmedSuspicionThreshold` 0.6 /
  `LockdownSuspicionThreshold` 0.9).
- `OnTerritoryAlertChanged` delegate for HUD, Tales conditions and alarm readers; the ladder is
  maintained on evidence and on suspicion decay.

### Done — G7 disguise doubt

- `ETerritoryStealthEvidence::Disguise` (non-aggressive) plus
  `bDisguiseRaisesSuspicion` / `DisguiseSuspicionPerSecond` (0.05).
- While a disguised player stays visible and `IsDisguiseAccepted` fails for the Place's owning
  faction, the observer adds slow suspicion — a bad uniform is a slow burn, not a free pass.

### Done — G8 shared noise profile

- `UTerritoryNoiseProfile : UPrimaryDataAsset` (Loudness, MaximumRange, NoiseTag), registered in the
  Stealth menu with a factory.
- `UTerritoryDistractionComponent::NoiseProfile` overrides its own Loudness/Range/tag when set, so
  rocks, bottles, grenades and scripted noises can share one authored recipe.

### PIE verified — HopDistrictTest / Blacksmith (stealth-enabled Place)

Runtime session: PIE In Viewport, player `BP_TerritoryPlayerCharacter_C`, Place
`BP_Property_Blacksmith_C_1` (`DA_Place_Blacksmith`, active profile `DA_Stealth_RescueMission`),
guard `BP_TerritoryGuard_C` (`TerritoryStealthObserver`). Debug: `bEnableDebug` + `bDebugStealth`
on the CDO for the session only (turned back off afterwards).

| Check | Runtime result |
|---|---|
| Crouch applies vendor stealth | `GetStealthRating()` **40 → 90** after `Crouch` (`GE_CrouchStealth` +50) |
| Effective sight formula | `CalculateEffectiveSightStrength(Raw=1.0)` = **0.10** at rating 90 → below `MinimumSightEvidence` 0.2 |
| Attack-goal gate | `ShouldSuppressAttackGoal(player)` = **true** while crouched inside the Place |
| Warning path | **0** new `[ContestGate] … warned` lines in the stealth Place (the only warning in the log predates the fix) |
| Contest path | **0** `[Attacker]` registrations, **0** attack goals, no chase |
| Guard ownership | Guard's `OwningTerritory` = `BP_Property_Blacksmith_C_1`; capture point `PlaceDefinition` = `DA_Place_Blacksmith` |
| Hiding spot (crouched) | `CalculateEffectiveSightStrength(Raw=1.0)` = **0.0** at `BP_TerritoryHidingSpot` (`HidingSpot_Blacksmith`, placed at `(-2350,-600,95)`, radius 160, `bRequiresCrouch`) |
| Hiding spot (standing) | same call, same spot, `UnCrouch` + rating 40 → **0.6** (hidden flag clears, so crouch is genuinely required) |
| Hiding spot repeat | re-crouch → **0.0** again; `ShouldSuppressAttackGoal` = true |

Test artifact: `K2_SetActorLocation` into the Place drops the GASP crouch for a frame (falling
movement mode), so the arrival frame is rated 40 and point-blank/instant sight can latch `Exposed`
before the re-crouch. This is a teleport-only artifact; it is why the log still shows
`strength=1.0`/`Exposed` lines while the steady state is rating 90, sight 0.10, no warning/contest.
Walk in already crouched for the intended path.

### Still open

- **Disguise suspicion** and the **alert ladder** (Calm → Searching → Alarmed → Lockdown) were not
  driven through a clean (unexposed) PIE run; thresholds and evidence wiring are unit-tested.
- **Stale gunfire expiry** after `Aggression Evidence Window` not exercised in PIE (no weapon fire in
  the session).

---

## 8. Vendor crouch / Sneak skill assets (verified)

| Asset | Reality |
|---|---|
`.../GameplayAbilities/Movement/GA_Crouch` | Crouch / un-crouch + toggle setting only. **It does not apply any stealth effect.** |
`.../GameplayEffects/GE_CrouchStealth` | Infinite effect, `ADD_BASE StealthRating = +50`. |
`.../Character/BP/BP_NarrativePlayer_GASP` | Owns `CrouchStealthEffect` (+ class) and applies it on **OnStartCrouch**, removes it on **OnEndCrouch**. This is the only native/vendor consumer of the effect. |
Project chain | `BP_TerritoryPlayerCharacter` → `BP_PCharacter_DemoMap` → **`BP_NarrativePlayer_GASP`** — so the project inherits crouch stealth correctly. |
`.../Data/SkillTrees/Sneak/Skill_Sneak` | One perk: **Feather Quiet**, max 5 levels, "Your footsteps make less sound." |
`.../Sneak/Perks/GE_FeatherQuiet` | Infinite `StealthRating` modifier with a **SetByCaller** magnitude (no fixed value). |
`.../Sneak/Perks/Perk_Sneak_FeatherQuiet` | **No functions and no graph nodes** — `SetPerkLevel` is never overridden and `GE_FeatherQuiet` is never applied anywhere in vendor content. The perk is authored but **non-functional** in 2.4.2. |

Consequences and knobs:

- Crouch = **+50** rating. Territory's effective sight is `raw × (1 − rating/100)`, so crouch alone is a
  **50%** sight cut. Set `Maximum Stealth Rating = 50` on the profile to make crouch fully hidden.
- Vendor foley already scales footstep loudness **and range** by `1 − rating/100`, which is why our
  footstep evidence quietly fades as the rating rises.
- To make the Sneak perk real, a project-side child of `Perk_Sneak_FeatherQuiet` can override
  `SetPerkLevel(NewPerkLevel)` and apply/remove `GE_FeatherQuiet` with
  `SetByCaller.StealthRating = Level × 10` (5 levels → +50, so crouch + full perk = 100 = silent and
  invisible under the default maximum). Nothing in the plugin needs to change; `GetStealthRating()`
  already aggregates it.
