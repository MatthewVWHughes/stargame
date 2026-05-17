#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SystemicGameplayQueryService.generated.h"

UCLASS()
class STARGAME_API USystemicGameplayQueryService : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static FSystemicGameplayState MakeM9FixtureState(const FStarSystemDefinition& SystemDefinition);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static FSystemicGameplayState MakeM10FixtureState(const FStarSystemDefinition& SystemDefinition);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static FSystemicGameplayState MakeM10_5FixtureState(const FStarSystemDefinition& SystemDefinition);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static FSystemicGameplayState MakeM11FixtureState(const FStarSystemDefinition& SystemDefinition);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static FSystemicGameplayState MakeM12FixtureState(const FStarSystemDefinition& SystemDefinition);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static bool ValidateSystemicGameplayState(
		const FStarSystemDefinition& SystemDefinition,
		const FSystemicGameplayState& State,
		FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static bool ValidateLogicalEncounterState(
		const FStarSystemDefinition& SystemDefinition,
		const FSystemicGameplayState& State,
		FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static bool ValidateCombatDamageThreatState(
		const FStarSystemDefinition& SystemDefinition,
		const FSystemicGameplayState& State,
		FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static bool ValidateRealizedAISliceState(
		const FStarSystemDefinition& SystemDefinition,
		const FSystemicGameplayState& State,
		FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static bool ValidateM12GameplayState(
		const FStarSystemDefinition& SystemDefinition,
		const FSystemicGameplayState& State,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool SelectRealizedPromotionCandidates(
		const FStarSystemDefinition& SystemDefinition,
		const FSystemicGameplayState& State,
		FName ActorBudgetProfileId,
		const FMovingFrameTarget& ObserverTarget,
		const FSimulationClockSnapshot& ClockSnapshot,
		double SimulationTimeSeconds,
		TArray<FRealizedActorMappingRecord>& OutPromotions,
		TArray<FName>& OutBlockedShipIds,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool ApplyDamageEventOnce(
		FSystemicGameplayState& InOutState,
		const FDamageEventRecord& DamageEvent,
		FDamageEventRecord& OutResult,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool ApplySimulationEventOnce(
		FSystemicGameplayState& InOutState,
		const FSimulationEventRecord& Event,
		double AppliedTimeSeconds,
		FSimulationEventResultRecord& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool RecordOffense(
		FSystemicGameplayState& InOutState,
		const FOffenseEvent& Offense,
		FEvidenceRecord& OutEvidence,
		FCriminalRecord& OutCriminalRecord,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool TransferCargo(
		FSystemicGameplayState& InOutState,
		const FCargoTransferRequest& Request,
		FCargoTransferResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool ExecuteMarketTransaction(
		FSystemicGameplayState& InOutState,
		const FMarketTransactionRequest& Request,
		FMarketTransactionResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool ExecuteProgressionMarketTransactionOnce(
		FSystemicGameplayState& InOutState,
		const FMarketTransactionRequest& Request,
		FMarketTransactionResult& OutResult,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool ExecuteStationServiceRequestOnce(
		FSystemicGameplayState& InOutState,
		const FStationServiceRequest& Request,
		FStationServiceResultRecord& OutResult,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool AcceptMissionOfferOnce(
		FSystemicGameplayState& InOutState,
		FName OfferId,
		FName PlayerId,
		FName IdempotencyKey,
		FMissionInstanceState& OutMission,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool CompleteMissionOnce(
		FSystemicGameplayState& InOutState,
		FName MissionInstanceId,
		FName SourceEventId,
		FName IdempotencyKey,
		FProgressionDebugLedgerEntry& OutCompletionEntry,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool FailMissionOnce(
		FSystemicGameplayState& InOutState,
		FName MissionInstanceId,
		FName SourceEventId,
		FName IdempotencyKey,
		FProgressionDebugLedgerEntry& OutFailureEntry,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool ApplyEncounterProgressionOutcomeOnce(
		FSystemicGameplayState& InOutState,
		FName EncounterId,
		FName IdempotencyKey,
		FProgressionDebugLedgerEntry& OutProgressionEntry,
		FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static bool ResolveStationServiceEndpoint(
		const FSystemicGameplayState& State,
		FName ServiceEndpointId,
		FStationServiceEndpointDefinition& OutEndpoint,
		FString& OutDebugReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static bool BuildSystemicDecisionInputSnapshot(
		const FStarSystemDefinition& SystemDefinition,
		const FSystemicGameplayState& State,
		FName RouteSegmentId,
		FSystemicDecisionInputSnapshot& OutSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool ReservePatrolForRoute(
		const FStarSystemDefinition& SystemDefinition,
		FSystemicGameplayState& InOutState,
		FName RouteSegmentId,
		FName SourceEventId,
		double ExpiresAtTimeSeconds,
		FPatrolReservationRecord& OutReservation,
		FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static bool SelectPirateInterdictionHazard(
		const FStarSystemDefinition& SystemDefinition,
		const FSystemicGameplayState& State,
		FName RouteSegmentId,
		const FSimulationClockSnapshot& ClockSnapshot,
		double SimulationTimeSeconds,
		FInterdictionHazardRecord& OutHazard,
		FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static bool ChooseFleeDestination(
		const FStarSystemDefinition& SystemDefinition,
		const FSystemicGameplayState& State,
		FName ThreatRouteSegmentId,
		FMovingFrameTarget& OutDestination,
		FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static bool SelectLogicalFightPolicy(
		const FStarSystemDefinition& SystemDefinition,
		const FSystemicGameplayState& State,
		FName EncounterId,
		FName& OutPolicyId,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Systemic")
	static bool ResolveLogicalEncounterOnce(
		FSystemicGameplayState& InOutState,
		FName EncounterId,
		double AppliedTimeSeconds,
		FLogicalEncounterResolutionResult& OutResult,
		FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static FString BuildSystemicDebugSummary(const FSystemicGameplayState& State);

	UFUNCTION(BlueprintPure, Category = "Stargame|Systemic")
	static FString BuildProgressionDebugTrace(const FSystemicGameplayState& State);
};
