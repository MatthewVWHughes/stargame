#include "UI/StargameSystemMapWidget.h"

#include "InputCoreTypes.h"

namespace
{
	constexpr double MinZoom = 0.35;
	constexpr double MaxZoom = 12.0;
	constexpr double DefaultHitRadiusPixels = 18.0;

	FVector2D SafeLocalSize(const FVector2D& LocalSize)
	{
		return FVector2D(FMath::Max(1.0, LocalSize.X), FMath::Max(1.0, LocalSize.Y));
	}

	FName ResolveMarkerSelectableTarget(const FSystemMapMarkerView& Marker)
	{
		if (!Marker.TargetId.IsNone())
		{
			return Marker.TargetId;
		}
		return Marker.SourceId;
	}

	bool IsSelectableMarkerType(FName MarkerType)
	{
		return MarkerType == FName(TEXT("selected_target")) ||
			MarkerType == FName(TEXT("mission_target")) ||
			MarkerType == FName(TEXT("encounter_actor"));
	}

	bool IsSelectableEntryType(FName EntryType)
	{
		return EntryType == FName(TEXT("body")) ||
			EntryType == FName(TEXT("station")) ||
			EntryType == FName(TEXT("gate")) ||
			EntryType == FName(TEXT("resource_zone"));
	}
}

void UStargameSystemMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Collapsed);
	RefreshMapView();
}

void UStargameSystemMapWidget::OpenMap()
{
	bMapOpen = true;
	PanOffset = FVector2D::ZeroVector;
	Zoom = 1.0;
	bDragging = false;
	LastStatusText.Reset();
	SetVisibility(ESlateVisibility::Visible);
	RefreshMapView();
	SetKeyboardFocus();
	OnMapOpened(CachedMapView);
	OnMapViewChanged(CachedMapView, PanOffset, Zoom);
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(TakeWidget());
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetIgnoreLookInput(true);
		PlayerController->SetIgnoreMoveInput(true);
		PlayerController->bShowMouseCursor = true;
	}
}

void UStargameSystemMapWidget::CloseMap()
{
	bMapOpen = false;
	bDragging = false;
	SetVisibility(ESlateVisibility::Collapsed);
	OnMapClosed();
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetIgnoreLookInput(false);
		PlayerController->SetIgnoreMoveInput(false);
		PlayerController->bShowMouseCursor = true;
	}
}

void UStargameSystemMapWidget::ToggleMap()
{
	if (bMapOpen)
	{
		CloseMap();
	}
	else
	{
		OpenMap();
	}
}

void UStargameSystemMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bMapOpen)
	{
		if (RefreshMapView())
		{
			OnMapViewChanged(CachedMapView, PanOffset, Zoom);
		}
	}
}

bool UStargameSystemMapWidget::RefreshMapView()
{
	UStargameSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!Session || !Session->GetSystemMapOverview(CachedMapView))
	{
		CachedMapView = FSystemMapOverviewView();
		CachedMapView.DebugReason = Session ? CachedMapView.DebugReason : TEXT("No Stargame session.");
		return false;
	}
	return true;
}

void UStargameSystemMapWidget::SetMapPanOffset(FVector2D InPanOffset)
{
	PanOffset = InPanOffset;
	OnMapViewChanged(CachedMapView, PanOffset, Zoom);
}

void UStargameSystemMapWidget::SetMapZoom(double InZoom)
{
	Zoom = FMath::Clamp(InZoom, MinZoom, MaxZoom);
	OnMapViewChanged(CachedMapView, PanOffset, Zoom);
}

void UStargameSystemMapWidget::CalculateContentBounds(const FSystemMapOverviewView& MapView, FVector2D& OutMin, FVector2D& OutMax)
{
	bool bAnyPoint = false;
	auto IncludePoint = [&bAnyPoint, &OutMin, &OutMax](const FVector2D& Point)
	{
		if (!bAnyPoint)
		{
			OutMin = Point;
			OutMax = Point;
			bAnyPoint = true;
			return;
		}
		OutMin.X = FMath::Min(OutMin.X, Point.X);
		OutMin.Y = FMath::Min(OutMin.Y, Point.Y);
		OutMax.X = FMath::Max(OutMax.X, Point.X);
		OutMax.Y = FMath::Max(OutMax.Y, Point.Y);
	};

	for (const FSystemMapEntryViewModel& Entry : MapView.Entries)
	{
		IncludePoint(Entry.MapPosition);
	}
	for (const FSystemMapRouteView& Route : MapView.Routes)
	{
		IncludePoint(Route.SourceMapPosition);
		IncludePoint(Route.DestinationMapPosition);
		IncludePoint(Route.MidpointMapPosition);
	}
	for (const FSystemMapMarkerView& Marker : MapView.Markers)
	{
		IncludePoint(Marker.MapPosition);
	}

	if (!bAnyPoint)
	{
		OutMin = FVector2D(-100.0, -100.0);
		OutMax = FVector2D(100.0, 100.0);
		return;
	}

	const FVector2D Extents = OutMax - OutMin;
	const double Padding = FMath::Max(100.0, FMath::Max(Extents.X, Extents.Y) * 0.12);
	OutMin -= FVector2D(Padding, Padding);
	OutMax += FVector2D(Padding, Padding);
}

FVector2D UStargameSystemMapWidget::ProjectMapPosition(const FVector2D& MapPosition, const FVector2D& LocalSize, const FVector2D& PanOffset, double Zoom, const FVector2D& ContentMin, const FVector2D& ContentMax)
{
	const FVector2D Size = SafeLocalSize(LocalSize);
	const FVector2D ContentSize(
		FMath::Max(1.0, ContentMax.X - ContentMin.X),
		FMath::Max(1.0, ContentMax.Y - ContentMin.Y));
	const double FitScale = FMath::Min(Size.X / ContentSize.X, Size.Y / ContentSize.Y) * 0.84;
	const FVector2D Normalized = MapPosition - ContentMin;
	const FVector2D Fitted = FVector2D(Normalized.X * FitScale, Normalized.Y * FitScale);
	const FVector2D FittedContentSize = ContentSize * FitScale;
	const FVector2D CenteringOffset = (Size - FittedContentSize) * 0.5;
	const FVector2D Center = Size * 0.5;
	return Center + ((CenteringOffset + Fitted) - Center) * FMath::Clamp(Zoom, MinZoom, MaxZoom) + PanOffset;
}

bool UStargameSystemMapWidget::FindNearestSelectableTarget(const FSystemMapOverviewView& MapView, const FVector2D& LocalClickPosition, const FVector2D& LocalSize, const FVector2D& PanOffset, double Zoom, double HitRadiusPixels, FStargameSystemMapHitResult& OutHit)
{
	OutHit = FStargameSystemMapHitResult();
	FVector2D ContentMin;
	FVector2D ContentMax;
	CalculateContentBounds(MapView, ContentMin, ContentMax);
	double BestDistance = FMath::Max(1.0, HitRadiusPixels);

	auto Consider = [&](FName TargetId, FName SourceId, FName MarkerId, FName EntryType, const FVector2D& MapPosition, double RadiusBonus)
	{
		if (TargetId.IsNone())
		{
			return;
		}
		const FVector2D ScreenPosition = ProjectMapPosition(MapPosition, LocalSize, PanOffset, Zoom, ContentMin, ContentMax);
		const double Distance = FVector2D::Distance(LocalClickPosition, ScreenPosition);
		const double EffectiveDistance = Distance - RadiusBonus;
		if (EffectiveDistance <= BestDistance)
		{
			BestDistance = EffectiveDistance;
			OutHit.bHit = true;
			OutHit.TargetId = TargetId;
			OutHit.SourceId = SourceId;
			OutHit.MarkerId = MarkerId;
			OutHit.EntryType = EntryType;
			OutHit.ScreenPosition = ScreenPosition;
			OutHit.DistancePixels = Distance;
		}
	};

	for (const FSystemMapEntryViewModel& Entry : MapView.Entries)
	{
		if (IsSelectableEntryType(Entry.EntryType))
		{
			Consider(Entry.SourceId, Entry.SourceId, NAME_None, Entry.EntryType, Entry.MapPosition, Entry.IconScale * 2.0);
		}
	}

	for (const FSystemMapMarkerView& Marker : MapView.Markers)
	{
		if (IsSelectableMarkerType(Marker.MarkerType))
		{
			Consider(ResolveMarkerSelectableTarget(Marker), Marker.SourceId, Marker.MarkerId, Marker.MarkerType, Marker.MapPosition, 0.0);
		}
	}

	return OutHit.bHit;
}

bool UStargameSystemMapWidget::TrySelectAtLocalPosition(const FVector2D& LocalPosition)
{
	UStargameSessionSubsystem* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (!Session)
	{
		LastStatusText = TEXT("Map selection failed: no Stargame session.");
		OnMapStatusChanged(LastStatusText);
		return false;
	}

	const FVector2D LocalSize = GetCachedGeometry().GetLocalSize();
	FStargameSystemMapHitResult Hit;
	if (!FindNearestSelectableTarget(CachedMapView, LocalPosition, LocalSize, PanOffset, Zoom, DefaultHitRadiusPixels, Hit))
	{
		LastStatusText = TEXT("No selectable map target under cursor.");
		OnMapStatusChanged(LastStatusText);
		return false;
	}

	if (!Session->SelectNavigationTargetById(Hit.TargetId))
	{
		LastStatusText = FString::Printf(TEXT("Map target '%s' is not a valid navigation target."), *Hit.TargetId.ToString());
		OnMapStatusChanged(LastStatusText);
		return false;
	}

	LastStatusText = FString::Printf(TEXT("Selected %s."), *Hit.TargetId.ToString());
	RefreshMapView();
	OnMapStatusChanged(LastStatusText);
	OnMapViewChanged(CachedMapView, PanOffset, Zoom);
	return true;
}

bool UStargameSystemMapWidget::SelectAtLocalPosition(FVector2D LocalPosition)
{
	return TrySelectAtLocalPosition(LocalPosition);
}

FReply UStargameSystemMapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bMapOpen)
	{
		return FReply::Unhandled();
	}

	LastMouseLocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bDragging = true;
		return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget());
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		TrySelectAtLocalPosition(LastMouseLocalPosition);
		return FReply::Handled().SetUserFocus(TakeWidget());
	}
	return FReply::Unhandled();
}

FReply UStargameSystemMapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply UStargameSystemMapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bMapOpen || !bDragging)
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	PanOffset += LocalPosition - LastMouseLocalPosition;
	LastMouseLocalPosition = LocalPosition;
	OnMapViewChanged(CachedMapView, PanOffset, Zoom);
	return FReply::Handled();
}

FReply UStargameSystemMapWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bMapOpen)
	{
		return FReply::Unhandled();
	}

	const double OldZoom = Zoom;
	Zoom = FMath::Clamp(Zoom * (InMouseEvent.GetWheelDelta() > 0.0f ? 1.18 : 1.0 / 1.18), MinZoom, MaxZoom);
	const FVector2D LocalMouse = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	PanOffset = LocalMouse - (LocalMouse - PanOffset) * (Zoom / FMath::Max(OldZoom, UE_SMALL_NUMBER));
	OnMapViewChanged(CachedMapView, PanOffset, Zoom);
	return FReply::Handled();
}

FReply UStargameSystemMapWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::M)
	{
		CloseMap();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
