#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"

/**
 * The operator view's look, in one place.
 *
 * A warm graphite base, so the panels sit quietly over the world. Colour is
 * kept for meaning: teal when things are running normally, amber for anything
 * worth watching, coral for stops and failures, violet for an external client
 * driving. Colours are written as sRGB hex, the way a designer would hand
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

	// Surfaces
	inline FLinearColor PanelFill()    { return Hex(TEXT("141518E6")); }
	inline FLinearColor PanelOutline() { return Hex(TEXT("2B2D31")); }
	inline FLinearColor Divider()      { return Hex(TEXT("25272B")); }

	// Text
	inline FSlateColor TextPrimary() { return Hex(TEXT("ECE8E1")); }
	inline FSlateColor TextMuted()   { return Hex(TEXT("8E8A82")); }
	inline FSlateColor TextFaint()   { return Hex(TEXT("5F5C56")); }

	/** Text on a filled button, dark so it reads on any of the meaning colours. */
	inline FSlateColor TextOnFill()  { return Hex(TEXT("141518")); }

	/** The fill of a button that is not selected. */
	inline FSlateColor ControlFill() { return Hex(TEXT("2B2D31")); }

	// Meaning
	inline FSlateColor Normal()    { return Hex(TEXT("3CC8B4")); }
	inline FSlateColor Attention() { return Hex(TEXT("E0A84A")); }
	inline FSlateColor Stop()      { return Hex(TEXT("E8644F")); }
	inline FSlateColor External()  { return Hex(TEXT("9C8CF2")); }

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
		static const FSlateRoundedBoxBrush Brush(Hex(TEXT("E0A84A0F")), 6.0f, Hex(TEXT("E0A84A55")), 1.0f);
		return &Brush;
	}

	/** A one-pixel rule between sections. */
	inline const FSlateBrush* DividerBrush()
	{
		static const FSlateColorBrush Brush(Divider());
		return &Brush;
	}

	/**
	 * Buttons are drawn white and tinted per button, so one style serves them
	 * all: a selected mode, an unselected one, and the stop. The brushes differ
	 * only in opacity, which is what makes hovering and pressing visible.
	 */
	inline const FButtonStyle& ButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f), 6.0f))
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor::White, 6.0f))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.65f), 6.0f))
			.SetNormalPadding(FMargin(0.0f))
			.SetPressedPadding(FMargin(0.0f));
		return Style;
	}

	/** A white circle, tinted per use to show a status. */
	inline const FSlateBrush* DotBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FLinearColor::White, 4.0f);
		return &Brush;
	}
}
