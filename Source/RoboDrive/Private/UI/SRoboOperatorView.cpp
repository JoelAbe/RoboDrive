#include "SRoboOperatorView.h"

#include "GameFramework/PlayerController.h"
#include "RoboOperatorStyle.h"
#include "RoboPlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
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

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(Key)
					.Font(RoboOperatorStyle::LabelFont())
					.ColorAndOpacity(TextColour)
				]
			];
	}
}

void SRoboOperatorView::Construct(const FArguments& InArgs)
{
	Player = InArgs._Player;
	Telemetry = FRoboOperatorTelemetry::Gather(Player.Get());

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

		// Below it, the robot column on the left and the world everywhere else.
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

			// The world shows through here. Nothing is drawn, so nothing is hidden.
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
		]
	];
}

void SRoboOperatorView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	Telemetry = FRoboOperatorTelemetry::Gather(Player.Get());
}

TSharedRef<SWidget> SRoboOperatorView::MakeTopBar()
{
	using namespace RoboOperatorStyle;

	return SNew(SBorder)
		.BorderImage(PanelBrush())
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
								? FText::FromString(FString::Printf(TEXT("%.2f m/s"), Telemetry.CommandLinearMS))
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
								? FText::FromString(FString::Printf(TEXT("%.2f rad/s"), Telemetry.CommandAngularRadS))
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
								? FText::FromString(FString::Printf(TEXT("%.2f m/s"), Telemetry.ActualLinearMS))
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
								? FText::FromString(FString::Printf(TEXT("%.2f rad/s"), Telemetry.ActualYawRateRadS))
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
										? FText::FromString(FString::Printf(TEXT("%.1f deg"), Telemetry.HeadingErrorDeg))
										: OperatorNoValue();
								}),
								TextPrimary(), NumberFont())
						]
					]
				]
			]
		];
}
