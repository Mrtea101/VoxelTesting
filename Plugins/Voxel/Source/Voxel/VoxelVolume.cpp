// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelVolume.h"
#include "Kismet/KismetMathLibrary.h"
#include "Async/Async.h"
#include "VoxelUtilities/VoxelStatics.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "VoxelChunk/VoxelChunkLodManager.h"
#include "VoxelProceduralGeneration/VoxelProceduralGenerator.h"
#include "VoxelProceduralGeneration/VoxelProcMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "VoxelChunk/VoxelChunkGenWorker.h"


AVoxelVolume::AVoxelVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	UBillboardComponent* billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	billboard->SetupAttachment(RootComponent);

	BoundingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	BoundingBox->SetupAttachment(RootComponent);
	BoundingBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ChunkLodManager = CreateDefaultSubobject<UVoxelChunkLodManager>(MakeUniqueObjectName(this, UVoxelChunkLodManager::StaticClass()));
}

void AVoxelVolume::BeginPlay()
{
	Super::BeginPlay();

	ChunkLodManager->Init(MaxVolumeSize, MaxDepth);

	if (ensure(ProceduralGeneratorClass))
	{
		if (ProceduralGenerator && IsValid(ProceduralGenerator)) ProceduralGenerator->ConditionalBeginDestroy();
		ProceduralGenerator = NewObject<UVoxelProceduralGenerator>(this, ProceduralGeneratorClass.Get(), MakeUniqueObjectName(this, UVoxelProceduralGenerator::StaticClass()));
		ProceduralGenerator->Init(MaxVolumeSize);
	}

	WorkerThreads.Reserve(ChunkGenerationThreads);
	for (int i = 0; i < ChunkGenerationThreads; i++)
	{
		WorkerThreads.Add(FVoxelChunkGenWorker(this));
	}

	UpdateVolume();
}

void AVoxelVolume::OnConstruction(const FTransform& Transform)
{
	BoundingBox->SetBoxExtent(FVector(MaxVolumeSize / 2.f));
	if (HasActorBegunPlay()) UpdateVolume();
}

void AVoxelVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (UpdateOnTick) UpdateVolume();
}

void AVoxelVolume::UpdateVolume()
{
	RechunkVolume();

	if (!RegenQueue.IsEmpty())
	{
		for (int i = 0; i < ChunkGenerationThreads; i++)
		{
			WorkerThreads[i].Work();
		}
	}

	for (int tickRemeshes = 0; !RemeshQueue.IsEmpty() && tickRemeshes < ChunkRemeshingTickLimit; tickRemeshes++)
	{
		ProcessRemeshQueue();
	}

	for (int tickMeshCleanups = 0; !MeshCleanupQueue.IsEmpty() && tickMeshCleanups < ChunkRemeshingTickLimit; tickMeshCleanups++)
	{
		ProcessMeshCleanupQueue();
	}
}

bool AVoxelVolume::RechunkVolume()
{
	if (RegenQueue.IsEmpty() && RemeshQueue.IsEmpty())
	{
		TMap<VoxelChunkNode*, TArray<VoxelChunkNode*>> DirtyChunkGroups;
		{
			FVector lodCenter(0);

			if (const UWorld* world = GetWorld())
			{
				if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
				{
					if (APawn* pawn = PC->GetPawn())
					{
						lodCenter = UKismetMathLibrary::InverseTransformLocation(GetActorTransform(), pawn->GetActorLocation());
					}
				}
			}

			ChunkLodManager->RechunkToCenter(lodCenter, DirtyChunkGroups);

			if (DirtyChunkGroups.IsEmpty()) return false;
		}

		for (TPair<VoxelChunkNode*, TArray<VoxelChunkNode*>>& group : DirtyChunkGroups)
		{
			TArray<FVoxelChunkData*> chunkDataRefs;

			// If the key is a leaf, it's the parent that needs creation, its children need destruction (and node deletion)
			// Note: the value array is empty, use parents direct children and recurse
			if (group.Key->IsLeaf())
			{
				chunkDataRefs.Add(new FVoxelChunkData(group.Key, ChunkResolution));
			}
			// If the key is not a leaf, it's a parent node that was a leaf but needs deletion, the value array children need creation
			// Note: the value array nodes possibly have greater than 1 depth from parent (could be more than 8)
			else
			{
				for (VoxelChunkNode* leaf : group.Value)
					chunkDataRefs.Add(new FVoxelChunkData(leaf, ChunkResolution));
			}

			for (FVoxelChunkData* chunkDataRef : chunkDataRefs)
			{
				RegenQueue.Enqueue({ chunkDataRef, group.Key });
			}
		}

		return true;
	}

	return false;
}

void AVoxelVolume::ProcessRegenerateQueue()
{
	if (!RegenQueue.IsEmpty())
	{
		TPair<FVoxelChunkData*, VoxelChunkNode*> chunkData_parent;
		if (RegenQueue.Dequeue(chunkData_parent) && chunkData_parent.Key && chunkData_parent.Value)
		{
			RegenerateChunk(*chunkData_parent.Key);
			RemeshQueue.Enqueue(chunkData_parent);
		}
	}
}

void AVoxelVolume::ProcessRemeshQueue()
{
	TPair<FVoxelChunkData*, VoxelChunkNode*> chunkData_parent;
	if (RemeshQueue.Dequeue(chunkData_parent) && chunkData_parent.Key && chunkData_parent.Value)
	{
		// create mesh if chunk has triangles (todo check material)
		if (chunkData_parent.Key->Triangles.Num() && chunkData_parent.Key->Triangles[0].Num())
		{
			RemeshChunk(*chunkData_parent.Key);
			MeshCleanupQueue.Enqueue({ chunkData_parent.Key->Chunk->Mesh, chunkData_parent.Value });
		}
		else
		{
			CleanupMeshes(chunkData_parent.Value);
		}

		delete chunkData_parent.Key;
	}
}

void AVoxelVolume::ProcessMeshCleanupQueue()
{
	TPair<UVoxelProcMeshComponent*, VoxelChunkNode*> creationMesh_deletionParent;
	if (!MeshCleanupQueue.Peek(creationMesh_deletionParent) || !creationMesh_deletionParent.Value)
	{
		MeshCleanupQueue.Pop();
		return;
	}

	if (!creationMesh_deletionParent.Key || creationMesh_deletionParent.Key->IsRenderStateDirty())
	{
		MeshCleanupQueue.Pop();
		CleanupMeshes(creationMesh_deletionParent.Value);
	}
}

void AVoxelVolume::CleanupMeshes(VoxelChunkNode* InDeletionParent)
{
	if (InDeletionParent->IsLeaf())
	{
		for (int i = 0; i < 8; i++)
		{
			delete InDeletionParent->Children[i];
			InDeletionParent->Children[i] = nullptr;
		}
	}
	else
	{
		InDeletionParent->ClearMesh();
	}
}

void AVoxelVolume::RegenerateChunk(FVoxelChunkData& OutChunkMeshData)
{
	float time = UGameplayStatics::GetTimeSeconds(this);

	const int edgeCount = ChunkResolution + 1;
	const double chunkSize = OutChunkMeshData.Chunk->GetSize(MaxVolumeSize);
	const double chunkExtent = chunkSize / 2.0;
	const double voxelSize = chunkSize / ChunkResolution;
	const double voxelExtent = voxelSize / 2.0;

	FArray3D<double>& cornerValues = OutChunkMeshData.CornerValues;
	TArray<int32>& TriangleIndices = OutChunkMeshData.Triangles[0];

	// Try to make allocations outside the loop
	double cornerValuesBuffer[8];
	FVector edgeVertices[12];
	FIntVector cornerLocationIndex;
	FVector cornerLocationWorld;
	int idxFlag = 0;
	int edgeFlags = 0;
	int i = 0;
	int j = 0;
	double edgeOffset = 0.0;
	double c1 = 0.0;
	double c2 = 0.0;

	// Start marching cubes
	for (int x = 0; x < ChunkResolution; x++)
	{
		for (int y = 0; y < ChunkResolution; y++)
		{
			for (int z = 0; z < ChunkResolution; z++)
			{
				// Find values at the cube's corners
				for (i = 0; i < 8; i++)
				{
					cornerLocationIndex.X = x + (int)VoxelStatics::a2fVertexOffset[i][0];
					cornerLocationIndex.Y = y + (int)VoxelStatics::a2fVertexOffset[i][1];
					cornerLocationIndex.Z = z + (int)VoxelStatics::a2fVertexOffset[i][2];

					if (cornerValues[cornerLocationIndex] == -1.0)
					{
						cornerLocationWorld.Set(
							OutChunkMeshData.Chunk->Location.X - chunkExtent + cornerLocationIndex.X * voxelSize,
							OutChunkMeshData.Chunk->Location.Y - chunkExtent + cornerLocationIndex.Y * voxelSize,
							OutChunkMeshData.Chunk->Location.Z - chunkExtent + cornerLocationIndex.Z * voxelSize
						);
						cornerValues[cornerLocationIndex] = ProceduralGenerator->GenerateProceduralValue(cornerLocationWorld, GetActorLocation(), time);
					}

					cornerValuesBuffer[i] = cornerValues[cornerLocationIndex];
				}

				// Find which vertices are inside of the surface and which are outside
				idxFlag = 0;
				for (i = 0; i < 8; i++)
				{
					if (cornerValuesBuffer[i] <= ActiveCornerThreshold)
						idxFlag |= 1 << i;
				}

				// Find which edges are intersected by the surface
				edgeFlags = VoxelStatics::aiCubeEdgeFlags[idxFlag];

				// If the cube is entirely inside or outside of the surface,
				// then there will be no intersections, continue to next cube
				if (!edgeFlags) continue;

				// Find the point of intersection of the surface with each edge
				for (i = 0; i < 12; i++)
				{
					//if there is an intersection on this edge
					if (edgeFlags & (1 << i))
					{
						c1 = cornerValuesBuffer[VoxelStatics::a2iEdgeConnection[i][0]];
						c2 = cornerValuesBuffer[VoxelStatics::a2iEdgeConnection[i][1]];
						edgeOffset = c1 == c2 ? 0.5 : FMath::Clamp((ActiveCornerThreshold - c1) / (c2 - c1), 0.0, 1.0);

						edgeVertices[i].X = (x + VoxelStatics::a2fVertexOffset[VoxelStatics::a2iEdgeConnection[i][0]][0]
							+ edgeOffset * VoxelStatics::a2fEdgeDirection[i][0]) * voxelSize - chunkExtent;

						edgeVertices[i].Y = (y + VoxelStatics::a2fVertexOffset[VoxelStatics::a2iEdgeConnection[i][0]][1]
							+ edgeOffset * VoxelStatics::a2fEdgeDirection[i][1]) * voxelSize - chunkExtent;

						edgeVertices[i].Z = (z + VoxelStatics::a2fVertexOffset[VoxelStatics::a2iEdgeConnection[i][0]][2]
							+ edgeOffset * VoxelStatics::a2fEdgeDirection[i][2]) * voxelSize - chunkExtent;
					}
					else
					{
						edgeVertices[i] = FVector::ZeroVector;
					}
				}

				//Draw the triangles that were found, there can be up to five per cube
				for (i = 0; i < 5; i++)
				{
					if (VoxelStatics::a2iTriangleConnectionTable[idxFlag][3 * i] < 0)
						break;

					for (j = 0; j < 3; j++)
					{
						if (!bReverseTriangles)
						{
							TriangleIndices.Add(
								OutChunkMeshData.Vertices.Add(
									edgeVertices[VoxelStatics::a2iTriangleConnectionTable[idxFlag][3 * i + j]]
								)
							);
						}
					}

					if (bReverseTriangles)
					{
						TriangleIndices.Add(OutChunkMeshData.Vertices.Num() - 1);
						TriangleIndices.Add(OutChunkMeshData.Vertices.Num() - 2);
						TriangleIndices.Add(OutChunkMeshData.Vertices.Num() - 3);
					}
				}
			}
		}
	}
}

void AVoxelVolume::RemeshChunk(const FVoxelChunkData& InChunkMeshData)
{
	UVoxelProcMeshComponent* newMesh = NewObject<UVoxelProcMeshComponent>(this);
	newMesh->SetupAttachment(RootComponent);
	newMesh->RegisterComponent();
	newMesh->SetRelativeLocation(InChunkMeshData.Chunk->Location);

	if (bDebugDrawChunkBoxes)
	{
		UBoxComponent* box = NewObject<UBoxComponent>(newMesh);
		box->SetupAttachment(newMesh);
		box->RegisterComponent();
		box->SetHiddenInGame(false);
		box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		box->SetBoxExtent(FVector(InChunkMeshData.Chunk->GetSize(MaxVolumeSize) / 2));
	}

	newMesh->CreateMeshSection_LinearColor(
		0,
		InChunkMeshData.Vertices,
		InChunkMeshData.Triangles[0],
		TArray<FVector>(),
		TArray<FVector2D>(),
		TArray<FLinearColor>(),
		TArray<FProcMeshTangent>(),
		CreateCollisionMeshes && InChunkMeshData.Chunk->Depth >= ChunkLodManager->MaxDepth - 1
	);

	InChunkMeshData.Chunk->Mesh = newMesh;
}
