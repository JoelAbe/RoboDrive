#include "RoboPlayerController.h"

#include "RoboDrive.h"
#include "RoboVehicle.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

void ARoboPlayerController::BeginPlay()
{
	Super::BeginPlay();

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

	ARoboVehicle* Vehicle = Cast<ARoboVehicle>(GetPawn());
	if (Vehicle == nullptr)
	{
		return;
	}

	// Only submit while the driver is actually holding something. Otherwise
	// manual input would win arbitration forever and nothing else could drive.
	const double Amount = FMath::Max(FMath::Abs(DriveInput), FMath::Abs(TurnInput));
	if (Amount > InputDeadzone)
	{
		FDriveCommand Command;
		Command.Linear = DriveInput * MaxLinearSpeedMS;
		Command.Angular = TurnInput * MaxYawRateRadS;
		Command.TimeSeconds = GetWorld()->GetTimeSeconds();
		Command.Source = EDriveSource::Manual;

		Vehicle->SubmitDriveCommand(Command);
	}

	// Triggered events fire every frame a key is held, so clearing here means
	// a released key simply stops arriving rather than needing a release event.
	DriveInput = 0.0;
	TurnInput = 0.0;
}