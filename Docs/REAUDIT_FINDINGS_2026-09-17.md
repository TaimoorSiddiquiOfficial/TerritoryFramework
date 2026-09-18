# Territory Framework — Full Re-Audit Findings

**Date:** 2026-09-17
**Scope:** Complete Source C++ re-audit, editor-side (MCP) integration, the four
reported problem areas, and the faction-attitude vision.
**Method:** Source-first audit. Narrative Pro is vendor and read-only. Four
independent audit passes (guard targeting, lock/availability, UI widgets,
faction attitude) plus direct source verification.
**Status:** Waves 1 and 2 applied and building clean on UE 5.8. See Part 7 for
what shipped and what is still open. Everything else here is findings.

---

## Part 1 — What you asked for, extracted

### 1.1 The reported bug (your words)

> "When Faction.Heroes (Player Character) defeat Place Guards it can capture
> place after captured I assign 1 Owning Faction Guard. When counter arrives the
> enemy wait, when Player Character enters District it reacts and moves to chase
> player to attack **ignoring the one Guard assigned by Player Character**."

Three separate behaviours are inside that sentence, and they have three
different causes:

| What you saw | Cause | Verdict |
|---|---|---|
| The counter-attack force **waits** | `Require Player Proximity To Activate` is authored **on** in your profile. It defaults to *off*, which deploys immediately — so this is a setting in your data asset, not missing code. See §2.3 | Working as authored, not a defect |
| It **reacts when you enter the District** | Same proximity gate | Working as authored |
| It **ignores your assigned guard** | A real defect in target scoring | **Bug — confirmed** |

### 1.2 The other requests

2. Full re-audit of C++ and editor-side (MCP) integration.
3. Territory is "limited" — check how the community feature integrates with
   Narrative Pro.
4. Territory widgets look odd; limited access in the Command Center; the
   `TerritoryCaptureHUD` covers the screen with a dark background.
5. Your story vision — Far Cry-style domination, where a mid-game betrayal
   changes faction attitude, and **faction attitude is a main factor in the
   territory system**.
6. Plain-English metadata with one easy example per option, for community
   developers.
7. The lock bool / lock config that "are not working".

### 1.3 Your vision, restated

You want a **domination** game, not a capture-counter:

- You work **for** a Regime and take territory **on its behalf**.
- A story beat accuses you and casts you out — a **betrayal**.
- After that, every system that keyed off "your faction" must re-evaluate.
- Faction attitude is not decoration. It decides who lets you in, who shoots
  you, what you can buy, and what you can hold.

The crucial word is **"for"**. Today the framework has no concept of *who took
a place or on whose behalf* — only *who owns it now*. That single missing fact
is what blocks almost everything else in your vision. See Part 5.

---

## Part 2 — The guard bug (your headline issue)

**Verdict: CONFIRMED DEFECT.** The docs already promise the correct behaviour,
so this is a spec-versus-code break, not a design disagreement.

### 2.1 What is actually correct

- Your assigned guard **is** registered as a defender. The spawn path calls
  `RegisterDefender(Guard)` (`TerritoryVolume.cpp:3051-3052`). This is not a
  "the guard never existed" bug.
- The guard **does** fight back correctly. It has an explicit active-assault
  clause (`TerritoryGuardCharacter.cpp:342-347`).
- The counter-attack **does** plan around defenders at the strategic layer
  (`TerritoryCounterAttackSubsystem.cpp:3838-3839`).

So both sides are individually healthy. The failure is entirely in **how an
attacker chooses between two legal hostile targets.**

### 2.2 The three faults

**Fault 1 — The player is poured into the defenders list.**
`TerritoryAssaultParticipantComponent.cpp:633` feeds `ApplyDefenderPreference` a
list built by `CollectTakeoverCombatants`. That function starts with the real
registered defenders, then **appends you** whenever you are inside the Place
bounds plus 800 cm and belong to the defending faction (`:893-916`). The
takeover path is on by default.

**Fault 2 — That list defines "who is not a defender."**
`TerritoryAssaultTargetPolicy::IsGoalTargetingRegisteredDefender` only asks one
question: *is this goal's target in the list I was handed?* Because you are in
the list, your attack goal is classed as a defender goal and is **exempt from
the very suppression meant to stop you outranking the guard.** Both goals
survive at equal score, and nothing anywhere ranks guard above player. The only
ordering in the whole plugin is combat (3.0) > movement (2.0). There is no
defender tier.

**Fault 3 — Damaging an attacker deletes the guard from the list.**
`CollectTakeoverCombatants:883` replaces the entire list with
`{MostRecentThreat}` — you. The guard is instantly reclassified as a
non-defender and its attack goal is set to **score 0** for a 20-second window
(`DamagingEnemyMemorySeconds`) that **refreshes on every hit you land**.

Practical effect: while you shoot, the attacker is *deterministically* locked
onto you and **cannot** select its own assigned guard. That is exactly the
behaviour you reported.

### 2.3 Why it waits — corrected

> **Correction (2026-09-17).** This section originally said activation "never looks
> at whether the place is garrisoned" and called that a design defect. Re-checking
> the actual setting changed that conclusion.

Activation is gated on player proximity **only when you ask for it**.
`ShouldActivateWaitingAssault` (`TerritoryCounterAttackSubsystem.cpp:2886`) takes
`bRequirePlayerProximity`, and the profile setting behind it —
`bRequirePlayerProximityForActivation` — **defaults to `false`**, with this tooltip
already on it:

> "Disable for autonomous counterattacks. Attackers will deploy after the warning
> and fight Territory guards without waiting for the player."

So the shipped default is already guard-first: with proximity off the force deploys
regardless of garrison *or* player. If your force waits, your
`DA_CounterAttack.uasset` has `Require Player Proximity To Activate` **on** — which
is a supported setting for staging an attack on the player's arrival.

**What was genuinely missing** is the middle case your bug report describes: *wait for
the player, but if the player left a guard behind, fight that guard.* The old model
could only express "never wait" or "always wait for the player" — never "wait for the
player, but a garrison counts."

`TerritoryCounterAttackProfile::bGarrisonTriggersActivation` (default **false**) now
expresses it. It is opt-in on purpose: it only has an effect when you have *explicitly*
asked for player proximity, and silently overriding an explicit author request would
break every staged ambush. Turn it on and a counter-attack that finds a living
registered defender starts on that guard instead of idling.

### 2.4 The docs already describe the intended fix

`Docs/17_Counterattack_System.md:553-556` states:

> "**After all registered defenders are gone**, exact non-defender scores are
> restored so **the player** or other hostile targets become eligible again."

The code does not implement that boundary. You become eligible **while a
registered defender is still alive**. Restoring the documented order is the fix.

### 2.5 Two related hazards

- **The whole suppression system can silently no-op.** It early-returns unless
  an attack-goal class was cached (`:774`), and that class is only learned from
  the goal that is *already* current. If your project's attack goal item keys
  differently, suppression never runs at all. `GetCombatDebugString()` prints
  `AttackClass=` — one line of log confirms or refutes this. **Applied:** the
  early-return now warns once per participant.

- **The strategic and physical layers disagree — WITHDRAWN.** This claim does not
  survive reading the code, and is recorded here so it is not acted on later.
  `BuildObjectiveLocations(Territory, bIncludeRegisteredDefenders)` only ever
  *prepends* live defender positions when the flag is true; the guard posts and
  patrol nodes are appended either way (`TerritoryAssaultTargetPolicy.cpp:111-143`).
  So the `false` call sites do not exclude the garrison — they omit one extra
  candidate source. The line reference `:1407-1413` was also wrong: the
  force-forget is at `TerritoryAssaultParticipantComponent.cpp:1472-1477`, and it is
  a **deliberate, symmetric reset** (both directions, then `RequestStimuliListenerUpdate`)
  at the vehicle-to-foot transition so senses re-acquire cleanly. Nothing there
  hands re-acquisition to a layer where the guard loses. No change made.

### 2.6 Required runtime verification (blocked)

Authored values live inside `Content/TerritoryFramework/DA_CounterAttack.uasset`.
They **cannot be read from disk** — `.uasset` name tables are compressed in this
project, so binary probing returns false negatives. This needs the editor open
and a Monolith MCP query. Fields to read: `bPrioritizeTerritoryTakeover`,
`DamagingEnemyMemorySeconds`, `DefendingPlayerEngagementPadding`,
`ActivationRadius`, `bRequirePlayerProximityForActivation`,
`bNotifyDefendingFactionOnly`.

---

## Part 3 — The lock bool and lock config

**Verdict: the bool is GONE, not broken. Your instruction doc is stale.**

### 3.1 Direct answer

| Your claim | Reality |
|---|---|
| "There is a start lock bool in every district blueprint" | **False today.** It does not exist. Not dead code — **deleted** code. |
| "There is a lock config" | **True, and it is the state config.** They genuinely are the same mechanism. |
| "They are not working" | **Half true.** The surviving field works, but only on a brand-new campaign. |

### 3.2 The bool was deleted

Removed in commit `86e0cf1` ("Complete strict Territory DataAsset migration").
A regression test now guards against its return
(`TerritoryFrameworkTests.cpp:225-228` asserts `bStartsLocked`,
`LockConditions`, and `StateConfigs` are all absent from the actor class).

A repo-wide source search returns **zero declarations**. A binary scan of every
shipped `.uasset` in both plugin and project content returns **zero
occurrences** — and the scan was validated against known-good strings, so the
negative is real.

### 3.3 Where the old "lock config" went

Old `LockConditions` array → **moved** to the `Exit Conditions` of the Locked row
inside `StateConfigs`. That row is now the sole authoring home for lock
conditions and events, and it **is** the unlock gate
(`TerritoryVolume.cpp:2167-2170`).

So your instinct was right: **the state config does the same job the lock config
did.** There is only one mechanism left. That part of your report is confirmed.

### 3.4 Why it *feels* broken

Two real reasons:

1. **`Initial Availability` is a one-shot.** It is read only when a fresh
   campaign initialises (`TerritoryVolume.cpp:183-196`). On **any existing save**,
   the saved value wins and the field is never read for gameplay again. Change it
   in a DataAsset, test on a save, and nothing happens — by design, but
   undocumented where you looked. The tooltip does admit it: *"Saved games keep
   their saved availability."*
2. **The unlock conditions are never auto-evaluated.** `TryUnlockWithContext()`
   is the only unlock path and it has just **two production callers**
   (`TerritoryControlSubsystem.cpp:107`, `TerritoryStoryEvents.cpp:212`). Nothing
   polls the Locked row's exit conditions. A designer who fills in unlock
   conditions and expects the place to open by itself sees nothing happen,
   because something must explicitly call unlock.

### 3.5 The real culprit: stale documentation

`Docs/Blueprint_Setup_Tutorial.md:136-152` — sections **6.1 Start Locked** and
**6.3 Lock Conditions** — still instruct designers to set properties that were
deleted:

> "On the territory volume: **Starts Locked** = true"
> "...add conditions to the **Lock Conditions** array"

This doc **contradicts** `Docs/03_Core_Actors.md:53-56` and
`Docs/14_API_Reference.md:132-133`, which correctly say the properties were
removed. A developer following the tutorial sets nothing, sees no effect, and
concludes the feature is broken. **This is the most likely origin of your
report.**

### 3.6 Duplication and dead branches found

- The legacy rule `InitialState == Locked ? Locked : InitialAvailability` is
  implemented **three times** (`TerritoryDefinition.cpp:141-143`,
  `TerritoryVolume.cpp:1575`, `TerritoryWorldState.cpp:81-82`).
- `ETerritoryState::Locked` is **unreachable at runtime** — rejected at three
  entry points (`CommitOwnershipData`, the control-subsystem validator,
  `SetDerivedControl`). It survives only as a save-migration input.
- The framework **synthesises** a "Locked state" from availability in five
  read paths, so the Locked row behaves like a real state. This is why the
  duplication is invisible to a designer — and why it keeps confusing people.
- `IsLocked()` ORs both fields (`TerritoryVolume.cpp:2152-2158`); the
  `State == Locked` half is a legacy-test crutch.
- **`TerritoryGuardLifecyclePolicy` had two dead branches.** `Retire` and
  `Restore` (`:31-42`) could never fire, because `CommitOwnershipData` rejects
  `NewState == Locked`. Only a unit test reached them. The actual guard despawn
  happens elsewhere, in `ReconcileAvailabilityDependentSystems()`.
  **Verified against a running guard, and dead on every path including save
  load — see Part 20.** Deletion confirmed safe and **applied 2026-09-19**; both
  enum values are gone and the Part 20 probe sequence re-ran identical. See the
  closure note at the end of Part 20.

---

## Part 4 — UI widgets

**Verdict: the HUD complaint is a C++ regression the project already fixed once.**

### 4.1 The capture HUD — direct cause of your complaint

**The card's appearance is hardcoded in C++, not authored in the widget.**
`TerritoryHUDWidget.cpp:25-27` calls `ApplySurface(CaptureSurface,
FLinearColor(0.025, 0.04, 0.055, 0.94), ...)` inside `NativeConstruct`. Alpha
**0.94** is near-opaque.

Your own audit doc (`VISION_AND_COMPLETE_REAUDIT_2026-08-25.md:142-148`) records
this exact problem being found and fixed — *"almost opaque black `#010202F5`"*
changed to *"a lighter translucent teal-gray"*, card reduced to **328 x 108**.

**The C++ was never updated.** It re-imposes the dark surface at runtime over
whatever the artist authors. That is your bug.

**The size is hardcoded too, and re-applied every 0.5 s.**
`TerritoryHUDWidget.cpp:195-203` calls `SetSize(360 x 124 or 162)`. Three
problems:

- It is larger than the documented 328 x 108 contract.
- `SetSize` writes only the bottom-right offsets. If `CaptureSurface` uses
  **stretch anchors** or a non-zero top-left offset, the rendered rect becomes
  *anchor region + 360 x 124*, i.e. the compact card expands past the screen
  edges.

> **Audit correction — the "no DPI scaling" finding is probably wrong.**
> The audit flagged the absence of `GetDPIScale` calls as a defect. In UMG, DPI
> scaling is applied by the viewport root, not per widget, so authored sizes are
> in Slate units and are scaled automatically. Adding `GetDPIScale` math here
> would likely **double-scale** the card. I am not treating this as a defect.

> **30-second check for you:** open `WBP_TerritoryCaptureHUD`, select
> `CaptureSurface`, and confirm the anchors are a **point anchor**, not stretch.

**The exact mechanism (verified after the audit — the audit guessed wrong here).**

`bUseThemeTexture` **defaults to `true`** (`Public/UI/TerritoryUITheme.h:53`) and
the HUD's call does not override it. So the **texture branch runs**, not the
rounded-box fallback. And the config proves a texture is set:

```
Config/DefaultEngine.ini:390
TerritoryPanelTexture=/Game/TerritoryFramework/UI/Textures/T_Territory_InformationPanel
```

That means the card is **not** the dark navy the C++ appears to pass. In the
texture branch (`TerritoryUITheme.cpp:118-122`) the RGB is derived from
**`Outline`**, and the **`Fill` RGB is thrown away** — only its **alpha**
survives. The HUD passes `Outline = (0.18, 0.52, 0.48)` and `Fill.A = 0.94`, so
the tint becomes:

```
R = 0.72 + 0.18 * 0.28 = 0.770
G = 0.72 + 0.52 * 0.28 = 0.866
B = 0.72 + 0.48 * 0.28 = 0.854
A = Fill.A             = 0.940
```

So the card is `T_Territory_InformationPanel` — a texture authored for the large
Command Center panels — multiplied by a bright tint at **94 % alpha**. The
texture is what makes it dark; the 0.94 alpha is what makes it *cover* the world.
**The HUD cannot control its own surface today**, because the only parameter it
could use for that (`Fill`) is discarded.

**Two consequences worth separating:**

1. The near-opaque alpha is the "covering almost all the space" part. This is
   real and is the C++'s fault.
2. The darkness comes from reusing a large-panel texture on a compact card.
   The settings doc claims `TerritoryPanelTexture` covers *"compact HUD cards"*,
   but the texture itself is authored dark — so that reuse is the design error,
   not the alpha alone.

Neither is fixable by editing the widget asset, because `NativeConstruct`
overwrites the brush at runtime on every construct.

**The card's "pressure" element has no data source.** The asset ships static
placeholder text including the literal string
`"Guards 0/0   Income 0   Upkeep 0"`. C++ writes name, owner, state and progress
but **never pressure**. Worse, several named widgets in the asset
(`CaptureOwnerLabel`, `CapturePressureLabel`, `Text_CaptureEyebrow`,
`CaptureStateBadge`, `CaptureStack`, `CaptureHeader`, `CaptureOwnerRow`,
`CaptureAccentSize`, `CaptureHUDRoot`) have **zero references anywhere in
Source**. The asset's EventGraph nodes are all disabled placeholders, so there is
no Blueprint glue either. Any placeholder that is not collapsed ships to the
player as-is — a permanently visible `Guards 0/0  Income 0  Upkeep 0` reads as
broken.

**Two divergent copies of the widget ship.** The runtime copy
(`Content/TerritoryFramework/UI/`, 72 358 bytes) and the plugin copy
(`Plugins/TerritoryFramework/Content/UI/`, 70 496 bytes) have identical widget
names but **different bytes** — they have already drifted.

### 4.2 Command Center — the "odd" full-screen surface — APPLIED

`TerritoryCommandRoot` was styled **white, 84 % opaque, full screen**
(`TerritoryJournalWidget.cpp:586-591`). With `TerritoryScreenBackgroundTexture`
set (`DefaultEngine.ini:391`) that is a screen-sized image tinted white at 84 %
across the entire Command Center — the Command Center mirror of your HUD
complaint, and the direct cause of *"covering almost all the space with dark
background."*

**Correction to my own wording above.** The code comment on that very call site
already said *"Territory only themes its content surface and cards"* — the code
contradicted its own stated intent. Also, `Role == Screen` tints **directly from
`Fill`**, and `Role == Panel` keeps **only `Fill.A`** while deriving visible
colour from `Outline`. So the white-84 % argument is the *tint* over the texture,
not a second surface. This matters because it means exposing the Screen fill to
settings carries none of the "would darken the whole Command Center" risk that
made the Wave 1 theming item deferred.

**Applied.** Three surface tokens added to `TerritoryDeveloperSettings`
(`Territory|UI|Theme`):

| Token | Default | Why |
|---|---|---|
| `bTerritoryCommandScreenUseBackgroundTexture` | **off** | The screen texture is authored as a full menu backdrop, so drawing it here double-paints over the host Narrative menu's own background. Mirrors the existing HUD precedent (`bTerritoryHUDCardUsePanelTexture = false`). |
| `TerritoryCommandScreenFillColor` | **transparent** | A Command Center is a screen *inside* your menu. Painting it opaque hides the world and your menu art for no reason. The panels carry their own fill, so content stays readable. |
| `TerritoryCommandPanelFillColor` / `TerritoryCommandPanelOutlineColor` | previous literals | The panel loop's two hardcoded values, now authorable. |

Both call sites now read from settings with the old literals as fallbacks when
settings are unavailable, so behaviour is unchanged for anyone who wants the old
look — flip the texture flag back on.

**Still true, and deliberately left alone:** every panel and row remains a
hardcoded near-opaque dark literal (alpha `0.92`–`0.98`, ~a dozen sites across
`TerritoryJournalWidget.cpp` and `TerritoryDistrictRowWidget.cpp`). A precise
count of the literals in `TerritoryJournalWidget.cpp` is **40 occurrences of
`FLinearColor(0.`** — but the large majority are **text** colours, not surfaces.
Text colour is authored content, not theming, and rewriting all 40 sites would be
a large refactor with real regression risk for no player-visible gain. I exposed
the *surfaces* (the actual complaint) and left text alone. The remaining row
surfaces (`0.014,0.014,0.014,0.98` at `TerritoryDistrictRowWidget.cpp:308`,
`0.035,0.032,0.01,0.94` at `:419`) are the next candidates if the palette needs
to go further.

The accent red is still copy-pasted three times with two different values — not
touched, because consolidating it is a palette decision, not a bug fix.

### 4.3 "Limited access" — mostly literal

- **There is no capture/claim action anywhere in the UI — investigated, and this
  is intentional. Finding withdrawn.** I filed this as a defect ("it displays
  'Available for capture' but no control captures anything"). Four separate
  plugin docs say the opposite is true by design, and they are emphatic:
  - `01_Quick_Start.md:113` — *"`ForceCaptureWithContext` is for explicit scripted
    transitions and tests, **not physical gameplay**."*
  - `16_District_Management.md:5` — players see capture/counterattack risk
    *"**without giving the widget gameplay authority**."*
  - `18_Operations_UI.md:322` — *"**Never perform a guard, capture, economy,
    diplomacy, or assault mutation directly from widget state.**"*
  - `33_Territory_Asset_Creation_Menu.md:92` — a Capture Point exists *"only when
    this location uses **physical hold-zone capture**."*

  Capture is meant to be **physical**: you walk into the Place or its Capture
  Point and take it, or a quest/dialogue hands it over. On top of that,
  `bForceCapture` is itself documented as a diplomacy-*bypass* switch for scripted
  use. So adding a capture button to the Command Center would not have been a fix
  — it would have **violated a stated architectural invariant and let players take
  territory from a menu.** Withdrawn, and not built.
- The **district management panel does guards only** — waypoint, espionage,
  economy, production, threat, diplomacy, places and upgrades are all
  display-only there, though your docs describe it as covering them.
- **The intelligence filter row was inert — APPLIED, and my original wording was
  imprecise.** I wrote "declared and never constructed." The truth is more
  specific and more interesting: the seven `Btn_Intelligence*` members
  (`TerritoryJournalWidget.h:172-191`) were the **only** buttons in the file
  declared without a binding specifier — the other ~80 use
  `meta=(BindWidgetOptional)`. Everything downstream of them was already fully
  written and correctly guarded: click handlers, button text, tab styling, and
  the filter application table. Because UMG never populated the pointers, every
  `if (Btn_IntelligenceX)` guard was false, so **no filter could ever be
  applied.** Adding the missing `meta=(BindWidgetOptional)` is the whole fix;
  it cannot regress, because a widget that does not author the row still gets
  null pointers and the same guards.
  - *Content follow-up for you:* the buttons must exist in the authored widget
    with those exact names before the row actually appears. The C++ half is done.
- **The entire injected command block can silently vanish.**
  `TerritoryJournalWidget.cpp:1193` does `if (!PlannerHost) return;` with no log
  and no placeholder. The district panel falls back to *any* `UVerticalBox`
  (`:187-200`). In the shipped asset only `ManagementStack` exists by that name —
  so a rename by an artist **deletes every staffing control with zero
  diagnostics.**
- **Streamed-out districts are never actionable — investigated, and my original
  framing was wrong.** I listed
  `TerritoryUIBlueprintLibrary.cpp:1452-1455` hardcoding `bManageable = false` as
  a defect. Reading it in context, it is a **deliberate, well-documented policy**:
  the same block sets localized user-facing reasons —
  *"Travel near this District to load its Places before issuing commands"* — and
  keeps strategic intelligence available while withholding only commands. That
  split is correct, because a management action cannot spawn a Narrative NPC
  without a loaded actor (`Docs/05_Guard_System.md`: *"a missing actor cannot
  spawn a guard"*). **No change made.** Withdrawing the finding rather than
  "fixing" working design.
- `+5 Guards` claims *"atomically"* in its tooltip but routes through the same
  per-target call as ±1, and the cost helper is hardcoded to quantity 1
  (`:789`, `:1057`), so the UI cannot even price a batch.
- `GarrisonTargetSelector` is declared but never constructed — dead population
  code at `:1394-1427`.

### 4.4 Stale data in the Command Center

**Fixed and test-verified 2026-09-17.**

`GetDistrictOperationsRevision` (`TerritoryUIBlueprintLibrary.cpp:1751-1857`) is
the invalidation key, and it **omits fields it displays**: `PlannedAttackers`,
`AssaultResolution`, `ThreatSummary`, all four failure-reason texts,
`GuardQuality`/`Fortification`/`AlliedSupport`/`StrategicValue`, and many
per-row fields. Result: values visibly change without the panel rebuilding.

`:1856` casts a `uint32` hash to `int32`, so a negative revision will break any
`> 0` Blueprint check.

Also: `GuardQuality`, `Fortification`, `AlliedSupport` and `StrategicValue` are
**summed** across child places with `+=` (`:1017-1053`) even though the header
describes `StrategicValue` as *"Relative importance"* — sums, not maxima.
*(Resolved: summing is correct for the first three, which are documented as a multiplier and as
contributions, and wrong only for `StrategicValue`. Fixed and test-verified — see the end of §4.4.)*

#### What shipped

The revision now folds in every field the panel can render — the whole
`FTerritoryDistrictOperationsView` and every struct reachable through it, not a
hand-picked subset. The two production counters (`ProducingSiteCount`,
`BlockedProductionSiteCount`) were found by the new test, not by reading, and are
now hashed too. The return is masked (`Hash & 0x7FFFFFFFu`) so a `Revision > 0`
Blueprint check can never be defeated by a negative value.

The three loaded-actor handles (`District`, `City`, nested `Territory`) stay
excluded, and the reason is written at the exclusion site: they are handles, not
displayed content, and their addresses change on streaming and GC in ways that
would force needless rebuilds. Everything a widget renders about load state is
covered by `bRegistered`, `bRuntimeLoaded` and the stable tags.

**Why the fix is a test, not a patch.** The bug existed *because* the field list
was maintained by hand and drifted. So
`TerritoryFramework.UI.Regression.EveryDisplayedFieldInvalidatesRevision`
(`TerritoryUITests.cpp`) walks the struct by reflection: it pre-populates every
array so nested rows are reachable, takes a baseline revision, then perturbs
**one field at a time** down the whole property chain and asserts the revision
moved. A field added later is covered for free, and a field the test cannot
perturb **fails loudly** telling the author to hash it or document the exclusion
— so unknown coverage is a failure, never a silent pass. It also asserts the
handle exclusion list is exactly the set of handles reflection finds, so a new
handle cannot quietly drop out.

The reflection walk deliberately proves element *values* are hashed, not just
array lengths: probing an array on its own would pass on a hash that folded in
only `Num()`.

#### §4.4's other bullet — resolved 2026-09-17. Summing was right for three of the four fields and wrong for one.

The audit lumped four floats together: *"`GuardQuality`, `Fortification`, `AlliedSupport` and
`StrategicValue` are summed across child places with `+=` even though the header describes
`StrategicValue` as 'Relative importance' — sums, not maxima."* Three of those four are **correct as
they were**, and the audit's grouping hid that. Their own doc comments, five lines apart in
`TerritoryDefinition.h`, are the authority:

| Field | Documented as | Correct aggregate |
|---|---|---|
| `GuardQuality` | *"Relative quality **multiplier**"* | **average** — already a weighted average (`:1054`) |
| `FortificationStrength` | *"Additional authored defence **strength**"* | **sum** — a flat contribution to a defence estimate |
| `NearbyAlliedSupport` | *"Authored **contribution** from nearby allied support"* | **sum** — the same shape |
| `StrategicValue` | *"**Relative importance** of this Territory"* | **maximum** — see below |

A multiplier has to be averaged, because it multiplies per-guard counts; two contributions really do
add up, because both are counted in `DistrictDefencePower = Active*Quality + Reserve*Quality*0.5 +
Fortification + AlliedSupport`. Only `StrategicValue` is *intensive* — a relative, comparable
quantity — and feeding an intensive property into a running total is the actual error. So the
audit's instinct was right and its scope was one field too wide.

**Ownership decision (yours, 2026-09-17): maximum.** A District or defence front is as valuable as
its single most valuable member, not as valuable as all of them added together. Summing meant a
District of six trivial Places outranked one vital Place, and made the number track how the map
happened to be carved into Places rather than how important the place is.

**What shipped.** One rule, in one place — `TerritoryAssaultTargetPolicy::AggregateStrategicValue`
— called by both consumers, so the number a player reads and the number the AI plans with can never
be aggregated two different ways:

- `TerritoryUIBlueprintLibrary.cpp` — the Command Center's District view.
- `TerritoryCounterAttackSubsystem.cpp` — strategic planning.

The planning site also lost its old correction, which is worth recording because it was a real
bug hiding in plain sight:

```cpp
// The default struct value is one; remove it after accumulating authored values so
// one target with StrategicValue=1 remains one instead of being silently doubled.
Input.StrategicValue = FMath::Max(0.f, Input.StrategicValue - 1.f);
```

That comment describes the intent as "one target stays one". The code produced the right answer
**only for a front of exactly one Territory** — for two targets at 1 it gave 2. Subtracting the
struct's default was cancelling the wrong thing, and it is gone now that the value is assigned
rather than accumulated.

**Two corrections to my own earlier reasoning, recorded because I nearly shipped them as findings.**

1. I first believed the planner counted the **District's own** `StrategicValue` while the Command
   Center did not, and had a "display/planner divergence" written up. That is **wrong**.
   `BuildDefenceFront` appends `OwningDistrict->GetProperties()` (`TerritoryAssaultTargetPolicy.cpp:50`)
   — the District's *Places*, not the District — and the `Independent` filter at `:61` drops the
   District itself, because *"a District grants strategic staging rights; it is never a physical
   defender or assault objective."* Both aggregates already used the same set. Reading the function
   rather than assuming cost one command and saved a fabricated defect.
2. What is *true* is that a District's own authored `StrategicValue` was **dead data**: read by
   nothing, because both aggregates walked Places only. `TerritoryDefinition.cpp` applies it
   unconditionally (`:146`) while zeroing its three siblings for a non-physical Territory
   (`:167-171`), so a designer could author a District importance that no code ever consulted. The
   Command Center now seeds its own District into the importance set, which gives that field a
   purpose and a destination — and the reason the asymmetry at `:146` is deliberate (a District is a
   legitimate target; it simply has no guards of its own to fortify) is now written at the site.

**Test:** `TerritoryFramework.CounterAttack.Regression.StrategicValueIsRelativeImportanceNotATally`.
It covers the rule ({1, 5} → 5, not 6; order-independence; empty → 0; a zero Place cannot drag a
valuable neighbour down; a null entry is skipped), the fixture validity of the tags it depends on,
and the one set difference that is easy to get wrong — planning's front excludes the District while
the Command Center's set includes it.

**The test was proved able to fail before it was trusted.** I temporarily reverted the shared rule to
a sum, rebuilt, and ran it: **8 assertions failed**, including the end-to-end one at the planner's
own entry point, which reported `8.000000` where the max gives 7. That run is what converts "both
consumers call the shared function" from an argument into a verified fact — a regression at either
call site now fails this test rather than passing quietly. The rule was then restored and re-run
green.

**Known coverage gap, stated rather than papered over:** the Command Center assertion passes the
importance *set* explicitly, so it proves the rule and the set's contents but not
`BuildDistrictOperationsView`'s own construction of that set (the line that seeds `District` into
it). Covering that needs a full viewer/world fixture, and a test that fails for asset reasons is
worse than an honest gap. It is named here so the next reader knows what is not proved.

**A fixture change this needed:** `Config/Tags/TerritoryExamples.ini` gained one example Place
(`Territory.HavenReach.MarketSquare.Warehouse`), so the sample District has two Places. Without it
no test could build a **multi-Place defence front** at all — the front's sibling aggregation had no
coverage whatsoever — and with one Place in the front a sum and a maximum are indistinguishable, so
the assertion would have passed vacuously.

**Run counts for this change:** targeted test `Result={Success}`, `TEST_EXIT_CODE=0`. Full suite
**348 tests, 346 pass, 2 fail** — one test added, it passes, and the two failures are the same
pre-existing pair, so neither the code change nor the new example tag regressed anything.

### 4.5 Localization gaps (player-visible)

**Bullets 1, 2 and 4 fixed and test-verified 2026-09-17. Bullet 3 withdrawn.
This section is closed.**

- ~~The **most frequently seen** messages are not localized —
  `TerritoryDistrictManagementWidget.cpp:300-344` uses
  `FText::FromString(TEXT("Move closer to the district management point."))`.~~
  **Fixed.** Six `FText::FromString` call sites became three shared
  `NSLOCTEXT` accessors: `GetManagementUnavailableReason`,
  `GetOutOfManagementRangeReason`, `GetNoGarrisonSelectedReason`. Buy and remove
  now call the *same* accessor, so one situation cannot be described two ways.
  The accessors are public `BlueprintPure`, so a project's own HUD can show
  exactly the sentence the widget would have shown instead of hand-copying it.
- ~~Raw `FName` assault-route values and raw territory tags are printed to the
  player (`TerritoryJournalWidget.cpp:2592-2601`, `:2802-2809`).~~
  **Real — fixed, and it was worse than reported.** The line numbers had drifted
  (the live sites were `:2617` and `:2828`). Both blocks printed an author-typed
  `FName` (`"RemovedDeparture"`) and a raw gameplay tag (`"Territory.Heroes.Farm"`)
  straight at the player. Both were also built entirely with `FText::FromString`,
  so the sentences around them — *"No assault routes selected."*, *"Unspecified
  transaction"*, *"No recent transactions."* — were untranslatable, which the
  audit had not noticed. The file already contained the proof that raw tags are
  not player copy: `IsTechnicalPlayerCopy` (`:88`) rejects any string containing
  `"Territory."` in authored description text, and the transaction line was
  producing exactly that.
- ~~The filter option table is `FString` and filtering compares
  `GetTerritoryStatusText(...).ToString()` for equality (`:1694`) — localizing
  the enum display names silently breaks filtering.~~
  **Withdrawn — not supported by the code.** The filter's *options* and its
  *matching* both come from one function (`TerritoryJournalWidget.cpp:1667` builds
  the list, `:1718` tests membership), so a wording change — or a translation —
  moves both sides together and the filter cannot drift. Owner filtering resolves
  the label back to a `FGameplayTag` and compares tags. `DoesDistrictMatchFilter`
  compares enums and booleans, never strings. A grep for any
  `ToString() ==` comparison across `Private/UI/` returns nothing at all.
- ~~`GetTerritoryStatusText` returns **empty** `FText` when `StaticEnum` is null
  (`:2134-2137`), producing a blank status label.~~
  **Real, but not for the reason given — fixed.** `StaticEnum` is never null for a
  loaded reflected `UENUM`, so the null guard was not the hole. The hole was the
  *other* input: `UEnum::GetDisplayNameTextByValue` returns `FText::GetEmpty()` when
  no entry matches the value, so any state byte this build does not recognise
  printed a blank label in all nine call sites.

#### What shipped (bullet 1)

`FText::FromString` builds text with **no namespace and no key**, so the
translation pipeline can never see it — a translated build would show English
here forever. The three messages now carry
`NSLOCTEXT("TerritoryManagement", ...)`, matching the namespace the same file
already used for its buttons and labels.

**How that is verified.** `TerritoryFramework.UI.Regression.ManagementRefusalTextIsLocalizable`
asserts, for each of the three: a namespace of `TerritoryManagement`, its
expected key, a source string that still reads exactly as it always did, and a
key that no sibling message shares. It then runs a **permanent negative
control**: it builds the very same sentence the old way and requires it to be
*rejected*. Without that, a change that made `FTextInspector` report a namespace
for everything would let the whole test pass vacuously.

**Two traps this test surfaced, both worth keeping:**

1. `FText::ShouldGatherForLocalization()` is **not** a key check. It inspects the
   source string only, so it returns `true` for a fresh `FText::FromString` and
   for a properly keyed message alike — it cannot tell the two apart. My first
   draft used it and the negative control failed, which is exactly what the
   negative control is for. The discriminating check is
   `FTextInspector::GetNamespace` / `GetKey`. A note in the test says so, so
   nobody "improves" it by adding the non-discriminating check back.
2. `UTerritoryDistrictManagementWidget` is `UCLASS(Abstract)`, so a test cannot
   `NewObject` it — doing so trips an ensure about instantiating abstract classes.
   Reaching the two guard actions therefore needs the `WBP_` subclass, which
   would make a text test fail for widget-asset reasons. The test asserts the
   messages directly instead, and says in a comment that the buy/remove *sharing*
   is enforced by the code shape (one accessor, two call sites) rather than by
   this test — stated plainly rather than implied.

#### What shipped (bullet 4)

Reading the engine settled what the audit had guessed wrong, and found a second
defect the audit had not mentioned at all:

- `UEnum::GetDisplayNameTextByIndex` (`Enum.cpp:871`) only produces *keyed* text
  inside `#if WITH_EDITOR` (line 880). In a packaged build it falls through to
  `FText::FromString` at line 907 — no namespace, no key, **untranslatable**. So
  reading a player-facing label out of an enum's `DisplayName` metadata works in
  the editor and quietly stops working in a shipped game. Every other helper in
  the same file already spelled its labels out with `NSLOCTEXT`; these two were
  the exceptions.
- For a value with no matching entry, `GetDisplayNameTextByValue` chains
  `GetIndexByValue` → `INDEX_NONE` → `GetNameStringByIndex(-1)` → `return FString();`
  (line 787) → `GetDisplayNameTextByIndex` → `if (RawName.IsEmpty()) return FText::GetEmpty();`
  (lines 875-878). **The blank-label claim was true.** It is also a plausible
  input, not a theoretical one: `ETerritoryState` already carries a legacy
  serialized value for compatibility, so a save written by another build can
  carry a byte this build does not know.

`GetTerritoryStatusText` and `GetDiplomacyStateText` are now explicit `switch`es
with `NSLOCTEXT` per value and a keyed `default:` — the same shape as
`GetAssaultStateText`, `GetAssaultResolutionText`, `GetDiplomacyEventTypeText`,
`GetThreatLevelText` and `GetProductionStatusText`, which already documented
exactly this contract ("never returns the C++ enum identifier", "Localizable
player-facing outcome"). An unrecognised value now reads `"Unknown state"` /
`"Unknown relationship"` instead of nothing. Wording is unchanged for every known
value **except** the legacy state, which the metadata labels `"Locked (Legacy)"`;
the parenthetical is a note to developers about the serialized compatibility
value, so players now see `"Locked"`.

`TerritoryJournalWidget`'s private `GetStateOption(ETerritoryState)` had its own
copy of the reflection; it now delegates, so the option a player picks and the
value compared against it are the same string by construction.

**Still reflection, deliberately.** Everything else in the codebase that calls
`GetDisplayNameTextByValue` is authoring-time text — story-condition summaries,
debug strings, analyzers — where following the designer's rename is the point.
Only the two player-facing helpers were changed. A grep confirms the split.

**How that is verified.**
`TerritoryFramework.UI.Regression.StatusTextIsAlwaysReadable` sweeps every known
value of both enums plus out-of-range bytes (200, 255), asserting each label is
non-empty, carries a key, and carries **this plugin's** `TerritoryOperations`
namespace.

The namespace assertion is the one that matters, and it is not a matter of
opinion: the engine's own path returns the text in the `UObjectDisplayNames`
namespace, and two **permanent controls** in the test run that path directly to
prove the assertion discriminates. Both were run, not assumed:

1. the engine spells a known state identically (`"Contested"`) but files it under
   a namespace that is not `TerritoryOperations` — so a source-string check alone
   would have passed on the old code, and only the namespace catches it;
2. the engine returns **genuinely blank** text for `200` — so the blank-label
   finding, and the fix for it, are both confirmed by running code.

A third control keeps the key/namespace assertions from going vacuous, using the
old spelling as the counter-example.

**Honest limit.** A test running in an editor build cannot observe the
`WITH_EDITOR` difference itself — both spellings return keyed text here. It can
observe *which* namespace the text carries, which is what discriminates the two
implementations. The packaged-build claim above rests on the engine source cited,
not on this test. The test says so in its own comment.

#### What shipped (bullet 2)

The two rows now go through three public `BlueprintPure` statics on the journal
widget rather than being formatted inline inside a refresh routine:
`GetAssaultApproachListText`, `GetTransactionLineText`, `GetTransactionAuditText`.
That is the same shape §4.5a used for the guard-refusal messages, and for the
same two reasons — a project's own HUD can reuse the exact string instead of
copying it, and the string becomes testable without constructing a widget tree.

- **Routes** are split with `FName::NameToDisplayString`, the plugin's own
  established treatment for an identifier that has to be read (the file already
  used it for class names at `:85`). `"RemovedDeparture"` now reads
  `"Removed Departure"`. The separator and the empty sentence carry keys.
- **Transactions** resolve the source tag through
  `UTerritoryBlueprintLibrary::GetFriendlyTagDisplayName`, the same Narrative Pro
  tag names every other Territory screen uses, so a player never reads a tag. The
  amount keeps its explicit sign through `FNumberFormattingOptions::SetAlwaysSign`,
  and an empty reason reads *"Unspecified transaction"*.
- Note the honest limit, stated in the header: the route name and the friendly tag
  name are **derived from an identifier, so they are not themselves
  translatable** — there is no table to translate `"GuardStaffing"` from. What is
  translatable is the sentence around them, which is what now carries keys.

**How that is verified.**
`TerritoryFramework.UI.Regression.JournalRowsShowNamesNotIdentifiers` asserts both
transformations and, crucially, the **absence of the raw token** — that absence is
the fix.

**The test failed on its first run, and the failure was mine.** I had asserted that
the composed lines carry a localization key. They cannot: `FTextInspector::GetKey`
reads `GetTextHistory().GetTextId()`, and text composed by `FText::Join` or
`FText::Format` has no text id of its own — the key belongs to the *pattern* the
formatter owns internally. The assertion fails against a perfectly correct
implementation. It is removed, with the reason written into the test so nobody adds
it back, exactly as §4.5a handled `ShouldGatherForLocalization`. The content and
leak checks carry the weight for composed lines; the directly-returned sentences do
assert their keys.

Two further traps avoided by checking rather than assuming:

1. The leak assertion needs a **registered** tag or it passes for the wrong reason —
   an unregistered tag produces no bracket at all, so "the raw tag is absent" would
   be vacuously true. The fixture is `Territory.Capability.GuardStaffing`, declared
   natively by the plugin so it is registered in any project that loads it, and the
   test **asserts the fixture is valid** so it fails loudly if that ever changes.
2. Two controls prove the leak checks are not vacuous, by confirming each
   transformation actually removes the raw token the old code printed.

### 4.6 Focus and navigation

**One real defect fixed and test-verified 2026-09-17 — and it is not any of the four claims below.
One claim withdrawn, two verified-as-described-but-defensible. This section is closed.**

The four claims, checked one at a time against the live code:

- **Claim 1 — the fallback focuses "the first enabled Narrative button" in tree order
  (`TerritoryActivatableWidget.cpp:42-56`).** **Verified as described, and not a defect on its
  own.** The scan is a *fallback*: it runs only when no `DesiredFocusTargetName` /
  `InitialFocusWidgetName` is configured (lines 50-63 try the configured name first). When it does
  run it returns a real, enabled Narrative button — which is what a fallback is *for*. "Arbitrary
  but valid" is a weaker property than "deterministic", but it never produces an unreachable
  control by itself. What it *did* do, though, was fail to notice that its candidates could be
  invisible — that is bullet 4 below, and it is the one genuine defect in this area.
- **Claim 2 — no navigation rules between `SelectDistrictButton` and the adjacent
  Waypoint/Espionage buttons, which collapse and expand dynamically.** **Largely unsupported.**
  `SetNavigationRule` / `SetNavigationRuleBase` / `SetNavigationRuleCustom` appear **zero times**
  anywhere in `Private/UI/`. This is not an oversight: there is exactly one focusability call in
  the whole UI layer (`SetIsFocusable(true)`, `TerritoryActivatableWidget.cpp:9`), so the plugin
  deliberately hands navigation to Slate's **automatic** navigation. Automatic navigation is
  re-evaluated per directional move and skips collapsed and disabled widgets as it goes, so a
  panel that collapses does not strand focus — it just changes what the next move reaches. The
  audit's premise (that explicit rules are required for correctness here) does not hold. The
  related doc phrase *"zero unreachable controls"* **was** an overclaim, and it is fixed in
  substance by bullet 4 — the code now enforces it rather than asserting it.
- **Claim 3 — the button labelled "Close" does not close the screen
  (`TerritoryJournalWidget.cpp:716-717`, `:3014-3031`).** **Behaviour verified, framing
  withdrawn.** `HandleCloseSelectedTerritoryClicked` (`:3078`) closes the *detail pane*, and the
  header at `:138` documents that pane as *"equivalent to the Quest Journal information pane"* —
  i.e. the pane is the thing being closed, by design, matching the vendor's own Quest Journal
  interaction. The screen itself exits through CommonUI back (`bDeactivateOnBack = true`,
  `TerritoryActivatableWidget.cpp:10`). What remains is a **labelling** nit: a button inside a
  pane that says "Close" while meaning "close this pane" reads ambiguously on a full-screen
  widget. I have **not** changed it — that string is authored on a `WBP_` asset I cannot inspect
  from here, and renaming player-facing text I cannot see the context of is how you break a
  screen you have never looked at.
- **Claim 4 — desired focus order in the district panel ignores the injected "Apply plan" button
  (`TerritoryDistrictManagementWidget.cpp:687-702`).** **Verified as described, but defensible —
  and a duplicate-button bug the audit implied does not exist.** `AddGuardButton` is first
  because adding a guard *is* the panel's primary action; the injected "Apply plan" is the
  commit step for a multi-step flow, so leading with it would put the confirmation before the
  choice. Separately checked, because the claim's phrasing invited the suspicion:
  `BuildGarrisonControls()` is called **once** (`:121`, from `NativeConstruct`), and the 0.5 s
  refresh timer calls `RefreshManagementDisplay` only — it does not rebuild the controls. So the
  injected button cannot accumulate duplicates.

**Bullet 4 — the real defect, and it was not in the audit: `IsVisible()` does not see a collapsed
ancestor.**

The focus guards asked `UWidget::IsVisible()`. That reads only the widget's **own** cached Slate
visibility (`Widget.cpp:395-404`), and collapsing a panel does **not** touch its children —
`SetVisibilityInternal` (`:422-441`) sets the named widget's property and cached `SWidget` and
stops there, with no recursion. So a button inside a collapsed detail pane kept reporting itself
`Visible`, passed the guard, and could be handed focus. The player gets a focus ring on nothing:
the exact failure the docs' *"zero unreachable controls"* line promised could not happen.

Fixed with a new ancestor-walking predicate on `UTerritoryActivatableWidget`:

```cpp
bool UTerritoryActivatableWidget::IsFocusTargetReachable(const UWidget* Widget)
{
	if (!Widget || !Widget->GetIsEnabled())
	{
		return false;
	}

	// Collapsed and Hidden take a widget's descendants off the screen. Every other visibility mode
	// is still drawn. The chain is walked because a child keeps its own "Visible" value while an
	// ancestor is collapsed, so asking the widget about itself alone answers the wrong question.
	for (const UWidget* Current = Widget; Current; Current = Current->GetParent())
	{
		const ESlateVisibility CurrentVisibility = Current->GetVisibility();
		if (CurrentVisibility == ESlateVisibility::Collapsed
			|| CurrentVisibility == ESlateVisibility::Hidden)
		{
			return false;
		}
	}

	return true;
}
```

Both paths in `NativeGetDesiredFocusTarget` now call it — the explicit-target test (line 58) and
the fallback scan (line 73). `HitTestInvisible` and `SelfHitTestInvisible` are treated as
**reachable** on purpose: they suppress mouse hit-testing but the widget is still drawn, and a
gamepad focus path never hit-tests.

**What shipped:** the static above, plus doc comments on both the header and the implementation
explaining the ancestor walk with a plain example. Exposed as `BlueprintPure` so a project's own
derived widget can ask the same question rather than re-deriving the walk.

**Test:** `TerritoryFramework.UI.Regression.FocusNeverTargetsAHiddenWidget`. It builds a headless
tree with `NewObject<UOverlay>` + `AddChild` (no `WBP_` asset involved, so it cannot fail for
asset reasons), then asserts reachable-when-visible, unreachable-when-the-pane-is-`Collapsed`,
unreachable-when-`Hidden`, the pane itself unreachable, reachable again after restoring, reachable
under `HitTestInvisible`, unreachable when disabled, and unreachable when null.

The load-bearing assertion is the **control**:

```cpp
TestEqual(TEXT("Control: the child still reports its own visibility as Visible"),
	PaneChild->GetVisibility(), ESlateVisibility::Visible);
```

That runs while the pane is collapsed. It pins the exact value the old `IsVisible()` guard read —
`Visible`, for a child nobody can see — so the test demonstrates the bug it guards against instead
of merely asserting the new behaviour. Remove the ancestor walk and this test fails; the control is
what makes that true rather than coincidental.

**Run counts for §4.6:** `TerritoryFramework.UI.Regression.FocusNeverTargetsAHiddenWidget` →
`Result={Success}`, `TEST_EXIT_CODE=0`. Full suite **347 tests, 345 pass, 2 fail** — one test
added, it passes, and the two failures are the same pre-existing pair
(`CounterAttackMapConfiguration`, `ExclusiveTabSelection`), so §4.6 introduced no regression.

---

## Part 5 — Faction attitude and your betrayal vision

**Verdict: the plumbing exists and is well built. It stops short in nine places.**

### 5.1 What already works — do not rebuild it

- Attitude and treaties **sync both ways**.
- Capture permission and dialogue **genuinely honour attitude**.
- `UTerritorySetDiplomacyEvent` is a **real designer-facing betrayal hook** —
  its header names your exact use case.

So the betrayal beat has a usable trigger today. What is missing is the
*consequences*.

### 5.2 Where the ripple stops

| # | Gap | Why it blocks your vision |
|---|---|---|
| F1 | **Guards can never become hostile from attitude.** ~~`GetTeamAttitudeTowards` deliberately downgrades narrative hostility to Neutral (`TerritoryGuardCharacter.cpp:255-278`).~~ **Root cause found and it is worse than first stated:** vendor's `ShouldBeAggressiveTowardsTarget_Implementation` is literally `return Hostiles.Contains(Target);` (`NarrativeNPCCharacter.cpp:502-505`) — a **per-actor** set filled by actual damage, not a faction attitude. No faction-level attitude can reach the guard decision at all. | Turn the Regime hostile and its own guards still shrug at you. The betrayal has no teeth. **Fixed** — see item 12. |
| F2 | **`Claimed` state blocks hostility.** `EvaluateTerritoryTarget` required `Contested` (`:362-364`) unless an active assault existed, so the only faction-level hostility channel (diplomacy War) could never fire in a Claimed Place. | A territory you secured for them stays "safe" even after they hate you. **Fixed** — see item 12. |
| F3 | ~~**Reputation is a dead-end integer** — zero gameplay effect.~~ **Fixed** — reputation can now declare War and Alliance, opt-in and off by default. See item 14. | No way to express "the Regime's opinion of you is now hostile." **Closed.** |
| F4 | ~~**The economy is entirely attitude-blind.** Prices, access, services never consult attitude.~~ **Fixed for guard prices** — item 14. Other services still do not consult attitude. | Betrayed, but you could still buy their guards at the friendly price. **Closed for recruitment; the rest of the economy is still blind.** |
| F5 | **District/garrison access is exact-tag equality** with no notion of who captured the place. | You cannot distinguish "their district" from "the district I won for them." |
| F6 | ~~Clients read a non-replicated `FactionAllianceMap`.~~ **Withdrawn twice; real defect found, fixed and verified (2026-09-19).** The live replication path was confirmed working in PIE; the actual defect was that it never published at session start. See the F6 entry in Part 5. | Original "attitude changes invisible on clients" — **refuted in PIE**. This audit's own revision — "no live path, changes wait for a save" — **also refuted.** Real defect: a standing war the session starts with was absent from the replicated model. Fixed via `PublishDiplomacyReadModel()` at authority `BeginPlay`; control test proven able to fail. |
| F7 | **The treaty projection is lossy.** | Alliance/Trade/NonAggression/Ceasefire collapse ambiguously. |
| F8 | **Sentinel attitude tags are invisible to the bridge.** | Some authoring styles silently do nothing. |
| F9 | **The player faction silently falls back to a project default.** | If the player has no faction tag, the whole system quietly misattributes them. |

**F5 is the one that matters most for your vision.** Today ownership is a bare
`FGameplayTag` with **no record of who captured it or on whose behalf**. That
single missing fact is what makes "the territories you took for us" impossible to
express — which is exactly the question in Part 6. *(F5 is now fixed — see item 11
in Part 7.)*

### F6 — settled at runtime, and **both** the original claim and this audit's revision were wrong

The original audit said: *"clients read a non-replicated `FactionAllianceMap`, so attitude changes
are invisible or wrong on clients."* The first revision of this entry kept half of that and added a
sharper-sounding claim: that no live publication path existed for diplomacy, so changes could not reach
clients until a save cycle. **That revision was also wrong, and the runtime session that was supposed to
settle it settled it — against this audit.**

What stands from the original entry:

- **True:** vendor's `FactionAllianceMap` is `UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly)`
  with no replication specifier (`NarrativeGameState.h:297`). The vendor field really does not replicate.
- **True, and still true:** `UTerritoryDiplomacySubsystem` holds zero references to `WorldState`
  (zero matches in `TerritoryDiplomacySubsystem.cpp`).
- **True:** `AttitudeToDiplomacyState` maps `Hostile → War` and `Friendly → Alliance`.

**The error was the direction I searched.** I looked for the diplomacy subsystem pointing at the
WorldState and, finding nothing, concluded no live link existed. The link runs the other way: the
**WorldState reaches into the subsystem** and binds its delegates. `SetTreaty` was the wrong search
term — diplomacy publication never went through `SetTreaty`.

**The live path exists and is fully wired:**

`ATerritoryWorldState::BeginPlay` (authority) → `SubscribeToLiveUpdates()`
(`TerritoryWorldState.cpp:142`) → binds `OnDiplomacyStateChanged` → `OnDiplomacyChangedLive`
(`:1048-1050`) → writes `ReplicatedTreaties` + `ForceNetUpdate()` (`:1248-1299`). The betrayal entry
point is the vendor delegate: `GS->OnFactionAttitudeChanged` →
`UTerritoryDiplomacySubsystem::OnFactionAttitudeChanged` (`:653`) → broadcast (`:703`) → the live
handler. Bound at `TerritoryDiplomacySubsystem.cpp:33`, re-bound with `AddUniqueDynamic` at `:92`.

**Confirmed at runtime** (PIE on `/Game/HopDistrictTest`, 2026-09-18; the running editor this item was
blocked on):

| moment | GameState attitude | `ReplicatedTreaties` |
|---|---|---|
| fresh session, no writes | `Bandits→Heroes = Hostile`, `Heroes→Bandits = Hostile` | **empty** |
| after forcing Hostile→Friendly | `Friendly` | **1 row, `State=Alliance`** |

The second row is the refutation: a mid-session attitude change reached `ReplicatedTreaties` with **no
save cycle**. `AttitudeToDiplomacyState` predicts `Friendly → Alliance` — exactly the row observed. *(Note
this function now lives at `:796-804`; the `:705-713` cited in the withdrawn revision had drifted.)* This
also proves the delegate **binding** is live, which `TerritoryDiplomacyReplicationTests.cpp` structurally
cannot show — see below.

**The real defect is one layer up: there is no startup publication.**

`ReplicatedTreaties` has exactly four production write sites: the save boundary
(`ExportPersistentState`, `:814`), the load path (`SyncDiplomacySubsystemFromReplicatedState`, `:992`),
live changes (`OnDiplomacyChangedLive`, `:1299`), and the manual API (`SetTreaty`, `:386`). **None of
them runs at world begin.**

Meanwhile `LoadFromGameState()` (`TerritoryDiplomacySubsystem.cpp:551-617`) populates `ActiveTreaties`
from the GameState's authored attitudes and **never broadcasts** — it writes `ActiveTreaties` directly
(`:602`, `:614`). The chain is verified end to end: `ReadFactionAttitudes` iterates
`GameState->FactionAllianceMap` (`TerritoryNarrativeProAdapter.cpp:127`), the live session starts with
`Bandits↔Heroes = Hostile`, and `AttitudeToDiplomacyState` maps that to **War** (`:801`). So the subsystem
holds a War treaty at session start while the replicated model holds nothing.

**Consequence — and this is what matters for the betrayal vision: diplomacy is invisible until it
moves.** The first *change* of a session replicates correctly; the landscape it happens in does not. A
client's only source is `OnRep_DiplomacyState` → `SyncDiplomacySubsystemFromReplicatedState` (`:906`),
and the client's own `OnWorldBeginPlay` early-returns on `NM_Client` (`:88`), so it never reads the
GameState either. A client begins play believing Bandits and Heroes are at peace. Diplomacy is uniquely
exposed here because it is the only replicated model with no periodic publisher — the economy self-heals
on its tick.

**Status: FIXED and verified (2026-09-19).** Two design traps shaped the fix, both verified:

1. **A broadcast-based fix would be silently dropped.** Subsystem `OnWorldBeginPlay` runs *before* actor
   `BeginPlay`: `UWorld::BeginPlay()` calls `WorldSubsystem->OnWorldBeginPlay` at `World.cpp:6056` and
   `GameMode->StartPlay()` (which triggers actor `BeginPlay`) at `:6065`. So `LoadFromGameState` fires
   before the WorldState subscribes. Publishing must happen WorldState-side, where the subsystem state is
   already final by `BeginPlay`.
2. **Do not reuse `ExportPersistentState()` for it.** That function also writes the `Saved*` baselines
   (`:852-863`), which are the *deserialization target* for a load. Publishing at `BeginPlay` on a fresh
   session would clobber the baseline a later `Load_Implementation` reads — save-data loss risk.

**The change applied.** Extracted the diplomacy read-model fill out of `ExportPersistentState` into a new
`ATerritoryWorldState::PublishDiplomacyReadModel()` (treaties + reputation + history, then `ForceNetUpdate`)
and called it from the authority `BeginPlay` branch immediately after `SubscribeToLiveUpdates()`. The save
boundary now calls the same helper, so the two paths cannot drift — which matters, because two paths
disagreeing was the shape of the original complaint. Verified in the same commit series: the helper is
authority-gated and null-safe, so it is a no-op on clients and in worlds with no diplomacy subsystem.

**Evidence chain — red, fix, green, and a mutation control:**

| step | observation |
|---|---|
| control added, before fix | **red** — `A standing treaty reaches the replicated model at world begin` expected 1, got 0; premise ("fixture loaded the standing war") and binding assertions both **passed** |
| fix applied | **green** — both diplomacy replication tests pass |
| `AddDynamic` deleted (mutation) | **red** — `Authoritative WorldState bound the live diplomacy delegate` false, while the *old* test still passed |
| mutation reverted | **green**, and the full suite is 354/354, 0 fail, no crash |

The third row is the one that matters for this audit's recurring finding. **The pre-existing test in that
file cannot fail on a missing binding; the new control can.** That was the coverage hole, and it is now
closed rather than merely noted. The control exercises the real ordering (`InitializeActorsForPlay` →
`BeginPlay`) instead of calling the publish directly, which is what makes it able to see the gap.

**A fixture trap worth recording**, hit while writing that control: spawning the vendor `ANarrativeGameState`
with `World->SpawnActor` **crashes the editor**. The vendor save subsystem binds `OnActorSpawned`
(`NarrativeSaveSubsystem.cpp:652`) and calls `INarrativeStableActor::Execute_GetActorGUID` on anything
implementing the stable-actor interface; `ANarrativeGameState` implements it via
`INarrativeSavableActor` without a native `GetActorGUID`, so the interface default's
`checkf(false, ...)` (`NarrativeStableActor.cpp:8`) fires and takes the process down. `NewObject` bypasses
spawn handlers and is sufficient here — the actor only needs to exist as the world's GameState for
`LoadFromGameState` to read it. This is vendor code and out of scope to fix, but anyone writing a test that
spawns a vendor save-aware actor with `SpawnActor` will meet it.

**One limitation, stated plainly:** no real `NM_Client` could be spawned in this environment — the PIE
tooling available exposes no player-count parameter. The authority-side behaviour is now directly verified
end to end, before and after the fix. The client-side consequence is still derived from the source paths at
`:88` and `:906`, not directly observed.

---

## Part 6 — Decisions I need from you

### 6.1 The betrayal question (blocking)

After the Regime turns on you, what happens to the territory **you captured for
them**?

| Option | Meaning | Data cost |
|---|---|---|
| **A — Stay theirs** | Realistic. You lose everything you built. Punishing, simple. | Cheapest — no model change beyond a flag |
| **B — Flip to you** | Rewarding. The betrayal becomes your power base. | Needs `CapturedBy` + `CapturedFor` recorded |
| **C — Go Contested** | Neutral. Anyone can take them. Most dynamic, most systems-touching. | Needs the same record, plus contest machinery per place |

My recommendation is **B**, recorded as data rather than hardcoded: store
`CapturedBy` and `CapturedFor` on every place. Then A, B and C all become
**authorable story choices** rather than engine behaviour — you pick per quest.
That is the Far Cry shape, and it costs one struct field either way.

### 6.2 Which workstream first

The findings are independent. I can start on any of them.

---

## Part 7 — Prioritised plan

Ordered by *impact per hour of work*, not by severity.

> **Status — 2026-09-17.** Wave 1 items 1, 2, 4, 5 and 6 are **applied**; item 3 is
> deliberately deferred (see the warning below). Wave 2 is **fully applied** — the
> guard-first targeting fix, the garrison activation toggle, and the silent-no-op log.
> Every build so far is clean on UE 5.8 (`TDAEditor`, `Result: Succeeded`, 16 actions,
> artifacts verified newer than the edited sources).
>
> Wave 3 is under way: items 11 (`CapturedBy`/`CapturedFor` provenance) and 12 (attitude-driven
> guard hostility) are **applied**. Items 13–15 (replicated attitude map, reputation/economy
> attitude inputs, Command Center UI) are untouched. Wave 4 still waits on the option set
> stabilising.
>
> **Automation tests have still not been run — that needs the running editor.** The new
> contract assertions are written but unexecuted, and the authored values inside
> `DA_CounterAttack.uasset` remain unread (also needs the editor).

### Wave 1 — Small, high-impact (fixes things you have already seen) — APPLIED

| # | Work | Files | Risk |
|---|---|---|---|
| 1 | **Give the HUD card its own surface.** Stop forcing `TerritoryPanelTexture` onto the compact card, drop the alpha from 0.94 to something translucent, and stop `NativeConstruct` overwriting the authored brush. | `TerritoryHUDWidget.cpp:25-27` | Low — one call |
| 2 | **Make the card size a setting** with the documented 328 x 108 default, instead of hardcoded 360 x 124/162. | `TerritoryHUDWidget.cpp:195-203` | Low |
| 4 | **Fix the stale tutorial** (`Blueprint_Setup_Tutorial.md` §6.1/§6.3) and add the "fresh campaign only" note to `InitialAvailability`. | 2 docs | None |
| 5 | **Add missing-host logging** so the command block cannot vanish silently. | `TerritoryJournalWidget.cpp:1193`, `TerritoryDistrictManagementWidget.cpp:200` | Low |
| 6 | **Delete the two dead guard-lifecycle branches** (`Retire`/`Restore`) or wire them correctly. **Checked against a live guard and confirmed dead on every path — see Part 20. Deletion applied 2026-09-19: both enum values and both branches removed, suite 354/354, and the Part 20 probe sequence re-ran identical.** | `TerritoryGuardLifecyclePolicy.h` | Low |

> **Warning on item 3.** Changing the texture branch to tint from `Fill` looks
> like a clean bug fix, but the current formula (`0.72 + Outline * 0.28`) is a
> deliberate **brightening** curve. Every panel caller passes a dark `Fill`
> (~`0.04, 0.06, 0.08`). Tinting from `Fill` directly would multiply every
> panel's texture toward black and **darken the entire Command Center.** If item
> 3 is wanted, it should be done as a palette migration (Wave 3, item 15), not
> as a one-line change. Item 1 alone fixes the reported complaint.

### Wave 2 — The reported bug (behavioural) — APPLIED (guard-first + toggle)

**What shipped.** The component now keeps two lists instead of one. The *engagement* list
still includes a defending player and the most recent attacker, so participants walk toward
the real fight. The *defender priority* list contains **only live hostile registered
defenders**, and it is the only list handed to `ApplyDefenderPreference`. This closes all
three faults at once: the player can no longer be classed as a defender, can no longer
outrank a guard on equal score, and can no longer erase the guard by damaging the attacker
(`MostRecentThreat` now only affects movement, not scoring).

`TerritoryCounterAttackProfile::bDamageRetaliationOverridesDefenderPriority` (default
**false**) restores the older damage-wins behaviour per profile, with metadata explaining
the trade-off. `GetCombatDebugString()` now prints `Defenders=[]` separately from
`Eligible=[]` plus `Retaliate=`, so this class of bug is diagnosable in one line.

All Wave 2 items are now closed.

| # | Work | State |
|---|---|---|
| 6 | **Restore the documented defender-first order.** Stop feeding the player-polluted list into `ApplyDefenderPreference`; add a real defender tier so a live registered defender always outranks a defending player. | **Applied** |
| 7 | **Stop `MostRecentThreat` from erasing the defender list.** Keep defenders; append the threat at the lower tier. | **Applied** |
| 8 | **Make activation garrison-aware** so a counter-attack does not ignore the guard standing in front of it. | **Applied** — opt-in `bGarrisonTriggersActivation`; §2.3 revised |
| 9 | **Add a log when the attack-goal class is never learned**, so the silent no-op is visible. | **Applied** |
| 10 | **Reconcile the strategic and physical layers** — they currently disagree about defenders. | **Withdrawn** — see §2.5; the premise was wrong |

### Wave 3 — The vision (after your Part 6 decision)

| # | Work | State |
|---|---|---|
| 11 | Record `CapturedBy` / `CapturedFor` on every place — the missing fact from F5 | **Applied** |
| 12 | F1 + F2: let attitude produce real hostility, and stop `Claimed` from blocking it | **Applied** |
| 13 | F6: replicate the attitude map, or replicate a client-safe projection | **Done — reframed, fixed, verified 2026-09-19.** The live path worked all along; the defect was the missing *startup* publication. Added `ATerritoryWorldState::PublishDiplomacyReadModel()`, called from authority `BeginPlay` and shared with the save boundary. New control `Diplomacy.Replication.StandingTreatyPublishesAtWorldBeginPlay` went red before the fix, green after, and was proven able to fail by deleting the `AddDynamic` it guards — which the pre-existing test in that file cannot do. Full suite 354/354. |
| 14 | F3/F4: give reputation and economy an attitude input | **Applied and test-verified** — both halves |
| 15 | Command Center UI rework: theming/palette, the missing capture action, the dead filter buttons, the streamed-out district limit, and stale panel data | **Applied where real** — theming tokens + filter-button fix shipped; the stale revision key (§4.4) is fixed and test-verified. The "no capture action" and "streamed-out district" findings were **withdrawn**: both are documented design, not defects. §4.4 is closed (the one genuinely wrong aggregate out of four fixed, the other three defended); §4.5 is closed (two bullets fixed, one withdrawn); §4.6 is closed (one real defect fixed, one claim withdrawn, two defended). |

**Item 11 — what shipped.** Two saved, replicated fields on `FTerritoryOwnershipData`:

- `CapturedBy` — the faction that physically took the Place.
- `CapturedFor` — the faction it was taken for. Equal to `CapturedBy` on a solo
  capture; different when you capture on someone else's behalf.

Both are derived in `CommitOwnershipData` — the single atomic ownership path — and never
taken from the caller's proposed struct, exactly like `FormerOwningFactions`. Only a real
owner change writes them; a same-owner progress/garrison update preserves the tenure, and
derived City/District aggregate reduction never overwrites it. Losing the Place to nobody
clears both.

That single pair is what makes your betrayal authorable without any new subsystem: a quest
can ask *"which Places did I win for the faction that later accused me?"* by testing
`CapturedFor == <their faction>` against `CapturedBy == <yours>`. All three outcomes —
stay loyal, flip, or go contested — are now per-quest data rather than engine behaviour.

**Old-save caveat, documented in `CONDITIONAL_RETAKE_DIALOGUE_2026-09-07.md`:** a Place
already owned in a pre-existing save loads with both fields empty until its next real owner
change. Provenance conditions must read empty as "predates the feature", not as "nobody
captured it".

**Item 12 — what shipped.** `FTerritoryGuardBehaviorTemplate::bEngageAtWarInClaimedTerritory`
(default **off**) lets a defender attack a faction it is at War with even when the Place is
`Claimed` rather than `Contested`. That is the whole fix for F1+F2, because diplomacy War is
the *only* faction-level hostility channel that reaches the guard decision — see the F1 row
above for why.

Opt-in on purpose: making every `Claimed` Place lethal at War would change stealth and quest
content that deliberately runs inside hostile territory. Author it per Territory Definition so
the betrayal is dangerous where the story needs it.

The full decision order (11 steps, first match wins) and every combat option are now documented
in `Docs/05_Guard_System.md` → "Combat Policy — who a guard will attack".

**Item 14, half one (F4) — what shipped.** The economy was entirely attitude-blind (F4), so a
faction you had betrayed still sold you guards at the friendly price. Four new fields on
`UTerritoryDefinition` close that:

| Field | Default | Meaning |
|---|---|---|
| `bAttitudeAffectsPrices` | **off** | Master switch. Off means nothing below is read, so no existing project is silently repriced. |
| `FriendlyPriceMultiplier` | `0.85` | Applied at Alliance and Trade Agreement. |
| `NeutralPriceMultiplier` | `1.0` | Applied at None, Non-Aggression, and Ceasefire. |
| `WarPriceMultiplier` | `1.5` | Applied at War. |

Two new Blueprint-pure accessors do the work — `GetGuardRecruitmentCostFor(Requester, Count)`
and its `GetGuardPurchaseCostFor` twin. The old parameterless overloads are **unchanged** and
still ignore attitude, so every existing caller and Blueprint node keeps its current behaviour.

The important part is *where* the multiplier is applied. `CanSetDesiredGuardCount` is the single
place a guard purchase is charged, and `TrySetDesiredGuardCount` already reuses it — so the
scaling happens once, at that one site, using the exact buyer. The two Command Center display
sites were then repointed at the same `...For(Viewer, 1)` accessor, which is what stops the panel
showing one number and the purchase charging another. **Shown price and charged price cannot
disagree, because they are the same call.**

**Test-verified, and the test caught a real gap.** `TerritoryFramework.Economy.Regression.
AttitudeChangesGuardPrice` covers: policy off → War does not move the price; policy on → War
costs `base × 1.5`, Alliance costs `base × 0.85`, Neutral costs exactly base; a three-guard War
batch costs more than three base-priced guards; a zero multiplier floors at free rather than
inverting into a refund; and the old parameterless overload still returns base.

Its first run **failed**, and the failure was informative rather than mine-this-time:
`FTerritoryOwnershipData::GuardRecruitmentCost` defaults to **0**, and the shared fixture commits
ownership without setting it — so the test Place recruited for free and every multiplier
assertion would have passed on `0 == 0`. A test that cannot fail is worse than no test; the fix
gives the Place the 50 its definition declares, so the arithmetic has something to scale.

**Item 14, half two (F3) — what shipped.** Reputation has real teeth now, and the way it got
them matters more than the feature.

The first design I tried was to make `GetDiplomacyState` fall back to a reputation-derived
state, so every consumer — guards, prices, UI — would inherit it for free. **That would have
been a bug.** `SetDiplomacyState` ends with `if (GetDiplomacyState(...) == NewState)` to decide
whether to broadcast, so an invented state on the read path would have silently suppressed real
change notifications. The read model that mutation paths compare against must stay exactly the
treaty record.

So reputation works the other way round: it **writes a real treaty** through the same path a
designer or a quest uses. One authority, no read-path divergence, and every existing consumer —
including the guard gate from item 12 and the price switch from item 14 half one — inherits it
with **zero** repointing.

Three new settings, all opt-in:

| Setting | Default | Meaning |
|---|---|---|
| `bReputationDrivesDiplomacy` | **off** | Master switch. Off means reputation stays a number only quests read. |
| `HostileReputationThreshold` | `-50` | At or below this, reputation declares War. |
| `AlliedReputationThreshold` | `50` | At or above this, reputation declares Alliance. |

Ownership is the part worth reading. A new `FTreatyRecord::bReputationDerived` flag records *who*
created a treaty, because the naive version — "reputation sets the state when reputation changes"
— is escalate-only and would let a player earn +100 after a bad start and stay at War forever.
With the flag, reputation owns what it created: it raises, lowers, **and withdraws** its own
treaties, and it never touches one it did not create. An authored peace or alliance outranks the
number permanently, even after reputation moves again. The flag defaults to `false`, which is the
safe reading for existing saves — every treaty written before it existed is treated as authored
and stays untouched.

Reputation also needed to know *whose* standing it records. `SetReputationSubjectFaction` pins it;
left empty it resolves from the local player's Narrative faction through the same helper the rest
of the plugin uses. **An unresolved subject does nothing rather than guessing** — asserted
explicitly, because a silent guess here is exactly finding F9.

**Test-verified.** `TerritoryFramework.Diplomacy.Regression.ReputationDeclaresDiplomacy` passes,
covering: off by default and a ruined reputation declares nothing; an unresolved subject is never
guessed at; hostile reputation declares War and stamps provenance; recovering reputation
**withdraws** the war it declared and leaves no treaty behind; allied reputation declares an
Alliance; an authored Ceasefire is not overwritten in either direction; and an authored write
takes ownership away from reputation permanently.

Full suite after both halves: **342 tests, 340 pass, 2 fail** — the same two pre-existing failures,
so neither item 14 change regressed anything.

**Item 15 §4.4 — the stale revision key — is now closed.** The revision hashes every displayed
field, and a reflection-driven test keeps it that way:
`TerritoryFramework.UI.Regression.EveryDisplayedFieldInvalidatesRevision`. Writing that test found
two further omissions reading had missed (`ProducingSiteCount`, `BlockedProductionSiteCount`), which
is exactly the argument for testing this rather than re-reading it.

Full suite after §4.4: **343 tests, 341 pass, 2 fail**.

**Item 15 §4.5 bullet 1 — the unlocalizable refusal messages — is now closed too.** Six
`FText::FromString` literals became three shared, keyed accessors, and
`TerritoryFramework.UI.Regression.ManagementRefusalTextIsLocalizable` proves they carry a namespace
and key, keeps their wording frozen, and includes a permanent negative control so the test cannot
pass vacuously.

Full suite after §4.5a: **344 tests, 342 pass, 2 fail** — still the same two pre-existing failures.

Full suite after §4.5b: **345 tests, 343 pass, 2 fail** — one test added, it passes, and the two
failures are the same pre-existing pair, so §4.5b introduced no regression.

Full suite after §4.5c: **346 tests, 344 pass, 2 fail** — again one test added, again the same two
pre-existing failures. §4.5 is now fully resolved: two bullets fixed, one withdrawn.

Full suite after §4.6: **347 tests, 345 pass, 2 fail** — one test added
(`FocusNeverTargetsAHiddenWidget`), it passes, and the two failures are the same pre-existing pair.
§4.6 is now closed: one real defect fixed (the collapsed-ancestor focus bug, which the audit had
not identified), one claim withdrawn, two verified-as-described-and-defensible.

**Item 15 — what shipped so far.** Three things, in order of how directly they answer your
complaint that the widgets *"look odd"* and have *"limited access"*:


1. **The full-screen Command Center wash is gone by default** (§4.2). The root surface no longer
   paints a screen-sized texture over your menu background, and its flat fallback colour is now
   transparent instead of white-at-84 %. This is the Command Center twin of the HUD card fix.
2. **Panel colours are authorable** — `TerritoryCommandPanelFillColor` and
   `TerritoryCommandPanelOutlineColor`, defaulting to the exact values that were hardcoded, so
   nothing moves until you choose to move it.
3. **The Intelligence filter row was fixed at its root** (§4.3) — seven buttons had no binding
   specifier, so the entire filter was inert despite all its code existing. One specifier per
   button restored it.

**Not done, and honestly still open:** the two long-standing test failures at tasks #19
(`ButtonStyle_TerritoryTab`'s parent path names a master style that does not exist) and #20
(`CounterAttackMapConfiguration` needs ZoneGraph, which this project does not enable), and task #12
— the diplomacy live-replication path, which needs a running two-client session to confirm and which
I deliberately have **not** written a fix for. **§4.4, §4.5 and §4.6 are all closed** as of
2026-09-17, each with a test, and each with at least one audit claim withdrawn or narrowed because
the code did not support it.

**And the biggest "finding" in this item was wrong.** I had filed *"there is no capture action
anywhere in the UI"* as the largest piece of your "limited access" complaint. It is not a defect —
it is a documented invariant, stated four times, that the Command Center must never mutate
capture, because capture is **physical** (walk in, or take a Capture Point, or receive it from a
quest). Building that button would have broken the plugin's core contract. **Withdrawn and not
built** — see §4.3. That is three findings in this one item that did not survive contact with the
code: the streamed-out district limit, the "limited access" capture action, and (earlier) the
strategic/physical layer split. The theming and filter-button fixes are real; the rest of the
"limited access" list was describing deliberate design.

**Verification — now genuinely run.** The automation suite was executed headlessly (see Part 9).
`TerritoryFramework.Guards.Regression.QuestIndependentDefenceAndPerPlayerStealth` — the test
covering the `bEngageAtWarInClaimedTerritory` policy from item 12 — **initially failed**, and the
failure was in my test, not the code. It took two corrections to get right:

1. It hid the player with `ClearInfiltratorExposure` before asserting the policy would engage.
   That is wrong: a *hidden* player is refused earlier in `EvaluateTerritoryTarget`, so the guard
   was declining for a better reason — the stealth contract — and the policy was never consulted.
2. Switching to reporting **Damage** evidence made the player visible but escalated the Place to
   `Contested`, and `Contested` is itself an allow, so the policy was still never consulted.

The working version disables stealth infiltration for the Place to isolate the faction-War path.
With that, policy off → refuses, policy on → engages, policy off again → refuses. That is a real
proof the policy works, and it is the reason to distrust the earlier "compile-only" claim: the
first two versions compiled perfectly and were both wrong.

### Wave 4 — Documentation (your request 6)

A plain-English metadata pass: **one easy example per option**, written for
community developers who use Narrative Pro. To be authored *after* the option
set stabilises, so it documents what ships rather than what was proposed.

---

## Part 9 — The headless test run (new, 2026-09-17)

Earlier in this audit I claimed the automation tests "need the editor" and left every change
verified by compilation only. **That was wrong, and it mattered** — a compile proves the code
parses, not that it behaves. Unreal's automation tests run **headless**, no editor GUI:

```
UnrealEditor-Cmd.exe "<project>.uproject" ^
  -ExecCmds="Automation RunTests TerritoryFramework; Quit" ^
  -unattended -nopause -nosplash -nullrhi -log -stdout -FullStdOutLogOutput
```

**Result today: 350 tests, 350 pass, 0 fail — the suite is fully green.** The suite has moved six
times, and each move is accounted for rather than netted out:

| When | Total | Pass | Fail | What changed |
|---|---|---|---|---|
| First headless run | 337 | 334 | 3 | the state this audit inherited |
| After items 11–15b | 344 | 342 | 2 | four tests added (item 14 pair, §4.4 revision coverage, §4.5 localization) |
| After the style-asset fix (defect 1) | 349 | 348 | 1 | later item-15/§4.6 tests added; **defect 1 fixed**, which also cleared `ExclusiveTabSelection` |
| After the dependency guard | 350 | 349 | 1 | the guard added; **it is now the one failure, deliberately** (defect 1b is real and unrepointed) |
| After defect 1b was repointed | 350 | 350 | 0 | the 7 assets / 10 references were repaired; **the guard went green** — and 1d records what that green does and does not prove |
| After the tab-style repoint broke hover again | 350 | 349 | 1 | **`ExclusiveTabSelection` re-broken by a working-tree config edit**, then re-fixed — see defect 1e |
| **Final** | **350** | **350** | **0** | guard green at 50 assets / 0 unresolvable references |

**Which test is failing has changed, so the count alone is not the story** — three separate turns
here carry the same numbers with different contents, and the last two differ by only one test:

- `Integration.CounterAttackMapConfiguration` **failed earlier today and passes now**, with no change
  I can attribute it to. That is now a test-stability finding rather than a failure: see defect 2.
- `UI.Regression.ShippedUIDependenciesResolve` — the guard added in this step — **was** the remaining
  failure while defect 1b was unrepaired, and that was the correct state: the references were
  genuinely broken and the failure was the test doing its job. It is **now green**, because defect 1b
  was fixed. A guard that stays red forever stops being read; this one earned its red and then
  cleared it.
- `UI.ExclusiveTabSelection` **passed, then failed again, then passed** — the re-break was an
  external config edit, not a regression in any fix. See defect 1e.

### The failure that was mine

`TerritoryFramework.Guards.Regression.QuestIndependentDefenceAndPerPlayerStealth` failed on the
assertion I had added for `bEngageAtWarInClaimedTerritory`. Two successive test versions were
wrong before the third worked — see the item 15 note above. The **code was right the whole time**;
my test was wrong twice, and both wrong versions compiled cleanly. This is the single strongest
argument for the step-then-test discipline: nothing in the build could have revealed it.

### The genuine defects found — all pre-existing, none of them mine

All were confirmed **not** to be headless artifacts: they fail identically without `-nullrhi`, and
each is a real property of the tree as it was handed to me.

| # | Defect | State |
|---|---|---|
| 1 | Territory tab button style cannot load — parent named the impossible `/Game/NP_RPGUITheme/...` | fixed and verified |
| 1b | The same defect family, much wider — 7 assets holding 10 unresolvable references | fixed and verified |
| 1e | `TerritoryTabButtonStyle` repointed to a dialogue style mid-session, killing tab hover | fixed and verified |
| 2 | `CounterAttackMapConfiguration` "non-deterministic on `UZoneGraphSubsystem` registration" | **root-caused and fixed — it was never non-deterministic; see Part 16** |
| 3 | Item 7 — "lock bool in every district blueprint and there is a lock config although they are not working" | **not a code defect.** The state-based config *is* the working mechanism; the legacy bool/enum is the dead half. The real defect was documentation that taught a hidden property and promised an unlock nothing performs. Documentation fixed and build/test-verified; see **Part 17** |

**1. The Territory tab button style cannot load — FIXED AND VERIFIED (2026-09-17).**

```
TerritoryFramework.UI.ExclusiveTabSelection → "Territory tabs have a configured CommonUI style" is null
LogLinker: Failed to load BlueprintGeneratedClass
  /Game/NP_RPGUITheme/Style/MasterStyles/Button/ButtonStyle_NarrativeMaster
  as Parent for .../ButtonStyle_TerritoryTab
```

*Root cause.* `ButtonStyle_TerritoryTab` named a parent class path **that cannot exist** —
`/Game/NP_RPGUITheme/...`. `NP_RPGUITheme` is a **plugin** with `CanContainContent: true`, so its
content mounts at `/NP_RPGUITheme/...` and can never appear under `/Game/`. Confirmed both ways:
`find_asset_data("/NP_RPGUITheme/Style/MasterStyles/Button/ButtonStyle_NarrativeMaster")` resolves,
and `/Game/NP_RPGUITheme` does not exist. Because the parent could not resolve, the asset failed to
load, its generated class never existed, and every Territory navigation tab fell back to the
**default** button style — unstyled, not merely mis-themed. This is the most direct explanation
found for tabs rendering oddly.

*Blast radius was both copies,* which matters because they come from different sources:
`Content/TerritoryFramework/UI/Styles/` (the plugin's own copy, which
`UTerritoryDeveloperSettings`'s constructor defaults point at) and this project's duplicate under
`/Game/TerritoryFramework/UI/Styles/` (which `Config/DefaultEngine.ini:385` overrides to). Fixing
either one alone would have left the other broken.

*The fix, and how I knew it was safe.* Both copies were reparented to the Narrative master style.
The stored parent now reads `/NP_RPGUITheme/.../ButtonStyle_NarrativeMaster`, and I proved that
costs consumers nothing new: **NarrativePro keeps a redirect at its own theme paths.**
`find_asset_data("/NarrativePro/Pro/Core/UI/Style/MasterStyles/Button/ButtonStyle_NarrativeMaster")`
reports `package_name: /NP_RPGUITheme/Style/MasterStyles/Button/ButtonStyle_NarrativeMaster` — the
same redirect holds for `TextStyle_Master_Primary` and `TextStyle_Master_Primary_H2`. So pointing a
Territory asset at NarrativePro's theme path adds no dependency the plugin does not already declare.

*Verification, in the mandated order.* The new test
`TerritoryFramework.UI.Regression.ShippedStyleAssetsAreLoadable` was **written and run RED first** —
it failed on `ButtonStyle_TerritoryTab` only, both in the named check and in the folder sweep. Then
the fix, then the same test **GREEN**: *"Checked 24 shipped UI assets under /TerritoryFramework/UI;
0 unusable."* Then `TerritoryFramework.UI.ExclusiveTabSelection` — one of the two failures this
audit inherited — **passed**, which is a second independent proof the reparent produced a working
parent. Then the full suite: **348 pass / 1 fail.** (That is the count *at this point in the audit*,
not the end state — the remaining failure was the ZoneGraph non-determinism, and later steps take the
suite to 350 / 350 / 0. See the run table above and defects 1b and 1e.)

*One bonus from that second proof.* `ExclusiveTabSelection` asserts the tab's hover and selected
tints and its text styles. While `TabStyle` was null those assertions were **skipped**; now they
execute and pass, so the new parent preserved a valid hover/selected visual contract rather than
merely resolving.

**Method trap worth recording, because it produced a wrong fix first.** The initial attempt used
`unreal.load_class(None, "<full object path>")`, which **resolved by bare name**: asked for
NarrativePro's `ButtonStyle_NarrativeMaster_C`, it returned `/NP_RPGUITheme/.../ButtonStyle_NarrativeMaster_C`
— a *different asset with the same name*. That silently reparented to the wrong master, and
`reparent_blueprint` returns void, so nothing in the call's return value said so. The asset saved,
shrank, and could not be verified. **I reverted both copies to pristine with `git checkout --` and
redid it** using `EditorAssetLibrary.load_asset(path).generated_class()`, which resolves the exact
`UClass` with no name ambiguity. The rule: to get an asset's class, load the asset and take its
generated class — never ask the class loader by name.

**1b. The same defect family is much wider than one asset — seven assets holding ten unresolvable
references across the two scopes. FIXED AND VERIFIED (2026-09-17).**

`WBP_TerritoryButton_Text` was the first instance found, but it is not the only one. An asset-registry
sweep of both UI folders (method in the next paragraph) found **six distinct unresolvable
`/Game/NP_RPGUITheme/…` references spread across four widget assets**, in both the plugin's copy and
the project's:

| Widget | Missing reference | plugin copy | project copy |
|---|---|---|---|
| `Styles/WBP_TerritoryButton_Text` | `Style/MasterStyles/Text/Primary/TextStyle_Master_Primary_H2` | ✔ | ✔ |
| `WBP_TerritoryCaptureHUD` | `TextStyle_Master_Primary_H2` | ✔ | — |
| `WBP_TerritoryCaptureHUD` | `TextStyle_Master_Primary_T1` | ✔ | — |
| `WBP_HopTerritoryJournalWidget` | `Style/RichText/DT_NarrativeDefaultUIRichText` | ✔ | ✔ |
| `W_TerritoryPlayerMenu` | `Widgets/Menus/W_NarrativeMenu_Skills` | ✔ | ✔ |
| `W_TerritoryPlayerMenu` | `Style/MasterStyles/Borders/BorderStyle_MenuBackground_Grid` (plugin) / `…_Paper` (project) | Grid | Paper |

*Four numbers describe that table, and they are all the same finding at different granularity — worth
stating so they do not read as a contradiction:* **6** distinct dead package paths, **4** distinct
widget names, **7** asset files (the plugin and project copies counted separately), and **10**
path×file instances (the ✔ marks). The headline figure this audit tracks is the last two.

Two things in that table are worth more than the list itself.

**Every one of the six resolves at the plugin mount.** `does_asset_exist` and `load_asset` both
succeed for `/NP_RPGUITheme/Style/MasterStyles/Text/Primary/TextStyle_Master_Primary_H2`,
`…_T1`, `/NP_RPGUITheme/Style/RichText/DT_NarrativeDefaultUIRichText`,
`/NP_RPGUITheme/Widgets/Menus/W_NarrativeMenu_Skills`, and both `BorderStyle_MenuBackground_*`. So
the fix is a repoint in each case — nothing is missing from the project.

**Note which target is *not* valid, because I got this wrong first.** These six do **not** exist
under `/NarrativePro/Pro/Core/…`. The redirect proved in defect 1 covers NarrativePro paths shaped
`…/Pro/Core/**UI**/Style/MasterStyles/…`; my first substitution dropped the `/UI/` segment and
produced paths that don't exist. The correct and better fix is to reference the plugin mount
`/NP_RPGUITheme/…` **directly**, which relies on no redirect at all.

**The two copies have drifted apart, which is the duplication cost showing up as data.** The
`BorderStyle_MenuBackground_*` reference is `_Grid` in the plugin's copy and `_Paper` in the
project's, and `WBP_TerritoryCaptureHUD`'s two stale references exist only in the plugin's copy.
Two copies of the same asset have been edited independently and now differ — which is the concrete
argument for the §6 "keep one authored copy" question, not a hypothetical one.

*How these were found (and one way that failed).* The first sweep reported **zero** dangling
dependencies — and that zero was worthless: my registry-presence check (`ar.contains` /
`ar.get_asset_by_package_name`) did not exist, so every dependency came back "untestable" and the
total was an artifact of a broken probe. The corrected run first **proved** its own absence check
against two controls — a package that must be present and one that must be absent — and only then
judged anything. It also showed that a naive version **over-reports**: `/Script/UMG`,
`/Script/CommonUI`, `/Script/TerritoryFramework` and friends are *code modules*, not packages, so
they always look absent and must be filtered out. With that filter, the six above are the complete
set.

The first instance, `WBP_TerritoryButton_Text`, logs on every load:

```
LoadErrors: While trying to load package ... a dependent package
  /Game/NP_RPGUITheme/.../TextStyle_Master_Primary_H2 was not available
```

It is used by `WBP_TerritoryCommandRow`, `WBP_TerritoryDistrictManagement` and
`WBP_HopTerritoryJournalWidget`, so that error appears three more times downstream.

*Impact is bounded, and I am not overstating it.* The widget still loads and its generated class is
valid, and its **own** `Style` property correctly points at `ButtonStyle_TerritoryAction` (which
resolves). So this is a dangling dependency to clean up, not a visibly broken button.

*Why it looked unfixable, and the correction.* My earlier note said the reference "lives on a child
widget inside the widget tree" and therefore "needs the editor". That was **wrong on both counts**,
and it is worth recording how, because it would have led to a bad fix:

- A `dir()`-based scan of all 67 exposed asset names and all 383 CDO names found **no**
  `NP_RPGUITheme` reference. That negative was real *for the asset and its CDO*, but I over-read it
  as "so it must be in the widget tree". It was wrong in a *third* way I had not considered: much of
  it was **orphaned import-table residue**, which lives on the *package*, not on any object, so
  neither an asset scan nor a widget scan can see it.
- The widget tree *is* reachable from Python. The inner object loads at
  `<Pkg>.<ShortName>:WidgetTree` and individual widgets at `<Pkg>.<ShortName>:WidgetTree.<Name>`.
  There is still no enumerator (`root_widget` / `get_all_widgets` / `all_widgets` are all absent),
  so you read widgets **by name** — but "unreachable" was false.

*The fix, and the two-mechanism finding that matters more than the fix.* The references in the table
came in **two kinds that look identical in a dependency listing but need opposite handling**:

> *A caveat on counting, kept deliberately.* The table above is the per-asset register and is the
> authority: seven asset files, ten marked reference instances. The guard's own per-asset counts
> (the `5→4`, `9→7`, `12→11` deltas below) count the *distinct dependency paths it resolved per
> asset*, which is not the same unit as a ✔ in the table — one path can be reached from more than one
> place. I did **not** reconcile the two numberings to the last instance, and I would rather say so
> than print a split that looks precise and isn't. What the two mechanisms below rest on is the
> exactness of the predicted deltas and the hand-read values, not the total.

1. **Orphaned import-table residue** — the property was already `None`, and only the package's import
   table still named the dead package. A plain resave rebuilds the import table from live references
   and **prunes** it, harmlessly. Proven against four pre-committed predictions, each measured
   exactly: the guard's per-asset counts moved `5→4`, `9→7` and `12→11` across the three assets
   resaved for this, and a fourth asset resaved as a **control stayed clean at 0→0**. Predicting the
   *number* before saving, rather than observing it after, is what makes this evidence rather than a
   coincidence.
2. **Live broken property references**, which a resave would have *silently destroyed*. `TabMenus[4]`
   in `W_TerritoryPlayerMenu` was `None` because it had been authored against
   `/Game/NP_RPGUITheme/Widgets/Menus/W_NarrativeMenu_Skills`. **The Skills tab of the Territory
   player menu was dead**, and it had been dead silently: the guard saw a dangling import, but the
   *evidence* — a non-null authored value that failed to load — reads back as `None` too.
   Repointed by hand to `/NP_RPGUITheme/Widgets/Menus/W_NarrativeMenu_Skills` in both copies.

The tell that distinguishes the two: **two copies of the same asset holding two *different* dead
values.** The two `W_TerritoryPlayerMenu` copies had `BorderStyle_MenuBackground_Grid` in the plugin
and `…_Paper` in the project, both now `None`. Two deliberately different authored values that both
died is one shared mechanism, not coincidence.

**The lesson, stated plainly because it nearly cost a feature:** had I resaved
`W_TerritoryPlayerMenu` the way I resaved the other four assets, **the guard would have gone green
while the Skills tab stayed broken permanently, with the evidence destroyed.** A green guard is not
proof the feature works — for this defect family, read the live value *before* pruning. Inspecting
first is the only reason the Skills tab was found at all.

*Verification.* The guard was re-run after every write and ends at:

```
Checked 50 Territory UI assets across 2 scope(s); 0 asset(s) hold 0 unresolvable reference(s).
```

Tracked as tasks #28 and #30.

*Coverage: the gap was real, and the guard closes it.* The `ShippedStyleAssetsAreLoadable` test does
**not** catch any of these six, and its own red run proved it: only `ButtonStyle_TerritoryTab` failed;
`WBP_TerritoryButton_Text` passed. That test checks that an asset **loads and its class compiles**,
and a broken reference sitting inside a widget tree does neither of those things — the asset loads
fine and compiles fine while a tab is silently dead.

The reason the guard *does* catch them is worth stating, because I initially got it backwards: the
asset registry **retains unresolvable content-package imports even when the reference failed to
load**. So a dead reference is still visible from C++ via `K2_GetDependencies` +
`GetAssetsByPackageName`, whether it was authored on the asset itself or on a widget inside its tree.
I had concluded earlier that widget-tree references were unreachable and that only the editor could
find them; that conclusion was wrong, and the widget tree being reachable from Python
(`<Pkg>.<ShortName>:WidgetTree`) is what let me repair the values by hand rather than merely detect
them.

**1c. The dependency guard — added, and the gap closed.**

`TerritoryFramework.UI.Regression.ShippedUIDependenciesResolve` asserts that no Territory UI asset
hard-references a package that does not exist. It walks each asset's dependencies with
`IAssetRegistry::K2_GetDependencies` (`bIncludeHardPackageReferences`) and requires every one to be
present via `GetAssetsByPackageName`.

Three details that make it a guard rather than decoration:

- **`/Script/…` is skipped.** Those are code modules, not packages, so they are never in the asset
  registry and a naive version reports *every* asset as broken. Getting this wrong is what made the
  first probe's "zero dangling" meaningless.
- **It asserts it inspected something** (`CheckedAssets > 0`), so a sweep that silently stops finding
  assets fails instead of passing vacuously.
- **The project's `/Game` copy is checked only if present**, so a consumer project that keeps a
  single authored copy in the plugin does not fail for its absence.

**Written red-first, with a prediction made in advance.** The probe's independent Python enumeration
said seven broken assets and ten references. The C++ test, written afterwards, reported *exactly*
that — same seven assets, same references, no extras, no false positives:

```
Checked 48 Territory UI assets across 2 scope(s); 7 asset(s) hold 10 unresolvable reference(s).
```

Two independent methods agreeing on the same seven is what makes this credible; a test that merely
agrees with the code it was written beside would not be.

**The red was intentional, and it has now been earned and cleared.** This test failed until defect 1b
was repointed, and while it failed that was the correct state — the references were genuinely broken,
so the failure was the test doing its job rather than an obstacle to be silenced. (I had offered to
downgrade it to `AddWarning` for a green suite in the meantime; that is now moot — the fix landed, so
the suite is green *and* truthful.)

**1d. The one thing this guard cannot see — and it matters.**

The guard answers exactly one question: *does any Territory UI asset hard-reference a package that
does not exist?* It cannot tell you **why**, and for this defect family the why decides the fix:

| Cause | Correct fix | Wrong fix |
|---|---|---|
| Orphaned import-table residue (property already `None`) | resave — the import table is rebuilt from live references | repointing nothing; there is no live value to repoint |
| A live value that failed to load (reads back as `None`) | repair the value by hand | **resave** — this prunes it, turns the guard green, and leaves the feature dead with the evidence gone |

Both states present to the guard as "unresolvable reference", and to a naive reader as "just resave
it". Only reading the *live property value first* separates them. That is why the two-mechanism
finding in 1b is worth more than the fix: **the guard can prove the absence of a broken reference,
never the presence of a working one.** A green guard is evidence about references, not about
behaviour — which is precisely the trap that would have hidden the dead Skills tab.

**1e. `ExclusiveTabSelection` broke a second time — from a config edit outside this work.**

After the suite first went green, a routine full-suite run came back with one failure:

```
Result={Fail} Name={ExclusiveTabSelection}
Error: Expected 'An unselected tab has a visible hover treatment' to be false.
       [TerritoryUITests.cpp(502)]
```

It was **not** a regression in the TabMenus repair, and proving that mattered more than assuming it.
The test never reads `TabMenus` or any widget: it reads `Settings->TerritoryTabButtonStyle` and
asserts the configured tab style's normal and hovered brush tints differ. The working tree had
repointed that setting from `ButtonStyle_TerritoryTab` to a newly-created
`ButtonStyle_TerritoryDialogue`, whose `NormalBase` and `NormalHovered` were **both `(1,1,1,A=0)`** —
identical, so the assertion could not pass. That style carries its hover in `NormalHoveredTextStyle`
instead, which is correct for a dialogue button and wrong for a tab.

The attribution evidence, since "not mine" is a claim that needs proof:

- `Config/DefaultEngine.ini` was written at `16:15:41Z`, but the only `.ini` this audit ever edited is
  the plugin's own `Config/Tags/TerritoryExamples.ini` (`13:56Z`). No `Edit`/`Write` targeted
  `DefaultEngine.ini`, and `config_query` / `set_developer_setting` was called **zero** times.
- The decisive corroboration for defect 1's earlier fix: `ButtonStyle_TerritoryTab`'s mtime
  (`14:36Z`) sits exactly inside the `14:30Z → 14:43Z` window in which `ExclusiveTabSelection` flipped
  Fail → Success. It passed *because* the setting pointed at that asset, so the `16:15Z` repoint is
  what regressed it.

The user chose to keep the dialogue style and author its tints rather than restore the tab style.
Consumers were checked **before** writing (`referenced_by_count: 0` — the config setting is its only
consumer, so no dialogue button elsewhere changes), `NormalHovered` was set to `(1,1,1,0.1)` to match
the asset's own 10%-alpha convention, and the test went **red → Success with zero assertion errors**.
The full suite followed: **350 / 350**.

*Two things left deliberately undone, because they are design choices rather than defects:*
`SelectedHovered` remains a 10% **black** wash while `NormalHovered` is now a 10% **white** wash — an
inconsistency inherited from the duplicate — and the style is otherwise a blank duplicate, so tabs
are text-only at rest.

*Method trap found here.* `set_property_at_path` with `dry_run: true` reported `current` as
`(0,0,0,0)` for all four brushes — a default-constructed `FSlateBrush`, i.e. the *property default*,
not the CDO value. Acting on it would have written the wrong thing. The authoritative signals are the
same call's real-write `old_value`, then `get_cdo_properties`, then the test itself;
`get_cdo_properties` is not perfectly stable either (it reported a zeroed brush's RGB as `(0,0,0)` on
one call and `(1,1,1)` on the next), so read the **alpha** and treat the RGB of an alpha-0 brush as
noise.

**2. `TerritoryFramework.Integration.CounterAttackMapConfiguration` — the failure did not reproduce,
and the test is not deterministic. Open, and now recorded as a stability finding.**

This one has been through three statements and the last two were both wrong, so here is the sequence
plainly.

- *First statement:* "the ZoneGraph plugin is not enabled for this project." **Wrong.** `ZoneGraph`
  and `ZoneGraphAnnotations` have been `Enabled: true` in `TDA.uproject` since 2026-08-31
  (`364a140`).
- *Second statement:* "the cause is undiagnosed; most plausibly the test world has no `ZoneGraphData`
  actor." **Also wrong** — it was a guess dressed as a re-diagnosis, and the retraction it belonged to
  was itself unnecessary.
- *What is actually true, traced to the source:* the failing assertion is
  `TerritoryNarrativeProMigrationTests.cpp:568`, which asserts that
  `UTerritoryCounterAttackSubsystem::ValidateNarrativeVehicleRoute` succeeded. The *message* came
  from the product code at `TerritoryCounterAttackSubsystem.cpp:3947`, where
  `World->GetSubsystem<UZoneGraphSubsystem>()` returned null:
  `if (!ZoneGraph) return Fail(TEXT("No ZoneGraph subsystem is available"));`.
  So the ZoneGraph attribution was right in substance — the subsystem genuinely reported itself
  absent. My "correction" of it was the error, prompted by a grep of `TDA.uproject` that answered a
  different question than the one asked.

**And the test now passes.** Both in isolation and in the full suite. I cannot attribute that to
anything, and I checked rather than assumed:

| Candidate | Ruled out because |
|---|---|
| ZoneGraph enablement | enabled since 2026-08-31, no diff in `TDA.uproject` |
| The test's own source | byte-identical since 2026-09-10 (`73fc93d`); file is not dirty |
| The map fixture | `/Game/HopDistrictTest.umap` exists and is clean in git; the test does not take its "fixture missing → skip" early exit |
| My code changes | the build was incremental (12 actions) and `TerritoryCounterAttackSubsystem` was **not** recompiled |
| `-nullrhi` | the earlier failure reproduced with and without it; today's passing runs use it |
| The shared fixture namespace | defined in the same untouched file, not in the dirty ones |
| Order-dependence on my new test | it passes standalone too |

So the honest conclusion is that the outcome depends on **process-level `UZoneGraphSubsystem`
registration** that I have not been able to observe directly — which makes this test
**non-deterministic**, and that matters more than the original red. A test that passes or fails on
module-registration state it does not control will eventually fail in CI for no reason, and the
failure will look exactly like the one reported here.

**Not fixed, and deliberately not guessed at.** Resolving it needs a run that captures whether
`UZoneGraphSubsystem` is registered when the test executes — an editor run with the ZoneGraph module
loaded, and ideally a log of `GetSubsystem` before the assertion. Task #20 records this state. What I
will not do is write a fix for a failure I can no longer reproduce.

## Part 8 — Audit honesty notes

Things I could **not** verify, stated plainly:

1. **The Unreal Editor was not running**, so every Monolith MCP query failed.
   All asset-level claims here come from C++ plus the audit documents. This
   matches `AGENTS.md`, which names source as authoritative — but it means
   **authored values inside DataAssets are unread** (see §2.6).
   - *Partly resolved later the same day:* the editor did run briefly (PID 26796)
     but Monolith still reported unavailable before it closed, so **nothing was
     read through MCP**. The headless test runner in Part 9 does **not** need the
     editor, and it is what produced the first real runtime evidence in this audit.
2. **Binary probing of `.uasset` property values is not viable** in this project
   — the name tables are compressed, so greps return false negatives. I
   discovered this mid-audit after an earlier command reported a misleading
   zero, and I corrected it. The lock audit independently confirmed the same
   limitation and validated its negatives against known-good strings.
3. **This started read-only and no longer is.** That claim was true when written
   and is now false: Waves 1–3 and items 11–13, 15a and 15b are **applied code
   changes**, each rebuilt and DLL-timestamp-verified. No Narrative Pro vendor
   file has been touched, per `AGENTS.md`.
4. **Every failure this audit inherited is now resolved, and the suite is green —
   but green arrived in stages, and one stage was red for the right reason.**
   The tab style defect is fixed and test-verified (defect 1), the wider family of
   seven assets and ten unresolvable references is fixed (defect 1b), and the
   ZoneGraph failure did not reproduce and is tracked as non-determinism rather than
   a red (defect 2). The dependency guard added in 1c **was** the single remaining
   failure for a while, failing on a genuine unrepaired defect — that was the
   intended state, not a regression. It is green now because the defect was fixed,
   not because the guard was weakened. Final: **350 / 350, 0 failures**.
5. **Five things I stated and had to correct, all recorded above rather than quietly
   dropped.** The ZoneGraph diagnosis (wrong, then wrongly "corrected", then traced
   to source); the claim that the dangling-reference family was one asset with an
   unclosable coverage gap (it is ten references across seven assets, and the gap is
   closed); a probe that reported "zero dangling dependencies" when its own presence
   check was broken, producing a zero that meant nothing; the claim that a broken
   reference inside a widget tree is invisible to C++ (it is not — the asset registry
   keeps the failed import); and the conclusion that the widget tree is unreachable
   from Python (it is reachable at `<Pkg>.<ShortName>:WidgetTree`; only an enumerator
   is missing). The last two mattered because, taken together, they were the reason I
   had recorded this defect family as "detectable but unfixable outside the editor".
6. **A defect that appears mid-audit is not necessarily one I caused, and "not mine"
   needs evidence rather than assertion.** `ExclusiveTabSelection` went red a second
   time (defect 1e) from a working-tree repoint of `TerritoryTabButtonStyle` made
   outside any tool call of mine. I did not simply declare that; I showed it — the
   config mtime, the zero `config_query` calls, the only `.ini` this audit ever edited,
   and the mtime coincidence that explains why the test passed the first time. The
   same standard applied to the two orphan assets that appeared between asset
   listings and bumped the guard's coverage from 48 to 50: reported, not absorbed.

---

## Part 10 — The documentation wave (new, 2026-09-17)

Your item 6: *"Meta Description with easiest example in all option required to understand by game
dev, as it is a community plugin for those devs who use Narrative Pro."*

### Why this is generated rather than written

I did not hand-write the option reference, and that is a deliberate choice. There are **787
designer-facing options across 117 classes**. A hand-written list of that size cannot be checked —
you would have no way to know whether an option was missing, renamed, or described from memory. So
the reference is **generated from the C++**, because `AGENTS.md` makes source authoritative:

```
Tools/Extract-Options.py      reads Source/TerritoryFramework  -> Tools/options_inventory.json
Tools/Check-OptionsExtraction.py   proves the extraction is right (7 controls)
Tools/Generate-OptionsDoc.py  reads the inventory              -> Docs/37_Every_Option_Reference.md
Tools/Verify-OptionsDoc.py    proves the doc is complete and invented nothing
```

All four are now in the plugin's tracked `Tools/` folder next to `Validate-Content.py`, so anyone can
regenerate the doc. Running the chain twice produces a **byte-identical** doc and inventory — I
checked, because a generator that is not deterministic cannot be trusted to describe a fixed source.

### What the numbers say about the plugin's own documentation

| | |
|---|---|
| UPROPERTYs in `Source/TerritoryFramework` | 1848 |
| Designer-facing (`EditAnywhere`) | 787 |
| Classes and structs covered | 117 |
| Options where the author wrote a description | 389 |
| **Options with NO description in source** | **398 (51%)** |

That last line is the finding, and it is the real answer to your request. **Half the plugin's options
have no authored description at all.** The generator prints `*no description in source*` for every
one of them rather than inventing a plausible sentence — because an invented description in a
reference doc is worse than a visible gap: a developer would trust it.

This is also why Part B of the documentation (the plain-English layer with a worked example per
option) has to be written from the code that *consumes* each option, not from the option's name.

### Two extractor bugs that only the controls caught

Neither of these was visible by reading the script, and both would have shipped wrong documentation:

1. **`bool bEngageAtWarInClaimedTerritory = false;` produced the property name `false`.** My
   "last identifier on the line" rule picked the initialiser, not the name. Caught because the
   positive control asked for a property I knew by name and got zero hits — the control existed
   precisely because I did not trust the parser.
2. **A property name repeated in two classes resolved both to the earlier class.** I re-found the
   declaration text to get its position, so a second class's same-named property was attributed to
   the first. This showed up as 83 "duplicate" rows. The fix was to carry the real offset out of the
   scanner instead of looking the text up again.

A third failure was structural: the original regex could not match a multi-line `UCLASS(...)` whose
specifier list contains nested parentheses (`ClassGroup=(Territory), meta=(...)`), so **417 of 1832
rows resolved no class at all** — 23% of the document would have been filed under `?`. Replacing the
regex with a balanced-paren scanner also found 16 properties the regex had missed entirely.

### One behavioural gap found while reading for the guide

`ETerritoryStealthEscalationScope` offers three values, and two of them are the same value in code:

| Value | What the enum tooltip promises | What shipped C++ does |
|---|---|---|
| `LocalAlarm` | Guards investigate; territory is not registered as Contested | **Distinct** — the only value the code branches on |
| `TerritoryConflict` | Register the player as a contester; diplomacy decided by State Events | Register the player as a contester |
| `FactionWar` | Register the player as a contester, then let the Contested State Event declare War | Register the player as a contester |

Every comparison in the plugin is `== LocalAlarm` or `!= LocalAlarm`
(`TerritoryGuardCharacter.cpp:357`, `TerritoryVolume.cpp:1346`, `TerritoryControlSubsystem.cpp:1004`,
`:1089`). `TerritoryConflict` and `FactionWar` are never distinguished from each other. Their
promised difference lives in **authored State Event content**, not in C++ — which the enum's own
header comment half-admits ("Faction War preserves the existing contest flow").

This is not a crash and not a bug in the strict sense, but as a *documentation* matter it is a trap:
a designer picking `FactionWar` over `TerritoryConflict` expects the framework to do something
different, and it will not do anything different by itself. The option guide states this plainly
rather than repeating the tooltip as if it were behaviour.

---

## Part 11 — The reported guard bug: the mechanism, read out of the code

Your item 1, verbatim:

> *"When Faction.Heroes (Player Character) defeat Place Guards it can capture place after captured i
> assign 1 Owning Faction Guard when counter arrives the enemy wait when Player Character Enter
> District it React and move to chase player to attack ignoring the one Guard Assign by Player
> Character."*

I traced both halves of this — **why the arriving force waits**, and **why the assigned guard does
not fight it** — and both are decided by options whose defaults are `false`. No code is broken. The
behaviour you are describing is the framework doing what its current configuration says.

### Half one — why the force waits, and then chases the player

`UTerritoryCounterAttackSubsystem` evaluates a waiting force here
(`Private/Subsystems/TerritoryCounterAttackSubsystem.cpp:1746-1760`):

```cpp
const bool bProximityRequired = Profile->bRequirePlayerProximityForActivation
    && !Assault.bImmediateDeployment;
const bool bRelevantPlayerNearby = !bProximityRequired
    || HasRelevantPlayerNearby(Assault, Territory, Profile->ActivationRadius);
// Only consulted when the author actually asked to wait for a player;
// an immediate deployment never waits and so never needs a stand-in trigger.
const bool bGarrisonHoldsDefence = bProximityRequired
    && Profile->bGarrisonTriggersActivation
    && !TerritoryAssaultTargetPolicy::CollectRegisteredDefenders(Territory).IsEmpty();
```

and the decision itself (`:2895-2905`):

```cpp
// A garrison standing in the Place is its own reason to attack. Without this the
// force idles until a player walks in, and then has a player to chase.
const bool bProximityAllowsActivation = !bRequirePlayerProximity
    || bRelevantPlayerNearby || bGarrisonHoldsDefence;
```

That comment is a description of your bug report. Read the two flags as a pair:

| `bRequirePlayerProximityForActivation` | `bGarrisonTriggersActivation` | What the arriving force does |
|---|---|---|
| `false` (default) | ignored | Activates on its own the moment it arrives |
| `true` | `true` | Activates if a player is near **or** a guard holds the Place |
| **`true`** | **`false`** | **Idles until a real player is inside `ActivationRadius`, then attacks — and the only target it has is that player** |

The third row is your report, exactly: *"the enemy wait … when Player Character Enter District it
React and move to chase player"*. The assigned guard does not count as a reason to begin, because
`bGarrisonTriggersActivation` defaults to `false` and is only consulted at all when proximity is
required. So the guard is not "ignored" through a targeting mistake — it is not a trigger, and by
the time the force is awake the player is the relevant actor.

**To confirm in one minute:** open the counter-attack profile asset the Place uses
(`UTerritoryCounterAttackProfile`) and look at `bRequirePlayerProximityForActivation` and
`bGarrisonTriggersActivation`. If the first is ticked and the second is not, that is this bug, and
the fix is to tick `bGarrisonTriggersActivation` (the guard then makes the force attack on arrival,
without you standing there).

### Half two — why the assigned guard does not fight back

Separately, the guard has its own acceptance chain that answers *may I attack this actor*
(`Private/Core/TerritoryGuardCharacter.cpp:286-374`). Three of its exits are relevant, and all three
are reachable with the shipped defaults:

**1. A Claimed Place is not defended against a faction you are merely at war with.** The chain ends:

```cpp
// Faction-level hostility has no other channel here: Narrative's personal aggressiveness
// test is a per-actor Hostiles set, not an attitude, so without this a broken alliance
// leaves every Claimed Place the player captured for them completely safe.
if (Behavior && Behavior->bEngageAtWarInClaimedTerritory)
{
    return Result(true, TEXT("The Place is claimed, but this faction is at War and the guard is ordered to defend it."));
}
return Result(false, TEXT("The Place is not contested and there is no confirmed local threat."));
```

`bEngageAtWarInClaimedTerritory` is `false` by default. A Place the player has just captured is
`Claimed`, not `Contested`, so this branch is the one that decides — and by default it says no. The
author's comment shows the default is a deliberate design position, not an oversight: a fresh capture
is *not* automatically defended against an at-war faction unless you ask for it.

**2. The one branch that would override that only covers the assault's defence front.** Above the
branch in (1) there is a check for an arriving assault character:

```cpp
if (Participant && !Participant->HasRetired() && Attacker->CanEngageAssaultTarget(this)
    && TerritoryAssaultTargetPolicy::BuildDefenceFront(
        Participant->GetTargetTerritory()).Contains(OwningTerritory.Get()))
    return Result(true, TEXT("An active hostile assault threatens this Place's defence front."));
```

and `BuildDefenceFront` (`Private/Combat/TerritoryAssaultTargetPolicy.cpp:32-77`) returns:

- the target **only if it is an `Independent` Place** — a City or District target returns an **empty
  list**;
- plus that Place's **same-owner sibling Places** under the same District.

So this branch cannot fire if the counter-attack is scheduled against a District — and a capital
District is a legitimate assault target (see Part 10's note that `StrategicValue` deliberately
survives on a District). Nor can it fire if your assigned guard stands on a Place that is neither the
attack target nor a same-owner sibling of it. A guard you posted to the *wrong* Place, or posted to a
Place whose owner relationship changed, will simply watch.

**3. A stale `FactionOverride` on the guard post makes the guard the wrong faction.** Both
`FTerritoryGuardPostTemplate.FactionOverride` and `UTerritoryGuardPostDefinition.FactionOverride` pin
a spawned guard's faction instead of letting it follow the current owner. Before any of the above, the
guard refuses any target sharing its own faction and requires a War pair between its faction and the
target's (`:320-340`). A post whose `FactionOverride` still names the faction you *took the Place from*
produces a guard that cannot be at war with the arriving force — and, because the attackers only treat
registered defenders of the target as defenders
(`TerritoryAssaultParticipantComponent.cpp:670-681`), is not a defender they need to engage either.
This is the reading that matches *"ignoring the one Guard Assign by Player Character"* most literally.

### What I did not do, and why

I have not changed any of these defaults. Each is a defensible design position — "a captured Place is
not instantly besieged" and "don't attack an empty Place" are both reasonable, and reversing either
would change every project using the plugin. They are **configuration decisions that belong to you**,
and the guide (file 38) now states each one with the symptom it causes.

### The diagnostic gap this exposed

`EvaluateTerritoryTarget` computes a human-readable refusal reason for every one of its thirteen exits
— "The Place is not contested and there is no confirmed local threat." among them — but **the plugin
never writes that reason to any log**. It is exposed to Blueprints (so a debug widget can display it)
and used by a test, and that is all. So when a guard declines to fight, nothing in the output log says
why, and `bDebugGuardSpawning` / `bDebugGuardDeaths` do not print it either.

That is why this bug had to be traced by reading rather than by observing. Logging the reason at
verbose when `bEnableDebug` is on is a small, plugin-owned change that would turn a future occurrence
of this bug into a one-line answer. Recorded as a task rather than done here, because it is a code
change and belongs in its own step with its own test.


---

## Part 12 — Wave 4 part B: the options guide, and the four bugs its own test found

### What was delivered

`Docs/38_Options_Guide.md` — **1350 lines, 13273 words, 17 class sections, 225 option bullets, 263
rows in the generated defaults appendix**, with a worked example for every option and five concept
sections (**Stealth and disguise**, **Economy**, **`UTerritoryDeveloperSettings`**, **Diplomacy,
attitude and betrayal**, **Saving and persistence**) covering the systems the class sections alone
would have scattered.

It is checked by `Tools/Verify-OptionsGuide.py`, which holds the hand-written prose answerable to the
same C++ inventory that the *generated* file 37 is built from. Eight independent checks, all seen to
go red from a green baseline:

| Check | Question it answers |
|---|---|
| C1 | Does every section heading name a class that really declares designer options? |
| C2 | Does every option bullet resolve inside the class of its own section? |
| C3 | Does the defaults appendix match the C++ initialiser exactly? |
| C4 | Are the four options the reported bug turns on actually explained? |
| C5 | Is every backticked identifier a real name from C++? |
| C6 | Does every option the prose teaches have a row in the appendix? |
| C7 | Does every in-document link point at a heading that exists? |
| C8 | Does any option bullet sit outside a class section (so nothing checked it)? |

`Tools/Generate-OptionsGuideDefaults.py` emits the appendix from the inventory, so the stated defaults
cannot drift from the source: they are printed, not remembered. The **263 rows / 787 designer options**
split is the honest coverage statement — the guide explains the options a developer touches first and
points into file 37 for the rest.

### The four defects the checks found in my own work

Every one of these was found by *running a control*, not by re-reading the text, and each is recorded
because the failure mode is general:

1. **`` after a closing backtick can never match.** The bullet regex ended `...```, but the
   character just consumed is a backtick — a non-word character — so no word boundary can exist there
   and the pattern silently matched **nothing**. The two scripts that used it reported "0 bullets"
   while two other checks passed, which is what made it look like a content problem rather than a
   regex problem.
2. **A bullet naming three options only had its first checked.** `- `A`, `B` and `C` — ...` was
   captured as one name, so two of the three were never validated — which is exactly how a
   wrong-class error half-escaped. Found by adding C6 and watching **19 options** turn up missing from
   the appendix that the prose was teaching.
3. **Hand-typed anchors for em-dash headings all pointed at nothing.** GitHub's slug rule *deletes*
   the em dash, so "`A` — b" slugs to `a--b`, not `a-b`. Nine links looked correct and landed nowhere.
   Found by adding C7; the same run also caught a genuine typo — `ftterritory...` with three t's.
4. **C5 was flagging the appendix's own default cells** (`true`, `false`, `nullptr`) as invented
   identifiers. The fix is not to add them to `EXTRA` — it is to skip table rows, because C3 checks
   those cells against the initialiser, which is a *stronger* claim than "is this a known name". The
   skip is verified not to be a hole: mutating a table default to `NotARealThing` still goes red, via C3.

### Two findings the writing surfaced, not the audit

- **`DefaultTerritoryIncome`, `DefaultGuardCost` and `DefaultMaxConcurrentAttackers` are still in the
  config file, marked `DeprecatedProperty`, and never read by gameplay.** A project that inherits and
  tunes them changes nothing, with no error. They are now documented as what they are.
- **`ProductionCycleObservationIntervalSeconds` does not set the production cycle length** — only how
  often the server checks Narrative's clock. The tooltip says so; the name does not, and it is a
  natural thing to misread.

### The process lesson, recorded because it nearly corrupted the evidence

The eight-mutation battery first reported "RED as expected" for all eight **while the document was
already failing**, because an edit of mine had backticked the placeholder word `Class`. A red baseline
makes every mutation look caught. The battery now asserts the baseline in the same script and aborts
otherwise, and decodes child output with `errors="replace"` — the verifier prints source lines
containing em dashes, which arrive as cp1252 on Windows and killed a strict UTF-8 read mid-run, leaving
the file mutated and the run unfinished. Both are written into the working-pattern memory.

---

## Part 13 — Making the guard's decision observable (new, 2026-09-17)

### The problem this fixes

`ATerritoryGuardCharacter::EvaluateTerritoryTarget` computes a human-readable refusal reason for
every one of its **thirteen** exits — *"The target belongs to the guard's perceived faction."*,
*"A protective treaty blocks combat with this faction."*, *"There is no personal hostility or faction
War."* — and the plugin wrote **none of them to any log**. So the guard bug you reported could only be
diagnosed by reading the source and reasoning about which exit fired. That is how Part 11 of this
report had to be written, and it is not how a shipped plugin should behave. A developer running the
game should be able to *see* why a guard did nothing.

### The change

One `UE_LOG`, in the one place every exit already funnels through — the `Result` lambda at the top of
`EvaluateTerritoryTarget`, not thirteen times over:

```
GuardCharacter TerritoryGuardCharacter_0 refuses BP_TerritoryPlayerCharacter_C_0 at TerritoryProperty_0: There is no personal hostility or faction War.
GuardCharacter TerritoryGuardCharacter_0 engages BP_TerritoryPlayerCharacter_C_0 at TerritoryProperty_0: This player is exposed and belongs to a faction at War.
```

Who decided, whether it engaged or refused, about whom, at which Place, and why. Gated on
`UTerritoryDeveloperSettings::ShouldDebugCombat()`, which is the master `bEnableDebug` gate **and** the
`Debug|Combat` category **and** `DebugVerbosityLevel >= 5`. So it is silent by default, and one
checkbox turns it on. No new setting was added: a guard's engage decision *is* an attack permission,
which is what that category already documents.

Both directions are logged, deliberately. Your report is a guard acting on the **wrong** target, and a
log that only recorded refusals would have shown nothing at all in that case.

### The test, and how it was verified

A new test — `TerritoryFramework.Guards.Regression.DecisionReasonIsObservable` — captures the plugin's
own `LogTerritory` output with a custom `FOutputDevice` and asserts on **the text a developer would
actually read**, not on an internal variable. That distinction is the whole point of the fix.

The mandated order was followed: the test was written **first**, built, and run against unimplemented
logging. It went red with **exactly** the five logging assertions failing, and — importantly — with
*"A confirmed enemy at War is engaged"* passing, which proves the positive path is really reached and
the allow assertion will have real content. Then the implementation was written and the test went green.

Two mutations were then run to prove the assertions discriminate rather than merely pass:

| Mutation | Result |
|---|---|
| **M1** — remove the debug gate, log unconditionally | Exactly one failure: *"With the debug system off the plugin logs no guard decision"* |
| **M2** — log refusals only | Exactly one failure: *"The allow reason reaches the log too"* |

Each mutation was caught by precisely the assertion written to guard it, and only by that one. Restored,
rebuilt, re-ran.

**Full suite: 351 tests, 351 pass, 0 fail** (baseline before this step was 350/350 — one test added).

### Two engine traps hit on the way, recorded so they cost nothing next time

- **`Category != LogTerritory` does not compile.** `LogTerritory` is the *category object*
  (`FLogCategoryLogTerritory`), not the `FName` a log device receives. `GetCategoryName()` is the
  accessor.
- **The editor module cannot name `LogTerritory` at all.** `DEFINE_LOG_CATEGORY` does not export the
  symbol from the runtime module's DLL, so referencing it from a test is an unresolved external
  (`LNK2001`). The test therefore matches the category by its **name string** — which is the better
  contract anyway, because `LogTerritory` is exactly what a developer types into the Output Log filter.

### What this does *not* fix

It makes the bug **observable**; it does not correct the behaviour. Part 11 already identified the
mechanism (defender-first assault targeting), and task 10 in the plan shipped that change. This step is
what lets you confirm the next occurrence from a log line instead of a source read. It is also the
prerequisite for the still-open check of the reported Place's own assets — the two counterattack flags
and the guard post `FactionOverride` — because those values can now be read against a decision the log
narrates.

---

## Part 14 — The reported Place's own assets, read at last (2026-09-18)

This closes the check Part 13 left open: the two counterattack flags and the guard post
`FactionOverride` on the Place from the reported bug. It also produced the drift inventory that the
duplicate content mount had been hiding, and one finding that explains the report.

### The answers to the three questions asked

| Question | Answer | How it was proven |
|---|---|---|
| Are the guard posts forcing a faction? | **No — every override is empty** | 7 posts on `DA_Place_Blacksmith`, and `DA_PostGuardDefinition.FactionOverride`, all empty |
| Are the two activation flags set? | Both `false` on the shipped profile, in **both** mounts | direct property read |
| Is a missing counter-attack profile the cause? | **No — the Place has one** | `counter_attack_profile` resolves to `DA_CounterAttack` |

Empty is the **correct** value, not a defect: `TerritoryDefinition.h:213` documents it as "Empty means
guards inherit the current Territory owner at spawn time." A stale non-empty override would have been
the defect; there isn't one.

**Two controls proved the tag reader before any of it was believed.** The same helper that rendered the
empty override rendered `Narrative.Factions.Bandits` correctly for the force row's `Faction`, and
`initial_owning_faction`. A reader that prints "empty" for everything cannot be distinguished from a
real empty value — that is precisely how the earlier pass reported an unset tag that was set.

### The finding that explains the report

`DA_Place_Blacksmith` ships `guard_behavior.bEngageAtWarInClaimedTerritory = false`, the C++ default.
That flag is the whole difference between a garrison that fights and one that stands still:

```
TerritoryGuardCharacter.cpp:376-387
  if (OwningTerritory->GetTerritoryState() == ETerritoryState::Contested)
      return Result(true, "A faction at War is contesting the Place.");
  if (Behavior && Behavior->bEngageAtWarInClaimedTerritory)
      return Result(true, "The Place is claimed, but this faction is at War and the guard is ordered to defend it.");
  return Result(false, "The Place is not contested and there is no confirmed local threat.");
```

A Place the player captures **is** `Claimed`, not `Contested` — `TerritoryVolume.cpp:1635`:

```cpp
const ETerritoryState NewState = NewFaction.IsValid() ? ETerritoryState::Claimed : ETerritoryState::Unclaimed;
```

So the player's assigned guard, standing in a Claimed Place, **refuses** to engage the at-War counter-
attack force and reports "The Place is not contested and there is no confirmed local threat." That is
the report's "ignoring the one Guard Assign by Player Character", stated by the code itself.

The plugin documents this outcome in the flag's own comment (`TerritoryDefinition.h:141-142`):

> "With this off (default) the Regime's own guards keep ignoring you in every Place you captured for
> them, because those Places are Claimed and peaceful. With this on, the war has teeth — their guards
> defend the territory you won for them the moment the alliance breaks."

**This is a content default, not a code defect**, and it is deliberate — the same comment explains that
turning it on everywhere would make stealth and quest content inside hostile territory lethal. It is
also already covered by a test (`TerritoryGuardResponseTests.cpp:196-202` asserts both directions), so
there is no coverage gap to fill. The remedy is to author the flag on, which is also exactly the
"war has teeth" behaviour the betrayal vision in item 5 asks for.

Part 13's log line is what makes this confirmable in play rather than by source read: the refusal above
is written verbatim to `LogTerritory` under `Debug|Combat`.

**Ruled out rather than left as guesses.** `bPrioritizeClosestHostilePlayer` looks like the culprit by
name, but `TerritoryGuardCharacter.cpp:590-591` returns early unless the state is `Contested`, so it is
inert in a Claimed Place. The `1800 → 500` time window on the plugin's `DA_CounterAttack` also looked
like an inverted window, but `IsNarrativeTimeInWindow` wraps across midnight, those two numbers are the
C++ defaults (`TimeWindowStart = 1800.f`, `TimeWindowEnd = 500.f`), and `TimePolicy` defaults to
`AnyTime`, which ignores the window entirely. Neither is a defect.

The activation gate is likewise **not** the cause: with proximity not required, the garrison flag off,
time policy `AnyTime`, and the state `Claimed` after capture, `ShouldActivateWaitingAssault` returns
true immediately.

### The two content mounts have genuinely drifted

The project holds only the six sample Territory assets — there is no user-authored Territory content —
and the two copies of each have diverged. Verified differences, with the renderer's false positives
removed:

| Asset | Property | plugin `/TerritoryFramework/` | project `/Game/TerritoryFramework/` |
|---|---|---|---|
| every asset | `stable_territory_guid` | one GUID | **a different GUID** |
| `DA_CounterAttack` | force goal generator | `GoalGenerator_TerritoryAttack` | `/Game/HOPTRENDY/.../GoalGenerator_Hop_Attack` |
| `DA_CounterAttack` | time window | 1800/500 (defaults) | 0/2400 |
| `DA_CounterAttack` | `corpse_suspicion` (stealth) | 0.5 | 0.9744 |
| `DA_Place_Blacksmith` | `initial_availability` | UNLOCKED | **LOCKED** |
| `DA_Place_Blacksmith` | `max_concurrent_attackers` | 2 | 3 |
| `DA_Place_Blacksmith` | `quest_runtime_overrides` | none | wires `NQ_CaptureBlacksmith` |
| `DA_Place_Farm` | `capture_point.automatic_capture` | false | true |
| `DA_Place_Farm` | `gameplay_benefits` | empty | grants Weapon Upgrades |
| `DA_Place_Farm` | `production_profile` | 1 rule | 2 rules (adds a rifle-ammo refill) |

The project copy references `/Game/HOPTRENDY/...` and wires this game's own quest, so **the project copy
is the one adapted to this project** and the plugin copy is the stock sample. Which one the level
actually places is still unverified — that is the open check, and it decides whether the table above
describes the running game or only the samples.

`stable_territory_guid` differing on *every* asset is the most consequential entry: it means the two
copies are different territory identities, not the same identity stored twice, so save data keyed by
GUID cannot be expected to match across them.

### Why the differ had to be repaired before it could be believed

The first diff reported 10 differences on `DA_Place_Farm`, which was obviously wrong, and its own
must-be-same control failed. Three false-positive generators were hiding in the renderer, all found by
printing the first character where two rendered strings diverge rather than a truncated head:

1. **Mount prefix.** A reference renders `/TerritoryFramework/X` on one side and
   `/Game/TerritoryFramework/X` on the other, so every asset reference looked different. That is the
   duplicate mount leaking into the comparison, not authored drift.
2. **Heap addresses.** A struct this API will not enumerate fell back to `str()`, which embeds an
   address. `stable_territory_guid` rendered as `{}` with only `0x...` differing — the GUID *value* was
   never read, and the diff was pure address noise. It is read for real above.
3. **Negative zero.** `y: -0.000000` versus `y: 0.000000` is mathematically identical and is what broke
   the must-be-same control on `guard_posts`. The guard posts are in fact identical, matching the
   field-by-field read.

Two lessons worth keeping, because both cost real time here:

- **A must-be-same control is worth more than a must-differ control.** The must-differ half passed the
  whole time, while the must-be-same half is what exposed the renderer. A differ that only proves it
  can find differences will happily report a hundred of them.
- **An unexplained diff and a real difference look identical in truncated output.** Printing the first
  divergence turned nine unexplained keys into a definite answer in one run.

---

## Part 15 — Which content mount the running game actually uses (2026-09-18)

Part 14 ended with an open question: the two content mounts have drifted, and the table it printed
describes the running game only if the level places the project copy. **It does. Verified by reading
the placed actors' own property, not by inference.**

### The answer

The startup map is `/Game/HopDistrictTest` (`Config/DefaultEngine.ini:3`). Loading it and reading every
actor that carries a `territory_definition`:

| Placed actor | `territory_definition` |
|---|---|
| `BP_Property_Blacksmith_C` | `/Game/TerritoryFramework/Definitions/DA_Place_Blacksmith` |
| `BP_Property_Farm_C` | `/Game/TerritoryFramework/Definitions/DA_Place_Farm` |
| `BP_City_HavenReach_C` | `/Game/TerritoryFramework/Definitions/DA_City_HavenReach` |
| `BP_District_CastleHill_C` | `/Game/TerritoryFramework/Definitions/DA_District_CastleHill` |
| `BP_District_MarketSquare_C` | `/Game/TerritoryFramework/Definitions/DA_District_MarketSquare` |

**Every one is the PROJECT mount.** Across all 1566 World packages under `/Game`, only five reference
Territory content at all, and the totals are **plugin = 0, project = 36**. No level anywhere uses the
plugin's `Content/` copy.

So the drift table in Part 14 describes **the running game**, not just the samples. In particular the
running Blacksmith Place ships `initial_availability = LOCKED` and wires `NQ_CaptureBlacksmith`, and
the running `DA_Place_Farm` has `automatic_capture = true`.

### Two probes had to be thrown away first, and why

**Pass 8 was worthless.** It called `registry.get_dependents(...)`, which does not exist on this
Python wrapper. Every "0 dependent(s)" line was a swallowed exception, and — the part that matters —
its *absent* control used the same broken call, so the control "passed" because the call failed rather
than because the package was absent. **A control that can only fail is not a control.**

**Pass 10 was also worthless, for the same defect in a different costume.** It scanned each
Blueprint's CDO for properties whose *name* contained "definition", found eight, and printed
`CONTROL => chain reader WORKS`. But all eight values were `None`. It had matched a **name**, not a
**value**. That is exactly how the earlier pass reported an unset GameplayTag that was set.

The useful finding that fell out of pass 10 anyway: `territory_definition` is `None` on the CDO of all
five project-mount Blueprints *and* all three plugin-mount Blueprints. The Blueprint default carries no
definition — it is assigned per placed instance. That is *why* reading the level was the only route to
the answer, and it is why a CDO-only check could never have settled this.

Pass 11 fixed the control by checking values: the level must load and yield actors (78 — OK), and at
least one actor must expose a non-null definition (5 — OK). Only then were the REF lines believed.

### One probe detail worth keeping

The 17 actors that printed `territory_definition = '<ERR AttributeError>'` are **correct**, not
failures: guard spawn points, capture points, road guides and the world-state actor genuinely have no
such property. The five that do have it read cleanly. A reader is proven by the values it returns
where a value exists, not by printing something for every object.

### Standing consequence

The plugin's `Content/` mount is **dead content in this project** — nothing references it. Any edit
made only to the plugin copy is invisible in play. The project copy at `/Game/TerritoryFramework/` is
the live one, and it is the one any gameplay change (such as authoring
`bEngageAtWarInClaimedTerritory = true` from Part 14) must be written to.

---

## Part 16 — Defect 2 is closed: it was never non-deterministic (2026-09-18)

Part 7 recorded `TerritoryFramework.Integration.CounterAttackMapConfiguration` as **non-deterministic
on process-level `UZoneGraphSubsystem` registration**, and said plainly that the honest move was to
track that rather than guess a fix. That diagnosis was right to refuse a guess and wrong about the
cause. **The test is fully deterministic — it just depends on a piece of editor state it never
established.**

### The mechanism, proven by a one-variable experiment

`TerritoryNarrativeProMigrationTests.cpp:459` does:

```cpp
UWorld* World = LoadObject<UWorld>(nullptr, TEXT("/Game/HopDistrictTest.HopDistrictTest"));
```

and passes that world to the route validator, which fails at
`TerritoryCounterAttackSubsystem.cpp:3947` when `World->GetSubsystem<UZoneGraphSubsystem>()` is null.

`LoadObject` returns the **already-initialised editor world** only when that map is the one the editor
currently has open. Point the editor anywhere else and it returns a freshly loaded, **uninitialised**
asset world, whose world subsystems do not exist — so the route can never validate. Confirmed directly
from Python: with `/Game/HopDistrictTest` open, `load_asset("/Game/HopDistrictTest")` returned *the
same object* as the editor world (identical address `0x0000017C137C1C00`).

Running **one test, twice, changing only the editor's current world**:

| Editor's current world | Result | Message |
|---|---|---|
| `/Game/HopDistrictTest` (the project's `EditorStartupMap`) | **Pass** | — |
| `/Game/HOPTRENDY/Map/L_AlMalik` | **Fail** | `Blacksmith Sedan route is complete: No ZoneGraph subsystem is available` |

That is the original red, reproduced on demand — and it reproduces **deterministically**, which is
what refutes the non-determinism story. The earlier audit could not reproduce it because every run was
headless with the startup map open, which is the passing configuration.

Two supporting facts, both measured rather than assumed:

- The map **does** carry ZoneGraph content — 1 `ZoneGraphData` actor and 2 `ZoneShape` actors — so
  "the map has no ZoneGraph" was never the explanation.
- The ZoneGraph *types* are exposed to Python (`unreal.ZoneGraphData`, `ZoneGraphTag`,
  `ZoneGraphTagFilter`) but the **subsystem class is not**: `unreal.ZoneGraphSubsystem`,
  `unreal.ZoneGraphSubsystemLibrary` and `unreal.SubsystemBlueprintLibrary` all resolve to `None`, and
  `dir(world)` contains **zero** entries matching "subsystem". That is a checked negative, not an
  assumption — which is why this had to be observed through C++.

### The fix

The test now asserts the precondition instead of inheriting it, and reports an explicit skip when it
cannot be met:

```cpp
if (!World->GetSubsystem<UZoneGraphSubsystem>())
{
    AddInfo(TEXT("Vehicle-route validation skipped: the loaded world has no UZoneGraphSubsystem. "
        "Open /Game/HopDistrictTest in the editor (it is the project's EditorStartupMap) so the "
        "test receives an initialised world, then re-run to validate the route."));
}
else
{
    // ... the three route assertions, unchanged ...
}
```

The three assertions are **untouched inside the else** — this is deliberately not a weakening. Note
also that the skip is reached only for this one reason; every *other* route failure still fails.

### Verification, and the control that stops this being a cover-up

The dangerous failure mode of a fix like this is neutering the test: making it green by never running
the check. So the two runs were made a two-sided control:

| Run | Result | Skip message in log | What it proves |
|---|---|---|---|
| Startup map, **before** fix | Pass | absent | baseline green |
| Other map, **before** fix | Fail | absent | the red exists |
| Startup map, **after** fix | **Pass** | **absent** | validation still genuinely executes |
| Other map, **after** fix | **Pass** | **present** | the false red is gone, and reported as a skip, not a pass |

The third row is the one that matters. If the fix had neutered the test, the skip message would appear
there too. It does not — in the standard invocation the route is still validated for real. And in the
full suite the skip appears **0 times**, so nothing was silently downgraded.

Full suite after the change: **351 tests, 351 pass, 0 fail** (the runner's own line: `Found 351
automation tests based on 'TerritoryFramework'`). The earlier baseline recorded 350; this change adds
no test, so the +1 predates it and is **not** reconciled here — recorded rather than explained away.

### Two method notes worth keeping

- **My first attempt at this experiment was invalid and I threw it away.** Git Bash's MSYS path
  conversion rewrote the map argument `/Game/HOPTRENDY/Map/L_AlMalik` into
  `D:/Program Files/Git/Game/HOPTRENDY/Map/L_AlMalik`, so the editor never loaded a different map and
  both runs passed. The "pass" was an artifact of my own instrument. Re-run with
  `MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*'` and the red appeared immediately. **A passing control
  run is not reassuring if you have not confirmed the variable actually changed.**
- **"Non-deterministic" is a conclusion that needs the same evidence as any other.** Here it survived
  an audit pass and a memory entry because it was consistent with every observation that had been
  made. It was disconfirmed by one experiment that varied a single input.

---

## Part 17 — Item 7: the "lock bool / lock config" that does not work (2026-09-18)

### The complaint, in the user's words

> "start lock bool in every district blueprint and there is a lock config although they are not working
> instead we have state based config unclaim, claimed, contested, locked does the same with advancement."

### Answer to the premise: the user's instinct is correct

Three mechanisms exist for "how does a Territory begin, and how does it change". Only one of them does
anything, and it is the state-based one the user points at.

| Mechanism | Declared at | Real state |
|---|---|---|
| `ETerritoryInitialState InitialState` | `TerritoryVolume.h:750` | Live, but its `Locked` value is `UMETA(Hidden)` legacy — **a designer cannot select it** |
| `ETerritoryAvailability InitialAvailability` | `TerritoryVolume.h:747` | Live; the supported way to start Locked |
| State configs — `StateConfigs` + `ExitConditions` | `TerritoryTypes.h:598-601` | Live; **the working mechanism** |

So "the lock bool does not work" is true, and it is not a code bug: the legacy half is *supposed* to be
inert, and the state configs are what replaced it.

### Why the legacy values look dead

- **`InitialState == Locked` is unreachable at runtime.** The compatibility rule
  `InitialState == Locked ? Locked : InitialAvailability` is implemented three times, and because
  `Locked` cannot be chosen in the editor, that branch never fires. It exists only to read old saves.
- **Nothing auto-unlocks.** This is the part that reads as a bug and is not one.

### The gate is real, uniform, and not special-cased for `Locked`

- `CanUnlock()` (`TerritoryVolume.cpp:2205`) is a **const query**. It answers a question; it does not act.
- `CheckStateExitConditions(...)` is called from exactly two places: inside `CommitOwnershipData`
  (`:1785`), i.e. while a transition is *already being committed*, and inside `CanUnlockWithContext`
  (`:2215`), which is itself a query.
- Requesting an unlock has exactly **two production callers**: `TerritoryControlSubsystem.cpp:107` (the
  unlock API) and `TerritoryStoryEvents.cpp:212` (story events).

**Exit conditions are a gate, not a trigger.** They decide whether a *requested* change may proceed;
nothing polls them to *start* one. That is coherent design, and it treats every state the same way —
there is no `Locked` special case and no `bAutoUnlock` anywhere in the plugin.

### The genuine defect was documentation, and it is the direct cause of "they are not working"

`Docs/Blueprint_Extension_Guide.md` carried a "Pattern: Quest-gated territory" that gave a designer two
false instructions:

```
Initial State: Locked                                     ← a value that cannot be selected
State Configs -> Locked -> Exit Conditions: [QuestComplete_Q001]
→ Territory stays Locked until quest Q001 completes        ← false; nothing polls, nothing opens it
```

A designer following that pattern gets a Territory that never opens, and the pattern tells them the code
is to blame. That is exactly the experience item 7 reports. The `ExitConditions` tooltip said "Every
condition must pass before this state can end", which is true but omits the load-bearing half: *something
must still ask.*

### The fix — three files, documentation only

1. `Docs/Blueprint_Extension_Guide.md` — pattern corrected to `Initial Availability: Locked`, the false
   claim replaced with "stays Locked until something **REQUESTS** the unlock **AND** Q001 is complete",
   plus a blockquote stating that Exit Conditions are a gate, that if nothing asks a Locked Territory
   stays Locked forever however many conditions pass, and that `Initial State`'s `Locked` option is
   hidden for save compatibility.
2. `Source/TerritoryFramework/Public/Core/TerritoryTypes.h:600` — the `ExitConditions` tooltip now says
   the same thing.
3. `Docs/37_Every_Option_Reference.md:672` — the generated reference's quotation of that tooltip,
   updated in the same step so the two cannot drift.

### Deliberately not changed

- **`TerritoryStoryOutcomeAnalyzer.cpp:262-313` was left alone.** I suspected it taught auto-unlock and
  went to fix it; it already says "…or a trusted server action later tries to unlock it" and "A failed
  local condition blocks that requested lock/unlock." My hypothesis was wrong. Correct code was not
  "fixed".
- **No `bAutoUnlock` was added.** That would silently change gameplay for every already-authored
  District — a design change, not a defect repair.

### Verification

- **Build:** `Result: Succeeded`, `Total execution time: 57.94 seconds`. Both DLLs relinked at
  `2026-09-18_12:18:34`, newer than the edited header (`05:19:00`) — the compile really happened.
- **Suite:** runner's own line `Found 351 automation tests based on 'TerritoryFramework'`;
  **351 pass, 0 fail**.
- **The Part 16 fix held:** `Vehicle-route validation skipped` appears **0 times**, so that test is
  still validating the route rather than passing by skip.
- **Honest limit, stated rather than glossed.** A tooltip string has no test. The suite is a
  *regression* check, not evidence that the new wording is correct — the wording is verified by reading
  the call paths above. The two doc edits are likewise verified by reading. What this run proves is: the
  header compiles and nothing regressed.

### A false green worth recording (the second of this audit)

The first attempt at this rebuild printed **`BUILD EXIT=0`** and had written a **132-byte** log
containing nothing but the `cmd.exe` banner — the batch file was never executed, and both DLLs were
still older than the source. A zero exit code from this invocation means nothing. What settled it was
the log's own `Result:` line plus the DLL timestamps, not the code. Same lesson as the MSYS trap in
Part 16: **verify the instrument before trusting its reading.**

Also recorded: this build was blocked for about an hour because `UnrealEditor.exe` (PID 30984) and
`LiveCodingConsole.exe` (PID 21760) were holding it — UBT refuses to link while Live Coding is active
(`Unable to build while Live Coding is active`). That editor session was **not** launched by me, so I
did not kill it; it was closed by the user, and the build then completed. Killing another session to
win a build is not a trade this audit makes.

### Consequence for the item-5 betrayal vision

The gate semantics are exactly what a betrayal needs: a faction-attitude change can *request* an unlock
and the gate decides whether it is allowed. But the same semantics mean **every intended lock needs a
caller** — a District authored `Locked` with no story event wired to it is Locked forever. That is now
documented as behaviour rather than left to look like a broken bool.

---

## Part 18 — §3.6, first item: the initial-availability rule had *four* copies, not three (2026-09-18)

### Correction to Part 3 before anything else

Part 3 §3.6 recorded that the legacy rule
`InitialState == ETerritoryInitialState::Locked ? Locked : InitialAvailability` was **implemented three
times**. It was four. The audit missed `TerritoryStoryOutcomeAnalyzer.cpp:221`, which is the *editor*
copy — the one that tells a designer what a Territory's starting availability will be. The count is
corrected here rather than quietly fixed, because the missed copy was the most consequential one: a
rule that disagrees between the analyzer and the runtime produces a designer-facing report that is
simply untrue, which is the same failure shape as item 7.

| # | Site | Role |
|---|---|---|
| 1 | `TerritoryVolume.cpp:1576` | the placed actor's own answer |
| 2 | `TerritoryWorldState.cpp:81` | the replicated summary a client reads |
| 3 | `TerritoryDefinition.cpp:142` | applying a Definition onto a Territory |
| 4 | `TerritoryStoryOutcomeAnalyzer.cpp:221` | **what the editor tells the designer** (missed in Part 3) |

### The refactor

One exported function, `TerritoryResolveInitialAvailability(InitialState, InitialAvailability)`,
declared in `Core/TerritoryTypes.h` and defined in `Core/TerritoryTypes.cpp`. All four sites now call
it. The rule itself is unchanged — this is a deduplication, not a behaviour change, and that is the
point: the four copies agreed *today*, and nothing was keeping them agreeing tomorrow.

`TerritoryTypes.h` was chosen as the home because both modules already include it, so the editor
analyzer and the runtime now share one definition rather than one each.

### Why this needed an unusual control

**A behaviour-preserving refactor has no red available by construction.** The refactor changes nothing
observable, so there was no failing test to write first — the new test passed the moment it was
written. That is not evidence. The honest control for this shape is a **mutation test**: break the rule
on purpose, confirm the test notices, then restore.

Two facts are easy to get backwards and both need guarding, so the test asserts both directions:

- The legacy value **wins**: `InitialState = Locked` must stay `Locked` even when
  `InitialAvailability = Unlocked`. Dropping this branch silently unlocks every Territory authored
  before `InitialAvailability` existed — invisible, and nothing else in the suite would notice.
- Everything else **passes through**, including `Unlocked`. Without this direction, a "simplify it to
  always return Locked" edit would pass.

### The control, recorded run by run

| Run | Change | Build | Test result |
|---|---|---|---|
| 1 | refactor in place | Succeeded (52.79 s) | **Pass** — `Found 1`, 1 pass |
| 2 | helper mutated to `return InitialAvailability;` | Succeeded | **Fail** — see below |
| 3 | mutation reverted | Succeeded | **Pass** |
| 4 | full suite | — | `Found 352`, **352 pass, 0 fail** |

Run 2 failed for exactly the intended reasons, and the failure text is quoted because that is the
evidence:

```
Error: Expected 'InitialState=3 with InitialAvailability=0 resolves correctly' to be 1, but it was ...
Error: Expected 'Legacy Initial State = Locked keeps a Territory locked even when Initial Availability ...'
Result={Fail} Name={InitialAvailabilityRule}
```

`InitialState=3` is `Locked` and `InitialAvailability=0` is `Unlocked` — the one grid cell the mutation
should break. It is worth naming what did **not** fail: the pass-through assertions, because the
mutation pushed in the opposite direction. A mutation that made the helper always return `Locked`
would fail those instead. Both directions are covered; only one was exercised.

### Suite count

**352 tests, 352 pass, 0 fail.** The runner's own line: `Found 352 automation tests based on
'TerritoryFramework'`. Baseline was 351; the difference is exactly the one test added here, which is
stated because the previous session left a +1 *unreconciled*. This one is reconciled: one new
`IMPLEMENT_SIMPLE_AUTOMATION_TEST`. The ZoneGraph skip still appears **0 times**, so Part 16's fix
remains intact.

### What is NOT done yet — the same defect, second rule

The sibling rule — "does this Territory start Claimed?" — is duplicated the same way and was left
alone deliberately, one step at a time:

- `ATerritoryVolume::ResolveInitialTerritoryState()` (`TerritoryVolume.cpp:1552`) — a full `switch`
- `TerritoryStoryOutcomeAnalyzer.cpp:~200-215` — a second, near-identical `switch`
- `TerritoryWorldState.cpp:83-88` — a third expression of it as `bStartsClaimed`

This one carries a stated invariant in a comment — *"Never create the contradictory state 'Claimed with
no owner'"* — which the three copies each restate. It is the next step, and it is worth doing for the
reason the analyzer copy above proves: the editor was already the site that drifted.

---

## Part 19 — §3.6, second item: the initial *political* state rule, same treatment (2026-09-18)

The sibling of Part 18, and the same disease: the rule *"does a new campaign start this Territory owned
or unowned?"* was written out three times, each copy restating the same invariant in its own comment.

| # | Site | Role |
|---|---|---|
| 1 | `ATerritoryVolume::ResolveInitialTerritoryState()` (`TerritoryVolume.cpp:1552`) | a full four-case `switch` |
| 2 | `ResolveInitialPlaceState()` (`TerritoryStoryOutcomeAnalyzer.cpp:201`) | a second, near-identical `switch` |
| 3 | inline `bStartsClaimed` (`TerritoryWorldState.cpp:83`) | a third expression: `owner.IsValid() && InitialState != Unclaimed` |

### What the rule actually is, once you write it down

Collapsing the three copies shows they all say the same short thing, which none of them said plainly:

- **`Unclaimed` means unclaimed** — and it wins even over a filled faction, because that is what the
  option promises.
- **Every other value behaves identically**: `Automatic`, `Claimed` and the legacy `Locked` all resolve
  to "owned if a faction is set, unowned if not". They differ in *availability*, not ownership.
- Therefore **"Claimed" with an empty faction resolves to Unclaimed** — the game never creates an
  ownerless claimed Territory.

The old `switch` bodies made this look like four distinct cases; it is two branches. The new
`TerritoryResolveInitialPoliticalState(InitialState, bHasInitialOwningFaction)` states it as two, with
the reason each one exists.

### The refactor

One exported function in `Core/TerritoryTypes.h` / `.cpp`, beside the availability helper from Part 18.
It takes `bHasInitialOwningFaction` rather than the tag itself so a Definition asset, a placed actor and
the replicated world state can all call it without needing each other's types.

`TerritoryWorldState.cpp` now derives both `CurrentOwner` and `State` from that single answer. They were
already derived from one `bStartsClaimed`, so this is not a fix — but it makes it structurally
impossible for the replicated summary to report a `State` that disagrees with its own `CurrentOwner`,
which is worth having on the replication path.

### The control — and what makes this one stronger than Part 18's

Same shape: **behaviour-preserving refactor, so no red is available by construction**, hence a mutation.
But this rule already had a **pre-existing test** — `TerritoryFramework.State.DefinitionInitialState`
(`TerritoryFrameworkTests.cpp:492-545`) — covering the placed-actor path with exactly the interesting
cases: `Locked` preserves its owner, explicit `Unclaimed` beats a filled faction, `Claimed` with no
faction resolves to Unclaimed. So the mutation could be aimed at the shared helper to see whether
assertions written *before* the refactor still bite.

| Run | Change | Build | Result |
|---|---|---|---|
| 1 | refactor in place | Succeeded | **3 pass** — the two new rule tests + the pre-existing `DefinitionInitialState` |
| 2 | helper mutated to drop the `Unclaimed` override | Succeeded | **2 failures** — see below |
| 3 | mutation reverted | Succeeded | **3 pass** |
| 4 | full suite | — | `Found 353`, **353 pass, 0 fail** |

Run 2, quoted as evidence:

```
Result={Fail} Name={DefinitionInitialState}
Result={Fail} Name={InitialPoliticalStateRule}
Error: Expected 'Explicit Unclaimed wins even when a faction is set' to be 0, but it was 1.
Error: Expected 'InitialState=1 with owner set=true resolves correctly' to be 0, but it was 1.
```

Two things make this a better control than Part 18's:

- **A test written before the refactor caught a change made inside it.** That is direct evidence the
  shared helper is load-bearing for the existing assertions — not merely that a new test agrees with
  itself. If the refactor had introduced a second, silently-divergent rule, `DefinitionInitialState`
  would have stayed green while the new test went red, or vice versa.
- **`InitialAvailabilityRule` stayed green through the same run.** The mutation was aimed at one rule
  and exactly one rule failed; the other rule's test was unaffected. That is the isolation check the
  Part 18 control could not provide, because there was only one rule in play then.

### Suite count

**353 tests, 353 pass, 0 fail** (`Found 353 automation tests based on 'TerritoryFramework'`). Previous
run was 352; the delta is exactly the one test added here, so the count remains reconciled rather than
merely reported. The ZoneGraph skip still appears **0 times**.

### What §3.6 still has open

One item from Part 3 §3.6 remains untouched:

- **`ETerritoryState::Locked` is unreachable at runtime** — `Locked` is an *availability*, and the two
  rules above are now the only places that translate it. Worth revisiting, but it is a semantic
  question, not a refactor.

The guard-lifecycle dead branches are now **checked** — see Part 20. They are dead on every path, not
just live gameplay, so deleting them is safe. **Deleted 2026-09-19** — see the closure note at the end
of Part 20.

---

## Part 20 — §3.6, third item: the guard-lifecycle dead branches, checked against a live guard (2026-09-19)

Part 19 left this item explicitly blocked: *"That one should not be done until it can be tested against
a running guard, not just compiled."* This is that test, run in PIE against the live level
`HopDistrictTest` with a real garrison driven through lock and unlock.

### The claim under test

`TerritoryGuardLifecyclePolicy::DetermineAction` (`TerritoryGuardLifecyclePolicy.h:17`) has two branches
Part 3 called dead:

- `Retire` — requires `NewState == ETerritoryState::Locked` (`:45-48`)
- `Restore` — requires `OldState == Locked && NewState == Claimed` (`:50-55`)

with `DetermineAction` called from exactly one place, the switch at `TerritoryVolume.cpp:1880`.

The audit inferred they were dead because `CommitOwnershipData` rejects `NewState == Locked`
(`:1727-1733`). The gap the item flagged is real: an inference drawn from a guard is not the same thing
as watching a garrison.

### The live level, and why it is already in the state under test

`HopDistrictTest` places five Territory volumes and **seven** `BP_TerritoryGuardSpawnPoint_C` actors.
Measured at PIE start:

| volume | measured at session start |
|---|---|
| `BP_City_HavenReach0` | Contested, unlocked, 0 guards, `desired=0` |
| `BP_District_CastleHill0` | Contested, unlocked, 0 guards, `desired=0` |
| `BP_District_MarketSquare0` | Contested, unlocked, 0 guards, `desired=0` |
| **`BP_Property_Blacksmith1`** | **LOCKED, Claimed, 0 guards, `desired=3`** |
| `BP_Property_Farm_CastleHill` | LOCKED, Claimed, 0 guards, `desired=0` |

The Blacksmith is authored **locked with a garrison target of 3**, so a session opens with the lock
already in force and no guards standing — precisely the precondition the dead branches were written for.

### Driving it

Through `editor_query run_pie_smoke` with timed Python probes run against the live PIE world. Three
harness facts, all learned the hard way, worth keeping for the next live check:

- **Probe `print` output is captured nowhere** — not in the action's `python_output`, not in the log.
  Probes must write to a file. Every probe here appends to `Saved/tf_probe*.txt`.
- **Two Python APIs do not exist in this build**: `unreal.Text.from_string` and
  `unreal.SubsystemBlueprintLibrary`.
- `LockTerritoryWithContext` is reachable as `lock_territory_with_context(unreal.Text(),
  unreal.TerritoryTransitionContext())` — a **default-constructed `FText`** works fine.

### The evidence

Four transitions in one session, each sampled after it had settled:

| step | `guards` | guard *actors* | `IsLocked` | `State` | `Availability` |
|---|---|---|---|---|---|
| baseline (authored) | 0 | 0 | true | **Claimed** | Locked |
| `try_unlock(true)` | **3** | **3** — `BP_TerritoryGuard0/1/2` | false | **Claimed** | Unlocked |
| `lock_territory_with_context(...)` | **0** | **0** | true | **Claimed** | Locked |
| `try_unlock(true)` again | **3** | **3** — `BP_TerritoryGuard3/4/5` | false | **Claimed** | Unlocked |

Three things this settles:

1. **The real despawn path works.** Locking a garrisoned Territory took the guard count from 3 to 0 —
   and the guard **actor** count to 0 with it, so the guards were destroyed, not merely deregistered. A
   deregistration-only bug is exactly what a dead branch could have been concealing, which is why the
   actor count is the measurement that matters rather than the registry count.
2. **The real restore path works, and it is a spawn.** The second unlock produced three *new* actors
   (`Guard3/4/5`; `Guard0/1/2` were gone). This confirms the header's own claim that restoring a
   garrison "happens by normal spawn on the next unlock, not here".
3. **`desired=3` survived the lock.** The garrison target is a field, not a state — which is why an
   unlock can restore to full strength without any lifecycle bookkeeping.

### Why `State` never changing is the discriminating measurement

At every sample `State == Claimed`, never `Locked`. That is not incidental; it is the refutation:

- `DetermineAction` is called at `:1880`, **after** `OwnershipData = MoveTemp(CommittedData)` at
  `:1851`. The state the switch reads is therefore the state observed from outside — there is no
  sampling race to worry about.
- At the lock, `OldOwner == NewOwner` and `OldState == NewState` (`Claimed` → `Claimed`), so
  `DetermineAction` returns **`Preserve` at `:28`** — before it ever compares against `Locked`. The
  switch body took `case Preserve: default: break;`.
- Therefore `DespawnGuards()` was **not** called from inside the commit. It came from
  `ReconcileAvailabilityDependentSystems():1539`, reached because `LockTerritoryWithContext` calls it at
  `:2242` — which is what Part 3 claimed, and what this run now *shows* rather than asserts.
- `Restore` needs `OldState == Locked`; the pre-unlock samples read `Claimed`, so it did not fire
  either.

### Closing the legacy-save loophole

`Restore` was only dead *from live gameplay* on the Part 3 reading, because `OldState` reads
`OwnershipData.State` and a loaded save could in principle supply `Locked`. So every write to that field
was enumerated:

| site | why it cannot write `Locked` |
|---|---|
| `TerritoryVolume.cpp:195` — initial state | `TerritoryResolveInitialPoliticalState` returns Claimed/Unclaimed/Contested; already asserted by `TerritoryInitialAvailabilityRuleTests:147` |
| `:227` — **save migration** | converts `Locked` → `Availability = Locked` + Claimed/Unclaimed itself |
| `:252` | literal `Unclaimed` |
| `:1625` — `SetOwningFactionWithContext` | literal Claimed/Unclaimed (`:1620`) |
| `:1674` — `SetDerivedControl` | `:1661-1665` normalizes `Locked` away first |
| `:1986` — `ForceSetTerritoryState` | `:1972-1979` redirects `Locked` to `LockTerritoryWithContext` |
| `:2235` — `LockTerritoryWithContext` | writes Claimed/Unclaimed |
| `:2286` — `TryUnlockWithContext` | writes Claimed/Unclaimed |
| `TerritoryControlSubsystem.cpp:1417` | a `Locked` value is rejected by `CommitOwnershipData:1727` before it can commit |
| `TerritoryControlSubsystem.cpp:1673` | `:1564` rejects `DesiredState == Locked` before this line |

Four entry points reject `Locked` (`CommitOwnershipData:1727`, the control-subsystem validator `:1564`,
`SetDerivedControl:1661`, `ForceSetTerritoryState:1972`), and the one path that could have injected it —
the pre-availability save migration at `:222-229` — converts it instead. `OwnershipData.State` can
therefore never hold `Locked`, **including after loading an old save**. Both branches are dead by
construction, not merely unobserved.

### What is *not* dead

Deleting the branches must not be read as deleting the `Locked` concept. `TerritoryVolume.cpp:1925-1928`
fires the `Locked` **state config row** when availability flips:

```cpp
if (OldAvailability != NewAvailability)
{
    FireStateEvents(ETerritoryState::Locked,
        NewAvailability == ETerritoryAvailability::Locked, TransitionContext);
}
```

So `DA_Place_Blacksmith`'s authored `Locked` row — which exists, alongside all four states — still drives
designers' lock and unlock events. **The row stays; only the two unreachable `DetermineAction` branches
go.**

### Verdict and limits

**Both branches are dead on every path. Deleting them loses no live behaviour.** The behaviour they were
written for — retiring a garrison on lock, restoring it on unlock — demonstrably happens through
`ReconcileAvailabilityDependentSystems()` and works in both directions on real actors.

Limits, stated plainly:

- **The deletion is not in this change.** This part records the check that unblocks it; the edit, the
  rebuild and the re-run are still pending. The re-run is the real confirmation — the same probe
  sequence must reproduce the table above. *(**Superseded 2026-09-19** — all three are now done and the
  sequence did reproduce the table. See the closure note at the end of this part.)*
- **`DetermineAction` was not instrumented**, so "the switch took `Preserve`" is a deduction from the
  observed `State` plus the call ordering, not a logged branch. It is a sound deduction — the switch
  cannot read a state different from the one committed two statements earlier — but it is a deduction.
- **No `NM_Client` was exercised.** Same limitation as the F6 work: the PIE tooling exposes no
  player-count parameter. The lock/despawn path is authority-side, which is where it was measured.

### An incidental observation, outside §3.6

`BP_District_MarketSquare0` went **Contested → Claimed with no player action** in both PIE runs, after
the 3-second sample and before teardown, while `BP_City_HavenReach0` and `BP_District_CastleHill0` stayed
Contested. Nothing in this check touched that volume — the probes only ever called lock/unlock on the
Blacksmith. It may be the authored initial state resolving late, or it may be the counter-attack /
domination system claiming a district unprompted. Not chased here. Recorded because a district claiming
itself belongs to the area reported in item 1, and it is reproducible.

### Suite count

Unchanged at **354 tests, 354 pass, 0 fail** — this part adds no code, only evidence. The next change
(the deletion) will move the count, because the unit test at `TerritoryFrameworkTests.cpp:1533-1539`
exists to keep those two enum values alive.

### Closure — the deletion was applied (2026-09-19)

Both branches are gone. `Retire` and `Restore` no longer exist as enum values.

**The prediction immediately above is wrong, and worth recording as wrong.** The count did **not**
move. Removing assertions from inside an existing test does not change the test count, so the suite
still reads **354 tests, 354 pass, 0 fail**. A `Path=` set diff against the 9/17 log confirms nothing
was added or removed: deduped path sets of size 354 on both sides, and the four differences all
predate this change (availability rules, treaty replication, `Guards.Regression.DecisionReasonIsObservable`).
The reasoning "the test exists to keep those enum values alive, so deleting them moves the count"
conflated *the test's assertions* with *the test's existence*.

What was removed: both enum values and both `DetermineAction` branches
(`TerritoryGuardLifecyclePolicy.h`), the two `switch` cases (`TerritoryVolume.cpp:1879`), and the two
assertions that kept them alive. The `Locked` state-config row was left in place exactly as required
above — it still fires for designer lock and unlock, and the comment left in its place says so.

Rather than delete the two assertions outright they were replaced with assertions pinning the
surviving behaviour — `Claimed → Locked` and `Locked → Claimed` now both assert `Preserve` — so a
reintroduced `Locked` branch fails the test instead of regrowing silently. That the replacements
actually discriminate was checked by inverting one, rebuilding, and confirming the run reported
`Result={Fail}` with that exact assertion's message and zero successes; then reverted and rebuilt.

**The re-run happened, and it is the confirmation this part asked for.** The probe sequence above was
replayed verbatim against the rebuilt DLL, on a fresh editor session on `/Game/HopDistrictTest`. The
table is identical to the one recorded here line for line, with one expected difference: the `A|DEFN=`
object address, which a fresh process reallocates. The rows that decide the question:

| Stage | Row | Baseline | Post-deletion |
|---|---|---|---|
| C | `GARRISONED` | `guards=3` | `guards=3` |
| D | `POST_LOCK` | `guards=0` | `guards=0` |
| G | `FINAL` | `guards=3`, `GUARD_ACTORS=3` | `guards=3`, `GUARD_ACTORS=3` |

Session verdict `ok: true`, all seven probes fired with `python_ok: true`, zero `must_absent` hits in
both the active-runtime and teardown windows. Unlock still spawns the garrison and lock still despawns
it, with the branches that nominally did that work deleted — so the deduction this part flagged as a
deduction, that the switch takes `Preserve` and the churn is `ReconcileAvailabilityDependentSystems()`'s,
now has behavioural confirmation rather than only call-ordering.

The limits stated above still stand: `DetermineAction` remains uninstrumented, and no `NM_Client` was
exercised. The `BP_District_MarketSquare0` Contested → Claimed observation reproduced unchanged, and
is still unchased.

---

## Part 21 — The capture flow as Narrative quests and dialogues, and the Farm owner that cannot spawn (2026-09-19)

The question, verbatim: *"check i have setup whole territory capture as Narrative Quest and Dialogues."*
And on the Farm: *"Castle Hill Farm using Auto Capture — check if territory with no defending guard can
spawn FarmOwner."*

Both are answered below. **No code or asset was changed** — this part is evidence only.

### Method

Authored `.uasset` values were read **through the running editor** (Python against the live asset
registry), not probed from disk — Part 14 is the record of why disk probing lies on this project. Source
claims were read from the plugin's own C++. No PIE session was run: every question here is about authored
data and static call paths, so a live session adds nothing.

The referencer checks below use two tools, and **one of them is unreliable** — see *Traps*. Where they
disagree, the disagreement is recorded rather than resolved in favour of the convenient answer.

### The quest exists, and its logic is authored

`NQ_CaptureBlacksmith` is **not** the blank template an earlier read of this session suggested. Only its
*metadata* is untouched. The asset holds **seven `QuestState` subobjects** and a fully connected 13-node
Quest Graph:

```
Root → Action_23 → State_3 → Action_0 → State_0 → Action_21 → State_7 → Action_2 → State_10 → Action_19 → State_5 → Action_4 → Success_0
                                                              ↑______________________________________|
```

`Action_19` takes input from both `State_7` and `State_10` and feeds `State_5`, so the
`State_7 → Action_2 → State_10 → Action_19 → State_7` ring is a **repeat loop** that exits to the success
terminal. This is a real state machine, not scaffolding.

The authored content per state:

| State | Authored content |
|---|---|
| `QuestState_0` (`QuestStart`) | `TerritoryStateCondition`: territory `Territory.HavenReach.MarketSquare.Blacksmith`, `required_state = LOCKED`, `not_ = True`, query `POLITICAL_STATE` — "start when the Blacksmith is not locked". Branch `QuestBranch_95`, task `BPT_TerritoryState_C` |
| `QuestState_1` | Description **"Assign atleast 1 guard"**. Branch `QuestBranch_50` carries a `TerritoryScheduleEnemyWaveEvent`, task `BPT_TerritoryState_C` |
| `QuestState_2` | Two conditions: `NC_IsQuestAtState` (`QuestState_7`) **and** `TerritoryOwnershipCondition` — territory `Territory.HavenReach.CastleHill.Farm`, `required_owner = Narrative.Factions.Heroes` |
| `QuestState_7` | `NE_BeginDialogue` event → dialogue `DBP_Hahsir_C` (`/Game/HOPTRENDY/Character/Hashir/DBP_Hahsir`), `refire_on_load = True`, runtime `END`. Branch `QuestBranch_369`, task `BPT_TerritoryCapture_C` |
| `QuestState_9` | Branch `QuestBranch_20` carries a `TerritorySetDiplomacyEvent`, task `BPT_TerritoryCapture_C` |
| `QuestState_10` | Branch `QuestBranch_309`, task `BPT_PlayDialogueNode_C` |
| `QuestState_13` | Branch `QuestBranch_135`, task `BPT_FollowNPCToLocation_C` |

The task classes are Territory's own: `BPT_TerritoryState`, `BPT_TerritoryCapture`, plus NarrativePro's
`BPT_PlayDialogueNode` and `BPT_FollowNPCToLocation`. **All task paths resolve at the plugin mount**
(`/TerritoryFramework/Tales/Tasks/…`) and are **absent at `/Game/TerritoryFramework/Tales/Tasks/…`** —
the reverse of the asset-mount drift recorded in Part 15, and worth noting because a future re-mount of
the plugin's Content would break every task in this quest.

The quest is bound to the Place by a soft class path inside a struct, not a hard reference:
`DA_Place_Blacksmith.quest_runtime_overrides` = one rule with `quest_class = NQ_CaptureBlacksmith_C`,
`active_quest_state = InProgress`, `include_child_territories = False`, `pause_state_rules = True`,
`pause_automatic_capture = False`, `pause_automatic_counterattacks = True`. Note what this rule *is*: it
pauses the territory's own rules **while the quest is in progress**. It reads the quest's state; it does
not start the quest. What starts it is `DBP_Hahsir` — the only asset in the project that references the
quest (see *Traps* for how confident that can be).

**Metadata is left at template defaults**, which is what made the first read look like an untouched
template:

| Field | Value |
|---|---|
| `quest_name` | `My New Quest` |
| `quest_description` | `Enter a description for your quest here.` |
| `QuestState_0.description` | `This is the start of my Quest.` |
| `QuestState_0.on_entered_func_name` | `OnActivated/Deactivated-QuestStart` |
| Blueprint parent | `QBP_DemoQuestBase_C` |

And one structural anomaly: **`QuestBranch_135.conditions` is `[None]`** — a literal null entry in a
condition array, reproducible across reads. Its sibling branches have `[]`. Whatever Narrative Pro does
with a null condition, an empty slot in an authored array is not intended.

### The Blacksmith handover dialogue link is crossed

The handover chain fires correctly up to the NPC and then plays a dialogue with no content:

```
guards defeated
  → TerritoryOwnerHandoverEvent_0        (in all_defenders_defeated_events; owner_territory_tag =
                                          Territory.HavenReach.MarketSquare.Blacksmith ✓;
                                          begin_dialogue_immediately = True; runtime START; refire_on_load False)
  → ActivateHandover()                   (TerritoryStoryOwnerSpawner.cpp:89)
  → spawns OwnerSpawn->NPCToSpawn        = NPC_BlacksmithOwner
  → NPC_BlacksmithOwner.Dialogue         = DBP_BlacksmithRetakeHandover   ← no Dialogue Graph
```

`DBP_BlacksmithRetakeHandover` — and `DBP_BlacksmithRetakePlanning` — contain **only** a zero-node
`EventGraph`. They have **no Dialogue Graph at all**, so they cannot play a line. Meanwhile the dialogue
that *is* authored, `DBP_BlacksmithHandover`, has **7 dialogue nodes** (Root, `NPC_0`, `Player_0`,
`NPC_1`, `NPC_2`, `Player_1`, `NPC_3` — three NPC lines and two player choices) and an 11-node
`GetStringVariable` helper. **No asset in the project was found to reference it.**

So the story-capture climax spawns the owner and opens an empty conversation. The **Farm is correct** in
the same chain: `NPC_FarmOwner.Dialogue = DBP_FarmHandover`, which has 4 authored dialogue nodes.

The retake layer is in the same half-built state: `DA_Recipe_BlacksmithRetakePlanning` and
`DA_Recipe_BlacksmithRetakeHandover` contain **genuinely authored text** (e.g. *"Your faction has retaken
the Blacksmith. Control is restored…"*), and `DA_Situation_Blacksmith` carries the correct territory tag
— but the recipes were never baked into playable graphs, and the two retake Blueprints that do reference
the situation profile have no graph to play.

**The existing test suite is green on this path.** The assertions I read at
`TerritoryNarrativeProMigrationTests.cpp:861-887` check that the handover event's outer is the live
Blacksmith actor and that its `OwnerTerritoryTag` fallback is stable. They do not check what dialogue the
spawned NPC will play — which is exactly where the break is. This is the same lesson as
[[test-after-every-step]]: green means the assertions that exist pass, nothing more.

### The mechanism — an empty field, and the likely one-field fix

The spawner has a *designed* slot for exactly this dialogue, and it is empty. `BeginOwnerDialogue`
(`TerritoryStoryOwnerSpawner.cpp:223-227`):

```cpp
TSubclassOf<UDialogue> DialogueClass = OverrideDialogue;          // from StoryOwner.DialogueOverride
if (!DialogueClass && IsValid(OwnerSpawn->NPCToSpawn))
{
    DialogueClass = OwnerSpawn->NPCToSpawn->Dialogue.LoadSynchronous();   // fallback
}
```

`OverrideDialogue` is populated from the Place template by `ApplyPlaceDefinition` (`:36`):
`OverrideDialogue = Template.DialogueOverride.LoadSynchronous();`.

So the resolution order is **story-owner override → NPC's own dialogue**, and the measured values are:

| | `story_owner.dialogue_override` | falls back to NPC dialogue | result |
|---|---|---|---|
| Blacksmith | **`None`** | `DBP_BlacksmithRetakeHandover` — no Dialogue Graph | nothing plays |
| Farm | `None` | `DBP_FarmHandover` — 4 nodes | works |

This explains the "no referencer" observation rather than leaving it as a mystery: `DBP_BlacksmithHandover`
is not orphaned by accident — it is **waiting in the slot that was never filled**. The Farm works despite
having the same empty override because its NPC's fallback dialogue is authored; the Blacksmith is the one
case where the fallback is empty and the override was therefore required.

**The likely fix is one field, not a rewiring:** set
`DA_Place_Blacksmith.story_owner.dialogue_override = DBP_BlacksmithHandover`. That plays the authored
7-node handover at the handover moment while leaving `NPC_BlacksmithOwner.Dialogue` as the retake
handover for subsequent conversations — which reads as the design intent, given the two dialogues' names
and that the override exists at all. Both assets stay in use; nothing is replaced.

Not applied. It is an authored-data change on a Place you are actively working in, so it is your call,
and it is worth deciding together with the retake question below.

### Coverage by territory

| Territory | Quest override | Story owner | Owner's dialogue | Verdict |
|---|---|---|---|---|
| **Blacksmith** | `NQ_CaptureBlacksmith` | enabled, `story_capture_from_bounds` | `DBP_BlacksmithRetakeHandover` — **empty** | Wired end-to-end, broken at the dialogue |
| **Farm** | none | enabled | `DBP_FarmHandover` (4 nodes) | Correct |
| MarketSquare district | none | n/a — `StoryOwner` is Place-only | — | No narrative content |
| CastleHill district | none | n/a | — | No narrative content |
| HavenReach city | none | n/a | — | No narrative content |

The Farm having no quest override is **not** an omission: its unlock is driven by the Blacksmith, via a
`UTerritoryLockEvent` on the Blacksmith and a `UTerritoryOwnershipCondition` on the Farm's Locked
`ExitConditions` (`TerritoryToCheck = Territory.HavenReach.MarketSquare.Blacksmith`). That chain is
deliberate and is asserted by the migration tests.

### The Farm under Auto Capture cannot spawn its owner

**Answer: no.** With `initial_guard_count = 0` the Farm never has a defender, so nothing can ever trigger
the handover path — and the Farm has no handover event anyway. Four independent facts, each sufficient:

1. **The spawner has exactly one activation entry point.** `ActivateHandover`
   (`TerritoryStoryOwnerSpawner.cpp:89`) is the only method that spawns the owner. A grep over the whole
   non-test Source tree finds **one caller**: `UTerritoryOwnerHandoverEvent::Execute`
   (`TerritoryOwnerHandoverEvent.cpp:55`). `BeginPlay` (`:47`) only re-completes a handover that was
   *already* activated and saved (`if (HasAuthority() && bHandoverActivated)`, `:61`).
2. **The Farm has no handover event.** `DA_Place_Farm.all_defenders_defeated_events = []` and
   `defender_died_events = []`. Nothing calls `ActivateHandover` for it.
3. **The Farm cannot self-activate.** The constructor forces `bActivateOnBeginPlay = false`
   (`TerritoryStoryOwnerSpawner.cpp:17`), and the level actor that serves it — `BP_StoryOwnerSpawner_C_0`,
   `NPCToSpawn = NPC_FarmOwner` — reads `bActivateOnBeginPlay = False` in the live world. The other
   spawner, `BP_StoryOwnerSpawner_C_1` (`NPCToSpawn = NPC_BlacksmithOwner`), is also `False`. And
   `BP_StoryOwnerSpawner`'s EventGraph is **three disabled, unconnected event stubs** (BeginPlay,
   ActorBeginOverlap, Tick, each with no exec connections), so the Blueprint adds no activation path.
4. **Even a handover event could not fire.** `GetAllDefendersDefeatedEvents()` has exactly one consumer in
   non-test source — the loop at `TerritoryVolume.cpp:2561` — which is reached only from
   `TryCompleteDefenderDefeat` (`:2543`). That function is called at the end of `OnDefenderDied` (`:2385`,
   call at `:2540`) and returns early unless the registered-defender count is zero (`:2550`). It is a
   **death-driven** check, never evaluated at BeginPlay. With zero guards and no `RegisterDefender` calls,
   no death ever occurs.

The Farm's relevant authored state, for the record: `initial_state = AUTOMATIC`,
`initial_availability = LOCKED`, `initial_guard_count = 0`, `guard_posts = []`,
`story_capture_from_bounds = False`, `control_mode = INDEPENDENT`, `capture_point.enabled = True`,
`management_point.enabled = True`, `story_owner.enabled = True` →
`NPC_FarmOwner` with `dialogue_override = None`, `interaction_distance = 300`.

So the Farm is authored as an **Auto Capture** property whose owner NPC exists, is correctly bound to a
real authored dialogue, and **can never appear**. Auto Capture captures the territory without a fight;
the owner is the reward for winning that fight. The two halves are mutually exclusive as configured.

This also gives the Farm's `story_owner.bEnabled = True` a different reading than the Blacksmith's: on the
Blacksmith the flag gates a reachable path, on the Farm it gates nothing at all.

### Traps — two of which produced false conclusions in this session

1. **The asset reference index does not resolve soft class paths embedded in structs.** It reports
   `referenced_by: []` and `depends_on: []` for `NQ_CaptureBlacksmith`, whose only real binding is the
   soft class path inside `DA_Place_Blacksmith.quest_runtime_overrides`. `referenced_by: []` is therefore
   **not** evidence of orphanhood on this project.
2. **`find_package_referencers_for_asset` produces false negatives.** It reported `DBP_FarmHandover` as
   having **zero referencers** while `NPC_FarmOwner.Dialogue` demonstrably points at it — a direct
   property read of a loaded asset. An interim conclusion of this session ("`DBP_BlacksmithHandover` is
   orphaned") was built on this API and is **retracted as stated**; the finding above rests instead on a
   property scan of 127 assets under `/Game/TerritoryFramework` and `/Game/HOPTRENDY/Character`, which
   found no mention of `DBP_BlacksmithHandover`. That scan's own blind spot is a reference held inside a
   Blueprint graph node's task payload — so the honest form of the claim is "no referencer was found",
   not "provably orphaned". The **material** half of the finding does not depend on it:
   `NPC_BlacksmithOwner.Dialogue` is a plain property read and it points at the empty dialogue.
3. **`FGameplayTag` prints as `{}`.** Every tag in this part (`owner_territory_tag`, the condition tags)
   is valid; only the Python struct repr is empty. Read `get_editor_property('tag_name')`. Two of the
   tags here were read both ways to confirm it: `Territory.HavenReach.MarketSquare.Blacksmith` on the
   handover event, and `Territory.HavenReach.CastleHill.Farm` + `Narrative.Factions.Heroes` on the
   quest's ownership condition.

The same API also reported `DBP_Hahsir` as a referencer of `NQ_CaptureBlacksmith`. Given trap 2, that
claim is recorded as **unverified**: `blueprint_query search_nodes` for `Quest` across that Blueprint
returns zero matches, and I did not enumerate its 17-node EventGraph task payloads. The
quest → `DBP_Hahsir` direction (the `NE_BeginDialogue` event, read as a property) is solid; the reverse
direction is not.

### Verdict and limits

- **The quest is real.** A 7-state, 13-node Narrative Quest using Territory's own quest tasks,
  conditions and events, bound to the Place through `quest_runtime_overrides`. It is not demo scaffolding
  with a capture condition bolted on, and the conditions read the correct Blacksmith and Farm tags.
- **The Blacksmith's dialogue is broken and the Farm's is fine** — and the defect is **one empty field,
  not missing authoring**. `story_owner.dialogue_override` is `None` on the Blacksmith, so the spawner
  falls back to the NPC's own graphless dialogue; the authored 7-node handover dialogue sits in the
  override slot that was never filled. The empty retake Blueprints have authored recipes behind them that
  were never baked into playable graphs.
- **The Farm cannot spawn its owner** as configured.
- **One territory, not the whole map.** The city and both districts carry no quest override, no story
  owner (the field is Place-only) and no defender events. "Whole territory capture as Narrative Quest and
  Dialogues" overstates it: the quest covers the Blacksmith chain, with the Farm reached through it.

Limits, stated plainly:

- **`[None]` in `QuestBranch_135.conditions` is recorded, not diagnosed.** I did not read Narrative Pro's
  branch-condition evaluation to establish whether a null entry is ignored, skipped, or a crash risk.
  Vendor source is out of scope to change; the observation is yours to act on.
- **The dialogue break is confirmed at the property level, not observed in play.** `NPC_BlacksmithOwner`
  pointing at a graphless Blueprint is decisive on its own, but nobody has watched the owner appear and
  say nothing. If you want that on the record, it is a PIE check against a defeated Blacksmith garrison.
- **The referencer API's failure mode was only ever observed as a false negative**, so the
  `DBP_Hahsir → quest` link is more likely true than false. It is marked unverified because I did not
  confirm it, not because I doubt it.
- **Nothing here is a code defect in Territory Framework.** The two findings are authored-data defects:
  a wrong dialogue assignment and an Auto Capture territory whose owner cannot be reached. The plugin
  behaves exactly as written in both cases.

### Suite count

Unchanged at **354 tests, 354 pass, 0 fail** — this part adds no code, only evidence. Note what that
number does *not* cover: the migration tests assert the handover event's binding and its fallback tag,
and pass, while the dialogue the spawned NPC will actually play is empty. The suite has no assertion on
the NPC's dialogue target. That is the gap the count hides.
