#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"

/**
 * The operator view's look, in one place.
 *
 * A deep navy base, so the panels sit quietly over the world. Colour is kept
 * for meaning: green when things are running normally, amber for anything
 * worth watching, red for stops and failures, violet for an external client
 * driving, and blue for actions you can take. Colours are written as sRGB hex, the way a designer would hand
 * them over, and converted to the linear colour Slate works in.
 */
namespace RoboOperatorStyle
{
	inline FLinearColor Hex(const TCHAR* Code)
	{
		return FLinearColor(FColor::FromHex(Code));
	}

	// Layout. Slate units are scaled with the screen's DPI, so these keep the
	// same proportion of the screen at any resolution: on a 16:9 display, the
	// two side columns together leave about 70% of the width to the world.
	// In the editor the play viewport is smaller than the screen, so the
	// panels are drawn smaller there too; full screen is the true size.
	inline constexpr float ScreenMargin = 16.0f;
	inline constexpr float ColumnWidth = 300.0f;
	inline constexpr float PanelPadding = 18.0f;

	// The lidar view: its drawing area, and how much of the world it shows.
	// It sits under the robot panel, so it is exactly as wide as the column
	// inside its 12-unit padding. Most of what matters to avoidance is within
	// a few metres, so it shows 5 m ahead rather than the lidar's full 12 m.
	inline constexpr float LidarViewWidth = ColumnWidth - 2.0f * 12.0f;
	inline constexpr float LidarViewHeight = 200.0f;
	inline constexpr double LidarViewRangeM = 5.0;

	// Surfaces
	inline FLinearColor PanelFill()    { return Hex(TEXT("131A26E8")); }
	inline FLinearColor PanelOutline() { return Hex(TEXT("27324A")); }
	inline FLinearColor Divider()      { return Hex(TEXT("212B3D")); }

	/** Behind the lidar view's drawing: darker than the panels, and solid. */
	inline FLinearColor ViewFill()     { return Hex(TEXT("0B111C")); }

	// Text
	inline FSlateColor TextPrimary() { return Hex(TEXT("E6EDF3")); }
	inline FSlateColor TextMuted()   { return Hex(TEXT("8B95A7")); }
	inline FSlateColor TextFaint()   { return Hex(TEXT("566178")); }

	/** Text on a filled button, dark so it reads on any of the meaning colours. */
	inline FSlateColor TextOnFill()  { return Hex(TEXT("0B111C")); }

	/** The fill of a button that is not selected. */
	inline FSlateColor ControlFill() { return Hex(TEXT("243149")); }

	// Meaning
	inline FSlateColor Normal()    { return Hex(TEXT("2FCB6E")); }
	inline FSlateColor Attention() { return Hex(TEXT("F5A524")); }
	inline FSlateColor Stop()      { return Hex(TEXT("EF4444")); }
	inline FSlateColor External()  { return Hex(TEXT("A78BFA")); }

	/** Actions: things that do something once, such as restarting, rather than a state. */
	inline FSlateColor Accent()    { return Hex(TEXT("3B82F6")); }

	/** Lidar returns, close to the orange of the rays drawn in the world. */
	inline FSlateColor LidarReturn() { return Hex(TEXT("F0883E")); }

	// Fonts. The engine's default family, so nothing extra has to be shipped.
	// Numbers are monospaced so they do not jitter sideways as they change.
	inline FSlateFontInfo LabelFont()  { return FCoreStyle::GetDefaultFontStyle("Regular", 10); }
	inline FSlateFontInfo BodyFont()   { return FCoreStyle::GetDefaultFontStyle("Regular", 12); }
	inline FSlateFontInfo ValueFont()  { return FCoreStyle::GetDefaultFontStyle("Regular", 15); }
	inline FSlateFontInfo NumberFont() { return FCoreStyle::GetDefaultFontStyle("Mono", 13); }
	inline FSlateFontInfo TitleFont()  { return FCoreStyle::GetDefaultFontStyle("Bold", 13); }

	// Brushes. Widgets keep a pointer to the brush they draw with, so every
	// brush must outlive every widget. A function-local static lives for the
	// whole program, and is shared by every file that includes this header.

	/** The translucent rounded panel everything sits on. */
	inline const FSlateBrush* PanelBrush()
	{
		static const FSlateRoundedBoxBrush Brush(PanelFill(), 8.0f, PanelOutline(), 1.0f);
		return &Brush;
	}

	/** The outlined box that sets ground truth apart. */
	inline const FSlateBrush* TruthBrush()
	{
		static const FSlateRoundedBoxBrush Brush(Hex(TEXT("F5A52412")), 6.0f, Hex(TEXT("F5A52466")), 1.0f);
		return &Brush;
	}

	/** A one-pixel rule between sections. */
	inline const FSlateBrush* DividerBrush()
	{
		static const FSlateColorBrush Brush(Divider());
		return &Brush;
	}

	/**
	 * A button that draws nothing itself. The operator view's buttons draw
	 * their own shape inside, so its colour can follow both what the button
	 * means and whether the mouse is over it.
	 */
	inline const FButtonStyle& BareButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateNoResource())
			.SetHovered(FSlateNoResource())
			.SetPressed(FSlateNoResource())
			.SetNormalPadding(FMargin(0.0f))
			.SetPressedPadding(FMargin(0.0f));
		return Style;
	}

	/** A white rounded shape, tinted per use: a button's outline and fill. */
	inline const FSlateBrush* ButtonShapeBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 6.0f);
		return &Brush;
	}

	/** An outlined action button's fill: the panel's navy, but solid. */
	inline FSlateColor ActionFill() { return Hex(TEXT("15203A")); }

	/**
	 * Sliders, drawn white and tinted per slider like the buttons: the bar in
	 * the control colour, the handle in the meaning colour.
	 */
	inline const FSliderStyle& SliderStyle()
	{
		static const FSlateRoundedBoxBrush Bar(FLinearColor::White, 2.0f);
		static const FSlateRoundedBoxBrush Thumb(FLinearColor::White, 6.0f, FVector2f(12.0f, 12.0f));
		static const FSliderStyle Style = FSliderStyle()
			.SetNormalBarImage(Bar)
			.SetHoveredBarImage(Bar)
			.SetDisabledBarImage(Bar)
			.SetNormalThumbImage(Thumb)
			.SetHoveredThumbImage(Thumb)
			.SetDisabledThumbImage(Thumb)
			.SetBarThickness(4.0f);
		return Style;
	}

	/** A plain white square, tinted per use, for drawing shapes by hand. */
	inline const FSlateBrush* SolidBrush()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return &Brush;
	}

	/** A white circle, tinted per use to show a status. */
	inline const FSlateBrush* DotBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 4.0f);
		return &Brush;
	}
}
