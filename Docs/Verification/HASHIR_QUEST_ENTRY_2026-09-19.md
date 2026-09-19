# Hashir quest entry regression

Checked in TDA on 19 September 2026, UE 5.8.

The project dialogue's trip line contained a second Native Begin Quest event
for the Blacksmith quest. The project asset now retains quest startup only on
the original offer. Driving and Farm unlock events remain on the trip line.

The plugin change in this batch is a project integration test, not a runtime
quest system. `TerritoryFramework.ProjectStory.HashirTripDoesNotStartOrRestartQuest`
loads the actual compiled dialogue through
`UDialogueBlueprintGeneratedClass::InitializeDialogue`, invokes its node events
through `UNarrativeNodeBase::ProcessEvents`, and checks `UTalesComponent` state.
Native's Begin Quest event loads the class asynchronously, so the test processes
its actual latent action before making assertions. Cleanup uses `ForgetQuest`.

The test verifies that the trip does not create a quest, that the original
offer does create one, and that a later trip keeps the same quest instance and
state with no duplicate quest. If the TDA dialogue package is absent, it reports
that this project-only check was skipped; that does not verify project content.

The UE 5.8 full editor build passed. The final focused test passed with zero
errors and two warnings: Narrative's server dialogue diagnostic and the lack of
a player speaker avatar in the minimal test world. The fixture does not test
camera presentation. See [the saved test result](HASHIR_QUEST_ENTRY_2026-09-19.json).

Earlier attempts are not counted as passes: the cleanup first used a protected
method, Live Coding did not register the new test, and the first executable test
checked the asynchronous event before its callback. The final build and test
correct these fixture issues without changing Narrative Pro.

Authority remains with the server's Tales component and the existing Territory
unlock event. This change adds no save fields, replicated state, GUIDs or public
Blueprint API. Existing quest progress is retained; the removed event affects
future visits to the authored trip line. Campaign save/reload, multiplayer story
interaction, World Partition and packaged verification remain separate gates.

The TDA project report `Docs/STORY_SEQUENCE_CHECK_2026-09-19.md` describes the
story checks, vehicle test and GitHub LFS upload blocker. The project asset fix
must travel with the updated project; the plugin test alone cannot correct an
older copy of that asset.
