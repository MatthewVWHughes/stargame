#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StarfieldBackdrop.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

UCLASS()
class STARGAME_API AStarfieldBackdrop : public AActor
{
	GENERATED_BODY()

public:
	AStarfieldBackdrop();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void BuildStars();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UInstancedStaticMeshComponent> Stars;

	UPROPERTY(EditAnywhere, Category = "Starfield", meta = (ClampMin = "16"))
	int32 StarCount = 650;

	UPROPERTY(EditAnywhere, Category = "Starfield", meta = (ClampMin = "100000.0", Units = "cm"))
	float StarfieldRadius = 30000000.0f;

	UPROPERTY(EditAnywhere, Category = "Starfield", meta = (ClampMin = "1.0"))
	float BaseStarScale = 38.0f;

	UPROPERTY(EditAnywhere, Category = "Starfield")
	int32 Seed = 8401;
};
