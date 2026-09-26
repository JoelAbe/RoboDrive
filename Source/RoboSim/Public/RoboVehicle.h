#pragma once

#include "CoreMinimal.h"
#include "RoboDriveArbiter.h"
#include "RoboWheel.h"
#include "GameFramework/Pawn.h"
#include "RoboVehicle.generated.h"

/**
 * A differential drive ground robot.
 *
 * The chassis is a single Chaos rigid body. Wheels are added in the next step
 * as raycast wheels: a trace per wheel provides suspension, and drive forces
 * are applied at the contact patch. Keeping the wheels behind a small
 * interface leaves room to replace them with constrained rigid bodies later,
 * which is what a skid-steer comparison would need.
 */
UCLASS()
class ROBOSIM_API ARoboVehicle : public APawn
{
	GENERATED_BODY()

public:
	ARoboVehicle();

	/** Submit a command. The arbiter decides which source actually drives. */
	UFUNCTION(BlueprintCallable, Category = "Robot")
	void SubmitDriveCommand(const FDriveCommand& Command);

	/** Which source is currently driving. */
	UFUNCTION(BlueprintPure, Category = "Robot")
	EDriveSource GetActiveDriveSource() const;

	/** Temporary: drive from the Details panel before input exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bUseDebugCommand = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (Units = "m/s"))
	double DebugLinearMS = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	double DebugAngularRadS = 0.0;

	/** Draw the active source and command above the robot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebugState = true;
	
	/** Total mass of the robot, kg. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robot", meta = (ClampMin = "0.1", Units = "kg"))
	double MassKg = 20.0;

	/** Distance between the left and right wheel contact points, m. */
	UFUNCTION(BlueprintPure, Category = "Robot")
	double GetWheelSeparationM() const;

	/** Distance between the two driven wheels, m. Also the kinematic wheel separation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robot|Layout", meta = (ClampMin = "0.05", Units = "m"))
	double WheelTrackM = 0.6;

	/** Distance from the centre to each caster, m. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robot|Layout", meta = (ClampMin = "0.05", Units = "m"))
	double CasterOffsetM = 0.32;

	/** Height of the wheel mounts relative to the chassis origin, m. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robot|Layout", meta = (Units = "m"))
	double WheelMountHeightM = -0.1;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Position the wheel components from the layout properties. */
	void ApplyWheelLayout();
	
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** The chassis rigid body. Everything else attaches to this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot")
	TObjectPtr<UStaticMeshComponent> Chassis;

	/** Front left, front right, rear left, rear right. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot")
	TArray<TObjectPtr<URoboWheel>> Wheels;

	/** Third-person view. Superseded by the operator view in Phase 5. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View")
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View")
	TObjectPtr<class UCameraComponent> Camera;

private:
	FDriveArbiter Arbiter;
};