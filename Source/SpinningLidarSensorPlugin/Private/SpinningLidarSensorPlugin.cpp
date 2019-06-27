// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SpinningLidarSensorPlugin.h"

#define LOCTEXT_NAMESPACE "FSpinningLidarSensorPluginModule"

void FSpinningLidarSensorPluginModule::StartupModule() {
    // This code will execute after your module is loaded into memory;
    // the exact timing is specified in the .uplugin file per-module
    InstalledPlugins.AddUnique(PluginNameEnum::SpinningLidarSensorPlugin);
}

void FSpinningLidarSensorPluginModule::ShutdownModule() {
    // This function may be called during shutdown to clean up your module.
    // For modules that support dynamic reloading,
    // we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FSpinningLidarSensorPluginModule, SpinningLidarSensorPlugin)

#ifdef ConfigurationPluginIncluded
    ASpinningLidarSensorActor* ASpinningLidarSensorPlugin::SpawnSpinningLidarsFromYAML(
        UWorld* const World, UDocumentNode *Node, AActor* ParentActor, FString* Error) {
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    ASpinningLidarSensorActor* NewSpinningLidar =
            World->SpawnActor<ASpinningLidarSensorActor>(ASpinningLidarSensorActor::StaticClass(),
                                                         Location, Rotation);

    NewSpinningLidar->SetParamsFromYaml(Node);
    if (!NewSpinningLidar->Error.IsEmpty()) {
        *Error += NewSpinningLidar->Error;
        NewSpinningLidar->Destroy();
        return nullptr;
    }

    NewSpinningLidar->Initialize();
    if (!NewSpinningLidar->Error.IsEmpty()) {
        *Error += NewSpinningLidar->Error;
        NewSpinningLidar->Destroy();
        return nullptr;
    }

    if (ParentActor) {
        NewSpinningLidar->AttachToActor(ParentActor,
                                        FAttachmentTransformRules
                                        (EAttachmentRule::KeepRelative, false));
    }

    return NewSpinningLidar;
}
#endif
