#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoboOdometry.h"
#include "RoboWheelEncoder.h"
#include "RoboOdometryComponent.generated.h"

class ARoboVehicle;

/**
 * Wheel odometry: estimates the robot's pose from its encoders alone.
 *
 * Deliberately blind. It never reads the robot's true transform except to
 * record ground truth for comparison, so every error accumulates permanently.
 * The calibration error factors exist to make that visible: a wheel radius
 * the software believes wrongly biases every measurement the same way.
 */
UCLASS(ClassGroup = (Robo), meta = (BlueprintSpawnableComponent))
class ROBOSIM_API URoboOdometryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoboOdometryComponent();

	/** Pose estimated from the encoders, REP-103. */
	UFUNCTION(BlueprintPure, Category = "Odometry")
	FPose2D GetEstimatedPose() const { return EstimatedPose; }

	/** Where the robot actually is, from the engine. For comparison only. */
	UFUNCTION(BlueprintPure, Category = "Odometry")
	FPose2D GetTruePose() const { return TruePose; }

	/** How far the estimate has drifted, m. */
	UFUNCTION(BlueprintPure, Category = "Odometry")
	double GetPositionErrorM() const;

	/** How far the estimated heading has drifted, rad. */
	UFUNCTION(BlueprintPure, Category = "Odometry")
	double GetHeadingErrorRad() const;

	/** Snap the estimate back to truth and clear the trails. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Odometry")
	void ResetOdometry();

	/** How often the encoders are read, Hz. Real encoders are polled, not sampled per frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Odometry",
		meta = (ClampMin = "1.0", ClampMax = "1000.0"))
	double SampleRateHz = 50.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Odometry", meta = (ClampMin = "1"))
	int32 TicksPerRevolution = 2048;

	/** Gaussian noise on measured wheel rotation, rad/s. Zero is a clean baseline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Odometry|Error",
		meta = (ClampMin = "0.0"))
	double EncoderNoiseStdDevRadS = 0.0;

	/** What the software believes the wheel radius is, as a factor of the truth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Odometry|Error",
		meta = (ClampMin = "0.5", ClampMax = "1.5"))
	double WheelRadiusErrorFactor = 1.0;

	/** What the software believes the wheel separation is, as a factor of the truth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Odometry|Error",
		meta = (ClampMin = "0.5", ClampMax = "1.5"))
	double WheelSeparationErrorFactor = 1.0;

	/** Seed for the noise, so a run can be repeated exactly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Odometry|Error")
	int32 RandomSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawTrails = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawReadout = true;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** One encoder reading and one integration step. */
	void SampleOnce(double DeltaSeconds);

	/** Read the robot's true pose from the engine, in REP-103. */
	FPose2D ReadTruePose() const;

	UPROPERTY(Transient)
	TObjectPtr<ARoboVehicle> Vehicle;

	FPose2D EstimatedPose;
	FPose2D TruePose;

	FWheelEncoder LeftEncoder;
	FWheelEncoder RightEncoder;
	FRandomStream NoiseStream;

	double NextSampleTimeSeconds = 0.0;

	/** Last drawn trail points, in Unreal space. */
	FVector LastTruthPointUU = FVector::ZeroVector;
	FVector LastEstimatePointUU = FVector::ZeroVector;
	bool bHasTrailPoints = false;
};