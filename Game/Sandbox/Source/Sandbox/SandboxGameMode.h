// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "SandboxGameMode.generated.h"

UCLASS(minimalapi)
class ASandboxGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASandboxGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;

protected:
	void SpawnDelayedPlayer();

	UPROPERTY(EditDefaultsOnly, Category = "GameMode Settings")
	float PlayerSpawnDelay;

private:
	UPROPERTY()
	TArray<APlayerController*> QueuedPlayerControllers;

	FTimerHandle PlayerSpawnTimerHandle;
};



