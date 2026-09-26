#include "RoboStubAutonomy.h"

#include "RoboAutonomy.h"
#include "RoboPatterns.h"
#include "RoboVehicle.h"

URoboStubAutonomy::URoboStubAutonomy()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URoboStubAutonomy::BeginPlay()
{
	Super::BeginPlay();

	Vehicle = Cast<ARoboVehicle>(GetOwner());

	if (Vehicle == nullptr)
	{
		UE_LOG(LogRoboAutonomy, Warning,
			TEXT("%s is attached to %s, which is not a RoboVehicle. It will not drive anything."),
			*GetName(), *GetNameSafe(GetOwner()));
		return;
	}

	UE_LOG(LogRoboAutonomy, Log, TEXT("Stub autonomy driving %s at %.2f m/s, radius %.2f m"), *Vehicle->GetName(), SpeedMS, TurnRadiusM);
}

void URoboStubAutonomy::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled || Vehicle == nullptr)
	{
		// Sending nothing is the point: the robot should stop by itself once
		// the last command expires, rather than holding its final order.
		return;
	}

	const RoboDiffDrive::FBodyVelocity Body = RoboPatterns::CircleCommand(SpeedMS, TurnRadiusM);

	FDriveCommand Command;
	Command.Linear = Body.Linear;
	Command.Angular = Body.Angular;
	Command.TimeSeconds = GetWorld()->GetTimeSeconds();
	Command.Source = EDriveSource::Autonomy;

	Vehicle->SubmitDriveCommand(Command);
}