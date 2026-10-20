#pragma once

#include "CoreMinimal.h"
#include "RoboOperatorTelemetry.h"
#include "RoboOperatorTuning.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class APlayerController;
class SVerticalBox;

/**
 * Everything the operator sees, laid over the game view.
 *
 * A top bar across the screen, a column on the left for the robot, one on the
 * right for choosing a scenario and tuning, and the lidar view at the bottom. The rest is left empty, so the world shows through:
 * the panels cover the view rather than shrinking it.
 *
 * Telemetry and tuning are read once per frame in Tick, and everything on
 * screen reads from those copies, so all of it describes the same moment.
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
	TSharedRef<SWidget> MakeTuningPanel();
	TSharedRef<SWidget> MakeScenarioPanel();
	TSharedRef<SWidget> MakeLidarPanel();

	/** Re-read the Scenarios folder and rebuild the list of buttons. */
	void RefreshScenarioList();

	/** Load a scenario file through the player controller, and close the list. */
	void LoadScenarioFile(const FString& FileName);

	/** Change one or more settings, apply them to the robot, and keep the result. */
	void ChangeTuning(TFunctionRef<void(FRoboOperatorTuning&)> Change);

	TWeakObjectPtr<APlayerController> Player;
	FRoboOperatorTelemetry Telemetry;
	FRoboOperatorTuning Tuning;

	/** The list of scenario files, rebuilt each time it is opened. */
	TSharedPtr<SVerticalBox> ScenarioList;
	bool bScenarioListOpen = false;

	/** Why the last scenario failed to load, shown until the next attempt. */
	FText ScenarioError;
};
