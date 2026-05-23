#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "GameFramework/Character.h"
#include "StationInteriorPawn.generated.h"

class UCameraComponent;
class APlayerController;
class AStationMissionContactActor;
class AStationInteriorRoomActor;
class AStationInteriorInteractableActor;
class UStargameSessionSubsystem;
class UStargameStationInteractionWidget;

UCLASS()
class STARGAME_API AStationInteriorPawn : public ACharacter
{
	GENERATED_BODY()

public:
	AStationInteriorPawn();

	UFUNCTION(BlueprintCallable, Category = "Station|Interaction")
	void ConfigureStationInteriorPawn(UStargameSessionSubsystem* InSession, AStationInteriorRoomActor* InRoom);

	UFUNCTION(BlueprintPure, Category = "Station|Interaction")
	UStargameSessionSubsystem* GetOwningStationSession() const { return OwningSession.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Station|Combat")
	void SetHostileBoardingEnabled(bool bInEnabled);

	UFUNCTION(BlueprintCallable, Category = "Station|Combat")
	void ApplyCombatProfile(const FStationInteriorCombatProfileDefinition& Profile);

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	bool IsHostileBoardingEnabled() const { return bHostileBoardingEnabled; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	bool IsOnFootAlive() const { return OnFootHealth > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetOnFootHealth() const { return OnFootHealth; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetMaxOnFootHealth() const { return MaxOnFootHealth; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetOnFootWeaponDamage() const { return OnFootWeaponDamage; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetOnFootWeaponRangeCm() const { return OnFootWeaponRangeCm; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetOnFootWeaponCooldownSeconds() const { return OnFootWeaponCooldownSeconds; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetOnFootWeaponCooldownRemainingSeconds() const { return OnFootWeaponCooldownRemainingSeconds; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	bool IsOnFootWeaponReady() const { return bHostileBoardingEnabled && IsOnFootAlive() && OnFootWeaponCooldownRemainingSeconds <= 0.0f; }

	UFUNCTION(BlueprintCallable, Category = "Station|Combat")
	void TakeOnFootDamage(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Station|Combat")
	bool FireOnFootWeapon();

	UFUNCTION(BlueprintCallable, Category = "Station|Interaction")
	bool InteractWithFocusedMissionContact();

	UFUNCTION(BlueprintCallable, Category = "Station|Interaction")
	bool InteractWithFocusedStationObject();

	UFUNCTION(BlueprintCallable, Category = "Station|Interaction")
	bool ExecuteMissionContactCommand(FName GiverNpcId);

	UFUNCTION(BlueprintCallable, Category = "Station|Interaction")
	bool ExecuteStationObjectCommand(FName InteractionType);

	UFUNCTION(BlueprintCallable, Category = "Station|Interaction")
	bool ExecuteMarketCommodityTransaction(FName CommodityId, bool bBuy);

	UFUNCTION(BlueprintCallable, Category = "Station|Interaction")
	void CloseStationInteractionPanel();

	UFUNCTION(BlueprintPure, Category = "Station|Interaction")
	AStationMissionContactActor* FindFocusedMissionContact() const;

	UFUNCTION(BlueprintPure, Category = "Station|Interaction")
	AStationInteriorInteractableActor* FindFocusedStationObject() const;

	UFUNCTION(BlueprintCallable, Category = "Station|Interaction")
	void RefreshFocusedStationActor();

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station|Interaction")
	TSubclassOf<UStargameStationInteractionWidget> StationInteractionWidgetClass;

private:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void RequestExitStation();
	void RequestInteract();
	void RequestPrimaryFire();
	FName MakeInteractionEventId(const TCHAR* Prefix);
	bool ExecuteStationService(FName ServiceType, double Amount);
	bool ExecuteMarketBuy();
	bool ExecuteMissionBoard();
	bool TryLaunchFromInterior();
	bool OpenStationInteractionPanelForContact(AStationMissionContactActor* Contact);
	bool OpenStationInteractionPanelForObject(AStationInteriorInteractableActor* Interactable);
	void SetStationPanelInputLocked(bool bLocked);
	void ClearFocusedStationActors();
	void UpdateMissionContactPrompt(AStationMissionContactActor* Contact) const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, Category = "Station|Interaction")
	float MissionContactInteractionRangeCm = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Station|Interaction")
	float StationObjectInteractionRangeCm = 420.0f;

	UPROPERTY(EditAnywhere, Category = "Station|Combat")
	float MaxOnFootHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, Category = "Station|Combat")
	float OnFootHealth = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Station|Combat")
	float OnFootWeaponDamage = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Station|Combat")
	float OnFootWeaponRangeCm = 6000.0f;

	UPROPERTY(EditAnywhere, Category = "Station|Combat")
	float OnFootWeaponCooldownSeconds = 0.35f;

	UPROPERTY(VisibleAnywhere, Category = "Station|Combat")
	bool bHostileBoardingEnabled = false;

	float OnFootWeaponCooldownRemainingSeconds = 0.0f;

	int32 InteractionSequence = 0;

	UPROPERTY()
	TObjectPtr<UStargameSessionSubsystem> OwningSession;

	UPROPERTY()
	TObjectPtr<AStationInteriorRoomActor> OwningRoom;

	UPROPERTY()
	TObjectPtr<AStationMissionContactActor> FocusedMissionContact;

	UPROPERTY()
	TObjectPtr<AStationInteriorInteractableActor> FocusedStationObject;

	UPROPERTY()
	TObjectPtr<UStargameStationInteractionWidget> StationInteractionPanel;

	TWeakObjectPtr<APlayerController> StationPanelPlayerController;

	bool bStationInteractionPanelOpen = false;
};
