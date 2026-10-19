#include "SRoboOperatorView.h"

#include "GameFramework/PlayerController.h"
#include "RoboOperatorStyle.h"
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

	/** A one-pixel rule between sections, with space around it. */
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
}

void SRoboOperatorView::Construct(const FArguments& InArgs)
{
	Player = InArgs._Player;
	Telemetry = FRoboOperatorTelemetry::Gather(Player.Get());

	const float Margin = RoboOperatorStyle::ScreenMargin;

	ChildSlot
	[
		SNew(SVerticalBox)

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

			// Whether there is a simulation running at all.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				OperatorDot(TAttribute<FSlateColor>::CreateLambda([this]
				{
					return Telemetry.bHasSim ? Normal() : TextMuted();
				}))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(TitleFont())
				.ColorAndOpacity(TextPrimary())
				.Text_Lambda([this]
				{
					return Telemetry.bHasSim ? INVTEXT("Simulating") : INVTEXT("No simulation");
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
						return Telemetry.bHasRobot ? UEnum::GetDisplayValueAsText(Telemetry.DrivingSource) : OperatorNoValue();
					}),
					TAttribute<FSlateColor>::CreateLambda([this]
					{
						return OperatorSourceColour(Telemetry.DrivingSource);
					}),
					ValueFont())
			]

			// What was asked for, next to what the robot is actually doing.
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
					OperatorField(INVTEXT("Actual speed"),
						TAttribute<FText>::CreateLambda([this]
						{
							return Telemetry.bHasRobot
								? FText::FromString(FString::Printf(TEXT("%.2f m/s"), Telemetry.ActualSpeedMS))
								: OperatorNoValue();
						}),
						TextPrimary(), NumberFont())
				]

				+ SUniformGridPanel::Slot(1, 1)
				[
					OperatorField(INVTEXT("Brain"),
						TAttribute<FText>::CreateLambda([this]
						{
							return Telemetry.bHasRobot ? FText::FromString(Telemetry.BrainName) : OperatorNoValue();
						}),
						TextPrimary(), BodyFont())
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 12.0f)
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
