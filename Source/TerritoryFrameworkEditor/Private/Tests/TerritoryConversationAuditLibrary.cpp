#include "Tests/TerritoryConversationAuditLibrary.h"
#include "Social/TerritoryConversationMeetingPoint.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameplayEffect.h"
#include "Tales/TalesComponent.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "TimerManager.h"

FString UTerritoryConversationAuditLibrary::GetActorNetworkIdentityForPIE(AActor* Actor)
{
	if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return FString();
	const UWorld* World = Actor->GetWorld();
	if (!World || World->WorldType != EWorldType::PIE || World->bIsTearingDown) return FString();
	const UNetDriver* Driver = Actor->GetNetDriver();
	if (!Driver || Driver->GetWorld() != World || Driver->IsUsingIrisReplication()) return FString();
	const TSharedPtr<FNetGUIDCache>& Cache = Driver->GetNetGuidCache();
	if (!Cache.IsValid()) return FString();
	const FNetworkGUID Identity = Cache->GetNetGUID(Actor);
	return Identity.IsValid() && !Identity.IsDefault() ? Identity.ToString() : FString();
}

bool UTerritoryConversationAuditLibrary::ScheduleConversationForPIE(ATerritoryConversationMeetingPoint* Meeting, UTalesComponent* ExplicitListener)
{
	if (!IsValid(Meeting) || !Meeting->HasAuthority() || !IsValid(ExplicitListener)
		|| !Meeting->GetWorld() || Meeting->GetWorld()->WorldType != EWorldType::PIE
		|| ExplicitListener->GetWorld() != Meeting->GetWorld()) return false;
	TWeakObjectPtr<ATerritoryConversationMeetingPoint> WeakMeeting(Meeting);
	TWeakObjectPtr<UTalesComponent> WeakListener(ExplicitListener);
	Meeting->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakMeeting, WeakListener]()
	{
		if (WeakMeeting.IsValid() && WeakListener.IsValid())
		{
			FString Failure;
			if (!WeakMeeting->TryStartConversation(WeakListener.Get(), Failure)) WeakMeeting->LastStopReason = Failure;
		}
	}));
	return true;
}

bool UTerritoryConversationAuditLibrary::ScheduleConversationCancelForPIE(ATerritoryConversationMeetingPoint* Meeting)
{
	if (!IsValid(Meeting) || !Meeting->HasAuthority() || !Meeting->GetWorld()
		|| Meeting->GetWorld()->WorldType != EWorldType::PIE) return false;
	TWeakObjectPtr<ATerritoryConversationMeetingPoint> WeakMeeting(Meeting);
	Meeting->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakMeeting]()
	{
		if (WeakMeeting.IsValid()) WeakMeeting->CancelConversation(TEXT("Explicit editor audit cancellation."));
	}));
	return true;
}

bool UTerritoryConversationAuditLibrary::ScheduleGuardDamageForPIE(ATerritoryGuardCharacter* Guard, AActor* ExactSource, float Damage)
{
	if (!IsValid(Guard) || !Guard->HasAuthority() || !IsValid(ExactSource) || ExactSource == Guard
		|| !Guard->GetWorld() || Guard->GetWorld()->WorldType != EWorldType::PIE || ExactSource->GetWorld() != Guard->GetWorld()
		|| !FMath::IsFinite(Damage) || Damage <= 0.f) return false;
	TWeakObjectPtr<ATerritoryGuardCharacter> WeakGuard(Guard);
	TWeakObjectPtr<AActor> WeakSource(ExactSource);
	Guard->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakGuard, WeakSource, Damage]()
	{
		if (!WeakGuard.IsValid() || !WeakSource.IsValid() || !WeakGuard->HasAuthority()) return;
		auto* SourceASC = FTerritoryNarrativeProAdapter::ResolveAbilitySystem(WeakSource.Get());
		auto* TargetASC = WeakGuard->GetNarrativeAbilitySystemComponent();
		if (!SourceASC || !TargetASC) return;
		auto* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UNarrativeAttributeSetBase::GetDamageAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FScalableFloat(Damage);
		Effect->Modifiers.Add(Modifier);
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(WeakSource.Get(), WeakSource.Get());
		FGameplayEffectSpec Spec(Effect, Context, 1.f);
		SourceASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC);
	}));
	return true;
}
