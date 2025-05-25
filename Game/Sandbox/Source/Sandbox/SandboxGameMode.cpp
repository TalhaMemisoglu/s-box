// Copyright Epic Games, Inc. All Rights Reserved.

#include "SandboxGameMode.h"
#include "SandboxHUD.h"
#include "SandboxCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

ASandboxGameMode::ASandboxGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ASandboxHUD::StaticClass();
}

void ASandboxGameMode::SetScore(APlayerController * Player, int32 Score) {
    if(Player) {
        Scores.Add(Player, Score);
    }
}

int32 ASandboxGameMode::GetScore(APlayerController * Player) {
    if(Scores.Contains(Player)) {
        return Scores[Player];
    }
    else {
        return 0;
    }
}
