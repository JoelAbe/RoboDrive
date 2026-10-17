#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RoboMission.generated.h"

/** Where a mission stands. Written by whatever is executing it, read by anyone. */
UENUM(BlueprintType)
enum class EMissionStatus : uint8
{
	/** No goal has been set. */
	None = 0		UMETA(DisplayName = "None"),

	/** A goal is set and has not been reached. */
	Active = 1		UMETA(DisplayName = "Active"),

	/**
	 * Whatever is driving believes it has reached the goal. A claim, not a
	 * verdict: whether it really has is decided against ground truth by the
	 * test harness.
	 */
	Arrived = 2		UMETA(DisplayName = "Arrived"),

	/** Whatever is driving has given up, for example because it is blocked. */
	Failed = 3		UMETA(DisplayName = "Failed")
};

/** A destination in REP-103 metres. */
USTRUCT(BlueprintType)
struct FMissionGoal
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission", meta = (Units = "m"))
	double X = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission", meta = (Units = "m"))
	double Y = 0.0;

	/** Within this distance of the goal counts as arrived. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission",
		meta = (ClampMin = "0.01", Units = "m"))
	double ToleranceM = 0.5;
};

/**
 * The robot's current task: where it has been asked to go, and how that is
 * going.
 *
 * Lives in RoboSim because both the API and the autonomy need it, and RoboSim
 * is the one module both can see. Keeping the goal out of the brain also means
 * an external brain, such as a ROS stack, can execute the same mission on equal
 * terms with the in-engine one.
 */
UCLASS()
class ROBOSIM_API URoboMissionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null outside a game or PIE world. */
	static URoboMissionSubsystem* Get(const UObject* WorldContext);

	/** Give the robot somewhere to go. The status becomes Active. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void SetGoal(const FMissionGoal& NewGoal);

	/** Withdraw the goal. The status returns to None. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void ClearGoal();

	/** Called by whatever is executing the mission to report how it is going. */
	UFUNCTION(BlueprintCallable, Category = "Mission")
	void ReportStatus(EMissionStatus NewStatus);

	UFUNCTION(BlueprintPure, Category = "Mission")
	bool HasGoal() const { return Status != EMissionStatus::None; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	FMissionGoal GetGoal() const { return Goal; }

	UFUNCTION(BlueprintPure, Category = "Mission")
	EMissionStatus GetStatus() const { return Status; }

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	FMissionGoal Goal;
	EMissionStatus Status = EMissionStatus::None;
};