#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "GameFramework/Actor.h"
#include "StationInteriorRoomActor.generated.h"

class AStationMissionContactActor;
class AStationInteriorInteractableActor;
class AStationInteriorHostileActor;
class AStationInteriorPawn;
class UStaticMeshComponent;
class UStargameSessionSubsystem;

UCLASS()
class STARGAME_API AStationInteriorRoomActor : public AActor
{
	GENERATED_BODY()

public:
	AStationInteriorRoomActor();

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	FTransform GetPlayerStartTransform() const;

	UFUNCTION(BlueprintCallable, Category = "Station|Interior")
	void ConfigureStationInterior(FName InStationId, const TArray<FStationQuestGiverDefinition>& InQuestGivers, bool bInHostileBoarding = false, UStargameSessionSubsystem* InSession = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Station|Interior")
	void ArmHostileBoarding(AStationInteriorPawn* PlayerPawn);

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	int32 GetMissionContactCount() const { return MissionContactActors.Num(); }

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	int32 GetInteractableCount() const { return InteractableActors.Num(); }

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	int32 GetHostileCount() const { return HostileActors.Num(); }

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	int32 GetLiveHostileCount() const;

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	bool IsHostileBoarding() const { return bHostileBoarding; }

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	FName GetStationId() const { return StationId; }

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	FName GetCombatProfileId() const { return CombatProfileId; }

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	bool AreHostilesCleared() const;

	UFUNCTION(BlueprintCallable, Category = "Station|Interior")
	void NotifyHostileKilled(AStationInteriorHostileActor* Hostile);

	UFUNCTION(BlueprintCallable, Category = "Station|Interior")
	void RegisterMissionContact(AStationMissionContactActor* Contact);

	UFUNCTION(BlueprintCallable, Category = "Station|Interior")
	void RegisterInteractable(AStationInteriorInteractableActor* Interactable);

	UFUNCTION(BlueprintCallable, Category = "Station|Interior")
	void RegisterHostile(AStationInteriorHostileActor* Hostile);

	UFUNCTION(BlueprintImplementableEvent, Category = "Station|Interior")
	void OnStationInteriorConfigured(FName InStationId, const TArray<FStationQuestGiverDefinition>& InQuestGivers, bool bInHostileBoarding);

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	void BuildHostileCombatViews(TArray<FStationInteriorHostileCombatView>& OutHostiles) const;

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	AStationMissionContactActor* FindMissionContact(FName NpcId) const;

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	AStationMissionContactActor* FindNearestMissionContact(const FVector& WorldLocation, float MaxDistanceCm) const;

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	AStationInteriorInteractableActor* FindNearestInteractable(const FVector& WorldLocation, float MaxDistanceCm) const;

	UFUNCTION(BlueprintPure, Category = "Station|Interior")
	AStationInteriorHostileActor* FindNearestHostile(const FVector& WorldLocation, float MaxDistanceCm) const;

private:
	void ClearMissionContacts();
	void ClearInteractables();
	void ClearHostiles();
	void RegisterAuthoredInteractables();
	void SpawnDefaultInteractable(FName InteractionType, const FText& DisplayName, const FVector& LocalPositionCm);
	void ConfigureOrSpawnDefaultInteractable(FName InteractionType, const FText& DisplayName, const FVector& LocalPositionCm);
	void SpawnDefaultHostiles(AStationInteriorPawn* PlayerPawn, const FStationInteriorCombatProfileDefinition& CombatProfile);

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> FloorMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CeilingMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> NorthWallMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> SouthWallMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> EastWallMesh;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WestWallMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station|Interior", meta = (AllowPrivateAccess = "true"))
	FTransform PlayerStartLocalTransform = FTransform(FRotator::ZeroRotator, FVector(-350.0, 0.0, 100.0));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station|Interior", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AStationMissionContactActor> MissionContactActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station|Interior", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AStationInteriorInteractableActor> InteractableActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station|Interior", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AStationInteriorHostileActor> HostileActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station|Interior", meta = (AllowPrivateAccess = "true"))
	TArray<FTransform> HostileSpawnLocalTransforms;

	UPROPERTY(VisibleAnywhere, Category = "Station|Interior")
	FName StationId;

	UPROPERTY(VisibleAnywhere, Category = "Station|Interior")
	bool bHostileBoarding = false;

	UPROPERTY(VisibleAnywhere, Category = "Station|Interior")
	FName CombatProfileId = TEXT("profile_godot_hostile_boarding_basic");

	UPROPERTY()
	TObjectPtr<UStargameSessionSubsystem> OwningSession;

	UPROPERTY(VisibleAnywhere, Category = "Station|Interior")
	TArray<TObjectPtr<AStationMissionContactActor>> MissionContactActors;

	UPROPERTY(VisibleAnywhere, Category = "Station|Interior")
	TArray<TObjectPtr<AStationInteriorInteractableActor>> InteractableActors;

	UPROPERTY(VisibleAnywhere, Category = "Station|Interior")
	TArray<TObjectPtr<AStationInteriorHostileActor>> HostileActors;
};
