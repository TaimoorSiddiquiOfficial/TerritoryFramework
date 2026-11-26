#include "Tales/TerritoryReplyTimeoutProfile.h"

#include "Tales/Dialogue.h"

float UTerritoryReplyTimeoutProfile::ResolveSeconds(const UDialogue* Dialogue) const
{
	if (!bEnabled || !Dialogue)
	{
		return 0.f;
	}
	for (const FTerritoryReplyTimeoutRule& Rule : DialogueOverrides)
	{
		if (Rule.Dialogue.Get() == Dialogue)
		{
			return FMath::Max(0.f, Rule.Seconds);
		}
	}
	return FMath::Max(0.f, DefaultSeconds);
}

FName UTerritoryReplyTimeoutProfile::ResolveSilentReplyID(const UDialogue* Dialogue) const
{
	for (const FTerritoryReplyTimeoutRule& Rule : DialogueOverrides)
	{
		if (Rule.Dialogue.Get() == Dialogue && !Rule.SilentReplyID.IsNone())
		{
			return Rule.SilentReplyID;
		}
	}
	return SilentReplyID;
}

bool UTerritoryReplyTimeoutProfile::IsEnabledForDialogue(const UDialogue* Dialogue) const
{
	return bEnabled && Dialogue != nullptr
		&& ResolveSeconds(Dialogue) > 0.f
		&& !ResolveSilentReplyID(Dialogue).IsNone();
}
