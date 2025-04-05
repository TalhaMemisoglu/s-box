// Copyright Epic Games, Inc. All Rights Reserved.

#include "SandboxGameMode.h"
#include "SandboxHUD.h"
#include "SandboxCharacter.h"
#include "UObject/ConstructorHelpers.h"

ASandboxGameMode::ASandboxGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ASandboxHUD::StaticClass();
}
