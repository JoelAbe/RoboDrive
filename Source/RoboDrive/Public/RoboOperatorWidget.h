#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RoboOperatorWidget.generated.h"

class SRoboOperatorView;

/**
 * The operator view, as a UMG widget, so the player controller can create it
 * and put it on screen like any other widget.
 *
 * It has no Widget Blueprint. Its contents are built in C++ with Slate, in
 * SRoboOperatorView, so the layout is text that can be read and reviewed.
 * What it shows comes from FRoboOperatorTelemetry.
 */
UCLASS()
class URoboOperatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Hide the operator view if it is showing, and show it if hidden. */
	void ToggleShown();

protected:
	virtual void NativeConstruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedPtr<SRoboOperatorView> View;
};
