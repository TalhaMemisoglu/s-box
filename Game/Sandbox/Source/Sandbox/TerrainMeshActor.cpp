#include "TerrainMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ATerrainMeshActor::ATerrainMeshActor()
{
    PrimaryActorTick.bCanEverTick = true;
    ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProcMesh;

    //for preview the dynamic map before start the game
    #if WITH_EDITOR
        if (!IsRunningGame())
        {
            TArray<TArray<float>> PreviewMap;
            for (int32 X = 0; X < MapWidth; ++X)
            {
                TArray<float> Row;
                for (int32 Y = 0; Y < MapHeight; ++Y)
                {
                    float Z = FMath::Sin(X * 0.1f) * FMath::Cos(Y * 0.1f) * 100.f;
                    Row.Add(Z);
                }
                PreviewMap.Add(Row);
            }

            UpdateMeshFromHeightmap(PreviewMap, MapGridSpacing);
        }
    #endif
}

void ATerrainMeshActor::BeginPlay()
{
    Super::BeginPlay();
}

void ATerrainMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    static float Time = 0.0f;
    Time += DeltaTime;

    // Generate animated heightmap for testing
    TArray<TArray<float>> HeightMap;

    for (int32 X = 0; X < MapWidth; ++X)
    {
        TArray<float> Row;
        for (int32 Y = 0; Y < MapHeight; ++Y)
        {
            float HeightValue = FMath::Sin(X * 0.1f + Time) * FMath::Cos(Y * 0.1f + Time) * 100.f;
            Row.Add(HeightValue);
        }
        HeightMap.Add(Row);
    }

    UpdateMeshFromHeightmap(HeightMap, MapGridSpacing);

    /* Move the player to the center of terrain only once
    if (!bPlayerCentered)
    {
        ACharacter* Player = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
        if (Player)
        {
            FVector CenterLocation = FVector(MapWidth * MapGridSpacing / 2, MapHeight * MapGridSpacing / 2, 500);
            Player->SetActorLocation(CenterLocation);
            bPlayerCentered = true;
        }
    }
    */
}

void ATerrainMeshActor::UpdateMeshFromHeightmap(const TArray<TArray<float>>& HeightMap, float GridSpacing)
{
    int32 Width = HeightMap.Num();
    int32 Height = HeightMap[0].Num();

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    for (int32 X = 0; X < Width; X++)
    {
        for (int32 Y = 0; Y < Height; Y++)
        {
            float Z = HeightMap[X][Y];
            Vertices.Add(FVector(X * GridSpacing, Y * GridSpacing, Z));
            Normals.Add(FVector(0, 0, 1));
            UV0.Add(FVector2D((float)X / Width, (float)Y / Height));
            VertexColors.Add(FColor::White);
            Tangents.Add(FProcMeshTangent(1, 0, 0));
        }
    }

    for (int32 X = 0; X < Width - 1; X++)
    {
        for (int32 Y = 0; Y < Height - 1; Y++)
        {
            int32 A = X + Y * Width;
            int32 B = (X + 1) + Y * Width;
            int32 C = X + (Y + 1) * Width;
            int32 D = (X + 1) + (Y + 1) * Width;

            Triangles.Add(A); Triangles.Add(B); Triangles.Add(C);
            Triangles.Add(B); Triangles.Add(D); Triangles.Add(C);
        }
    }

    ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, true);
}
