#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RoboPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
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
	double MaxYawRateRadS = 2.0;

	/** Below this, the driver is considered to have let go. */
	UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	double InputDeadzone = 0.05;

private:
	void OnDrive(const FInputActionValue& Value);
	void OnTurn(const FInputActionValue& Value);

	/** Input for this frame, cleared after each submission. */
	double DriveInput = 0.0;
	double TurnInput = 0.0;
};