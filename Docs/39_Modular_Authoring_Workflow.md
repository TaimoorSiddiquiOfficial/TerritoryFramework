# Territory authoring: ownership and integration

This guide describes the September 27 refactor. Consult its verification report for
the exact build and gameplay checks performed; an available class is not a release certificate.

## Keep each setting at its owning layer

| Concern | Edit here | Derived or contextual state |
| --- | --- | --- |
| Physical floor regions | Placed Territory Floor Volume box | Containment in a loaded Place |
| Floor identity, quotas, events, combat policy | Place Definition floor rows | Guard resolution uses the physical volumes |
| A post's floor assignment | Place Definition Guard Posts row, via Detect & Sync Floors | Spawn Point displays the resolved assignment |
| Reusable reserve policy | Guard Post Definition | Six ignored row defaults are hidden while a profile is assigned |
| NPC equipment | Native NPC Definition loadout/Item Collection | Activity eligibility reads the current Native inventory |
| Combat target permission | Existing Territory guard/assault policy | Narrative owns perception, activity selection and attack execution |
| Guard conversations | Conversation Profile + Meeting Point + Native dialogue | Temporary goals and exact actors; no campaign conversation registry |
| Relationship dialogue | Place Guard Behavior dialogue profile | Resolved from the current Definition on server and client |
| Music | Definition state Audio row | Narrative Music owns soundtrack playback |

An override is not automatically duplication. A reusable post profile and a specific
Place row serve different authoring scopes. The Editor should expose the effective
source and hide settings that cannot affect the result.

## Floors

1. Place **Territory Floor Volume** through Place Actors > Territory Framework.
2. Resize its Floor Bounds and select the stable floor ID. Leave its Place binding
   empty only when its complete transformed box is inside one loaded Place.
3. Load the relevant World Partition cells. Select the Place, its Definition or a
   valid bound post and use **Detect & Sync Floors**. An unbound floor volume offers
   **Detect & Sync Loaded Floors**, which scans all loaded Places.
4. Review warnings, then save the changed authoring assets/map.

Detection binds unassigned physical regions, adds missing policy rows and updates
canonical post assignments. It preserves floor names, IDs, existing policy and events.
Ambiguous automatic containment, posts inside different floor IDs and impossible
quotas fail before mutation. Explicit owner binding bypasses automatic containment.
Overlapping geometry away from posts is not globally rejected. Canonical post rows
without loaded actors are reported as partial coverage; unloaded floor boxes and
Places cannot be discovered. The operation does not guess missing
post locations or generate new saved floor identities at runtime.

City synchronization uses the same implementation in preservation mode; explicit
Detect & Sync permits a geometric reassignment. Existing serialized row values survive
profile changes. Undo/Redo is an Editor transaction, not a runtime campaign operation.

## Ranged combat, close melee and cover

The existing default bandit is sword-only. **NPC_TerritoryRangedGuard** is an opt-in
example on both content mounts: it retains the sword and uses Native's
**IC_PistolWithAmmo** collection. Assign that definition to the intended Place post;
do not expect a ranged activity to manufacture a firearm or ammunition.

Territory melee activities inherit **BPA_Attack_Melee**. Their wrapper retains the
Territory permission and inventory checks and adds close-range preference: enter
within 300 cm, retain the current melee activity until 450 cm. Negative, zero and
Native's 0.2 unreachable fallback scores stay unchanged. Native owns weapon switching,
ability execution and tactical movement. These thresholds are exposed on the helper
call for a custom activity, rather than copied into every NPC definition.

Crouching is posture. Native cover requires actual BP_Cover markers, navigable
approaches, suitable geometry and the cover activity. HopDistrictTest had no markers
at audit time. Do not enable the cover activity globally and call crouching alone
proof of functional cover. Preserve the current Native ranged distance/EQS behavior
until cover positions are authored and claim/release/interruption are verified.

## Reserve response

A post records a recent real damage event on its tracked guard. A successfully
admitted replacement receives a finite investigation of the victim's hit location.
It does not inherit sight, an attack goal, personal Hostiles, faction War, or the
attacker's current location. Neutral personal aggression can cause investigation;
treaties, quest/faction/floor restrictions and the response opt-out still apply.

Reports expire, are transient, and clear on owner/load/rebind/removal boundaries.
Zero investigation policy suppresses the response. Real Narrative perception is
still required to acquire an attack target. Existing reserve accounting is unchanged.

## Two-guard conversations

The supported first slice stages two ready defenders in one explicit Place for one
explicit personal Tales listener. Assault recruitment, parties, simultaneous audiences
and joining a line in progress are unsupported and must not be presented as complete.

1. Use **Tools > Territory Framework > Create Definition or Profile > Territory
   Conditional Dialogue Recipe** and choose **TerritoryAmbientDialogue**
   as Parent Dialogue Class. Use two distinct role IDs, normally FirstGuard/SecondGuard.
2. Author an acyclic NPC-only graph with both roles speaking. Conditions and events
   remain ordinary Native Narrative objects. Do not add player choices, camera shots
   or unbounded lines. Ordinary recipes still use NPC Definitions and UDialogue.
3. Call **Create Dialogue From Recipe** from an Editor Utility Blueprint, or Python's
   `unreal.TerritoryDialogueEditorLibrary.create_dialogue_from_recipe`. Supply a new
   asset path, check the returned error and save the result explicitly. This is an
   Editor library function, not a button in the recipe's Details panel. Assign its
   generated class to **Territory Conversation Profile** from the same creation menu.
4. Place a **Conversation Meeting Point**, bind its Place/Profile, and put both offsets
   on reachable ground. Keep the arrival regions separate.
5. Add **Territory Conversation Listener Component** to the player's Narrative
   Player Controller Blueprint. Supply that controller's personal Tales to
   **Try Start Conversation** on authority from an authored arrival/story trigger.

The returned success means staging was admitted, not that playback finished. Inspect
Phase and Last Stop Reason. The goal scores below investigation/combat and above patrol.
Damage, death, interruption, loss of listener/scope/readiness, loading and bounded
timeouts release only the meeting's own goals. Native resumes its remaining duties.

Two instances of the same NPC Definition remain distinct roles. The owning client
acknowledges the exact actors before Native dialogue begins. Other players do not
receive a second story execution. Do not award an overheard objective merely because
staging started; author a deliberate Native completion condition/event.

The stock Native subtitle overlay names the current speaker from the exact NPC actor,
falling back to the role ID for non-NPC actors. Two guards with the same authored NPC
name can therefore share a display label while remaining distinct meeting roles.
Use the **Enabled** subtitle setting when checking ambient text: Native's **Dialogue
Only** setting deliberately suppresses free-movement dialogue, including these meetings.
The Native dialogue menu retains its normal skip/exit bindings and free-movement input
configuration; this does not imply that the CommonUI input configuration is unchanged.

The ambient subclass owns no player camera, input lock, transform or hidden state.
During its Native end cleanup, its cached OwningPawn is temporarily unavailable to
prevent Native from unhiding a listener owned by another scene. End callbacks must
use **Get Listening Pawn** or the supplied Tales component's **Get Owning Pawn**.
Do not write camera/input/visibility policy through the ambient dialogue's cached fields.

Blacksmith's empty interaction spots remain disabled by the user's choice. A two-NPC
meeting is separate from a one-NPC Narrative bench/work-slot claim.

The project example is under **/Game/TerritoryFramework/Examples/GuardConversation**.
**DR_GuardNews** generates **DBP_GuardNews**, with a Blacksmith situation condition
and a fallback news exchange. **CP_GuardNews** configures the meeting. Place
**BP_GuardConversationMeeting**, bind its Place and check both arrival offsets on
navigation. Its overlap trigger supplies only the entering player's controller/Tales.
The demo-map player controller now includes the listener component. This is an opt-in
meeting example; no meeting was inserted into an existing patrol route automatically.

A meeting point is a coordination trigger, never geometry. Collision profile
**OverlapAllDynamic** only rewrites the eight built-in channels in BaseEngine.ini, so
every custom game trace channel keeps the engine default of Block. Narrative traces
weapons on **GameTraceChannel2** and projectiles on **GameTraceChannel6**, and a line
trace returns the first blocking hit, so a player standing inside the trigger became
the first hit for melee sweeps, firearm traces and projectile lines: every attack
registered on the trigger instead of the enemy behind it. **ApplyNonCombatCollision**
runs in BeginPlay and clears those trace responses on every primitive component while
keeping pawn overlap intact, because that overlap is the only entry point into
**TryStartConversation**. Authored Blueprints that add a primitive component at runtime
must call it again. Give any other trigger volume the same treatment rather than
relying on a profile name.

The old unused patrol **ActivityTag** still serializes for compatibility and remains
readable in Blueprint, but is no longer an editable Details setting. It starts no
activity. Use the explicit meeting/profile workflow for the supported conversation.

## Diplomacy dialogue and cutscene conditions

Dialogue selection reads the current Place Definition and exact owner faction, including
after late owner replication. A cleared profile or empty slot uses Native fallback.
Several current profile slots point to the same greeting; resolving the right slot does
not make identical authored content visibly different. Author relationship-specific
dialogues where different text is intended.

Use **Is In Cutscene** with the exact Player Controller. Optionally restrict it to one
Level Sequence and choose whether paused playback counts. It checks actual Narrative
movie playback for that player's audience; ordinary ambient sequences are excluded.
**Territory Cutscene Condition** provides the same boolean check for Narrative conditions.
Missing player context is rejected even when the expected boolean is false.

## Music and Editor discovery

Place rules take precedence over District, then City. Same-tier overlaps use Music
Priority and a deterministic tie break. State cues share one Territory-owned channel;
Native music/voice are separate. Leaving the final applicable rule restores the prior
set/theme, falling back to the world's default set and Music.Ambient.

Use **Override Narrative Music** for a sustained state soundtrack and put the theme's
tracks in its Native Music Set. State Entered/Exited Sound fields are short cues;
putting a full song there plays it alongside Native's soundtrack. Blacksmith's Claimed
and Contested rows now enable the Native override, use DA_BlacksmithMusic with their
matching Music.Territory.State themes, and have no full-song entry cue. The world's
default set is also DA_BlacksmithMusic; leaving restores its Music.Ambient playlist.

Native can queue themes without changing Get Active Theme and has no public queue
cancellation API. A rapid exit may let the queued Territory theme become audible briefly
before restoration. Story scenes should use the documented automatic-music handoff;
do not add a competing AmbientSound music player to compensate.

**Tools > Territory Framework** discovers the same factory catalog as the Content
Browser, including floor policy, situation, recipe and conversation profiles. Floor
volumes and meeting points also have Blueprint factories and Place Actors entries.
Their Blueprint factories are under **Create Actor Blueprint** in the Tools submenu.
No separate project Developer Menu implementation was found during the source search.
