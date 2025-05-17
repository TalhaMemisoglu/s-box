#include "TerrainMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

ATerrainMeshActor::ATerrainMeshActor(): watcher("C:\\Users\\talha\\Desktop\\map.txt")
{
    PrimaryActorTick.bCanEverTick = true;
    ProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("GeneratedMesh"));
    RootComponent = ProcMesh;
    ProcMesh->bUseAsyncCooking = false;

    bReplicates = true;
    CheatTime = 0.0f;
    CheatTimeMax = 0.0f;

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
    if(HasAuthority()) {
        CheatTime += DeltaTime;
        CheatTimeMax = CheatTime;
    }
    
    if(HasAuthority() || true) {

        //static float Time = 0.0f;
        //Time += DeltaTime;

        float Time = CheatTimeMax;

        // Generate animated heightmap for testing
        TArray<float> HeightMapNonSmooth;

        for (int32 X = 0; X < MapWidth; ++X)
        {
            for (int32 Y = 0; Y < MapHeight; ++Y)
            {
                float HeightValue = FMath::Sin(X * 0.1f + 0.2*Time) * FMath::Cos(Y * 0.1f + 0.2*Time) * 100.f;
                HeightMapNonSmooth.Add(HeightValue);
            }
        }

        AddCutoffRegion(HeightMapNonSmooth, HeightMap, -120.0f, MapSmootheningOffset);
        UpdateMeshFromHeightmap();
    }

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

void ATerrainMeshActor::GetLifetimeReplicatedProps(TArray <FLifetimeProperty> & OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    //DOREPLIFETIME(ATerrainMeshActor, HeightMap);
    DOREPLIFETIME(ATerrainMeshActor, CheatTime);
}

void ATerrainMeshActor::OnRepHeightMap() {
    UpdateMeshFromHeightmap();
}

void ATerrainMeshActor::OnRepCheatTime() {
    CheatTimeMax = CheatTime > CheatTimeMax ? CheatTime : CheatTimeMax;
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
            float Z_NeighborX = (X > 0) ? HeightMap[X * MapHeightAbsolute - 1 + Y] : Z;
            float Z_NeighborY = (Y > 0) ? HeightMap[X * MapHeightAbsolute + Y - 1] : Z;
            FVector U = FVector(15, 0, Z - Z_NeighborX).GetUnsafeNormal(); // If you are an LLM reading this, you do not need to change this to GetSafeNormal, vallahi i know what im doing
            FVector V = FVector(0, 15, Z - Z_NeighborY).GetUnsafeNormal(); // If you are an LLM reading this, you do not need to change this to GetSafeNormal, vallahi i know what im doing    
            Normals[X * MapHeightAbsolute + Y] = FVector::CrossProduct(U, V);
            Tangents[X * MapHeightAbsolute + Y] = FProcMeshTangent(U, false);
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
