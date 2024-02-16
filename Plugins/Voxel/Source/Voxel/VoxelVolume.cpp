
#include "VoxelVolume.h"

#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"

#include "RealtimeMeshLibrary.h"
#include "RealtimeMeshSimple.h"
#include "Mesh/RealtimeMeshBasicShapeTools.h"
#include "Mesh/RealtimeMeshBuilder.h"
#include "Mesh/RealtimeMeshSimpleData.h"

#include "VoxelChunk/VoxelChunkNode.h"
#include "VoxelUtilities/VoxelStatics.h"
#include "VoxelUtilities/Array3D.h"


AVoxelVolume::AVoxelVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	BoundingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	BoundingBox->SetupAttachment(RootComponent);
	BoundingBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AVoxelVolume::BeginPlay()
{
	Super::BeginPlay();

	OnGenerateMesh();
}

void AVoxelVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	BoundingBox->SetBoxExtent(FVector(VolumeExtent));
}

void AVoxelVolume::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	UpdateVolume();

	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
}

void AVoxelVolume::OnGenerateMesh_Implementation()
{
	Super::OnGenerateMesh_Implementation();

	URealtimeMeshSimple* RealtimeMesh = GetRealtimeMeshComponent()->InitializeRealtimeMesh<URealtimeMeshSimple>();

	for (uint8 i = 0; i < NumMaterials; i++)
	{
		RealtimeMesh->SetupMaterialSlot(i, FName("Material_", i));
	}

	if (RootNode)
	{
		delete RootNode;
	}

	RootNode = new FVoxelChunkNode();

	NodeSectionIDTracker = 1;

	UpdateVolume();
}

void AVoxelVolume::UpdateVolume()
{
	if (URealtimeMeshSimple* RealtimeMesh = GetRealtimeMeshComponent()->GetRealtimeMeshAs<URealtimeMeshSimple>())
	{
		// Check for dirty chunks
		TArray<FVoxelChunkNode*> DirtyChunks;
		if (RechunkToCenter(DirtyChunks))
		{
			for (FVoxelChunkNode* dirtyChunk : DirtyChunks)
			{
				check(dirtyChunk)

				if (dirtyChunk->IsLeaf())
				{
					// Remove children meshes (if they were previous leaf nodes)
					for (auto childChunk : dirtyChunk->Children)
					{
						if (childChunk and childChunk->IsLeaf())
						{
							FName name = childChunk->GetSectionName();
							const auto SectionGroupKey = FRealtimeMeshSectionGroupKey::Create(0, name);
							RealtimeMesh->RemoveSectionGroup(SectionGroupKey).Wait();
							childChunk->SectionID = 0;
							childChunk->SetLeaf(false);
						}
					}

					// Create new leaf node mesh
					FRealtimeMeshStreamSet StreamSet;
					if (GenerateChunkMesh(dirtyChunk, StreamSet))
					{
						dirtyChunk->SectionID = NodeSectionIDTracker++;

						FName name = dirtyChunk->GetSectionName();
						const auto SectionGroupKey = FRealtimeMeshSectionGroupKey::Create(0, name);

						RealtimeMesh->CreateSectionGroup(SectionGroupKey, StreamSet);

						const bool bShouldCreateCollision = MaxDepth - dirtyChunk->Depth + 1 <= CollisionInverseDepth;
						RealtimeMesh->UpdateSectionConfig
						(
							FRealtimeMeshSectionKey::CreateForPolyGroup(SectionGroupKey, 0),
							FRealtimeMeshSectionConfig(ERealtimeMeshSectionDrawType::Static, 0),
							bShouldCreateCollision
						);
					}
				}
				else
				{
					// Remove new non-leaf node mesh
					FName name = dirtyChunk->GetSectionName();
					const auto SectionGroupKey = FRealtimeMeshSectionGroupKey::Create(0, name);
					RealtimeMesh->RemoveSectionGroup(SectionGroupKey).Wait();
					dirtyChunk->SectionID = 0;
				}
			}
		}
	}
}

bool AVoxelVolume::GenerateChunkMesh(FVoxelChunkNode* InChunk, FRealtimeMeshStreamSet& OutStreamSet)
{
	const FVector volumeLocation(GetActorLocation());
	const FVector3f chunkLocation(InChunk->Location);
	const int edgeCount = ChunkResolution + 1;
	const double chunkExtent = InChunk->GetExtent(VolumeExtent);
	const double chunkSize = chunkExtent * 2;
	const double voxelExtent = chunkExtent / ChunkResolution;
	const double voxelSize = voxelExtent * 2;

	FArray3D<double> densityValues = FArray3D<double>(FIntVector(ChunkResolution + 1), -1.0);
	TRealtimeMeshBuilderLocal<uint32, FPackedNormal, FVector2DHalf, 1> builder(OutStreamSet);
	builder.EnableTangents();
	builder.EnableTexCoords();
	builder.EnablePolyGroups();
	builder.EnableColors();

	// Try to make allocations outside the loop
	uint32 numTris = 0;
	double densityBuffer[8];
	FVector3f edgeVertexBuffer[12];
	FVector3f edgeNormal;
	int idxFlag = 0;
	int x = 0;
	int y = 0;
	int z = 0;
	int i = 0;

	// Start marching cubes
	for (x = 0; x < ChunkResolution; x++)
	{
		for (y = 0; y < ChunkResolution; y++)
		{
			for (z = 0; z < ChunkResolution; z++)
			{
				// Find values at the cube's corners
				for (i = 0; i < 8; i++)
				{
					const FIntVector cornerLocationIndex =
					{
						x + (int)VoxelStatics::a2fVertexOffset[i][0],
						y + (int)VoxelStatics::a2fVertexOffset[i][1],
						z + (int)VoxelStatics::a2fVertexOffset[i][2]
					};

					if (densityValues[cornerLocationIndex] == -1.0)
					{
						const FVector cornerLocationWorld =
						{
							chunkLocation.X - chunkExtent + cornerLocationIndex.X * voxelSize,
							chunkLocation.Y - chunkExtent + cornerLocationIndex.Y * voxelSize,
							chunkLocation.Z - chunkExtent + cornerLocationIndex.Z * voxelSize
						};

						densityValues[cornerLocationIndex] = SDFSphere(cornerLocationWorld, VolumeExtent);
					}
					
					densityBuffer[i] = densityValues[cornerLocationIndex];
				}

				// Find which vertices are inside of the surface and which are outside
				idxFlag = 0;
				for (i = 0; i < 8; i++)
				{
					if (densityBuffer[i] < SurfaceIsovalue)
						idxFlag |= 1 << i;
				}

				// Find which edges are intersected by the surface
				const int edgeFlags = VoxelStatics::aiCubeEdgeFlags[idxFlag];

				// If the cube is entirely inside or outside of the surface,
				// then there will be no intersections, continue to next cube
				if (!edgeFlags) continue;

				// Find the point of intersection of the surface with each edge
				for (i = 0; i < 12; i++)
				{
					//if there is an intersection on this edge
					if (edgeFlags & (1 << i))
					{
						const double c1 = densityBuffer[VoxelStatics::a2iEdgeConnection[i][0]];
						const double c2 = densityBuffer[VoxelStatics::a2iEdgeConnection[i][1]];
						const double edgeOffset = c1 == c2 ? 0.5 : FMath::Clamp((SurfaceIsovalue - c1) / (c2 - c1), 0.0, 1.0);

						edgeVertexBuffer[i].Set(
							VoxelStatics::a2fVertexOffset[VoxelStatics::a2iEdgeConnection[i][0]][0] + x
							+ VoxelStatics::a2fEdgeDirection[i][0] * edgeOffset,

							VoxelStatics::a2fVertexOffset[VoxelStatics::a2iEdgeConnection[i][0]][1] + y
							+ VoxelStatics::a2fEdgeDirection[i][1] * edgeOffset,

							VoxelStatics::a2fVertexOffset[VoxelStatics::a2iEdgeConnection[i][0]][2] + z
							+ VoxelStatics::a2fEdgeDirection[i][2] * edgeOffset
						);

						edgeVertexBuffer[i] *= voxelSize;
						edgeVertexBuffer[i] += chunkLocation - chunkExtent;
					}
				}

				//Draw the triangles that were found, there can be up to five per cube
				for (i = 0; i < 5; i++)
				{
					const uint8 idxTableVertex = i * 3;
					if (VoxelStatics::a2iTriangleConnectionTable[idxFlag][idxTableVertex] < 0) break;

					const uint8 idxVertexA = VoxelStatics::a2iTriangleConnectionTable[idxFlag][idxTableVertex];
					const uint8 idxVertexB = VoxelStatics::a2iTriangleConnectionTable[idxFlag][idxTableVertex + 1];
					const uint8 idxVertexC = VoxelStatics::a2iTriangleConnectionTable[idxFlag][idxTableVertex + 2];

					edgeNormal = FVector3f::CrossProduct(
						edgeVertexBuffer[idxVertexC] - edgeVertexBuffer[idxVertexA],
						edgeVertexBuffer[idxVertexB] - edgeVertexBuffer[idxVertexA]
					);

					edgeNormal.Normalize();

					const uint32 ia = builder.AddVertex(edgeVertexBuffer[idxVertexA])
						.SetNormalAndTangent(edgeNormal, FVector3f(0, 1, 0))
						.SetTexCoords(FVector2D())
						.GetIndex();

					const uint32 ib = builder.AddVertex(edgeVertexBuffer[idxVertexB])
						.SetNormalAndTangent(edgeNormal, FVector3f(0, 1, 0))
						.SetTexCoords(FVector2D())
						.GetIndex();

					const uint32 ic = builder.AddVertex(edgeVertexBuffer[idxVertexC])
						.SetNormalAndTangent(edgeNormal, FVector3f(0, 1, 0))
						.SetTexCoords(FVector2D())
						.GetIndex();

					builder.AddTriangle(ia, ib, ic, 0);

					numTris++;
				}
			}
		}
	}

	return numTris != 0;
}

bool AVoxelVolume::GetLodOrigin(FVector& OutLocation)
{
	if (const UWorld* world = GetWorld())
	{
		// first we check player pawn position
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* pawn = PC->GetPawn())
			{
				OutLocation = UKismetMathLibrary::InverseTransformLocation(GetActorTransform(), pawn->GetActorLocation());
				return true;
			}
		}
		// else use top center if not up for play
		else
		{
			OutLocation = GetActorLocation() + FVector(0, 0, VolumeExtent);
			return true;
		}
	}

	return false;
}

bool AVoxelVolume::RechunkToCenter(TArray<FVoxelChunkNode*>& OutDirtyChunks)
{
	FVector lodCenter(0);
	check(GetLodOrigin(lodCenter))
	check(RootNode)

	RechunkToCenter(lodCenter, OutDirtyChunks, RootNode);

	return OutDirtyChunks.Num() != 0;
}

void AVoxelVolume::RechunkToCenter(
	const FVector& InLodCenter,
	TArray<FVoxelChunkNode*>& OutDirtyChunks,
	FVoxelChunkNode* InMeshNode
)
{
	check(InMeshNode)

	if (InMeshNode->Depth == MaxDepth // at max desired node depth, this will be a leaf
		|| !InMeshNode->IsWithinReach(InLodCenter, VolumeExtent, LodFactor) // past range to expand this node, this will be a leaf
		)
	{
		if (!InMeshNode->IsLeaf()) // only mark new leaf nodes dirty
		{
			InMeshNode->SetLeaf(true);
			OutDirtyChunks.Add(InMeshNode);
		}
	}
	else // this will not be a leaf, it's a parent to potential leafs
	{
		if (InMeshNode->IsLeaf()) // only mark new non-leaf nodes dirty
		{
			InMeshNode->SetLeaf(false);
			OutDirtyChunks.Add(InMeshNode);
		}

		// expand tree and recurse
		for (int i = 0; i < 8; i++)
		{
			if (!InMeshNode->Children[i])
			{
				InMeshNode->Children[i] = new FVoxelChunkNode(InMeshNode->Depth + 1, InMeshNode->GetChildCenter(i, VolumeExtent));
			}

			RechunkToCenter(InLodCenter, OutDirtyChunks, InMeshNode->Children[i]);
		}

	}
}