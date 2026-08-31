#pragma once

#include "CoreMinimal.h"
#include "Core/TerritoryAudioTypes.h"
#include "Core/TerritoryTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "TerritoryMusicSubsystem.generated.h"

class ATerritoryVolume;
class UNarrativeMusicSubsystem;
class UTaggedMusicSet;

/**
 * Client-cosmetic bridge from replicated Territory state to Narrative Music.
 *
 * One soundtrack exists per GameInstance because Narrative Music is itself a
 * GameInstance subsystem. For split-screen, the first valid local player in the
 * GameInstance local-player array is the explicit soundtrack listener.
 */
UCLASS(BlueprintType, meta=(DisplayName="Territory Narrative Music"))
class TERRITORYFRAMEWORK_API UTerritoryMusicSubsystem
	: public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }

	/** Immediately re-evaluates local position and replicated Territory state. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Territory|Audio",
		meta=(DisplayName="Refresh Territory Music Now"))
	void RefreshNow();

	/** Most-specific loaded Territory currently containing the soundtrack listener. */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Territory|Audio")
	ATerritoryVolume* GetObservedTerritory() const { return ObservedTerritory.Get(); }

	/** Territory currently supplying the Narrative music rule; may be a parent. */
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Territory|Audio")
	ATerritoryVolume* GetMusicTerritory() const { return MusicTerritory.Get(); }

	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Territory|Audio")
	FGameplayTag GetAppliedMusicTheme() const { return AppliedMusicTheme; }

	/** Pure authoring rule used by validation and native tests. */
	static bool IsMusicConfigUsable(const FTerritoryStateAudioConfig& Config);

private:
	AActor* ResolveLocalListener() const;
	ATerritoryVolume* ResolveMusicTerritory(const FVector& ListenerLocation) const;
	const FTerritoryStateAudioConfig* FindStateAudio(
		const ATerritoryVolume* Territory, ETerritoryState State) const;
	void RefreshObservedTerritory(ATerritoryVolume* NewTerritory);
	void RefreshMusicTerritory(ATerritoryVolume* NewTerritory);
	void ApplyMusicRule(ATerritoryVolume* Territory,
		const FTerritoryStateAudioConfig& Config);
	void ReleaseMusicRule();
	void MaintainMusicRule();
	void PlayStateSound(const TSoftObjectPtr<class USoundBase>& Sound,
		const FTerritoryStateAudioConfig& Config) const;
	void ResetForWorld(UWorld* NewWorld);

	UNarrativeMusicSubsystem* GetNarrativeMusic() const;
	static bool SetNarrativeTheme(UNarrativeMusicSubsystem* Music,
		FGameplayTag Theme, bool bImmediate);
	static void SetNarrativeMusicSet(UNarrativeMusicSubsystem* Music,
		const TSoftObjectPtr<UTaggedMusicSet>& MusicSet);
	static void ResetNarrativeMusicSet(UNarrativeMusicSubsystem* Music);

	float RefreshAccumulator = 0.f;
	float RefreshInterval = 0.2f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UWorld> EvaluatedWorld;

	UPROPERTY(Transient)
	TWeakObjectPtr<ATerritoryVolume> ObservedTerritory;

	ETerritoryState ObservedState = ETerritoryState::Unclaimed;
	FTerritoryStateAudioConfig ObservedAudio;
	bool bHasObservedState = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<ATerritoryVolume> MusicTerritory;

	ETerritoryState MusicState = ETerritoryState::Unclaimed;
	FString AppliedRuleKey;
	FTerritoryStateAudioConfig AppliedAudio;
	FGameplayTag AppliedMusicTheme;

	UPROPERTY(Transient)
	TSoftObjectPtr<UTaggedMusicSet> BaselineMusicSet;

	FGameplayTag BaselineMusicTheme;
	bool bOwnsMusicRule = false;
	bool bThemeApplyPending = false;
	bool bThemeAppliedByTerritory = false;
	bool bRestoringBaseline = false;
};
