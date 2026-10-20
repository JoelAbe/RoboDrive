#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoboImu.h"
#include "RoboImuComponent.generated.h"

class ARoboVehicle;

/**
 * A simulated IMU.
 *
 * Also integrates its own gyroscope into a heading estimate, as a second
 * dead-reckoning source alongside wheel odometry. A gyro bias drifts that
 * heading linearly with time even while standing still, which is the same
 * shape of error as a wheel radius calibration error and a useful contrast
 * with the random walk that noise produces.
 */
UCLASS(ClassGroup = (Robo), meta = (BlueprintSpawnableComponent))
class ROBOSIM_API URoboImuComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoboImuComponent();

	UFUNCTION(BlueprintPure, Category = "IMU")
	FImuSample GetLatestSample() const { return LatestSample; }

	/** Heading from integrating the gyroscope alone, rad. */
	UFUNCTION(BlueprintPure, Category = "IMU")
	double GetGyroHeadingRad() const { return GyroHeadingRad; }

	/** How far that heading has drifted from truth, rad. */
	UFUNCTION(BlueprintPure, Category = "IMU")
	double GetGyroHeadingErrorRad() const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "IMU")
	void ResetImu();

	/** IMUs run fast: 100 to 1000 Hz is typical. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IMU",
		meta = (ClampMin = "1.0", ClampMax = "2000.0"))
	double SampleRateHz = 200.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IMU|Error",
		meta = (ClampMin = "0.0"))
	double AccelNoiseStdDevMS2 = 0.05;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IMU|Error",
		meta = (ClampMin = "0.0"))
	double GyroNoiseStdDevRadS = 0.002;

	/** Constant offset on the gyroscope, rad/s. The source of heading drift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IMU|Error")
	FVector GyroBiasRadS = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IMU|Error")
	FVector AccelBiasMS2 = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "IMU|Error")
	int32 RandomSeed = 7777;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawReadout = false;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SampleOnce(double DeltaSeconds, double NowSeconds);

	UPROPERTY(Transient)
	TObjectPtr<ARoboVehicle> Vehicle;

	FImuSample LatestSample;
	FRandomStream NoiseStream;

	FVector PreviousVelocityMS = FVector::ZeroVector;
	double NextSampleTimeSeconds = 0.0;
	double GyroHeadingRad = 0.0;
	bool bHasPreviousVelocity = false;
};