# Hierarchy, Availability, and Unlocks

## Mental model

Territory uses two independent questions:

1. **Is it available?** `Locked` or `Unlocked`.
2. **Who controls it?** `Unclaimed`, `Contested`, or `Claimed`, plus the owning faction.

This separation preserves story truth. A Bandit-owned farm may be locked because its rescue
quest has not started. Unlocking it reveals the farm; it does not give the farm to Heroes.

## Authority by layer

```text
City (aggregate authority and settings)
└── District (aggregate rights, perks, and settings)
    └── Place (physical gameplay)
        ├── bounds / capture
        ├── guards / posts / patrol
        ├── story owner / dialogue handover
        ├── production / income
        └── counterattack approaches and target
```

City and District control is a read model reduced from the complete Definition hierarchy:

- all expected children loaded, unlocked, claimed by one faction -> `Claimed` by that faction;
- any mixed, partial, locked, or contested political control -> `Contested`;
- no political child control -> `Unclaimed`;
- an incomplete World Partition set can never claim a parent.

Only Place ownership is directly mutated. Capturing a parent never rewrites children.
Runtime parent tags and gameplay-tag prefixes cannot add a child. `GetDistricts()` and
`GetProperties()` expose only loaded actors present in the exact Definition arrays, and parent
lookup fails closed when those references disagree.

## Unlock scopes

| Scope | Result |
|---|---|
| Automatic Hierarchy + Place | Unlock ancestors in root-to-leaf order, then the Place; do not unlock siblings |
| Automatic Hierarchy + District/City | Unlock the target, then authored descendants |
| Exact Only | Unlock only the exact target |
| Force Exact | Exact target, bypassing its lock conditions |
| Force Hierarchy | Hierarchy cascade, bypassing every local lock condition |

Normal cascades return a result row for every attempted tag: unlocked, already unlocked,
blocked by a local condition, missing from the loaded world, or invalid. This makes quest logs
and debugging explicit instead of guessing that a World Partition actor succeeded.

Easy example: `Unlock Castle Hill` opens the District and attempts Farm and Keep. Farm opens.
Keep remains locked because `Defeat the Warden` is incomplete. The result reports Keep as
blocked, and the District remains strategically insecure until its authored Places are secure.

## Locked behavior

A locked territory preserves political ownership and save identity but remains silent for
physical gameplay:

- hidden from capture/command lists and POI tracking;
- no capture, defender, guard, or counterattack target activity;
- no income, upkeep, production, or perks;
- no secure-District staging power;
- no retroactive production payout after unlock.

Availability is effective through the complete ancestor path. An unlocked Blacksmith below a
locked Market Square District is still unavailable; its own local flag is not overwritten.
Capture, production, income, garrison commands, counterattacks, POI tracking, and Tales capture
queries all use the effective rule. When an ancestor is streamed out, its exact WorldState
summary supplies availability and parent identity; missing or cyclic data fails closed.

Guard posts are Place-only. Definition binding wins, followed by an exact Place tag, then
placement/patrol overlap with a Place. Large City/District bounds are deliberately ignored.

The Locked State Config's entry/exit conditions and events remain the Narrative authoring hook.
Use the Territory Lock/Unlock events for runtime changes. `ETerritoryState::Locked` exists only
to migrate older serialized assets and is rejected by the atomic ownership API.

## Designer checklist

1. Put physical settings only on Place Definitions.
2. Build hierarchy only through City `Districts` and District `Places` arrays.
3. Use stable complete gameplay tags for Tales events.
4. Put each local story requirement in that actor's Locked exit conditions.
5. Use `Automatic Hierarchy` for ordinary story unlocks.
6. Reserve force scopes for explicit cinematics, admin tools, or migration.
7. Run Data Validation after synchronization; parent physical values are errors.
