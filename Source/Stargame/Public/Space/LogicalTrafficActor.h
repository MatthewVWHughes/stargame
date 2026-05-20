#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "GameFramework/Actor.h"
#include "LogicalTrafficActor.generated.h"

class USceneComponent;

UCLASS()
class STARGAME_API ALogicalTrafficActor : public AActor
{
	GENERATED_BODY()

public:
	ALogicalTrafficActor();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	void ConfigureTrafficActor(FName InShipInstanceId, FName InGroupId, EShipGoalKind InGoalKind);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	void ConfigureEncounterBehavior(FName InEncounterId, FName InIntentId, FName InIntentType, FName InThreatId, FName InTargetShipId, FName InBehaviorVariantId, FName InCommsVariantId, FName InLocalBehaviorStateId, const FString& InCommsLine);

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	FName GetShipInstanceId() const { return ShipInstanceId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	FName GetGroupId() const { return GroupId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	EShipGoalKind GetGoalKind() const { return GoalKind; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	FName GetEncounterId() const { return EncounterId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	FName GetIntentType() const { return IntentType; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	FName GetBehaviorVariantId() const { return BehaviorVariantId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	FName GetCommsVariantId() const { return CommsVariantId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	FName GetLocalBehaviorStateId() const { return LocalBehaviorStateId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	FString GetCommsLine() const { return CommsLine; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Traffic AI")
	void OnTrafficActorConfigured(FName InShipInstanceId, FName InGroupId, EShipGoalKind InGoalKind);

	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Traffic AI")
	void OnEncounterBehaviorConfigured(FName InEncounterId, FName InIntentId, FName InIntentType, FName InThreatId, FName InTargetShipId, FName InBehaviorVariantId, FName InCommsVariantId, FName InLocalBehaviorStateId, const FString& InCommsLine);

private:
	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FName ShipInstanceId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FName GroupId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	EShipGoalKind GoalKind = EShipGoalKind::None;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FName EncounterId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FName IntentId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FName IntentType;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FName ThreatId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FName TargetShipId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FName BehaviorVariantId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FName CommsVariantId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FName LocalBehaviorStateId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Traffic AI")
	FString CommsLine;
};
