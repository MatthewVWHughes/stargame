#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StargameGameModeBase.generated.h"

class ADirectionalLight;
class ASkyAtmosphere;
class AGasGiantPlanetActor;
class AGasGiantLocalFogActor;
class AGasGiantVolumetricCloudActor;
class AStarfieldBackdrop;

UCLASS()
class STARGAME_API AStargameGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AStargameGameModeBase();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	bool bSpawnPrototypeGasGiantScene = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AGasGiantPlanetActor> PrototypeGasGiantClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AGasGiantLocalFogActor> PrototypeLocalFogClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	bool bSpawnPrototypeLocalFog = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AGasGiantVolumetricCloudActor> PrototypeVolumetricCloudClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	bool bSpawnPrototypeVolumetricCloud = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	FVector PrototypeGasGiantLocation = FVector(8000000.0, 0.0, 0.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ADirectionalLight> PrototypeSunLightClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ASkyAtmosphere> PrototypeSkyAtmosphereClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	FRotator PrototypeSunLightRotation = FRotator(-18.0, -34.0, 0.0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float PrototypeSunLightIntensity = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	FLinearColor PrototypeSunLightColor = FLinearColor(1.0f, 0.96f, 0.88f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stargame|Prototype", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AStarfieldBackdrop> PrototypeStarfieldClass;
};
