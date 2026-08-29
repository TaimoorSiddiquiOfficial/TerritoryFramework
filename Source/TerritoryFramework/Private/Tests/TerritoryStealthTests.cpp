#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "AI/TerritoryInvestigationActivity.h"
#include "AI/TerritoryInvestigationGoal.h"
#include "AI/TerritoryStealthObserverComponent.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryStealthProfile.h"
#include "Core/TerritoryStealthTags.h"
#include "Core/TerritoryTypes.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Interaction/TerritoryDistractionComponent.h"
#include "Interaction/TerritoryDistractionProjectile.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Tales/TerritoryStealthConditions.h"
#include "Tales/TerritoryStealthEvents.h"
#include "UnrealFramework/NarrativeCharacter.h"

namespace
{
	bool IsBlueprintAuthorityFunction(const UClass* Class, const FName Name)
	{
		const UFunction* Function = Class ? Class->FindFunctionByName(Name) : nullptr;
		return Function && Function->HasAnyFunctionFlags(FUNC_BlueprintCallable)
			&& Function->HasAnyFunctionFlags(FUNC_BlueprintAuthorityOnly);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthProfileDefaults,
	"TerritoryFramework.Stealth.ProfileDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthProfileDefaults::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UTerritoryStealthProfile* Profile =
		GetDefault<UTerritoryStealthProfile>();
	TestNotNull(TEXT("Stealth profile CDO exists"), Profile);
	if (!Profile) return false;
	TestTrue(TEXT("Assigned profile defers story contest by default"),
		Profile->bAllowStealthInfiltration);
	TestEqual(TEXT("Narrative attack-compatible immediate sight threshold"),
		Profile->ImmediateSightExposureThreshold, 0.8f);
	TestEqual(TEXT("Narrative attack-compatible minimum sight evidence"),
		Profile->MinimumSightEvidence, 0.2f);
	TestTrue(TEXT("Direct guard sight cannot be defeated by rating at point-blank range"),
		Profile->bPointBlankSightAlwaysExposes);
	TestEqual(TEXT("Point-blank exposure defaults to three metres"),
		Profile->PointBlankSightExposureDistance, 300.f);
	TestEqual(TEXT("Default investigator budget is bounded"),
		Profile->MaximumInvestigators, 2);
	TestTrue(TEXT("Default activity follows the Territory Narrative investigation contract"),
		Profile->InvestigationActivityClass
		&& Profile->InvestigationActivityClass->IsChildOf(
			UTerritoryInvestigationActivity::StaticClass()));
	TestEqual(TEXT("Exposure uses the built-in Gameplay Event tag"),
		Profile->BreakStealthGameplayEventTag,
		TerritoryStealthTags::Exposed.GetTag());
	TestTrue(TEXT("Confirmed exposure cancels active stealth abilities by default"),
		Profile->bCancelActiveStealthAbilitiesOnExposure);
	TestTrue(TEXT("Dedicated Territory stealth abilities are canceled on exposure"),
		Profile->StealthAbilityTagsToCancel.HasTagExact(
			TerritoryStealthTags::StealthAbility.GetTag()));
	TestTrue(TEXT("Narrative crouch stealth is canceled on exposure"),
		Profile->StealthAbilityTagsToCancel.HasTagExact(
			FGameplayTag::RequestGameplayTag(TEXT("Abilities.Crouch"), false)));
	TestTrue(TEXT("Confirmed exposure removes configured temporary stealth effects by default"),
		Profile->bRemoveActiveStealthEffectsOnExposure);
	TestTrue(TEXT("The plugin CDO does not hardcode project or vendor Gameplay Effect assets"),
		Profile->StealthGameplayEffectsToRemove.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthSightFormula,
	"TerritoryFramework.Stealth.SightFormula",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthSightFormula::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestEqual(TEXT("Zero rating preserves raw sight"),
		UTerritoryStealthObserverComponent::ApplyStealthRatingToSight(
			0.8f, 1.f, 0.f, 100.f), 0.8f);
	TestEqual(TEXT("Fifty rating halves raw sight"),
		UTerritoryStealthObserverComponent::ApplyStealthRatingToSight(
			0.8f, 1.f, 50.f, 100.f), 0.4f);
	TestEqual(TEXT("Maximum rating removes ordinary sight"),
		UTerritoryStealthObserverComponent::ApplyStealthRatingToSight(
			0.8f, 1.f, 100.f, 100.f), 0.f);
	TestEqual(TEXT("Invalid negative detection multiplier is safe"),
		UTerritoryStealthObserverComponent::ApplyStealthRatingToSight(
			0.8f, -2.f, 0.f, 100.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthAuthoringContract,
	"TerritoryFramework.Stealth.AuthoringContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthAuthoringContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestNotNull(TEXT("State Config exposes a stealth override"),
		FTerritoryStateConfig::StaticStruct()->FindPropertyByName(
			TEXT("StealthProfileOverride")));
	TestNotNull(TEXT("Definition exposes a default stealth profile"),
		UTerritoryDefinition::StaticClass()->FindPropertyByName(
			TEXT("DefaultStealthProfile")));
	TestNotNull(TEXT("Stealth profile exposes temporary Gameplay Effects to remove on detection"),
		UTerritoryStealthProfile::StaticClass()->FindPropertyByName(
			TEXT("StealthGameplayEffectsToRemove")));

	const UClass* ControlClass = UTerritoryControlSubsystem::StaticClass();
	TestTrue(TEXT("Register infiltrator is server-only Blueprint API"),
		IsBlueprintAuthorityFunction(ControlClass, TEXT("RegisterInfiltrator")));
	TestTrue(TEXT("Evidence mutation is server-only Blueprint API"),
		IsBlueprintAuthorityFunction(ControlClass, TEXT("ReportStealthEvidence")));
	TestTrue(TEXT("Quest override is server-only Blueprint API"),
		IsBlueprintAuthorityFunction(ControlClass,
			TEXT("SetStealthInfiltrationOverride")));

	TestTrue(TEXT("Exposure condition is inline authorable"),
		UTerritoryExposureCondition::StaticClass()->HasAnyClassFlags(CLASS_EditInlineNew));
	TestTrue(TEXT("Evidence condition is inline authorable"),
		UTerritoryStealthEvidenceCondition::StaticClass()->HasAnyClassFlags(CLASS_EditInlineNew));
	TestTrue(TEXT("Reveal event is inline authorable"),
		UTerritoryRevealInfiltratorEvent::StaticClass()->HasAnyClassFlags(CLASS_EditInlineNew));
	TestTrue(TEXT("Distraction event is inline authorable"),
		UTerritoryReportDistractionEvent::StaticClass()->HasAnyClassFlags(CLASS_EditInlineNew));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthNarrativeIntegrationContract,
	"TerritoryFramework.Stealth.NarrativeIntegrationContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthNarrativeIntegrationContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestNotNull(TEXT("Narrative still supplies StealthRating"),
		UNarrativeAttributeSetBase::StaticClass()->FindPropertyByName(
			TEXT("StealthRating")));
	TestNotNull(TEXT("Narrative character still exposes its stealth rating"),
		ANarrativeCharacter::StaticClass()->FindFunctionByName(
			TEXT("GetStealthRating")));
	TestNotNull(TEXT("Narrative sight-strength extension point still exists"),
		ANarrativeCharacter::StaticClass()->FindFunctionByName(
			TEXT("CalcSightStrength")));

	const ATerritoryGuardCharacter* GuardCDO =
		GetDefault<ATerritoryGuardCharacter>();
	TestNotNull(TEXT("Every native Territory guard owns the perception adapter"),
		GuardCDO ? GuardCDO->StealthObserver.Get() : nullptr);
	TestNotNull(TEXT("Distraction component is available to Narrative projectile Blueprints"),
		UTerritoryDistractionComponent::StaticClass());
	const ATerritoryDistractionProjectile* Projectile =
		GetDefault<ATerritoryDistractionProjectile>();
	TestNotNull(TEXT("Reusable Narrative distraction projectile owns swept collision"),
		Projectile ? Projectile->Collision.Get() : nullptr);
	TestNotNull(TEXT("Reusable Narrative distraction projectile owns movement"),
		Projectile ? Projectile->ProjectileMovement.Get() : nullptr);
	TestNotNull(TEXT("Reusable Narrative distraction projectile reports one hearing stimulus"),
		Projectile ? Projectile->Distraction.Get() : nullptr);

	const UTerritoryInvestigationGoal* Goal =
		GetDefault<UTerritoryInvestigationGoal>();
	TestTrue(TEXT("Investigation interrupts patrol but remains below Narrative attack"),
		Goal && Goal->DefaultScore > 1.f && Goal->DefaultScore < 3.f);
	return true;
}

#endif
