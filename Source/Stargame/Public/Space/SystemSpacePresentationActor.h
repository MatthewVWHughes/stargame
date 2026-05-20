#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SystemSpacePresentationActor.generated.h"

class USceneComponent;

UCLASS()
class STARGAME_API ASystemSpacePresentationActor : public AActor
{
	GENERATED_BODY()

public:
	ASystemSpacePresentationActor();

	void ConfigureForSystem(FName InSystemId);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Stargame|Space")
	void OnConfiguredForSystem(FName InSystemId);

private:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Stargame|Space")
	FName SystemId;
};
