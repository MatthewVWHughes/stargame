#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GasGiantCloudField.generated.h"

class UMaterialInterface;
class UInstancedStaticMeshComponent;
class USceneComponent;

UCLASS(Blueprintable)
class STARGAME_API AGasGiantCloudField : public AActor
{
	GENERATED_BODY()

public:
	AGasGiantCloudField();

	UFUNCTION(BlueprintCallable, Category = "Gas Giant|Cloud Field")
	void RebuildCloudLayer();

	UFUNCTION(BlueprintPure, Category = "Gas Giant|Damage")
	float ComputeCrushDepthAlpha(FVector WorldLocation) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> CloudRoot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> CloudPuffs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Local Cloud Layer", meta = (AllowPrivateAccess = "true", ClampMin = "1000.0", Units = "cm"))
	float PlanetRadius = 5000000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Local Cloud Layer", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float DeepLayerBottomAltitude = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Local Cloud Layer", meta = (AllowPrivateAccess = "true", ClampMin = "1000.0", Units = "cm"))
	float DeepLayerHeight = 75000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Local Cloud Layer", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
	int32 CloudPuffCount = 900;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Local Cloud Layer", meta = (AllowPrivateAccess = "true", ClampMin = "100.0", Units = "cm"))
	float MinPuffRadius = 14000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Local Cloud Layer", meta = (AllowPrivateAccess = "true", ClampMin = "100.0", Units = "cm"))
	float MaxPuffRadius = 52000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Local Cloud Layer", meta = (AllowPrivateAccess = "true"))
	FLinearColor CloudColor = FLinearColor(0.50f, 0.28f, 0.11f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Local Cloud Layer", meta = (AllowPrivateAccess = "true"))
	int32 RandomSeed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float CrushDepthStartRadius = 5000000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Damage", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "cm"))
	float CrushDepthFatalRadius = 4985000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Motion", meta = (AllowPrivateAccess = "true"))
	float DriftDegreesPerSecond = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gas Giant|Debug", meta = (AllowPrivateAccess = "true"))
	bool bRebuildEveryConstruction = true;
};
