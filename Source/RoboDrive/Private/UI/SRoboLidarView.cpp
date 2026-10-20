#include "SRoboLidarView.h"

#include "RoboLidarViewMath.h"
#include "RoboOperatorStyle.h"
#include "RoboOperatorTelemetry.h"
#include "Rendering/DrawElements.h"

namespace
{
	/** Pixels kept free below the robot and above the view's far edge. */
	constexpr double LidarViewBottomMarginPx = 28.0;
	constexpr double LidarViewTopMarginPx = 8.0;

	/** The chassis footprint, m: 0.6 long and 0.4 wide, from the vehicle's collision box. */
	const FVector2D LidarViewRobotSizeM(0.6, 0.4);
}

void SRoboLidarView::Construct(const FArguments& InArgs)
{
	Telemetry = InArgs._Telemetry;

	// Anything that would be drawn past the edges, such as the far side of a
	// range ring, is cut off rather than spilling over the panel.
	SetClipping(EWidgetClipping::ClipToBounds);
}

FVector2D SRoboLidarView::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(RoboOperatorStyle::LidarViewWidth, RoboOperatorStyle::LidarViewHeight);
}

int32 SRoboLidarView::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	if (Telemetry == nullptr)
	{
		return LayerId;
	}

	const FRoboOperatorTelemetry& View = *Telemetry;
	const FVector2D Size(AllottedGeometry.GetLocalSize());

	// The robot sits near the bottom, facing up: almost everything that
	// matters to it is ahead.
	const FVector2D RobotPx(0.5 * Size.X, Size.Y - LidarViewBottomMarginPx);
	const double PixelsPerMetre = (RobotPx.Y - LidarViewTopMarginPx) / RoboOperatorStyle::LidarViewRangeM;

	auto ToView = [&](const FVector2D& PointM)
	{
		return FVector2f(RoboLidarViewMath::RobotToView(PointM, RobotPx, PixelsPerMetre));
	};

	// Shapes are drawn in layers, back to front: guides, then what was
	// decided, then what was seen, then the robot on top.
	const int32 BackgroundLayer = LayerId;
	const int32 GuideLayer = LayerId + 1;
	const int32 DecisionLayer = LayerId + 2;
	const int32 ReturnLayer = LayerId + 3;
	const int32 RobotLayer = LayerId + 4;

	// A solid background: through a translucent one, the paths and rays drawn
	// in the world behind the panel show through and look like returns.
	FSlateDrawElement::MakeBox(OutDrawElements, BackgroundLayer, AllottedGeometry.ToPaintGeometry(),
		RoboOperatorStyle::SolidBrush(), ESlateDrawEffect::None, RoboOperatorStyle::ViewFill());

	auto DrawLines = [&](const TArray<FVector2f>& Points, const FLinearColor& Colour, float Thickness, int32 Layer)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(),
			Points, ESlateDrawEffect::None, Colour, true, Thickness);
	};

	auto DrawSquare = [&](const FVector2f& CentrePx, const FVector2f& SizePx, const FLinearColor& Colour, int32 Layer)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(SizePx, FSlateLayoutTransform(1.0f, CentrePx - SizePx * 0.5f)),
			RoboOperatorStyle::SolidBrush(), ESlateDrawEffect::None, Colour);
	};

	// Range rings, one per metre, for judging distance at a glance.
	const FLinearColor RingColour = RoboOperatorStyle::PanelOutline();
	for (int32 RingM = 1; RingM <= FMath::FloorToInt(RoboOperatorStyle::LidarViewRangeM); ++RingM)
	{
		TArray<FVector2f> Ring;
		constexpr int32 Segments = 64;
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			const double AngleRad = UE_DOUBLE_TWO_PI * Index / Segments;
			Ring.Add(ToView(RoboLidarViewMath::AlongHeadingM(AngleRad, RingM)));
		}
		DrawLines(Ring, RingColour, 1.0f, GuideLayer);
	}

	// The edges of the lidar's field of view. Behind them is its blind spot,
	// where nothing can be seen and so nothing is ever chosen.
	if (View.bHasScan)
	{
		const FLinearColor EdgeColour = RoboOperatorStyle::TextFaint().GetSpecifiedColor();
		const double ReachM = 2.0 * RoboOperatorStyle::LidarViewRangeM;

		for (const double EdgeRad : { View.ScanAngleMinRad, View.ScanAngleMaxRad })
		{
			DrawLines({ ToView(FVector2D::ZeroVector), ToView(RoboLidarViewMath::AlongHeadingM(EdgeRad, ReachM)) },
				EdgeColour, 1.0f, GuideLayer);
		}
	}

	// Where the robot believes the goal is.
	if (View.bHasGoalBearing)
	{
		const FLinearColor GoalColour = RoboOperatorStyle::Normal().GetSpecifiedColor().CopyWithNewOpacity(0.7f);
		DrawLines({ ToView(FVector2D::ZeroVector),
			ToView(RoboLidarViewMath::AlongHeadingM(View.GoalBearingRad, 2.0 * RoboOperatorStyle::LidarViewRangeM)) },
			GoalColour, 1.5f, DecisionLayer);
	}

	// The corridor the brain checked along its chosen heading: nothing it saw
	// is inside it, which is why that heading was chosen.
	if (View.ChosenHeadingRad.IsSet() && View.CorridorLengthM > 0.0)
	{
		const TArray<FVector2D> CornersM = RoboLidarViewMath::CorridorCornersM(
			View.ChosenHeadingRad.GetValue(), View.CorridorLengthM, View.CorridorHalfWidthM);

		TArray<FVector2f> Outline;
		for (const FVector2D& CornerM : CornersM)
		{
			Outline.Add(ToView(CornerM));
		}
		// Close the outline by repeating the first corner. Copied out first:
		// Outline[0] is a reference into the array, and Add may move the
		// array to make room, leaving that reference pointing at freed memory.
		const FVector2f FirstCorner = Outline[0];
		Outline.Add(FirstCorner);

		DrawLines(Outline, RoboOperatorStyle::External().GetSpecifiedColor().CopyWithNewOpacity(0.6f), 1.0f, DecisionLayer);
	}

	// What the lidar returned, exactly as the brain received it.
	const FLinearColor ReturnColour = RoboOperatorStyle::LidarReturn().GetSpecifiedColor();
	for (const FVector2D& PointM : View.ScanPointsM)
	{
		DrawSquare(ToView(PointM), FVector2f(3.0f, 3.0f), ReturnColour, ReturnLayer);
	}

	// The chosen heading, as an arrow the length of the corridor, matching
	// the purple arrow drawn over the robot in the world.
	if (View.ChosenHeadingRad.IsSet())
	{
		const double HeadingRad = View.ChosenHeadingRad.GetValue();
		const double LengthM = View.CorridorLengthM > 0.0 ? View.CorridorLengthM : 1.0;
		const FVector2D TipM = RoboLidarViewMath::AlongHeadingM(HeadingRad, LengthM);
		const FLinearColor ArrowColour = RoboOperatorStyle::External().GetSpecifiedColor();

		DrawLines({ ToView(FVector2D::ZeroVector), ToView(TipM) }, ArrowColour, 2.0f, ReturnLayer);

		// Two short strokes back from the tip, 150 degrees either side of the heading.
		constexpr double BarbM = 0.25;
		const double BarbRad = FMath::DegreesToRadians(150.0);
		DrawLines({ ToView(TipM + RoboLidarViewMath::AlongHeadingM(HeadingRad + BarbRad, BarbM)), ToView(TipM),
			ToView(TipM + RoboLidarViewMath::AlongHeadingM(HeadingRad - BarbRad, BarbM)) },
			ArrowColour, 2.0f, ReturnLayer);
	}

	// The robot itself, to scale: forward is up the screen, so its length is
	// the rectangle's height.
	DrawSquare(FVector2f(RobotPx),
		FVector2f(LidarViewRobotSizeM.Y * PixelsPerMetre, LidarViewRobotSizeM.X * PixelsPerMetre),
		RoboOperatorStyle::TextPrimary().GetSpecifiedColor(), RobotLayer);

	return RobotLayer;
}
