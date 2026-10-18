#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoboGoToGoalBrain.h"
#include "RoboAutonomyComponent.generated.h"

class ARoboVehicle;
class URoboLidarComponent;
class URoboOdometryComponent;

/**
 * Runs a brain on the robot.
 *
 * At a fixed rate on the simulation clock it builds a snapshot of what the
 * robot could know, asks the brain what to do, submits the command as the
 * Autonomy source and reports the brain's status to the mission.
 *
 * Attached in the editor rather than created by the vehicle, because RoboSim
 * cannot reference RoboAutonomy: the brain attaches to the body, never the
 * other way round.
 */
UCLASS(ClassGroup = (Robo), meta = (BlueprintSpawnableComponent))
class ROBOAUTONOMY_API URoboAutonomyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoboAutonomyComponent();

	/** Unticking stops the brain; its last command then expires and the robot stops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autonomy")
	bool bEnabled = true;

	/** How often the brain decides, Hz. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Autonomy",
		meta = (ClampMin = "1.0", ClampMax = "100.0"))
	double UpdateRateHz = 10.0;

	/** Tuning for the go-to-goal brain. Editable during play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autonomy")
	FGoToGoalSettings GoToGoal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawReadout = true;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Create a brain from the current settings. */
	TUniquePtr<FRoboBrain> MakeBrain() const;

	/**
	 * Build the brain's view of the world. The only code that touches the
	 * robot on a brain's behalf, so it is where "never read ground truth" is
	 * actually enforced.
	 */
	FBrainInputs BuildInputs(double NowSeconds) const;

	UPROPERTY(Transient)
	TObjectPtr<ARoboVehicle> Vehicle;

	UPROPERTY(Transient)
	TObjectPtr<URoboOdometryComponent> Odometry;

	UPROPERTY(Transient)
	TObjectPtr<URoboLidarComponent> Lidar;

	/** Held as the base type, so a different brain can be swapped in without changing this component. */
	TUniquePtr<FRoboBrain> Brain;

	FBrainOutput LastOutput;
	double NextUpdateTimeSeconds = 0.0;
};