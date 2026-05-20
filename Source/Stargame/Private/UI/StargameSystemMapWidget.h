#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "StargameSystemMapWidget.generated.h"

USTRUCT(BlueprintType)
struct FStargameSystemMapHitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	bool bHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName TargetId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName SourceId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName MarkerId;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FName EntryType;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	FVector2D ScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "System Map")
	double DistancePixels = 0.0;
};

UCLASS()
class UStargameSystemMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Stargame|System Map")
	void OpenMap();

	UFUNCTION(BlueprintCallable, Category = "Stargame|System Map")
	void CloseMap();

	UFUNCTION(BlueprintCallable, Category = "Stargame|System Map")
	void ToggleMap();

	UFUNCTION(BlueprintPure, Category = "Stargame|System Map")
	bool IsMapOpen() const { return bMapOpen; }

	UFUNCTION(BlueprintPure, Category = "Stargame|System Map")
	FString GetLastStatusText() const { return LastStatusText; }

	UFUNCTION(BlueprintPure, Category = "Stargame|System Map")
	FSystemMapOverviewView GetCachedMapView() const { return CachedMapView; }

	UFUNCTION(BlueprintPure, Category = "Stargame|System Map")
	FVector2D GetMapPanOffset() const { return PanOffset; }

	UFUNCTION(BlueprintPure, Category = "Stargame|System Map")
	double GetMapZoom() const { return Zoom; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|System Map")
	void SetMapPanOffset(FVector2D InPanOffset);

	UFUNCTION(BlueprintCallable, Category = "Stargame|System Map")
	void SetMapZoom(double InZoom);

	UFUNCTION(BlueprintCallable, Category = "Stargame|System Map")
	bool SelectAtLocalPosition(FVector2D LocalPosition);

	static void CalculateContentBounds(const FSystemMapOverviewView& MapView, FVector2D& OutMin, FVector2D& OutMax);
	static FVector2D ProjectMapPosition(const FVector2D& MapPosition, const FVector2D& LocalSize, const FVector2D& PanOffset, double Zoom, const FVector2D& ContentMin, const FVector2D& ContentMax);
	static bool FindNearestSelectableTarget(const FSystemMapOverviewView& MapView, const FVector2D& LocalClickPosition, const FVector2D& LocalSize, const FVector2D& PanOffset, double Zoom, double HitRadiusPixels, FStargameSystemMapHitResult& OutHit);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|System Map")
	void OnMapOpened(const FSystemMapOverviewView& MapView);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|System Map")
	void OnMapClosed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|System Map")
	void OnMapViewChanged(const FSystemMapOverviewView& MapView, FVector2D InPanOffset, double InZoom);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|System Map")
	void OnMapStatusChanged(const FString& InStatusText);

private:
	bool RefreshMapView();
	bool TrySelectAtLocalPosition(const FVector2D& LocalPosition);

	UPROPERTY()
	bool bMapOpen = false;

	UPROPERTY()
	FSystemMapOverviewView CachedMapView;

	FVector2D PanOffset = FVector2D::ZeroVector;
	double Zoom = 1.0;
	bool bDragging = false;
	FVector2D LastMouseLocalPosition = FVector2D::ZeroVector;
	FString LastStatusText;
};
