#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/StargameDataTypes.h"
#include "StargameCommsWidget.generated.h"

class ASpaceFlightPawn;
class UStarSystemSubsystem;
class UStargameSessionSubsystem;
class UButton;
class UTextBlock;

USTRUCT(BlueprintType)
struct FStargameCommsView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Comms")
	bool bHasSelectedTarget = false;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Comms")
	bool bStationSelected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Comms")
	bool bCanRequestDocking = false;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Comms")
	FName TargetId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Comms")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Comms")
	FName PortId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Comms")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Comms")
	double DistanceMeters = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Comms")
	FString BodyText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Comms")
	FString StatusText;
};

UCLASS()
class UStargameCommsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Stargame|Comms")
	void OpenComms();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Comms")
	void CloseComms();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Comms")
	void ToggleComms();

	UFUNCTION(BlueprintPure, Category = "Stargame|Comms")
	bool IsCommsOpen() const { return bCommsOpen; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Comms")
	FStargameCommsView GetCachedCommsView() const { return CachedView; }

	static FStargameCommsView BuildCommsView(const UWorld* World, const ASpaceFlightPawn* FlightPawn, const FString& LastStatus = FString());
	static FStargameCommsView BuildCommsView(const UStargameSessionSubsystem* Session, const UStarSystemSubsystem* StarSystem, const ASpaceFlightPawn* FlightPawn, const FString& LastStatus = FString());

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Comms")
	void OnCommsOpened(const FStargameCommsView& View);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Comms")
	void OnCommsClosed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Comms")
	void OnCommsViewChanged(const FStargameCommsView& View);

private:
	UFUNCTION()
	void HandleRequestDockingClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void RefreshComms();
	bool ExecuteDockingRequest();
	void SetOwningPlayerInputLocked(bool bLocked);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RequestDockingButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CloseButton;

	FStargameCommsView CachedView;
	FString LastStatusText;
	bool bCommsOpen = false;
};
