// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Engine.h"
#include "GameFramework/Actor.h"
#include "SpinningLidarSensorActor.generated.h"

UCLASS()
class SPINNINGLIDARSENSORPLUGIN_API ASpinningLidarSensorActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASpinningLidarSensorActor();

	UPROPERTY(EditAnywhere, Transient)
	UStaticMesh* LidarMesh;

	UPROPERTY(Transient)
	UStaticMeshComponent* LidarMeshComponent;

	UPROPERTY(Transient)
	UMaterial* LidarMaterial;

	UPROPERTY()
	UCapsuleComponent* RootCapsule;

	UPROPERTY(EditAnywhere, Transient)
	USceneCaptureComponent2D* SceneCap;

	UPROPERTY(EditAnywhere, Transient)
	UTextureRenderTarget2D* RenderTexture;

	//The filename that the lidar data will be written do. The file will appear in the top level of your Unreal project folder.
	UPROPERTY(EditAnywhere)
	FString SaveFileName = FString("LidarRecording.csv");


	/*Lidar Sensor Properties*/

	//The lidar range in cm, with the Velodyne HDL-32E range as default
	UPROPERTY(EditAnywhere, Category = "Lidar Sensor Properties", meta = (UIMin = 0.f))
	float LidarRange = 10000.f; 

	//number of lidar beams
	UPROPERTY(EditANywhere, Category = "Lidar Sensor Properties", meta = (UIMin = 1))
    int32 NumBeams = 32;

	//elevation angle in degrees for the highest lidar beam
	UPROPERTY(EditANywhere, Category = "Lidar Sensor Properties", meta = (UIMin = -90.f, UIMax = 180.f))
	float MaxElevation = 10.67f;

	//elevation angle in degrees for the lowest lidar beam
	UPROPERTY(EditANywhere, Category = "Lidar Sensor Properties", meta = (UIMin = -180.f, UIMax = 180.f))
	float MinElevation = -30.67f;

	/*Horizontal/Azimuth resolution in degrees. 
	For the Velodyne HDL-32E, assuming linear proportionality between angular resolution and rotation rate, 
	Rotation Rate in Hz = 50 * Angular Resolution in degrees.*/
	UPROPERTY(EditAnywhere, Category = "Lidar Sensor Properties", meta = (UIMin = 0.1f, UIMax = 0.4f))
    float AngularResolution = 0.4f;

	//Sensor accuracy is +/- this number, in cm
	UPROPERTY(EditAnywhere, Category = "Lidar Sensor Properties", meta = (UIMin = 0.f))
	float RangeAccuracy = 2.f;

	//Positioning in cm of the start point for raycasts along the z axis of the sensor mesh
	UPROPERTY(EditAnywhere, Category = "Lidar Sensor Properties")
	float BeamStartRelativeZ = 7.21f;

	//The probability that a lidar point will be lost and not returned at the sensor's max range.
	//Equals the amplitude of a Gaussian function centered at max range which determines the
	//probability that a given lidar hit will not be returned, as a function of distance.
	UPROPERTY(EditAnywhere, Category = "Lidar Sensor Properties", meta = (UIMin = 0.f, UIMax = 1.f))
	float MaxRangeNoReturnProbability = 1.f;

	//The standard deviation in cm of a Gaussian function centered at the lidar's max range
	//which determines the probability that a given lidar hit will not be returned, as a function of distance.
	UPROPERTY(EditAnywhere, Category = "Lidar Sensor Properties", meta = (UIMin = 0.f))
	float FalloffStdDev = 10.f;

	//If checked, lidar points in the output file will be in the local coordinate frame of the sensor.
	//If unchecked, they will use world coordinates.
	UPROPERTY(EditAnywhere, Category = "Lidar Sensor Properties")
	bool bUseLocalCoordinates = false;



	/*Visualization Properties*/

	//Thickness of beams for raycast visualization
	UPROPERTY(EditAnywhere, Category = "Visualization Properties", meta = (UIMin = 0.f))
	float BeamThickness = 0.5f;

	//Size of visualized lidar point cloud points
	UPROPERTY(EditAnywhere, Category = "Visualization Properties", meta = (UIMin = 0.f))
    float LidarPointSize = 3.f;

	//Number of seconds that each visualized lidar hit will remain in the world
	UPROPERTY(EditANywhere, Category = "Visualization Properties", meta = (UIMin = 0.f))
    float PointLifetime = 1.f;

	//The color of the beam visualizations
	UPROPERTY(EditAnywhere, Category = "Visualization Properties")
	FColor BeamColor = FColor(255,0,0);

	//The color of the visualized lidar points
	UPROPERTY(EditAnywhere, Category = "Visualization Properties")
	FColor PointColor = FColor(10, 0, 0);

	//If checked, the color of the visualized lidar points will reflect the intensity of the return,
	//from red (low intensity) to green (high intensity).
	//If unchecked, the lidar point color chosen above will be used.
	UPROPERTY(EditAnywhere, Category = "Visualization Properties")
    bool bVisualizeIntensity = false;

	
	/*Simulation Properties*/

	/*The number of "ticks" or frame updates per second if the sensor were to run in real time, with one frame per angular position at which data is sampled. 
	For the Velodyne HDL-32E, 0.1 degree azimuth resolution at 5 Hz means 3600 frames/rotation, so 3600*5=18000 FPS. 
	0.4 degree resolution at 20 Hz is likewise 18000 FPS. 
	For slower than real time simulations, the recorded timestamps are scaled to account for the ratio between real time and simulation time.*/
	UPROPERTY(EditAnywhere, Category = "Simulation Properties", meta = (UIMin = 1))
    float SimTimeFramerate = 18000.f;

    //UIMin ensures this value will never be <= 0. ASSUMPTION: The frame rate required for real time will never be <1.

	//Choose a frame rate your machine can reliably achieve, and the simulation will be capped at that.
	UPROPERTY(EditAnywhere, Category = "Simulation Properties", meta = (UIMin = 0.f))
    float RealClockFramerate = 40.f;

	//If checked, use the real clock time to timestamp recorded data
	//If unchecked, timestamp according to simulation time, which may be slower than real time.
	UPROPERTY(EditAnywhere, Category = "Simulation Properties")
	bool bUseRealClockTimestamps = false;

	//The magnitude of effect that the angle of incidence has on the intensity of lidar returns,
	//for efficiency of refining the model to match real data.
	UPROPERTY(EditAnywhere, Category = "Simulation Properties", meta = (UIMin = 0.f, UIMax = 1.f))
	float IntensityAffectedByAngle = 1.f;

	//The default pixel dimensions of the texture render target used to render the base colors of the scene
	//from the perspective of the sensor, for use in calculating intensity values.
	//To change the dimensions of the render target, open the render target itself and set these values directly.
	UPROPERTY()
	FVector2D RenderTextureDimensions = FVector2D(512, 512);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	void WriteLidarPointsToFile(TArray<FHitResult> &LidarHits);
	void GetSceneView(USceneCaptureComponent2D * SceneCapture, TSharedPtr<FSceneView> &SceneView);
	void AddGaussianRangeNoise(FHitResult &Hit);
	void RandomizeWhetherHitReturns(FHitResult &Hit);
	FHitResult FireLidarBeam(float);
	void GetLidarPointIntensity(FHitResult &Hit, TArray<FColor> &ImageBitmap, TSharedPtr<FSceneView> SceneView, FColor &PointColorFromScene, float &HitIntensity);
	void VisualizeBeam(FHitResult &Hit, FColor &PointColorFromScene);
	float BeamSpacing;
	FString SaveFilePath;
	float SimTimeSeconds;
};
