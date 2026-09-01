#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class UTerritoryQuestCascadeRecipe;
struct FPropertyChangedEvent;
struct FTerritoryQuestCascadeBranch;

/** Read-only, live mission-flow explanation embedded in a cascade recipe. */
class STerritoryQuestMissionSummaryPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STerritoryQuestMissionSummaryPanel) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UTerritoryQuestCascadeRecipe>, Recipe)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~STerritoryQuestMissionSummaryPanel() override;

private:
	void Rebuild();
	void HandleObjectPropertyChanged(UObject* Object,
		FPropertyChangedEvent& PropertyChangedEvent);
	EActiveTimerReturnType HandleDeferredRefresh(double CurrentTime,
		float DeltaTime);
	FReply CopyReportToClipboard();
	TSharedRef<SWidget> BuildBranchBody(
		const FTerritoryQuestCascadeBranch& Branch,
		int32 SharedConditionCount) const;

	TWeakObjectPtr<UTerritoryQuestCascadeRecipe> Recipe;
	TSharedPtr<SVerticalBox> Content;
	FDelegateHandle PropertyChangedHandle;
	bool bRefreshScheduled = false;
};
