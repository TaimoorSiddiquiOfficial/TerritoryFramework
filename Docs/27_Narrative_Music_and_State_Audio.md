# Narrative Music and Territory State Audio

Territory Framework uses Narrative Pro's existing Music subsystem. It does not create a second
soundtrack player and it does not modify Narrative Pro source.

This integration answers two simple needs:

- change the Narrative music theme while the local player is inside a Territory;
- play a short local sound when that Territory enters or leaves a political state.

## Easy example

Blacksmith is calm while securely held. Bandits attack it, then Heroes win.

| Place state | Narrative music | Optional state sound |
|---|---|---|
| Unclaimed | `Music.Ambient` | quiet discovery cue |
| Contested | `Music.Combat` | alarm when the attack starts |
| Claimed | `Music.Ambient` or `Music.Territory.State.Claimed` | short victory cue |
| Locked availability | normally no override | optional lock or mystery cue |

`Music.Ambient` and `Music.Combat` are Narrative Pro themes. Territory also registers optional
`Music.Territory.State.Locked`, `Unclaimed`, `Contested`, and `Claimed` tags. A custom tag only
plays when the selected Narrative **Tagged Music Set** contains a track for that exact tag.

## Authoring in one Territory Definition

Open a City, District, or Place Territory Definition. Under **State Configs**, open the state row,
then open **Narrative Music And State Effects**.

1. Enable **Override Narrative Music**.
2. Set **Music Theme**.
3. Leave **Music Set Override** empty to use the current Narrative world/default music set, or
   assign a Tagged Music Set for a location with special music.
4. Leave **Immediate Theme Change** disabled for a smooth authored cross-fade. Enable it for a
   sudden ambush only.
5. Optionally assign **State Entered Sound** and **State Exited Sound**.
6. Keep the arrival/departure replay switches disabled for capture alarms and victory cues. Enable
   them only for sounds that should replay whenever the player walks in or out.

Example authoring:

```text
DA_Place_Blacksmith
└── State Configs
    ├── Unclaimed
    │   └── Audio: Override Music = true, Theme = Music.Ambient
    ├── Contested
    │   └── Audio: Override Music = true, Theme = Music.Combat,
    │              Entered Sound = S_BlacksmithAlarm
    └── Claimed
        └── Audio: Override Music = true, Theme = Music.Ambient,
                   Entered Sound = S_TerritorySecured
```

The state row remains the only authoring source. The live Territory actor receives a private
runtime copy when its Definition is applied.

## Place, District, and City priority

The most specific configured rule wins:

```text
Place rule
  ↓ if not configured
District rule
  ↓ if not configured
City rule
  ↓ if not configured
Current Narrative world music remains unchanged
```

This follows exact Definition parent links. It does not guess parents from tag names, and the
parent does not need a second overlapping physical capture volume.

Useful patterns:

- City: general regional identity.
- District: market, military, industrial, or residential mood.
- Place: unique forge, farm, fortress, boss arena, or story music.

## What triggers a sound

When the player is already inside the most-specific Territory:

- `Unclaimed -> Contested` plays the old state's Exit sound, then Contested's Enter sound;
- `Contested -> Claimed` plays Contested's Exit sound, then Claimed's Enter sound;
- first observation after loading is silent, avoiding a false capture fanfare;
- walking across a boundary is silent unless the row explicitly enables arrival or departure
  replay.

Availability follows the same availability-first rule as Territory UI. A Place that is locally
Locked, or effectively Locked by its District/City, selects its Locked audio row. This does not
restore the removed legacy Locked political state.

These one-shot sounds are local 2D presentation. Use normal Unreal Ambient Sound, Audio Volume,
MetaSound, attenuation, and reverb tools for spatial wind, machinery, rooms, and multi-floor sound.
Territory does not replace those systems.

## Narrative Music ownership

`UTerritoryMusicSubsystem` is a client-cosmetic GameInstance adapter. It:

- reads the local player's position;
- reads the replicated Territory state;
- selects the Definition state row;
- asks `UNarrativeMusicSubsystem` to select a Tagged Music Set and theme;
- restores the earlier Narrative set/theme after the player leaves;
- does not continuously overwrite a later quest or cinematic theme change.

There is one Narrative soundtrack per GameInstance. In split-screen, the first valid local player
is the soundtrack listener. State effects and music choices are not saved or replicated because
the authoritative Territory state is already saved and replicated.

## Validation and common mistakes

The Territory Definition validator reports an error when:

- Override Narrative Music is enabled but Music Theme is empty;
- the selected theme is not under the `Music` tag root;
- an assigned Tagged Music Set cannot load or has no row for the selected theme;
- state-effect volume or pitch is outside the supported range.

It warns when arrival/departure replay is enabled without its matching sound.

If music does not change:

1. confirm the state row is the live state, not only the new-campaign Initial State;
2. confirm **Override Narrative Music** is enabled;
3. confirm the active/default Tagged Music Set contains the exact theme;
4. if using a custom Music Set Override, confirm that asset contains the same theme;
5. remember that a Locked Territory uses the Locked audio row through effective Availability,
   while Unclaimed, Contested, and Claimed follow replicated political state.

## Multiplayer and dedicated servers

- Territory state changes remain server-authoritative.
- Music and state cues run only for local clients.
- Dedicated servers do not create the audio subsystem.
- A player hears the rule at their own location; another player elsewhere may hear a different
  state/theme.

This keeps music presentation separate from ownership, capture, diplomacy, save, and combat rules.
