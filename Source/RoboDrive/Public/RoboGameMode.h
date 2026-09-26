#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RoboGameMode.generated.h"

/**
 * Wires the operator's controller to the robot.
 *
 * No default pawn: the robot is placed in the level and tuned there, rather
 * than spawned at a PlayerStart.
 */
UCLASS()
class ARoboGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARoboGameMode();
};