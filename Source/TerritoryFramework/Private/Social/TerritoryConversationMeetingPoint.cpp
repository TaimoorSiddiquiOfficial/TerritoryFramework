#include "Social/TerritoryConversationMeetingPoint.h"
#include "Social/TerritoryConversationProfile.h"
#include "Social/TerritoryConversationListenerComponent.h"
#include "AI/TerritoryConversationGoal.h"
#include "AI/TerritoryConversationActivity.h"
#include "AI/TerritoryAssaultGoal.h"
#include "AI/TerritoryInvestigationGoal.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "Components/SceneComponent.h"
#include "Core/TerritoryGuardCharacter.h"
#include "Core/TerritoryVolume.h"
#include "Framework/TerritoryNarrativeProAdapter.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Tales/TalesComponent.h"
#include "Tales/TerritoryAmbientDialogue.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

ATerritoryConversationMeetingPoint::ATerritoryConversationMeetingPoint()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("MeetingRoot"));
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.1f;
}

void ATerritoryConversationMeetingPoint::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
		if (auto* Saves = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
			Saves->OnBeginLoad.AddUniqueDynamic(this, &ThisClass::OnBeginLoad);
}

void ATerritoryConversationMeetingPoint::EndPlay(const EEndPlayReason::Type Reason)
{
	CancelConversation(TEXT("Meeting streamed out or world ended."));
	if (GetWorld())
		if (auto* Saves = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
			Saves->OnBeginLoad.RemoveDynamic(this, &ThisClass::OnBeginLoad);
	Super::EndPlay(Reason);
}

bool ATerritoryConversationMeetingPoint::IsEligible(ATerritoryGuardCharacter* Guard,
	const FGameplayTagContainer& RequiredTags) const
{
	if (!IsValid(Territory) || !IsValid(Profile) || !IsValid(Guard) || !Guard->HasAuthority() || !Guard->IsAlive() || Guard->IsActorBeingDestroyed()
		|| Guard->GetWorld() != GetWorld() || Guard->GetOwningTerritory() != Territory
		|| !FTerritoryNarrativeProAdapter::IsCharacterReady(Guard)
		|| Guard->GetGuardFaction() != Territory->GetOwningFaction()
		|| FVector::DistSquared(Guard->GetActorLocation(), GetActorLocation()) > FMath::Square(Profile->RecruitRadius)) return false;
	const auto* Controller = Cast<ANarrativeNPCController>(Guard->GetController());
	const auto* Activities = Guard->GetActivityComponent();
	const auto* ASC = Guard->GetNarrativeAbilitySystemComponent();
	if (!Controller || Controller->GetPawn() != Guard || !Activities || !Activities->IsActive() || !ASC
		|| !ASC->HasAllMatchingGameplayTags(RequiredTags)
		|| Activities->HasGoal(UTerritoryAssaultGoal::StaticClass())
		|| Activities->HasGoal(UTerritoryInvestigationGoal::StaticClass())) return false;
	for (const UNPCGoalItem* Goal : Activities->GetGoals(UTerritoryConversationGoal::StaticClass()).Goals)
		if (Goal != FirstGoal && Goal != SecondGoal) return false;
	const TWeakObjectPtr<ATerritoryGuardCharacter> CheckedGuard(Guard);
	const TWeakObjectPtr<ATerritoryVolume> CheckedTerritory(Territory);
	const TWeakObjectPtr<UTerritoryConversationProfile> CheckedProfile(Profile);
	const TWeakObjectPtr<ANarrativeNPCController> CheckedController(const_cast<ANarrativeNPCController*>(Controller));
	const TWeakObjectPtr<UNPCActivityComponent> CheckedActivities(const_cast<UNPCActivityComponent*>(Activities));
	const TWeakObjectPtr<UNPCGoalItem> CheckedGoal(Activities->GetCurrentActivityGoal());
	const FGuid CheckedSession = SessionID;
	if (const UNPCGoalItem* Goal = CheckedGoal.Get())
		if (Goal != FirstGoal && Goal != SecondGoal && Goal->GetGoalScore() >= 2.f) return false;
	// Goal scores are Blueprint-native callbacks: a read may destroy/repossess the
	// candidate or cancel the meeting. Admit only the exact context scored above.
	return !IsActorBeingDestroyed() && CheckedGuard.IsValid() && !Guard->IsActorBeingDestroyed()
		&& Guard->IsAlive() && Guard->HasAuthority() && CheckedTerritory.IsValid()
		&& Territory == CheckedTerritory.Get() && CheckedProfile.IsValid() && Profile == CheckedProfile.Get()
		&& Guard->GetOwningTerritory() == Territory && Guard->GetGuardFaction() == Territory->GetOwningFaction()
		&& CheckedController.IsValid() && Guard->GetController() == CheckedController.Get()
		&& CheckedController->GetPawn() == Guard && CheckedActivities.IsValid()
		&& Guard->GetActivityComponent() == CheckedActivities.Get() && CheckedActivities->IsActive()
		&& CheckedActivities->GetCurrentActivityGoal() == CheckedGoal.Get() && SessionID == CheckedSession;
}

bool ATerritoryConversationMeetingPoint::IsSessionValid() const
{
	if (!HasAuthority() || !SessionID.IsValid() || !IsValid(Territory) || !IsValid(Profile)
		|| !Territory->IsAvailableForGameplay() || Territory->GetOwningFaction() != OwnerAtStart
		|| Territory->GetTerritoryState() == ETerritoryState::Contested
		|| !Listener.IsValid() || !ListenerAdapter.IsValid()
		|| !UTerritoryConversationListenerComponent::IsSupportedListener(Listener.Get(), ListenerAdapter.Get())
		|| FVector::DistSquared(Listener->GetOwningPawn()->GetActorLocation(), GetActorLocation()) > FMath::Square(Profile->ListenerRadius)
		|| !IsEligible(FirstGuard, Profile->FirstRoleTags) || !IsEligible(SecondGuard, Profile->SecondRoleTags)) return false;
	return FirstGuard != SecondGuard;
}

bool ATerritoryConversationMeetingPoint::OwnsGoal(const UTerritoryConversationGoal* Goal,
	const ATerritoryGuardCharacter* Guard) const
{
	return HasAuthority() && !bCancelling && SessionID.IsValid() && IsValid(Goal)
		&& Goal->SessionID == SessionID && Phase != ETerritoryConversationPhase::Idle
		&& ((Goal == FirstGoal && Guard == FirstGuard) || (Goal == SecondGoal && Guard == SecondGuard));
}

bool ATerritoryConversationMeetingPoint::AddMeetingGoal(ATerritoryGuardCharacter* Guard,
	const FVector& Location, UTerritoryConversationGoal*& OutGoal)
{
	OutGoal = nullptr;
	auto* Activities = Guard->GetActivityComponent();
	if (!Activities->GetActivity(UTerritoryConversationActivity::StaticClass())
		&& !Activities->AddActivity(UTerritoryConversationActivity::StaticClass(), false)) return false;
	auto* Goal = NewObject<UTerritoryConversationGoal>(Activities);
	Goal->Meeting = this; Goal->SessionID = SessionID; Goal->GoalKey = this;
	Goal->Destination = Location; Goal->ArrivalRadius = Profile->ArrivalRadius;
	Goal->GoalLifetime = Profile->MeetingTimeout + Profile->PreparationTimeout + Profile->MaximumDialogueDuration;
	if (Guard == FirstGuard) FirstGoal = Goal; else SecondGoal = Goal;
	const FGuid ExpectedSession = SessionID;
	if (Activities->AddGoal(Goal, true) != Goal || !IsSessionValid() || SessionID != ExpectedSession
		|| !Activities->GetGoals(UTerritoryConversationGoal::StaticClass()).Goals.Contains(Goal)) return false;
	OutGoal = Goal;
	return true;
}

bool ATerritoryConversationMeetingPoint::TryStartConversation(UTalesComponent* ExplicitListener, FString& Failure)
{
	Failure.Reset();
	if (!HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown || IsActorBeingDestroyed())
		{ Failure = TEXT("Only the live authority can start a meeting."); return false; }
	if (bCancelling || bStartingConversation || Phase != ETerritoryConversationPhase::Idle || GetWorld()->GetTimeSeconds() < NextAllowedTime)
		{ Failure = TEXT("Meeting already has its one listener or is cooling down."); return false; }
	TGuardValue<bool> Starting(bStartingConversation, true);
	if (!IsValid(Territory) || !Territory->IsAvailableForGameplay() || !Territory->GetOwningFaction().IsValid()
		|| Territory->GetTerritoryState() == ETerritoryState::Contested || !IsValid(Profile))
		{ Failure = TEXT("A ready, owned, uncontested Place and conversation profile are required."); return false; }
	if (!Profile->Validate(Failure)) return false;
	if (FirstOffset.ContainsNaN() || SecondOffset.ContainsNaN()
		|| FVector::DistSquared(FirstOffset, SecondOffset) < FMath::Square(2.f * Profile->ArrivalRadius))
		{ Failure = TEXT("Meeting positions must be finite and separated by two arrival radii."); return false; }
	APlayerController* PC = IsValid(ExplicitListener) ? ExplicitListener->GetOwningController() : nullptr;
	auto* Adapter = PC ? PC->FindComponentByClass<UTerritoryConversationListenerComponent>() : nullptr;
	if (!Adapter || !UTerritoryConversationListenerComponent::IsSupportedListener(ExplicitListener, Adapter)
		|| ExplicitListener->GetCurrentDialogue() || Adapter->HasPreparedConversation()
		|| ExplicitListener->GetWorld() != GetWorld()
		|| FVector::DistSquared(ExplicitListener->GetOwningPawn()->GetActorLocation(), GetActorLocation()) > FMath::Square(Profile->ListenerRadius))
		{ Failure = TEXT("Supply one nearby personal Tales listener with its controller's conversation component; parties and busy listeners are unsupported."); return false; }
	TArray<ATerritoryGuardCharacter*> Candidates;
	for (TActorIterator<ATerritoryGuardCharacter> It(GetWorld()); It; ++It)
		if (IsEligible(*It, FGameplayTagContainer())) Candidates.Add(*It);
	Candidates.Sort([](const ATerritoryGuardCharacter& A, const ATerritoryGuardCharacter& B) { return A.GetPathName() < B.GetPathName(); });
	for (ATerritoryGuardCharacter* A : Candidates)
	{
		if (!IsEligible(A, Profile->FirstRoleTags)) continue;
		for (ATerritoryGuardCharacter* B : Candidates)
		{
			if (B == A || !IsEligible(B, Profile->SecondRoleTags)) continue;
			FirstGuard = A; SecondGuard = B; break;
		}
		if (FirstGuard && SecondGuard) break;
	}
	if (!FirstGuard || !SecondGuard)
		{ FirstGuard = nullptr; SecondGuard = nullptr; Failure = TEXT("Two distinct ready, idle defenders matching the role filters were not found."); return false; }
	if (!IsValid(Territory) || IsActorBeingDestroyed() || !IsEligible(FirstGuard, Profile->FirstRoleTags)
		|| !IsEligible(SecondGuard, Profile->SecondRoleTags))
		{ FirstGuard = nullptr; SecondGuard = nullptr; Failure = TEXT("A role-selection callback invalidated a candidate."); return false; }
	SessionID = FGuid::NewGuid(); OwnerAtStart = Territory->GetOwningFaction();
	Listener = ExplicitListener; ListenerAdapter = Adapter; Phase = ETerritoryConversationPhase::Staging;
	Deadline = GetWorld()->GetTimeSeconds() + Profile->MeetingTimeout;
	LastStopReason.Reset(); DeferredStopReason.Reset();
	BindGuard(FirstGuard, true); BindGuard(SecondGuard, true);
	SetActorTickEnabled(true);
	UTerritoryConversationGoal* Added = nullptr;
	if (!AddMeetingGoal(FirstGuard, GetActorTransform().TransformPosition(FirstOffset), Added)
		|| !AddMeetingGoal(SecondGuard, GetActorTransform().TransformPosition(SecondOffset), Added))
	{
		Failure = TEXT("Native activity admission rejected or invalidated a participant.");
		CancelConversation(Failure); return false;
	}
	return true;
}

void ATerritoryConversationMeetingPoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Phase == ETerritoryConversationPhase::Idle) return;
	if (!IsSessionValid()) { CancelConversation(TEXT("Participant, listener, owner, readiness or threat changed.")); return; }
	if (GetWorld()->GetTimeSeconds() >= Deadline) { CancelConversation(TEXT("Conversation phase timed out.")); return; }
	const auto Admitted = [](ATerritoryGuardCharacter* Guard, UTerritoryConversationGoal* Goal)
	{
		return IsValid(Goal) && Guard->GetActivityComponent()->GetGoals(UTerritoryConversationGoal::StaticClass()).Goals.Contains(Goal)
			&& Guard->GetActivityComponent()->GetCurrentActivityGoal() == Goal;
	};
	if (!Admitted(FirstGuard, FirstGoal) || !Admitted(SecondGuard, SecondGoal))
		{ CancelConversation(TEXT("Native activity interrupted or removed a meeting goal.")); return; }
	if (Phase == ETerritoryConversationPhase::Playing)
	{
		if (!PlayingDialogue.IsValid() || Listener->GetCurrentDialogue() != PlayingDialogue.Get())
			CancelConversation(TEXT("Native dialogue finished or was replaced."));
		return;
	}
	if (Listener->GetCurrentDialogue()) { CancelConversation(TEXT("Listener began another dialogue.")); return; }
	if (Phase != ETerritoryConversationPhase::Staging) return;
	const auto Arrived = [](ATerritoryGuardCharacter* Guard, const UTerritoryConversationGoal* Goal)
	{
		return FVector::DistSquared2D(Guard->GetActorLocation(), Goal->Destination) <= FMath::Square(Goal->ArrivalRadius)
			&& FMath::Abs(Guard->GetActorLocation().Z - Goal->Destination.Z) <= 150.f;
	};
	if (Arrived(FirstGuard, FirstGoal) && Arrived(SecondGuard, SecondGoal))
	{
		Phase = ETerritoryConversationPhase::PreparingListener;
		Deadline = GetWorld()->GetTimeSeconds() + Profile->PreparationTimeout;
		if (!ListenerAdapter->Prepare(this, Listener.Get(), SessionID, Profile->Dialogue, FirstGuard, SecondGuard))
			CancelConversation(TEXT("Listener preparation was rejected."));
	}
}

void ATerritoryConversationMeetingPoint::ListenerPrepared(const FGuid& Session, bool bReady)
{
	if (!HasAuthority() || Session != SessionID || Phase != ETerritoryConversationPhase::PreparingListener) return;
	if (!bReady || !IsSessionValid() || GetWorld()->GetTimeSeconds() >= Deadline
		|| FirstGuard->GetActivityComponent()->GetCurrentActivityGoal() != FirstGoal
		|| SecondGuard->GetActivityComponent()->GetCurrentActivityGoal() != SecondGoal)
		{ CancelConversation(TEXT("Listener did not acknowledge the current roles in time.")); return; }
	Phase = ETerritoryConversationPhase::Playing;
	Deadline = GetWorld()->GetTimeSeconds() + Profile->MaximumDialogueDuration;
	if (!ListenerAdapter->BeginPrepared(Session)) { CancelConversation(TEXT("Native Tales rejected conversation start.")); return; }
	if (SessionID != Session || !IsSessionValid()) { CancelConversation(TEXT("Conversation start callback invalidated participants.")); return; }
	PlayingDialogue = ListenerAdapter->GetPreparedDialogue(Session);
	if (!DeferredStopReason.IsEmpty()) { const FString Reason = DeferredStopReason; DeferredStopReason.Reset(); CancelConversation(Reason); return; }
	if (!PlayingDialogue.IsValid()) CancelConversation(TEXT("Native dialogue completed during start."));
}

void ATerritoryConversationMeetingPoint::CancelConversation(const FString& Reason)
{
	if (!HasAuthority() || bCancelling || !SessionID.IsValid()) return;
	if (ListenerAdapter.IsValid() && ListenerAdapter->IsStartingNativeDialogue(SessionID))
	{
		// Native BeginDialogue dereferences CurrentDialogue after OnDialogueBegan.
		// Releasing inside that callback would crash its client dispatch. The caller
		// completes this cancellation immediately after the Native begin stack returns.
		DeferredStopReason = Reason;
		return;
	}
	TGuardValue<bool> Cancelling(bCancelling, true);
	const FGuid ReleasedSession = SessionID;
	const TWeakObjectPtr<UTalesComponent> ReleasedListener = Listener;
	const TWeakObjectPtr<UTerritoryConversationListenerComponent> ReleasedAdapter = ListenerAdapter;
	const TWeakObjectPtr<UDialogue> ReleasedDialogue = PlayingDialogue.IsValid() ? PlayingDialogue
		: TWeakObjectPtr<UDialogue>(ReleasedAdapter.IsValid() ? ReleasedAdapter->GetPreparedDialogue(ReleasedSession) : nullptr);
	const TWeakObjectPtr<ATerritoryGuardCharacter> ReleasedFirst = FirstGuard;
	const TWeakObjectPtr<ATerritoryGuardCharacter> ReleasedSecond = SecondGuard;
	const TWeakObjectPtr<UTerritoryConversationGoal> ReleasedFirstGoal = FirstGoal;
	const TWeakObjectPtr<UTerritoryConversationGoal> ReleasedSecondGoal = SecondGoal;
	SessionID.Invalidate(); Phase = ETerritoryConversationPhase::Idle;
	SetActorTickEnabled(false); LastStopReason = Reason;
	NextAllowedTime = GetWorld() ? GetWorld()->GetTimeSeconds() + (Profile ? Profile->Cooldown : 0.f) : 0.;
	BindGuard(ReleasedFirst.Get(), false); BindGuard(ReleasedSecond.Get(), false);
	if (ReleasedListener.IsValid())
	{
		UDialogue* Current = ReleasedListener->GetCurrentDialogue();
		if (Current && Current == ReleasedDialogue.Get())
			ReleasedListener->ExitDialogue(EExitDialogueReason::EDR_PlayerExited);
	}
	if (ReleasedAdapter.IsValid()) ReleasedAdapter->Release(ReleasedSession);
	// Never stop a replacement combat activity or clear another system's goals.
	for (UTerritoryConversationGoal* Goal : {ReleasedFirstGoal.Get(), ReleasedSecondGoal.Get()})
		if (IsValid(Goal) && Goal->SessionID == ReleasedSession)
			if (auto* Activities = Cast<UNPCActivityComponent>(Goal->GetOuter()))
				if (Activities->GetGoals(UTerritoryConversationGoal::StaticClass()).Goals.Contains(Goal)) Activities->RemoveGoal(Goal);
	FirstGoal = nullptr; SecondGoal = nullptr; FirstGuard = nullptr; SecondGuard = nullptr;
	Listener.Reset(); ListenerAdapter.Reset(); PlayingDialogue.Reset(); OwnerAtStart = FGameplayTag();
}

void ATerritoryConversationMeetingPoint::BindGuard(ATerritoryGuardCharacter* Guard, bool bBind)
{
	if (IsValid(Guard))
		if (auto* ASC = Guard->GetNarrativeAbilitySystemComponent())
		{
			if (bBind)
			{
				ASC->OnDamagedBy.AddUniqueDynamic(this, &ThisClass::OnGuardDamaged);
				ASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::OnGuardDeath);
			}
			else
			{
				ASC->OnDamagedBy.RemoveDynamic(this, &ThisClass::OnGuardDamaged);
				ASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::OnGuardDeath);
			}
		}
}

void ATerritoryConversationMeetingPoint::OnBeginLoad() { CancelConversation(TEXT("Native load cancels unfinished conversations.")); }
void ATerritoryConversationMeetingPoint::OnGuardDamaged(UNarrativeAbilitySystemComponent* Source, float Damage, const FGameplayEffectSpec& Spec)
{
	if (FMath::IsFinite(Damage) && Damage > 0.f) CancelConversation(TEXT("A participant took damage."));
}
void ATerritoryConversationMeetingPoint::OnGuardDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bDead)
{
	if (bDead) CancelConversation(TEXT("A participant died."));
}
