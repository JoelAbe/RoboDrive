#pragma once

#include "CoreMinimal.h"

/**
 * Geometry for drawing the robot's view of the world on screen.
 *
 * Points come in the robot's own frame, REP-103: metres, +X forward, +Y left.
 * The view shows the robot facing up the screen, where screen coordinates are
 * pixels with +X right and +Y DOWN. Both axes therefore swap and flip.
 */
namespace RoboLidarViewMath
{
	/** A point in the robot's frame, m, to a position in the view, px. */
	FVector2D RobotToView(const FVector2D& PointM, const FVector2D& RobotPx, double PixelsPerMetre);

	/** The point DistanceM along a heading from the robot, in the robot's frame. */
	FVector2D AlongHeadingM(double HeadingRad, double DistanceM);

	/**
	 * The four corners of the corridor a heading must keep clear, in the
	 * robot's frame, in order around it: the rectangle that
	 * RoboAvoidance::IsHeadingClear checks, from the robot out to LengthM ahead
	 * and HalfWidthM either side.
	 */
	TArray<FVector2D> CorridorCornersM(double HeadingRad, double LengthM, double HalfWidthM);
}