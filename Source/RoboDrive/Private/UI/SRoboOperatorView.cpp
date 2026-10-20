#include "SRoboOperatorView.h"

#include "GameFramework/PlayerController.h"
#include "RoboOperatorStyle.h"
#include "RoboPlayerController.h"
#include "RoboScenarioFiles.h"
#include "SRoboLidarView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/**
	 * A value with anything that would round to zero made exactly zero, so it
	 * prints as "0.00" rather than "-0.00", which reads as a direction when
	 * there is none. Resolution is the smallest step the display shows.
	 */
	double OperatorClean(double Value, double Resolution)
	{
		return FMath::Abs(Value) < 0.5 * Resolution ? 0.0 : Value;
	}

	/**
	 * Handles a click on a panel's background by doing nothing with it.
	 * Unhandled clicks travel on up to the game viewport, and a click on the
	 * world sets the robot's goal: a click meant for a panel must stop here.
	 */
	FReply OperatorSwallowClick(const FGeometry&, const FPointerEvent&)
	{
		return FReply::Handled();
	}

	/** Shown in place of a value that does not exist, such as speed with no robot. */
	FText OperatorNoValue()
	{
		return INVTEXT("-");
	}

	FSlateColor OperatorSourceColour(EDriveSource Source)
	{
		switch (Source)
		{
		case EDriveSource::Autonomy: return RoboOperatorStyle::Normal();
		case EDriveSource::Manual:   return RoboOperatorStyle::Attention();
		case EDriveSource::External: return RoboOperatorStyle::External();
		default:                     return RoboOperatorStyle::TextMuted();
		}
	}

	FSlateColor OperatorMissionColour(EMissionStatus Status)
	{
		switch (Status)
		{
		case EMissionStatus::Active:  return RoboOperatorStyle::Attention();
		case EMissionStatus::Arrived: return RoboOperatorStyle::Normal();
		case EMissionStatus::Failed:  return RoboOperatorStyle::Stop();
		default:                      return RoboOperatorStyle::TextMuted();
		}
	}

	/**
	 * What the brain is doing, in a sentence. When the brain is not in control,
	 * that is said instead: it keeps deciding, but nobody is listening, and
	 * showing its plans as if they were happening would mislead.
	 */
	FText OperatorActivityText(const FRoboOperatorTelemetry& Telemetry)
	{
		if (!Telemetry.bHasBrainActivity)
		{
			return FText::GetEmpty();
		}

		if (Telemetry.bStopped)
		{
			return INVTEXT("Overruled by the emergency stop");
		}

		if (Telemetry.DriveMode == EDriveMode::Manual)
		{
			return INVTEXT("Ignored in manual mode");
		}

		switch (Telemetry.BrainActivity)
		{
		case EBrainActivity::Idle:
			return INVTEXT("Waiting for a goal");

		case EBrainActivity::WaitingForScan:
			return INVTEXT("Waiting for its first lidar scan");

		case EBrainActivity::DrivingToGoal:
			return INVTEXT("Heading straight for the goal");

		case EBrainActivity::SteeringAround:
			// Positive is left, as everywhere in REP-103.
			return FText::FromString(FString::Printf(TEXT("Steering %.0f deg %s of the goal to get around something"),
				FMath::Abs(Telemetry.DeviationDeg), Telemetry.DeviationDeg > 0.0 ? TEXT("left") : TEXT("right")));

		case EBrainActivity::BoxedIn:
			return FText::FromString(FString::Printf(TEXT("Boxed in for %.1f s, turning to look for a way out"),
				Telemetry.BlockedForSeconds));

		case EBrainActivity::GaveUp:
			return INVTEXT("Gave up: boxed in for too long");

		case EBrainActivity::Arrived:
			return INVTEXT("At the goal");

		default:
			return FText::GetEmpty();
		}
	}

	FSlateColor OperatorActivityColour(const FRoboOperatorTelemetry& Telemetry)
	{
		if (Telemetry.bStopped || Telemetry.DriveMode == EDriveMode::Manual)
		{
			return RoboOperatorStyle::TextMuted();
		}

		switch (Telemetry.BrainActivity)
		{
		case EBrainActivity::DrivingToGoal:
		case EBrainActivity::Arrived:        return RoboOperatorStyle::Normal();
		case EBrainActivity::SteeringAround:
		case EBrainActivity::BoxedIn:        return RoboOperatorStyle::Attention();
		case EBrainActivity::GaveUp:         return RoboOperatorStyle::Stop();
		default:                             return RoboOperatorStyle::TextMuted();
		}
	}

	/** The player controller that carries out the operator's actions, if there is one. */
	ARoboPlayerController* OperatorController(const TWeakObjectPtr<APlayerController>& Player)
	{
		return Cast<ARoboPlayerController>(Player.Get());
	}

	/** A small coloured circle, for a status at a glance. */
	TSharedRef<SWidget> OperatorDot(const TAttribute<FSlateColor>& Colour)
	{
		return SNew(SBox)
			.WidthOverride(8.0f)
			.HeightOverride(8.0f)
			[
				SNew(SBorder)
				.BorderImage(RoboOperatorStyle::DotBrush())
				.BorderBackgroundColor(Colour)
				.Padding(0.0f)
			];
	}

	/** A one-pixel rule between sections. */
	TSharedRef<SWidget> OperatorDivider()
	{
		return SNew(SBox)
			.HeightOverride(1.0f)
			[
				SNew(SBorder)
				.BorderImage(RoboOperatorStyle::DividerBrush())
				.Padding(0.0f)
			];
	}

	/** A small muted label with its value underneath: the panel's basic unit. */
	TSharedRef<SWidget> OperatorField(const FText& Label, const TAttribute<FText>& Value,
		const TAttribute<FSlateColor>& Colour, const FSlateFontInfo& Font)
	{
		return SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(RoboOperatorStyle::LabelFont())
				.ColorAndOpacity(RoboOperatorStyle::TextMuted())
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(Value)
				.Font(Font)
				.ColorAndOpacity(Colour)
			];
	}

	/** A label and value on one line, for the top bar. */
	TSharedRef<SWidget> OperatorInlineField(const FText& Label, const TAttribute<FText>& Value)
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(RoboOperatorStyle::LabelFont())
				.ColorAndOpacity(RoboOperatorStyle::TextMuted())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(Value)
				.Font(RoboOperatorStyle::NumberFont())
				.ColorAndOpacity(RoboOperatorStyle::TextPrimary())
			];
	}

	/**
	 * A button with a label and the key that does the same thing, so the
	 * shortcut is learned by using the button.
	 *
	 * Not focusable: a focusable button would take the keyboard after a click,
	 * and the keys that drive the robot would stop reaching the game.
	 */
	TSharedRef<SWidget> OperatorButton(const TAttribute<FText>& Label, const FText& Key,
		const TAttribute<FSlateColor>& Fill, const TAttribute<FSlateColor>& TextColour,
		FOnClicked OnClicked)
	{
		return SNew(SButton)
			.ButtonStyle(&RoboOperatorStyle::ButtonStyle())
			.ButtonColorAndOpacity(Fill)
			.ContentPadding(FMargin(12.0f, 5.0f))
			.IsFocusable(false)
			.OnClicked(OnClicked)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(RoboOperatorStyle::TitleFont())
					.ColorAndOpacity(TextColour)
				]

				// Buttons without a shortcut collapse this, so the label stays centred.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(Key)
					.Font(RoboOperatorStyle::LabelFont())
					.ColorAndOpacity(TextColour)
					.Visibility(Key.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
				]
			];
	}

	/** A section heading inside a panel. */
	TSharedRef<SWidget> OperatorSectionLabel(const FText& Text)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(RoboOperatorStyle::LabelFont())
			.ColorAndOpacity(RoboOperatorStyle::TextMuted());
	}

	/**
	 * A labelled slider with its value on the right. Not focusable, for the
	 * same reason as the buttons: a focused slider would take the arrow keys.
	 */
	TSharedRef<SWidget> OperatorSlider(const FText& Label, float Min, float Max,
		const TAttribute<float>& Value, const TAttribute<FText>& ValueText,
		const FOnFloatValueChanged& OnChanged)
	{
		return SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(RoboOperatorStyle::BodyFont())
					.ColorAndOpacity(RoboOperatorStyle::TextPrimary())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(ValueText)
					.Font(RoboOperatorStyle::NumberFont())
					.ColorAndOpacity(RoboOperatorStyle::TextPrimary())
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SSlider)
				.Style(&RoboOperatorStyle::SliderStyle())
				.MinValue(Min)
				.MaxValue(Max)
				.Value(Value)
				.OnValueChanged(OnChanged)
				.IsFocusable(false)
				.SliderBarColor(RoboOperatorStyle::ControlFill())
				.SliderHandleColor(RoboOperatorStyle::Normal())
			];
	}

	/** Two to four mutually exclusive choices, as a row of buttons. */
	TSharedRef<SWidget> OperatorChoice(const FText& Label, const TAttribute<bool>& bSelected, FOnClicked OnClicked)
	{
		return OperatorButton(Label, FText::GetEmpty(),
			TAttribute<FSlateColor>::CreateLambda([bSelected]
			{
				return bSelected.Get() ? RoboOperatorStyle::Normal() : RoboOperatorStyle::ControlFill();
			}),
			TAttribute<FSlateColor>::CreateLambda([bSelected]
			{
				return bSelected.Get() ? RoboOperatorStyle::TextOnFill() : RoboOperatorStyle::TextPrimary();
			}),
			OnClicked);
	}

	// Visibility spans orders of magnitude, from a few metres to hundreds, so
	// its slider is logarithmic: moving it the same distance anywhere along
	// its length multiplies the visibility by the same factor.
	constexpr double OperatorMinVisibilityM = 5.0;
	constexpr double OperatorMaxVisibilityM = 200.0;

	double OperatorSliderToVisibility(float Position)
	{
		return OperatorMinVisibilityM * FMath::Pow(OperatorMaxVisibilityM / OperatorMinVisibilityM, static_cast<double>(Position));
	}

	float OperatorVisibilityToSlider(double VisibilityM)
	{
		const double Clamped = FMath::Clamp(VisibilityM, OperatorMinVisibilityM, OperatorMaxVisibilityM);
		return static_cast<float>(FMath::Loge(Clamped / OperatorMinVisibilityM)
			/ FMath::Loge(OperatorMaxVisibilityM / OperatorMinVisibilityM));
	}
}

void SRoboOperatorView::Construct(const FArguments& InArgs)
{
	Player = InArgs._Player;
	Telemetry = FRoboOperatorTelemetry::Gather(Player.Get());
	Tuning = FRoboOperatorTuning::Read(Player.Get());

	const float Margin = RoboOperatorStyle::ScreenMargin;

	// The containers that span the screen pass clicks through to the game.
	// Only the panels and buttons themselves can be clicked.
	ChildSlot
	[
		SNew(SVerticalBox)
		.Visibility(EVisibility::SelfHitTestInvisible)

		// The top bar, across the full width.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(Margin, Margin, Margin, 0.0f)
		[
			MakeTopBar()
		]

		// Below it, the robot on the left, tuning on the right, the world between.
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(Margin, 12.0f, Margin, Margin)
		[
			SNew(SHorizontalBox)
			.Visibility(EVisibility::SelfHitTestInvisible)

			// Only as tall as its contents, so it covers no more than it must.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			[
				SNew(SBox)
				.WidthOverride(RoboOperatorStyle::ColumnWidth)
				[
					MakeRobotPanel()
				]
			]

			// The world shows through here, with only the lidar view at the
			// bottom, small, where it covers the least of what matters.
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			[
				MakeLidarPanel()
			]

			// Choosing a scenario above, tuning below.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			[
				SNew(SBox)
				.WidthOverride(RoboOperatorStyle::ColumnWidth)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						MakeScenarioPanel()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						MakeTuningPanel()
					]
				]
			]
		]
	];
}

void SRoboOperatorView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	Telemetry = FRoboOperatorTelemetry::Gather(Player.Get());

	// Read back every frame, so a change made elsewhere, such as in the
	// editor's Details panel, shows here too.
	Tuning = FRoboOperatorTuning::Read(Player.Get());
}

void SRoboOperatorView::ChangeTuning(TFunctionRef<void(FRoboOperatorTuning&)> Change)
{
	FRoboOperatorTuning Changed = Tuning;
	Change(Changed);
	Changed.Apply(Player.Get());
	Tuning = Changed;
}

TSharedRef<SWidget> SRoboOperatorView::MakeTopBar()
{
	using namespace RoboOperatorStyle;

	return SNew(SBorder)
		.BorderImage(PanelBrush())
		.OnMouseButtonDown_Static(&OperatorSwallowClick)
		.Padding(FMargin(14.0f, 8.0f))
		[
			SNew(SHorizontalBox)

			// The state of the whole simulation, with the emergency stop
			// taking precedence over everything else.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				OperatorDot(TAttribute<FSlateColor>::CreateLambda([this]
				{
					if (!Telemetry.bHasSim)
					{
						return TextMuted();
					}
					return Telemetry.bStopped ? Stop() : Normal();
				}))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(TitleFont())
				.ColorAndOpacity_Lambda([this]
				{
					return Telemetry.bStopped ? Stop() : TextPrimary();
				})
				.Text_Lambda([this]
				{
					if (!Telemetry.bHasSim)
					{
						return INVTEXT("No simulation");
					}
					return Telemetry.bStopped ? INVTEXT("Emergency stop") : INVTEXT("Simulating");
				})
			]

			// Which scenario is running, if any.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(BodyFont())
				.ColorAndOpacity(TextMuted())
				.Text_Lambda([this]
				{
					return Telemetry.bHasScenario ? FText::FromString(Telemetry.ScenarioName) : INVTEXT("No scenario");
				})
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				OperatorInlineField(INVTEXT("Sim time"), TAttribute<FText>::CreateLambda([this]
				{
					return Telemetry.bHasSim
						? FText::FromString(FString::Printf(TEXT("%.1f s"), Telemetry.SimTimeSeconds))
						: OperatorNoValue();
				}))
			]

			// Below 1x the simulation cannot keep up with the wall clock.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(24.0f, 0.0f, 0.0f, 0.0f)
			[
				OperatorInlineField(INVTEXT("Speed"), TAttribute<FText>::CreateLambda([this]
				{
					return Telemetry.bHasSim
						? FText::FromString(FString::Printf(TEXT("%.1fx real time"), Telemetry.RealTimeFactor))
						: OperatorNoValue();
				}))
			]

			// Mode: who may drive. The selected one is filled.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(32.0f, 0.0f, 0.0f, 0.0f)
			[
				OperatorButton(INVTEXT("Auto"), INVTEXT("1"),
					TAttribute<FSlateColor>::CreateLambda([this]
					{
						return Telemetry.DriveMode == EDriveMode::Auto ? Normal() : ControlFill();
					}),
					TAttribute<FSlateColor>::CreateLambda([this]
					{
						return Telemetry.DriveMode == EDriveMode::Auto ? TextOnFill() : TextPrimary();
					}),
					FOnClicked::CreateLambda([this]
					{
						if (ARoboPlayerController* Controller = OperatorController(Player))
						{
							Controller->SetDriveMode(EDriveMode::Auto);
						}
						return FReply::Handled();
					}))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				OperatorButton(INVTEXT("Manual"), INVTEXT("2"),
					TAttribute<FSlateColor>::CreateLambda([this]
					{
						return Telemetry.DriveMode == EDriveMode::Manual ? Attention() : ControlFill();
					}),
					TAttribute<FSlateColor>::CreateLambda([this]
					{
						return Telemetry.DriveMode == EDriveMode::Manual ? TextOnFill() : TextPrimary();
					}),
					FOnClicked::CreateLambda([this]
					{
						if (ARoboPlayerController* Controller = OperatorController(Player))
						{
							Controller->SetDriveMode(EDriveMode::Manual);
						}
						return FReply::Handled();
					}))
			]

			// The emergency stop. Space engages it from anywhere; releasing it
			// takes this button, so a stop is never undone by a stray key.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(16.0f, 0.0f, 0.0f, 0.0f)
			[
				OperatorButton(
					TAttribute<FText>::CreateLambda([this]
					{
						return Telemetry.bStopped ? INVTEXT("Release stop") : INVTEXT("Stop");
					}),
					INVTEXT("Space"),
					TAttribute<FSlateColor>::CreateLambda([this]
					{
						return Telemetry.bStopped ? Attention() : Stop();
					}),
					TextOnFill(),
					FOnClicked::CreateLambda([this]
					{
						if (ARoboPlayerController* Controller = OperatorController(Player))
						{
							if (Telemetry.bStopped)
							{
								Controller->ReleaseEmergencyStop();
							}
							else
							{
								Controller->EngageEmergencyStop();
							}
						}
						return FReply::Handled();
					}))
			]
		];
}

TSharedRef<SWidget> SRoboOperatorView::MakeRobotPanel()
{
	using namespace RoboOperatorStyle;

	return SNew(SBorder)
		.BorderImage(PanelBrush())
		.OnMouseButtonDown_Static(&OperatorSwallowClick)
		.Padding(PanelPadding)
		[
			SNew(SVerticalBox)

			// Which robot, with a dot coloured by how its mission is going.
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					OperatorDot(TAttribute<FSlateColor>::CreateLambda([this]
					{
						return OperatorMissionColour(Telemetry.MissionStatus);
					}))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Font(TitleFont())
					.ColorAndOpacity(TextPrimary())
					.Text_Lambda([this]
					{
						return Telemetry.bHasRobot ? FText::FromString(Telemetry.RobotName) : INVTEXT("No robot");
					})
				]
			]

			// Who is driving: the arbiter's choice, made visible.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 14.0f, 0.0f, 0.0f)
			[
				OperatorField(INVTEXT("Driving"),
					TAttribute<FText>::CreateLambda([this]
					{
						if (!Telemetry.bHasRobot)
						{
							return OperatorNoValue();
						}
						return Telemetry.bStopped
							? INVTEXT("Stopped")
							: UEnum::GetDisplayValueAsText(Telemetry.DrivingSource);
					}),
					TAttribute<FSlateColor>::CreateLambda([this]
					{
						return Telemetry.bStopped ? Stop() : OperatorSourceColour(Telemetry.DrivingSource);
					}),
					ValueFont())
			]

			// What was asked for, above what the robot is actually doing. The
			// gap between the rows is the vehicle's own response: acceleration,
			// grip and slip.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(0.0f, 0.0f, 12.0f, 10.0f))

				+ SUniformGridPanel::Slot(0, 0)
				[
					OperatorField(INVTEXT("Command v"),
						TAttribute<FText>::CreateLambda([this]
						{
							return Telemetry.bHasRobot
								? FText::FromString(FString::Printf(TEXT("%.2f m/s"), OperatorClean(Telemetry.CommandLinearMS, 0.01)))
								: OperatorNoValue();
						}),
						TextPrimary(), NumberFont())
				]

				+ SUniformGridPanel::Slot(1, 0)
				[
					OperatorField(INVTEXT("Command w"),
						TAttribute<FText>::CreateLambda([this]
						{
							return Telemetry.bHasRobot
								? FText::FromString(FString::Printf(TEXT("%.2f rad/s"), OperatorClean(Telemetry.CommandAngularRadS, 0.01)))
								: OperatorNoValue();
						}),
						TextPrimary(), NumberFont())
				]

				+ SUniformGridPanel::Slot(0, 1)
				[
					OperatorField(INVTEXT("Actual v"),
						TAttribute<FText>::CreateLambda([this]
						{
							return Telemetry.bHasRobot
								? FText::FromString(FString::Printf(TEXT("%.2f m/s"), OperatorClean(Telemetry.ActualLinearMS, 0.01)))
								: OperatorNoValue();
						}),
						TextPrimary(), NumberFont())
				]

				+ SUniformGridPanel::Slot(1, 1)
				[
					OperatorField(INVTEXT("Actual w"),
						TAttribute<FText>::CreateLambda([this]
						{
							return Telemetry.bHasRobot
								? FText::FromString(FString::Printf(TEXT("%.2f rad/s"), OperatorClean(Telemetry.ActualYawRateRadS, 0.01)))
								: OperatorNoValue();
						}),
						TextPrimary(), NumberFont())
				]
			]

			// Which brain, and what it is doing right now.
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				OperatorField(INVTEXT("Brain"),
					TAttribute<FText>::CreateLambda([this]
					{
						return Telemetry.bHasRobot ? FText::FromString(Telemetry.BrainName) : OperatorNoValue();
					}),
					TextPrimary(), BodyFont())
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(BodyFont())
				.AutoWrapText(true)
				.Visibility_Lambda([this]
				{
					return Telemetry.bHasBrainActivity ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.ColorAndOpacity_Lambda([this]
				{
					return OperatorActivityColour(Telemetry);
				})
				.Text_Lambda([this]
				{
					return OperatorActivityText(Telemetry);
				})
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 12.0f)
			[
				OperatorDivider()
			]

			// The mission, as the robot believes it is going.
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				OperatorField(INVTEXT("Mission"),
					TAttribute<FText>::CreateLambda([this]
					{
						return Telemetry.bHasGoal ? UEnum::GetDisplayValueAsText(Telemetry.MissionStatus) : INVTEXT("No goal");
					}),
					TAttribute<FSlateColor>::CreateLambda([this]
					{
						return OperatorMissionColour(Telemetry.MissionStatus);
					}),
					ValueFont())
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(BodyFont())
				.ColorAndOpacity(TextMuted())
				.AutoWrapText(true)
				// Collapsed, not just empty, without a goal: an empty line
				// still takes up its padding and leaves a gap.
				.Visibility_Lambda([this]
				{
					return Telemetry.bHasGoal ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.Text_Lambda([this]
				{
					if (!Telemetry.bHasGoal)
					{
						return FText::GetEmpty();
					}

					FString Text = FString::Printf(TEXT("Goal (%.1f, %.1f)"), Telemetry.GoalX, Telemetry.GoalY);
					if (Telemetry.bHasBelievedDistance)
					{
						Text += FString::Printf(TEXT(", %.1f m away, it believes"), Telemetry.BelievedDistanceM);
					}
					return FText::FromString(Text);
				})
			]

			// How to give it somewhere to go, said where someone would look.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(LabelFont())
				.ColorAndOpacity(TextFaint())
				.AutoWrapText(true)
				.Text_Lambda([this]
				{
					return Telemetry.bHasGoal
						? INVTEXT("Click the floor for a new goal, right-click to clear it")
						: INVTEXT("Click the floor to give the robot a goal");
				})
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 12.0f)
			[
				OperatorDivider()
			]

			// The sensor, and the conditions it is working in.
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				OperatorField(INVTEXT("Lidar"),
					TAttribute<FText>::CreateLambda([this]
					{
						return Telemetry.bHasLidar
							? FText::FromString(FString::Printf(TEXT("%d beams, %.0f Hz, %.0f m"),
								Telemetry.LidarBeamCount, Telemetry.LidarRateHz, Telemetry.LidarRangeM))
							: INVTEXT("None");
					}),
					TextPrimary(), BodyFont())
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				OperatorField(INVTEXT("Weather"),
					TAttribute<FText>::CreateLambda([this]
					{
						return Telemetry.bHasFog
							? FText::FromString(FString::Printf(TEXT("Fog, %.0f m visibility"), Telemetry.VisibilityM))
							: INVTEXT("Clear");
					}),
					TAttribute<FSlateColor>::CreateLambda([this]
					{
						return Telemetry.bHasFog ? Attention() : TextPrimary();
					}),
					BodyFont())
			]

			// Ground truth, set apart: the operator may see it, the robot never does.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 16.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.BorderImage(TruthBrush())
				.Padding(10.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(INVTEXT("Ground truth, never shown to the robot"))
						.Font(LabelFont())
						.ColorAndOpacity(Attention())
					]

					// How far the robot's belief has drifted, as two separate
					// numbers so neither can be cut off by the column's width.
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(SUniformGridPanel)
						.SlotPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))

						+ SUniformGridPanel::Slot(0, 0)
						[
							OperatorField(INVTEXT("Odometry off by"),
								TAttribute<FText>::CreateLambda([this]
								{
									return Telemetry.bHasOdometry
										? FText::FromString(FString::Printf(TEXT("%.2f m"), Telemetry.OdometryErrorM))
										: OperatorNoValue();
								}),
								TextPrimary(), NumberFont())
						]

						+ SUniformGridPanel::Slot(1, 0)
						[
							OperatorField(INVTEXT("Heading off by"),
								TAttribute<FText>::CreateLambda([this]
								{
									return Telemetry.bHasOdometry
										? FText::FromString(FString::Printf(TEXT("%.1f deg"), OperatorClean(Telemetry.HeadingErrorDeg, 0.1)))
										: OperatorNoValue();
								}),
								TextPrimary(), NumberFont())
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> SRoboOperatorView::MakeTuningPanel()
{
	using namespace RoboOperatorStyle;

	return SNew(SBorder)
		.BorderImage(PanelBrush())
		.OnMouseButtonDown_Static(&OperatorSwallowClick)
		.Padding(PanelPadding)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(INVTEXT("Tuning"))
				.Font(TitleFont())
				.ColorAndOpacity(TextPrimary())
			]

			// Which brain drives.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 14.0f, 0.0f, 6.0f)
			[
				OperatorSectionLabel(INVTEXT("Brain"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					OperatorChoice(INVTEXT("Go to goal"),
						TAttribute<bool>::CreateLambda([this] { return Tuning.BrainKind == EBrainKind::GoToGoal; }),
						FOnClicked::CreateLambda([this]
						{
							ChangeTuning([](FRoboOperatorTuning& T) { T.BrainKind = EBrainKind::GoToGoal; });
							return FReply::Handled();
						}))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					OperatorChoice(INVTEXT("Avoiding"),
						TAttribute<bool>::CreateLambda([this] { return Tuning.BrainKind == EBrainKind::Avoiding; }),
						FOnClicked::CreateLambda([this]
						{
							ChangeTuning([](FRoboOperatorTuning& T) { T.BrainKind = EBrainKind::Avoiding; });
							return FReply::Handled();
						}))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 14.0f, 0.0f, 0.0f)
			[
				OperatorSlider(INVTEXT("Max speed"), 0.1f, 1.0f,
					TAttribute<float>::CreateLambda([this] { return static_cast<float>(Tuning.MaxSpeedMS); }),
					TAttribute<FText>::CreateLambda([this]
					{
						return FText::FromString(FString::Printf(TEXT("%.2f m/s"), Tuning.MaxSpeedMS));
					}),
					FOnFloatValueChanged::CreateLambda([this](float NewValue)
					{
						ChangeTuning([NewValue](FRoboOperatorTuning& T) { T.MaxSpeedMS = NewValue; });
					}))
			]

			// How far ahead avoidance looks, and how much room it leaves. Used
			// only by the avoiding brain.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				OperatorSlider(INVTEXT("Avoidance lookahead"), 0.5f, 3.0f,
					TAttribute<float>::CreateLambda([this] { return static_cast<float>(Tuning.LookaheadM); }),
					TAttribute<FText>::CreateLambda([this]
					{
						return FText::FromString(FString::Printf(TEXT("%.1f m"), Tuning.LookaheadM));
					}),
					FOnFloatValueChanged::CreateLambda([this](float NewValue)
					{
						ChangeTuning([NewValue](FRoboOperatorTuning& T) { T.LookaheadM = NewValue; });
					}))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				OperatorSlider(INVTEXT("Avoidance margin"), 0.0f, 0.5f,
					TAttribute<float>::CreateLambda([this] { return static_cast<float>(Tuning.MarginM); }),
					TAttribute<FText>::CreateLambda([this]
					{
						return FText::FromString(FString::Printf(TEXT("%.2f m"), Tuning.MarginM));
					}),
					FOnFloatValueChanged::CreateLambda([this](float NewValue)
					{
						ChangeTuning([NewValue](FRoboOperatorTuning& T) { T.MarginM = NewValue; });
					}))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 16.0f, 0.0f, 12.0f)
			[
				OperatorDivider()
			]

			// The weather, and how the lidar copes with it.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				OperatorSectionLabel(INVTEXT("Weather"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					OperatorChoice(INVTEXT("Clear"),
						TAttribute<bool>::CreateLambda([this] { return !Tuning.bFog; }),
						FOnClicked::CreateLambda([this]
						{
							ChangeTuning([](FRoboOperatorTuning& T) { T.bFog = false; });
							return FReply::Handled();
						}))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					OperatorChoice(INVTEXT("Fog"),
						TAttribute<bool>::CreateLambda([this] { return Tuning.bFog; }),
						FOnClicked::CreateLambda([this]
						{
							ChangeTuning([](FRoboOperatorTuning& T) { T.bFog = true; });
							return FReply::Handled();
						}))
				]
			]

			// Only meaningful in fog, so only shown in fog.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.Visibility_Lambda([this] { return Tuning.bFog ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					OperatorSlider(INVTEXT("Visibility"), 0.0f, 1.0f,
						TAttribute<float>::CreateLambda([this] { return OperatorVisibilityToSlider(Tuning.VisibilityM); }),
						TAttribute<FText>::CreateLambda([this]
						{
							return FText::FromString(FString::Printf(TEXT("%.0f m"), Tuning.VisibilityM));
						}),
						FOnFloatValueChanged::CreateLambda([this](float NewValue)
						{
							// Whole metres: finer steps mean nothing in fog, and
							// each change to the weather is logged.
							const double VisibilityM = FMath::RoundToDouble(OperatorSliderToVisibility(NewValue));
							ChangeTuning([VisibilityM](FRoboOperatorTuning& T) { T.VisibilityM = VisibilityM; });
						}))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				OperatorSlider(INVTEXT("Lidar fog echoes"), 0.0f, 0.3f,
					TAttribute<float>::CreateLambda([this] { return static_cast<float>(Tuning.FogEchoFraction); }),
					TAttribute<FText>::CreateLambda([this]
					{
						return FText::FromString(FString::Printf(TEXT("%.2f"), Tuning.FogEchoFraction));
					}),
					FOnFloatValueChanged::CreateLambda([this](float NewValue)
					{
						ChangeTuning([NewValue](FRoboOperatorTuning& T) { T.FogEchoFraction = NewValue; });
					}))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 16.0f, 0.0f, 12.0f)
			[
				OperatorDivider()
			]

			// What is drawn in the 3D world. The panels replace the floating
			// text readouts, so those start hidden.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				OperatorSectionLabel(INVTEXT("Show in the world"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(0.0f, 0.0f, 6.0f, 6.0f))

				+ SUniformGridPanel::Slot(0, 0)
				[
					OperatorChoice(INVTEXT("Lidar rays"),
						TAttribute<bool>::CreateLambda([this] { return Tuning.bShowLidarRays; }),
						FOnClicked::CreateLambda([this]
						{
							ChangeTuning([](FRoboOperatorTuning& T) { T.bShowLidarRays = !T.bShowLidarRays; });
							return FReply::Handled();
						}))
				]

				+ SUniformGridPanel::Slot(1, 0)
				[
					OperatorChoice(INVTEXT("Paths"),
						TAttribute<bool>::CreateLambda([this] { return Tuning.bShowPaths; }),
						FOnClicked::CreateLambda([this]
						{
							ChangeTuning([](FRoboOperatorTuning& T) { T.bShowPaths = !T.bShowPaths; });
							return FReply::Handled();
						}))
				]

				+ SUniformGridPanel::Slot(0, 1)
				[
					OperatorChoice(INVTEXT("Heading"),
						TAttribute<bool>::CreateLambda([this] { return Tuning.bShowHeading; }),
						FOnClicked::CreateLambda([this]
						{
							ChangeTuning([](FRoboOperatorTuning& T) { T.bShowHeading = !T.bShowHeading; });
							return FReply::Handled();
						}))
				]

				+ SUniformGridPanel::Slot(1, 1)
				[
					OperatorChoice(INVTEXT("Readouts"),
						TAttribute<bool>::CreateLambda([this] { return Tuning.bShowReadouts; }),
						FOnClicked::CreateLambda([this]
						{
							ChangeTuning([](FRoboOperatorTuning& T) { T.bShowReadouts = !T.bShowReadouts; });
							return FReply::Handled();
						}))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(INVTEXT("Changes apply at once. Changing a brain setting restarts the brain."))
				.Font(LabelFont())
				.ColorAndOpacity(TextFaint())
				.AutoWrapText(true)
			]
		];
}

TSharedRef<SWidget> SRoboOperatorView::MakeScenarioPanel()
{
	using namespace RoboOperatorStyle;

	return SNew(SBorder)
		.BorderImage(PanelBrush())
		.OnMouseButtonDown_Static(&OperatorSwallowClick)
		.Padding(PanelPadding)
		[
			SNew(SVerticalBox)

			// The current scenario, on a line of its own so a long name has room.
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				OperatorField(INVTEXT("Scenario"),
					TAttribute<FText>::CreateLambda([this]
					{
						return Telemetry.bHasScenario ? FText::FromString(Telemetry.ScenarioName) : INVTEXT("None loaded");
					}),
					TextPrimary(), BodyFont())
			]

			// And the controls for it, beneath.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				// Opens and closes the list below, like a dropdown that pushes
				// the panel down rather than floating over it.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					OperatorButton(
						TAttribute<FText>::CreateLambda([this]
						{
							return bScenarioListOpen ? INVTEXT("Close") : INVTEXT("Choose");
						}),
						FText::GetEmpty(), ControlFill(), TextPrimary(),
						FOnClicked::CreateLambda([this]
						{
							bScenarioListOpen = !bScenarioListOpen;
							if (bScenarioListOpen)
							{
								// Read the folder now, so a file added since
								// the last look appears without restarting.
								RefreshScenarioList();
							}
							return FReply::Handled();
						}))
				]

				// Start the current scenario over. With none loaded, the robot
				// goes back to where it spawned.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					OperatorButton(INVTEXT("Restart"), INVTEXT("R"), ControlFill(), TextPrimary(),
						FOnClicked::CreateLambda([this]
						{
							if (ARoboPlayerController* Controller = OperatorController(Player))
							{
								Controller->RestartScenario();
							}
							return FReply::Handled();
						}))
				]
			]

			// The scenario files, one button each. Collapsed until opened.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 10.0f, 0.0f, 0.0f)
			[
				SAssignNew(ScenarioList, SVerticalBox)
				.Visibility_Lambda([this]
				{
					return bScenarioListOpen ? EVisibility::Visible : EVisibility::Collapsed;
				})
			]

			// Why the last load failed, such as malformed JSON in a file.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(BodyFont())
				.ColorAndOpacity(Stop())
				.AutoWrapText(true)
				.Text_Lambda([this] { return ScenarioError; })
				.Visibility_Lambda([this]
				{
					return ScenarioError.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
				})
			]
		];
}

void SRoboOperatorView::RefreshScenarioList()
{
	if (!ScenarioList.IsValid())
	{
		return;
	}

	ScenarioList->ClearChildren();

	// None first: the level as it was built, with no obstacles, goal or fog.
	ScenarioList->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		OperatorChoice(INVTEXT("None"),
			TAttribute<bool>::CreateLambda([this] { return !Telemetry.bHasScenario; }),
			FOnClicked::CreateLambda([this]
			{
				if (ARoboPlayerController* Controller = OperatorController(Player))
				{
					Controller->UnloadScenario();
				}
				ScenarioError = FText::GetEmpty();
				bScenarioListOpen = false;
				return FReply::Handled();
			}))
	];

	const TArray<FString> Files = RoboScenarioFiles::List();
	if (Files.IsEmpty())
	{
		ScenarioList->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("No .json files in %s"), *RoboScenarioFiles::Directory())))
			.Font(RoboOperatorStyle::BodyFont())
			.ColorAndOpacity(RoboOperatorStyle::TextMuted())
			.AutoWrapText(true)
		];
		return;
	}

	for (const FString& File : Files)
	{
		// Shown without ".json", and filled when it is the one running. A
		// scenario's own name usually matches its file name, which is what
		// the operator sees in the folder.
		const FString Stem = FPaths::GetBaseFilename(File);

		ScenarioList->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			OperatorChoice(FText::FromString(Stem),
				TAttribute<bool>::CreateLambda([this, Stem]
				{
					return Telemetry.bHasScenario && Telemetry.ScenarioName == Stem;
				}),
				FOnClicked::CreateLambda([this, File]
				{
					LoadScenarioFile(File);
					return FReply::Handled();
				}))
		];
	}
}

void SRoboOperatorView::LoadScenarioFile(const FString& FileName)
{
	ARoboPlayerController* Controller = OperatorController(Player);
	if (Controller == nullptr)
	{
		return;
	}

	FString Error;
	if (Controller->LoadScenarioFile(FileName, Error))
	{
		ScenarioError = FText::GetEmpty();
		bScenarioListOpen = false;
	}
	else
	{
		// Leave the list open, so another can be picked straight away.
		ScenarioError = FText::FromString(FString::Printf(TEXT("%s: %s"), *FileName, *Error));
	}
}

TSharedRef<SWidget> SRoboOperatorView::MakeLidarPanel()
{
	using namespace RoboOperatorStyle;

	// A small key, one coloured square and a word for each thing drawn.
	// The return type is spelled out: Slate's declarative syntax only becomes a
	// widget when converted to one, and "auto" would keep the unconverted form.
	auto KeyEntry = [](const FText& Label, const FSlateColor& Colour) -> TSharedRef<SWidget>
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				OperatorDot(Colour)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(LabelFont())
				.ColorAndOpacity(TextMuted())
			];
	};

	return SNew(SBorder)
		.BorderImage(PanelBrush())
		.OnMouseButtonDown_Static(&OperatorSwallowClick)
		.Padding(12.0f)
		// Only with a lidar to show.
		.Visibility_Lambda([this]
		{
			return Telemetry.bHasScan ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			// Fixed to the drawing's width, so the caveat wraps to fit it
			// instead of stretching the panel.
			SNew(SBox)
			.WidthOverride(LidarViewWidth)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(INVTEXT("What the robot sees"))
					.Font(TitleFont())
					.ColorAndOpacity(TextPrimary())
				]

				// The honest caveat, where it applies.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(INVTEXT("Lidar returns as the brain receives them. In fog, echoes look like obstacles."))
					.Font(LabelFont())
					.ColorAndOpacity(TextMuted())
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SRoboLidarView)
					.Telemetry(&Telemetry)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth() [ KeyEntry(INVTEXT("Returns"), LidarReturn()) ]
					+ SHorizontalBox::Slot().AutoWidth() [ KeyEntry(INVTEXT("Goal, as believed"), Normal()) ]
					+ SHorizontalBox::Slot().AutoWidth() [ KeyEntry(INVTEXT("Chosen heading"), External()) ]
				]
			]
		];
}
