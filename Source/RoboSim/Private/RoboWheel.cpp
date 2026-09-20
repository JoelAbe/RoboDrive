#include "RoboWheel.h"

#include "RoboFrames.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

URoboWheel::URoboWheel()
{
	PrimaryComponentTick.bCanEverTick = false;   // the vehicle drives the update
}

void URoboWheel::UpdateSuspension(UPrimitiveComponent* Body, float DeltaSeconds)
{
	Contact = FWheelContact();

	if (Body == nullptr || DeltaSeconds <= 0.0f)
	{
		PreviousCompressionM = 0.0;
		return;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Trace straight down the suspension axis, from the mounting point.
	const FVector Anchor = GetComponentLocation();
	const FVector Down = -GetUpVector();
	const double MaxLengthM = RestLengthM + WheelRadiusM;
	const double MaxLengthUU = MaxLengthM * RoboFrames::UUPerMeter;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RoboWheelSuspension), false, GetOwner());

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit, Anchor, Anchor + Down * MaxLengthUU, ECC_Visibility, Params);

	if (!bHit)
	{
		// Airborne: the suspension hangs fully extended and applies nothing.
		PreviousCompressionM = 0.0;

		if (bDrawDebug)
		{
			DrawDebugLine(World, Anchor, Anchor + Down * MaxLengthUU, FColor::Red, false, -1.0f, 0, 1.0f);
		}
		return;
	}

	// How far the suspension has been pushed in, in metres.
	const double HitDistanceM = Hit.Distance * RoboFrames::MetersPerUU;
	const double CompressionM = FMath::Clamp(MaxLengthM - HitDistanceM, 0.0, RestLengthM);

	// Rate of compression, from the change since the last update. Positive
	// while compressing, negative while the suspension extends again.
	const double CompressionRateMS = (CompressionM - PreviousCompressionM) / DeltaSeconds;
	PreviousCompressionM = CompressionM;

	const double ForceN = RoboSuspension::ComputeForceN(
		CompressionM, CompressionRateMS, StiffnessNPerM, DampingNsPerM);

	// Unreal works in kg and centimetres, so its force unit is kg.cm/s^2.
	// One newton is one kg.m/s^2, which is 100 of those.
	const FVector ForceUU = GetUpVector() * (ForceN * RoboFrames::UUForcePerNewton);
	Body->AddForceAtLocation(ForceUU, Hit.ImpactPoint);

	Contact.bGrounded = true;
	Contact.Location = Hit.ImpactPoint;
	Contact.Normal = Hit.ImpactNormal;
	Contact.CompressionM = CompressionM;
	Contact.SuspensionForceN = ForceN;

	if (bDrawDebug)
	{
		DrawDebugLine(World, Anchor, Hit.ImpactPoint, FColor::Green, false, -1.0f, 0, 1.0f);
		DrawDebugSphere(World, Hit.ImpactPoint, 3.0f, 8, FColor::Yellow, false, -1.0f, 0, 1.0f);
	}
}