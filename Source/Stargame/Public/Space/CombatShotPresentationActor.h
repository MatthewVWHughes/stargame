#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatShotPresentationActor.generated.h"

class USceneComponent;

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

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Combat")
	void OnShotPresentationConfigured(FName InShotPresentationId, FName InPresentationType, const FVector& StartPositionCm, const FVector& EndPositionCm, double InStartedAtTimeSeconds, double InDurationSeconds);

private:
	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat")
	FName ShotPresentationId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat")
	FName PresentationType;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat")
	double StartedAtTimeSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Combat")
	double DurationSeconds = 0.0;
};
