// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "TerrainMeshActor.generated.h"

UCLASS()
class SANDBOX_API ATerrainMeshActor : public AActor
{
	GENERATED_BODY()
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	//for testing dynamic map
	int32 MapWidth = 100;
	int32 MapHeight = 100;
	float MapGridSpacing = 20.f;

	bool bPlayerCentered = false; //to move the player once

public:
	// Sets default values for this actor's properties
	ATerrainMeshActor();

	virtual void Tick(float DeltaTime) override;

	void UpdateMeshFromHeightmap(const TArray<TArray<float>>& HeightMap, float GridSpacing);

    void AddCutoffRegion(const TArray<TArray<float>>& HeightMap, TArray<TArray<float>>& Output, float CutoffHeight, int32 Detail);

	UPROPERTY(VisibleAnywhere)
		UProceduralMeshComponent* ProcMesh;

};
