#include "SMaskWidget.h"
#include "HittestGrid.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Layout/SlateClickClippingState.h"

void SMaskWidget::Construct(const FArguments& InArgs)
{
	check(InArgs._Style);
	BgColorAndOpacity = InArgs._BgColorAndOpacity;
	Style = const_cast<FMaskWidgetStyle*>(InArgs._Style);

	SetCanTick(false);
}

void SMaskWidget::SetBgColorAndOpacity(const TAttribute<FSlateColor>& InColorAndOpacity)
{
	SetAttribute(BgColorAndOpacity, InColorAndOpacity, EInvalidateWidgetReason::Paint);
}

void SMaskWidget::SetBgColorAndOpacity(FLinearColor InColorAndOpacity)
{
	SetBgColorAndOpacity(TAttribute<FSlateColor>(InColorAndOpacity));
}

void SMaskWidget::SetStyle(const FMaskWidgetStyle* InStyle)
{
	if (InStyle == nullptr)
	{
		FArguments Defaults;
		Style = const_cast<FMaskWidgetStyle*>(Defaults._Style);
	}
	else
	{
		Style = const_cast<FMaskWidgetStyle*>(InStyle);
	}

	check(Style);

	Invalidate(EInvalidateWidget::Layout);
	
	IsMaskUpdated = true;
}

void SMaskWidget::SetMaskPosition(const int32& ClipIndex, TAttribute<FVector2D> InMaskPosition)
{
	if (Style->SetMaskPos(ClipIndex, InMaskPosition.Get()))
	{
		IsMaskUpdated = true;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SMaskWidget::SetBackgroundImage(const FSlateBrush* InBackgroundImage)
{
	if (BackgroundImage != InBackgroundImage)
	{
		IsMaskUpdated = true;
		BackgroundImage = InBackgroundImage;
		Invalidate(EInvalidateWidget::Layout);
	}
}

const FSlateBrush* SMaskWidget::GetBackgroundImage() const
{
	return BackgroundImage ? BackgroundImage : &Style->BackgroundImage;
}

const UTexture2D* SMaskWidget::GetMaskTextureByIndex(const int32& Index) const
{
	return Style->GetMaskTextureByIdx(Index);
}

const FSlateBrush* SMaskWidget::GetMaskMatBrush() const
{
	return &Style->MaskMatBrush;
}

void SMaskWidget::ReIndexClip()
{
	Style->ReIndexClip();
}

bool SMaskWidget::IsInteractable() const
{
	return IsEnabled();
}

int32 SMaskWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 RetLayerId = LayerId;
	SMaskWidget* MutableThis = const_cast<SMaskWidget*>(this);

	const FSlateBrush* CurBgImage = GetBackgroundImage();
	const FSlateBrush* MatBrush = GetMaskMatBrush();

#if !WITH_EDITOR
	if (MutableThis->IsMaskUpdated)
	{
#endif
		if (UMaterialInstanceDynamic* DyMat = Cast<UMaterialInstanceDynamic>(MatBrush->GetResourceObject()))
		{
			FVector2D GSize = AllottedGeometry.GetLocalSize();
			const TArray<FMaskClip> Clips = Style->MaskClips;
			for (uint8 i = 0; i < MAX_MASK_CLIP_COUNT; i++)
			{
				if (i < Clips.Num())
				{
					FMaskClip Clip = Clips[i];
					FVector2D Size = Clip.GetSize();
					FVector2D Pos = Clip.GetPos();
					DyMat->SetVectorParameterValue(*FString::Printf(TEXT("MaskUV_%d"), i), FLinearColor(Pos.X / GSize.X, Pos.Y / GSize.Y, Size.X / GSize.X, Size.Y / GSize.Y));
					if (UTexture2D* Tex = Clip.GetMaskTexture())
					{
						DyMat->SetTextureParameterValue(*FString::Printf(TEXT("MaskTex_%d"), i), Tex);
					}
				}
				else
				{
					DyMat->SetVectorParameterValue(*FString::Printf(TEXT("MaskUV_%d"), i), FLinearColor(0.f, 0.f, 0.f, 0.f));
				}
			}
			DyMat->SetTextureParameterValue("BgTex", Cast<UTexture>(CurBgImage->GetResourceObject()));
		}

#if !WITH_EDITOR
		MutableThis->IsMaskUpdated = false;
	}
#endif

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		RetLayerId++,
		AllottedGeometry.ToPaintGeometry(),
		MatBrush,
		ESlateDrawEffect::None,
		InWidgetStyle.GetColorAndOpacityTint() *
		BgColorAndOpacity.Get().GetColor(InWidgetStyle) * CurBgImage->GetTint(InWidgetStyle)
	);

	FSlateLayoutTransform TranLayout(AllottedGeometry.Scale, AllottedGeometry.AbsolutePosition);
	const TArray<FMaskClip> Clips = Style->MaskClips;
	for (uint8 i = 0; i < MAX_MASK_CLIP_COUNT && i < Clips.Num(); i++)
	{
		if (Clips[i].IsEnable())
		{
			FGeometry MaskGeometry = AllottedGeometry.MakeChild(Clips[i].GetPos(), Clips[i].GetSize(), 1.f);
			Args.GetHittestGrid().AddClickClip(this, MakeShareable(new FSlateClickClippingState(i, MaskGeometry, FOnClickClipClicked::CreateRaw(MutableThis, &SMaskWidget::OnClickClipClicked))));
		}
	}

	return RetLayerId;
}

bool SMaskWidget::OnClickClipClicked(const FVector2D& HitUVInMask, const int32& ClipIndex)
{
	bool bThroughMask = false;

	if (const UTexture2D* MaskTexture = GetMaskTextureByIndex(ClipIndex))
	{
		int32 SizeX = MaskTexture->GetSizeX();
		int32 SizeY = MaskTexture->GetSizeY();
		int32 BufferLen = SizeX * SizeY;
		int32 BufferIdx = floor(HitUVInMask.Y * SizeY) * SizeX + floor(HitUVInMask.X * SizeX);
		if (const FColor* MaskData = reinterpret_cast<const FColor*>(MaskTexture->PlatformData->Mips[0].BulkData.LockReadOnly()))
		{
			if (BufferIdx >= 0 && BufferIdx < BufferLen)
			{
				if (MaskData[BufferIdx].R > 0)
				{
					bThroughMask = true;
				}
			}
		}
		MaskTexture->PlatformData->Mips[0].BulkData.Unlock();
	}
	else
	{
		bThroughMask = FVector2D::DistSquared(HitUVInMask - 0.5f, FVector2D::ZeroVector) < 0.25f;
	}

// 移动设备没有Hover，此时触发可以认为是OnClick
// 触发OnClicked是为了给用户一个比较方便的方法处理MaskWidget被点击了
#if PLATFORM_IOS || PLATFORM_ANDROID
	if (OnClicked.IsBound())
	{
		OnClicked.Execute(ClipIndex, bThroughMask);
	}
#endif

	return bThroughMask;
}

FVector2D SMaskWidget::ComputeDesiredSize(float) const
{
	return GetBackgroundImage()->ImageSize;
}

FReply SMaskWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (OnClicked.IsBound())
	{
		return OnClicked.Execute(-1, false);
	}
	return FReply::Handled();
}

FReply SMaskWidget::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& GestureEvent)
{
	if (OnClicked.IsBound())
	{
		return OnClicked.Execute(-1, false);
	}
	return FReply::Handled();
}
