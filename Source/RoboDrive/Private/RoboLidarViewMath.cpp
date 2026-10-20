#include "RoboLidarViewMath.h"

FVector2D RoboLidarViewMath::RobotToView(const FVector2D& PointM, const FVector2D& RobotPx, double PixelsPerMetre)
{
	// Forward (+X) is up the screen, which is screen -Y. Left (+Y) is screen -X.
	return FVector2D(
		RobotPx.X - PointM.Y * PixelsPerMetre,
		RobotPx.Y - PointM.X * PixelsPerMetre);
}

FVector2D RoboLidarViewMath::AlongHeadingM(double HeadingRad, double DistanceM)
{
	return FVector2D(FMath::Cos(HeadingRad), FMath::Sin(HeadingRad)) * DistanceM;
}

TArray<FVector2D> RoboLidarViewMath::CorridorCornersM(double HeadingRad, double LengthM, double HalfWidthM)
{
	// Two unit vectors: one along the heading, one a quarter turn to its left.
	const FVector2D Forward(FMath::Cos(HeadingRad), FMath::Sin(HeadingRad));
	const FVector2D Left(-Forward.Y, Forward.X);

	return {
		Left * HalfWidthM,                          // near left, beside the robot
		Forward * LengthM + Left * HalfWidthM,      // far left
		Forward * LengthM - Left * HalfWidthM,      // far right
		-Left * HalfWidthM                          // near right
	};
}