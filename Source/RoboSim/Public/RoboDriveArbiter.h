#pragma once

#include "CoreMinimal.h"
#include "RoboDriveCommand.h"

/**
 * Chooses which command source is currently driving.
 *
 * Sources submit commands; the arbiter picks the highest-priority source whose
 * most recent command has not expired. If nothing is fresh, the result is a
 * stop, so a source that dies cannot leave the robot driving on its last
 * order.
 *
 * The current time is passed in rather than read from the engine, so this is
 * testable without a world and works unchanged under simulated time.
 */
class ROBOSIM_API FDriveArbiter
{
public:
	/** How long a command stays valid, in seconds. */
	static constexpr double DefaultCommandTimeout = 0.25;

	explicit FDriveArbiter(double InCommandTimeout = DefaultCommandTimeout);

	/** Record the latest command from a source. Later commands replace earlier ones. */
	void Submit(const FDriveCommand& Command);

	/**
	 * Decide who is driving at the given simulation time.
	 * Returns the winning command, or a zeroed stop command if none is fresh.
	 */
	FDriveCommand Evaluate(double SimTimeSeconds);

	/** The source that won the most recent Evaluate. */
	EDriveSource GetActiveSource() const { return ActiveSource; }

	double GetCommandTimeout() const { return CommandTimeout; }
	void SetCommandTimeout(double Seconds) { CommandTimeout = Seconds; }

	/** Forget every submitted command, as when resetting a scenario. */
	void Reset();

private:
	double CommandTimeout;
	EDriveSource ActiveSource = EDriveSource::None;

	/** Latest command per source, indexed by the source's numeric value. */
	static constexpr int32 SourceCount = 4;
	FDriveCommand Latest[SourceCount];
};