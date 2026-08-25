#include "Debug/TerritoryDebugger.h"
#include "Debug/TerritoryGameplayDebuggerCategory.h"
#include "Core/TerritoryTypes.h"
#include "Core/TerritoryVolume.h"
#include "Subsystems/TerritoryCounterAttackSubsystem.h"
#include "Subsystems/TerritoryRegistrySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

FText UTerritoryDebugger::BuildTerritoryDebugSummary(
	const UObject* WorldContextObject, const AActor* DebugActor)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World) return FText::FromString(TEXT("Territory: no valid world"));

	ATerritoryVolume* Territory = const_cast<ATerritoryVolume*>(
		Cast<ATerritoryVolume>(DebugActor));
	if (!Territory && DebugActor)
	{
		if (UTerritoryRegistrySubsystem* Registry =
			World->GetSubsystem<UTerritoryRegistrySubsystem>())
		{
			Territory = Registry->GetTerritoryAtLocation(DebugActor->GetActorLocation());
		}
	}
	if (!Territory) return FText::FromString(TEXT("Territory: no Territory at debug actor"));

	const UEnum* StateEnum = StaticEnum<ETerritoryState>();
	FString Summary = FString::Printf(
		TEXT("Territory: %s\nOwner: %s  State: %s  Progress: %.3f\nGuards: %d/%d desired  Capacity: %d"),
		*Territory->GetTerritoryTag().ToString(),
		*Territory->GetOwningFaction().ToString(),
		StateEnum ? *StateEnum->GetNameStringByValue(
			static_cast<int64>(Territory->GetTerritoryState())) : TEXT("Unknown"),
		Territory->GetControlProgress(), Territory->GetDefenderCount(),
		Territory->GetDesiredGuardCount(), Territory->GetMaxGuardCount());

	if (UTerritoryCounterAttackSubsystem* Counterattacks =
		World->GetSubsystem<UTerritoryCounterAttackSubsystem>())
	{
		const TArray<FTerritoryAssaultRecord> Assaults =
			Counterattacks->GetAssaultsForTerritoryActor(Territory);
		Summary += FString::Printf(TEXT("\nAssault records: %d"), Assaults.Num());
		for (const FTerritoryAssaultRecord& Assault : Assaults)
		{
			Summary += FString::Printf(TEXT("\n%s  %s"),
				*Assault.AssaultID.ToString(),
				*Counterattacks->GetAssaultDebugString(Assault.AssaultID));
		}
	}
	return FText::FromString(Summary);
}

#if WITH_GAMEPLAY_DEBUGGER
FGameplayDebuggerCategory_Territory::FGameplayDebuggerCategory_Territory()
{
	SetDataPackReplication<FRepData>(&DataPack);
}

void FGameplayDebuggerCategory_Territory::CollectData(
	APlayerController* OwnerPC, AActor* DebugActor)
{
	DataPack.Summary = UTerritoryDebugger::BuildTerritoryDebugSummary(
		OwnerPC ? static_cast<const UObject*>(OwnerPC) : DebugActor, DebugActor).ToString();
}

void FGameplayDebuggerCategory_Territory::DrawData(
	APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	(void)OwnerPC;
	TArray<FString> Lines;
	DataPack.Summary.ParseIntoArrayLines(Lines, false);
	for (const FString& Line : Lines)
	{
		CanvasContext.Print(Line);
	}
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Territory::MakeInstance()
{
	return MakeShared<FGameplayDebuggerCategory_Territory>();
}
#endif
