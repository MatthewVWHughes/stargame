#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StargameStationInteractionWidget.generated.h"

class AStationInteriorPawn;
class UButton;
class UTextBlock;
class UStargameStationMarketAction;
struct FDockedMissionContactOption;

USTRUCT(BlueprintType)
struct FStargameStationInteractionActionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Station")
	FName ActionId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Station")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Station")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Station")
	FName CommodityId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Station")
	bool bBuy = true;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Station")
	FName GiverNpcId;
};

UCLASS()
class UStargameStationInteractionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	void ShowStationObject(AStationInteriorPawn* InStationPawn, FName InInteractionType, const FText& InDisplayName);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	void ShowMissionContact(AStationInteriorPawn* InStationPawn, FName InGiverNpcId, const FText& InDisplayName);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	void ExecutePrimaryAction();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	void ExecuteMarketAction(FName CommodityId, bool bBuy);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	void ExecuteAction(const FStargameStationInteractionActionView& Action);

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	const TArray<FStargameStationInteractionActionView>& GetCurrentActions() const { return CurrentActions; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Station")
	void ClosePanel();

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Station")
	void OnStationInteractionChanged(const FText& InTitle, const FString& InBody, const FString& InStatus, bool bStatusIsError, const TArray<FStargameStationInteractionActionView>& Actions);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Station")
	void OnStationInteractionClosed();

private:
	friend class UStargameStationMarketAction;

	UFUNCTION()
	void HandlePrimaryClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void ClearActionButtons();
	void AddActionButton(const FText& Label, bool bEnabled);
	void AddMarketActionButton(const FText& Label, bool bEnabled, FName CommodityId, bool bBuy);
	void AddMissionContactActionButton(const FDockedMissionContactOption& Contact);
	void SetPanelText(const FText& InTitle, const FString& InBody, const FString& InStatus = FString(), bool bStatusIsError = false);
	void RefreshStationObject();
	void RefreshMissionContact();

	UPROPERTY()
	TObjectPtr<AStationInteriorPawn> StationPawn;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PrimaryButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> CloseButton;

	UPROPERTY()
	TArray<TObjectPtr<UStargameStationMarketAction>> MarketActions;

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TArray<FStargameStationInteractionActionView> CurrentActions;

	FName InteractionType;
	FName GiverNpcId;
	bool bShowingMissionContact = false;
};

UCLASS()
class UStargameStationMarketAction : public UObject
{
	GENERATED_BODY()

public:
	void Configure(UStargameStationInteractionWidget* InOwner, FName InCommodityId, bool bInBuy);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY()
	TObjectPtr<UStargameStationInteractionWidget> Owner;

	FName CommodityId;
	bool bBuy = true;
};
