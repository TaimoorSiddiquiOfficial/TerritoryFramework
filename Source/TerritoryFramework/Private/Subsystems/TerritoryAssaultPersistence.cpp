#include "Subsystems/TerritoryCounterAttackSubsystem.h"

#include "Combat/TerritoryAssaultCharacter.h"
#include "Combat/TerritoryAssaultParticipantComponent.h"
#include "Combat/TerritoryCounterAttackProfile.h"
#include "Core/TerritoryDeveloperSettings.h"
#include "Core/TerritoryVolume.h"
#include "Interaction/InteractionComponent.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Navigation/TerritoryRoadTrafficSubsystem.h"
#include "Vehicles/MountComponent.h"
#include "Vehicles/NarrativeVehicleBase.h"

namespace
{
	constexpr int32 MaximumSavedParticipants = 1024;
	constexpr int32 MaximumSavedVehicles = 8;
	constexpr int32 MaximumSavedRoutePoints = 32768;

	bool ValidTransform(const FTransform& Transform)
	{
		return !Transform.ContainsNaN() && Transform.IsValid()
			&& Transform.GetLocation().GetAbsMax() < 1.e9;
	}
}

TArray<FTerritoryAssaultRecord> UTerritoryCounterAttackSubsystem::GetPersistentState() const
{
	TArray<FTerritoryAssaultRecord> Result = GetAllAssaults();
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client) return Result;
	for (FTerritoryAssaultRecord& Record : Result)
	{
		// Pending manifests are already durable. Append admitted actors without changing
		// the live record or invoking Narrative's save callbacks from this read method.
		if (const auto* Participants = LiveParticipants.Find(Record.AssaultID))
		{
			for (const auto& WeakNPC : *Participants)
			{
				const ATerritoryAssaultCharacter* NPC = WeakNPC.Get();
				if (!IsValid(NPC) || NPC->IsActorBeingDestroyed() || !NPC->AssaultParticipant
					|| NPC->AssaultParticipant->HasRetired()) continue;
				FTerritoryAssaultSurvivor Member;
				Member.SpawnGUID = NPC->GetActorGUID_Implementation();
				Member.ApproachID = NPC->GetAssaultApproachID();
				Member.Transform = NPC->GetActorTransform();
				if (ANarrativeVehicleBase* Vehicle = NPC->AssaultParticipant->GetPendingIngressVehicle())
				{
					for (const auto& Pair : PhysicalVehicles)
					{
						if (Pair.Value.Get() != Vehicle) continue;
						Member.VehicleID = Pair.Key;
						Member.SeatIndex = NPC->AssaultParticipant->GetIngressSeatIndex();
						if (auto* Checkpoint = Record.VehicleCheckpoints.FindByPredicate(
							[&Member](const auto& Entry) { return Entry.VehicleID == Member.VehicleID; }))
						{
							// Only the driver advances the route cursor.
							if (Member.SeatIndex == 0) NPC->AssaultParticipant->UpdateVehicleCheckpoint(*Checkpoint);
						}
						break;
					}
				}
				Record.PendingSurvivors.Add(Member);
			}
		}
		Record.VehicleCheckpoints.RemoveAll([&Record](const auto& Vehicle)
		{
			return !Record.PendingSurvivors.ContainsByPredicate(
				[&Vehicle](const auto& Member) { return Member.VehicleID == Vehicle.VehicleID; });
		});
		for (auto& Checkpoint : Record.VehicleCheckpoints)
		{
			ANarrativeVehicleBase* Vehicle = PhysicalVehicles.FindRef(Checkpoint.VehicleID).Get();
			// A just-restored campaign may still be retiring its old physical actors.
			if (IsValid(Vehicle) && !Vehicle->IsActorBeingDestroyed() && !Vehicle->IsHidden())
			{
				Checkpoint.Transform = Vehicle->GetActorTransform();
				Checkpoint.HealthFraction = Vehicle->GetMaxHealth() > 0.f
					? FMath::Clamp(Vehicle->GetHealth() / Vehicle->GetMaxHealth(), 0.f, 1.f) : 1.f;
			}
		}
		Record.PendingSurvivors.Sort([](const auto& A, const auto& B)
		{
			if (A.VehicleID != B.VehicleID) return A.VehicleID.ToString() < B.VehicleID.ToString();
			return A.SeatIndex == B.SeatIndex ? A.SpawnGUID.ToString() < B.SpawnGUID.ToString() : A.SeatIndex < B.SeatIndex;
		});
		Record.PhysicalStateVersion = 1;
	}
	return Result;
}

void UTerritoryCounterAttackSubsystem::NormalizePhysicalCheckpoint(FTerritoryAssaultRecord& Record)
{
	const int32 Remaining = Record.PlannedForce - Record.KilledForce - Record.WithdrawnForce;
	const bool bPhysical = Record.State == ETerritoryAssaultState::Active
		|| Record.State == ETerritoryAssaultState::RecaptureCountdown;
	if (!bPhysical)
	{
		Record.PendingSurvivors.Reset();
		Record.VehicleCheckpoints.Reset();
		Record.LegacySurvivorsToRestore = 0;
		Record.LegacyVehicleRestoreCredits.Reset();
		Record.PhysicalStateVersion = 1;
		return;
	}
	if (Record.PhysicalStateVersion == 0)
	{
		Record.LegacySurvivorsToRestore = FMath::Clamp(Record.AliveForce, 0, FMath::Min(Remaining, MaximumSavedParticipants));
		Record.LegacyVehicleRestoreCredits = Record.VehicleDeploymentsByApproach;
		Record.PendingSurvivors.Reset();
		Record.VehicleCheckpoints.Reset();
	}
	Record.PhysicalStateVersion = 1;
	TSet<FGuid> VehicleIDs;
	Record.VehicleCheckpoints.RemoveAll([&VehicleIDs, &Record](auto& Vehicle)
	{
		if (!Vehicle.VehicleID.IsValid() || VehicleIDs.Contains(Vehicle.VehicleID)
			|| VehicleIDs.Num() >= FMath::Clamp(Record.VehicleDeploymentsUsed, 0, MaximumSavedVehicles)
			|| Vehicle.ApproachID.IsNone() || Vehicle.VehicleClass.IsNull()
			|| !ValidTransform(Vehicle.Transform) || !ValidTransform(Vehicle.ParkDestination)
			|| !ValidTransform(Vehicle.WalkDestination) || Vehicle.RoutePoints.Num() < 2
			|| Vehicle.RoutePoints.Num() > MaximumSavedRoutePoints
			|| Vehicle.RoutePoints.ContainsByPredicate([](const FVector& Point) { return Point.ContainsNaN() || Point.GetAbsMax() >= 1.e9; })) return true;
		Vehicle.HealthFraction = FMath::IsFinite(Vehicle.HealthFraction) ? FMath::Clamp(Vehicle.HealthFraction, 0.f, 1.f) : 0.f;
		VehicleIDs.Add(Vehicle.VehicleID);
		return false;
	});
	TSet<FGuid> SpawnIDs;
	const int32 OriginalManifestCount = FMath::Min(Record.PendingSurvivors.Num(), Remaining);
	Record.PendingSurvivors.RemoveAll([&](auto& Member)
	{
		if (!Member.SpawnGUID.IsValid() || SpawnIDs.Contains(Member.SpawnGUID)
			|| SpawnIDs.Num() >= FMath::Min(Remaining, MaximumSavedParticipants)
			|| Member.ApproachID.IsNone() || !ValidTransform(Member.Transform)
			|| (Member.VehicleID.IsValid() && !VehicleIDs.Contains(Member.VehicleID))) return true;
		Member.SeatIndex = FMath::Clamp(Member.SeatIndex, 0, 31);
		SpawnIDs.Add(Member.SpawnGUID);
		return false;
	});
	// Invalid saved physical slots are withdrawn, never converted into fresh reserve.
	Record.WithdrawnForce += FMath::Max(0, OriginalManifestCount - Record.PendingSurvivors.Num());
	Record.LegacySurvivorsToRestore = FMath::Clamp(Record.LegacySurvivorsToRestore, 0,
		FMath::Min(MaximumSavedParticipants, Remaining - OriginalManifestCount));
	int32 CreditBudget = FMath::Clamp(Record.VehicleDeploymentsUsed - VehicleIDs.Num(), 0, MaximumSavedVehicles);
	TSet<FName> CreditedApproaches;
	Record.LegacyVehicleRestoreCredits.RemoveAll([&](auto& Credit)
	{
		if (Credit.ApproachID.IsNone() || CreditedApproaches.Contains(Credit.ApproachID) || CreditBudget <= 0) return true;
		Credit.Count = FMath::Clamp(Credit.Count, 0, CreditBudget);
		CreditBudget -= Credit.Count;
		CreditedApproaches.Add(Credit.ApproachID);
		return Credit.Count == 0;
	});
}

TArray<ATerritoryAssaultCharacter*> UTerritoryCounterAttackSubsystem::ReconstructParticipants(
	FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory)
{
	TArray<ATerritoryAssaultCharacter*> Spawned;
	if (bReconstructingParticipants || bRestoringState || !GetWorld() || GetWorld()->GetNetMode() == NM_Client
		|| !IsValid(Territory) || !Territory->HasAuthority() || Assault.IsTerminal()) return Spawned;
	TGuardValue<bool> ReconstructionGuard(bReconstructingParticipants, true);
	const FAssaultAccess Access = CaptureAssaultAccess(Assault);
	const auto Current = [&]() { return IsAssaultCurrent(Access) && !Assault.IsTerminal() && IsValid(Territory); };
	const auto* Profile = Territory->GetCounterAttackProfile();
	const auto* AuthoredForce = Profile ? Profile->FindFactionForce(Assault.AttackingFaction) : nullptr;
	if (!AuthoredForce) return Spawned;
	const FTerritoryFactionAssaultConfig Force = *AuthoredForce;
	UNPCDefinition* Definition = Assault.LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
		&& !Assault.StoryAttackerDefinitionOverride.IsNull() ? Assault.StoryAttackerDefinitionOverride.LoadSynchronous() : Force.AttackerDefinition.Get();
	if (!Current() || !Definition) return Spawned;
	const int32 OverrideLevel = ResolveScaledEnemyLevel(Assault, Territory, Force);
	const auto Pending = Assault.PendingSurvivors;
	bool bWaitingForOldActors = false;
	bool bAttempted = false;
	TSet<FGuid> FailedVehicleRestores;
	for (FTerritoryAssaultSurvivor Member : Pending)
	{
		const auto* Settings = GetDefault<UTerritoryDeveloperSettings>();
		if (Settings && CountLiveParticipants() >= Settings->MaxLiveCounterAttackNPCs) break;
		if (!Assault.PendingSurvivors.ContainsByPredicate([&Member](const auto& Entry) { return Entry.SpawnGUID == Member.SpawnGUID; })) continue;
		bool bOldNPCExists = false;
		for (TActorIterator<ATerritoryAssaultCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetActorGUID_Implementation() == Member.SpawnGUID)
			{
				bOldNPCExists = true;
				bAttempted = true;
				bWaitingForOldActors |= It->AssaultParticipant && It->AssaultParticipant->HasRetired()
					&& It->GetLifeSpan() > 0.f && It->GetLifeSpan() <= 1.f;
				break;
			}
		}
		// Narrative removes its GUID lookup on Destroy. Wait for that removal before
		// reusing the original key, including the brief latent-dismount retirement.
		if (bOldNPCExists) continue;
		bAttempted = true;
		// The original departure can be streamed out after a car has crossed the city.
		// Resolve its authored policy, then use the saved physical position/route.
		const auto* AuthoredApproach = Territory->GetCounterAttackApproaches().FindByPredicate(
			[&Member](const auto& Entry) { return Entry.bEnabled && Entry.ApproachID == Member.ApproachID; });
		if (!AuthoredApproach) continue;
		const FTerritoryAssaultApproach Approach = *AuthoredApproach;
		FTerritoryAssaultVehicleCheckpoint VehicleState;
		ANarrativeVehicleBase* Vehicle = nullptr;
		UTerritoryAssaultParticipantComponent* Driver = nullptr;
		if (Member.VehicleID.IsValid())
		{
			if (FailedVehicleRestores.Contains(Member.VehicleID)) continue;
			const auto* SavedVehicle = Assault.VehicleCheckpoints.FindByPredicate([&Member](const auto& Entry) { return Entry.VehicleID == Member.VehicleID; });
			if (!SavedVehicle) continue;
			VehicleState = *SavedVehicle;
			// The original driver may have died, leaving no live route cursor to save.
			// Project the saved car onto its remaining route before any remount/drive.
			if (!UTerritoryRoadTrafficSubsystem::TrimRouteToDeparture(VehicleState.RoutePoints,
				VehicleState.Transform.GetLocation(), 600.f))
			{
				if (VehicleState.RoutePoints.Num() < 2 || FVector::DistSquared(
					VehicleState.Transform.GetLocation(), VehicleState.RoutePoints.Last()) > FMath::Square(600.f)) continue;
				// At the final parking point: keep a short terminal segment so Native
				// ingress finishes/dismounts instead of driving back to the departure.
				const FVector End = VehicleState.RoutePoints.Last();
				VehicleState.RoutePoints = {End - VehicleState.Transform.GetRotation().GetForwardVector() * 100.f, End};
			}
			Vehicle = PhysicalVehicles.FindRef(Member.VehicleID).Get();
			if (IsValid(Vehicle) && (Vehicle->IsHidden() || Vehicle->IsActorBeingDestroyed()))
			{ bWaitingForOldActors = true; continue; }
			if (IsValid(Vehicle) && HasPlayerVehicleOccupant(Vehicle))
			{
				// Preserve the player's physical car and restore its attackers beside it.
				// This deployment is already paid; no replacement car is authorized.
				Member.Transform.SetLocation(Vehicle->GetActorLocation() + Vehicle->GetActorRightVector() * (450.f + Member.SeatIndex * 250.f));
				Member.VehicleID.Invalidate();
				Vehicle = nullptr;
			}
			else if (VehicleState.HealthFraction <= 0.f)
			{
				Member.Transform.SetLocation(VehicleState.Transform.GetLocation() + VehicleState.Transform.GetRotation().GetRightVector() * (450.f + Member.SeatIndex * 250.f));
				Member.VehicleID.Invalidate();
				Vehicle = nullptr;
			}
			else if (!IsValid(Vehicle))
			{
				UClass* VehicleClass = VehicleState.VehicleClass.LoadSynchronous();
				if (!Current()) return Spawned;
				if (!VehicleClass || !VehicleClass->IsChildOf(ANarrativeVehicleBase::StaticClass())) continue;
				FActorSpawnParameters Params;
				Params.ObjectFlags |= RF_Transient;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
				Vehicle = GetWorld()->SpawnActor<ANarrativeVehicleBase>(VehicleClass, VehicleState.Transform, Params);
				if (!Current()) { if (IsValid(Vehicle)) Vehicle->Destroy(); return Spawned; }
				if (!IsValid(Vehicle))
				{
					// The old physical pose can now intersect debris or traffic. Reuse the
					// same finite deployment on its remaining route, with one bounded retry.
					const auto* Traffic = GetWorld()->GetSubsystem<UTerritoryRoadTrafficSubsystem>();
					FTransform Departure = VehicleState.Transform;
					if (Traffic && Traffic->ResolveBlockedDeparture(VehicleState.RoutePoints, Departure)
						&& UTerritoryRoadTrafficSubsystem::TrimRouteToDeparture(VehicleState.RoutePoints, Departure.GetLocation()))
					{
						VehicleState.Transform = Departure;
						Vehicle = GetWorld()->SpawnActor<ANarrativeVehicleBase>(VehicleClass, Departure, Params);
						if (!Current()) { if (IsValid(Vehicle)) Vehicle->Destroy(); return Spawned; }
					}
					if (!IsValid(Vehicle)) { FailedVehicleRestores.Add(Member.VehicleID); continue; }
				}
				// Every pending passenger and any save callback must see the same recovered route.
				if (auto* Updated = Assault.VehicleCheckpoints.FindByPredicate(
					[&Member](const auto& Entry) { return Entry.VehicleID == Member.VehicleID; }))
				{
					VehicleState.Transform = Vehicle->GetActorTransform();
					*Updated = VehicleState;
				}
				Vehicle->SetVehicleSaveGuid(FGuid());
				PhysicalVehicles.Add(Member.VehicleID, Vehicle);
				LiveAssaultVehicles.FindOrAdd(Assault.AssaultID).Add(Vehicle);
				LiveVehicleRetirementRules.Add(Vehicle, Approach.VehicleRetirement);
				if (VehicleState.HealthFraction < 1.f)
				{
					UAbilitySystemComponent* ASC = Vehicle->GetAbilitySystemComponent();
					if (!ASC || !ASC->HasAttributeSetForAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())
						|| Vehicle->GetMaxHealth() <= 0.f)
					{
						// An unusable Native ASC cannot silently turn a damaged car into a fresh one.
						LiveAssaultVehicles.FindOrAdd(Assault.AssaultID).Remove(Vehicle);
						LiveVehicleRetirementRules.Remove(Vehicle);
						PhysicalVehicles.Remove(Member.VehicleID);
						Vehicle->Destroy();
						if (!Current()) return Spawned;
						continue;
					}
					ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), Vehicle->GetMaxHealth() * VehicleState.HealthFraction);
				}
				if (!Current()) return Spawned;
			}
			if (Vehicle)
			{
				const UMountComponent* Mount = Vehicle->FindComponentByClass<UMountComponent>();
				if (!Mount || Mount->InteractionSlots.IsEmpty()) continue;
				if (const auto* Live = LiveParticipants.Find(Assault.AssaultID))
					for (const auto& WeakNPC : *Live)
						if (const auto* NPC = WeakNPC.Get(); NPC && NPC->AssaultParticipant
							&& NPC->AssaultParticipant->GetPendingIngressVehicle() == Vehicle
							&& NPC->AssaultParticipant->GetIngressSeatIndex() == 0) Driver = NPC->AssaultParticipant;
				// Elect a living passenger as driver when the original driver was killed.
				if (!Driver) Member.SeatIndex = 0;
				if (Member.SeatIndex >= Mount->InteractionSlots.Num()) continue;
				const float Angle = PI * 0.5f + 2.f * PI * Member.SeatIndex / Mount->InteractionSlots.Num();
				Member.Transform = Vehicle->GetActorTransform();
				Member.Transform.SetLocation(Vehicle->GetActorLocation() +
					(Vehicle->GetActorForwardVector() * FMath::Cos(Angle) + Vehicle->GetActorRightVector() * FMath::Sin(Angle)) * FMath::Max(350.f, Profile->ParticipantSpacing));
			}
		}
		if (!Vehicle)
		{
			FVector Objective;
			if (!FindReachableObjective(Territory, Member.Transform.GetLocation(), Objective)) continue;
		}
		ATerritoryAssaultCharacter* NPC = SpawnParticipant(Assault, Territory, Force, Definition, Approach, Member.Transform, OverrideLevel, Member.SpawnGUID);
		if (!Current()) return Spawned;
		if (!IsValid(NPC)) continue;
		if (Vehicle && IsValid(Vehicle))
		{
			const bool bEscape = Assault.LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
				&& Assault.StoryPursuitDirection == ETerritoryStoryPursuitDirection::PlayerChasesEnemy;
			if (Driver && !Driver->HasRetired())
				NPC->AssaultParticipant->ConfigureNarrativeVehiclePassenger(Driver, Vehicle, Member.SeatIndex,
					VehicleState.RoutePoints, VehicleState.ParkDestination, VehicleState.WalkDestination,
					Approach.VehicleIngressTimeoutSeconds, bEscape);
			else
				NPC->AssaultParticipant->ConfigureNarrativeVehicleIngress(Vehicle, VehicleState.RoutePoints,
					VehicleState.ParkDestination, VehicleState.WalkDestination, Approach.VehicleMaximumDriveSpeed,
					Approach.VehicleIngressTimeoutSeconds, bEscape, Approach.VehicleAwareness,
					bEscape ? Assault.StoryMaximumChaseDistance : 0.f, bEscape ? Assault.StoryChaseDistanceGraceSeconds : 0.f,
					bEscape && Assault.bStoryAbandonDamagedVehicleForFinalFight, Assault.StoryVehicleAbandonHealthFraction);
		}
		Spawned.Add(NPC);
	}
	if (!Current()) return Spawned;
	if (!Spawned.IsEmpty()) Assault.ConsecutiveSpawnFailures = 0;
	else if (bAttempted && !bWaitingForOldActors) IncrementSpawnFailureCount(Assault);
	if (Assault.ConsecutiveSpawnFailures >= FMath::Max(1, Profile->MaxConsecutiveSpawnFailures))
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled, ETerritoryAssaultResolution::SpawnFailed);
	else if (bAttempted) BroadcastChanged(Assault);
	return Spawned;
}

bool UTerritoryCounterAttackSubsystem::MigrateLegacySurvivors(FTerritoryAssaultRecord& Assault, ATerritoryVolume* Territory)
{
	if (Assault.LegacySurvivorsToRestore <= 0 || !Assault.PendingSurvivors.IsEmpty()) return true;
	const FAssaultAccess Access = CaptureAssaultAccess(Assault);
	const auto* Profile = Territory->GetCounterAttackProfile();
	const auto* AuthoredForce = Profile ? Profile->FindFactionForce(Assault.AttackingFaction) : nullptr;
	if (!AuthoredForce) return false;
	const FTerritoryFactionAssaultConfig Force = *AuthoredForce;
	UNPCDefinition* Definition = Assault.LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
		&& !Assault.StoryAttackerDefinitionOverride.IsNull() ? Assault.StoryAttackerDefinitionOverride.LoadSynchronous() : Force.AttackerDefinition.Get();
	if (!IsAssaultCurrent(Access)) return false;
	for (FName ApproachID : Assault.SelectedApproaches)
	{
		FTerritoryAssaultApproach Approach;
		FTransform Start, Foot, DropOff;
		FVector Objective;
		if (!ResolveApproach(Territory, ApproachID, Approach, Start)
			|| !ResolveApproachObjective(Territory, Approach, Start, Objective, Foot, &DropOff)) continue;
		if (Approach.EntryType == ETerritoryAssaultEntryType::NarrativeVehicle)
		{
			const auto* Credit = Assault.LegacyVehicleRestoreCredits.FindByPredicate([ApproachID](const auto& Entry) { return Entry.ApproachID == ApproachID && Entry.Count > 0; });
			if (!Credit) continue;
			const bool bEscape = Assault.LaunchMode == ETerritoryAssaultLaunchMode::StoryPursuit
				&& Assault.StoryPursuitDirection == ETerritoryStoryPursuitDirection::PlayerChasesEnemy;
			if (bEscape) Swap(Start, DropOff);
			const int32 PreviousLegacyCount = Assault.LegacySurvivorsToRestore;
			SpawnNarrativeVehicleParticipants(Assault, Territory, Force, Definition, Approach, Start, Start,
				DropOff, Objective, Assault.LegacySurvivorsToRestore, ResolveScaledEnemyLevel(Assault, Territory, Force), true);
			if (!IsAssaultCurrent(Access)) return false;
			if (Assault.LegacySurvivorsToRestore == PreviousLegacyCount)
			{
				IncrementSpawnFailureCount(Assault);
				if (Assault.ConsecutiveSpawnFailures >= FMath::Max(1, Profile->MaxConsecutiveSpawnFailures))
					ResolveAssault(Assault, ETerritoryAssaultState::Cancelled, ETerritoryAssaultResolution::SpawnFailed);
			}
			return IsAssaultCurrent(Access) && !Assault.IsTerminal();
		}
		for (int32 Index = 0; Index < Assault.LegacySurvivorsToRestore; ++Index)
		{
			FTerritoryAssaultSurvivor& Member = Assault.PendingSurvivors.AddDefaulted_GetRef();
			Member.SpawnGUID = FGuid::NewGuid();
			Member.ApproachID = ApproachID;
			Member.Transform = CalculateParticipantDeploymentTransform(Foot, Objective, Index, Profile->ParticipantSpacing);
		}
		Assault.LegacySurvivorsToRestore = 0;
		return true;
	}
	// An old save has no positions or identities. If its authored deployment has gone,
	// fail finitely; do not turn that missing history into new cars or endless reserve.
	IncrementSpawnFailureCount(Assault);
	if (Assault.ConsecutiveSpawnFailures >= FMath::Max(1, Profile->MaxConsecutiveSpawnFailures))
		ResolveAssault(Assault, ETerritoryAssaultState::Cancelled, ETerritoryAssaultResolution::InvalidApproachOrRoute);
	return false;
}

bool UTerritoryCounterAttackSubsystem::HasPlayerVehicleOccupant(const ANarrativeVehicleBase* Vehicle)
	{
		if (!IsValid(Vehicle)) return false;
		if (Cast<APlayerController>(Vehicle->GetController())) return true;
		const UMountComponent* Mount = Vehicle->FindComponentByClass<UMountComponent>();
		if (!Mount) return false;
		for (const FActiveInteractionSlot& Slot : Mount->SlotStatuses)
		{
			if (Slot.SlotStatus == EInteractionSlotStatus::ISS_Free || !IsValid(Slot.SlotUser)) continue;
			// Narrative's player interaction component lives on the controller.
			if (Cast<APlayerController>(Slot.SlotUser->GetOwner())) return true;
			const APawn* Occupant = Cast<APawn>(Slot.SlotUser->GetOwner());
			// A mounted Narrative player can be temporarily unpossessed. The native
			// slot still owns the relationship, including passengers and entry transitions.
			if (IsValid(Occupant) && (Cast<ANarrativePlayerCharacter>(Occupant)
				|| Occupant->IsPlayerControlled())) return true;
		}
		return false;
	}
