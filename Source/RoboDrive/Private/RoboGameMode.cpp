#include "RoboGameMode.h"

#include "RoboPlayerController.h"

ARoboGameMode::ARoboGameMode()
{
	PlayerControllerClass = ARoboPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}