#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Abilities/TerritoryDistractionAbility.h"
#include "AI/TerritoryInvestigationActivity.h"
#include "AI/TerritoryInvestigationGoal.h"
#include "AI/TerritoryStealthObserverComponent.h"
#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryDisguiseProfile.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryGuardPerceptionProfile.h"
#include "Core/TerritoryNoiseProfile.h"
#include "Interaction/TerritoryHidingSpotComponent.h"
#include "Core/TerritoryStealthProfile.h"
#include "Core/TerritoryStealthTags.h"
#include "Core/TerritoryBlueprintLibrary.h"
#include "Core/TerritoryTypes.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Interaction/TerritoryDistractionComponent.h"
#include "Interaction/TerritoryDistractionProjectile.h"
#include "Items/InventoryComponent.h"
#include "Items/NarrativeItem.h"
#include "Items/TerritoryDisguiseClothingItem.h"
#include "Subsystems/TerritoryControlSubsystem.h"
#include "Subsystems/TerritoryDisguiseSubsystem.h"
#include "Tales/TerritoryDisguiseTask.h"
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
	TestTrue(TEXT("Narrative footstep stimuli raise suspicion by default"),
		Profile->bFootstepsRaiseSuspicion);
	TestTrue(TEXT("One heard footstep stays a small suspicion gain"),
		Profile->FootstepSuspicion > 0.f && Profile->FootstepSuspicion <= 0.2f);
	TestTrue(TEXT("Repeated footsteps are rate limited per guard and target"),
		Profile->FootstepEvidenceCooldown > 0.f);
	TestTrue(TEXT("Footsteps sit below the investigation dispatch threshold"),
		Profile->FootstepSuspicion < Profile->InvestigationSuspicionThreshold);
	TestTrue(TEXT("A distraction is strong enough to dispatch investigators"),
		Profile->ThrowableDistractionSuspicion >= Profile->InvestigationSuspicionThreshold);
	TestTrue(TEXT("Alert ladder thresholds are ordered"),
		Profile->SearchingSuspicionThreshold < Profile->AlarmedSuspicionThreshold
		&& Profile->AlarmedSuspicionThreshold < Profile->LockdownSuspicionThreshold);
	TestTrue(TEXT("A rejected disguise raises suspicion slowly by default"),
		Profile->bDisguiseRaisesSuspicion && Profile->DisguiseSuspicionPerSecond > 0.f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthAttackGoalGate,
	"TerritoryFramework.Stealth.AttackGoalGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * Pins the guard attack-goal gate rule: stealth suppresses the goal only while the target is
 * hidden and not point-blank, not exposed, and not firing while seen. Both the success path
 * (guard investigates) and the failure path (guard attacks) are covered.
 */
bool FTFStealthAttackGoalGate::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float Min = 0.2f;
	TestTrue(TEXT("A hidden target below the sight threshold suppresses the attack goal"),
		UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState(
			ETerritoryExposureState::Undetected, false, false, true, 0.1f, Min));
	TestTrue(TEXT("A suspicious target still suppresses the attack goal"),
		UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState(
			ETerritoryExposureState::Suspicious, false, false, true, 0.f, Min));
	TestFalse(TEXT("A confirmed target keeps the normal attack goal"),
		UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState(
			ETerritoryExposureState::Exposed, false, false, true, 0.f, Min));
	TestFalse(TEXT("Point-blank contact always keeps the attack goal"),
		UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState(
			ETerritoryExposureState::Undetected, true, false, true, 0.f, Min));
	TestFalse(TEXT("Firing while seen keeps the attack goal"),
		UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState(
			ETerritoryExposureState::Suspicious, false, true, true, 0.05f, Min));
	TestFalse(TEXT("Sight at the evidence threshold keeps the attack goal"),
		UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState(
			ETerritoryExposureState::Undetected, false, false, true, Min, Min));
	TestFalse(TEXT("Clear sight above the threshold keeps the attack goal"),
		UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState(
			ETerritoryExposureState::Suspicious, false, false, true, 0.6f, Min));
	// The chase-loop regression: an exposed player the contest rule does not admit to combat
	// (holstered, no war, no recent aggression) must never receive an attack goal.
	TestTrue(TEXT("An exposed but unarmed target is investigated instead of chased"),
		UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState(
			ETerritoryExposureState::Exposed, false, false, false, 1.f, Min));
	TestTrue(TEXT("Point-blank does not bypass the contest admission rule"),
		UTerritoryStealthObserverComponent::ShouldSuppressAttackGoalForState(
			ETerritoryExposureState::Exposed, true, false, false, 1.f, Min));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthAggressionFreshness,
	"TerritoryFramework.Stealth.AggressionFreshness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * Pins the freshness rule that stopped stale gunfire from keeping a holstered visitor attackable:
 * aggression expires after the window, and the guard's combat permission, the story-bounds contest
 * and the attack-goal gate now share this one answer.
 */
bool FTFStealthAggressionFreshness::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FTerritoryInfiltrationSnapshot Snapshot;
	Snapshot.LastEvidence = ETerritoryStealthEvidence::Gunshot;
	Snapshot.LastEvidenceWorldTime = 100.f;

	TestTrue(TEXT("A fresh gunshot counts as aggression"),
		UTerritoryBlueprintLibrary::IsRecentAggressiveStealthEvidenceAtTime(
			Snapshot, 110.f, 20.f));
	TestFalse(TEXT("A stale gunshot stops counting as aggression"),
		UTerritoryBlueprintLibrary::IsRecentAggressiveStealthEvidenceAtTime(
			Snapshot, 200.f, 20.f));
	Snapshot.LastEvidenceWorldTime = -1.f;
	TestFalse(TEXT("An untimestamped clue cannot prove recent aggression"),
		UTerritoryBlueprintLibrary::IsRecentAggressiveStealthEvidenceAtTime(
			Snapshot, 110.f, 20.f));
	Snapshot.LastEvidenceWorldTime = 100.f;
	TestTrue(TEXT("A zero window keeps the legacy last-evidence-wins behaviour"),
		UTerritoryBlueprintLibrary::IsRecentAggressiveStealthEvidenceAtTime(
			Snapshot, 9999.f, 0.f));
	TestTrue(TEXT("A negative window also keeps the legacy behaviour"),
		UTerritoryBlueprintLibrary::IsRecentAggressiveStealthEvidenceAtTime(
			Snapshot, 9999.f, -1.f));

	Snapshot.LastEvidence = ETerritoryStealthEvidence::Footstep;
	TestFalse(TEXT("Suspicion-only evidence is never aggression"),
		UTerritoryBlueprintLibrary::IsRecentAggressiveStealthEvidenceAtTime(
			Snapshot, 100.f, 20.f));
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
	TestTrue(TEXT("Tag-addressed evidence reporting is server-only Blueprint API"),
		IsBlueprintAuthorityFunction(ControlClass, TEXT("ReportStealthEvidenceByTag")));
	TestNotNull(TEXT("Stealth profile exposes data-driven evidence rules"),
		UTerritoryStealthProfile::StaticClass()->FindPropertyByName(
			TEXT("EvidenceRules")));
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
	const UTerritoryDistractionAbility* Ability =
		GetDefault<UTerritoryDistractionAbility>();
	TestNotNull(TEXT("Distraction projectile is owned by a Narrative Gameplay Ability"),
		Ability);
	TestTrue(TEXT("Distraction ability uses Narrative's Throw input tag"),
		Ability && Ability->InputTag == FGameplayTag::RequestGameplayTag(
			TEXT("Narrative.Input.Throw"), false));
	TestTrue(TEXT("Distraction ability exposes the Narrative-compatible ability tag"),
		Ability && Ability->GetAssetTags().HasTagExact(
			TerritoryStealthTags::DistractionAbility));
	TestNotNull(TEXT("Distraction ability has a safe native projectile fallback"),
		Ability ? Ability->ProjectileClass.Get() : nullptr);
	TestTrue(TEXT("Equipped Narrative item is the single distraction ability source"),
		Ability && Ability->bRequireEquippedNarrativeItemSource);
	TestTrue(TEXT("A successful distraction throw consumes one source item"),
		Ability && Ability->bConsumeSourceItemOnSuccessfulThrow);
	TestTrue(TEXT("Missing source item fails the inventory contract"),
		Ability && !Ability->IsThrowableSourceItemReady(nullptr));
	if (Ability)
	{
		UNarrativeItem* SourceItem = NewObject<UNarrativeItem>(GetTransientPackage());
		TestFalse(TEXT("An item outside Narrative inventory cannot activate the throw"),
			Ability->IsThrowableSourceItemReady(SourceItem));
		SourceItem->OwningInventory = NewObject<UNarrativeInventoryComponent>(
			GetTransientPackage());
		TestFalse(TEXT("An inventory pointer alone does not prove membership or equipment"),
			Ability->IsThrowableSourceItemReady(SourceItem));
		SourceItem->SetQuantity(0);
		TestFalse(TEXT("An empty item stack cannot activate the throw"),
			Ability->IsThrowableSourceItemReady(SourceItem));
	}
	TestTrue(TEXT("Distraction thrown Gameplay Event tag exists"),
		TerritoryStealthTags::DistractionThrownEvent.GetTag().IsValid());
	TestTrue(TEXT("Distraction impact Gameplay Event tag exists"),
		TerritoryStealthTags::DistractionImpactEvent.GetTag().IsValid());

	const UTerritoryInvestigationGoal* Goal =
		GetDefault<UTerritoryInvestigationGoal>();
	TestTrue(TEXT("Investigation interrupts patrol but remains below Narrative attack"),
		Goal && Goal->DefaultScore > 1.f && Goal->DefaultScore < 3.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDisguisePolicy,
	"TerritoryFramework.Stealth.Disguise.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDisguisePolicy::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UTerritoryDisguiseProfile* Disguise =
		NewObject<UTerritoryDisguiseProfile>(GetTransientPackage());
	UTerritoryStealthProfile* TerritoryPolicy =
		NewObject<UTerritoryStealthProfile>(GetTransientPackage());
	const FGameplayTag Heroes = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Heroes"), false);
	const FGameplayTag Bandits = FGameplayTag::RequestGameplayTag(
		TEXT("Narrative.Factions.Bandits"), false);
	Disguise->PerceivedFaction = Bandits;
	Disguise->Quality = 1.f;
	FText Reason;
	TestTrue(TEXT("Perfect Bandit uniform passes ordinary Bandit security"),
		UTerritoryDisguiseSubsystem::EvaluateDisguisePolicy(
			Disguise, TerritoryPolicy, Bandits, Bandits, false, Reason));
	TestFalse(TEXT("A Bandit uniform does not impersonate Heroes"),
		UTerritoryDisguiseSubsystem::EvaluateDisguisePolicy(
			Disguise, TerritoryPolicy, Heroes, Heroes, false, Reason));
	TestFalse(TEXT("A faction that burned the identity rejects it"),
		UTerritoryDisguiseSubsystem::EvaluateDisguisePolicy(
			Disguise, TerritoryPolicy, Bandits, Bandits, true, Reason));

	TerritoryPolicy->RequiredDisguiseClearanceTags.AddTag(
		TerritoryStealthTags::DisguiseOfficerClearance);
	TestFalse(TEXT("Normal uniform fails an officer checkpoint"),
		UTerritoryDisguiseSubsystem::EvaluateDisguisePolicy(
			Disguise, TerritoryPolicy, Bandits, Bandits, false, Reason));
	Disguise->ClearanceTags.AddTag(TerritoryStealthTags::DisguiseOfficerClearance);
	TestTrue(TEXT("Officer uniform passes the officer checkpoint"),
		UTerritoryDisguiseSubsystem::EvaluateDisguisePolicy(
			Disguise, TerritoryPolicy, Bandits, Bandits, false, Reason));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFDisguiseAuthoringContract,
	"TerritoryFramework.Stealth.Disguise.AuthoringContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFDisguiseAuthoringContract::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UTerritoryDisguiseProfile* Profile =
		GetDefault<UTerritoryDisguiseProfile>();
	TestNotNull(TEXT("Disguise profile is available"), Profile);
	TestTrue(TEXT("Disguise profile defaults to perfect normal quality"),
		Profile && FMath::IsNearlyEqual(Profile->Quality, 1.f));
	TestTrue(TEXT("Disguise activation event defaults to a registered tag"),
		Profile && Profile->ActivatedEventTag.IsValid());
	TestTrue(TEXT("Disguise compromise event defaults to a registered tag"),
		Profile && Profile->CompromisedEventTag.IsValid());
	TestTrue(TEXT("Narrative clothing integration derives from clothing"),
		UTerritoryDisguiseClothingItem::StaticClass()->IsChildOf(
			UEquippableItem_Clothing::StaticClass()));
	TestTrue(TEXT("Disguise condition is inline authorable"),
		UTerritoryDisguiseCondition::StaticClass()->HasAnyClassFlags(CLASS_EditInlineNew));
	TestTrue(TEXT("Identity-check event is inline authorable"),
		UTerritoryDisguiseIdentityCheckEvent::StaticClass()->HasAnyClassFlags(
			CLASS_EditInlineNew));
	TestTrue(TEXT("Double-agent quest task is inline authorable"),
		UTerritoryDisguiseTask::StaticClass()->HasAnyClassFlags(CLASS_EditInlineNew));

	const UClass* SubsystemClass = UTerritoryDisguiseSubsystem::StaticClass();
	TestTrue(TEXT("Activate Disguise is server-only Blueprint API"),
		IsBlueprintAuthorityFunction(SubsystemClass, TEXT("ActivateDisguise")));
	TestTrue(TEXT("Identity Check is server-only Blueprint API"),
		IsBlueprintAuthorityFunction(SubsystemClass, TEXT("PerformIdentityCheck")));
	TestTrue(TEXT("Active disguise GAS state tag exists"),
		TerritoryStealthTags::DisguiseActive.GetTag().IsValid());
	return true;
}

/**
 * The holstered-weapon contest gate decides whether an unarmed visitor may start a story contest,
 * be registered by the exposure escalation, or be fought by a guard. The rule is pure, so the
 * decision itself is pinned here; the war and wield-state cases need a live world and live behind
 * the guard and control fixtures.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthContestWeaponGate,
	"TerritoryFramework.Stealth.Regression.HolsteredWeaponContestGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthContestWeaponGate::RunTest(const FString& Parameters)
{
	(void)Parameters;

	AActor* BareActor = NewObject<AActor>(GetTransientPackage(), TEXT("ContestGateProbe"));
	if (!TestNotNull(TEXT("Probe actor exists"), BareActor)) return false;

	TestTrue(TEXT("An unconfigured profile keeps the unrestricted legacy admission"),
		UTerritoryBlueprintLibrary::DoesPawnAdmitStoryContest(BareActor, nullptr, nullptr));

	UTerritoryStealthProfile* Profile =
		NewObject<UTerritoryStealthProfile>(GetTransientPackage(), TEXT("ContestGatePolicy"));
	if (!TestNotNull(TEXT("Probe profile exists"), Profile)) return false;
	TestTrue(TEXT("Requiring a wielded weapon is the shipped default"),
		Profile->bContestRequiresWieldedWeapon);
	TestTrue(TEXT("The warning cooldown defaults to a finite value"),
		FMath::IsFinite(Profile->HolsteredWeaponWarningCooldown)
			&& Profile->HolsteredWeaponWarningCooldown > 0.f);

	TestFalse(TEXT("A holstered, unarmed visitor with no war is not admitted"),
		UTerritoryBlueprintLibrary::DoesPawnAdmitStoryContest(BareActor, nullptr, Profile));
	TestTrue(TEXT("An aggressor is admitted even while unarmed"),
		UTerritoryBlueprintLibrary::DoesPawnAdmitStoryContest(BareActor, nullptr, Profile, true));

	Profile->bContestRequiresWieldedWeapon = false;
	TestTrue(TEXT("Clearing the requirement restores the legacy admission"),
		UTerritoryBlueprintLibrary::DoesPawnAdmitStoryContest(BareActor, nullptr, Profile));

	TestTrue(TEXT("Confirmed damage is aggression"),
		UTerritoryBlueprintLibrary::IsAggressiveStealthEvidence(
			ETerritoryStealthEvidence::Damage));
	TestTrue(TEXT("A seen defender kill is aggression"),
		UTerritoryBlueprintLibrary::IsAggressiveStealthEvidence(
			ETerritoryStealthEvidence::DefenderKilledSeen));
	TestTrue(TEXT("Firing while seen is aggression"),
		UTerritoryBlueprintLibrary::IsAggressiveStealthEvidence(
			ETerritoryStealthEvidence::FireSeen));
	TestFalse(TEXT("Plain guard sight is not aggression"),
		UTerritoryBlueprintLibrary::IsAggressiveStealthEvidence(
			ETerritoryStealthEvidence::Sight));
	TestFalse(TEXT("An unseen corpse is not aggression"),
		UTerritoryBlueprintLibrary::IsAggressiveStealthEvidence(
			ETerritoryStealthEvidence::Corpse));
	TestFalse(TEXT("A throwable distraction is not aggression"),
		UTerritoryBlueprintLibrary::IsAggressiveStealthEvidence(
			ETerritoryStealthEvidence::ThrowableDistraction));
	TestFalse(TEXT("Heard footsteps are suspicion, not aggression"),
		UTerritoryBlueprintLibrary::IsAggressiveStealthEvidence(
			ETerritoryStealthEvidence::Footstep));
	TestFalse(TEXT("Disguise doubt is not aggression"),
		UTerritoryBlueprintLibrary::IsAggressiveStealthEvidence(
			ETerritoryStealthEvidence::Disguise));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthEvidenceRules,
	"TerritoryFramework.Stealth.EvidenceRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthEvidenceRules::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UTerritoryStealthProfile* Profile =
		NewObject<UTerritoryStealthProfile>(GetTransientPackage());
	if (!TestNotNull(TEXT("Probe profile exists"), Profile)) return false;

	TestEqual(TEXT("Without a rule the built-in evidence weight is used"),
		Profile->GetEvidenceSuspicion(ETerritoryStealthEvidence::Gunshot, 0.35f), 0.35f);
	TestFalse(TEXT("Without a rule evidence does not force exposure"),
		Profile->IsEvidenceForcedExposure(ETerritoryStealthEvidence::Gunshot));

	FTerritoryEvidenceRule Rule;
	Rule.Tag = TerritoryStealthTags::DistractionThrowable.GetTag();
	Rule.Evidence = ETerritoryStealthEvidence::Gunshot;
	Rule.Suspicion = 0.9f;
	Rule.bForcesExposure = true;
	Profile->EvidenceRules.Add(Rule);

	TestEqual(TEXT("A rule overrides the built-in suspicion weight"),
		Profile->GetEvidenceSuspicion(ETerritoryStealthEvidence::Gunshot, 0.35f), 0.9f);
	TestTrue(TEXT("A rule can force exposure"),
		Profile->IsEvidenceForcedExposure(ETerritoryStealthEvidence::Gunshot));
	TestFalse(TEXT("Other channels stay on their built-in behaviour"),
		Profile->IsEvidenceForcedExposure(ETerritoryStealthEvidence::Footstep));

	FTerritoryEvidenceRule Found;
	TestTrue(TEXT("The rule is found by evidence channel"),
		Profile->FindEvidenceRule(ETerritoryStealthEvidence::Gunshot, Found));
	TestEqual(TEXT("The found rule keeps its weight"), Found.Suspicion, 0.9f);
	TestTrue(TEXT("The rule is found by its semantic tag"),
		Profile->FindEvidenceRuleForTag(
			TerritoryStealthTags::DistractionThrowable.GetTag(), Found));
	TestFalse(TEXT("An invalid tag never matches a rule"),
		Profile->FindEvidenceRuleForTag(FGameplayTag(), Found));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthGuardPerceptionProfile,
	"TerritoryFramework.Stealth.GuardPerceptionProfile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthGuardPerceptionProfile::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UTerritoryGuardPerceptionProfile* Tier =
		GetDefault<UTerritoryGuardPerceptionProfile>();
	if (!TestNotNull(TEXT("Guard perception tier CDO exists"), Tier)) return false;

	// Defaults must equal Narrative's shipped controller values, so an untouched tier is a no-op.
	TestEqual(TEXT("Default sight radius matches Narrative"), Tier->SightRadius, 6000.f);
	TestEqual(TEXT("Default lose-sight radius matches Narrative"), Tier->LoseSightRadius, 7000.f);
	TestEqual(TEXT("Default peripheral angle matches Narrative"),
		Tier->PeripheralVisionAngleDegrees, 180.f);
	TestEqual(TEXT("Default sight memory matches Narrative"), Tier->SightMaxAge, 0.f);
	TestEqual(TEXT("Default hearing range matches Narrative"), Tier->HearingRange, 10000.f);
	TestNotNull(TEXT("Tier can be applied to a perception component"),
		UTerritoryGuardPerceptionProfile::StaticClass()->FindFunctionByName(
			TEXT("ApplyTo")));
	TestTrue(TEXT("Tier is addressable as a primary asset"),
		Tier->GetPrimaryAssetId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthAlertLadder,
	"TerritoryFramework.Stealth.AlertLadder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthAlertLadder::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UTerritoryStealthProfile* Profile = GetDefault<UTerritoryStealthProfile>();
	if (!TestNotNull(TEXT("Profile CDO exists"), Profile)) return false;

	using EL = ETerritoryAlertLevel;
	TestEqual(TEXT("No suspicion is Calm"),
		UTerritoryControlSubsystem::ComputeAlertLevel(Profile,
			ETerritoryExposureState::Undetected, 0.f), EL::Calm);
	TestEqual(TEXT("Low suspicion is Suspicious"),
		UTerritoryControlSubsystem::ComputeAlertLevel(Profile,
			ETerritoryExposureState::Undetected, 0.1f), EL::Suspicious);
	TestEqual(TEXT("Searching begins at the searching threshold"),
		UTerritoryControlSubsystem::ComputeAlertLevel(Profile,
			ETerritoryExposureState::Suspicious, Profile->SearchingSuspicionThreshold), EL::Searching);
	TestEqual(TEXT("Alarmed begins at the alarmed threshold"),
		UTerritoryControlSubsystem::ComputeAlertLevel(Profile,
			ETerritoryExposureState::Suspicious, Profile->AlarmedSuspicionThreshold), EL::Alarmed);
	TestEqual(TEXT("A confirmed target is Alarmed even with low suspicion"),
		UTerritoryControlSubsystem::ComputeAlertLevel(Profile,
			ETerritoryExposureState::Exposed, 0.1f), EL::Alarmed);
	TestEqual(TEXT("Confirmed plus full suspicion is Lockdown"),
		UTerritoryControlSubsystem::ComputeAlertLevel(Profile,
			ETerritoryExposureState::Exposed, Profile->LockdownSuspicionThreshold), EL::Lockdown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTFStealthHidingSpotAndNoise,
	"TerritoryFramework.Stealth.HidingSpotAndNoise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTFStealthHidingSpotAndNoise::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Hiding spots are a server-registered world state; the classes and authority split are pinned
	// here, the world behaviour is the PIE step.
	const UTerritoryHidingSpotComponent* Spot = GetDefault<UTerritoryHidingSpotComponent>();
	TestNotNull(TEXT("Hiding spot component exists"), Spot);
	TestTrue(TEXT("A hiding spot defaults to a crouch-sized volume"),
		Spot && Spot->EnterRadius >= 50.f);
	TestTrue(TEXT("Crouching is required by default"), Spot && Spot->bRequiresCrouch);

	const UClass* ControlClass = UTerritoryControlSubsystem::StaticClass();
	TestTrue(TEXT("Hiding registration is server-only Blueprint API"),
		IsBlueprintAuthorityFunction(ControlClass, TEXT("SetTargetHidden")));
	TestNotNull(TEXT("Hidden state is readable for HUD and AI queries"),
		ControlClass->FindFunctionByName(TEXT("IsTargetHidden")));

	const UTerritoryNoiseProfile* Noise = GetDefault<UTerritoryNoiseProfile>();
	TestNotNull(TEXT("Noise profile exists"), Noise);
	TestEqual(TEXT("Noise profile defaults to a normal throw loudness"), Noise->Loudness, 1.f);
	TestEqual(TEXT("Noise profile defaults to the distraction range"), Noise->MaximumRange, 3000.f);
	TestTrue(TEXT("Noise profile carries a stimulus tag by default"),
		Noise && Noise->NoiseTag.IsValid());
	return true;
}

#endif
