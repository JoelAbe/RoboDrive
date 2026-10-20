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

/** Contact patch force maths, kept engine-free so it can be tested directly. */
namespace RoboTraction
{
	/** Longitudinal (X) and lateral (Y) force at a contact patch, in newtons. */
	struct FContactForceN
	{
		double Longitudinal = 0.0;
		double Lateral = 0.0;
	};

	/**
	 * Force a wheel can apply this step, limited by grip.
	 *
	 * The wheel is asked to correct its longitudinal speed error and to cancel
	 * any sideways motion. Both demands share one friction budget: if their
	 * combined magnitude exceeds it, they are scaled down together, which is
	 * what a skidding wheel does.
	 *
	 * @param SpeedErrorMS   Commanded minus actual longitudinal speed, m/s.
	 * @param LateralSpeedMS Sideways speed at the contact patch, m/s.
	 * @param LoadMassKg     Share of the robot's mass carried by this wheel, kg.
	 * @param DeltaSeconds   Step length, s.
	 * @param NormalForceN   Suspension force pressing the wheel down, N.
	 * @param GripCoefficient Friction coefficient (about 1.0 for rubber on concrete).
	 */
	inline FContactForceN ComputeForceN(double SpeedErrorMS, double LateralSpeedMS,
		double LoadMassKg, double DeltaSeconds, double NormalForceN, double GripCoefficient)
	{
		if (DeltaSeconds <= 0.0)
		{
			return FContactForceN{};
		}

		// The force that would exactly cancel each velocity error this step.
		FContactForceN Demand;
		Demand.Longitudinal = LoadMassKg * SpeedErrorMS / DeltaSeconds;
		Demand.Lateral = LoadMassKg * -LateralSpeedMS / DeltaSeconds;

		const double MaxForceN = FMath::Max(NormalForceN, 0.0) * GripCoefficient;
		const double DemandN = FMath::Sqrt(
			Demand.Longitudinal * Demand.Longitudinal + Demand.Lateral * Demand.Lateral);

		if (DemandN > MaxForceN && DemandN > UE_DOUBLE_SMALL_NUMBER)
		{
			// Beyond the friction circle: the wheel slips. Scale both
			// components so the direction is kept but the magnitude is capped.
			const double Scale = MaxForceN / DemandN;
			Demand.Longitudinal *= Scale;
			Demand.Lateral *= Scale;
		}

		return Demand;
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
 * wheel order is deterministic and there is one place that sets when wheels
 * update (ADR 0006 keeps that on the actor tick).
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
	bool bDrawDebug = false;

	
	/** Apply drive and lateral friction at the contact patch found by UpdateSuspension. */
	void UpdateDrive(UPrimitiveComponent* Body, double TargetSpeedMS, double LoadMassKg, float DeltaSeconds);

	/** Driven wheels push and resist sideways slide. Casters only carry weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel")
	bool bIsDriven = true;

	/** Friction coefficient at the contact patch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wheel", meta = (ClampMin = "0.0"))
	double GripCoefficient = 1.0;

	/** Measured longitudinal speed at the contact patch, m/s. Recorded for slip analysis; the encoders read commanded rotation instead. */
	UPROPERTY(BlueprintReadOnly, Category = "Wheel")
	double MeasuredSpeedMS = 0.0;

private:
	FWheelContact Contact;

	/** Compression from the previous update, m, for the damping rate. */
	double PreviousCompressionM = 0.0;

	bool bHadContact = false;
};