#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "StarSystemSubsystem.generated.h"

class APlayerController;
class APawn;
class ACombatShotPresentationActor;
class ALogicalTrafficActor;
class AM0SystemMarkerActor;
class ASystemSpacePresentationActor;

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

USTRUCT(BlueprintType)
struct STARGAME_API FRealizedTrafficActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Traffic AI")
	FName ShipInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Traffic AI")
	FName GroupId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Traffic AI")
	EShipGoalKind GoalKind = EShipGoalKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Traffic AI")
	FName RouteSegmentId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Traffic AI")
	double RouteProgress01 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Traffic AI")
	TWeakObjectPtr<ALogicalTrafficActor> Actor;
};

USTRUCT(BlueprintType)
struct STARGAME_API FRealizedEncounterActorEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName ActorId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName EncounterId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName HazardId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName PatrolReservationId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	EShipGoalKind Role = EShipGoalKind::None;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName IntentId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName IntentType;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName ThreatId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName TargetShipId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName BehaviorVariantId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName CommsVariantId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName LocalBehaviorStateId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FString CommsLine;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FName RouteSegmentId;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	double RouteProgress01 = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FRouteSample RouteSample;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	FRouteSample TargetRouteSample;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter", meta = (Units = "cm"))
	FVector DesiredPositionCm = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter", meta = (Units = "cm"))
	double DistanceToDesiredCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter", meta = (Units = "cm"))
	double DistanceTrendCm = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	bool bSteeringActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	double LastSteeringSimulationTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Stargame|Encounter")
	TWeakObjectPtr<ALogicalTrafficActor> Actor;
};

UCLASS()
class STARGAME_API UStarSystemSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	bool BuildSystem(const FStarSystemDefinition& SystemDefinition, double InitialSimulationTimeSeconds = 0.0);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	void TearDownActiveSystem();

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	bool SpawnPlayerAtSpawnZone(FName SpawnZoneId, APlayerController* PlayerController, TSubclassOf<APawn> PawnClass = nullptr);

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	APawn* GetActivePlayerPawn() const { return ActivePlayerPawn.Get(); }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FName GetActiveSystemId() const { return ActiveSystemDefinition.SystemId; }

	const FStarSystemDefinition& GetActiveSystemDefinition() const { return ActiveSystemDefinition; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FStargameScaleContract GetActiveScaleContract() const { return ActiveSystemDefinition.Scale; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool IsSystemBuildComplete() const { return bBuildComplete; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FString GetLastBuildError() const { return LastBuildError; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FStargameValidationReport GetLastBuildValidationReport() const { return LastBuildValidationReport; }

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetNavigationTargets(TArray<FNavigationTargetDefinition>& OutTargets) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool FindNavigationTarget(FName TargetId, FNavigationTargetDefinition& OutTarget) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool BuildNavigationTargetViewModel(FName TargetId, FName SelectedTargetId, const FVector& ObserverSystemPositionCm, const FVector& ObserverVelocityCmPerSec, double SimulationTimeSeconds, FNavigationTargetViewModel& OutViewModel) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void BuildNavigationTargetViewModels(FName SelectedTargetId, const FVector& ObserverSystemPositionCm, const FVector& ObserverVelocityCmPerSec, double SimulationTimeSeconds, TArray<FNavigationTargetViewModel>& OutViewModels) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool QueryNearestGravityWell(const FVector& ShipSystemPositionCm, const FVector& ShipSystemVelocityCmPerSec, EShipFlightMode FlightMode, double SimulationTimeSeconds, FGravityWellQueryResult& OutResult) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool BuildSupercruiseTargetTelemetry(FName TargetId, const FVector& ObserverSystemPositionCm, const FVector& ObserverVelocityCmPerSec, double SimulationTimeSeconds, FSupercruiseTargetTelemetry& OutTelemetry) const;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	bool RefreshRegisteredEntityTransforms(double SimulationTimeSeconds);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	bool RealizeTrafficActors(const FActiveTrafficSimulationState& TrafficState, double SimulationTimeSeconds);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	bool RefreshRealizedTrafficActors(const FActiveTrafficSimulationState& TrafficState, double SimulationTimeSeconds);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	void ClearRealizedTrafficActors();

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	void GetRealizedTrafficActors(TArray<FRealizedTrafficActorEntry>& OutActors) const;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Encounter")
	bool RealizeSystemicEncounterActors(const FSystemicGameplayState& SystemicState, double SimulationTimeSeconds);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Encounter")
	void ClearRealizedEncounterActors();

	UFUNCTION(BlueprintPure, Category = "Stargame|Encounter")
	void GetRealizedEncounterActors(TArray<FRealizedEncounterActorEntry>& OutActors) const;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Encounter")
	bool ApplyEncounterResponseManeuver(FName EncounterId, FName ResponseStateId, double SimulationTimeSeconds, FVector& OutStartPositionCm, FVector& OutEndPositionCm, FName& OutManeuverStateId);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Combat")
	bool SpawnCombatShotPresentation(FName ShotPresentationId, FName PresentationType, const FVector& StartPositionCm, const FVector& EndPositionCm, double StartedAtTimeSeconds, double DurationSeconds, FName& OutActorId);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Combat")
	void RefreshCombatShotPresentations(double SimulationTimeSeconds);

	UFUNCTION(BlueprintPure, Category = "Stargame|Combat")
	bool IsCombatShotPresentationActive(FName ShotPresentationId) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredEntityIds(TArray<FName>& OutEntityIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredEntities(TArray<FActiveSystemEntityEntry>& OutEntities) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredSpawnZoneIds(TArray<FName>& OutSpawnZoneIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredGravityWellIds(TArray<FName>& OutGravityWellIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredResourceZoneIds(TArray<FName>& OutResourceZoneIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredDockingPortIds(TArray<FName>& OutDockingPortIds) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	void GetRegisteredDockingPorts(TArray<FDockingPortRegistryEntry>& OutDockingPorts) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool FindDockingPort(FName StationId, FName PortId, FDockingPortRegistryEntry& OutDockingPort) const;

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	bool FindDockingPortRuntimeState(FName StationId, FName PortId, FDockingPortRuntimeState& OutRuntimeState) const;

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	bool TryReserveDockingPort(FName StationId, FName PortId, FName ShipInstanceId, double SimulationTimeSeconds, FDockingPortRuntimeState& OutRuntimeState, FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	bool OccupyDockingPort(FName StationId, FName PortId, FName ShipInstanceId, FDockingPortRuntimeState& OutRuntimeState, FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	bool RestoreDockingPortRuntimeState(const FDockingPortRuntimeState& SavedRuntimeState, FDockingPortRuntimeState& OutRuntimeState, FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Space")
	bool ReleaseDockingPort(FName StationId, FName PortId, FName ShipInstanceId);

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

	UFUNCTION(BlueprintPure, Category = "Stargame|Space")
	FString GetM4DebugSummary(FName SelectedTargetId, const FVector& ObserverSystemPositionCm, const FVector& ObserverVelocityCmPerSec, double SimulationTimeSeconds, const FSupercruiseTelemetry& Telemetry) const;

	UPROPERTY(BlueprintAssignable, Category = "Stargame|Space")
	FStarSystemBuildComplete OnSystemBuildComplete;

private:
	bool ValidateSystemForBuild(const FStarSystemDefinition& SystemDefinition, FString& OutError) const;
	void AddBuildValidationIssue(FName Code, FName ObjectId, const FString& Message);
	bool RegisterEntity(FName EntityId, FName EntityType, const FTransform& Transform, double VisualRadiusCm);
	bool RegisterNavigationTarget(const FNavigationTargetDefinition& Target);
	bool RegisterSpawnZone(const FSpawnZoneDefinition& SpawnZone);
	bool RegisterDockingPort(FName StationId, const FDockingPortDefinition& DockingPort);
	bool RegisterGravityWell(const FGravityWellDefinition& GravityWell);
	bool RegisterResourceZone(const FResourceZoneDefinition& ResourceZone);
	bool RegisterMapEntry(const FMapEntryDefinition& MapEntry);
	bool SpawnSystemPresentation(FName SystemId);
	static FName MakeDockingPortRegistryId(FName StationId, FName PortId);

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
	TMap<FName, FDockingPortRuntimeState> DockingPortRuntimeStatesById;

	UPROPERTY()
	TMap<FName, FGravityWellDefinition> GravityWellsById;

	UPROPERTY()
	TMap<FName, FResourceZoneDefinition> ResourceZonesById;

	UPROPERTY()
	TMap<FName, FMapEntryDefinition> MapEntriesById;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpawnedSystemActors;

	UPROPERTY()
	TObjectPtr<ASystemSpacePresentationActor> SpacePresentationActor;

	UPROPERTY()
	TMap<FName, FRealizedTrafficActorEntry> RealizedTrafficActorsByShipId;

	UPROPERTY()
	TMap<FName, FRealizedEncounterActorEntry> RealizedEncounterActorsById;

	UPROPERTY()
	TMap<FName, TObjectPtr<ACombatShotPresentationActor>> CombatShotPresentationActorsById;

	UPROPERTY()
	TWeakObjectPtr<APawn> ActivePlayerPawn;

	UPROPERTY()
	FStargameValidationReport LastBuildValidationReport;

	int32 ActiveBuildGeneration = 0;
	bool bBuildComplete = false;
	FString LastBuildError;
};
