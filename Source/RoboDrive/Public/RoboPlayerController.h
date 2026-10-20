#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RoboPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class URoboOperatorWidget;
enum class EDriveMode : uint8;
struct FInputActionValue;

/**
 * Turns keyboard input into drive commands.
 *
 * Manual input is just another command source: it goes through the same
 * FDriveCommand path as the autonomy, and wins only because the arbiter ranks
 * it higher. Commands are submitted only while the driver is actually holding
 * a key, so releasing the controls lets manual input expire and hands control
 * back rather than latching a stale command. On a real vehicle that gating
 * would be a deadman switch.
 */
UCLASS()
class ARoboPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Operator actions, shared by the keyboard and the operator view's buttons. */
	void SetDriveMode(EDriveMode Mode);
	void EngageEmergencyStop();
	void ReleaseEmergencyStop();

	/** Load a file from the Scenarios folder. Returns false and fills OutError if it cannot. */
	bool LoadScenarioFile(const FString& FileName, FString& OutError);

	/** Start the current scenario again, or with none loaded return the robot to its spawn. */
	void RestartScenario();

	/** Remove the current scenario, leaving the level as it was built, with the robot at its spawn. */
	void UnloadScenario();
	
protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Keys to actions. Assigned in a Blueprint subclass. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> InputMapping;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DriveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> TurnAction;

	/** Speed at full stick or key press. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.0", Units = "m/s"))
	double MaxLinearSpeedMS = 1.0;

	/** Yaw rate at full input, rad/s. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.0"))
	double MaxYawRateRadS = 0.5;

	/** Below this, the driver is considered to have let go. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	double InputDeadzone = 0.05;

	/** Fastest the command may change, m/s per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.1"))
	double MaxLinearAccelMS2 = 2.0;

	/** Fastest the yaw rate may change, rad/s per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.1"))
	double MaxYawAccelRadS2 = 3.0;

private:
	void OnDrive(const FInputActionValue& Value);
	void OnTurn(const FInputActionValue& Value);

	/** Give the robot a goal at the point on the floor under the mouse. */
	void SetGoalUnderCursor();

	/** Withdraw the robot's goal. */
	void ClearGoal();

	/** Draw the current goal, however it was set, coloured by how the mission is going. */
	void DrawGoalMarker() const;
	
	double CurrentLinearMS = 0.0;
	double CurrentAngularRadS = 0.0;
	
	/** Input for this frame, cleared after each submission. */
	double DriveInput = 0.0;
	double TurnInput = 0.0;

	/** The panel on screen. A UPROPERTY so the garbage collector knows it is in use. */
	UPROPERTY(Transient)
	TObjectPtr<URoboOperatorWidget> OperatorWidget;
};