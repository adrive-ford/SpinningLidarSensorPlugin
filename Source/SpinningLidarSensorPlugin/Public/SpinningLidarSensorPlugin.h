// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine.h"
#include "ModuleManager.h"
#include "SpinningLidarSensorActor.h"
#include "SpinningLidarSensorPlugin.generated.h"

class FSpinningLidarSensorPluginModule : public IModuleInterface {
 public:
    /** IModuleInterface implementation */
    void StartupModule() override;
    void ShutdownModule() override;
};
