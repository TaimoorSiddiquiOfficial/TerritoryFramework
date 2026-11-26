# Native damage cue readiness

NativeRun44 (G12) completed capture, finite assault/casualty, save, cold restart,
physical World Partition unload/reload and late-join recapture. It failed the log
gate: Client3 received GC_TakeDamage_Heavy while BP_TerritoryAssualtGuard1 had no
CharacterVisual. Eight Blueprint accessed-none errors are retained in the evidence.
It is not counted as a clean run.

The installed Native GC_TakeDamage OnBurst graph directly reads CharacterVisual,
MainMesh and AnimInstance. GameplayCueManager calls the public
IGameplayCueInterface::ShouldAcceptGameplayCue before dispatching the global cue.
Territory guard and assault classes implement that extension point and share the
existing TerritoryNarrativeProAdapter. Only Executed GameplayCue.TakeDamage (and
child tags) requires a completed local visual, registered main mesh, skeletal asset
and animation instance. A burst that cannot render is discarded; no delayed hit
reaction is replayed. Other cue families and OnActive/WhileActive/Removed are
unmodified. Custom TakeDamage child bursts therefore inherit this presentation
readiness rule even if a project's custom burst itself could render without a mesh.

Narrative remains damage, health, death, ability and appearance-loading authority.
No Native source or asset is changed. No new save field, replicated state, timer,
queue or Blueprint node is required. Existing NPC Blueprint children inherit the
interface after recompilation; no identity or campaign migration is involved.

The expanded WaitForNativeBaseAppearance regression exercises both Territory NPC
classes through GameplayCueManager's AActor interface dispatch: absent/partial
visual, missing mesh, real animation readiness, child tags, client readiness,
unrelated cues and persistent cleanup. Fresh multiplayer and full release gates
must pass on this revision before the repair can be declared verified.
