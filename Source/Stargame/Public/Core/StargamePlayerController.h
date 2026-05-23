#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "StargamePlayerController.generated.h"

class UStargameSystemMapWidget;
class UStargameCommsWidget;
class UStargameFlightHUDWidget;
class UStargamePauseMenuWidget;
class UUserWidget;

UCLASS()
class STARGAME_API AStargamePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AStargamePlayerController();

	UFUNCTION(Exec)
	void NewGame();

	UFUNCTION(Exec)
	void ContinueGame();

	UFUNCTION(Exec)
	void SaveGame();

	UFUNCTION(Exec)
	void EnterStation();

	UFUNCTION(Exec)
	void StationRepair();

	UFUNCTION(Exec)
	void StationRefuel();

	UFUNCTION(Exec)
	void StationBuyOre();

	UFUNCTION(Exec)
	void StationAcceptMission();

	UFUNCTION(Exec)
	void StationCompleteMission();

	UFUNCTION(Exec)
	void StationUndock();

	UFUNCTION(Exec)
	void EncounterEscape();

	UFUNCTION(Exec)
	void EncounterSurrender();

	UFUNCTION(Exec)
	void EncounterPatrolResponse();

	UFUNCTION(Exec)
	void EncounterFireWeapon();

	UFUNCTION(Exec)
	void ToggleSystemMap();

	UFUNCTION(Exec)
	void ToggleComms();

	UFUNCTION(Exec)
	void TogglePauseMenu();

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	bool HasLastDockedStationCommandResult() const { return bHasLastDockedStationCommandResult; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Station")
	FDockedStationCommandResult GetLastDockedStationCommandResult() const { return LastDockedStationCommandResult; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|UI")
	TSubclassOf<UStargameSystemMapWidget> SystemMapWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|UI")
	TSubclassOf<UStargameCommsWidget> CommsWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|UI")
	TSubclassOf<UStargamePauseMenuWidget> PauseMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|UI")
	TSubclassOf<UStargameFlightHUDWidget> FlightHUDWidgetClass;

private:
	void RecordDockedStationCommandResult(const FDockedStationCommandResult& Result);
	bool AllowDebugStationHotkey(const TCHAR* CommandName);
	void ExecuteDockedServiceByType(FName ServiceType, double Amount);
	FName MakeDockedCommandId(const TCHAR* Prefix);
	void ApplyEncounterOutcome(FName OutcomeType);
	void SyncFlightHUDForPossessedPawn();

	UPROPERTY()
	TObjectPtr<UStargameSystemMapWidget> SystemMapWidget;

	UPROPERTY()
	TObjectPtr<UStargameCommsWidget> CommsWidget;

	UPROPERTY()
	TObjectPtr<UStargamePauseMenuWidget> PauseMenuWidget;

	UPROPERTY()
	TObjectPtr<UStargameFlightHUDWidget> FlightHUDWidget;

	int32 DockedCommandSequence = 0;

	UPROPERTY()
	bool bHasLastDockedStationCommandResult = false;

	UPROPERTY()
	FDockedStationCommandResult LastDockedStationCommandResult;
};
