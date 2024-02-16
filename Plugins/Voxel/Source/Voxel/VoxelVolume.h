
#pragma once

#include "CoreMinimal.h"
#include "RealtimeMeshActor.h"
#include "RealtimeMeshLibrary.h"
#include "RealtimeMeshSimple.h"

#include "VoxelVolume.generated.h"

class UBoxComponent;
struct FVoxelChunkNode;

UCLASS()
class VOXEL_API AVoxelVolume : public ARealtimeMeshActor
{
	GENERATED_BODY()

	AVoxelVolume();

protected:

	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	virtual void OnGenerateMesh_Implementation() override;
	void UpdateVolume();

	/* Generates mesh to the streamset, returns true if any triangles were generated */
	bool GenerateChunkMesh(FVoxelChunkNode* InChunk, FRealtimeMeshStreamSet& OutStreamSet);

	/* Simple signed distance field for a sphere */
	static double SDFSphere(const FVector& InLocation, double InRadius)
	{
		return InLocation.Length() / InRadius;
	};

	/* Get origin for lod calculations, usually player pawn location */
	bool GetLodOrigin(FVector& OutLocation);

	/* Calls RechunkToCenter with GetLodOrigin and RootNode */
	bool RechunkToCenter(TArray<FVoxelChunkNode*>& OutDirtyChunks);

	/* Recursively adds dirty chunk nodes (leaf to non-leaf and vice versa) to OutDirtyChunks */
	void RechunkToCenter(
		const FVector& InLodCenter,
		TArray<FVoxelChunkNode*>& OutDirtyChunks,
		FVoxelChunkNode* InMeshNode
	);

	// Keeps track of which section ids we have already used
	short NodeSectionIDTracker = 1;

	// Base node for the chunk octree
	FVoxelChunkNode* RootNode = nullptr;

public:

	// Simple bounding box visual for the editor 
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UBoxComponent> BoundingBox;
    
	// Total diameter of the volume's bounds
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel", Meta = (ClampMin = "1"))
	double VolumeExtent = 524288;

	// Voxels per chunk
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel", Meta = (ClampMin = "1"))
	int ChunkResolution = 16;
	
	// Number of subdivisions the main chunk will get to provide more detail (should be near log2(ChunkResolution))
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel")
	uint8 MaxDepth = 7;

	// Factor for chunk render distance
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel")
	float LodFactor = 1.f;
    
	// Threshold that determines the boundary between which corners should be considered fully active (where mesh is created)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel", Meta = (ClampMin = "0", ClampMax = "1"))
	double SurfaceIsovalue = 1.0;

	// Chunk depth, from most detailed, to create collision for
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel")
	uint8 CollisionInverseDepth = 3;
	
	// Number of materials to use
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Voxel", Meta = (ClampMin = "1"))
	uint8 NumMaterials = 1;
};