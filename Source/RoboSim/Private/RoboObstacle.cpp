#include "RoboObstacle.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ARoboObstacle::ARoboObstacle()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}

	// Spawned at runtime, so Movable: a Static actor created after the level
	// loads produces lighting warnings and cannot be moved.
	Mesh->SetMobility(EComponentMobility::Movable);

	// Blocks everything, including the Visibility channel the lidar and wheel
	// traces use. WorldDynamic object type keeps it out of floor traces.
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
}

void ARoboObstacle::SetSizeM(const FVector& SizeM)
{
	// Scaling the root is safe here because nothing is attached to it. That
	// was not true of the robot's chassis, which is why the robot uses an
	// unscaled collision box with the visual mesh as a child.
	Mesh->SetRelativeScale3D(SizeM);
}