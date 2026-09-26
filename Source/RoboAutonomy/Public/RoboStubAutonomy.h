#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoboStubAutonomy.generated.h"

class ARoboVehicle;

/**
 * A placeholder brain that drives a constant circle.
 *
 * It exists to exercise the command path before real autonomy: it proves a
 * second source can drive through the same interface as the keyboard, that
 * manual input overrides it, and that the robot stops on its own when this
 * stops sending. Replaced in Phase 4 by autonomy that reacts to sensors.
 */
UCLASS(ClassGroup = (Robo), meta = (BlueprintSpawnableComponent))
class ROBOAUTONOMY_API URoboStubAutonomy : public UActorComponent
{
	GENERATED_BODY()

public:
	URoboStubAutonomy();

	/** Unticking this stops commands entirely, which the watchdog should catch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autonomy")
	bool bEnabled = true;

	/** Forward speed to hold, m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autonomy", meta = (Units = "m/s"))
	double SpeedMS = 0.5;

	/** Radius of the circle to drive, m. Positive curves left; zero drives straight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autonomy", meta = (Units = "m"))
	double TurnRadiusM = 2.0;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<ARoboVehicle> Vehicle;
};