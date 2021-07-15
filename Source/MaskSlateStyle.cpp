#include "MaskSlateStyle.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

FMaskWidgetStyle::FMaskWidgetStyle()
{
	if (UMaterial* Mat = LoadObject<UMaterial>(NULL, TEXT("/Game/Assets/UI/Material/Slate/MaskMaterial.MaskMaterial")))
	{
		if (UMaterialInstanceDynamic* DyMat = UMaterialInstanceDynamic::Create(Mat, GetTransientPackage()))
		{
			MaskMatBrush.SetResourceObject(DyMat);
		}
		else
		{
			MaskMatBrush.SetResourceObject(Mat);
		}
	}
}

void FMaskWidgetStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	OutBrushes.Add(&BackgroundImage);
}

const FName FMaskWidgetStyle::TypeName(TEXT("FMaskWidgetStyle"));

const FMaskWidgetStyle& FMaskWidgetStyle::GetDefault()
{
	static FMaskWidgetStyle Default;
	return Default;
}

void FMaskWidgetStyle::ReIndexClip()
{
	for (uint8 i = 0; i < MaskClips.Num(); i++)
	{
		MaskClips[i].SetIndex(i);
	}
}

const UTexture2D* FMaskWidgetStyle::GetMaskTextureByIdx(const int32& Index) const
{
	if (MaskClips.Num() > Index)
	{
		return MaskClips[Index].GetMaskTexture();
	}
	return nullptr;
}

bool FMaskWidgetStyle::SetMaskTextureByIdx(const int32& Index, UTexture2D* Texture)
{
	if (MaskClips.Num() > Index)
	{
		MaskClips[Index].SetMaskTexture(Texture);
		return true;
	}
	return false;
}

bool FMaskWidgetStyle::SetMaskSize(const int32& Index, const FVector2D& Size)
{
	if (MaskClips.Num() > Index)
	{
		MaskClips[Index].SetSize(Size);
		return true;
	}
	return false;
}

bool FMaskWidgetStyle::SetMaskSizeXY(const int32& Index, const float& X, const float& Y)
{
	if (MaskClips.Num() > Index)
	{
		return true;
	}
	return false;
}

bool FMaskWidgetStyle::SetMaskPos(const int32& Index, const FVector2D& Pos)
{
	if (MaskClips.Num() > Index)
	{
		MaskClips[Index].SetPosition(Pos);
		return true;
	}
	return false;
}

bool FMaskWidgetStyle::SetMaskPosXY(const int32& Index, const float& X, const float& Y)
{
	if (MaskClips.Num() > Index)
	{
		return true;
	}
	return false;
}

bool FMaskWidgetStyle::SetMaskPosSize(const int32& Index, const FVector4& PosSize)
{
	if (MaskClips.Num() > Index)
	{
		return true;
	}
	return false;
}

bool FMaskWidgetStyle::SetMaskPosSizeXYZW(const int32& Index, const float& X, const float& Y, const float& Z, const float& W)
{
	if (MaskClips.Num() > Index)
	{
		return true;
	}
	return false;
}

bool FMaskWidgetStyle::EnableMaskClickClip(const int32& Index, bool Enable)
{
	if (MaskClips.Num() > Index)
	{
		MaskClips[Index].SetEnable(Enable);
		return true;
	}
	return false;
}

int32 FMaskWidgetStyle::AddMaskClickClip(const FVector2D& Position, const FVector2D& Size, UTexture2D* Mask)
{
	int32 Count = MaskClips.Num();
	if (Count < MAX_MASK_CLIP_COUNT)
	{
		MaskClips.Add(FMaskClip(Count, Position, Size, Mask));
		return Count;
	}
	return -1;
}

bool FMaskWidgetStyle::RemoveMaskClickClip(const int32& ClipIndex)
{
	if (ClipIndex >= 0 && ClipIndex < MaskClips.Num())
	{
		MaskClips.RemoveAt(ClipIndex);
		return true;
	}
	else
	{
		UE_LOG(LogInit, Error, TEXT("ERROR: FMaskWidgetStyle::RemoveMaskClickClip invalid ClipIndex = %d, MaskClips.Num() = %d"), ClipIndex, MaskClips.Num());
	}
	return false;
}
