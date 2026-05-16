#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "StarSystemSubsystem.generated.h"

class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStarSystemBuildComplete);

USTRUCT(BlueprintType)
struct STARGAME_API FActiveSystemEntityEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Space")
	FName EntityId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Space")
	FName EntityType;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Space")
	int32 BuildGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Space")
	TWeakObjectPtr<AActor> Actor;
};

UCLASS()
class STARGAME_API UStarSystemSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	double GetGameTimeSeconds() const { return GameTimeSeconds; }

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	void AdvanceGameTime(double DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	bool BuildSystem(const FStarSystemDefinition& SystemDefinition);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	void TearDownActiveSystem();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	bool SpawnPlayerAtSpawnZone(FName SpawnZoneId, APlayerController* PlayerController);

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FName GetActiveSystemId() const { return ActiveSystemDefinition.SystemId; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool IsSystemBuildComplete() const { return bBuildComplete; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FString GetLastBuildError() const { return LastBuildError; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetNavigationTargets(TArray<FNavigationTargetDefinition>& OutTargets) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredEntityIds(TArray<FName>& OutEntityIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredEntities(TArray<FActiveSystemEntityEntry>& OutEntities) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool FindSpawnZone(FName SpawnZoneId, FSpawnZoneDefinition& OutSpawnZone) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FString GetM0DebugSummary() const;

	UPROPERTY(BlueprintAssignable, Category = "Stargame|Space")
	FStarSystemBuildComplete OnSystemBuildComplete;

private:
	bool ValidateSystemForBuild(const FStarSystemDefinition& SystemDefinition, FString& OutError) const;
	bool RegisterEntity(FName EntityId, FName EntityType, const FTransform& Transform, double VisualRadiusCm);
	bool RegisterNavigationTarget(const FNavigationTargetDefinition& Target);

	double GameTimeSeconds = 0.0;

	UPROPERTY()
	FStarSystemDefinition ActiveSystemDefinition;

	UPROPERTY()
	TMap<FName, FActiveSystemEntityEntry> RegisteredEntities;

	UPROPERTY()
	TMap<FName, FNavigationTargetDefinition> NavigationTargetsById;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedSystemActors;

	int32 ActiveBuildGeneration = 0;
	bool bBuildComplete = false;
	FString LastBuildError;
};
