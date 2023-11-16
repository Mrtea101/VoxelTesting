// Fill out your copyright notice in the Description page of Project Settings.

#include "VoxelChunkLodManager.h"
#include "Components/BoxComponent.h"
#include "VoxelProceduralGeneration/VoxelProcMeshComponent.h"
#include "VoxelVolume.h"


void VoxelChunkNode::ClearMesh()
{
    if (Mesh && Mesh->IsValidLowLevel())
    {
        // delete debug box first (if exists)
        USceneComponent* box = Mesh->GetChildComponent(0);
        if (box) box->ConditionalBeginDestroy();

        Mesh->ConditionalBeginDestroy();
        Mesh = nullptr;
    }
}

const FVector UVoxelChunkLodManager::NodeOffsets[8] =
{
    FVector(-0.5f, -0.5f, -0.5f),
    FVector(-0.5f, -0.5f, 0.5f),
    FVector(-0.5f, 0.5f, -0.5f),
    FVector(-0.5f, 0.5f, 0.5f),
    FVector(0.5f, -0.5f, -0.5f),
    FVector(0.5f, -0.5f, 0.5f),
    FVector(0.5f, 0.5f, -0.5f),
    FVector(0.5f, 0.5f, 0.5f)
};

void UVoxelChunkLodManager::Init(float InMaxWorldSize, uint8 InMaxDepth)
{
    RootNode = new VoxelChunkNode();
    MaxWorldSize = InMaxWorldSize;
    MaxDepth = InMaxDepth;
}

void UVoxelChunkLodManager::RechunkToCenter(
    const FVector& InLodCenter,
    TMap<VoxelChunkNode*, TArray<VoxelChunkNode*>>& OutGroupedDirtyChunks
)
{
    if (!RootNode)
    {
        UE_LOG(LogTemp, Warning, TEXT("RootNode null"));
        return;
    }

    RechunkToCenter(InLodCenter, OutGroupedDirtyChunks, RootNode);
}

void UVoxelChunkLodManager::RechunkToCenter(
    const FVector& InLodCenter,
    TMap<VoxelChunkNode*, TArray<VoxelChunkNode*>>& OutGroupedDirtyChunks,
    VoxelChunkNode* InMeshNode,
    VoxelChunkNode* InParentPreviousLeaf
)
{
    if (!InMeshNode)
    {
        UE_LOG(LogTemp, Warning, TEXT("InMeshNode null"));
        return;
    }

    if (InMeshNode->Depth == MaxDepth // at max desired node depth, this will be a leaf
        || !IsWithinReach(InLodCenter, InMeshNode) // past range to expand this node, this will be a leaf
        )
    {
        if (!InMeshNode->IsLeaf()) // ensures old leafs aren't rechunked
        {
            InMeshNode->SetLeaf(true);

            // If we have a InParentPreviousLeaf, InMeshNode is the child of the less detailed node mesh which should be deleted
            if (InParentPreviousLeaf)
            {
                TArray<VoxelChunkNode*>& group = OutGroupedDirtyChunks.FindOrAdd(InParentPreviousLeaf);
                group.Add(InMeshNode);
            }
            else // InMeshNode is the parent of more detailed children (possibly not leafs) which should be deleted
            {
                TArray<VoxelChunkNode*>& group = OutGroupedDirtyChunks.FindOrAdd(InMeshNode);
            }
        }
    }
    else // this will not be a leaf, it's now a parent to potential leafs
    {
        // If this was a new leaf, we need to keep track of it for deletion
        // We pass it down the recursion until we get to the leafs for creation, then group them
        if (InMeshNode->IsLeaf())
        {
            InMeshNode->SetLeaf(false);
            InParentPreviousLeaf = InMeshNode;
        }

        // expand tree and recurse
        for (int i = 0; i < 8; i++)
        {
            if (!InMeshNode->Children[i])
            {
                InMeshNode->Children[i] = new VoxelChunkNode(InMeshNode->Depth + 1, GetChildCenter(InMeshNode->Location, InMeshNode->GetSize(MaxWorldSize) * 0.5f, i));
            }

            RechunkToCenter(InLodCenter, OutGroupedDirtyChunks, InMeshNode->Children[i], InParentPreviousLeaf);
        }
    }
}

void UVoxelChunkLodManager::GetChildren(VoxelChunkNode* InMeshNode, TArray<VoxelChunkNode*>& OutChildren, bool bRecurse)
{
    for (VoxelChunkNode* child : InMeshNode->Children)
    {
        if (child)
        {
            OutChildren.Add(child);
            if (bRecurse) GetChildren(child, OutChildren, true);
        }
    }
}

FVector UVoxelChunkLodManager::GetChildCenter(const FVector& InParentCenter, double InChildSize, int InChildIndex)
{
    return InParentCenter + NodeOffsets[InChildIndex] * InChildSize;
}

bool UVoxelChunkLodManager::IsWithinReach(const FVector& InTargetPosition, VoxelChunkNode* InMeshNode)
{
    const double meshNodeSize = InMeshNode->GetSize(MaxWorldSize);
    const FVector distanceToCenter = (InTargetPosition - InMeshNode->Location).GetAbs();
    const FVector boxExtends = FVector(meshNodeSize / 2.0);

    FVector v = (distanceToCenter - boxExtends).ComponentMax(FVector::ZeroVector);
    v *= v;

    const double distanceToNode = FMath::Sqrt(v.X + v.Y + v.Z);
    return distanceToNode < LODFactor * meshNodeSize;
}
