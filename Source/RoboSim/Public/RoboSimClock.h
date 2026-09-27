#pragma once

#include "CoreMinimal.h"
#include "RoboSimClock.generated.h"

UENUM(BlueprintType)
enum class ESimClockMode : uint8
{
	/** Sim time follows real time, in whole fixed steps. Used while driving. */
	RealTime = 0	UMETA(DisplayName = "Real Time"),

	/** Sim time only advances when something asks. Used by tests and replay. */
	Synchronous = 1	UMETA(DisplayName = "Synchronous")
};

/**
 * The simulation's own clock.
 *
 * Advances in fixed steps so a run is reproducible, and never reads the wall
 * clock itself: real time is handed to it, or ignored entirely in synchronous
 * mode. Everything that stamps or schedules anything in the simulation reads
 * time from here rather than from the engine.
 */
class ROBOSIM_API FSimClock
{
public:
	/** Default physics rate, matching the project's substep size. */
	static constexpr double DefaultStepSeconds = 1.0 / 120.0;

	/**
	 * Most steps one Advance may take, so a long stall cannot demand a burst
	 * of catch-up steps that makes the next frame longer still.
	 */
	static constexpr int32 DefaultMaxStepsPerAdvance = 8;

	explicit FSimClock(double InStepSeconds = DefaultStepSeconds);

	/** Current simulation time, seconds since reset. Always a multiple of the step. */
	double GetTimeSeconds() const { return StepCount * StepSeconds; }

	/** Steps taken since reset. The authoritative count; time is derived from it. */
	int64 GetStepCount() const { return StepCount; }

	double GetStepSeconds() const { return StepSeconds; }

	ESimClockMode GetMode() const { return Mode; }
	void SetMode(ESimClockMode InMode) { Mode = InMode; }

	/**
	 * Feed real elapsed time. Takes as many whole steps as fit, keeping the
	 * remainder for next time. Does nothing in synchronous mode.
	 * @return how many steps were taken.
	 */
	int32 Advance(double RealDeltaSeconds);

	/** Take steps regardless of real time. @return how many were taken. */
	int32 Step(int32 Steps = 1);

	/** Back to zero, as when a scenario restarts. */
	void Reset();

	/** How far sim time has fallen behind what was fed in, seconds. */
	double GetPendingSeconds() const { return Accumulator; }

private:
	double StepSeconds;
	double Accumulator = 0.0;
	int64 StepCount = 0;
	ESimClockMode Mode = ESimClockMode::RealTime;
	int32 MaxStepsPerAdvance = DefaultMaxStepsPerAdvance;
};