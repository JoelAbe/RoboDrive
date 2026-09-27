#include "RoboSimClock.h"

FSimClock::FSimClock(double InStepSeconds) : StepSeconds(InStepSeconds > 0.0 ? InStepSeconds : DefaultStepSeconds)
{
}

int32 FSimClock::Advance(double RealDeltaSeconds)
{
	if (Mode == ESimClockMode::Synchronous || RealDeltaSeconds <= 0.0)
	{
		return 0;
	}

	Accumulator += RealDeltaSeconds;

	int32 StepsTaken = 0;
	while (Accumulator >= StepSeconds && StepsTaken < MaxStepsPerAdvance)
	{
		Accumulator -= StepSeconds;
		++StepCount;
		++StepsTaken;
	}

	// Long stall: give up on catching up rather than demanding an ever larger
	// burst of steps. Sim time falls behind real time, which is the correct
	// trade for a simulator; the alternative is a spiral it never escapes.
	if (Accumulator > StepSeconds * MaxStepsPerAdvance)
	{
		Accumulator = 0.0;
	}

	return StepsTaken;
}

int32 FSimClock::Step(int32 Steps)
{
	if (Steps <= 0)
	{
		return 0;
	}

	StepCount += Steps;
	return Steps;
}

void FSimClock::Reset()
{
	Accumulator = 0.0;
	StepCount = 0;
}