#include "RoboOperatorWidget.h"

#include "GameFramework/PlayerController.h"
#include "UI/SRoboOperatorView.h"

void URoboOperatorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// On screen, but never in the way: clicks and keys pass through to the game.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> URoboOperatorWidget::RebuildWidget()
{
	// A Widget Blueprint would build its contents from the designer here.
	// This widget builds them in C++ instead.
	View = SNew(SRoboOperatorView)
		.Player(GetOwningPlayer());

	return View.ToSharedRef();
}

void URoboOperatorWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	View.Reset();
}

void URoboOperatorWidget::ToggleShown()
{
	SetVisibility(GetVisibility() == ESlateVisibility::Collapsed
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
}
