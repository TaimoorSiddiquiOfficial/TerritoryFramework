#pragma once

#include "CoreMinimal.h"
#include "Tales/NarrativeCondition.h"
#include "Tales/NarrativeEvent.h"
#include "TerritoryNarrativeContext.generated.h"

class ANarrativeCharacter;
class APlayerController;
class APawn;
class UTalesComponent;

/**
 * Narrative 2.5 evaluates conditions with a single ANarrativeCharacter and keeps the
 * pawn/controller/Tales overloads protected. Territory story assets are authored
 * against the explicit Tales context, so this base keeps that context available and
 * recovers it from the character when Narrative dispatches on its own.
 */
UCLASS(Abstract)
class TERRITORYFRAMEWORK_API UTerritoryNarrativeCondition : public UNarrativeCondition
{
	GENERATED_BODY()

public:
	/**
	 * Explicit-context evaluation. Returns the raw result; Narrative's inherited
	 * Not option stays the caller's responsibility, exactly as with CheckCondition.
	 */
	virtual bool CheckConditionInContext(APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent);

protected:
	virtual bool CheckCondition_Implementation(ANarrativeCharacter* Character) override;
};

/** Event counterpart of UTerritoryNarrativeCondition. */
UCLASS(Abstract)
class TERRITORYFRAMEWORK_API UTerritoryNarrativeEvent : public UNarrativeEvent
{
	GENERATED_BODY()

public:
	/** Explicit-context execution. */
	virtual void ExecuteEventInContext(APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent);

	/**
	 * Narrative's activate path, which defaults to executing the event. OnActivate is
	 * protected in Narrative 2.5, so this keeps the explicit context reachable.
	 */
	virtual void ActivateInContext(APawn* Target, APlayerController* Controller,
		UTalesComponent* NarrativeComponent);

protected:
	virtual void ExecuteEvent_Implementation(ANarrativeCharacter* Character) override;
};
