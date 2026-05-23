#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatShotPresentationActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class STARGAME_API ACombatShotPresentationActor : public AActor
{
	GENERATED_BODY()

public:
	ACombatShotPresentationActor();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Combat")
	void ConfigureShotPresentation(FName InShotPresentationId, FName InPresentationType, const FVector& StartPositionCm, const FVector& EndPositionCm, double InStartedAtTimeSeconds, double InDurationSeconds);

	UFUNCTION(BlueprintPure, Category = "Stargame|Combat")
	FName GetShotPresentationId() const { return ShotPresentationId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Combat")
	FName GetPresentationType() const { return PresentationType; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Combat")
	double GetExpiresAtTimeSeconds() const { return StartedAtTimeSeconds + DurationSeconds; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Combat")
	FVector GetStartPositionCm() const { return StoredStartPositionCm; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Combat")
	FVector GetEndPositionCm() const { return StoredEndPositionCm; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Combat")
	void OnShotPresentationConfigured(FName InShotPresentationId, FName InPresentationType, const FVector& StartPositionCm, const FVector& EndPositionCm, double InStartedAtTimeSeconds, double InDurationSeconds);

private:
	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat")
	FName ShotPresentationId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat")
	FName PresentationType;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat")
	double StartedAtTimeSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat")
	double DurationSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat", meta = (Units = "cm"))
	FVector StoredStartPositionCm = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat", meta = (Units = "cm"))
	FVector StoredEndPositionCm = FVector::ZeroVector;
};
