#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RoboWheel.generated.h"

/** Spring and damper maths, kept free of the engine so it can be tested directly. */
namespace RoboSuspension
{
	/**
	 * Suspension force in newtons.
	 *
	 * @param CompressionM       How far the suspension is compressed, m. Zero when fully extended.
	 * @param CompressionRateMS  How fast it is compressing, m/s. Negative while extending.
	 * @param StiffnessNPerM     Spring constant, N/m.
	 * @param DampingNsPerM      Damping coefficient, N.s/m.
	 *
	 * Never negative: a suspension can push the chassis up, never pull it down.
	 */
	inline double ComputeForceN(double CompressionM, double CompressionRateMS, double StiffnessNPerM, double DampingNsPerM)
	{
		const double Force = StiffnessNPerM * CompressionM + DampingNsPerM * CompressionRateMS;
		return FMath::Max(Force, 0.0);
	}
}

/** What a wheel found beneath it on the last update. */
USTRUCT(BlueprintType)
struct FWheelContact
{
	GENERATED_BODY()

	/** True if the trace found ground within reach. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	bool bGrounded = false;

	/** World-space contact point. Drive forces are applied here. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	FVector Location = FVector::ZeroVector;

	/** Surface normal at the contact point. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	FVector Normal = FVector::UpVector;

	/** Suspension compression, m. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	double CompressionM = 0.0;

    /** Suspension force applied on the last update, N. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	double SuspensionForceN = 0.0;
};

/**
 * One raycast wheel.
 *
 * The component's own transform is the suspension anchor: where the wheel is
 * mounted on the chassis. Moving the component in the editor moves the wheel.
 * The vehicle drives the update rather than the component ticking itself, so
 * wheel order is deterministic and there is one place to change when this
 * moves onto the physics tick in Phase 2.
 */
UCLASS(ClassGroup = (Robo), meta = (BlueprintSpawnableComponent))
class ROBOSIM_API URoboWheel : public USceneComponent
{
	GENERATED_BODY()

public:
	URoboWheel();

	/** Trace for ground and apply the suspension force to Body. */
	void UpdateSuspension(UPrimitiveComponent* Body, float DeltaSeconds);

	const FWheelContact& GetContact() const { return Contact; }

	/** Wheel radius, m. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel", meta = (ClampMin = "0.01", Units = "m"))
	double WheelRadiusM = 0.1;

	/** Suspension travel when fully extended, m. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension", meta = (ClampMin = "0.001", Units = "m"))
	double RestLengthM = 0.08;

	/** Spring stiffness, N/m. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension", meta = (ClampMin = "0.0"))
	double StiffnessNPerM = 1500.0;

	/** Damping coefficient, N.s/m. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Suspension", meta = (ClampMin = "0.0"))
	double DampingNsPerM = 120.0;

	/** Draw the trace and contact point during play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bDrawDebug = true;

private:
	FWheelContact Contact;

	/** Compression from the previous update, m, for the damping rate. */
	double PreviousCompressionM = 0.0;
};