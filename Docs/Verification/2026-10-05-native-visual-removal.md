# Client visual removal and release-process race

## Confirmed failure and reproducer

G13 TDA18 completed all gameplay phases but failed its log gate during physical
streaming: m_tal_nrw_body had 342 bone-visibility entries and zero pose transforms.
G13 Native47/48/49 passed clean; they do not disprove the intermittent TDA failure.
The installed engine's public export addresses identify the relevant call chain:
ExcludeHiddenBones, ComputeRequiredBones, RecalcRequiredBones and animation tick.
Full PDBs were unavailable, so distant nearest-export matches are not exact symbols.

A private isolated-world reproducer confirms the same engine ensure with an Engine
skeletal cube: after forced removal of a simulated Native character, its visual
remains valid, its leader reference is stale, its pose count is zero and its bone
visibility count is two. Native Destroyed calls the visual's ordinary Destroy,
which the engine correctly refuses for a simulated network actor. The separate
visual actor channel can therefore outlive the character channel. The visibility
mismatch is a consequence of that lifetime gap, not malformed skeletal asset data.

The failed plain base-NPC fixture lacked Native's stable-GUID implementation and
asserted before reaching this boundary. The valid Territory guard fixture supplies
that required identity and reproduces the intended failure. Repeated physical WP
and lag probes that did not reproduce it are not presented as proof of a fix.

## Focused extension

The existing Territory module observes game-world actor destruction and routes it
to the existing Narrative adapter before Native Destroyed and component disposal.
For a retiring client Native character only, it finds that character's visual and
releases only skeletal follower links whose leader belongs to the ending character.
The public SetLeaderPoseComponent API allocates valid local pose/visibility buffers.
Unrelated live leaders remain linked. Native retains the visual actor's replicated
lifetime and may rebind its appearance normally. Server visual destruction is
unchanged. No vendor source/asset, new UObject class, tick, timer, save data, actor
identity or gameplay authority is added. World/module cleanup unbinds observers.

ClientVisualLeaderRemoval tests the actual world destruction hook with simulated,
autonomous and authority roles, separate visual lifetime, buffer validity, unrelated
leader preservation and normal rebind. RecalcRequiredBones must run without an ensure.
On UE 5.8, the same test fails on G13 with stale leader/zero local pose buffers for
both client roles, then passes after this repair (one completed test, zero failures,
no crash or timeout). Fresh full automation and composed runtime verification remain
required; their exact-source receipts accompany the final acceptance report.

The first full Game Development build exposed a missing explicit WeakObjectPtr
include in the module header, masked by the Editor's includes. The header now owns
that dependency. G14 Editor/TDA tests passed, but its failed Game build and interrupted
composed runs are historical evidence only; the final snapshot must pass all targets.

## Build preflight

G13's UE 5.7 admission saw an editor PID in a CIM snapshot after that editor exited.
The process/module query threw and prevented the build. The preflight now rechecks
that exact PID after an enumeration exception: a verified disappearance is harmless;
an unreadable live process still fails closed. Tests execute the actual preflight
for exited, unrelated, unreadable and loaded-host cases. The old script fails the
exit-race regression. Host-path protection and cold-build requirements remain.
