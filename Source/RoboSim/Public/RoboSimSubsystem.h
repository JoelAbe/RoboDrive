#pragma once

#include "CoreMinimal.h"
#include "RoboSimClock.h"
#include "Subsystems/WorldSubsystem.h"
#include "RoboSimSubsystem.generated.h"

/**
 * Owns the simulation clock for a world.
 *
 * Everything in the simulation stamps and schedules from here rather than from
 * the engine, so the whole simulation shares one notion of "now" that can run
 * slower than real time or be stepped from outside.
 */
UCLASS()
class ROBOSIM_API URoboSimSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null outside a game or PIE world. */
	static URoboSimSubsystem* Get(const UObject* WorldContext);

	/** Current simulation time, seconds. */
	UFUNCTION(BlueprintPure, Category = "Simulation")
	double GetSimTimeSeconds() const { return Clock.GetTimeSeconds(); }

	/** Fixed steps taken since the world started. */
	UFUNCTION(BlueprintPure, Category = "Simulation")
	int64 GetStepCount() const { return Clock.GetStepCount(); }

	/**
	 * Simulated seconds per real second, averaged over a short window.
	 * 1.0 means keeping up; below that the simulation is running slow.
	 */
	UFUNCTION(BlueprintPure, Category = "Simulation")
	double GetRealTimeFactor() const { return RealTimeFactor; }

	FSimClock& GetClock() { return Clock; }
	const FSimClock& GetClock() const { return Clock; }

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	FSimClock Clock;

	/** Rolling window for the real-time factor. Wall-clock start, s; negative until the first tick. */
	double WindowStartWallSeconds = -1.0;
	
	double WindowSimSeconds = 0.0;
	double RealTimeFactor = 1.0;

	static constexpr double RealTimeFactorWindowSeconds = 0.5;
};