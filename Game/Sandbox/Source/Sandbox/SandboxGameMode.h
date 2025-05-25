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

    UFUNCTION(BlueprintCallable)
    void SetScore(APlayerController * Player, int32 Score);

    UFUNCTION(BlueprintCallable)
    int32 GetScore(APlayerController * Player);

private:
    TMap<APlayerController*, int32> Scores;
};



