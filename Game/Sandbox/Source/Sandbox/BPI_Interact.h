// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_Interact.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_Interact : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for actors that can be interacted with by the player character.
 */
class SANDBOX_API IBPI_Interact
{
	GENERATED_BODY()

public:
	// Called when the player character interacts with this actor.
	// The InteractingPawn is the pawn (character) that initiated the interaction.
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Interaction")
	void OnPlayerInteraction(APawn* InteractingPawn);
}; 