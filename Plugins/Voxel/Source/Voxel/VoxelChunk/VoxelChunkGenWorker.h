// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class AVoxelVolume;

struct FVoxelChunkGenWorker : public FRunnable
{
    FRunnableThread* Thread = nullptr;
    AVoxelVolume* VoxelVolume = nullptr;
    bool bIsDone = false;

    FVoxelChunkGenWorker(AVoxelVolume* InVoxelVolume) : VoxelVolume(InVoxelVolume) {};

    virtual ~FVoxelChunkGenWorker()
    {
        if (Thread)
        {
            delete Thread;
            Thread = nullptr;
        }
    };
    
    bool ShouldRun();
    void Work();

    // Begin FRunnable interface.
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
    virtual void Exit() override;
    // End FRunnable interface
};