#pragma once

#include "CoreMinimal.h"
#include "RoboDiffDrive.h"
#include "RoboDriveArbiter.h"
#include "RoboWheel.h"
#include "GameFramework/Pawn.h"
#include "RoboVehicle.generated.h"

/**
 * A differential drive ground robot.
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
	
	/** Centre of mass offset from the chassis centre, m. Negative Z lowers it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robot", meta = (Units = "m"))
	FVector CenterOfMassOffsetM = FVector(0.0, 0.0, -0.05);

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

	/** The wheel speeds most recently commanded. What the encoders observe. */
	RoboDiffDrive::FWheelSpeeds GetCommandedWheelSpeeds() const { return LastWheelSpeeds; }

	/** True wheel radius, m. Odometry may be told a different value on purpose. */
	UFUNCTION(BlueprintPure, Category = "Robot")
	double GetWheelRadiusM() const;

	/** Current speed of the chassis, m/s. */
	UFUNCTION(BlueprintPure, Category = "Robot")
	double GetSpeedMS() const;

	/** Chassis velocity in REP-103: m/s, right-handed, +Y left. */
	UFUNCTION(BlueprintPure, Category = "Robot")
	FVector GetLinearVelocityMS() const;

	/** Chassis angular velocity in REP-103: rad/s. */
	UFUNCTION(BlueprintPure, Category = "Robot")
	FVector GetAngularVelocityRadS() const;

	/** Chassis orientation in REP-103. */
	UFUNCTION(BlueprintPure, Category = "Robot")
	FQuat GetRotationRos() const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Position the wheel components from the layout properties. */
	void ApplyWheelLayout();
	
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	/** Chassis rigid body. Unscaled, so attached components keep real offsets. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot")
	TObjectPtr<class UBoxComponent> Chassis;

	/** Visual only. Scaled freely without disturbing the physics or the wheels. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot")
	TObjectPtr<UStaticMeshComponent> ChassisMesh;

	/** Front left, front right, rear left, rear right. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot")
	TArray<TObjectPtr<URoboWheel>> Wheels;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot")
	TObjectPtr<class URoboOdometryComponent> Odometry;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot")
	TObjectPtr<class URoboImuComponent> Imu;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot")
	TObjectPtr<class URoboLidarComponent> Lidar;
	
	/** Third-person view. Superseded by the operator view in Phase 5. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View")
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View")
	TObjectPtr<class UCameraComponent> Camera;

private:
	FDriveArbiter Arbiter;
	RoboDiffDrive::FWheelSpeeds LastWheelSpeeds;
};