#include "TerrainMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "SandboxCharacter.h"

ATerrainMeshActor::ATerrainMeshActor()
{
    PrimaryActorTick.bCanEverTick = true;
    ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProcMesh;
    ProcMesh->bUseAsyncCooking = false;
    bReplicates = true;
    bAlwaysRelevant = true;
    NetUpdateFrequency = 1.0f;
}

void ATerrainMeshActor::BeginPlay()
{
    Super::BeginPlay();
    SetMapSize(100, 100, 15, 20.0f, 0.2f);
    UpdateTime = 0.0f;
    if(GetLocalRole() == ROLE_Authority)
    {
        watcher = new FileWatcher();
        watcher->Start("C:\\Users\\talha\\Desktop\\slope.bin");
    }
    else {
        APlayerController * LocalController = GEngine->GetFirstLocalPlayerController(GetWorld());
        if(LocalController) {
            ASandboxCharacter * Character = Cast<ASandboxCharacter>(LocalController->GetPawn());
            if(Character) {
                Character->RequestMapUpdate(this);
            }
        }
    }
}

void ATerrainMeshActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if(GetLocalRole() == ROLE_Authority) {
        watcher->Stop();
    }
}

void ATerrainMeshActor::SetMapSize(int32 Width, int32 Height, int32 SmootheningOffset, float GridSpacing, float UVScale) {
    MapWidth = Width;
    MapHeight = Height;
    MapSmootheningOffset = SmootheningOffset;
    MapGridSpacing = GridSpacing;
    MapUVScale = UVScale;
    MapWidthAbsolute = MapWidth + MapSmootheningOffset * 2;
    MapHeightAbsolute = MapHeight + MapSmootheningOffset * 2;

    Vertices.SetNum(MapWidthAbsolute * MapHeightAbsolute);
    Normals.SetNum(MapWidthAbsolute * MapHeightAbsolute);
    Tangents.SetNum(MapWidthAbsolute * MapHeightAbsolute);

    TArray<FVector2D> UV;
    UV.SetNum(MapWidthAbsolute * MapHeightAbsolute);

    TArray<int32> Triangles;
    Triangles.SetNum(6 * (MapWidthAbsolute - 1) * (MapHeightAbsolute - 1));

    TargetHeightMap.AddDefaulted(MapHeight * MapWidth);
    PreviousHeightMap.AddDefaulted(MapHeight * MapWidth);

    for(int32 Y = 0; Y < MapHeightAbsolute; ++Y)
    {
        for(int32 X = 0; X < MapWidthAbsolute; ++X)
        {
            Vertices[Y + MapWidthAbsolute * X] = FVector(Y - 0.5f * MapHeightAbsolute, X - 0.5f * MapWidthAbsolute, 0) * MapGridSpacing;
            Normals[Y + MapWidthAbsolute * X] = FVector::UpVector;
            UV[Y + MapWidthAbsolute * X] = FVector2D(Y, X) * MapUVScale;
            Tangents[X + Y * MapWidthAbsolute] = FProcMeshTangent(FVector(1, 0, 0), false);
        }
    }

    for(int32 Y = 0; Y < MapHeightAbsolute - 1; ++Y)
    {
        for(int32 X = 0; X < MapWidthAbsolute - 1; ++X)
        {
            Triangles[6 * (Y + X * (MapWidthAbsolute - 1)) + 0] = (Y + 0) + (X + 0) * MapWidthAbsolute;
            Triangles[6 * (Y + X * (MapWidthAbsolute - 1)) + 1] = (Y + 0) + (X + 1) * MapWidthAbsolute;
            Triangles[6 * (Y + X * (MapWidthAbsolute - 1)) + 2] = (Y + 1) + (X + 1) * MapWidthAbsolute;
            Triangles[6 * (Y + X * (MapWidthAbsolute - 1)) + 3] = (Y + 0) + (X + 0) * MapWidthAbsolute;
            Triangles[6 * (Y + X * (MapWidthAbsolute - 1)) + 4] = (Y + 1) + (X + 1) * MapWidthAbsolute;
            Triangles[6 * (Y + X * (MapWidthAbsolute - 1)) + 5] = (Y + 1) + (X + 0) * MapWidthAbsolute;
        }
    }

    ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), true);
    //ProcMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
}

void ATerrainMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    static float Time = 0.0f;
    
    TArray<uint8> FileContents;
    if(GetLocalRole() == ROLE_Authority && watcher->Update(FileContents)) {
        const float * RawData = (float*)FileContents.GetData();

        CompressedData.Reset(0);
        for(int32 i = 0; i < MapWidth * MapHeight; ++i) {
            CompressedData.Add((RawData[i]-300.f)/4);
        }

        float NewUpdateTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
        SendMapData(NewUpdateTime, CompressedData);
    }

    float TimeSinceMapUpdate = GetWorld()->GetGameState()->GetServerWorldTimeSeconds() - UpdateTime;
    const float LerpTime = 0.5f;
    const float LerpStart = 0.5f;

    TArray<float> HeightMapRaw;

    for (int32 i = 0; i < PreviousHeightMap.Num(); ++i)
    {
        HeightMapRaw.Add(FMath::Lerp(PreviousHeightMap[i], TargetHeightMap[i], FMath::Clamp((TimeSinceMapUpdate - LerpStart) / LerpTime, 0.0f, 1.0f)));
    }

    AddCutoffRegion(HeightMapRaw, HeightMap, -120.0f, MapSmootheningOffset);
    UpdateMeshFromHeightmap();
}

void ATerrainMeshActor::SendMapData_Implementation(float Time, const TArray<int8> & Data) {
    UpdateTime = Time;
    PreviousHeightMap = TargetHeightMap;

    TargetHeightMap.SetNum(Data.Num(), false);
    for (int32 i = 0; i < Data.Num(); ++i)
    {
        TargetHeightMap[i] = 300.f + 4.0f*Data[i];
    }
}

void ATerrainMeshActor::RequestMapUpdate_Implementation() {
    SendMapData(UpdateTime, CompressedData);
}

static inline float smoothLerp(float a, float b, float c) {
    return a + (b - a) * FMath::SmoothStep(0, 1, c);
}

void ATerrainMeshActor::AddCutoffRegion(const TArray<float>& Input, TArray<float>& Output, float CutoffHeight, int32 Detail)
{
    Output.Reset(Output.Num());

    // left part
    for (int32 X = 0; X < Detail; X++)
    {
        // left-bottom corner
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(Input[0], CutoffHeight, FMath::Sqrt(FMath::Square(X - Detail) + FMath::Square(Y - Detail)) / (Detail));
            Output.Add(HeightValue);
        }

        // left-middle part
        for(int32 Y = 0; Y < MapHeight; Y++)
        {
            float HeightValue = smoothLerp(CutoffHeight, Input[Y], float(X) / (Detail));
            Output.Add(HeightValue);
        }

        // left-top corner
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(Input[MapHeight - 1], CutoffHeight, FMath::Sqrt(FMath::Square(X - Detail) + FMath::Square(Y)) / (Detail));
            Output.Add(HeightValue);
        }
    }

    // middle part
    for (int32 X = 0; X < MapWidth; X++)
    {
        // middle-bottom part
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(CutoffHeight, Input[X * MapHeight], ((float)Y) / (Detail));
            Output.Add(HeightValue);
        }

        // middle-middle part
        for (int32 Y = 0; Y < MapHeight; ++Y) {
            Output.Add(Input[X * MapHeight + Y]);
        }

        // middle-top part
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(CutoffHeight, Input[X * MapHeight + MapHeight - 1], ((float)(Detail - Y)) / (Detail));
            Output.Add(HeightValue);
        }
    }

    // right part
    for (int32 X = 0; X < Detail; X++)
    {
        // right-bottom corner
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(Input[(MapWidth - 1) * MapHeight], CutoffHeight, FMath::Sqrt(FMath::Square(X) + FMath::Square(Detail - Y)) / Detail);
            Output.Add(HeightValue);
        }

        // right-middle part
        for(int32 Y = 0; Y < MapHeight; Y++)
        {
            float HeightValue = smoothLerp(Input[(MapWidth - 1) * MapHeight + Y], CutoffHeight, ((float)X) / (Detail));
            Output.Add(HeightValue);
        }

        // right-top corner
        for (int32 Y = 0; Y < Detail; Y++)
        {
            float HeightValue = smoothLerp(Input[(MapWidth - 1) * MapHeight + MapHeight - 1], CutoffHeight, FMath::Sqrt(FMath::Square(X) + FMath::Square(Y)) / Detail);
            Output.Add(HeightValue);
        }
    }
}

void ATerrainMeshActor::UpdateMeshFromHeightmap()
{
    for (int32 X = 0; X < MapWidthAbsolute; X++)
    {
        for (int32 Y = 0; Y < MapHeightAbsolute; Y++)
        {
            float Z = HeightMap[X * MapHeightAbsolute + Y];
            Vertices[X * MapHeightAbsolute + Y].Z = Z;

            // Calculate Normals and Tangents (check bounds for neighbors)
            float Z_NeighborX = (X < MapWidthAbsolute - 1) ? HeightMap[(X + 1) * MapHeightAbsolute + Y] : Z;
            float Z_NeighborY = (Y < MapHeightAbsolute - 1) ? HeightMap[X * MapHeightAbsolute + Y + 1] : Z;
            FVector U = FVector(MapGridSpacing, 0, Z_NeighborX - Z);
            FVector V = FVector(0, MapGridSpacing, Z_NeighborY - Z);
            Normals[X * MapHeightAbsolute + Y] = FVector::CrossProduct(U, V).GetUnsafeNormal();
            Tangents[X * MapHeightAbsolute + Y] = FProcMeshTangent(U, false);
        }
    }

    ProcMesh->UpdateMeshSection(0, Vertices, Normals, TArray<FVector2D>(), TArray<FColor>(), Tangents);
}
