#include "RoboPlayerController.h"

#include "RoboDrive.h"
#include "RoboVehicle.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Misc/App.h"
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