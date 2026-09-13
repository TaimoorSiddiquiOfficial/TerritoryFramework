#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/TerritoryAudioTypes.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryMusicTags.h"
#include "Core/TerritoryStealthProfile.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Sound/SoundWave.h"
#include "Music/NarrativeMusicSubsystem.h"
#include "Music/TaggedMusicSet.h"
#include "Subsystems/TerritoryMusicSubsystem.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryMusicAuthoringContract,
	"TerritoryFramework.Audio.NarrativeMusic.AuthoringContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryMusicAuthoringContract::RunTest(const FString& Parameters)
{
	const FTerritoryStateAudioConfig Defaults;
	TestFalse(TEXT("Territory audio is opt-in by default"),
		Defaults.bOverrideNarrativeMusic);
	TestFalse(TEXT("An empty row cannot override Narrative Music"),
		UTerritoryMusicSubsystem::IsMusicConfigUsable(Defaults));
	TestTrue(TEXT("Default state effect volume is neutral"),
		FMath::IsNearlyEqual(Defaults.StateEffectVolume, 1.f));
	TestTrue(TEXT("Default state effect pitch is neutral"),
		FMath::IsNearlyEqual(Defaults.StateEffectPitch, 1.f));

	TestTrue(TEXT("Locked Territory music tag exists"),
		TerritoryMusicTags::Locked.GetTag().IsValid());
	TestTrue(TEXT("Unclaimed Territory music tag exists"),
		TerritoryMusicTags::Unclaimed.GetTag().IsValid());
	TestTrue(TEXT("Contested Territory music tag exists"),
		TerritoryMusicTags::Contested.GetTag().IsValid());
	TestTrue(TEXT("Claimed Territory music tag exists"),
		TerritoryMusicTags::Claimed.GetTag().IsValid());

	FTerritoryStateAudioConfig Contested;
	Contested.bOverrideNarrativeMusic = true;
	Contested.MusicTheme = TerritoryMusicTags::Contested;
	TestTrue(TEXT("A valid state theme enables the Territory music adapter"),
		UTerritoryMusicSubsystem::IsMusicConfigUsable(Contested));
	Contested.MusicTheme = FGameplayTag();
	TestFalse(TEXT("An enabled row without a theme fails closed"),
		UTerritoryMusicSubsystem::IsMusicConfigUsable(Contested));

	const FStructProperty* AudioProperty = FindFProperty<FStructProperty>(
		FTerritoryStateConfig::StaticStruct(), TEXT("Audio"));
	TestNotNull(TEXT("Every state row owns one audio configuration"), AudioProperty);
	if (AudioProperty)
	{
		TestTrue(TEXT("The state row uses the typed audio struct"),
			AudioProperty->Struct.Get()
				== FTerritoryStateAudioConfig::StaticStruct());
		TestFalse(TEXT("Audio authoring is not duplicate save state"),
			AudioProperty->HasAnyPropertyFlags(CPF_SaveGame));
		TestFalse(TEXT("Audio authoring is not replicated state"),
			AudioProperty->HasAnyPropertyFlags(CPF_Net));
	}

	const UFunction* SetTheme = UNarrativeMusicSubsystem::StaticClass()
		->FindFunctionByName(TEXT("SetTheme"));
	const UFunction* OverrideMusicSet = UNarrativeMusicSubsystem::StaticClass()
		->FindFunctionByName(TEXT("OverrideMusicSet"));
	const UFunction* ResetMusicSet = UNarrativeMusicSubsystem::StaticClass()
		->FindFunctionByName(TEXT("ResetMusicSetToDefault"));
	TestNotNull(TEXT("Narrative Music exposes Set Theme"), SetTheme);
	TestNotNull(TEXT("Narrative Music exposes Tagged Music Set override"),
		OverrideMusicSet);
	TestNotNull(TEXT("Narrative Music exposes default-set restore"), ResetMusicSet);
	if (SetTheme)
	{
		const FStructProperty* ThemeProperty =
			FindFProperty<FStructProperty>(SetTheme, TEXT("Theme"));
		TestTrue(TEXT("Set Theme reflection bridge still receives a Gameplay Tag"),
			ThemeProperty && ThemeProperty->Struct == TBaseStructure<FGameplayTag>::Get());
		TestNotNull(TEXT("Set Theme reflection bridge still receives Immediate"),
			FindFProperty<FBoolProperty>(SetTheme, TEXT("bImmediate")));
		const FBoolProperty* ReturnProperty =
			FindFProperty<FBoolProperty>(SetTheme, TEXT("ReturnValue"));
		TestTrue(TEXT("Set Theme reflection bridge still returns success"),
			ReturnProperty && ReturnProperty->HasAnyPropertyFlags(CPF_ReturnParm));
	}
	if (OverrideMusicSet)
	{
		const FSoftObjectProperty* SetProperty =
			FindFProperty<FSoftObjectProperty>(OverrideMusicSet, TEXT("NewMusicSet"));
		TestTrue(TEXT("Music Set reflection bridge still receives a Tagged Music Set"),
			SetProperty && SetProperty->PropertyClass == UTaggedMusicSet::StaticClass());
	}
	if (ResetMusicSet)
	{
		const FBoolProperty* ReturnProperty =
			FindFProperty<FBoolProperty>(ResetMusicSet, TEXT("ReturnValue"));
		TestTrue(TEXT("Reset Music Set reflection bridge still returns success"),
			ReturnProperty && ReturnProperty->HasAnyPropertyFlags(CPF_ReturnParm));
	}
	TestTrue(TEXT("Territory adapter is a GameInstance subsystem like Narrative Music"),
		UTerritoryMusicSubsystem::StaticClass()->IsChildOf(
			UGameInstanceSubsystem::StaticClass()));

	const UFunction* RefreshFunction = UTerritoryMusicSubsystem::StaticClass()
		->FindFunctionByName(TEXT("RefreshNow"));
	TestNotNull(TEXT("Blueprint can request a cosmetic music refresh"), RefreshFunction);
	if (RefreshFunction)
	{
		TestTrue(TEXT("Music refresh is explicitly cosmetic"),
			RefreshFunction->HasAnyFunctionFlags(FUNC_BlueprintCosmetic));
		TestFalse(TEXT("Music refresh is not an authority mutation"),
			RefreshFunction->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryMusicDefinitionRoundTrip,
	"TerritoryFramework.Audio.NarrativeMusic.DefinitionStateRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryMusicDefinitionRoundTrip::RunTest(const FString& Parameters)
{
	UTerritoryPlaceDefinition* Definition = NewObject<UTerritoryPlaceDefinition>();
	TestNotNull(TEXT("Place Definition created"), Definition);
	if (!Definition) return false;

	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	Definition->StableTerritoryGUID = FGuid::NewGuid();
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	FTerritoryStateConfig& Unclaimed = Definition->StateConfigs.FindOrAdd(
		ETerritoryState::Unclaimed);
	UTerritoryStealthProfile* StateStealthProfile =
		NewObject<UTerritoryStealthProfile>(Definition);
	Unclaimed.StealthProfileOverride = StateStealthProfile;
	Unclaimed.Audio.bOverrideNarrativeMusic = true;
	Unclaimed.Audio.MusicTheme = TerritoryMusicTags::Unclaimed;
	Unclaimed.Audio.bImmediateThemeChange = true;
	FTerritoryStateConfig& Locked = Definition->StateConfigs.FindOrAdd(
		ETerritoryState::Locked);
	Locked.Audio.bOverrideNarrativeMusic = true;
	Locked.Audio.MusicTheme = TerritoryMusicTags::Locked;

	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	TestNotNull(TEXT("Audio definition preview world created"), World);
	if (!World) return false;

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags |= RF_Transient;
	ATerritoryProperty* Property = World->SpawnActor<ATerritoryProperty>(
		ATerritoryProperty::StaticClass(), FTransform::Identity, SpawnParams);
	TestNotNull(TEXT("Audio definition target Place created"), Property);
	if (Property)
	{
		TestTrue(TEXT("Definition audio applies through the normal Definition authority"),
			Definition->ApplyToTerritory(Property));
		TestEqual(TEXT("Preview Place starts in the configured active row"),
			Property->GetTerritoryState(), ETerritoryState::Unclaimed);
		FTerritoryStateAudioConfig ActiveAudio;
		TestTrue(TEXT("Place resolves its active audio row"),
			Property->GetActiveTerritoryAudioConfig(ActiveAudio));
		TestEqual(TEXT("Replicated state selects the authored Narrative music theme"),
			ActiveAudio.MusicTheme, TerritoryMusicTags::Unclaimed.GetTag());
		TestTrue(TEXT("Immediate state switch survives Definition cloning"),
			ActiveAudio.bImmediateThemeChange);
		TestEqual(TEXT("The shared state-row clone also preserves its stealth profile"),
			Property->GetActiveStealthProfile(), StateStealthProfile);

		Property->ForceSetTerritoryState(ETerritoryState::Locked);
		TestEqual(TEXT("Locking changes availability"),
			Property->GetTerritoryAvailability(), ETerritoryAvailability::Locked);
		TestEqual(TEXT("Locking does not revive a legacy political state"),
			Property->GetTerritoryState(), ETerritoryState::Unclaimed);
		FTerritoryStateAudioConfig LockedAudio;
		TestTrue(TEXT("Locked availability resolves the Locked audio row"),
			Property->GetActiveTerritoryAudioConfig(LockedAudio));
		TestEqual(TEXT("Locked availability selects its authored Narrative theme"),
			LockedAudio.MusicTheme, TerritoryMusicTags::Locked.GetTag());
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFTerritoryMusicObservationTransitions,
	"TerritoryFramework.Audio.NarrativeMusic.ObservationArrivalAndStateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFTerritoryMusicObservationTransitions::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Music observation world"), World)) return false;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	auto* Music = NewObject<UTerritoryMusicSubsystem>(GameInstance);
	auto* Place = World->SpawnActor<ATerritoryProperty>();
	auto* OtherPlace = World->SpawnActor<ATerritoryProperty>();
	if (!Place || !OtherPlace) { World->DestroyWorld(false); return false; }
	auto* Definition = NewObject<UTerritoryPlaceDefinition>();
	Definition->TerritoryTag = FGameplayTag::RequestGameplayTag(
		TEXT("Territory.HavenReach.MarketSquare.Blacksmith"), false);
	Definition->StableTerritoryGUID = FGuid::NewGuid();
	Definition->TerritoryActorClass = ATerritoryProperty::StaticClass();
	USoundWave* Arrival = NewObject<USoundWave>();
	USoundWave* Departure = NewObject<USoundWave>();
	USoundWave* Locked = NewObject<USoundWave>();
	auto& Audio = Definition->StateConfigs.FindOrAdd(ETerritoryState::Unclaimed).Audio;
	Audio.StateEnteredSound = Arrival;
	Audio.StateExitedSound = Departure;
	Audio.bPlayEnteredSoundOnPlayerArrival = true;
	Audio.bPlayExitedSoundOnPlayerDeparture = true;
	Audio.StateEffectVolume = 0.4f;
	Definition->StateConfigs.FindOrAdd(ETerritoryState::Locked).Audio.StateEnteredSound = Locked;
	TestTrue(TEXT("Authored audio applies"), Definition->ApplyToTerritory(Place));
	TestTrue(TEXT("Second place audio applies"), Definition->ApplyToTerritory(OtherPlace));

	TestEqual(TEXT("Outside all bounds is silent"), Music->RefreshObservedTerritory(nullptr).Num(), 0);
	auto Sounds = Music->RefreshObservedTerritory(Place);
	TestEqual(TEXT("First entry from outside produces exactly one cue"), Sounds.Num(), 1);
	if (Sounds.Num() == 1)
	{
		TestTrue(TEXT("Entry selects the authored arrival sound"), Sounds[0].Sound.Get() == Arrival);
		TestEqual(TEXT("Entry preserves authored sound volume"), Sounds[0].Config.StateEffectVolume, 0.4f);
	}
	TestEqual(TEXT("Standing in the place does not repeat arrival"), Music->RefreshObservedTerritory(Place).Num(), 0);
	Sounds = Music->RefreshObservedTerritory(nullptr);
	TestEqual(TEXT("Leaving produces one departure"), Sounds.Num(), 1);
	if (Sounds.Num() == 1) TestTrue(TEXT("Departure uses the previous row"), Sounds[0].Sound.Get() == Departure);
	TestEqual(TEXT("Re-entry produces one arrival"), Music->RefreshObservedTerritory(Place).Num(), 1);
	Sounds = Music->RefreshObservedTerritory(OtherPlace);
	TestEqual(TEXT("Moving between places requests departure then arrival"), Sounds.Num(), 2);
	if (Sounds.Num() == 2)
	{
		TestTrue(TEXT("Old row exits first"), Sounds[0].Sound.Get() == Departure);
		TestTrue(TEXT("New row enters second"), Sounds[1].Sound.Get() == Arrival);
	}
	OtherPlace->ForceSetTerritoryState(ETerritoryState::Locked);
	Sounds = Music->RefreshObservedTerritory(OtherPlace);
	TestEqual(TEXT("Availability change selects exit and Locked entry"), Sounds.Num(), 2);
	if (Sounds.Num() == 2) TestTrue(TEXT("Locked row sound selected"), Sounds[1].Sound.Get() == Locked);
	TestEqual(TEXT("Stable Locked state does not repeat"), Music->RefreshObservedTerritory(OtherPlace).Num(), 0);

	// Recreated local observation after load/travel does not carry presentation
	// history. The opt-in arrival rule is evaluated against the loaded state.
	Music->ResetForWorld(World);
	TestEqual(TEXT("First observation of a loaded place respects arrival opt-in"), Music->RefreshObservedTerritory(Place).Num(), 1);
	Music->RefreshObservedTerritory(nullptr);
	Definition->StateConfigs.FindChecked(ETerritoryState::Unclaimed).Audio.bPlayEnteredSoundOnPlayerArrival = false;
	TestTrue(TEXT("Silent arrival authoring applies"), Definition->ApplyToTerritory(Place));
	TestEqual(TEXT("Arrival can be disabled without disabling state cues"), Music->RefreshObservedTerritory(Place).Num(), 0);
	Place->ForceSetTerritoryState(ETerritoryState::Locked);
	TestEqual(TEXT("State change still emits cues with arrival disabled"), Music->RefreshObservedTerritory(Place).Num(), 2);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
