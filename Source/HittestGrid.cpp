// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/HittestGrid.h"
#include "Rendering/RenderingCommon.h"
#include "SlateGlobals.h"
#include "HAL/IConsoleManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogHittestDebug, Display, All);

DECLARE_CYCLE_STAT(TEXT("HitTestGrid AddWidget"), STAT_SlateHTG_AddWidget, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("HitTestGrid RemoveWidget"), STAT_SlateHTG_RemoveWidget, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("HitTestGrid Clear"), STAT_SlateHTG_Clear, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("HitTestGrid GetCollapsedWidgets"), STAT_SlateHTG_GetCollapsedWidgets, STATGROUP_Slate);

#define LOCTEXT_NAMESPACE "HittestGrid"
#define UE_SLATE_HITTESTGRID_ARRAYSIZEMAX 0

#if UE_SLATE_HITTESTGRID_ARRAYSIZEMAX
int32 HittestGrid_CollapsedHittestGridArraySizeMax = 0;
int32 HittestGrid_CollapsedWidgetsArraySizeMax = 0;
#endif

#if WITH_SLATE_DEBUGGING
FHittestGrid::FDebuggingFindNextFocusableWidget FHittestGrid::OnFindNextFocusableWidgetExecuted;
#endif //WITH_SLATE_DEBUGGING

constexpr bool IsCompatibleUserIndex(int32 RequestedUserIndex, int32 TestUserIndex)
{
	// INDEX_NONE means the user index is compatible with all
	return RequestedUserIndex == INDEX_NONE || TestUserIndex == INDEX_NONE|| RequestedUserIndex == TestUserIndex;
}

//
// Helper Functions
//

FVector2D ClosestPointOnSlateRotatedRect(const FVector2D &Point, const FSlateRotatedRect& RotatedRect)
{
	//no need to do any testing if we are inside of the rect
	if (RotatedRect.IsUnderLocation(Point))
	{
		return Point;
	}

	const static int32 NumOfCorners = 4;
	FVector2D Corners[NumOfCorners];
	Corners[0] = RotatedRect.TopLeft;
	Corners[1] = Corners[0] + RotatedRect.ExtentX;
	Corners[2] = Corners[1] + RotatedRect.ExtentY;
	Corners[3] = Corners[0] + RotatedRect.ExtentY;

	FVector2D RetPoint;
	float ClosestDistSq = FLT_MAX;
	for (int32 i = 0; i < NumOfCorners; ++i)
	{
		//grab the closest point along the line segment
		const FVector2D ClosestPoint = FMath::ClosestPointOnSegment2D(Point, Corners[i], Corners[(i + 1) % NumOfCorners]);

		//get the distance between the two
		const float TestDist = FVector2D::DistSquared(Point, ClosestPoint);

		//if the distance is smaller than the current smallest, update our closest
		if (TestDist < ClosestDistSq)
		{
			RetPoint = ClosestPoint;
			ClosestDistSq = TestDist;
		}
	}

	return RetPoint;
}

FORCEINLINE float DistanceSqToSlateRotatedRect(const FVector2D &Point, const FSlateRotatedRect& RotatedRect)
{
	return FVector2D::DistSquared(ClosestPointOnSlateRotatedRect(Point, RotatedRect), Point);
}

FORCEINLINE bool IsOverlappingSlateRotatedRect(const FVector2D& Point, const float Radius, const FSlateRotatedRect& RotatedRect)
{
	return DistanceSqToSlateRotatedRect( Point, RotatedRect ) <= (Radius * Radius);
}

bool ContainsInteractableWidget(const TArray<FWidgetAndPointer>& PathToTest)
{
	for (int32 i = PathToTest.Num() - 1; i >= 0; --i)
	{
		const FWidgetAndPointer& WidgetAndPointer = PathToTest[i];
		if (WidgetAndPointer.Widget->IsInteractable())
		{
			return true;
		}
	}
	return false;
}


const FVector2D CellSize(128.0f, 128.0f);

//
// FHittestGrid::FWidgetIndex
//
const FHittestGrid::FWidgetData& FHittestGrid::FWidgetIndex::GetWidgetData() const
{
	check(Grid->WidgetArray.IsValidIndex(WidgetIndex));
	return Grid->WidgetArray[WidgetIndex];
}

//
// FHittestGrid::FGridTestingParams
//
struct FHittestGrid::FGridTestingParams
{
	/** Ctor */
	FGridTestingParams()
	: CellCoord(-1, -1)
	, CursorPositionInGrid(FVector2D::ZeroVector)
	, Radius(-1.0f)
	, bTestWidgetIsInteractive(false)
	{}

	FIntPoint CellCoord;
	FVector2D CursorPositionInGrid;
	float Radius;
	bool bTestWidgetIsInteractive;
};

//
// FHittestGrid::FCell
//

void FHittestGrid::FCell::AddIndex(int32 WidgetIndex)
{
	check(!WidgetIndexes.Contains(WidgetIndex));
	WidgetIndexes.Add(WidgetIndex);
}

void FHittestGrid::FCell::RemoveIndex(int32 WidgetIndex)
{
	WidgetIndexes.RemoveSingleSwap(WidgetIndex);
}

//
// FHittestGrid
//

FHittestGrid::FHittestGrid()
	: WidgetMap()
	, WidgetArray()
	, Cells()
	, AppendedGridArray()
	, Owner(nullptr)
	, CullingRect()
	, NumCells(0, 0)
	, GridOrigin(0, 0)
	, GridSize(0, 0)
	, CurrentUserIndex(INDEX_NONE)
{
}

TArray<FWidgetAndPointer> FHittestGrid::GetBubblePath(FVector2D DesktopSpaceCoordinate, float CursorRadius, bool bIgnoreEnabledStatus, int32 UserIndex)
{
	checkSlow(IsInGameThread());

	const FVector2D CursorPositionInGrid = DesktopSpaceCoordinate - GridOrigin;

	if (WidgetArray.Num() > 0 && Cells.Num() > 0)
	{
		FGridTestingParams TestingParams;
		TestingParams.CursorPositionInGrid = CursorPositionInGrid;
		TestingParams.CellCoord = GetCellCoordinate(CursorPositionInGrid);
		TestingParams.Radius = 0.0f;
		TestingParams.bTestWidgetIsInteractive = false;

		// First add the exact point test results
		const FIndexAndDistance BestHit = GetHitIndexFromCellIndex(TestingParams);
		if (BestHit.IsValid())
		{
			const FWidgetData& BestHitWidgetData = BestHit.GetWidgetData();
			const TSharedPtr<SWidget> FirstHitWidget = BestHitWidgetData.GetWidget();
			// Make Sure we landed on a valid widget
			if (FirstHitWidget.IsValid() && IsCompatibleUserIndex(UserIndex, BestHitWidgetData.UserIndex))
			{
				TArray<FWidgetAndPointer> Path;

				TSharedPtr<SWidget> CurWidget = FirstHitWidget;
				while (CurWidget.IsValid())
				{
					FGeometry DesktopSpaceGeometry = CurWidget->GetPaintSpaceGeometry();
					DesktopSpaceGeometry.AppendTransform(FSlateLayoutTransform(GridOrigin - GridWindowOrigin));

					Path.Emplace(FArrangedWidget(CurWidget.ToSharedRef(), DesktopSpaceGeometry), TSharedPtr<FVirtualPointerPosition>());
					CurWidget = CurWidget->Advanced_GetPaintParentWidget();
				}

				if (!Path.Last().Widget->Advanced_IsWindow())
				{
					return TArray<FWidgetAndPointer>();
				}

				Algo::Reverse(Path);

				bool bRemovedDisabledWidgets = false;
				if (!bIgnoreEnabledStatus)
				{
					// @todo It might be more correct to remove all disabled widgets and non-hit testable widgets.  It doesn't make sense to have a hit test invisible widget as a leaf in the path
					// and that can happen if we remove a disabled widget. Furthermore if we did this we could then append custom paths in all cases since the leaf most widget would be hit testable
					// For backwards compatibility changing this could be risky
					const int32 DisabledWidgetIndex = Path.IndexOfByPredicate([](const FArrangedWidget& SomeWidget) { return !SomeWidget.Widget->IsEnabled(); });
					if (DisabledWidgetIndex != INDEX_NONE)
					{
						bRemovedDisabledWidgets = true;
						Path.RemoveAt(DisabledWidgetIndex, Path.Num() - DisabledWidgetIndex);
					}
				}

				if (!bRemovedDisabledWidgets && Path.Num() > 0)
				{
					if (BestHitWidgetData.CustomPath.IsValid())
					{
						const TArray<FWidgetAndPointer> BubblePathExtension = BestHitWidgetData.CustomPath.Pin()->GetBubblePathAndVirtualCursors(FirstHitWidget->GetTickSpaceGeometry(), DesktopSpaceCoordinate, bIgnoreEnabledStatus);
						Path.Append(BubblePathExtension);
					}
				}
	
				return Path;
			}
		}
	}

	return TArray<FWidgetAndPointer>();
}

bool FHittestGrid::SetHittestArea(const FVector2D& HittestPositionInDesktop, const FVector2D& HittestDimensions, const FVector2D& HitestOffsetInWindow)
{
	bool bWasCleared = false;

	// If the size of the hit test area changes we need to clear it out
	if (GridSize != HittestDimensions)
	{
		GridSize = HittestDimensions;
		NumCells = FIntPoint(FMath::CeilToInt(GridSize.X / CellSize.X), FMath::CeilToInt(GridSize.Y / CellSize.Y));
		
		const int32 NewTotalCells = NumCells.X * NumCells.Y;
		ClearInternal(NewTotalCells);

		bWasCleared = true;
	}

	GridOrigin = HittestPositionInDesktop;
	GridWindowOrigin = HitestOffsetInWindow;

	return bWasCleared;
}

void FHittestGrid::Clear()
{
	const int32 TotalCells = Cells.Num();
	ClearInternal(TotalCells);
}

void FHittestGrid::ClearInternal(int32 TotalCells)
{
	SCOPE_CYCLE_COUNTER(STAT_SlateHTG_Clear);
	Cells.Reset(TotalCells);
	Cells.SetNum(TotalCells);

	WidgetMap.Reset();
	WidgetArray.Reset();
    // HankShu-inkiu0@gmail.com add ClickClip Start
	ClickClipMap.Reset();
    // HankShu-inkiu0@gmail.com add ClickClip End
	AppendedGridArray.Reset();
}

bool FHittestGrid::IsDescendantOf(const TSharedRef<SWidget> Parent, const FWidgetData& ChildData) const
{
	const TSharedPtr<SWidget> ChildWidgetPtr = ChildData.GetWidget();
	if (ChildWidgetPtr == Parent)
	{
		return false;
	}

	const SWidget* ParentWidget = &Parent.Get();
	const SWidget* CurWidget = ChildWidgetPtr.Get();

	while (CurWidget)
	{
		if (ParentWidget == CurWidget)
		{
			return true;
		}
		CurWidget = CurWidget->Advanced_GetPaintParentWidget().Get();
	}

	return false;
}

#if WITH_SLATE_DEBUGGING
namespace HittestGridDebuggingText
{
	static FText Valid = LOCTEXT("StateValid", "Valid"); //~ The widget is valid will be consider as the result
	static FText NotCompatibleWithUserIndex = LOCTEXT("StateNotCompatibleWithUserIndex", "User Index not compatible"); //~ The widget is not compatible with the requested user index
	static FText DoesNotIntersect = LOCTEXT("StateDoesNotIntersect", "Does not intersect"); //~ The widget rect is not in the correct direction or is not intersecting with the "swept" rectangle
	static FText PreviousWidgetIsBetter = LOCTEXT("StatePreviousWidgetIsBetter", "Previous Widget was better"); //~ The widget would be valid but the previous valid is closer
	static FText NotADescendant = LOCTEXT("StateNotADescendant", "Not a descendant"); //~ We have a non escape boundary condition and the widget isn't a descendant of our boundary
	static FText Disabled = LOCTEXT("StateNotEnabled", "Disabled"); //~ The widget is not enabled
	static FText DoesNotSuportKeyboardFocus = LOCTEXT("StateDoesNotSuportKeyboardFocus", "Keyboard focus unsupported"); //~ THe widget does not support keyboard focus
}
	#define AddToNextFocusableWidgetCondidateDebugResults(Candidate, Result) { if (IntermediateResultsPtr) { IntermediateResultsPtr->Emplace((Candidate), (Result)); } }
#else
	#define AddToNextFocusableWidgetCondidateDebugResults(Candidate, Result) CA_ASSUME(Candidate)
#endif

template<typename TCompareFunc, typename TSourceSideFunc, typename TDestSideFunc>
TSharedPtr<SWidget> FHittestGrid::FindFocusableWidget(FSlateRect WidgetRect, const FSlateRect SweptRect, int32 AxisIndex, int32 Increment, const EUINavigation Direction, const FNavigationReply& NavigationReply, TCompareFunc CompareFunc, TSourceSideFunc SourceSideFunc, TDestSideFunc DestSideFunc, int32 UserIndex, TArray<FDebuggingFindNextFocusableWidgetArgs::FWidgetResult>* IntermediateResultsPtr) const
{
	FIntPoint CurrentCellPoint = GetCellCoordinate(WidgetRect.GetCenter());

	int32 StartingIndex = CurrentCellPoint[AxisIndex];

	float CurrentSourceSide = SourceSideFunc(WidgetRect);

	int32 StrideAxis, StrideAxisMin, StrideAxisMax;

	// Ensure that the hit test grid is valid before proceeding
	if (NumCells.X < 1 || NumCells.Y < 1)
	{
		return TSharedPtr<SWidget>();
	}

	if (AxisIndex == 0)
	{
		StrideAxis = 1;
		StrideAxisMin = FMath::Min(FMath::Max(FMath::FloorToInt(SweptRect.Top / CellSize.Y), 0), NumCells.Y - 1);
		StrideAxisMax = FMath::Min(FMath::Max(FMath::FloorToInt(SweptRect.Bottom / CellSize.Y), 0), NumCells.Y - 1);
	}
	else
	{
		StrideAxis = 0;
		StrideAxisMin = FMath::Min(FMath::Max(FMath::FloorToInt(SweptRect.Left / CellSize.X), 0), NumCells.X - 1);
		StrideAxisMax = FMath::Min(FMath::Max(FMath::FloorToInt(SweptRect.Right / CellSize.X), 0), NumCells.X - 1);
	}

	bool bWrapped = false;
	while (CurrentCellPoint[AxisIndex] >= 0 && CurrentCellPoint[AxisIndex] < NumCells[AxisIndex])
	{
		FIntPoint StrideCellPoint = CurrentCellPoint;
		int32 CurrentCellProcessed = CurrentCellPoint[AxisIndex];

		// Increment before the search as a wrap case will change our current cell.
		CurrentCellPoint[AxisIndex] += Increment;

		FSlateRect BestWidgetRect;
		TSharedPtr<SWidget> BestWidget = TSharedPtr<SWidget>();

		for (StrideCellPoint[StrideAxis] = StrideAxisMin; StrideCellPoint[StrideAxis] <= StrideAxisMax; ++StrideCellPoint[StrideAxis])
		{
			FCollapsedWidgetsArray WidgetIndexes;
			GetCollapsedWidgets(WidgetIndexes, StrideCellPoint.X, StrideCellPoint.Y);

			for (int32 i = WidgetIndexes.Num() - 1; i >= 0; --i)
			{
				const FWidgetData& TestCandidate = WidgetIndexes[i].GetWidgetData();
				const TSharedPtr<SWidget> TestWidget = TestCandidate.GetWidget();
				if (!TestWidget.IsValid())
				{
					continue;
				}

				if (!IsCompatibleUserIndex(UserIndex, TestCandidate.UserIndex))
				{
					AddToNextFocusableWidgetCondidateDebugResults(TestWidget, HittestGridDebuggingText::NotCompatibleWithUserIndex);
					continue;
				}

				FGeometry TestCandidateGeo = TestWidget->GetPaintSpaceGeometry();
				TestCandidateGeo.AppendTransform(FSlateLayoutTransform(-GridWindowOrigin));
				const FSlateRect TestCandidateRect = TestCandidateGeo.GetRenderBoundingRect();
				if (!(CompareFunc(DestSideFunc(TestCandidateRect), CurrentSourceSide) && FSlateRect::DoRectanglesIntersect(SweptRect, TestCandidateRect)))
				{
					AddToNextFocusableWidgetCondidateDebugResults(TestWidget, HittestGridDebuggingText::DoesNotIntersect);
					continue;
				}

				// If this found widget isn't closer then the previously found widget then keep looking.
				if (BestWidget.IsValid() && !CompareFunc(DestSideFunc(BestWidgetRect), DestSideFunc(TestCandidateRect)))
				{
					AddToNextFocusableWidgetCondidateDebugResults(TestWidget, HittestGridDebuggingText::PreviousWidgetIsBetter);
					continue;
				}

				// If we have a non escape boundary condition and this widget isn't a descendant of our boundary condition widget then it's invalid so we keep looking.
				if (NavigationReply.GetBoundaryRule() != EUINavigationRule::Escape
					&& NavigationReply.GetHandler().IsValid()
					&& !IsDescendantOf(NavigationReply.GetHandler().ToSharedRef(), TestCandidate))
				{
					AddToNextFocusableWidgetCondidateDebugResults(TestWidget, HittestGridDebuggingText::NotADescendant);
					continue;
				}

				if (!TestWidget->IsEnabled())
				{
					AddToNextFocusableWidgetCondidateDebugResults(TestWidget, HittestGridDebuggingText::Disabled);
					continue;
				}
				
				if (!TestWidget->SupportsKeyboardFocus())
				{
					AddToNextFocusableWidgetCondidateDebugResults(TestWidget, HittestGridDebuggingText::DoesNotSuportKeyboardFocus);
					continue;
				}

				BestWidgetRect = TestCandidateRect;
				BestWidget = TestWidget;
				AddToNextFocusableWidgetCondidateDebugResults(TestWidget, HittestGridDebuggingText::Valid);
			}
		}

		if (BestWidget.IsValid())
		{
			// Check for the need to apply our rule
			if (CompareFunc(DestSideFunc(BestWidgetRect), SourceSideFunc(SweptRect)))
			{
				switch (NavigationReply.GetBoundaryRule())
				{
				case EUINavigationRule::Explicit:
					return NavigationReply.GetFocusRecipient();
				case EUINavigationRule::Custom:
				case EUINavigationRule::CustomBoundary:
				{
					const FNavigationDelegate& FocusDelegate = NavigationReply.GetFocusDelegate();
					if (FocusDelegate.IsBound())
					{
						return FocusDelegate.Execute(Direction);
					}
					return TSharedPtr<SWidget>();
				}
				case EUINavigationRule::Stop:
					return TSharedPtr<SWidget>();
				case EUINavigationRule::Wrap:
					CurrentSourceSide = DestSideFunc(SweptRect);
					FVector2D SampleSpot = WidgetRect.GetCenter();
					SampleSpot[AxisIndex] = CurrentSourceSide;
					CurrentCellPoint = GetCellCoordinate(SampleSpot);
					bWrapped = true;
					break;
				}
			}

			return BestWidget;
		}

		// break if we have looped back to where we started.
		if (bWrapped && StartingIndex == CurrentCellProcessed) { break; }

		// If were going to fail our bounds check and our rule is to a boundary condition (Wrap or CustomBoundary) handle appropriately
		if (!(CurrentCellPoint[AxisIndex] >= 0 && CurrentCellPoint[AxisIndex] < NumCells[AxisIndex]))
		{
			if (NavigationReply.GetBoundaryRule() == EUINavigationRule::Wrap)
			{
				if (bWrapped)
				{
					// If we've already wrapped, unfortunately it must be that the starting widget wasn't within the boundary
					break;
				}
				CurrentSourceSide = DestSideFunc(SweptRect);
				FVector2D SampleSpot = WidgetRect.GetCenter();
				SampleSpot[AxisIndex] = CurrentSourceSide;
				CurrentCellPoint = GetCellCoordinate(SampleSpot);
				bWrapped = true;
			}
			else if (NavigationReply.GetBoundaryRule() == EUINavigationRule::CustomBoundary)
			{
				const FNavigationDelegate& FocusDelegate = NavigationReply.GetFocusDelegate();
				if (FocusDelegate.IsBound())
				{
					return FocusDelegate.Execute(Direction);
				}
			}
		}
	}

	return TSharedPtr<SWidget>();
}

#undef AddToNextFocusableWidgetCondidateDebugResults

TSharedPtr<SWidget> FHittestGrid::FindNextFocusableWidget(const FArrangedWidget& StartingWidget, const EUINavigation Direction, const FNavigationReply& NavigationReply, const FArrangedWidget& RuleWidget, int32 UserIndex)
{
	FGeometry StartingWidgetGeo = StartingWidget.Widget->GetPaintSpaceGeometry();
	StartingWidgetGeo.AppendTransform(FSlateLayoutTransform(-GridWindowOrigin));
	FSlateRect WidgetRect = StartingWidgetGeo.GetRenderBoundingRect();

	FGeometry BoundingRuleWidgetGeo = RuleWidget.Widget->GetPaintSpaceGeometry();
	BoundingRuleWidgetGeo.AppendTransform(FSlateLayoutTransform(-GridWindowOrigin));
	FSlateRect BoundingRuleRect = BoundingRuleWidgetGeo.GetRenderBoundingRect();

	FSlateRect SweptWidgetRect = WidgetRect;

	TSharedPtr<SWidget> Widget = TSharedPtr<SWidget>();

#if WITH_SLATE_DEBUGGING
	TArray<FDebuggingFindNextFocusableWidgetArgs::FWidgetResult> IntermediateResults;
	TArray<FDebuggingFindNextFocusableWidgetArgs::FWidgetResult>* IntermediateResultsPtr = OnFindNextFocusableWidgetExecuted.IsBound() ? &IntermediateResults : nullptr;
#else
	TArray<FDebuggingFindNextFocusableWidgetArgs::FWidgetResult>* IntermediateResultsPtr = nullptr;
#endif

	switch (Direction)
	{
	case EUINavigation::Left:
		SweptWidgetRect.Left = BoundingRuleRect.Left;
		SweptWidgetRect.Right = BoundingRuleRect.Right;
		SweptWidgetRect.Top += 0.5f;
		SweptWidgetRect.Bottom -= 0.5f;
		Widget = FindFocusableWidget(WidgetRect, SweptWidgetRect, 0, -1, Direction, NavigationReply,
			[](float A, float B) { return A - 0.1f < B; }, // Compare function
			[](FSlateRect SourceRect) { return SourceRect.Left; }, // Source side function
			[](FSlateRect DestRect) { return DestRect.Right; }, // Dest side function
			UserIndex, IntermediateResultsPtr);
		break;
	case EUINavigation::Right:
		SweptWidgetRect.Left = BoundingRuleRect.Left;
		SweptWidgetRect.Right = BoundingRuleRect.Right;
		SweptWidgetRect.Top += 0.5f;
		SweptWidgetRect.Bottom -= 0.5f;
		Widget = FindFocusableWidget(WidgetRect, SweptWidgetRect, 0, 1, Direction, NavigationReply,
			[](float A, float B) { return A + 0.1f > B; }, // Compare function
			[](FSlateRect SourceRect) { return SourceRect.Right; }, // Source side function
			[](FSlateRect DestRect) { return DestRect.Left; }, // Dest side function
			UserIndex, IntermediateResultsPtr);
		break;
	case EUINavigation::Up:
		SweptWidgetRect.Top = BoundingRuleRect.Top;
		SweptWidgetRect.Bottom = BoundingRuleRect.Bottom;
		SweptWidgetRect.Left += 0.5f;
		SweptWidgetRect.Right -= 0.5f;
		Widget = FindFocusableWidget(WidgetRect, SweptWidgetRect, 1, -1, Direction, NavigationReply,
			[](float A, float B) { return A - 0.1f < B; }, // Compare function
			[](FSlateRect SourceRect) { return SourceRect.Top; }, // Source side function
			[](FSlateRect DestRect) { return DestRect.Bottom; }, // Dest side function
			UserIndex, IntermediateResultsPtr);
		break;
	case EUINavigation::Down:
		SweptWidgetRect.Top = BoundingRuleRect.Top;
		SweptWidgetRect.Bottom = BoundingRuleRect.Bottom;
		SweptWidgetRect.Left += 0.5f;
		SweptWidgetRect.Right -= 0.5f;
		Widget = FindFocusableWidget(WidgetRect, SweptWidgetRect, 1, 1, Direction, NavigationReply,
			[](float A, float B) { return A + 0.1f > B; }, // Compare function
			[](FSlateRect SourceRect) { return SourceRect.Bottom; }, // Source side function
			[](FSlateRect DestRect) { return DestRect.Top; }, // Dest side function
			UserIndex, IntermediateResultsPtr);
		break;

	default:
		break;
	}

#if WITH_SLATE_DEBUGGING
	if (IntermediateResultsPtr)
	{
		FDebuggingFindNextFocusableWidgetArgs Args {StartingWidget, Direction, NavigationReply, RuleWidget, UserIndex, Widget , MoveTemp(*IntermediateResultsPtr)};
		OnFindNextFocusableWidgetExecuted.Broadcast(this, Args);
	}
#endif

	return Widget;
}

FIntPoint FHittestGrid::GetCellCoordinate(FVector2D Position) const
{
	return FIntPoint(
		FMath::Min(FMath::Max(FMath::FloorToInt(Position.X / CellSize.X), 0), NumCells.X - 1),
		FMath::Min(FMath::Max(FMath::FloorToInt(Position.Y / CellSize.Y), 0), NumCells.Y - 1));
}

bool FHittestGrid::IsValidCellCoord(const FIntPoint& CellCoord) const
{
	return IsValidCellCoord(CellCoord.X, CellCoord.Y);
}

bool FHittestGrid::IsValidCellCoord(const int32 XCoord, const int32 YCoord) const
{
	return XCoord >= 0 && XCoord < NumCells.X && YCoord >= 0 && YCoord < NumCells.Y;
}

void FHittestGrid::AddGrid(const TSharedRef<const FHittestGrid>& OtherGrid)
{
	auto GetCollapsedHittestGrid_NoTests = [](const FHittestGrid* HittestGrid, FCollapsedHittestGridArray& OutResult)
	{
		OutResult.Add(HittestGrid);
		for (int32 Index = 0; Index < OutResult.Num(); ++Index)
		{
			for (const FAppendedGridData& AppendedGridData : OutResult[Index]->AppendedGridArray)
			{
				if (const TSharedPtr<const FHittestGrid> Grid = AppendedGridData.Grid.Pin())
				{
					OutResult.Add(Grid.Get());
				}
			}
		}
	};

	const bool bIsContains = AppendedGridArray.ContainsByPredicate([OtherGrid](const FAppendedGridData& Other) { return Other.Grid == OtherGrid; });
	if (ensure(CanBeAppended(&OtherGrid.Get())))
	{
		if (!bIsContains)
		{
			// Check for recursion
			FCollapsedHittestGridArray AllHittestGrid;
			GetCollapsedHittestGrid_NoTests(this, AllHittestGrid); // we are building a new array, do not perform size check
			const bool bIsInCollapsed = AllHittestGrid.ContainsByPredicate([OtherGrid](const FHittestGrid* Other) { return &*OtherGrid == Other; });
			ensure(!bIsInCollapsed);
			if (bIsInCollapsed)
			{
				return;
			}

			AppendedGridArray.Emplace(OtherGrid->Owner, OtherGrid);
		}
	}
	else
	{
		RemoveGrid(OtherGrid);
	}
}

void FHittestGrid::RemoveGrid(const TSharedRef<const FHittestGrid>& OtherGrid)
{
	const int32 AppendedGridIndex = AppendedGridArray.IndexOfByPredicate(
		[OtherGrid](const FAppendedGridData& Other)
		{
			return Other.Grid == OtherGrid;
		});
	if (AppendedGridIndex != INDEX_NONE)
	{
		AppendedGridArray.RemoveAtSwap(AppendedGridIndex);
	}
}

void FHittestGrid::RemoveGrid(const SWidget* OtherGridOwner)
{
	const int32 AppendedGridIndex = AppendedGridArray.IndexOfByPredicate(
		[OtherGridOwner](const FAppendedGridData& Other)
		{
			return Other.CachedOwner == OtherGridOwner;
		});
	if (AppendedGridIndex != INDEX_NONE)
	{
#if WITH_SLATE_DEBUGGING
		// Confirmed that the cached grid is valid
		if (const TSharedPtr<const FHittestGrid> Pin = AppendedGridArray[AppendedGridIndex].Grid.Pin())
		{
			ensure(Pin->Owner == OtherGridOwner);
		}
#endif
		AppendedGridArray.RemoveAtSwap(AppendedGridIndex);
	}
}

bool FHittestGrid::CanBeAppended(const FHittestGrid* OtherGrid) const
{
	return OtherGrid && OtherGrid->Owner && this != OtherGrid && SameSize(OtherGrid);
}

bool FHittestGrid::SameSize(const FHittestGrid* OtherGrid) const
{
	return GridOrigin == OtherGrid->GridOrigin && GridWindowOrigin == OtherGrid->GridWindowOrigin && GridSize == OtherGrid->GridSize;
}

void FHittestGrid::AddWidget(const TSharedRef<SWidget>& InWidget, int32 InBatchPriorityGroup, int32 InLayerId, int32 InSecondarySort)
{
	if (!InWidget->GetVisibility().IsHitTestVisible())
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_SlateHTG_AddWidget);

	// Track the widget and identify it's Widget Index
	FGeometry GridSpaceGeometry = InWidget->GetPaintSpaceGeometry();
	GridSpaceGeometry.AppendTransform(FSlateLayoutTransform(-GridWindowOrigin));

	// Currently using grid offset because the grid covers all desktop space.
	const FSlateRect BoundingRect = GridSpaceGeometry.GetRenderBoundingRect();

	// Starting and ending cells covered by this widget.	
	const FIntPoint UpperLeftCell = GetCellCoordinate(BoundingRect.GetTopLeft());
	const FIntPoint LowerRightCell = GetCellCoordinate(BoundingRect.GetBottomRight());

	const int64 PrimarySort = (((int64)InBatchPriorityGroup << 32) | InLayerId);

	bool bAddWidget = true;
	if (int32* FoundIndex = WidgetMap.Find(&*InWidget))
	{
		FWidgetData& WidgetData = WidgetArray[*FoundIndex];
		if (WidgetData.UpperLeftCell != UpperLeftCell || WidgetData.LowerRightCell != LowerRightCell)
		{
			// Need to be updated
			RemoveWidget(InWidget);
		}
		else
		{
			// Only update
			bAddWidget = false;
			WidgetData.PrimarySort = PrimarySort;
			WidgetData.SecondarySort = InSecondarySort;
			WidgetData.UserIndex = CurrentUserIndex;
		}
	}

	if (bAddWidget)
	{
		int32& WidgetIndex = WidgetMap.Add(&*InWidget);
		WidgetIndex = WidgetArray.Emplace(InWidget, UpperLeftCell, LowerRightCell, PrimarySort, InSecondarySort, CurrentUserIndex);
		for (int32 XIndex = UpperLeftCell.X; XIndex <= LowerRightCell.X; ++XIndex)
		{
			for (int32 YIndex = UpperLeftCell.Y; YIndex <= LowerRightCell.Y; ++YIndex)
			{
				if (IsValidCellCoord(XIndex, YIndex))
				{
					CellAt(XIndex, YIndex).AddIndex(WidgetIndex);
				}
			}
		}
	}
}

void FHittestGrid::RemoveWidget(const TSharedRef<SWidget>& InWidget)
{
	RemoveWidget(&*InWidget);
}

void FHittestGrid::RemoveWidget(const SWidget* InWidget)
{
	SCOPE_CYCLE_COUNTER(STAT_SlateHTG_RemoveWidget);

	int32 WidgetIndex = INDEX_NONE;
	if (WidgetMap.RemoveAndCopyValue(InWidget, WidgetIndex))
	{
		const FWidgetData& WidgetData = WidgetArray[WidgetIndex];

		// Starting and ending cells covered by this widget.	
		const FIntPoint& UpperLeftCell = WidgetData.UpperLeftCell;
		const FIntPoint& LowerRightCell = WidgetData.LowerRightCell;

		for (int32 XIndex = UpperLeftCell.X; XIndex <= LowerRightCell.X; ++XIndex)
		{
			for (int32 YIndex = UpperLeftCell.Y; YIndex <= LowerRightCell.Y; ++YIndex)
			{
				checkSlow(IsValidCellCoord(XIndex, YIndex));
				CellAt(XIndex, YIndex).RemoveIndex(WidgetIndex);
			}
		}

		WidgetArray.RemoveAt(WidgetIndex);
	}

	RemoveGrid(InWidget);
}

void FHittestGrid::InsertCustomHitTestPath(const TSharedRef<SWidget> InWidget, TSharedRef<ICustomHitTestPath> CustomHitTestPath)
{
	int32 WidgetIndex = WidgetMap.FindChecked(&*InWidget);
	FWidgetData& WidgetData = WidgetArray[WidgetIndex];
	WidgetData.CustomPath = CustomHitTestPath;
}

FHittestGrid::FIndexAndDistance FHittestGrid::GetHitIndexFromCellIndex(const FGridTestingParams& Params) const
{
	//check if the cell coord 
	if (IsValidCellCoord(Params.CellCoord))
	{
		// Get the cell and sort it 
		FCollapsedWidgetsArray WidgetIndexes;
		GetCollapsedWidgets(WidgetIndexes, Params.CellCoord.X, Params.CellCoord.Y);

#if 0 //Unroll some data for debugging if necessary
		struct FDebugData
		{
			FWidgetData WidgetData;
			FName WidgetType;
			FName WidgetLoc;
			TSharedPtr<SWidget> Widget;
		};

		TArray<FDebugData> DebugData;
		for (int32 i = 0; i < WidgetIndexes.Num(); ++i)
		{
			FDebugData& Cur = DebugData.AddDefaulted_GetRef();
			Cur.WidgetData = WidgetIndexes[i].GetWidgetData();
			Cur.Widget = Cur.WidgetData.GetWidget();
			Cur.WidgetType = Cur.Widget.IsValid() ? Cur.Widget->GetType() : NAME_None;
			Cur.WidgetLoc = Cur.Widget.IsValid() ? Cur.Widget->GetCreatedInLocation() : NAME_None;
		}
#endif

		// Consider front-most widgets first for hittesting.
		for (int32 i = WidgetIndexes.Num() - 1; i >= 0; --i)
		{
			check(WidgetIndexes[i].IsValid());
			const FWidgetData& TestCandidate = WidgetIndexes[i].GetWidgetData();
			const TSharedPtr<SWidget> TestWidget = TestCandidate.GetWidget();

			// When performing a point hittest, accept all hittestable widgets.
			// When performing a hittest with a radius, only grab interactive widgets.
			const bool bIsValidWidget = TestWidget.IsValid() && (!Params.bTestWidgetIsInteractive || TestWidget->IsInteractable());
			if (bIsValidWidget)
			{
				const FVector2D WindowSpaceCoordinate = Params.CursorPositionInGrid + GridWindowOrigin;

				const FGeometry& TestGeometry = TestWidget->GetPaintSpaceGeometry();

				bool bPointInsideClipMasks = true;

				if (WidgetIndexes[i].GetCullingRect().IsValid())
				{
					bPointInsideClipMasks = WidgetIndexes[i].GetCullingRect().ContainsPoint(WindowSpaceCoordinate);
				}

				if (bPointInsideClipMasks)
				{
					const TOptional<FSlateClippingState>& WidgetClippingState = TestWidget->GetCurrentClippingState();
					if (WidgetClippingState.IsSet())
					{
						// TODO: Solve non-zero radius cursors?
						bPointInsideClipMasks = WidgetClippingState->IsPointInside(WindowSpaceCoordinate);
					}
				}

				// HankShu-inkiu0@gmail.com add ClickClip Start
				bool IsClickThrough = IsThroughClickClip(Params, TestWidget.Get());
				if (bPointInsideClipMasks && !IsClickThrough)
				// HankShu-inkiu0@gmail.com add ClickClip End
				{
					// Compute the render space clipping rect (FGeometry exposes a layout space clipping rect).
					const FSlateRotatedRect WindowOrientedClipRect = TransformRect(
						Concatenate(
							Inverse(TestGeometry.GetAccumulatedLayoutTransform()),
							TestGeometry.GetAccumulatedRenderTransform()),
						FSlateRotatedRect(TestGeometry.GetLayoutBoundingRect())
					);

					if (IsOverlappingSlateRotatedRect(WindowSpaceCoordinate, Params.Radius, WindowOrientedClipRect))
					{
						// For non-0 radii also record the distance to cursor's center so that we can pick the closest hit from the results.
						const bool bNeedsDistanceSearch = Params.Radius > 0.0f;
						const float DistSq = (bNeedsDistanceSearch) ? DistanceSqToSlateRotatedRect(WindowSpaceCoordinate, WindowOrientedClipRect) : 0.0f;
						return FIndexAndDistance(WidgetIndexes[i], DistSq);
					}
				}
			}
		}
	}

	return FIndexAndDistance();
}

 void FHittestGrid::GetCollapsedHittestGrid(FCollapsedHittestGridArray& OutResult) const
{
	OutResult.Add(this);
	for (const FAppendedGridData& AppendedGridData : AppendedGridArray)
	{
		if (const TSharedPtr<const FHittestGrid>& AppendedGrid = AppendedGridData.Grid.Pin())
		{
			if (ensure(!OutResult.Contains(AppendedGrid.Get()))) // Check for recursion
			{
				if (ensure(SameSize(AppendedGrid.Get())))
				{
					AppendedGrid->GetCollapsedHittestGrid(OutResult);
				}
			}
		}
	}

#if UE_SLATE_HITTESTGRID_ARRAYSIZEMAX
	HittestGrid_CollapsedHittestGridArraySizeMax = FMath::Max(OutResult.Num(), HittestGrid_CollapsedHittestGridArraySizeMax);
#endif
}

#define UE_VERIFY_WIDGET_VALIDITE 0
 void FHittestGrid::GetCollapsedWidgets(FCollapsedWidgetsArray& OutResult, const int32 X, const int32 Y) const
 {
	 SCOPE_CYCLE_COUNTER(STAT_SlateHTG_GetCollapsedWidgets);

	 const int32 CellIndex = Y * NumCells.X + X;
	 check(Cells.IsValidIndex(CellIndex));

	 FCollapsedHittestGridArray AllHitTestGrids;
	 GetCollapsedHittestGrid(AllHitTestGrids);

	 // N.B. it would be more efficient if we only rebuild if the cell has changed
	 //but the PrimarySort and SecondaySort are not updated directly and the cell doesn't know about it

	 {
		 for (const FHittestGrid* HittestGrid : AllHitTestGrids)
		 {
			 const TArray<int32>& WidgetsIndexes = HittestGrid->CellAt(X, Y).GetWidgetIndexes();
			 for (int32 WidgetIndex : WidgetsIndexes)
			 {
#if UE_VERIFY_WIDGET_VALIDITE
				 ensureAlways(HittestGrid->WidgetArray.IsValidIndex(WidgetIndex));
#endif
				 OutResult.Emplace(HittestGrid, WidgetIndex);
			 }
		 }

		 OutResult.StableSort([](const FWidgetIndex& A, const FWidgetIndex& B)
			 {
				 const FWidgetData& WidgetDataA = A.GetWidgetData();
				 const FWidgetData& WidgetDataB = B.GetWidgetData();
				 return WidgetDataA.PrimarySort < WidgetDataB.PrimarySort || (WidgetDataA.PrimarySort == WidgetDataB.PrimarySort && WidgetDataA.SecondarySort < WidgetDataB.SecondarySort);
			 });
	 }

#if UE_SLATE_HITTESTGRID_ARRAYSIZEMAX
	 HittestGrid_CollapsedWidgetsArraySizeMax = FMath::Max(OutResult.Num(), HittestGrid_CollapsedWidgetsArraySizeMax);
#endif
 }
#undef UE_VERIFY_WIDGET_VALIDITE

void FHittestGrid::RemoveStaleAppendedHittestGrid()
{
	for (int32 AppendedGridIndex = AppendedGridArray.Num() - 1; AppendedGridIndex >= 0; --AppendedGridIndex)
	{
		bool bToRemove = false;
		const TWeakPtr<const FHittestGrid>& WeakAppendedGrid = AppendedGridArray[AppendedGridIndex].Grid;
		if (const TSharedPtr<const FHittestGrid>& AppendedGrid = WeakAppendedGrid.Pin())
		{
			if (!SameSize(AppendedGrid.Get()))
			{
				bToRemove = true;
			}
		}
		else
		{
			// grid was removed without notifying the SlateInvalidationRoot.
			//That can happened when the widget containing the owner of the grid is removed in the same frame as it was added.
			bToRemove = true;
		}

		if (bToRemove)
		{
			AppendedGridArray.RemoveAtSwap(AppendedGridIndex);
		}
	}
}

#if WITH_SLATE_DEBUGGING
void FHittestGrid::LogGrid() const
{
	FString TempString;
	for (int32 y = 0; y < NumCells.Y; ++y)
	{
		for (int x = 0; x < NumCells.X; ++x)
		{
			TempString += "\t";
			TempString += "[";
			for (int32 i : CellAt(x, y).GetWidgetIndexes())
			{
				TempString += FString::Printf(TEXT("%d,"), i);
			}
			TempString += "]";
		}
		TempString += "\n";
	}

	TempString += "\n";

	UE_LOG(LogHittestDebug, Warning, TEXT("\n%s"), *TempString);

	for (TSparseArray<FWidgetData>::TConstIterator It(WidgetArray); It; ++It)
	{
		const FWidgetData& CurWidgetData = *It;
		const TSharedPtr<SWidget> CachedWidget = CurWidgetData.GetWidget();
		UE_LOG(LogHittestDebug, Warning, TEXT("  [%d][%d][%d] => %s @ %s"),
			It.GetIndex(),
			CurWidgetData.PrimarySort,
			CachedWidget.IsValid() ? *CachedWidget->ToString() : TEXT("Invalid Widget"),
			CachedWidget.IsValid() ? *CachedWidget->GetPaintSpaceGeometry().ToString() : TEXT("Invalid Widget"));
	}
}

constexpr FLinearColor GetUserColorFromIndex(int32 Index)
{
	switch (Index)
	{
	case 0:
		return FLinearColor::Red;
	case 1:
		return FLinearColor::Green;
	case 2:
		return FLinearColor::Blue;
	case 3:
		return FLinearColor::Yellow;
	default:
		return FLinearColor::White;
	}
};


void FHittestGrid::DisplayGrid(int32 InLayer, const FGeometry& AllottedGeometry, FSlateWindowElementList& WindowElementList, EDisplayGridFlags DisplayFlags) const
{
	static const FSlateBrush* FocusRectangleBrush = FCoreStyle::Get().GetBrush(TEXT("FocusRectangle"));
	static const FSlateBrush* BorderBrush = FCoreStyle::Get().GetBrush(TEXT("Border"));

	const FSlateBrush* Brush = EnumHasAnyFlags(DisplayFlags, EDisplayGridFlags::UseFocusBrush) ? FocusRectangleBrush : BorderBrush;

	auto DisplayGrid = [&] (const FHittestGrid* HittestGrid)
	{
		for (TSparseArray<FWidgetData>::TConstIterator It(HittestGrid->WidgetArray); It; ++It)
		{
			const FWidgetData& CurWidgetData = *It;
			const TSharedPtr<SWidget> CachedWidget = CurWidgetData.GetWidget();
			if (ensure(CachedWidget.IsValid())) // Widget should always be valid
			{
				if (EnumHasAnyFlags(DisplayFlags, EDisplayGridFlags::HideDisabledWidgets) && !CachedWidget->IsEnabled())
				{
					continue;
				}
				if (EnumHasAnyFlags(DisplayFlags, EDisplayGridFlags::HideUnsupportedKeyboardFocusWidgets) && !CachedWidget->SupportsKeyboardFocus())
				{
					continue;
				}

				FSlateDrawElement::MakeBox(
					WindowElementList,
					InLayer,
					CachedWidget->GetPaintSpaceGeometry().ToPaintGeometry(),
					Brush,
					ESlateDrawEffect::None,
					GetUserColorFromIndex(CurWidgetData.UserIndex)
				);
			}
		}
	};

	FCollapsedHittestGridArray AllHitTestGrids;
	GetCollapsedHittestGrid(AllHitTestGrids);
	for(const FHittestGrid* HittestGrid : AllHitTestGrids)
	{
		DisplayGrid(HittestGrid);
	}

	// Appended grid should always be valid
	FCollapsedHittestGridArray TestHitTestGrids;
	TestHitTestGrids.Add(this);
	for (int32 Index = 0; Index < TestHitTestGrids.Num(); ++Index)
	{
		const FHittestGrid* Grid = TestHitTestGrids[Index];
		for (const FAppendedGridData& OtherGridData : Grid->AppendedGridArray)
		{
			const TSharedPtr<const FHittestGrid> OtherGridPin = OtherGridData.Grid.Pin();
			if (ensureAlways(OtherGridPin.IsValid()))
			{
				TestHitTestGrids.Add(&*OtherGridPin);
			}
		}
	}
}
#endif // WITH_SLATE_DEBUGGING

// HankShu-inkiu0@gmail.com add ClickClip Start

void FHittestGrid::AddClickClip(const SWidget* InWidget, const TSharedPtr<FSlateClickClippingState>& InClickClip)
{
	if (!ClickClipMap.Contains(InWidget))
	{
		ClickClipMap.Add(InWidget);
	}
	ClickClipMap[InWidget].Add(InClickClip);
}

bool FHittestGrid::IsThroughClickClip(const FGridTestingParams& Params, const SWidget* ClickWidget) const
{
	if (ClickClipMap.Contains(ClickWidget))
	{
		uint8 HitClipNum = 0;
		uint8 ThroughClipNum = 0;
		TArray<TSharedPtr<FSlateClickClippingState>> ClickClips = ClickClipMap[ClickWidget];
		for (uint8 i = 0; i < ClickClips.Num(); i++)
		{
			if (ClickClips[i]->IsPointInside(Params.CursorPositionInGrid + GridWindowOrigin))
			{
				HitClipNum++;
				if (ClickClips[i]->IsClickThrough(Params.CursorPositionInGrid + GridWindowOrigin))
				{
					ThroughClipNum++;
				}
			}
		}

		if (HitClipNum > 0 && HitClipNum == ThroughClipNum)
		{
			return true;
		}
	}
	return false;
}

// HankShu-inkiu0@gmail.com add ClickClip end

#undef UE_SLATE_HITTESTGRID_ARRAYSIZEMAX
#undef LOCTEXT_NAMESPACE
