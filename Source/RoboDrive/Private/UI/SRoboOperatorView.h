#pragma once

#include "CoreMinimal.h"
#include "RoboOperatorTelemetry.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;

/**
 * Everything the operator sees, laid over the game view.
 *
 * A top bar across the screen, and a column on the left for the robot. The
 * rest is left empty, so the world shows through: the panels cover the view
 * rather than shrinking it.
 *
 * Telemetry is gathered once per frame in Tick, and every text on screen reads
 * from that one copy, so all of it describes the same moment.
 */
class SRoboOperatorView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRoboOperatorView) {}
		/** The player whose robot to show. */
		SLATE_ARGUMENT(TWeakObjectPtr<APlayerController>, Player)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedRef<SWidget> MakeTopBar();
	TSharedRef<SWidget> MakeRobotPanel();

	TWeakObjectPtr<APlayerController> Player;
	FRoboOperatorTelemetry Telemetry;
};
