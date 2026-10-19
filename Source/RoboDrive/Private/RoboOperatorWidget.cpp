#include "RoboOperatorWidget.h"

#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "RoboAutonomyComponent.h"
#include "RoboLidarComponent.h"
#include "RoboMission.h"
#include "RoboOdometryComponent.h"
#include "RoboSimSubsystem.h"
#include "RoboVehicle.h"
#include "RoboWeather.h"

namespace
{
	/** An enum value as the name shown in the editor, e.g. "Autonomy". */
	template <typename TEnum>
	FString OperatorEnumName(TEnum Value)
	{
		return UEnum::GetDisplayValueAsText(Value).ToString();
	}
}

void URoboOperatorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	SimText->SetText(FText::FromString(DescribeSim()));

	// The robot is whatever this player is controlling.
	const APlayerController* Player = GetOwningPlayer();
	const ARoboVehicle* Vehicle = Player != nullptr ? Cast<ARoboVehicle>(Player->GetPawn()) : nullptr;

	if (Vehicle == nullptr)
	{
		DrivingText->SetText(FText::FromString(TEXT("No robot")));
		MissionText->SetText(FText::GetEmpty());
		SensingText->SetText(FText::GetEmpty());
		TruthText->SetText(FText::GetEmpty());
		return;
	}

	DrivingText->SetText(FText::FromString(DescribeDriving(*Vehicle)));
	MissionText->SetText(FText::FromString(DescribeMission(*Vehicle)));
	SensingText->SetText(FText::FromString(DescribeSensing(*Vehicle)));
	TruthText->SetText(FText::FromString(DescribeTruth(*Vehicle)));
}

FString URoboOperatorWidget::DescribeSim() const
{
	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (Sim == nullptr)
	{
		return TEXT("Simulation not running");
	}

	return FString::Printf(TEXT("Sim time %.1f s     %.1fx real time"),
		Sim->GetSimTimeSeconds(), Sim->GetRealTimeFactor());
}

FString URoboOperatorWidget::DescribeDriving(const ARoboVehicle& Vehicle) const
{
	const FDriveCommand Command = Vehicle.GetActiveCommand();

	return FString::Printf(TEXT("Driving: %s\nCommand   v %.2f m/s   w %.2f rad/s\nActual speed   %.2f m/s"),
		*OperatorEnumName(Vehicle.GetActiveDriveSource()),
		Command.Linear, Command.Angular, Vehicle.GetSpeedMS());
}

FString URoboOperatorWidget::DescribeMission(const ARoboVehicle& Vehicle) const
{
	const URoboMissionSubsystem* Mission = URoboMissionSubsystem::Get(this);
	if (Mission == nullptr || !Mission->HasGoal())
	{
		return TEXT("Mission: no goal");
	}

	const FMissionGoal Goal = Mission->GetGoal();
	FString Text = FString::Printf(TEXT("Mission: %s\nGoal (%.1f, %.1f)"),
		*OperatorEnumName(Mission->GetStatus()), Goal.X, Goal.Y);

	// Distance as the robot believes it, from odometry. Whether it is right is
	// what the ground-truth section is for.
	if (const URoboOdometryComponent* Odometry = Vehicle.FindComponentByClass<URoboOdometryComponent>())
	{
		const FPose2D Believed = Odometry->GetEstimatedPose();
		const double DistanceM = FMath::Sqrt(FMath::Square(Goal.X - Believed.X) + FMath::Square(Goal.Y - Believed.Y));
		Text += FString::Printf(TEXT("   %.1f m away, it believes"), DistanceM);
	}

	return Text;
}

FString URoboOperatorWidget::DescribeSensing(const ARoboVehicle& Vehicle) const
{
	FString Brain = TEXT("none");
	if (const URoboAutonomyComponent* Autonomy = Vehicle.FindComponentByClass<URoboAutonomyComponent>())
	{
		Brain = Autonomy->bEnabled ? OperatorEnumName(Autonomy->BrainKind) : TEXT("switched off");
	}

	FString Lidar = TEXT("none");
	if (const URoboLidarComponent* LidarComponent = Vehicle.FindComponentByClass<URoboLidarComponent>())
	{
		Lidar = FString::Printf(TEXT("%d beams at %.0f Hz, %.0f m range"),
			LidarComponent->BeamCount, LidarComponent->ScanRateHz, LidarComponent->RangeMaxM);
	}

	FString Weather = TEXT("clear");
	if (const URoboWeatherSubsystem* WeatherSubsystem = URoboWeatherSubsystem::Get(this))
	{
		if (WeatherSubsystem->HasFog())
		{
			Weather = FString::Printf(TEXT("fog, %.0f m visibility"), WeatherSubsystem->GetVisibilityM());
		}
	}

	return FString::Printf(TEXT("Brain: %s\nLidar: %s\nWeather: %s"), *Brain, *Lidar, *Weather);
}

FString URoboOperatorWidget::DescribeTruth(const ARoboVehicle& Vehicle) const
{
	const URoboOdometryComponent* Odometry = Vehicle.FindComponentByClass<URoboOdometryComponent>();
	if (Odometry == nullptr)
	{
		return TEXT("No odometry to compare");
	}

	return FString::Printf(TEXT("Odometry is %.2f m and %.1f deg off the truth"),
		Odometry->GetPositionErrorM(), FMath::RadiansToDegrees(Odometry->GetHeadingErrorRad()));
}