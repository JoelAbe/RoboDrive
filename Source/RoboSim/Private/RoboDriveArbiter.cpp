#include "RoboDriveArbiter.h"

FDriveArbiter::FDriveArbiter(double InCommandTimeout): CommandTimeout(InCommandTimeout)
{
}

void FDriveArbiter::Submit(const FDriveCommand& Command)
{
	if (Command.Source == EDriveSource::None)
	{
		return;
	}

	Latest[static_cast<uint8>(Command.Source)] = Command;
}

FDriveCommand FDriveArbiter::Evaluate(double SimTimeSeconds)
{
	// A default-constructed command is a stop from nobody, which is the
	// correct answer when no source is driving.
	FDriveCommand Winner;

	for (const FDriveCommand& Candidate : Latest)
	{
		if (Candidate.Source == EDriveSource::None)
		{
			continue;
		}

		const bool bFresh = (SimTimeSeconds - Candidate.TimeSeconds) <= CommandTimeout;
		const bool bHigherPriority = Candidate.Source > Winner.Source;

		if (bFresh && bHigherPriority)
		{
			Winner = Candidate;
		}
	}

	ActiveSource = Winner.Source;
	return Winner;
}

void FDriveArbiter::Reset()
{
	for (FDriveCommand& Command : Latest)
	{
		Command = FDriveCommand();
	}

	ActiveSource = EDriveSource::None;
}