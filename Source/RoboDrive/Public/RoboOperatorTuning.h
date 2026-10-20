#pragma once

#include "CoreMinimal.h"
#include "RoboAutonomyComponent.h"

class APlayerController;

/**
 * The settings an operator can change from the view, read from the robot and
 * the world and written back.
 *
 * The same properties the editor's Details panel edits, gathered in one place,
 * so a packaged build, which has no Details panel, can still be tuned.
 */
struct FRoboOperatorTuning
{
	bool bHasRobot = false;

	// The brain
	EBrainKind BrainKind = EBrainKind::Avoiding;
	double MaxSpeedMS = 0.5;
	double LookaheadM = 1.5;
	double MarginM = 0.15;

	// The weather, and how the lidar copes with it
	bool bFog = false;
	double VisibilityM = 30.0;
	double FogEchoFraction = 0.1;

	// What is drawn in the 3D world
	bool bShowLidarRays = true;
	bool bShowPaths = true;
	bool bShowHeading = true;
	bool bShowReadouts = false;

	/** The current settings of the robot a player controls. Safe with a null player. */
	static FRoboOperatorTuning Read(const APlayerController* Player);

	/** Write these settings to that robot and its world. */
	void Apply(const APlayerController* Player) const;
};