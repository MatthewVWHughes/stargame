#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "M0SystemMarkerActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class STARGAME_API AM0SystemMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	AM0SystemMarkerActor();

	void InitializeMarker(FName InGameplayId, FName InEntityType, double VisualRadiusCm);

	UFUNCTION(BlueprintPure, Category = "Stargame|M0")
	FName GetGameplayId() const { return GameplayId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|M0")
	FName GetEntityType() const { return EntityType; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> MarkerMesh;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|M0")
	FName GameplayId;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|M0")
	FName EntityType;
};
