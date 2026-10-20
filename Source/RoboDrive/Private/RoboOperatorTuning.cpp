#include "RoboOperatorTuning.h"

#include "GameFramework/PlayerController.h"
#include "RoboImuComponent.h"
#include "RoboLidarComponent.h"
#include "RoboOdometryComponent.h"
#include "RoboVehicle.h"
#include "RoboWeather.h"
#include "RoboWheel.h"

FRoboOperatorTuning FRoboOperatorTuning::Read(const APlayerController* Player)
{
	FRoboOperatorTuning Out;

	if (Player == nullptr)
	{
		return Out;
	}

	if (const URoboWeatherSubsystem* Weather = URoboWeatherSubsystem::Get(Player))
	{
		Out.bFog = Weather->HasFog();

		// In clear air the default stays, so switching fog on starts somewhere sensible.
		if (Out.bFog)
		{
			Out.VisibilityM = Weather->GetVisibilityM();
		}
	}

	const ARoboVehicle* Vehicle = Cast<ARoboVehicle>(Player->GetPawn());
	if (Vehicle == nullptr)
	{
		return Out;
	}

	Out.bHasRobot = true;

	// The vehicle's own readout stands for all the text readouts, which are
	// switched together.
	Out.bShowReadouts = Vehicle->bDrawDebugState;

	if (const URoboAutonomyComponent* Autonomy = Vehicle->FindComponentByClass<URoboAutonomyComponent>())
	{
		Out.BrainKind = Autonomy->BrainKind;
		Out.MaxSpeedMS = Autonomy->GoToGoal.MaxSpeedMS;
		Out.LookaheadM = Autonomy->Avoidance.LookaheadM;
		Out.MarginM = Autonomy->Avoidance.MarginM;
		Out.bShowHeading = Autonomy->bDrawChosenHeading;
	}

	if (const URoboLidarComponent* Lidar = Vehicle->FindComponentByClass<URoboLidarComponent>())
	{
		Out.FogEchoFraction = Lidar->FogEchoFraction;
		Out.bShowLidarRays = Lidar->bDrawScan;
	}

	if (const URoboOdometryComponent* Odometry = Vehicle->FindComponentByClass<URoboOdometryComponent>())
	{
		Out.bShowPaths = Odometry->bDrawTrails;
	}

	return Out;
}

void FRoboOperatorTuning::Apply(const APlayerController* Player) const
{
	if (Player == nullptr)
	{
		return;
	}

	// Only touch the weather when it would actually change, so adjusting the
	// brain does not log a weather change every time.
	if (URoboWeatherSubsystem* Weather = URoboWeatherSubsystem::Get(Player))
	{
		if (bFog && (!Weather->HasFog() || Weather->GetVisibilityM() != VisibilityM))
		{
			Weather->SetFog(VisibilityM);
		}
		else if (!bFog && Weather->HasFog())
		{
			Weather->SetClear();
		}
	}

	ARoboVehicle* Vehicle = Cast<ARoboVehicle>(Player->GetPawn());
	if (Vehicle == nullptr)
	{
		return;
	}

	// Changing the brain or its settings is picked up on the brain's next
	// decision, which rebuilds it: the same path as an edit in the Details panel.
	if (URoboAutonomyComponent* Autonomy = Vehicle->FindComponentByClass<URoboAutonomyComponent>())
	{
		Autonomy->BrainKind = BrainKind;
		Autonomy->GoToGoal.MaxSpeedMS = MaxSpeedMS;
		Autonomy->Avoidance.LookaheadM = LookaheadM;
		Autonomy->Avoidance.MarginM = MarginM;
		Autonomy->bDrawChosenHeading = bShowHeading;
		Autonomy->bDrawReadout = bShowReadouts;
	}

	if (URoboLidarComponent* Lidar = Vehicle->FindComponentByClass<URoboLidarComponent>())
	{
		Lidar->FogEchoFraction = FogEchoFraction;
		Lidar->bDrawScan = bShowLidarRays;
	}

	if (URoboOdometryComponent* Odometry = Vehicle->FindComponentByClass<URoboOdometryComponent>())
	{
		Odometry->bDrawTrails = bShowPaths;
		Odometry->bDrawReadout = bShowReadouts;
	}

	if (URoboImuComponent* Imu = Vehicle->FindComponentByClass<URoboImuComponent>())
	{
		Imu->bDrawReadout = bShowReadouts;
	}

	Vehicle->bDrawDebugState = bShowReadouts;

	// Four wheels, so every one of them, not just the first found.
	TArray<URoboWheel*> Wheels;
	Vehicle->GetComponents(Wheels);
	for (URoboWheel* Wheel : Wheels)
	{
		Wheel->bDrawDebug = bShowReadouts;
	}
}