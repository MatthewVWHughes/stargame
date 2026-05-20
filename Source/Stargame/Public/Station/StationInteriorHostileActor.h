#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "GameFramework/Actor.h"
#include "StationInteriorHostileActor.generated.h"

class AStationInteriorPawn;
class AStationInteriorRoomActor;
class UCapsuleComponent;

UCLASS()
class STARGAME_API AStationInteriorHostileActor : public AActor
{
	GENERATED_BODY()

public:
	AStationInteriorHostileActor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Station|Combat")
	void ConfigureHostile(AStationInteriorRoomActor* InRoom, AStationInteriorPawn* InPlayer, FName InHostileId);

	UFUNCTION(BlueprintCallable, Category = "Station|Combat")
	void ApplyCombatProfile(const FStationInteriorCombatProfileDefinition& Profile);

	UFUNCTION(BlueprintCallable, Category = "Station|Combat")
	bool ApplyOnFootDamage(float Amount);

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	bool IsAlive() const { return bAlive; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetFireDamage() const { return FireDamage; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetDetectionRangeCm() const { return DetectionRangeCm; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetFireRangeCm() const { return FireRangeCm; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetFireCooldownSeconds() const { return FireCooldownSeconds; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	float GetFireCooldownRemainingSeconds() const { return FireCooldownRemainingSeconds; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	FName GetHostileId() const { return HostileId; }

	UFUNCTION(BlueprintPure, Category = "Station|Combat")
	FStationInteriorHostileCombatView GetCombatView() const;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Station|Combat")
	void OnHostileConfigured(FName InHostileId);

	UFUNCTION(BlueprintImplementableEvent, Category = "Station|Combat")
	void OnHostileHealthChanged(float InHealth, float InMaxHealth);

	UFUNCTION(BlueprintImplementableEvent, Category = "Station|Combat")
	void OnHostileDied();

private:
	bool HasLineOfSightToPlayer() const;
	void FireAtPlayer();
	void Die();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UCapsuleComponent> CollisionCapsule;

	UPROPERTY(VisibleAnywhere, Category = "Station|Combat")
	FName HostileId;

	UPROPERTY(EditAnywhere, Category = "Station|Combat")
	float MaxHealth = 45.0f;

	UPROPERTY(VisibleAnywhere, Category = "Station|Combat")
	float Health = 45.0f;

	UPROPERTY(EditAnywhere, Category = "Station|Combat")
	float FireDamage = 8.0f;

	UPROPERTY(EditAnywhere, Category = "Station|Combat")
	float DetectionRangeCm = 3500.0f;

	UPROPERTY(EditAnywhere, Category = "Station|Combat")
	float FireRangeCm = 2800.0f;

	UPROPERTY(EditAnywhere, Category = "Station|Combat")
	float FireCooldownSeconds = 1.3f;

	UPROPERTY(EditAnywhere, Category = "Station|Combat")
	float InaccuracyDegrees = 2.5f;

	UPROPERTY(VisibleAnywhere, Category = "Station|Combat")
	bool bAlive = true;

	float FireCooldownRemainingSeconds = 0.4f;

	UPROPERTY()
	TObjectPtr<AStationInteriorPawn> PlayerPawn;

	UPROPERTY()
	TObjectPtr<AStationInteriorRoomActor> OwningRoom;
};
