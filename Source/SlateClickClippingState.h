#pragma once

#include "CoreMinimal.h"

DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnClickClipClicked,
const FVector2D&,
const int32&)

class SLATECORE_API FSlateClickClippingState
{
public:
	FSlateClickClippingState(const int32& Index, const FVector2D& MaskPosRef, const FVector2D& MaskSizeRef, FOnClickClipClicked InOnClicked);

	bool IsPointInside(const FVector2D& Point) const;

	bool IsClickThrough(const FVector2D& Point) const;

private:

	int32 ClipIndex = -1;

	FVector2D MaskSize;

	FVector2D MaskPos;

	FOnClickClipClicked OnClicked;
};
