#include "Social/TerritoryConversationProfile.h"
#include "Tales/TerritoryAmbientDialogue.h"
#include "Misc/DataValidation.h"

bool UTerritoryConversationProfile::Validate(FString& Failure) const
{
	if (!FMath::IsFinite(RecruitRadius) || RecruitRadius < 100.f
		|| !FMath::IsFinite(ListenerRadius) || ListenerRadius < 100.f
		|| !FMath::IsFinite(ArrivalRadius) || ArrivalRadius < 10.f || ArrivalRadius >= RecruitRadius
		|| !FMath::IsFinite(MeetingTimeout) || MeetingTimeout < 1.f || MeetingTimeout > 120.f
		|| !FMath::IsFinite(PreparationTimeout) || PreparationTimeout < 1.f || PreparationTimeout > 15.f
		|| !FMath::IsFinite(MaximumDialogueDuration) || MaximumDialogueDuration < 1.f || MaximumDialogueDuration > 300.f
		|| !FMath::IsFinite(Cooldown) || Cooldown < 0.f)
	{
		Failure = TEXT("Conversation distances and timeouts must be finite and within their supported bounds.");
		return false;
	}
	return UTerritoryAmbientDialogue::ValidateDialogueClass(Dialogue, Failure);
}

#if WITH_EDITOR
EDataValidationResult UTerritoryConversationProfile::IsDataValid(FDataValidationContext& Context) const
{
	FString Failure;
	if (!Validate(Failure)) { Context.AddError(FText::FromString(Failure)); return EDataValidationResult::Invalid; }
	return EDataValidationResult::Valid;
}
#endif
