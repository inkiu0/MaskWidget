#include "Layout/SlateClickClippingState.h"

FSlateClickClippingState::FSlateClickClippingState(const int32& Index, const FVector2D& MaskPosRef, const FVector2D& MaskSizeRef, FOnClickClipClicked InOnClicked)
{
	ClipIndex = Index;
	MaskPos = MaskPosRef;
	MaskSize = MaskSizeRef;
	OnClicked = InOnClicked;
}

bool FSlateClickClippingState::IsPointInside(const FVector2D& Point) const
{
	FVector2D HitUVInMask = (Point - MaskPos) / MaskSize;
	if (HitUVInMask.X >= 0 && HitUVInMask.X <= 1 &&
		HitUVInMask.Y >= 0 && HitUVInMask.Y <= 1)
	{
		return true;
	}

	return false;
}

bool FSlateClickClippingState::IsClickThrough(const FVector2D& Point) const
{
	bool bThroughMask = false;
	FVector2D HitUVInMask = (Point - MaskPos) / MaskSize;
	if (HitUVInMask.X >= 0 && HitUVInMask.X <= 1 &&
		HitUVInMask.Y >= 0 && HitUVInMask.Y <= 1)
	{
		if (OnClicked.IsBound())
		{
			return OnClicked.Execute(HitUVInMask, ClipIndex);
		}
	}

	return bThroughMask;
}
