# Diplomacy System — Treaties, Wars, Reputation

## Architecture

```
Narrative GameState (BASE faction-to-faction attitude)
├── Friendly (Alliance, Trade, NonAggression map here)
├── Neutral (Ceasefire, None map here)
└── Hostile (War maps here)

TerritoryFramework (TREATY METADATA only)
├── ActiveTreaties[] — type, duration, expiry, permanence
├── FactionReputation — per-faction reputation score
└── DiplomacyHistory — event log

Bridge: UTerritoryNarrativeProAdapter is the sole TerritoryFramework access point
        to Narrative's public faction-attitude API. Server mutations write both
        directions in Narrative; clients hydrate treaty read models from WorldState.
        OnFactionAttitudeChanged() reconciles treaties when Narrative changes:
          - Compatible Friendly → preserves TradeAgreement/NonAggression metadata
          - Incompatible Friendly/Hostile → maps to Alliance/War
          - External Neutral → removes the treaty record, including Ceasefire
          - Externally authored attitudes are imported as permanent records
        Reentrancy guard (bSuppressSync) prevents recursive mutation from delegate listeners

Territory combat context (FINAL gate for Territory combat characters)
├── Territory guard: local Territory must be Contested AND treaty must be War
└── Assault guard: assigned assault must be Active AND treaty must be War
```

Changing the AI Controller does not change this faction relationship. Narrative asks the
character for its team attitude, so `ATerritoryGuardCharacter` and
`ATerritoryAssaultCharacter` apply the Territory context at the character layer. This keeps
the integration upgrade-safe and requires no edits to Narrative Pro Blueprints.

## Treaty Types

| Type | Narrative Attitude | Can Capture? | Notes |
|---|---|---|---|
| Alliance | Friendly | No | Strongest bond |
| TradeAgreement | Friendly | No | Economic cooperation |
| NonAggression | Friendly | No | Peace pact |
| Ceasefire | Neutral | Yes | Peace after war; permanent until explicitly changed or broken |
| War | Hostile | Yes | Full hostility |
| None | Neutral | Yes | Default state |

`None` means **Neutral / No Treaty**. It never means "use an old Hostile value." Calling
`SetDiplomacyState` with the same value still repairs Narrative's attitude map, so a Claimed
state policy can clear stale hostility left by an earlier War.

## Territory Combat Authorization

Ordinary Territory guards use both the active state and the treaty. This is intentionally
stricter than a general Narrative NPC:

| Territory State | Diplomacy | Guard Attacks on Sight? | Easy meaning |
|---|---|---:|---|
| Claimed | Neutral / No Treaty | No | The player may walk through the Place. |
| Claimed | War | No | War exists, but this Place is not currently a fight. |
| Contested | Neutral / No Treaty | No | A script cannot accidentally start a neutral-faction battle. |
| Contested | War | Yes | This is an authorized Territory battle. |

Counterattack characters use a related rule: **Active Assault + War = attack**. They do not
require the Place to be Contested before arrival because their valid arrival creates the
contest. Use `Can Engage Territory Target` and `Can Engage Assault Target` in Blueprint or MCP
debugging to see the final answer without reading AI perception state.

### Easy Blacksmith setup

For a Blacksmith initially owned by Bandits:

1. In the `Claimed` state config, add `Set Territory Diplomacy`:
   - Faction A Source = `Current Owning Faction`
   - Faction B Source = `Previous Owning Faction`
   - New State = `None`
   - Explicit fallbacks = `Bandits / Heroes`
2. Leave `Apply When State Starts Active` enabled. A fresh world applies only this safe
   diplomacy policy; the missing previous owner uses the Heroes fallback. It does not replay
   XP, quests, waves, or other state entry events.
3. In the `Contested` state config, use `Current Owning Faction / Contesting Faction / War`.
4. When the fight resolves, the next state's diplomacy event decides whether peace or War
   continues.

Now the same Place supports the complete story chain without reauthoring its row:

```text
Bandits own it; Heroes attack  -> Bandits / Heroes become War
Heroes capture it              -> Heroes / Bandits become None
Police later attack            -> Heroes / Police become War
Police capture it              -> Police / Heroes become None
```

Enable `Require Containing Territory Owner` to reject an old unrelated hardcoded pair. Keep
`Preserve Other Active Territory Wars` enabled on peace-like rows: capturing Blacksmith then
cannot cancel War while Farm is still contested by the same factions, including when Farm is
represented only by its World Partition capture summary.

This state-first authoring is modular: Narrative conditions may prevent either event, while the
same guard class remains reusable in every Place. `Requesting Faction` is also available for a
quest capture performed on behalf of a story faction.

### React to a faction's Claimed District count

Every Territory Narrative event inherits a `Conditions` array. Add `Territory Faction Claimed
District Count Condition` to a `Set Territory Diplomacy` event when expansion should change a
relationship:

```text
Faction Source = Narrative Target / Player Faction
Comparison = At Least
Claimed District Count = 2
```

The rule becomes true when that live Narrative faction owns two unlocked Districts in stable
`Claimed` state. The count is current political control, not lifetime capture history. Partial,
Contested, Locked, and Unclaimed Districts are excluded, and Definition-backed World Partition
rows remain visible through `TerritoryWorldState`. Use `Explicit Faction` for a fixed rule such as
“Bandits need at least one District before they can stage a strategic counterattack.”

## Diplomacy-aware NPC dialogue

Narrative NPCDefinition provides one default interaction dialogue. Territory adds an optional
`Territory Diplomacy Dialogue Profile` without editing Narrative Pro:

| Relationship | Easy dialogue tone |
|---|---|
| Same faction / Alliance / Trade | Friendly or respectful |
| None | Cautious, but not hateful |
| Ceasefire / Non-Aggression | Reserved |
| War | Hostile faction dialogue |

Assign the profile through the guard component's `Faction Dialogue Profiles` list. This is
important when one pawn Blueprint is shared by Bandit and Hero NPCDefinitions: map only Bandits
to the Bandit profile, and an unmapped Hero keeps the Hero definition's default dialogue. A
single-faction pawn Blueprint may use the component's fallback `Dialogue Profile` instead.
At interaction time,
the plugin resolves the specific player's exact Narrative faction tags and rich treaty, selects
the profile slot, and then delegates talk, loot, busy, ragdoll, and Tales behavior to Narrative's
`UNPCInteractable`. Empty profile slots fall back to NPCDefinition Dialogue. The choice is not
stored as one permanent value, so two multiplayer players with different factions do not overwrite
each other's conversation selection.

## API

### Actions (AuthorityOnly)

```cpp
Diplomacy->DeclareWar(FactionA, FactionB);      // Sets Hostile
Diplomacy->DeclarePeace(FactionA, FactionB);     // Sets Ceasefire
Diplomacy->FormAlliance(FactionA, FactionB);     // Sets Friendly
Diplomacy->BreakAlliance(FactionA, FactionB);    // Resets to Neutral
Diplomacy->SignTradeAgreement(A, B, Duration);   // Timed trade agreement
Diplomacy->SignNonAggression(A, B);              // Permanent Friendly treaty
Diplomacy->BreakCeasefire(A, B);                 // Return ceasefire to None/Neutral
```

### Queries

```cpp
EDiplomacyState State = Diplomacy->GetDiplomacyState(A, B);
bool bAtWar = Diplomacy->IsAtWar(A, B);
bool bAllied = Diplomacy->IsAllied(A, B);
bool bTrade = Diplomacy->HasTradeAgreement(A, B);
int32 Rep = Diplomacy->GetReputation(Faction);
```

### Reputation

```cpp
Diplomacy->AddReputation(Faction, 100);   // +100 reputation
Diplomacy->SetReputation(Faction, 500);   // Set to 500
int32 Rep = Diplomacy->GetReputation(Faction);
```

## Treaty Expiration

Timed treaties (e.g., trade agreements with `DurationGameTime > 0`) are checked every `TreatyExpirationCheckInterval` (default 10s). When expired:
1. `ExpiredTreaty` event recorded
2. `SetDiplomacyState(None)` called
3. Narrative attitude reset to Neutral

`DeclarePeace` creates a permanent Ceasefire; it is not processed by the expiration timer.

### Persistence

WorldState and SavableData restore treaty state, signed time, expiry, permanence, reputation, and available history directly before syncing Narrative attitudes. Narrative's attitude map alone cannot distinguish Alliance, TradeAgreement, and NonAggression because all three map to Friendly, so use the TerritoryFramework persistence actors when rich treaty identity matters.

The subsystem binds Narrative SaveSubsystem `OnFinishedLoad` and reapplies treaty-derived attitudes after each completed load, avoiding actor deserialization order races.

TerritoryFramework never edits Narrative Pro's faction database. Narrative faction tags remain identity and Narrative GameState remains combat-attitude authority; TerritoryFramework owns only the richer treaty/reputation metadata and its replicated read model.

## Delegates

| Delegate | Signature |
|---|---|
| OnDiplomacyStateChanged | (FactionA, FactionB, NewState) |
| OnDiplomacyEvent | (const FDiplomacyEvent&) |
| OnReputationChanged | (Faction, NewReputation) |

## Capture Rules

Before allowing capture, `AttemptCapture` checks:
1. Is territory Locked? → Blocked
2. Is attacker same faction as owner? → AlreadyOwned
3. Are factions Friendly? → DiplomaticallyBlocked
4. Are defenders alive? → DefendersRemain
5. All checks pass → Success (capture begins)

## Real Example: Diplomatic War Declaration

```cpp
// Player completes a quest that triggers war
void AMyQuestGiver::OnWarQuestCompleted()
{
    UTerritoryDiplomacySubsystem* Diplomacy = GetWorld()->GetSubsystem<UTerritoryDiplomacySubsystem>();
    
    FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Heroes"));
    FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(TEXT("Narrative.Factions.Bandits"));
    
    Diplomacy->DeclareWar(Heroes, Bandits);
    
    // Now all Heroes can capture Bandit territories
    // Bandit NPCs will be hostile to Heroes (via Narrative attitude)
}
```

## Real Example: Trade Agreement with Duration

```cpp
// Sign a 30-minute trade agreement
Diplomacy->SignTradeAgreement(Heroes, Merchants, 1800.0f);

// Both factions are now Friendly — can't capture each other
// After 30 minutes, treaty expires, attitudes reset to Neutral
```
