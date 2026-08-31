#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Core/TerritoryAudioTypes.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryHierarchy.h"
#include "Core/TerritoryMusicTags.h"
#include "Core/TerritoryStealthProfile.h"
#include "Engine/World.h"
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

	TestNotNull(TEXT("Narrative Music exposes Set Theme"),
		UNarrativeMusicSubsystem::StaticClass()->FindFunctionByName(TEXT("SetTheme")));
	TestNotNull(TEXT("Narrative Music exposes Tagged Music Set override"),
		UNarrativeMusicSubsystem::StaticClass()->FindFunctionByName(
			TEXT("OverrideMusicSet")));
	TestNotNull(TEXT("Narrative Music exposes default-set restore"),
		UNarrativeMusicSubsystem::StaticClass()->FindFunctionByName(
			TEXT("ResetMusicSetToDefault")));
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

#endif // WITH_DEV_AUTOMATION_TESTS
