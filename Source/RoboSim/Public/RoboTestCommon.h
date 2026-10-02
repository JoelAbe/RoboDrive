#pragma once

#include "CoreMinimal.h"
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
}