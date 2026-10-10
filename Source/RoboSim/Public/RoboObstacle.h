#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoboObstacle.generated.h"

/**
 * A box obstacle placed by a scenario.
 *
 * Collides with the robot and is seen by the lidar. Its object type is
 * WorldDynamic, so scenario placement can trace for the floor (WorldStatic)
 * without hitting obstacles that were spawned a moment earlier.
 */
UCLASS()
class ROBOSIM_API ARoboObstacle : public AActor
{
	GENERATED_BODY()

public:
	ARoboObstacle();

	/** Size of the box, metres. The engine cube is exactly 1 m, so this is also its scale. */
	void SetSizeM(const FVector& SizeM);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Obstacle")
	TObjectPtr<UStaticMeshComponent> Mesh;
};