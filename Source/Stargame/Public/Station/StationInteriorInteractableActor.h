#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StationInteriorInteractableActor.generated.h"

class UCapsuleComponent;
class UStaticMeshComponent;

UCLASS()
class STARGAME_API AStationInteriorInteractableActor : public AActor
{
	GENERATED_BODY()

public:
	AStationInteriorInteractableActor();

	UFUNCTION(BlueprintCallable, Category = "Station|Interior")
	void ConfigureInteractable(FName InStationId, FName InInteractionType, const FText& InDisplayName);

	UFUNCTION(BlueprintCallable, Category = "Station|Interior")
	void ConfigureInteractableFromDefaults(FName InStationId);

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	FName GetStationId() const { return StationId; }

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	FName GetInteractionType() const { return InteractionType; }

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintCallable, Category = "Station|Interior")
	void SetFocusedByPlayer(bool bInFocused);

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	bool IsFocusedByPlayer() const { return bFocusedByPlayer; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Station|Interior")
	void OnInteractableConfigured(FName InStationId, FName InInteractionType, const FText& InDisplayName);

	UFUNCTION(BlueprintImplementableEvent, Category = "Station|Interior")
	void OnFocusedByPlayerChanged(bool bInFocused);

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCapsuleComponent> InteractionArea;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ServiceMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station|Interior", meta = (AllowPrivateAccess = "true"))
	FName DefaultInteractionType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station|Interior", meta = (AllowPrivateAccess = "true"))
	FText DefaultDisplayName;

	UPROPERTY(VisibleAnywhere, Category = "Station|Interior")
	FName StationId;

	UPROPERTY(VisibleAnywhere, Category = "Station|Interior")
	FName InteractionType;

	UPROPERTY(VisibleAnywhere, Category = "Station|Interior")
	FText DisplayName;

	UPROPERTY(VisibleAnywhere, Category = "Station|Interior")
	bool bFocusedByPlayer = false;
};
