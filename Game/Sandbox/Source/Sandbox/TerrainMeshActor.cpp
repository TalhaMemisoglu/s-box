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
            float HeightValue = FMath::Sin(X * 0.1f + 0.2*Time) * FMath::Cos(Y * 0.1f + 0.2*Time) * 100.f;
            Row.Add(HeightValue);
        }
        HeightMap.Add(Row);
    }

    TArray<TArray<float>> CutoffMap;

    AddCutoffRegion(HeightMap, CutoffMap, -120.0f, 15);
    UpdateMeshFromHeightmap(CutoffMap, MapGridSpacing);

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

static inline float smoothLerp(float a, float b, float c) {
    return a + (b - a) * FMath::SmoothStep(0, 1, c);
}

void ATerrainMeshActor::AddCutoffRegion(const TArray<TArray<float>>& HeightMap, TArray<TArray<float>>& Output, float CutoffHeight, int32 Detail)
{
    int32 InputWidth = HeightMap.Num();
    int32 InputHeight = HeightMap[0].Num();

    // left part
    for (int32 X = 0; X < Detail; X++)
    {
        TArray<float> Row;
        Output.Add(Row);

        // left-bottom corner
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(HeightMap[0][0], CutoffHeight, FMath::Sqrt(FMath::Square(X - Detail) + FMath::Square(Y - Detail)) / (Detail));
            Output.Last().Add(HeightValue);
        }

        // left-middle part
        for(int32 Y = 0; Y < InputHeight; Y++)
        {
            float HeightValue = smoothLerp(CutoffHeight, HeightMap[0][Y], float(X) / (Detail));
            Output.Last().Add(HeightValue);
        }

        // left-top corner
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(HeightMap[0][InputHeight - 1], CutoffHeight, FMath::Sqrt(FMath::Square(X - Detail) + FMath::Square(Y)) / (Detail));
            Output.Last().Add(HeightValue);
        }
    }

    // middle part
    for (int32 X = 0; X < InputWidth; X++)
    {
        TArray<float> Row;
        Output.Add(Row);

        // middle-bottom part
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(CutoffHeight, HeightMap[X][0], ((float)Y) / (Detail));
            Output.Last().Add(HeightValue);
        }

        // middle-middle part
        Output.Last().Append(HeightMap[X]);

        // middle-top part
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(CutoffHeight, HeightMap[X][InputHeight - 1], ((float)(Detail - Y)) / (Detail));
            Output.Last().Add(HeightValue);
        }
    }

    // right part
    for (int32 X = 0; X < Detail; X++)
    {
        TArray<float> Row;
        Output.Add(Row);

        // right-bottom corner
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(HeightMap[InputWidth - 1][0], CutoffHeight, FMath::Sqrt(FMath::Square(X) + FMath::Square(Detail - Y)) / Detail);
            Output.Last().Add(HeightValue);
        }

        // right-middle part
        for(int32 Y = 0; Y < InputHeight; Y++)
        {
            float HeightValue = smoothLerp(HeightMap[InputWidth - 1][Y], CutoffHeight, ((float)X) / (Detail));
            Output.Last().Add(HeightValue);
        }

        // right-top corner
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(HeightMap[InputWidth - 1][InputHeight - 1], CutoffHeight, FMath::Sqrt(FMath::Square(X) + FMath::Square(Y)) / Detail);
            Output.Last().Add(HeightValue);
        }
    }
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
        float ZVPrev = 0;
        for (int32 Y = 0; Y < Height; Y++)
        {
            float Z = HeightMap[X][Y];
            Vertices.Add(FVector(X * GridSpacing, Y * GridSpacing, Z));
            //Normals.Add(FVector(0, 0, 1));
            FVector U = FVector(GridSpacing, 0, Z - (X == 0 ? 0 : HeightMap[X - 1][Y])).GetUnsafeNormal();
            FVector V = FVector(0, GridSpacing, Z - ZVPrev).GetUnsafeNormal();
            FVector Normal = FVector::CrossProduct(U, V);
            Normals.Add(Normal);
            UV0.Add(FVector2D((float)X / Width, (float)Y / Height));
            VertexColors.Add(FColor::White);
            Tangents.Add(FProcMeshTangent(U, false));
            ZVPrev = Z;
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
