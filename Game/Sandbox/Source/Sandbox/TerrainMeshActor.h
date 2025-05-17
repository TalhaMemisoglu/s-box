// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "FileWatcher.h"
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

	FileWatcher watcher;

	//for testing dynamic map
	int32 MapWidth;
	int32 MapHeight;
    int32 MapSmootheningOffset;
    int32 MapWidthAbsolute;
    int32 MapHeightAbsolute;
	float MapGridSpacing;
	float MapUVScale;

	bool bPlayerCentered = false; //to move the player once
	// bool bMeshSectionCreated = false; // Flag to track if section 0 is created

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FProcMeshTangent> Tangents;

public:
	// Sets default values for this actor's properties
	ATerrainMeshActor();

	virtual void Tick(float DeltaTime) override;

	void UpdateMeshFromHeightmap(const TArray<TArray<float>>& HeightMap);

    void SetMapSize(int32 Width, int32 Height, int32 SmootheningOffset, float GridSpacing, float UVScale);

    void AddCutoffRegion(const TArray<TArray<float>>& HeightMap, TArray<TArray<float>>& Output, float CutoffHeight, int32 Detail);
	void StartWatcher();

	void readFileContent();

	UPROPERTY(VisibleAnywhere)
		UProceduralMeshComponent* ProcMesh;

};
