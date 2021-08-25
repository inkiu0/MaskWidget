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
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateWidgetStyle.h"
#include "MaskSlateStyle.generated.h"

static const uint8 MAX_MASK_CLIP_COUNT = 3;

USTRUCT(BlueprintType)
struct MMOGAME_API FMaskClip
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaskClip)
	UTexture2D* MaskTex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaskClip)
	FVector2D MaskPosition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaskClip)
	FVector2D MaskSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MaskClip)
	bool ClipEnable = false;

private:

	int32 ClipIndex;

public:

	/**
	 * Default constructor.
	 */
	FMaskClip()
		: MaskTex(nullptr)
		, MaskPosition(0.f, 0.f)
		, MaskSize(32.f, 32.f)
		, ClipEnable(false)
		, ClipIndex(-1)
	{ }

	FMaskClip(const int32& Index, const FVector2D& Pos, const FVector2D& Size, UTexture2D* Mask)
	{
		ClipIndex = Index;
		MaskPosition = Pos;
		MaskSize = Size;
		MaskTex = Mask;
	}

	virtual ~FMaskClip() {}

	void SetMaskTexture(UTexture2D* const Texture) { MaskTex = Texture; }

	void SetSize(const FVector2D& Size) { MaskSize = Size; }

	void SetPosition(const FVector2D& Pos) { MaskPosition = Pos; }

	void SetEnable(bool Enable) { ClipEnable = Enable; }

	void SetIndex(const int32& Index) { ClipIndex = Index; }

	UTexture2D* GetMaskTexture() const { return MaskTex; }

	FVector2D GetSize() const { return MaskSize; }

	FVector2D GetPos() const { return MaskPosition; }

	bool IsEnable() const { return ClipEnable; }

	int32 GetClipIndex() const { return ClipIndex; }
};

/**
 * Represents the appearance of an SMaskWidget
 */
USTRUCT(BlueprintType)
struct MMOGAME_API FMaskWidgetStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

public:

	FMaskWidgetStyle();

	virtual ~FMaskWidgetStyle() {}

	virtual void GetResources(TArray< const FSlateBrush* >& OutBrushes) const override;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	static const FMaskWidgetStyle& GetDefault();

	/** Background image to use for the mask widget */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush BackgroundImage;
	FMaskWidgetStyle& SetBackgroundImage(const FSlateBrush& InBackgroundImage) { BackgroundImage = InBackgroundImage; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	TArray<FMaskClip> MaskClips;

	void ReIndexClip();

	const UTexture2D* GetMaskTextureByIdx(const int32& Index) const;

	bool SetMaskTextureByIdx(const int32& Index, UTexture2D* Texture);

	bool SetMaskSize(const int32& Index, const FVector2D& Size);

	bool SetMaskSizeXY(const int32& Index, const float& X, const float& Y);

	bool SetMaskPos(const int32& Index, const FVector2D& Pos);

	bool SetMaskPosXY(const int32& Index, const float& X, const float& Y);

	bool SetMaskPosSize(const int32& Index, const FVector4& PosSize);

	bool SetMaskPosSizeXYZW(const int32& Index, const float& X, const float& Y, const float& Z, const float& W);

	bool EnableMaskClickClip(const int32& Index, bool Enable);

	int32 AddMaskClickClip(const FVector2D& Position, const FVector2D& Size, UTexture2D* Mask);

	bool RemoveMaskClickClip(const int32& ClipIndex);

	UPROPERTY()
	FSlateBrush MaskMatBrush;

private:

	UPROPERTY()
	UMaterialInstanceDynamic* MaskDynamicMaterial;

public:

	/**
	* Unlinks all colors in this style.
	* @see FSlateColor::Unlink
	*/
	void UnlinkColors()
	{
		BackgroundImage.UnlinkColors();
	}
};
