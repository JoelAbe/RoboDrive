#pragma once

#include "CoreMinimal.h"
#include "RoboAvoidance.h"
#include "Components/ActorComponent.h"
#include "RoboGoToGoalBrain.h"
#include "RoboAutonomyComponent.generated.h"

class ARoboVehicle;
class URoboLidarComponent;
class URoboOdometryComponent;

/** Which brain the robot runs. */
UENUM(BlueprintType)
enum class EBrainKind : uint8
{
	/** Straight for the goal. Knows nothing about obstacles. */
	GoToGoal = 0	UMETA(DisplayName = "Go To Goal"),

	/** Go-to-goal plus lidar obstacle avoidance. */
	Avoiding = 1	UMETA(DisplayName = "Avoiding")
};

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

	/** Which brain drives. Changeable during play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autonomy")
	EBrainKind BrainKind = EBrainKind::Avoiding;
	
	/** Tuning for the go-to-goal brain. Editable during play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autonomy")
	FGoToGoalSettings GoToGoal;

	/** Tuning for obstacle avoidance, used by the Avoiding brain. Editable during play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autonomy")
	FAvoidanceSettings Avoidance;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawReadout = false;

	/** Draw the heading the brain chose, as an arrow from the robot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawChosenHeading = true;

	/** What the brain decided most recently. For display; nothing should steer by it. */
	const FBrainOutput& GetLastOutput() const { return LastOutput; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Create a brain from the current settings. */
	TUniquePtr<FRoboBrain> MakeBrain() const;

	/** The robot was moved to a new start: forget what the brain remembered about the old one. */
	void HandleRobotReset();
	FDelegateHandle RobotResetHandle;

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
	
	/** What the current brain was built from, to notice when any of it changes. */
	EBrainKind BuiltKind = EBrainKind::Avoiding;
	FGoToGoalSettings BuiltGoToGoal;
	FAvoidanceSettings BuiltAvoidance;

	/** Whether any setting has changed since the brain was built. */
	bool IsBrainStale() const;

	/** Build a brain from the current settings and remember what it was built from. */
	void RebuildBrain();
	
	double NextUpdateTimeSeconds = 0.0;
};