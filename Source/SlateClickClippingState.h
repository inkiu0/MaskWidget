#pragma once

#include "CoreMinimal.h"
#include "Geometry.h"

DECLARE_DELEGATE_RetVal_TwoParams(bool, FOnClickClipClicked,
const FVector2D&,
const int32&)

class SLATECORE_API FSlateClickClippingState
{
public:
	FSlateClickClippingState(const int32& Index, const FGeometry& Geometry, FOnClickClipClicked InOnClicked);

	bool IsPointInside(const FVector2D& Point) const;

	bool IsClickThrough(const FVector2D& Point) const;

private:

	int32 ClipIndex = -1;

	FGeometry DrawGeometry;

	FOnClickClipClicked OnClicked;
};
