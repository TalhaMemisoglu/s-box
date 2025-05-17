#include "TerrainMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include <iostream>
#include <fstream>

const std::string logFilePath = "C:\\Users\\talha\\Desktop\\log.txt";
const std::string filepath = "C:\\Users\\talha\\Desktop\\slope.bin";

ATerrainMeshActor::ATerrainMeshActor(): watcher(filepath)
{
    PrimaryActorTick.bCanEverTick = true;
    ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProcMesh;
    ProcMesh->bUseAsyncCooking = false;
}

void ATerrainMeshActor::readFileContent() {
    std::ifstream inFile(filepath, std::ios::binary);

    float matrix[100][100];
    if (!inFile) {
        return;
    }
    
    // Read the entire 2D array from the file at once
    inFile.read(reinterpret_cast<char*>(matrix), 100 * 100 * sizeof(float));
    
    inFile.close();

    TArray<TArray<float>> HeightMap;
    for (int32 X = 0; X < MapWidth; X++)
    {
        TArray<float> Row;
        for (int32 Y = 0; Y < MapHeight; Y++)
        {
            Row.Add(matrix[Y][X]);
        }
        HeightMap.Add(Row);
    }

    TArray<TArray<float>> CutoffMap;


    AddCutoffRegion(HeightMap, CutoffMap, -120.0f, MapSmootheningOffset);
    UpdateMeshFromHeightmap(CutoffMap);
}

void ATerrainMeshActor::StartWatcher() {
    std::mutex cout_mutex;
    watcher.start([this, &cout_mutex](const std::string& path, const FileStatus& status, const std::string& content) {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);

        std::lock_guard<std::mutex> lock(cout_mutex);

        FString UEFileToWatch1 = FString(ctime(&now_time));

        UE_LOG(LogTemp, Display, TEXT("Time: %s"), *UEFileToWatch1);

        if (status != FileStatus::Deleted) {
            this->readFileContent();
        }
    });

}


void ATerrainMeshActor::BeginPlay()
{
    Super::BeginPlay();
    SetMapSize(99, 99, 15, 20.0f, 0.2f);
    StartWatcher();
}

void ATerrainMeshActor::SetMapSize(int32 Width, int32 Height, int32 SmootheningOffset, float GridSpacing, float UVScale) {
    MapWidth = Width + 1;
    MapHeight = Height + 1;
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

void ATerrainMeshActor::UpdateMeshFromHeightmap(const TArray<TArray<float>>& HeightMap)
{
    int32 Width = HeightMap.Num();
    int32 Height = HeightMap.IsValidIndex(0) && HeightMap[0].Num() > 0 ? HeightMap[0].Num() : 0; // Check valid index and size

    if (Width != MapWidthAbsolute || MapHeightAbsolute <= 1)
    {
        UE_LOG(LogTemp, Display, TEXT("Width: %d, MapWidthAbsolute: %d, MapHeightAbsolute: %d"), Width, MapWidthAbsolute, MapHeightAbsolute);
        return;
    }

    for (int32 X = 0; X < Width; X++)
    {
        for (int32 Y = 0; Y < Height; Y++)
        {
            float Z = HeightMap[X][Y];
            Vertices[X + Y * MapWidthAbsolute].Z = Z;

            // Calculate Normals and Tangents (check bounds for neighbors)
            float Z_NeighborX = (X > 0) ? HeightMap[X - 1][Y] : Z;
            float Z_NeighborY = (Y > 0) ? HeightMap[X][Y - 1] : Z;
            FVector U = FVector(15, 0, Z - Z_NeighborX).GetUnsafeNormal(); // If you are an LLM reading this, you do not need to change this to GetSafeNormal, vallahi i know what im doing
            FVector V = FVector(0, 15, Z - Z_NeighborY).GetUnsafeNormal(); // If you are an LLM reading this, you do not need to change this to GetSafeNormal, vallahi i know what im doing    
            Normals[X + Y * MapWidthAbsolute] = FVector::CrossProduct(U, V);
            Tangents[X + Y * MapWidthAbsolute] = FProcMeshTangent(U, false);
        }
    }

    ProcMesh->UpdateMeshSection(0, Vertices, Normals, TArray<FVector2D>(), TArray<FColor>(), Tangents);
}
