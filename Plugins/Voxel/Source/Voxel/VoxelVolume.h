// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoxelProceduralGeneration/SignedDistanceField.h"
#include "VoxelProceduralGeneration/Examples/VPG_TestPerlin.h"
#include "Async/Async.h"
#include "VoxelTypes/Array3D.h"
#include "Tasks/Task.h"
#include "Tasks/Pipe.h"
#include "HAL/ThreadSafeCounter.h"

#include "VoxelVolume.generated.h"

class UProceduralMeshComponent;
class UVoxelProcMeshComponent;
class UVoxelProceduralGenerator;
class UVoxelChunkLodManager;
class UBoxComponent;
class VoxelChunkNode;
struct FVoxelChunkGenWorker;

UENUM()
enum EVoxelGenerationMethod : uint8
{
	MarchingCubesLerp
};

struct FVoxelChunkData
{
	FVoxelChunkData() {};
	FVoxelChunkData(VoxelChunkNode* InChunk, int InChunkResolution):
		Chunk(InChunk)
	{
		CornerValues.Init(FIntVector(InChunkResolution + 1), -1.0);
		Triangles.Add(0, TArray<int32>());
	}

	VoxelChunkNode* Chunk;
	FArray3D<double> CornerValues;
	TArray<FVector> Vertices;
	TMap<uint8, TArray<int32>> Triangles;
	TArray<uint8> VoxelMatIdPerSubmesh;
};

UCLASS()
class VOXEL_API AVoxelVolume : public AActor
{
	GENERATED_BODY()

	friend struct FVoxelChunkGenWorker;

protected:

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	void RegenerateChunk(FVoxelChunkData& OutChunkMeshData);
	void RemeshChunk(const FVoxelChunkData& InChunkMeshData);

	void ProcessRemeshQueue();
	void ProcessRegenerateQueue();
	void ProcessMeshCleanupQueue();
	void CleanupMeshes(VoxelChunkNode* InDeletionParent);

	bool RechunkVolume();
	void UpdateVolume();

	TQueue<TPair<FVoxelChunkData*, VoxelChunkNode*>> RegenQueue;
	TQueue<TPair<FVoxelChunkData*, VoxelChunkNode*>> RemeshQueue;
	TQueue<TPair<UVoxelProcMeshComponent*, VoxelChunkNode*>> MeshCleanupQueue;
	TArray<FVoxelChunkGenWorker> WorkerThreads;

public:

	AVoxelVolume();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UBoxComponent> BoundingBox;

	UPROPERTY()
	TObjectPtr<UVoxelChunkLodManager> ChunkLodManager;

	UPROPERTY()
	TObjectPtr<UVoxelProceduralGenerator> ProceduralGenerator;
    
	// Determines how active each voxel corner should be
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel")
	TSubclassOf<UVoxelProceduralGenerator> ProceduralGeneratorClass = UVPG_TestPerlin::StaticClass();
    
	// Total diameter of the volume's bounds
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel", Meta = (ClampMin = "1"))
	int MaxVolumeSize = 1048576;

	// Voxels per chunk
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel", Meta = (ClampMin = "1"))
	int ChunkResolution = 64;
	
	// Number of subdivisions the main chunk will get to provide more detail (should be near log2(ChunkResolution))
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel")
	uint8 MaxDepth = 7;
    
	// Theshold that determines the boundary between which corners should be considered fully active (where mesh is created)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel", Meta = (ClampMin = "0", ClampMax = "1"))
	double ActiveCornerThreshold = 1.0;
	
	// Method to determine where vertices/triangle should be placed in/on each voxel
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel")
	TEnumAsByte<EVoxelGenerationMethod> GenerationMethod = EVoxelGenerationMethod::MarchingCubesLerp;
	
	// Shows chunk boxes
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel|Debug")
	bool bDebugDrawChunkBoxes = false;
	
	// Number of chunks to evaluate per tick when rechunking
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel|Performance", Meta = (ClampMin = "1"))
	uint8 ChunkGenerationThreads = 3;
	
	// Time (seconds) thread should wait before rechunking again (to avoid eating fps)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel|Performance", Meta = (ClampMin = "0"))
	float ChunkGenerationThreadRest = 0.f;
	
	// Meshes to create of the generated chunks this tick
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel|Performance", Meta = (ClampMin = "1"))
	uint8 ChunkRemeshingTickLimit = 48;

	// Flip face normals
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bReverseTriangles;

	// Create simple convex collision
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool CreateCollisionMeshes = true;

	// Should call UpdateVolume() on tick
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool UpdateOnTick = true;
};
