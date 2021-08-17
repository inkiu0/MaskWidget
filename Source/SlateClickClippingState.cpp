#include "Layout/SlateClickClippingState.h"

FSlateClickClippingState::FSlateClickClippingState(const int32& Index, const FGeometry& Geometry, FOnClickClipClicked InOnClicked)
{
	ClipIndex = Index;
	DrawGeometry = Geometry;
	OnClicked = InOnClicked;
}

bool FSlateClickClippingState::IsPointInside(const FVector2D& Point) const
{
	FVector2D LocalPoint = DrawGeometry.AbsoluteToLocal(Point);
	FVector2D HitUVInMask = (LocalPoint - DrawGeometry.GetLocalPositionAtCoordinates(FVector2D(0.f, 0.f))) / DrawGeometry.GetLocalSize();
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
	FVector2D LocalPoint = DrawGeometry.AbsoluteToLocal(Point);
	FVector2D HitUVInMask = (LocalPoint - DrawGeometry.GetLocalPositionAtCoordinates(FVector2D(0.f, 0.f))) / DrawGeometry.GetLocalSize();
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
