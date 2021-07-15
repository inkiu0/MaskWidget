// MIT License

// Copyright (c) 2021 HankShu inkiu0@gmail.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "CoreMinimal.h"
#include "MaskSlateStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Materials/MaterialInterface.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Materials/MaterialInstanceDynamic.h"

class FPaintArgs;
class FSlateWindowElementList;

DECLARE_DELEGATE_RetVal_TwoParams(FReply, FMaskOnClicked,
const int32&,
const bool&)

UENUM()
enum class EEaseMode : uint8
{
	LinearIn,
	QuadEaseIn,
	QuadEaseOut,
	CubicEaseIn,
	CubicEaseOut
};

class MMOGAME_API SMaskWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMaskWidget)
		: _Style(&FCoreStyle::Get().GetWidgetStyle<FMaskWidgetStyle>("MaskWidget"))
		, _BgColorAndOpacity(FLinearColor::White)
		{
			_Visibility = EVisibility::Visible;
		}

		SLATE_STYLE_ARGUMENT(FMaskWidgetStyle, Style)

		/** 颜色和透明度 */
		SLATE_ATTRIBUTE(FSlateColor, BgColorAndOpacity)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual bool IsInteractable() const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent) override;

	/** 设置背景的颜色和透明度 */
	void SetBgColorAndOpacity(const TAttribute<FSlateColor>& InColorAndOpacity);

	/** 设置背景的颜色和透明度 */
	void SetBgColorAndOpacity(FLinearColor InColorAndOpacity);

	/** See attribute Style */
	void SetStyle(const FMaskWidgetStyle* InStyle);

	/** Set ClickClip's MaskPosition */
	void SetMaskPosition(const int32& ClipIndex, TAttribute<FVector2D> InMaskPosition);

	/** See attribute BackgroundImage */
	void SetBackgroundImage(const FSlateBrush* InBackgroundImage);

	void ReIndexClip();

private:

	const FSlateBrush* GetBackgroundImage() const;

	const UTexture2D* GetMaskTextureByIndex(const int32& Index) const;

	const FSlateBrush* GetMaskMatBrush() const;

	bool OnClickClipClicked(const FVector2D& Point, const int32& ClipIndex);

public:

	bool IsMaskUpdated = true;

protected:

	FMaskOnClicked OnClicked;

	/** 背景图片的颜色和透明度比例 */
	TAttribute<FSlateColor> BgColorAndOpacity;

	const FSlateBrush* BackgroundImage;

private:

	FMaskWidgetStyle* Style;
};
