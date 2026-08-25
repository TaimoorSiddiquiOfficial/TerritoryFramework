#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"

class FGameplayDebuggerCategory_Territory : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Territory();
	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC,
		FGameplayDebuggerCanvasContext& CanvasContext) override;
	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

private:
	struct FRepData
	{
		FString Summary;
		void Serialize(FArchive& Ar) { Ar << Summary; }
	};
	FRepData DataPack;
};

#endif
