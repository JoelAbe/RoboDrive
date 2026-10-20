#include "RoboPlayerController.h"

#include "RoboDrive.h"
#include "RoboVehicle.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "RoboScenario.h"
#include "RoboScenarioFiles.h"
#include "Blueprint/UserWidget.h"
#include "DrawDebugHelpers.h"
#include "RoboFrames.h"
#include "RoboMission.h"
#include "Dom/JsonObject.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "RoboOperatorWidget.h"
#include "RoboSimSubsystem.h"

void ARoboPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// The operator view, for a person watching. Skipped when nothing can be
	// rendered, as in a headless run, where nobody would see it.
	if (IsLocalController() && FApp::CanEverRender())
	{
		OperatorWidget = CreateWidget<URoboOperatorWidget>(this, URoboOperatorWidget::StaticClass());
		if (OperatorWidget != nullptr)
		{
			OperatorWidget->AddToViewport();

			// The view has buttons, so the mouse stays visible and free to click
			// them, while the keyboard keeps driving the robot.
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(InputMode);
			SetShowMouseCursor(true);
		}
	}
	
	if (InputMapping == nullptr)
	{
		UE_LOG(LogRoboDrive, Warning,
			TEXT("%s has no input mapping context assigned; keyboard control is disabled."), *GetName());
		return;
	}

	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(InputMapping, 0);
		}
	}
}

void ARoboPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInput == nullptr)
	{
		return;
	}

	if (DriveAction)
	{
		EnhancedInput->BindAction(DriveAction, ETriggerEvent::Triggered, this, &ARoboPlayerController::OnDrive);
	}

	if (TurnAction)
	{
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Triggered, this, &ARoboPlayerController::OnTurn);
	}
}

void ARoboPlayerController::OnDrive(const FInputActionValue& Value)
{
	DriveInput = Value.Get<float>();
}

void ARoboPlayerController::OnTurn(const FInputActionValue& Value)
{
	TurnInput = Value.Get<float>();
}

void ARoboPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Tab shows or hides the operator view. Checked directly rather than
	// through an input action: it is a viewing aid, not a way of driving.
	if (OperatorWidget != nullptr && WasInputKeyJustPressed(EKeys::Tab))
	{
		OperatorWidget->ToggleShown();
	}

	// Operator keys. Space stops the robot from anywhere; releasing takes the
	// button in the panel, so a stop cannot be undone by a stray key press.
	if (WasInputKeyJustPressed(EKeys::SpaceBar))
	{
		EngageEmergencyStop();
	}

	if (WasInputKeyJustPressed(EKeys::One))
	{
		SetDriveMode(EDriveMode::Auto);
	}

	if (WasInputKeyJustPressed(EKeys::Two))
	{
		SetDriveMode(EDriveMode::Manual);
	}

	if (WasInputKeyJustPressed(EKeys::R))
	{
		RestartScenario();
	}

	// Left-click on the world gives the robot a goal there; right-click
	// withdraws it. Clicks on the panels never arrive here: they handle
	// their own clicks before the game sees them.
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		SetGoalUnderCursor();
	}

	if (WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		ClearGoal();
	}

	DrawGoalMarker();
	
	ARoboVehicle* Vehicle = Cast<ARoboVehicle>(GetPawn());
	const URoboSimSubsystem* Sim = URoboSimSubsystem::Get(this);
	if (Vehicle == nullptr || Sim == nullptr)
	{
		return;
	}
	
	// Only submit while the driver is actually holding something. Otherwise
	// manual input would win arbitration forever and nothing else could drive.
	const double Amount = FMath::Max(FMath::Abs(DriveInput), FMath::Abs(TurnInput));
	if (Amount > InputDeadzone)
	{
		const double TargetLinearMS = DriveInput * MaxLinearSpeedMS;
		const double TargetAngularRadS = TurnInput * MaxYawRateRadS;

		// Ramp toward the request rather than jumping to it: a step command spends
		// grip on an acceleration the wheels cannot deliver anyway.
		CurrentLinearMS = FMath::FInterpConstantTo(CurrentLinearMS, TargetLinearMS, DeltaTime, MaxLinearAccelMS2);
		CurrentAngularRadS = FMath::FInterpConstantTo(CurrentAngularRadS, TargetAngularRadS, DeltaTime, MaxYawAccelRadS2);
		
		FDriveCommand Command;
		Command.Linear = CurrentLinearMS;
		Command.Angular = CurrentAngularRadS;
		Command.TimeSeconds = Sim->GetSimTimeSeconds();
		Command.Source = EDriveSource::Manual;

		Vehicle->SubmitDriveCommand(Command);
	}
	else
	{
		// Not driving, so there is no ramp to continue. Keeping the old value
		// would make the next key press resume the previous command first.
		CurrentLinearMS = 0.0;
		CurrentAngularRadS = 0.0;
	}

	// Triggered events fire every frame a key is held, so clearing here means
	// a released key simply stops arriving rather than needing a release event.
	DriveInput = 0.0;
	TurnInput = 0.0;
}

void ARoboPlayerController::SetDriveMode(EDriveMode Mode)
{
	if (ARoboVehicle* Vehicle = Cast<ARoboVehicle>(GetPawn()))
	{
		Vehicle->SetDriveMode(Mode);
	}
}

void ARoboPlayerController::EngageEmergencyStop()
{
	if (ARoboVehicle* Vehicle = Cast<ARoboVehicle>(GetPawn()))
	{
		Vehicle->EngageEmergencyStop();
	}
}

void ARoboPlayerController::ReleaseEmergencyStop()
{
	if (ARoboVehicle* Vehicle = Cast<ARoboVehicle>(GetPawn()))
	{
		Vehicle->ReleaseEmergencyStop();
	}
}

bool ARoboPlayerController::LoadScenarioFile(const FString& FileName, FString& OutError)
{
	URoboScenarioSubsystem* Scenarios = URoboScenarioSubsystem::Get(this);
	if (Scenarios == nullptr)
	{
		OutError = TEXT("no simulation running");
		return false;
	}

	const TSharedPtr<FJsonObject> Scenario = RoboScenarioFiles::Read(FileName, OutError);

	// The file's name stands in for a scenario that does not name itself.
	if (!Scenario.IsValid()
		|| !Scenarios->Load(Scenario.ToSharedRef(), OutError, FPaths::GetBaseFilename(FileName)))
	{
		UE_LOG(LogRoboDrive, Warning, TEXT("Could not load scenario %s: %s"), *FileName, *OutError);
		return false;
	}

	return true;
}

void ARoboPlayerController::UnloadScenario()
{
	if (URoboScenarioSubsystem* Scenarios = URoboScenarioSubsystem::Get(this))
	{
		FString Error;
		if (!Scenarios->Unload(Error))
		{
			UE_LOG(LogRoboDrive, Warning, TEXT("Could not unload the scenario: %s"), *Error);
		}
	}
}

void ARoboPlayerController::RestartScenario()
{
	if (URoboScenarioSubsystem* Scenarios = URoboScenarioSubsystem::Get(this))
	{
		FString Error;
		if (!Scenarios->Restart(Error))
		{
			UE_LOG(LogRoboDrive, Warning, TEXT("Could not restart: %s"), *Error);
		}
	}
}

void ARoboPlayerController::SetGoalUnderCursor()
{
	URoboMissionSubsystem* Mission = URoboMissionSubsystem::Get(this);
	FHitResult Hit;

	// Whatever is under the mouse: usually the floor, sometimes an obstacle
	// or a wall. Only where it is across the ground matters, not its height.
	if (Mission == nullptr || !GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		return;
	}

	// Into REP-103 metres, like every goal, at the boundary as always.
	const FVector GoalRos = RoboFrames::PositionToRos(Hit.ImpactPoint);

	FMissionGoal Goal;
	Goal.X = GoalRos.X;
	Goal.Y = GoalRos.Y;
	Mission->SetGoal(Goal);
}

void ARoboPlayerController::ClearGoal()
{
	if (URoboMissionSubsystem* Mission = URoboMissionSubsystem::Get(this))
	{
		Mission->ClearGoal();
	}
}

void ARoboPlayerController::DrawGoalMarker() const
{
	const URoboMissionSubsystem* Mission = URoboMissionSubsystem::Get(this);
	if (Mission == nullptr || !Mission->HasGoal())
	{
		return;
	}

	const FMissionGoal Goal = Mission->GetGoal();
	const FVector GoalUU = RoboFrames::PositionToUe(FVector(Goal.X, Goal.Y, 0.0));

	// Stand the ring on the floor. Static geometry only, so a scenario's
	// boxes, which are dynamic, are never mistaken for the ground.
	double FloorZ = 0.0;
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit,
		FVector(GoalUU.X, GoalUU.Y, 10000.0), FVector(GoalUU.X, GoalUU.Y, -10000.0),
		FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		FloorZ = Hit.ImpactPoint.Z;
	}

	// The panel's colours: amber under way, teal reached, coral given up.
	FColor Colour = FColor::FromHex(TEXT("E0A84A"));
	if (Mission->GetStatus() == EMissionStatus::Arrived)
	{
		Colour = FColor::FromHex(TEXT("3CC8B4"));
	}
	else if (Mission->GetStatus() == EMissionStatus::Failed)
	{
		Colour = FColor::FromHex(TEXT("E8644F"));
	}

	// Not persistent: drawn for one frame, every frame, so it follows the
	// goal and disappears with it.
	DrawDebugCylinder(GetWorld(),
		FVector(GoalUU.X, GoalUU.Y, FloorZ),
		FVector(GoalUU.X, GoalUU.Y, FloorZ + 50.0),
		Goal.ToleranceM * RoboFrames::UUPerMeter, 32, Colour, false, -1.0f, 0, 2.0f);
}[[]]