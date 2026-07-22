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

Bridge: SetNarrativeAttitude() pushes treaty-derived attitudes to Narrative
        OnFactionAttitudeChanged() reconciles treaties when Narrative changes
```

## Treaty Types

| Type | Narrative Attitude | Can Capture? | Notes |
|---|---|---|---|
| Alliance | Friendly | No | Strongest bond |
| TradeAgreement | Friendly | No | Economic cooperation |
| NonAggression | Friendly | No | Peace pact |
| Ceasefire | Neutral | Yes | Temporary peace after war |
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
