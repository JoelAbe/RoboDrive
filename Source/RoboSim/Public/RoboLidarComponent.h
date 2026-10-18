#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Math/RandomStream.h"
#include "RoboLidar.h"
#include "RoboLidarComponent.generated.h"

/**
 * A simulated planar lidar.
 *
 * A scene component rather than an actor component, because where it is
 * mounted matters: its transform is the origin of every beam.
 */
UCLASS(ClassGroup = (Robo), meta = (BlueprintSpawnableComponent))
class ROBOSIM_API URoboLidarComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URoboLidarComponent();

	const FLidarScan& GetLatestScan() const { return LatestScan; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lidar", meta = (ClampMin = "2"))
	int32 BeamCount = 180;

	/** Total sweep, degrees. 360 is a spinning lidar; 270 is typical for a fixed one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lidar", meta = (ClampMin = "1.0", ClampMax = "360.0"))
	double AngularSpanDeg = 270.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lidar", meta = (Units = "m"))
	double RangeMinM = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lidar", meta = (Units = "m"))
	double RangeMaxM = 12.0;

	/** Real planar lidars run slowly: 10 to 40 Hz. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lidar", meta = (ClampMin = "1.0"))
	double ScanRateHz = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lidar|Error", meta = (ClampMin = "0.0"))
	double RangeNoiseStdDevM = 0.02;

	/** Chance a beam returns nothing even with a target in range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lidar|Error", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double DropoutProbability = 0.0;

	/** How readily fog is mistaken for a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lidar|Error", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double FogEchoFraction = 0.1;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lidar|Error")
	int32 RandomSeed = 2024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawScan = true;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void Sweep(double NowSeconds);

	FLidarScan LatestScan;
	FRandomStream NoiseStream;
	double NextScanTimeSeconds = 0.0;
};