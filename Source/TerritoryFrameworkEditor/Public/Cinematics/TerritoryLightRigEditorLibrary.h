#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TerritoryLightRigEditorLibrary.generated.h"
class UTerritoryCinematicLightRigProfile;
class UEditorUtilityWidget;

/** Optional authoring bridge. All panel references and preview copying stay in the editor. */
UCLASS()
class TERRITORYFRAMEWORKEDITOR_API UTerritoryLightRigEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Territory|Editor|Optional Lights")
	static UEditorUtilityWidget* OpenControlPanel(UTerritoryCinematicLightRigProfile* Profile);
	/** Copies only the configured preset properties. Leaves the runtime Blueprint dirty for normal review and Save All. */
	UFUNCTION(BlueprintCallable, Category="Territory|Editor|Optional Lights")
	static bool CopyPanelLook(UTerritoryCinematicLightRigProfile* Profile, FString& Result);
};
