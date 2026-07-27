#include "Framework/TerritoryPlayerController.h"

#include "EnhancedInputComponent.h"
#include "GAS/NarrativeAbilityInputMapping.h"
#include "Widgets/NarrativeGameplayHUD.h"

void ATerritoryPlayerController::EnsureGameplayHUDClass()
{
	if (IsValid(GameplayHUDClass) && GameplayHUDClass->IsChildOf(UNarrativeGameplayHUD::StaticClass()))
	{
		return;
	}

	GameplayHUDClass = LoadClass<UNarrativeGameplayHUD>(
		nullptr,
		TEXT("/Game/TerritoryFramework/UI/WBP_TerritoryGameplayHUD.WBP_TerritoryGameplayHUD_C"));
	if (!IsValid(GameplayHUDClass))
	{
		UE_LOG(LogTemp, Error, TEXT("TerritoryPlayerController could not load the project Gameplay HUD; using Narrative's base HUD."));
		GameplayHUDClass = UNarrativeGameplayHUD::StaticClass();
	}
}

void ATerritoryPlayerController::OnPossess(APawn* InPawn)
{
	EnsureGameplayHUDClass();
	Super::OnPossess(InPawn);
}

void ATerritoryPlayerController::OnRep_PlayerState()
{
	EnsureGameplayHUDClass();
	Super::OnRep_PlayerState();
}

void ATerritoryPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput || !AbilityInputMappings)
	{
		return;
	}

	for (const auto& InputAbility : AbilityInputMappings->InputAbilities)
	{
		if (IsValid(InputAbility.InputAction))
		{
			EnhancedInput->BindAction(
				InputAbility.InputAction,
				ETriggerEvent::Canceled,
				this,
				&ATerritoryPlayerController::AbilityInputReleased,
				InputAbility.InputTag);
		}
	}
}
