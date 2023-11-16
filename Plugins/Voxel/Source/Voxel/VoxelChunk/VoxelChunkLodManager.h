// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "VoxelChunkLodManager.generated.h"

class UVoxelProcMeshComponent;
class UBoxComponent;


UENUM()
enum EVoxelChunkNode : uint8
{
    VCN_NotLeaf = 0,
    VCN_Leaf = 1,
};

class VoxelChunkNode
{
public:
	// 'n'th subdivision of the octree this node resides in (number of parent nodes)
	uint8 Depth;

	// Location in world space of the center of the chunk
	FVector Location;

	// Subdivided children chunk nodes, nullptrs if this is a leaf node, else full
	VoxelChunkNode* Children[8] = { nullptr };

	uint8 Flags = 0;

	// Mesh component to represent the chunk, nullptr if this not a leaf node
	UVoxelProcMeshComponent* Mesh = nullptr;

	VoxelChunkNode() :
		Depth(0),
		Location(FVector::ZeroVector) {};

	VoxelChunkNode(uint8 InDepth, const FVector& InLocation) :
		Depth(InDepth),
		Location(InLocation) {};

	~VoxelChunkNode()
	{
		ClearMesh();

		for (int i = 0; i < 8; i++)
		{
			if (Children[i])
			{
				delete Children[i];
				Children[i] = nullptr;
			}
		}
	}

	const bool IsLeaf() const { return Flags & VCN_Leaf; };
	void SetLeaf(bool value) { Flags = value ? Flags | VCN_Leaf : Flags & ~VCN_Leaf; };

	const double GetSize(float InMaxWorldSize) const
	{
		return InMaxWorldSize / exp2(Depth);
	}

	const FBox GetBox(float InMaxWorldSize) const
	{
		return FBox::BuildAABB(Location, FVector(GetSize(InMaxWorldSize) / 2.0));
	}

	void ClearMesh();
};

/**
 * 
 */
UCLASS()
class VOXEL_API UVoxelChunkLodManager : public UObject
{
    GENERATED_BODY()

public:
    static const FVector NodeOffsets[8];

    void Init(float InMaxWorldSize, uint8 InMaxDepth);

    float LODFactor = 1.f;

    VoxelChunkNode* RootNode = nullptr;
    float MaxWorldSize;

    // Max depth (chunk subdivisions),  Chunk Resolution will be (2 ^ MaxDepth) * (2 ^ MaxDepth) * (2 ^ MaxDepth)
    uint8 MaxDepth;

    void RechunkToCenter(
        const FVector& InLodCenter,
        TMap<VoxelChunkNode*, TArray<VoxelChunkNode*>>& OutGroupedDirtyChunks
    );

    void RechunkToCenter(
        const FVector& InLodCenter,
        TMap<VoxelChunkNode*, TArray<VoxelChunkNode*>>& OutGroupedDirtyChunks,
        VoxelChunkNode* InMeshNode,
        VoxelChunkNode* InParentPreviousLeaf = nullptr
    );

    void GetChildren(VoxelChunkNode* InMeshNode, TArray<VoxelChunkNode*>& OutChildren, bool bRecurse = false);
    FVector GetChildCenter(const FVector& InParentCenter, double InChildSize, int InChildIndex);
    bool IsWithinReach(const FVector& InTargetPosition, VoxelChunkNode* InMeshNode);
};
