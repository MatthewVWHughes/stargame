#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StargameBootMenuWidget.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class UStargameBootMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Stargame|Boot")
	void RefreshContinueState();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Boot")
	void NewGame();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Boot")
	void Continue();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Boot")
	void Quit();

protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Boot")
	void OnStatusChanged(const FText& Status, bool bIsError);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Boot")
	void OnContinueAvailabilityChanged(bool bCanContinue);

private:
	UFUNCTION()
	void HandleNewGameClicked();

	UFUNCTION()
	void HandleContinueClicked();

	UFUNCTION()
	void HandleQuitClicked();

	void FinishBootFlow();
	void SetStatusText(const FString& Status, bool bIsError = false);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> NewGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StatusText;
};
