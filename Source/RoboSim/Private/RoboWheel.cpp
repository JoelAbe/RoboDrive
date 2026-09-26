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
		bHadContact = false;
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
		bHadContact = false;

		if (bDrawDebug)
		{
			DrawDebugLine(World, Anchor, Anchor + Down * MaxLengthUU, FColor::Red, false, -1.0f, 0, 1.0f);
		}
		return;
	}

	// How far the suspension has been pushed in, in metres.
	const double HitDistanceM = Hit.Distance * RoboFrames::MetersPerUU;
	const double CompressionM = FMath::Clamp(MaxLengthM - HitDistanceM, 0.0, RestLengthM);

	// Rate of compression, from the change since the last update. On the first
	// frame of contact there is no previous sample, so treat the rate as zero
	// rather than inventing a large one from a single step.
	const double CompressionRateMS = bHadContact ? (CompressionM - PreviousCompressionM) / DeltaSeconds : 0.0;

	PreviousCompressionM = CompressionM;
	bHadContact = true;
	
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

void URoboWheel::UpdateDrive(UPrimitiveComponent* Body, double TargetSpeedMS, double LoadMassKg, float DeltaSeconds)
{
	MeasuredSpeedMS = 0.0;

	if (!bIsDriven || !Contact.bGrounded || Body == nullptr || DeltaSeconds <= 0.0f)
	{
		return;
	}

	// How the ground is moving under this wheel, in m/s.
	const FVector ContactVelocityUU = Body->GetPhysicsLinearVelocityAtPoint(Contact.Location);
	const FVector ContactVelocityMS = ContactVelocityUU * RoboFrames::MetersPerUU;

	// Wheel axes flattened onto the contact surface, so slopes are handled.
	const FVector Forward = FVector::VectorPlaneProject(GetForwardVector(), Contact.Normal).GetSafeNormal();
	const FVector Right = FVector::VectorPlaneProject(GetRightVector(), Contact.Normal).GetSafeNormal();

	MeasuredSpeedMS = FVector::DotProduct(ContactVelocityMS, Forward);
	const double LateralSpeedMS = FVector::DotProduct(ContactVelocityMS, Right);

	const RoboTraction::FContactForceN Force = RoboTraction::ComputeForceN(
		TargetSpeedMS - MeasuredSpeedMS, LateralSpeedMS, LoadMassKg, DeltaSeconds, Contact.SuspensionForceN, GripCoefficient);

	const FVector ForceUU =
		(Forward * Force.Longitudinal + Right * Force.Lateral) * RoboFrames::UUForcePerNewton;

	Body->AddForceAtLocation(ForceUU, Contact.Location);

	if (bDrawDebug)
	{
		// Scale by fraction of the friction budget used, so a cruising wheel
		// draws almost nothing and a slipping wheel draws a full-length line.
		const double MaxForceN = FMath::Max(Contact.SuspensionForceN, 0.0) * GripCoefficient;
		if (MaxForceN > UE_DOUBLE_SMALL_NUMBER)
		{
			constexpr double FullScaleUU = 40.0;
			const FVector DriveUU = Forward * (Force.Longitudinal / MaxForceN * FullScaleUU);
			const FVector GripUU = Right * (Force.Lateral / MaxForceN * FullScaleUU);

			DrawDebugLine(GetWorld(), Contact.Location, Contact.Location + DriveUU,
				FColor::Cyan, false, -1.0f, 0, 2.0f);
			DrawDebugLine(GetWorld(), Contact.Location, Contact.Location + GripUU,
				FColor::Magenta, false, -1.0f, 0, 2.0f);
		}
	}
}