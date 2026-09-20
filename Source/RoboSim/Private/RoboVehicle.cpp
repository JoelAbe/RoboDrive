#include "RoboVehicle.h"

#include "RoboSim.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ARoboVehicle::ARoboVehicle()
{
	PrimaryActorTick.bCanEverTick = true;

	Chassis = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Chassis"));
	SetRootComponent(Chassis);

	// Engine primitive: a 100 cm cube, scaled to a 60 x 40 x 20 cm body.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh (TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Chassis->SetStaticMesh(CubeMesh.Object);
	}

	Chassis->SetRelativeScale3D(FVector(0.6, 0.4, 0.2));
	Chassis->SetSimulatePhysics(true);
	Chassis->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Chassis->SetCollisionProfileName(TEXT("PhysicsActor"));
}

void ARoboVehicle::BeginPlay()
{
	Super::BeginPlay();

	// Override the mass the mesh volume would imply. A robot is not a solid
	// block of whatever the cube's default density is.
	Chassis->SetMassOverrideInKg(NAME_None, static_cast<float>(MassKg), true);

	UE_LOG(LogRoboSim, Log, TEXT("%s ready: %.1f kg"), *GetName(), MassKg);
}