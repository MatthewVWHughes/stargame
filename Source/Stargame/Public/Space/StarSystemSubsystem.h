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

USTRUCT(BlueprintType)
struct STARGAME_API FDockingPortRegistryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Space")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Space")
	FName PortId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Space")
	int32 BuildGeneration = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Space")
	FDockingPortDefinition Definition;
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
	FStargameScaleContract GetActiveScaleContract() const { return ActiveSystemDefinition.Scale; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool IsSystemBuildComplete() const { return bBuildComplete; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FString GetLastBuildError() const { return LastBuildError; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetNavigationTargets(TArray<FNavigationTargetDefinition>& OutTargets) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool FindNavigationTarget(FName TargetId, FNavigationTargetDefinition& OutTarget) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool BuildNavigationTargetViewModel(FName TargetId, FName SelectedTargetId, const FVector& ObserverSystemPositionCm, const FVector& ObserverVelocityCmPerSec, double SimulationTimeSeconds, FNavigationTargetViewModel& OutViewModel) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void BuildNavigationTargetViewModels(FName SelectedTargetId, const FVector& ObserverSystemPositionCm, const FVector& ObserverVelocityCmPerSec, double SimulationTimeSeconds, TArray<FNavigationTargetViewModel>& OutViewModels) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredEntityIds(TArray<FName>& OutEntityIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredEntities(TArray<FActiveSystemEntityEntry>& OutEntities) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredSpawnZoneIds(TArray<FName>& OutSpawnZoneIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredGravityWellIds(TArray<FName>& OutGravityWellIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredDockingPortIds(TArray<FName>& OutDockingPortIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredDockingPorts(TArray<FDockingPortRegistryEntry>& OutDockingPorts) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredMapEntryIds(TArray<FName>& OutMapEntryIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool FindSpawnZone(FName SpawnZoneId, FSpawnZoneDefinition& OutSpawnZone) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FString GetM0DebugSummary() const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FString GetM1DebugSummary() const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FString GetM3DebugSummary(FName SelectedTargetId, const FVector& ObserverSystemPositionCm, const FVector& ObserverVelocityCmPerSec, double SimulationTimeSeconds) const;

	UPROPERTY(BlueprintAssignable, Category = "Stargame|Space")
	FStarSystemBuildComplete OnSystemBuildComplete;

private:
	bool ValidateSystemForBuild(const FStarSystemDefinition& SystemDefinition, FString& OutError) const;
	bool RegisterEntity(FName EntityId, FName EntityType, const FTransform& Transform, double VisualRadiusCm);
	bool RegisterNavigationTarget(const FNavigationTargetDefinition& Target);
	bool RegisterSpawnZone(const FSpawnZoneDefinition& SpawnZone);
	bool RegisterDockingPort(FName StationId, const FDockingPortDefinition& DockingPort);
	bool RegisterGravityWell(const FGravityWellDefinition& GravityWell);
	bool RegisterMapEntry(const FMapEntryDefinition& MapEntry);
	static FName MakeDockingPortRegistryId(FName StationId, FName PortId);

	double GameTimeSeconds = 0.0;

	UPROPERTY()
	FStarSystemDefinition ActiveSystemDefinition;

	UPROPERTY()
	TMap<FName, FActiveSystemEntityEntry> RegisteredEntities;

	UPROPERTY()
	TMap<FName, FNavigationTargetDefinition> NavigationTargetsById;

	UPROPERTY()
	TMap<FName, FSpawnZoneDefinition> SpawnZonesById;

	UPROPERTY()
	TMap<FName, FDockingPortRegistryEntry> DockingPortsById;

	UPROPERTY()
	TMap<FName, FGravityWellDefinition> GravityWellsById;

	UPROPERTY()
	TMap<FName, FMapEntryDefinition> MapEntriesById;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedSystemActors;

	int32 ActiveBuildGeneration = 0;
	bool bBuildComplete = false;
	FString LastBuildError;
};
