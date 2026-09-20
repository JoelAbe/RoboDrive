#pragma once

#include "CoreMinimal.h"
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

	/** Total mass of the robot, kg. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Robot", meta = (ClampMin = "0.1", Units = "kg"))
	double MassKg = 20.0;

protected:
	virtual void BeginPlay() override;

	/** The chassis rigid body. Everything else attaches to this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Robot")
	TObjectPtr<UStaticMeshComponent> Chassis;
};