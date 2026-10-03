#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleManager.h"

ROBOSIM_API DECLARE_LOG_CATEGORY_EXTERN(LogRoboSim, Log, All);

/** Cost of this project's own simulation work. Toggle with "stat RoboSim". */
DECLARE_STATS_GROUP(TEXT("RoboSim"), STATGROUP_RoboSim, STATCAT_Advanced);

