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
#include "SMaskWidget.h"
#include "MaskSlateStyle.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "MaskWidget.generated.h"

class USlateBrushAsset;

UCLASS()
class MMOGAME_API UMaskWidget : public UWidget
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style", meta = (DisplayName = "Style"))
	FMaskWidgetStyle WidgetStyle;

	/** 背景颜色和透明属性 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance, meta = (sRGB = "true"))
	FLinearColor BgColorAndOpacity;

	/** BgColorAndOpacity 可绑定的委托 */
	UPROPERTY()
	FGetLinearColor BgColorAndOpacityDelegate;

public:

	/**  设置背景的颜色和透明度*/
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetBgColorAndOpacity(FLinearColor InColorAndOpacity);

	/**  设置背景的透明度*/
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetBgOpacity(float InOpacity);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetBgImage(UTexture2D* Tex, bool bMatchSize = false);

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetBrushTintColor(FSlateColor TintColor);

	UFUNCTION(BlueprintCallable, Category = "MaskClip")
	void SetMaskImage(const int32& ClipIndex, UTexture2D* Tex);

	UFUNCTION(BlueprintCallable, Category = "MaskClip")
	void SetMaskPos(const int32& ClipIndex, const FVector2D& Pos);

	UFUNCTION(BlueprintCallable, Category = "MaskClip")
	void SetMaskPosXY(const int32& ClipIndex, const float& X, const float& Y);

	UFUNCTION(BlueprintCallable, Category = "MaskClip")
	void SetMaskSize(const int32& ClipIndex, const FVector2D& Size);

	UFUNCTION(BlueprintCallable, Category = "MaskClip")
	void SetMaskSizeXY(const int32& ClipIndex, const float& X, const float& Y);

	UFUNCTION(BlueprintCallable, Category = "MaskClip")
	void SetMaskPosSize(const int32& ClipIndex, const FVector4& PosSize);

	UFUNCTION(BlueprintCallable, Category = "MaskClip")
	void SetMaskPosSizeXYZW(const int32& ClipIndex, const float& X, const float& Y, const float& Z, const float& W);

	UFUNCTION(BlueprintCallable, Category = "MaskClip")
	void EnableMaskClickClip(const int32& ClipIndex, bool Enable);

	UFUNCTION(BlueprintCallable, Category = "MaskClip")
	int32 AddMaskClickClip(const FVector2D& Position, const FVector2D& Size, UTexture2D* Mask = nullptr);

	UFUNCTION(BlueprintCallable, Category = "MaskClip")
	bool RemoveMaskClickClip(const int32& ClipIndex);

public:

	virtual void SynchronizeProperties() override;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:

	TSharedPtr<SMaskWidget> MyMask;

	virtual TSharedRef<SWidget> RebuildWidget() override;

	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, BgColorAndOpacity);
};
