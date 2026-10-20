#include "RoboOperatorWidget.h"

#include "GameFramework/PlayerController.h"
#include "UI/SRoboOperatorView.h"

void URoboOperatorWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The view itself lets clicks through to the game; only its panels and
	// buttons can be clicked.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
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
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
}
