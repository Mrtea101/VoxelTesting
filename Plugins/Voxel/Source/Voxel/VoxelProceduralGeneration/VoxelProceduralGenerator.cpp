// Fill out your copyright notice in the Description page of Project Settings.

//#include "VoxelProceduralGenerator.h"
//
//UVoxelProceduralGenerator::UVoxelProceduralGenerator()
//{
//    check(BiomeMaterialData.Num() <= 254)
//}
//
//void UVoxelProceduralGenerator::GenerateVoxelData(const FVector& InChunkOrigin,
//    const FArray3D<FVector>& InLocations,
//    FArray3D<uint8>& OutMaterialIDs,
//    FArray3D<CornerIntersections>& OutIntersections
//)
//{
//    FArray3D<double> planetSurfaceCachedSDF = FArray3D<double>(InLocations.GetSize3D());
//    FArray3D<double> cavesCachedSDF = FArray3D<double>(InLocations.GetSize3D());
//    FArray3D<double> biomeMaterialsCachedSDF = FArray3D<double>(InLocations.GetSize3D());
//
//    PrecomputeDistances(
//        InChunkOrigin,
//        InLocations,
//        PlanetSurfaceSDF,
//        CavesSDF,
//        BiomeMaterialsSDF,
//        planetSurfaceCachedSDF,
//        cavesCachedSDF,
//        biomeMaterialsCachedSDF
//    );
//
//    GenerateProcData(
//        planetSurfaceCachedSDF,
//        cavesCachedSDF,
//        biomeMaterialsCachedSDF,
//        PlanetSurfaceIsovalue,
//        CavesIsovalue,
//        OutMaterialIDs,
//        OutIntersections
//    );
//}
//
//void UVoxelProceduralGenerator::PrecomputeDistances(
//    const FVector& InChunkOrigin,
//    const FArray3D<FVector>& InLocations,
//    const FSignedDistanceField& InPlanetSurfaceSDF,
//    const FSignedDistanceField& InCavesSDF,
//    const FSignedDistanceField& InBiomeMaterialsSDF,
//    FArray3D<double>& OutPlanetCachedSDF,
//    FArray3D<double>& OutCavesCachedSDF,
//    FArray3D<double>& OutBiomeMaterialsCachedSDF
//)
//{
//    for (int i = 0; i < InLocations.GetSizeTotal(); i++)
//    {
//        FVector location = InChunkOrigin + InLocations[i];
//        OutPlanetCachedSDF[i] = InPlanetSurfaceSDF.GetDistance(location); //HALF PLANET: if (location.X > 0) planetCachedSDF[i] = 0; 
//        OutCavesCachedSDF[i] = InCavesSDF.GetDistance(location);
//        OutBiomeMaterialsCachedSDF[i] = InBiomeMaterialsSDF.GetDistance(location);
//    }
//}
//
//int32 UVoxelProceduralGenerator::GetBiomeID(uint8 InMaterialID)
//{
//    int idx = InMaterialID - 1;
//    if (BiomeMaterialData.IsValidIndex(idx))
//    {
//        return idx;
//    }
//    return -1;
//}
//
//uint8 UVoxelProceduralGenerator::GetMaterial(
//    const FArray3D<double>& InPlanetCachedSDF,
//    const FArray3D<double>& InCavesCachedSDF,
//    const FArray3D<double>& InBiomeMaterialsCachedSDF,
//    float InPlanetSurfaceIsovalue,
//    float InCavesIsovalue,
//    int32 InCornerID
//)
//{
//    if (InCavesCachedSDF[InCornerID] < InCavesIsovalue)
//    {
//        return (uint8)0; // cave empty material
//    }
//    else if (InPlanetCachedSDF[InCornerID] < InPlanetSurfaceIsovalue)
//    {
//        for (uint8 i = BiomeMaterialData.Num() - 1; i >= 0; i--)
//        {
//            if (InBiomeMaterialsCachedSDF[InCornerID] < BiomeMaterialData[i].Isovalue)
//            {
//                return (uint8)(i + 1); // solid materialId
//            }
//        }
//        return (uint8)1;
//    }
//    else
//    {
//        return (uint8)255; // TODO: use other id temporarily to distinguish with cave empty
//    }
//}
//
//uint8 UVoxelProceduralGenerator::GetIntersectionValue(
//    const FArray3D<double>& InPlanetCachedSDF,
//    const FArray3D<double>& InCavesCachedSDF,
//    const FArray3D<double>& InBiomeMaterialsCachedSDF,
//    float InPlanetSurfaceIsovalue,
//    float InCavesIsovalue,
//    FArray3D<uint8>& OutMaterialIDs,
//    const FIntVector& InCornerID1,
//    const FIntVector& InCornerID2
//)
//{
//    uint8 materialId1 = OutMaterialIDs[InCornerID1];
//    uint8 materialId2 = OutMaterialIDs[InCornerID2];
//
//    double d1, d2;
//    if (materialId1 == 0 || materialId2 == 0) // use cavesSDF
//    {
//        d1 = InCavesCachedSDF[InCornerID1] - InCavesIsovalue;
//        d2 = InCavesCachedSDF[InCornerID2] - InCavesIsovalue;
//    }
//    else if (materialId1 == 255 || materialId2 == 255)   //use planetSurfaceSDF
//    {
//        d1 = InPlanetCachedSDF[InCornerID1] - InPlanetSurfaceIsovalue;
//        d2 = InPlanetCachedSDF[InCornerID2] - InPlanetSurfaceIsovalue;
//    }
//    else  //use biomeSDF
//    {
//        float isovalue = BiomeMaterialData[materialId1 + 1].Isovalue;
//        d1 = InBiomeMaterialsCachedSDF[InCornerID1] - isovalue;
//        d2 = InBiomeMaterialsCachedSDF[InCornerID2] - isovalue;
//    }
//
//    return (uint8)(FMath::Abs(d1) / FMath::Abs(d2 - d1) * 255);
//}
//
//void UVoxelProceduralGenerator::GenerateProcData(
//    const FArray3D<double>& InPlanetCachedSDF,
//    const FArray3D<double>& InCavesCachedSDF,
//    const FArray3D<double>& InBiomeMaterialsCachedSDF,
//    float InPlanetSurfaceIsovalue,
//    float InCavesIsovalue,
//    FArray3D<uint8>& OutMaterialIDs,
//    FArray3D<CornerIntersections>& OutIntersections
//)
//{
//    
//    auto IsActiveEdge = [](uint8 InMaterialID1, uint8 InMaterialID2)
//    {
//        if (InMaterialID1 == 255) InMaterialID1 = 0;
//        if (InMaterialID2 == 255) InMaterialID2 = 0;
//
//        return ((InMaterialID1 == 0 && InMaterialID2 != 0) || (InMaterialID1 != 0 && InMaterialID2 == 0));
//    };
//
//    // set materials
//    for (int i = 0; i < OutMaterialIDs.GetSizeTotal(); i++)
//    {
//        OutMaterialIDs[i] = GetMaterial(
//            InPlanetCachedSDF,
//            InCavesCachedSDF,
//            InBiomeMaterialsCachedSDF,
//            InPlanetSurfaceIsovalue,
//            InCavesIsovalue,
//            i
//        );
//    }
//
//    // set intersections
//    const FIntVector& Size3D = OutIntersections.GetSize3D();
//    for (int x = 0; x < Size3D.X; x++)
//    {
//        for (int y = 0; y < Size3D.Y; y++)
//        {
//            for (int z = 0; z < Size3D.Z; z++)
//            {
//                CornerIntersections cornerIntersections;
//
//                if (x + 1 < Size3D.X && IsActiveEdge(OutMaterialIDs[FIntVector(x, y, z)], OutMaterialIDs[FIntVector(x + 1, y, z)]))
//                {
//                    cornerIntersections.x = GetIntersectionValue(
//                        InPlanetCachedSDF,
//                        InCavesCachedSDF,
//                        InBiomeMaterialsCachedSDF,
//                        InPlanetSurfaceIsovalue,
//                        InCavesIsovalue,
//                        OutMaterialIDs,
//                        FIntVector(x, y, z),
//                        FIntVector(x + 1, y, z)
//                    );
//                }
//                if (y + 1 < Size3D.Y && IsActiveEdge(OutMaterialIDs[FIntVector(x, y, z)], OutMaterialIDs[FIntVector(x, y + 1, z)]))
//                {
//                    cornerIntersections.y = GetIntersectionValue(
//                        InPlanetCachedSDF,
//                        InCavesCachedSDF,
//                        InBiomeMaterialsCachedSDF,
//                        InPlanetSurfaceIsovalue,
//                        InCavesIsovalue,
//                        OutMaterialIDs,
//                        FIntVector(x, y, z),
//                        FIntVector(x, y + 1, z)
//                    );
//                }
//                if (z + 1 < Size3D.Z && IsActiveEdge(OutMaterialIDs[FIntVector(x, y, z)], OutMaterialIDs[FIntVector(x, y, z + 1)]))
//                {
//                    cornerIntersections.z = GetIntersectionValue(
//                        InPlanetCachedSDF,
//                        InCavesCachedSDF,
//                        InBiomeMaterialsCachedSDF,
//                        InPlanetSurfaceIsovalue,
//                        InCavesIsovalue,
//                        OutMaterialIDs,
//                        FIntVector(x, y, z),
//                        FIntVector(x, y, z + 1)
//                    );
//                }
//
//                OutIntersections[FIntVector(x, y, z)] = cornerIntersections;
//            }
//        }
//    }
//}