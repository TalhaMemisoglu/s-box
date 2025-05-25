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

	// Varsayılan Gecikme Süresi
	PlayerSpawnDelay = 40.0f; 
}

void ASandboxGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer); 

	if (NewPlayer)
	{
		AGameStateBase* CurrentGameState = GetGameState<AGameStateBase>();
		if (CurrentGameState)
		{
			int32 NumPlayers = CurrentGameState->PlayerArray.Num();

			UE_LOG(LogTemp, Warning, TEXT("PostLogin: NewPlayer = %s, NumPlayers = %d"), *NewPlayer->GetName(), NumPlayers);

			if (NumPlayers > 1) 
			{
				UE_LOG(LogTemp, Warning, TEXT("Queuing player %s for delayed spawn."), *NewPlayer->GetName());
				QueuedPlayerControllers.Add(NewPlayer); 

				if (!GetWorldTimerManager().IsTimerActive(PlayerSpawnTimerHandle))
				{
					UE_LOG(LogTemp, Warning, TEXT("Starting player spawn timer. Delay: %f seconds."), PlayerSpawnDelay);
					GetWorldTimerManager().SetTimer(PlayerSpawnTimerHandle, this, &ASandboxGameMode::SpawnDelayedPlayer, PlayerSpawnDelay, false);
				}
			}
			else 
			{
				UE_LOG(LogTemp, Warning, TEXT("Player %s is the host or first player, spawning immediately."), *NewPlayer->GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("PostLogin: GameState is null!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("PostLogin: NewPlayer is null!"));
	}
}

void ASandboxGameMode::SpawnDelayedPlayer()
{
	UE_LOG(LogTemp, Warning, TEXT("PlayerSpawnTimerHandle ticked. Processing queued players."));

	if (QueuedPlayerControllers.Num() > 0)
	{
		APlayerController* PlayerToSpawn = QueuedPlayerControllers[0]; 
		if (PlayerToSpawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("Attempting to spawn player: %s"), *PlayerToSpawn->GetName());
			RestartPlayer(PlayerToSpawn); 
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("PlayerToSpawn was null in queue."));
		}
		QueuedPlayerControllers.RemoveAt(0); 
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("No players in queue."));
	}

	if (QueuedPlayerControllers.Num() > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("More players in queue. Restarting timer."));
		GetWorldTimerManager().SetTimer(PlayerSpawnTimerHandle, this, &ASandboxGameMode::SpawnDelayedPlayer, PlayerSpawnDelay, false);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Player queue is empty. Timer stopped."));
	}
}
