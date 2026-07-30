# Diplomacy System — Treaties, Wars, Reputation

## Architecture

```
Narrative GameState (SOLE AUTHORITY for AI attitudes)
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
```

## Treaty Types

| Type | Narrative Attitude | Can Capture? | Notes |
|---|---|---|---|
| Alliance | Friendly | No | Strongest bond |
| TradeAgreement | Friendly | No | Economic cooperation |
| NonAggression | Friendly | No | Peace pact |
| Ceasefire | Neutral | Yes | Peace after war; permanent until explicitly changed or broken |
| War | Hostile | Yes | Full hostility |
| None | Neutral | Yes | Default state |

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
