#pragma once

#include "CoreMinimal.h"
#include "RoboLidar.h"
#include "Misc/AutomationTest.h"

/**
 * Shared settings for this project's automation tests.
 *
 * These live in a named namespace rather than being copied into an anonymous
 * namespace per file because Unreal compiles several .cpp files together as a
 * single translation unit. An anonymous namespace is private to a translation
 * unit, not to a file, so duplicated names across files collide.
 */
namespace RoboTest
{
	inline constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	inline constexpr double Tolerance = 1e-9;

	/**
	 * A 270 degree scan at one-degree spacing with nothing in it, then each
	 * given point placed on the beam nearest its bearing. Lets a test build any
	 * situation the lidar could see, by hand.
	 */
	inline FLidarScan MakeScan(const TArray<FVector2D>& Points)
	{
		constexpr int32 BeamCount = 271;

		FLidarScan Scan;
		Scan.AngleMinRad = FMath::DegreesToRadians(-135.0);
		Scan.AngleIncrementRad = FMath::DegreesToRadians(1.0);
		Scan.RangeMinM = 0.05;
		Scan.RangeMaxM = 12.0;
		Scan.RangesM.Init(RoboLidar::NoReturn(), BeamCount);

		for (const FVector2D& Point : Points)
		{
			const double BearingRad = FMath::Atan2(Point.Y, Point.X);
			const int32 Beam = FMath::RoundToInt((BearingRad - Scan.AngleMinRad) / Scan.AngleIncrementRad);

			if (Beam >= 0 && Beam < BeamCount)
			{
				Scan.RangesM[Beam] = FMath::Min(Scan.RangesM[Beam], Point.Size());
			}
		}

		return Scan;
	}
}