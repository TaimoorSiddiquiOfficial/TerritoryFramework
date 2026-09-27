#include "TerritoryDefinitionEditorLibrary.h"

#include "Core/TerritoryDefinition.h"
#include "Core/TerritoryFloorVolume.h"
#include "Core/TerritoryGuardSpawnPoint.h"
#include "Core/TerritoryHierarchy.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"

FTerritoryDefinitionSyncReport UTerritoryDefinitionEditorLibrary::SynchronizeFloorsInWorld(
	UWorld* World, UTerritoryPlaceDefinition* PlaceDefinition, bool bUpdatePostAssignments)
{
	FTerritoryDefinitionSyncReport Report;
	if (!World || World->IsGameWorld() || (GEditor && GEditor->PlayWorld))
	{
		Report.Errors.Add(TEXT("Stop Play/Simulate and open an editor world before synchronizing floors."));
		return Report;
	}

	// Discovery reads loaded authoring objects only. Runtime registry population and streaming
	// order must never change a saved floor quota or the identity used by floor-cleared events.
	TArray<ATerritoryProperty*> Places;
	TMap<UTerritoryPlaceDefinition*, TArray<ATerritoryProperty*>> PlacesByDefinition;
	for (TActorIterator<ATerritoryProperty> It(World); It; ++It)
	{
		if (UTerritoryPlaceDefinition* Definition = Cast<UTerritoryPlaceDefinition>(It->GetTerritoryDefinition()))
		{
			Places.Add(*It);
			PlacesByDefinition.FindOrAdd(Definition).Add(*It);
		}
	}
	if (PlaceDefinition && !PlacesByDefinition.Contains(PlaceDefinition))
	{
		Report.Errors.Add(FString::Printf(TEXT("No loaded Place actor uses %s. Load its level cells before detecting floors."),
			*PlaceDefinition->GetPathName()));
		return Report;
	}
	for (const auto& Pair : PlacesByDefinition)
	{
		if ((!PlaceDefinition || Pair.Key == PlaceDefinition) && Pair.Value.Num() != 1)
			Report.Errors.Add(FString::Printf(TEXT("%s is used by %d loaded Place actors; floor assignments require one physical Place per Definition."),
				*Pair.Key->GetPathName(), Pair.Value.Num()));
	}

	TMap<ATerritoryFloorVolume*, UTerritoryPlaceDefinition*> RegionBindings;
	TMap<UTerritoryPlaceDefinition*, TArray<const ATerritoryFloorVolume*>> RegionsByDefinition;
	TArray<ATerritoryFloorVolume*> Regions;
	for (TActorIterator<ATerritoryFloorVolume> It(World); It; ++It) Regions.Add(*It);
	Regions.Sort([](const ATerritoryFloorVolume& A, const ATerritoryFloorVolume& B) { return A.GetPathName() < B.GetPathName(); });
	for (ATerritoryFloorVolume* Region : Regions)
	{
		UTerritoryPlaceDefinition* Definition = Region->GetPlaceDefinition();
		if (Definition && PlaceDefinition && Definition != PlaceDefinition) continue;
		if (!Definition)
		{
			TArray<ATerritoryProperty*> Candidates;
			for (ATerritoryProperty* Place : Places)
				if (Region->IsFullyContainedBy(Place)) Candidates.Add(Place);
			if (PlaceDefinition && !Candidates.ContainsByPredicate([PlaceDefinition](const ATerritoryProperty* Place)
				{ return Place->GetTerritoryDefinition() == PlaceDefinition; }))
			{
				const bool bCenterInRequestedPlace = PlacesByDefinition.FindChecked(PlaceDefinition).ContainsByPredicate([Region](const ATerritoryProperty* Place)
					{ return Place->ContainsPoint(Region->GetActorLocation()); });
				if (bCenterInRequestedPlace)
					Report.Errors.Add(FString::Printf(TEXT("Floor region '%s' has its center in the requested Place but its whole box is not contained. Bind its Place Definition explicitly or adjust its bounds."), *Region->GetActorLabel()));
				continue;
			}
			if (Candidates.Num() != 1)
			{
				Report.Errors.Add(FString::Printf(TEXT("Floor region '%s' has %d fully containing Places. Bind its Place Definition explicitly or fix overlapping bounds; no owner was guessed."),
					*Region->GetActorLabel(), Candidates.Num()));
				continue;
			}
			Definition = CastChecked<UTerritoryPlaceDefinition>(Candidates[0]->GetTerritoryDefinition());
			RegionBindings.Add(Region, Definition);
		}
		if (!PlacesByDefinition.Contains(Definition))
		{
			Report.Errors.Add(FString::Printf(TEXT("Floor region '%s' uses a Place whose actor is not loaded; its explicit binding was preserved."), *Region->GetActorLabel()));
			continue;
		}
		if (Region->GetPlaceDefinition() && PlacesByDefinition.FindChecked(Definition).Num() == 1)
		{
			const ATerritoryProperty* BoundPlace = PlacesByDefinition.FindChecked(Definition)[0];
			if (!BoundPlace->ContainsPoint(Region->GetActorLocation()))
				for (const ATerritoryProperty* OtherPlace : Places)
					if (OtherPlace != BoundPlace && Region->IsFullyContainedBy(OtherPlace))
						Report.Warnings.Add(FString::Printf(TEXT("Floor region '%s' is explicitly bound to %s but lies inside %s. The explicit binding was preserved; review the ownership."),
							*Region->GetActorLabel(), *Definition->GetName(), *OtherPlace->GetActorLabel()));
		}
		if (Region->FloorIndex < 0 || !FMath::IsFinite(Region->GetFloorBoundsVolume()) || Region->GetFloorBoundsVolume() <= 0.0)
		{
			Report.Errors.Add(FString::Printf(TEXT("Floor region '%s' needs a nonnegative floor identity and finite, nonempty bounds."), *Region->GetActorLabel()));
			continue;
		}
		RegionsByDefinition.FindOrAdd(Definition).Add(Region);
	}

	TMap<FTerritoryGuardPostTemplate*, int32> AssignmentChanges;
	TMap<UTerritoryPlaceDefinition*, TSet<int32>> MissingFloors;
	TSet<UTerritoryPlaceDefinition*> ChangedDefinitions;
	TArray<ATerritoryGuardSpawnPoint*> PostsToRefresh;
	if (bUpdatePostAssignments && RegionsByDefinition.IsEmpty() && Report.Errors.IsEmpty())
		Report.Warnings.Add(TEXT("No matching loaded floor regions were found. Place Floor Volume boxes before detecting floor assignments."));
	for (const auto& Pair : RegionsByDefinition)
	{
		UTerritoryPlaceDefinition* Definition = Pair.Key;
		TSet<int32> ExistingFloors;
		for (const FTerritoryFloorTemplate& Row : Definition->Floors)
		{
			if (Row.FloorIndex < 0 || ExistingFloors.Contains(Row.FloorIndex))
				Report.Errors.Add(FString::Printf(TEXT("%s has an invalid or duplicate floor identity %d; fix it before detection."), *Definition->GetName(), Row.FloorIndex));
			ExistingFloors.Add(Row.FloorIndex);
		}
		for (const ATerritoryFloorVolume* Region : Pair.Value)
			if (!ExistingFloors.Contains(Region->FloorIndex)) MissingFloors.FindOrAdd(Definition).Add(Region->FloorIndex);

		TMap<FName, TArray<ATerritoryGuardSpawnPoint*>> PostsByID;
		for (TActorIterator<ATerritoryGuardSpawnPoint> It(World); It; ++It)
			if (It->GetTerritoryDefinition() == Definition) PostsByID.FindOrAdd(It->GetGuardPostID()).Add(*It);
		TSet<FName> RowIDs;
		for (FTerritoryGuardPostTemplate& Row : Definition->GuardPosts)
		{
			if (Row.GuardPostID.IsNone() || RowIDs.Contains(Row.GuardPostID))
			{
				Report.Errors.Add(FString::Printf(TEXT("%s has a missing or duplicate Guard Post ID; floor detection requires unique bindings."), *Definition->GetName()));
				continue;
			}
			RowIDs.Add(Row.GuardPostID);
			if (Row.FloorIndex < 0)
				Report.Errors.Add(FString::Printf(TEXT("%s: post '%s' has an invalid negative floor identity."), *Definition->GetName(), *Row.GuardPostID.ToString()));
			else if (!ExistingFloors.Contains(Row.FloorIndex))
				MissingFloors.FindOrAdd(Definition).Add(Row.FloorIndex);
			const TArray<ATerritoryGuardSpawnPoint*>* BoundPosts = PostsByID.Find(Row.GuardPostID);
			if (!BoundPosts)
			{
				Report.Warnings.Add(FString::Printf(TEXT("%s: post '%s' is not loaded; its floor %d was preserved. Load all relevant cells to detect every post."),
					*Definition->GetName(), *Row.GuardPostID.ToString(), Row.FloorIndex));
				continue;
			}
			if (BoundPosts->Num() != 1)
			{
				Report.Errors.Add(FString::Printf(TEXT("%s: %d actors share post '%s'; floor detection cannot choose a physical slot."),
					*Definition->GetName(), BoundPosts->Num(), *Row.GuardPostID.ToString()));
				continue;
			}
			ATerritoryGuardSpawnPoint* Post = (*BoundPosts)[0];
			TSet<int32> ContainingFloors;
			for (const ATerritoryFloorVolume* Region : Pair.Value)
				if (Region->ContainsPoint(Post->GetActorLocation())) ContainingFloors.Add(Region->FloorIndex);
			if (ContainingFloors.Num() > 1)
			{
				Report.Errors.Add(FString::Printf(TEXT("Post '%s' overlaps regions with different floor identities; resolve the geometry before assigning its floor."), *Post->GetActorLabel()));
				continue;
			}
			const int32 DetectedFloor = ATerritoryFloorVolume::ResolveFloorAtLocation(Pair.Value, Post->GetActorLocation());
			if (DetectedFloor == INDEX_NONE)
			{
				Report.Warnings.Add(FString::Printf(TEXT("Post '%s' is outside its Place's loaded floor regions; authored floor %d was preserved."), *Post->GetActorLabel(), Row.FloorIndex));
				continue;
			}
			if (DetectedFloor == Row.FloorIndex) continue;
			if (!bUpdatePostAssignments)
			{
				Report.Warnings.Add(FString::Printf(TEXT("Post '%s' is authored on floor %d but geometry detects %d. Use Detect & Sync Floors to update its Definition row; normal setup preserves existing assignments."),
					*Post->GetActorLabel(), Row.FloorIndex, DetectedFloor));
				continue;
			}
			AssignmentChanges.Add(&Row, DetectedFloor);
			ChangedDefinitions.Add(Definition);
			PostsToRefresh.Add(Post);
		}
		for (const auto& PostPair : PostsByID)
			if (!RowIDs.Contains(PostPair.Key))
				Report.Errors.Add(FString::Printf(TEXT("%s has loaded posts bound to missing row '%s'; fix their binding before detecting floors."), *Definition->GetName(), *PostPair.Key.ToString()));
		// Do not turn a floor's existing story/quota into an impossible promise.
		for (const FTerritoryFloorTemplate& Floor : Definition->Floors)
		{
			int32 Capacity = 0;
			for (FTerritoryGuardPostTemplate& Row : Definition->GuardPosts)
			{
				const int32* Planned = AssignmentChanges.Find(&Row);
				Capacity += (Planned ? *Planned : Row.FloorIndex) == Floor.FloorIndex ? 1 : 0;
			}
			if (Floor.DesiredGuards > Capacity)
				Report.Errors.Add(FString::Printf(TEXT("%s: floor %d requests %d guards but the proposed assignment provides %d posts. Adjust its quota or geometry before synchronization."),
					*Definition->GetName(), Floor.FloorIndex, Floor.DesiredGuards, Capacity));
		}
	}
	if (!Report.Errors.IsEmpty()) return Report;
	for (const auto& Pair : MissingFloors) ChangedDefinitions.Add(Pair.Key);
	if (ChangedDefinitions.IsEmpty() && RegionBindings.IsEmpty())
	{
		Report.bSucceeded = true;
		return Report;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("TerritoryEditor", "SyncTerritoryFloors", "Detect and Synchronize Territory Floors"));
	for (UTerritoryPlaceDefinition* Definition : ChangedDefinitions) Definition->Modify();
	for (const auto& Pair : MissingFloors)
	{
		TArray<int32> SortedFloors = Pair.Value.Array();
		SortedFloors.Sort();
		for (int32 FloorIndex : SortedFloors)
		{
			FTerritoryFloorTemplate& Row = Pair.Key->Floors.AddDefaulted_GetRef();
			Row.FloorIndex = FloorIndex;
			Row.DisplayName = FText::FromString(FString::Printf(TEXT("Floor %d"), FloorIndex));
		}
	}
	for (const auto& Pair : AssignmentChanges) Pair.Key->FloorIndex = Pair.Value;
	for (const auto& Pair : RegionBindings)
	{
		Pair.Key->Modify();
		Pair.Key->PlaceDefinition = Pair.Value;
		Pair.Key->ApplyFloorVolumeDefinition();
		Pair.Key->MarkPackageDirty();
		++Report.UpdatedActors;
	}
	for (ATerritoryGuardSpawnPoint* Post : PostsToRefresh)
	{
		Post->Modify();
		Post->ApplyTerritoryDefinition();
		++Report.UpdatedActors;
	}
	for (UTerritoryPlaceDefinition* Definition : ChangedDefinitions) Definition->MarkPackageDirty();
	Report.UpdatedDefinitions = ChangedDefinitions.Num();
	Report.UpdatedFloorAssignments = AssignmentChanges.Num();
	Report.bSucceeded = true;
	return Report;
}
