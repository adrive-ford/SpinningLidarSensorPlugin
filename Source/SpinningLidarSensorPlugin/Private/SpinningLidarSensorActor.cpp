// Fill out your copyright notice in the Description page of Project Settings.


#include "SpinningLidarSensorActor.h"
#include "SpinningLidarSensorPlugin.h"
#include <random>
#include <math.h>
#include "Kismet/KismetMathLibrary.h"
#include "Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h"

// Sets default values, including meshes
ASpinningLidarSensorActor::ASpinningLidarSensorActor() {
    // Set this actor to call Tick() every frame.
    // You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    // Root component: a dummy for other components to move
    // relative to--for example, as the lidar scans,
    // the mesh and laser directions rotate relative to this root.
    RootCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("RootCapsule"));
    RootCapsule->InitCapsuleSize(0.1f, 0.1f);
    RootComponent = RootCapsule;

    // Import static mesh to visualize the sensor itself
    static ConstructorHelpers::FObjectFinder<UStaticMesh>
            LidarMeshLoad(TEXT("/SpinningLidarSensorPlugin/Shape_Cylinder.Shape_Cylinder"));
    if (LidarMeshLoad.Succeeded()) {
        LidarMesh = LidarMeshLoad.Object;
    }
    LidarMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidarMesh"));
    LidarMeshComponent->SetStaticMesh(LidarMesh);

    static ConstructorHelpers::FObjectFinder<UMaterial>
            LidarMaterialLoad(TEXT("/SpinningLidarSensorPlugin/M_Basic_Wall.M_Basic_Wall"));
    if (LidarMaterialLoad.Succeeded()) {
        LidarMaterial = LidarMaterialLoad.Object;
    }
    LidarMeshComponent->SetMaterial(0, LidarMaterial);

    // Scale according to Velodyne dimensions, from the default cylinder mesh
    // dimensions of 100cm height, 100cm diameter
    LidarMeshComponent->SetWorldScale3D(FVector(0.0853f, 0.0853f, 0.1442f));

    LidarMeshComponent->SetupAttachment(RootComponent);
    LidarMeshComponent->SetMobility(EComponentMobility::Movable);


    // Add a scene capture component to write the view from the sensor to a texture,
    // so that colors can be sampled from the world at the beam locations.
    SceneCap = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
    SceneCap->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
    RenderTexture = NewObject<UTextureRenderTarget2D>();
    RenderTexture->InitAutoFormat(RenderTextureDimensions.X, RenderTextureDimensions.Y);
    SceneCap->FOVAngle = 90.f;
    if (abs(MaxElevation - MinElevation) > 70.f) SceneCap->FOVAngle =
            abs(MaxElevation - MinElevation) + 20.f;
    SceneCap->TextureTarget = RenderTexture;
    SceneCap->SetupAttachment(LidarMeshComponent);
    SceneCap->SetRelativeRotation(FRotator((MaxElevation + MinElevation) / 2.f, 0, 0));
    SceneCap->SetRelativeLocation(FVector(0, 0, BeamStartRelativeZ));
}

// Called when the game starts or when spawned
void ASpinningLidarSensorActor::BeginPlay() {
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("lidar actor spawned"));

    // Update the field of view for the scene capture if
    // needed based on the max and min beam elevations
    float LidarFOV = abs(MaxElevation - MinElevation);
    if (LidarFOV > 70.f && LidarFOV < 150.f) {
        SceneCap->FOVAngle = abs(MaxElevation - MinElevation) + 20.f;
    } else if (LidarFOV >= 150.f) {
        SceneCap->FOVAngle = 170.f;
        UE_LOG(LogTemp, Warning, TEXT("Lidar beams have greater vertical FOV "
                                      "than the scene capture component! Intensities may not be"
                                      " calculated for points at the far top or bottom."));
    }

    // The difference in elevation between adjacent lidar beams
    BeamSpacing = (MaxElevation - MinElevation) / (NumBeams-1);

    // Set the directory where the output file will be saved,
    // by default the top level folder of the Unreal project
    // SaveFilePath = FPaths::ProjectDir() + SaveFileName;

    // Only use the top level folder of Unreal Project if there isn't already a defined path
    if (SaveFilePath.IsEmpty()) {
        SaveFilePath = FPaths::ProjectDir() + SaveFileName;
    }

    // Open file to write, and then write the headers for the columns in the .csv file
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    IFileHandle* FileHandle = PlatformFile.OpenWrite(*SaveFilePath, true);
    if (FileHandle) {
        FString StringToWrite = FString(TEXT("timestamp (seconds),x (cm),y (cm),z (cm),"
                                             "intensity (scale of 0 to 255)") LINE_TERMINATOR);

        FileHandle->Write((const uint8*)TCHAR_TO_ANSI(*StringToWrite), StringToWrite.Len());

        delete FileHandle;
    }

    // Initialize the "sim time" value, which keeps track of the simulation clock
    // regardless of whether the simulation runs in real time.
    SimTimeSeconds = 0.f;

    // Cap the frame rate at a value your machine can reliably achieve,
    // to lock the simulation at a constant frame rate.
    if (GEngine) GEngine->SetMaxFPS(RealClockFramerate);

    // Set global time dilation to match the ratio of sim time to real time
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), RealClockFramerate/SimTimeFramerate);
}

// Called every frame
void ASpinningLidarSensorActor::Tick(float DeltaTime) {
    Super::Tick(DeltaTime);

    /// Fire lasers in the direction the sensor is facing
    // An array to store the data from each lidar beam for this timestep
    TArray<FHitResult> LidarHits;

    // ASSUMPTION: The beams are evenly spaced in elevation.
    for (int i = 0; i < NumBeams-1; i++) {
        LidarHits.Emplace(FireLidarBeam(MinElevation + BeamSpacing * i));
    }

    // Doing the max elevation beam outside the loop so that
    // max elevation is as precise as possible, without rounding errors
    // NOTE: If there is only one beam, it will be at the max elevation angle.
    LidarHits.Emplace(FireLidarBeam(MaxElevation));

    // Write the results from all beams to file
    WriteLidarPointsToFile(LidarHits);

    // Increment the "sim time" value by one timestep,
    // according to the frame rate of the sensor if it ran in real time
    SimTimeSeconds += 1.f / SimTimeFramerate;

    // Apply a relative rotation to the sensor.
    // The mesh component will rotate while the root component is unchanged.
    LidarMeshComponent->AddRelativeRotation(FRotator(0, AngularResolution, 0));
}

void ASpinningLidarSensorActor::WriteLidarPointsToFile(TArray<FHitResult> &LidarHits) {
    // The time in seconds since the simulation began.
    // By default, use "sim time" which may be slower than real time,
    // unless the option has been chosen to use the real clock.
    float Timestamp = SimTimeSeconds;
    if (bUseRealClockTimestamps) {
        Timestamp = GetWorld()->GetRealTimeSeconds();
    }

    // Get a base color image of the scene to determine the intensity of each lidar return
    float HitIntensity = 0.f;
    FColor PointColorFromScene = PointColor;
    FTextureRenderTargetResource* RenderTextureResource =
            SceneCap->TextureTarget->GameThread_GetRenderTargetResource();
    TArray<FColor> ImageBitmap;
    RenderTextureResource->ReadPixels(ImageBitmap);

    /*Set up a FSceneView to match the perspective of the scene capture component,
     * so that its WorldToPixel function can be used to
     * find the pixel location of each lidar point*/
    TSharedPtr<FSceneView> SceneView;
    GetSceneView(SceneCap, SceneView);

    // Write to the specified output file
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    IFileHandle* FileHandle = PlatformFile.OpenWrite(*SaveFilePath, true);
    if (FileHandle) {
        for (auto Hit : LidarHits) {
            //// Calculate the intensity of the return
            // GetLidarPointIntensity(Hit, ImageBitmap, SceneView,
                                   // PointColorFromScene, HitIntensity);

            // Add Gaussian range noise based on angle of incidence
            AddGaussianRangeNoise(Hit);

            // Visualize the beam and any impact point it has
            VisualizeBeam(Hit, PointColorFromScene);

            // If the user has chosen to use the sensor's local coordinates,
            // transform into this frame.
            FVector LidarPoint = Hit.ImpactPoint;
            if (bUseLocalCoordinates && Hit.bBlockingHit) {
                LidarPoint = GetActorTransform().InverseTransformPositionNoScale(LidarPoint);
            }

            // Beams that don't hit anything return 0 for x, y, and z.
            if (!Hit.bBlockingHit) {
                LidarPoint = FVector(0.f, 0.f, 0.f);
            }

            FString StringToWrite = FString::Printf(TEXT("%f,%f,%f,%f,%f") LINE_TERMINATOR,
                                                    Timestamp,
                                                    LidarPoint.X,
                                                    LidarPoint.Y,
                                                    LidarPoint.Z,
                                                    HitIntensity);
            FileHandle->Write((const uint8*)TCHAR_TO_ANSI(*StringToWrite), StringToWrite.Len());

            // display sensor data in log for debugging
            UE_LOG(LogTemp, Warning, TEXT("Impact Point: %s, Timestamp: %s"),
                   *(LidarPoint.ToString()),
                   *FString::SanitizeFloat(Timestamp));
        }
        delete FileHandle;
    }
}

/*Set up a FSceneView to match the perspective of the scene capture component, so that its WorldToPixel function can be used
to find the pixel location of each lidar point*/
void ASpinningLidarSensorActor::GetSceneView(USceneCaptureComponent2D * SceneCapture,
                                             TSharedPtr<FSceneView> &OutSceneView) {
    FSceneViewInitOptions SceneViewOptions;
    float TextureTargetWidth = SceneCapture->TextureTarget->GetSurfaceWidth();
    float TextureTargetHeight = SceneCapture->TextureTarget->GetSurfaceHeight();
    SceneViewOptions.SetViewRectangle(FIntRect(0, 0, TextureTargetWidth, TextureTargetHeight));
    FSceneViewFamilyContext ViewFamilyContext(FSceneViewFamily::ConstructionValues(
                                                  SceneCapture->TextureTarget->
                                                  GameThread_GetRenderTargetResource(),
                                                  SceneCapture->GetScene(),
                                                  SceneCapture->ShowFlags));
    SceneViewOptions.ViewFamily = &ViewFamilyContext;

    // Find the translation and rotation for the view
    FTransform ViewTransform = SceneCapture->GetComponentToWorld();
    SceneViewOptions.ViewOrigin = ViewTransform.GetTranslation();
    // remove translation to get the transform for the rotation only
    ViewTransform.SetTranslation(FVector::ZeroVector);
    SceneViewOptions.ViewRotationMatrix = ViewTransform.ToInverseMatrixWithScale() *
            // Switch between screen coordinates (y-up, x-right, z-forward) and
            // Unreal's left-handed 3D coordinates (z-up, y-right, x-forward)
            FMatrix(FPlane(0, 0, 1, 0), FPlane(1, 0, 0, 0), FPlane(0, 1, 0, 0), FPlane(0, 0, 0, 1));
    SceneViewOptions.OverrideFarClippingPlaneDistance = SceneCapture->MaxViewDistanceOverride;

    // Find field of view multipliers for the x and y axis, which will be 1 for the larger axis

    // Half the field of view in radians
    float HalfFOV = SceneCapture->FOVAngle * (float)PI / 360.f;
    float MultFOVX;
    float MultFOVY;
    if (TextureTargetWidth > TextureTargetHeight) {
        MultFOVX = 1.f;
        MultFOVY = TextureTargetWidth / TextureTargetHeight;
    } else {
        MultFOVX = TextureTargetHeight / TextureTargetWidth;
        MultFOVY = 1.f;
    }

    SceneViewOptions.ProjectionMatrix =
            FPerspectiveMatrix(HalfFOV, HalfFOV, MultFOVX, MultFOVY,
                               GNearClippingPlane, GNearClippingPlane);
    //    OutSceneView = MakeShareable(new FSceneView(SceneViewOptions));
}

// Add Gaussian range noise based on angle of incidence.
void ASpinningLidarSensorActor::AddGaussianRangeNoise(FHitResult &Hit) {
    if (Hit.bBlockingHit) {
        // Determine the random amount by which to change the range of the hit
        std::random_device rd;  // random seed
        std::mt19937 gen(rd());  // random number engine based on Mersenne Twister algorithm

        // find the unit vector parallel to the beam, in world coordinates
        FVector BeamUnitVector =
                UKismetMathLibrary::GetDirectionUnitVector(GetActorLocation()
                                                           + GetActorUpVector()*BeamStartRelativeZ,
                                                           Hit.ImpactPoint);

        // Standard deviation.
        // ASSUMPTION: the range noise is greatest when the angle of incidence is closest
        // to parallel with the surface.
        // This function should be adjusted to match real data, but it has a standard deviation
        // based on the range accuracy from the spec sheet which is minimized when the beam is
        // perpendicular to the surface and maximized as the beam approaches parallel
        // to the surface.
        float StdDev = RangeAccuracy *
                (1.f - FVector::DotProduct(-BeamUnitVector, Hit.ImpactNormal));
        std::normal_distribution<float> d(0.f, StdDev);
        float RangeNoiseScalar = d(gen);

        // Add a vector of this length to the hit's position, parallel to the laser beam
        Hit.ImpactPoint += (BeamUnitVector*RangeNoiseScalar);
        Hit.Distance += RangeNoiseScalar;

        // Write to log for debugging
        /*UE_LOG(LogTemp, Warning, TEXT("Distance: %s, Range Noise Standard Deviation: %s, Range Noise Scalar: %s"),
            *FString::SanitizeFloat(Hit.Distance),
            *FString::SanitizeFloat(StdDev),
            *FString::SanitizeFloat(RangeNoiseScalar)
        );*/
    }
}

// Simulate the probability that there will be no return signal received for some hits,
// especially near max range.
// If the hit does not return due to this probability, set Hit.bBlockingHit to false.
void ASpinningLidarSensorActor::RandomizeWhetherHitReturns(FHitResult &Hit) {
    if (Hit.bBlockingHit && FalloffStdDev > 0.f) {
        // Calculate probablility that there will be no return, as a function of distance.
        // Use a gaussian function centered at the max distance.
        float ProbabilityOfNoReturn = ProbabilityOfNoReturn =
                MaxRangeNoReturnProbability * exp(-0.5f * pow((Hit.Distance - LidarRange) /
                                                              FalloffStdDev, 2));

        // Use RNG to determine whether this hit is received or lost.
        if (FMath::RandRange(0.f, 1.f) < ProbabilityOfNoReturn) Hit.bBlockingHit = false;

        // Write to log for debugging
        /*UE_LOG(LogTemp, Warning, TEXT("Distance: %s, ProbabilityOfNoReturn: %s"),
            *FString::SanitizeFloat(Hit.Distance),
            *FString::SanitizeFloat(ProbabilityOfNoReturn)
        );*/
    }
}

FHitResult ASpinningLidarSensorActor::FireLidarBeam(float BeamElevation) {
    // an out parameter of LineTraceSingleByChannel that will contain
    // the data returned from a firing of a laser
    FHitResult Hit;

    // The raycast starts at a location that is an adjustable distance
    // along the actor's z axis from the actor's root component.
    FVector BeamStart = GetActorLocation() + GetActorUpVector()*BeamStartRelativeZ;

    // A point at the max range of the raycast
    FVector BeamEnd = BeamStart +
            LidarMeshComponent->GetForwardVector().RotateAngleAxis(
                BeamElevation, -LidarMeshComponent->GetRightVector())*LidarRange;
    // Note: Unreal uses a left-handed coordinate system, so the "right" vector is multiplied
    // by -1 before rotating about it

    // Raycasting parameters: "true" to trace using full visible geometry,
    // "this" so that the sensor itself does not occlude the beam
    FCollisionQueryParams RaycastParameters(FName(TEXT("")), true, this);

    // Perform the raycast
    GetWorld()->LineTraceSingleByChannel(
                Hit,
                BeamStart,
                BeamEnd,
                ECollisionChannel::ECC_Visibility,
                RaycastParameters);

    // Simulate the probability that there will be no return signal received for some hits,
    // especially near max range.
    // If the hit does not return due to this probability, set Hit.bBlockingHit to false.
    RandomizeWhetherHitReturns(Hit);



    return Hit;
}

// Input a lidar hit and a bitmap of the render texture showing the lidar's view.
// The color and intensity of the hit are out parameters.
void ASpinningLidarSensorActor::GetLidarPointIntensity(FHitResult &Hit,
                                                       TArray<FColor> &ImageBitmap,
                                                       TSharedPtr<FSceneView> SceneView,
                                                       FColor &OutPointColorFromScene,
                                                       float &OutHitIntensity) {
    OutHitIntensity = 0.f;
    if (Hit.bBlockingHit) {
        // Find the texture coordinates at this location
        int32 TextureWidth = SceneCap->TextureTarget->SizeX;
        int32 TextureHeight = SceneCap->TextureTarget->SizeY;
        FVector2D PixelLocation;
        SceneView->WorldToPixel(Hit.ImpactPoint, PixelLocation);
        int32 PixelX = PixelLocation.X;
        int32 PixelY = PixelLocation.Y;

        // NOTE: if no base color can be found for a lidar point,
        // the intensity will remain at zero by default.
        // This issue should only happen if the difference between max and min beam elevation
        // is greater than the field of view for the scene capture component.
        if ((TextureWidth*(PixelY)+PixelX) < ImageBitmap.Num()) {
            // Determine color value at the hit location on the surface
            FColor BaseColor = ImageBitmap[TextureWidth*(PixelY)+PixelX];

            // Set intensity based on the red channel, as an approximation for infrared
            OutHitIntensity = (float)BaseColor.R;
        }

        // The magnitude of the effect of angle of incidence is adjustable in the editor,
        // for efficiency of refining the model to match real data.
        // multiply by a factor based on angle of incidence,
        // so that returns are brightest when the beam is perpendicular to the surface.
        FVector BeamUnitVector = UKismetMathLibrary::GetDirectionUnitVector(GetActorLocation()
                                                           + GetActorUpVector()*BeamStartRelativeZ,
                                                           Hit.ImpactPoint);
        OutHitIntensity *= IntensityAffectedByAngle *
                FVector::DotProduct(-BeamUnitVector, Hit.ImpactNormal) +
                (1.f- IntensityAffectedByAngle);

        // Set the lidar point color for visualization on a scale from red to green
        // where green is most intense and red is least.
        if (bVisualizeIntensity) OutPointColorFromScene =
                FColor::MakeRedToGreenColorFromScalar(OutHitIntensity / 255.f);
    }
}

// Visualize the beam and any impact point it has
void ASpinningLidarSensorActor::VisualizeBeam(FHitResult &Hit, FColor &PointColorFromScene) {
    // For visualization, determine whether to draw the beam ending
    // at the impact point or at max range
    FVector LidarPoint = Hit.TraceEnd;
    if (Hit.bBlockingHit) LidarPoint = Hit.ImpactPoint;

    // visualize the beam
    DrawDebugLine(
                GetWorld(),
                Hit.TraceStart,
                LidarPoint,
                BeamColor,
                false,
                0.0f,
                0.0f,
                BeamThickness);

    // visualize the point where the beam hits something, if it hits something
    if (Hit.bBlockingHit) {
        DrawDebugPoint(
                    GetWorld(),
                    LidarPoint,
                    LidarPointSize,
                    PointColorFromScene,  // PointColor,
                    false,
                    PointLifetime);
    }
}

#ifdef ConfigurationPluginIncluded
bool ASpinningLidarSensorActor::SetParamsFromYaml(UDocumentNode* SpinningLidarNode) {
    // TODO(future): expose velodyne specific parameters to be read in from yaml too
    // TODO(future): add ground truth
    // TODO(future): make saving csv a toggle - true/false

    Error.Empty();

    // check for Spinning Lidar Sensor name
    UDocumentNode* SpinningLidarNameNode;
    if (SpinningLidarNode->TryGetMapField("name", SpinningLidarNameNode)) {
        if (SpinningLidarNameNode->GetType() != "String") {
            Error += UDocumentNode::InvalidValueError("spinning-lidar.name",
                                                      SpinningLidarNameNode->GetType(), "String");
        } else {
            CommonActorName = SpinningLidarNameNode->ToString().TrimQuotes();
        }
    } else {
        Error += UDocumentNode::MissingRequiredFieldError("spinning-lidar.name");
    }

    // check for motion
    UDocumentNode* MotionNode;
    if (SpinningLidarNode->TryGetMapField("motion", MotionNode)) {
        MotionParamsInitialized = SetMotionParams(MotionNode, &SpinningLidarLocation,
                                                  &SpinningLidarRotation, &Error);
    }

    // check for location, rotation
    bool HasInitialPose = false;
    if (UDocumentNode::SetLocationNode(SpinningLidarNode, "SpinningLidarLocation",
                                       &SpinningLidarLocation, &Error)) {
        HasInitialPose = true;
    }

    if (UDocumentNode::SetRotationNode(SpinningLidarNode, "SpinningLidarRotation",
                                       &SpinningLidarRotation, &Error)) {
        if (!HasInitialPose) {
            Error += "\n - spinning-lidar.rotation cannot be specified without a location";
        }
    }

    if (MotionParamsInitialized && HasInitialPose) {
        Error += "\n - spinning-lidar should not have initial position and waypoints specified. "
                 "Use one or the other.";
    }
    if (!MotionParamsInitialized && !HasInitialPose) {
        Error += "\n - spinning-lidar needs either an initial position or waypoints specified.";
    }
    if (!Error.IsEmpty()) return false;
    return true;
}


bool ASpinningLidarSensorActor::Initialize() {
    // make sure data gets written to the correct output directory
    FString NewFilePath = OutputDirectory + SaveFileName;
    SaveFileName = NewFilePath;

    SaveFilePath = SaveFileName;

    SetActorLocationAndRotation(SpinningLidarLocation, SpinningLidarRotation,
                                false, 0, ETeleportType::None);

    if (!Error.IsEmpty()) return false;
    return true;
}
#endif
