#include "Core/TerritoryGuardPerceptionProfile.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"

UTerritoryGuardPerceptionProfile::UTerritoryGuardPerceptionProfile()
{
	// Defaults mirror BP_NarrativeNPCController so an unedited tier is a no-op.
}

FPrimaryAssetId UTerritoryGuardPerceptionProfile::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("TerritoryGuardPerceptionProfile"), GetFName());
}

bool UTerritoryGuardPerceptionProfile::ApplyTo(UAIPerceptionComponent* Perception) const
{
	if (!Perception)
	{
		return false;
	}

	FAISenseAffiliationFilter Filter;
	Filter.bDetectEnemies = bDetectEnemies ? 1 : 0;
	Filter.bDetectNeutrals = bDetectNeutrals ? 1 : 0;
	Filter.bDetectFriendlies = bDetectFriendlies ? 1 : 0;

	bool bApplied = false;
	UAISenseConfig* Configs[] = {
		Perception->GetSenseConfig(UAISense::GetSenseID<UAISense_Sight>()),
		Perception->GetSenseConfig(UAISense::GetSenseID<UAISense_Hearing>()),
		Perception->GetSenseConfig(UAISense::GetSenseID<UAISense_Damage>()) };
	for (UAISenseConfig* Config : Configs)
	{
		if (!Config)
		{
			continue;
		}

		if (UAISenseConfig_Sight* Sight = Cast<UAISenseConfig_Sight>(Config))
		{
			Sight->SightRadius = FMath::Max(0.f, SightRadius);
			// UE requires LoseSightRadius >= SightRadius or the sense silently misbehaves.
			Sight->LoseSightRadius = FMath::Max(Sight->SightRadius, LoseSightRadius);
			Sight->PeripheralVisionAngleDegrees = FMath::Clamp(
				PeripheralVisionAngleDegrees, 1.f, 360.f);
			Sight->AutoSuccessRangeFromLastSeenLocation =
				AutoSuccessRangeFromLastSeenLocation;
			Sight->DetectionByAffiliation = Filter;
			Sight->SetMaxAge(FMath::Max(0.f, SightMaxAge));
			Perception->ConfigureSense(*Sight);
			bApplied = true;
		}
		else if (UAISenseConfig_Hearing* Hearing = Cast<UAISenseConfig_Hearing>(Config))
		{
			Hearing->HearingRange = FMath::Max(0.f, HearingRange);
			Hearing->DetectionByAffiliation = Filter;
			Hearing->SetMaxAge(FMath::Max(0.f, HearingMaxAge));
			Perception->ConfigureSense(*Hearing);
			bApplied = true;
		}
		else if (UAISenseConfig_Damage* Damage = Cast<UAISenseConfig_Damage>(Config))
		{
			Damage->SetMaxAge(FMath::Max(0.f, DamageMaxAge));
			Perception->ConfigureSense(*Damage);
			bApplied = true;
		}
	}

	if (bApplied)
	{
		Perception->RequestStimuliListenerUpdate();
	}
	return bApplied;
}
