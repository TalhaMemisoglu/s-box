#include "TerrainMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ATerrainMeshActor::ATerrainMeshActor(): watcher("C:\\Users\\talha\\Desktop\\map.txt")
{
    PrimaryActorTick.bCanEverTick = true;
    ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProcMesh;
    ProcMesh->bUseAsyncCooking = false;

    //SetMapSize(100, 100, 15, 20.0f, 1.0f);

    /*
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
    */
}

void ATerrainMeshActor::StartWatcher() {
    std::mutex cout_mutex;
    watcher.start([&cout_mutex](const std::string& path, const FileStatus& status, const std::string& content) {
        auto now = std::chrono::system_clock::now();
        auto now_time = std::chrono::system_clock::to_time_t(now);

        std::lock_guard<std::mutex> lock(cout_mutex);

        //std::cout << std::string(50, '-') << std::endl;
        //std::cout << ctime(&now_time);

        FString UEFileToWatch1 = FString(ctime(&now_time));

        UE_LOG(LogTemp, Display, TEXT("Time: %s"), *UEFileToWatch1);

        switch (status) {
            case FileStatus::FirstTime:
                //std::cout << "Initial file read:" << std::endl;
                break;
            case FileStatus::Modified:
                //std::cout << "File was modified:" << std::endl;
                break;
            case FileStatus::Deleted:
                //std::cout << "File was deleted." << std::endl;
                //std::cout << "Exiting..." << std::endl;
                exit(0);
        }

        if (status != FileStatus::Deleted) {

            /* ************************************************************ */
            /*  TODO: This place will be changed to run with unreal engine  */
            /*  It may need to use some conditional variables etc.          */
            /* ************************************************************ */

            //std::cout << "Content:" << std::endl;
            //std::cout << content << std::endl;

            FString UEFileToWatch = FString(content.c_str());

            UE_LOG(LogTemp, Display, TEXT("Content: %s"), *UEFileToWatch);
        }
        //std::cout << std::string(50, '-') << std::endl;
        });

}


void ATerrainMeshActor::BeginPlay()
{
    Super::BeginPlay();
    SetMapSize(100, 100, 15, 20.0f, 0.2f);
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

    AddCutoffRegion(HeightMap, CutoffMap, -120.0f, MapSmootheningOffset);
    UpdateMeshFromHeightmap(CutoffMap);

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

void ATerrainMeshActor::UpdateMeshFromHeightmap(const TArray<TArray<float>>& HeightMap)
{
    int32 Width = HeightMap.Num();
    int32 Height = HeightMap.IsValidIndex(0) && HeightMap[0].Num() > 0 ? HeightMap[0].Num() : 0; // Check valid index and size

    if (Width != MapWidthAbsolute || MapHeightAbsolute <= 1)
    {
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

    /*
    // Empty UV arrays for channels we don't use (required by the function signature)
    TArray<FVector2D> EmptyUVs; // Re-use this for UV1, UV2, UV3

    // Revert to always creating the mesh section
    if (ProcMesh) // Check if ProcMesh is valid
    {
        // Ensure Triangles are calculated
        Triangles.Reset(); // Clear just in case
        Triangles.Reserve((Width - 1) * (Height - 1) * 6);
        for (int32 X = 0; X < Width - 1; X++)
        {
            for (int32 Y = 0; Y < Height - 1; Y++)
            {
                int32 A = X * Height + Y;
                int32 B = (X + 1) * Height + Y;
                int32 C = X * Height + (Y + 1);
                int32 D = (X + 1) * Height + (Y + 1);
                if (Vertices.IsValidIndex(A) && Vertices.IsValidIndex(B) && Vertices.IsValidIndex(C) && Vertices.IsValidIndex(D))
                {
                   Triangles.Add(A); Triangles.Add(C); Triangles.Add(B);
                   Triangles.Add(B); Triangles.Add(C); Triangles.Add(D);
                }
            }
        }
        // Always call CreateMeshSection with collision enabled
        ProcMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, EmptyUVs, EmptyUVs, EmptyUVs, LinearVertexColors, Tangents, true); // bCreateCollision = true
    }
    */
}
