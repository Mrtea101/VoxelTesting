// Fill out your copyright notice in the Description page of Project Settings.


#include "VoxelProcMeshComponent.h"


UVoxelProcMeshComponent::UVoxelProcMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseComplexAsSimpleCollision = true;
	bUseAsyncCooking = true;
}
