// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"
#include "ModuleManager.h"
#include "SpinningLidarSensorActor.h"

#if __has_include("ConfigurationPlugin.h")
#define ConfigurationPluginIncluded
#include "PluginMessaging.h"
#include "DocumentNode.h"
#endif
#include "SpinningLidarSensorPlugin.generated.h"

class FSpinningLidarSensorPluginModule : public IModuleInterface {
 public:
    /** IModuleInterface implementation */
    void StartupModule() override;
    void ShutdownModule() override;
};

UCLASS()
class SPINNINGLIDARSENSORPLUGIN_API ASpinningLidarSensorPlugin : public AActor {
    GENERATED_BODY()
 public:
    #ifdef ConfigurationPluginIncluded
    // Parse all the Spinning Lidar parameters from DocumentNode root
    static ASpinningLidarSensorActor* SpawnSpinningLidarsFromYAML(UWorld* const World,
                                                                  UDocumentNode *Node,
                                                                  AActor* ParentActor,
                                                                  FString* Error);
    #endif
};
