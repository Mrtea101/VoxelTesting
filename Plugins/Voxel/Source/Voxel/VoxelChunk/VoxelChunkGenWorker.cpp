// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "VoxelChunkGenWorker.h"
#include "VoxelVolume.h"

bool FVoxelChunkGenWorker::Init()
{
	return ShouldRun();
}

uint32 FVoxelChunkGenWorker::Run()
{
	while (ShouldRun())
	{
		VoxelVolume->ProcessRegenerateQueue();
		if (VoxelVolume->ChunkGenerationThreadRest) FPlatformProcess::Sleep(VoxelVolume->ChunkGenerationThreadRest);
	}

	return 0;
}

void FVoxelChunkGenWorker::Stop()
{

}

void FVoxelChunkGenWorker::Exit()
{
	bIsDone = true;
}

bool FVoxelChunkGenWorker::ShouldRun()
{
	return VoxelVolume 
		&& IsValid(VoxelVolume)
		&& !VoxelVolume->RegenQueue.IsEmpty();
}

void FVoxelChunkGenWorker::Work()
{
	if (bIsDone)
	{
		delete Thread;
		Thread = nullptr;
		bIsDone = false;
	}

	if (!Thread && ShouldRun())
	{
		Thread = FRunnableThread::Create(this, TEXT("FVoxelChunkGenWorker"), 0, TPri_TimeCritical);
	}
}
