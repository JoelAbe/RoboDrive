#include "RoboApiSettings.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

int32 URoboApiSettings::ResolvePort() const
{
	int32 Override = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("RoboApiPort="), Override) && Override > 0)
	{
		return Override;
	}

	return Port;
}