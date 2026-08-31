#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class UTerritoryDefinition;
struct FPropertyChangedEvent;
struct FTerritoryStoryOutcomeScenario;

/** Compact read-only Story Outcome viewer embedded in Territory Definition details. */
class STerritoryStoryOutcomePanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STerritoryStoryOutcomePanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UTerritoryDefinition>, Definition)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~STerritoryStoryOutcomePanel() override;

private:
	void Rebuild();
	void HandleObjectPropertyChanged(UObject* Object,
		FPropertyChangedEvent& PropertyChangedEvent);
	EActiveTimerReturnType HandleDeferredRefresh(double CurrentTime,
		float DeltaTime);
	FReply CopyReportToClipboard();

	TSharedRef<SWidget> BuildScenarioBody(
		const FTerritoryStoryOutcomeScenario& Scenario) const;
	void AddExplanationRow(const TSharedRef<SVerticalBox>& Box,
		const FString& Label, const FString& Value) const;

	TWeakObjectPtr<UTerritoryDefinition> Definition;
	TSharedPtr<SVerticalBox> Content;
	FDelegateHandle PropertyChangedHandle;
	bool bRefreshScheduled = false;
};
