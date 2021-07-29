#include "MaskWidget.h"
#include "CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "Slate/SlateBrushAsset.h"

#define LOCTEXT_NAMESPACE "UMG"

UMaskWidget::UMaskWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, BgColorAndOpacity(FLinearColor::White)
{

}

TSharedRef<SWidget> UMaskWidget::RebuildWidget()
{
	MyMask = SNew(SMaskWidget).Style(&WidgetStyle);

	MyMask->ReIndexClip();

	return MyMask.ToSharedRef();
}

void UMaskWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	TAttribute<FSlateColor> ColorAndOpacityBinding = PROPERTY_BINDING(FSlateColor, BgColorAndOpacity);
	if(MyMask.IsValid())
	{
		MyMask->SetStyle(&WidgetStyle);
		MyMask->SetBgColorAndOpacity(BgColorAndOpacity);
	}
}

void UMaskWidget::SetBgColorAndOpacity(FLinearColor InColorAndOpacity)
{
	BgColorAndOpacity = InColorAndOpacity;
	if (MyMask.IsValid())
	{
		MyMask->SetBgColorAndOpacity(BgColorAndOpacity);
	}
}

void UMaskWidget::SetBgOpacity(float InOpacity)
{
	BgColorAndOpacity.A = InOpacity;
	if (MyMask.IsValid())
	{
		MyMask->SetBgColorAndOpacity(BgColorAndOpacity);
	}
}

void UMaskWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyMask.Reset();
}

void UMaskWidget::SetMaskPos(const int32& ClipIndex, const FVector2D& Pos)
{
	if (WidgetStyle.SetMaskPos(ClipIndex, Pos))
	{
		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
}

void UMaskWidget::SetMaskPosXY(const int32& ClipIndex, const float& X, const float& Y)
{
	if (WidgetStyle.SetMaskPosXY(ClipIndex, X, Y))
	{
		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
}

void UMaskWidget::SetMaskSize(const int32& ClipIndex, const FVector2D& Size)
{
	if (WidgetStyle.SetMaskSize(ClipIndex, Size))
	{
		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
}

void UMaskWidget::SetMaskSizeXY(const int32& ClipIndex, const float& X, const float& Y)
{
	if (WidgetStyle.SetMaskSizeXY(ClipIndex, X, Y))
	{
		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
}

void UMaskWidget::SetMaskPosSize(const int32& ClipIndex, const FVector4& PosSize)
{
	if (WidgetStyle.SetMaskPosSize(ClipIndex, PosSize))
	{
		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
}

void UMaskWidget::SetMaskPosSizeXYZW(const int32& ClipIndex, const float& X, const float& Y, const float& Z, const float& W)
{
	if (WidgetStyle.SetMaskPosSizeXYZW(ClipIndex, X, Y, Z, W))
	{
		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
}

void UMaskWidget::SetBrushTintColor(FSlateColor TintColor)
{
	if (WidgetStyle.BackgroundImage.TintColor != TintColor)
	{
		WidgetStyle.BackgroundImage.TintColor = TintColor;

		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
}

void UMaskWidget::SetBgImage(UTexture2D* Tex, bool bMatchSize)
{
	if (Tex)
	{
		WidgetStyle.BackgroundImage.SetResourceObject(Tex);

		if (bMatchSize)
		{
			WidgetStyle.BackgroundImage.ImageSize.X = Tex->GetSizeX();
			WidgetStyle.BackgroundImage.ImageSize.Y = Tex->GetSizeY();
		}

		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
}

void UMaskWidget::SetMaskImage(const int32& ClipIndex, UTexture2D* Tex)
{
	if (Tex && WidgetStyle.SetMaskTextureByIdx(ClipIndex, Tex))
	{
		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
}

void UMaskWidget::EnableMaskClickClip(const int32& ClipIndex, bool Enable)
{
	if (WidgetStyle.EnableMaskClickClip(ClipIndex, Enable))
	{
		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
}

int32 UMaskWidget::AddMaskClickClip(const FVector2D& Position, const FVector2D& Size, UTexture2D* Mask)
{
	int32 Index = WidgetStyle.AddMaskClickClip(Position, Size, Mask);
	if (Index > -1)
	{
		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
	return Index;
}

bool UMaskWidget::RemoveMaskClickClip(const int32& ClipIndex)
{
	bool Ret = WidgetStyle.RemoveMaskClickClip(ClipIndex);
	if (Ret)
	{
		if (MyMask.IsValid())
		{
			MyMask->SetStyle(&WidgetStyle);
		}
	}
	return Ret;
}

#if WITH_EDITOR

const FText UMaskWidget::GetPaletteCategory()
{
	return LOCTEXT("Mask Widget", "Mask Widget");
}

#endif

#undef LOCTEXT_NAMESPACE
