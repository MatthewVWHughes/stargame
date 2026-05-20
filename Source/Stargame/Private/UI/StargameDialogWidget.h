#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "StargameDialogWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UStargameDialogChoiceAction;

USTRUCT(BlueprintType)
struct FStargameDialogChoiceView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Stargame|Dialog")
	FName ChoiceId;

	UPROPERTY(BlueprintReadWrite, Category = "Stargame|Dialog")
	FText Label;

	UPROPERTY(BlueprintReadWrite, Category = "Stargame|Dialog")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadWrite, Category = "Stargame|Dialog")
	bool bCloseAfter = true;
};

UCLASS()
class UStargameDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Stargame|Dialog")
	void ShowDialog(const FText& InTitle, const FString& InBody, const TArray<FStargameDialogChoiceView>& InChoices);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Dialog")
	void ShowMissionContact(const FDockedMissionContactOption& Contact);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Dialog")
	void CloseDialog();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Dialog")
	void SelectChoice(FName ChoiceId, bool bCloseAfter = true);

	UFUNCTION(BlueprintPure, Category = "Stargame|Dialog")
	bool IsDialogOpen() const { return bDialogOpen; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Dialog")
	FName GetLastSelectedChoiceId() const { return LastSelectedChoiceId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Dialog")
	FString GetLastStatusText() const { return LastStatusText; }

	static FString BuildMissionContactBody(const FDockedMissionContactOption& Contact);
	static TArray<FStargameDialogChoiceView> BuildMissionContactChoices(const FDockedMissionContactOption& Contact);

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Dialog")
	void OnDialogShown(const FText& InTitle, const FString& InBody, const TArray<FStargameDialogChoiceView>& InChoices);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Dialog")
	void OnDialogClosed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Dialog")
	void OnDialogStatusChanged(const FString& InStatusText, bool bIsError);

private:
	void HandleChoiceSelected(FName ChoiceId, bool bCloseAfter);
	bool ExecuteMissionChoice(FName ChoiceId);
	void SetOwningPlayerInputLocked(bool bLocked);
	FName MakeDialogEventId(const TCHAR* Prefix);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StatusText;

	FDockedMissionContactOption ActiveMissionContact;
	bool bHasActiveMissionContact = false;
	bool bDialogOpen = false;
	int32 DialogSequence = 0;
	FName LastSelectedChoiceId;
	FString LastStatusText;
};

UCLASS()
class UStargameDialogChoiceAction : public UObject
{
	GENERATED_BODY()

public:
	void Configure(UStargameDialogWidget* InOwner, FName InChoiceId, bool bInCloseAfter);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY()
	TObjectPtr<UStargameDialogWidget> Owner;

	FName ChoiceId;
	bool bCloseAfter = true;
};
