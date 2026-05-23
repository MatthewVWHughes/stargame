#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "StargamePauseMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class STextBlock;

enum class EStargamePauseMenuPage : uint8
{
	Main,
	Settings
};

UENUM(BlueprintType)
enum class EStargamePauseMenuTab : uint8
{
	Inventory,
	Missions,
	Settings
};

USTRUCT(BlueprintType)
struct FStargamePauseMenuView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Pause")
	FString InventoryText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Pause")
	FString MissionsText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Pause")
	FString SettingsText;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Pause")
	int32 PersonalInventoryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Pause")
	int32 ShipCargoCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Pause")
	int32 ActiveMissionCount = 0;
};

UCLASS()
class UStargamePauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Stargame|Pause")
	void OpenPauseMenu(EStargamePauseMenuTab Tab = EStargamePauseMenuTab::Inventory);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Pause")
	void ClosePauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Pause")
	void TogglePauseMenu(EStargamePauseMenuTab Tab = EStargamePauseMenuTab::Inventory);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Pause")
	void ExitGame();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Pause")
	void SetActiveTab(EStargamePauseMenuTab Tab);

	UFUNCTION(BlueprintPure, Category = "Stargame|Pause")
	bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Pause")
	EStargamePauseMenuTab GetActiveTab() const { return ActiveTab; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Pause")
	FStargamePauseMenuView GetCachedPauseMenuView() const { return CachedView; }

	static FStargamePauseMenuView BuildPauseMenuView(const UStargameSessionSubsystem* Session);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Pause")
	void OnPauseMenuOpened(EStargamePauseMenuTab Tab, const FStargamePauseMenuView& View);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Pause")
	void OnPauseMenuClosed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Pause")
	void OnActiveTabChanged(EStargamePauseMenuTab Tab, const FStargamePauseMenuView& View);

private:
	UFUNCTION()
	void HandleInventoryTabClicked();

	UFUNCTION()
	void HandleMissionsTabClicked();

	UFUNCTION()
	void HandleSettingsTabClicked();

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleExitClicked();

	UStargameSessionSubsystem* ResolveSession() const;
	bool IsSaveAllowed() const;
	bool IsLoadAllowed() const;
	FString BuildMainPanelText(const UStargameSessionSubsystem* Session) const;
	FString BuildSettingsPanelText(const UStargameSessionSubsystem* Session) const;
	void SetMenuStatus(const FText& StatusText);
	void ShowMainPage();
	void ShowSettingsPage();
	void HandleSaveClicked();
	void HandleLoadClicked();
	void HandleNewGameClicked();
	void ApplySettingsMenuChrome();
	void EnsureRuntimeExitButton();
	void RefreshPauseMenu();
	void SetOwningPlayerInputLocked(bool bLocked);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> FooterText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> InventoryTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> MissionsTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SettingsTabButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ExitButton;

	FStargamePauseMenuView CachedView;
	EStargamePauseMenuTab ActiveTab = EStargamePauseMenuTab::Inventory;
	EStargamePauseMenuPage ActivePage = EStargamePauseMenuPage::Main;
	TSharedPtr<STextBlock> NativeTitleText;
	TSharedPtr<STextBlock> NativeContextText;
	TSharedPtr<STextBlock> NativeBodyText;
	TSharedPtr<STextBlock> NativeStatusText;
	bool bPauseMenuOpen = false;
};
