#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"

struct FRoboOperatorTelemetry;

/**
 * A top-down drawing of what the robot sees: its lidar returns, the direction
 * of the goal, and the heading the brain chose with the corridor it checked.
 *
 * Everything is drawn in the robot's own frame, facing up the screen, from
 * exactly what the brain receives. Fog echoes therefore look like any other
 * return, because to the brain they are.
 *
 * A leaf widget: it has no children and draws itself in OnPaint, the Slate
 * equivalent of drawing with Unity's GL calls or a custom Graphic.
 */
class SRoboLidarView : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SRoboLidarView) {}
		/** The snapshot to draw from. Owned by the operator view, which outlives this widget. */
		SLATE_ARGUMENT(const FRoboOperatorTelemetry*, Telemetry)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	const FRoboOperatorTelemetry* Telemetry = nullptr;
};
