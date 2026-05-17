#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LogicalTrafficQueryService.generated.h"

UCLASS()
class STARGAME_API ULogicalTrafficQueryService : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	static FActiveTrafficSimulationState MakeM8FixtureTrafficState(const FStarSystemDefinition& SystemDefinition, double SimulationTimeSeconds);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	static EShipGoalExecutionResult UpdateActiveTraffic(
		const FStarSystemDefinition& SystemDefinition,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		const FActiveTrafficUpdateBudget& Budget,
		FActiveTrafficSimulationState& InOutTrafficState);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	static bool ApplyScriptedInterrupt(
		FShipTrafficInstance& Ship,
		const FShipGoalState& InterruptGoal,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	static bool RestoreSavedGoal(FShipTrafficInstance& Ship, FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	static bool PromoteLogicalTrader(
		const FStarSystemDefinition& SystemDefinition,
		FShipTrafficInstance& Ship,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FName RealizationToken,
		FRouteSample& OutTargetSample,
		FString& OutFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	static bool DemoteLogicalTrader(
		const FStarSystemDefinition& SystemDefinition,
		FShipTrafficInstance& Ship,
		const FRouteSample& ActorRouteSample,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FName ExpectedRealizationToken,
		FString& OutFailureReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	static FActiveTrafficSimulationState SanitizeTrafficStateForSave(const FActiveTrafficSimulationState& TrafficState);

	UFUNCTION(BlueprintCallable, Category = "Stargame|Traffic AI")
	static void RefreshTransientRouteSamples(
		const FStarSystemDefinition& SystemDefinition,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FActiveTrafficSimulationState& InOutTrafficState);

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	static EShipGoalExecutionResult CanExecuteAutonomousGoal(const FShipGoalState& Goal, FString& OutReason);

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	static bool BuildLogicalTrafficDebugView(const FShipTrafficInstance& Ship, FLogicalTrafficDebugView& OutDebugView);

	UFUNCTION(BlueprintPure, Category = "Stargame|Traffic AI")
	static FString BuildActiveTrafficDebugSummary(const FActiveTrafficSimulationState& TrafficState);

private:
	static bool UpdateTradeRouteGoal(
		const FStarSystemDefinition& SystemDefinition,
		const FSimulationClockSnapshot& ClockSnapshot,
		double PreviousSimulationTimeSeconds,
		double RequestedSimulationTimeSeconds,
		double AppliedDeltaSeconds,
		FShipTrafficInstance& Ship);
	static const TCHAR* GoalKindToString(EShipGoalKind GoalKind);
};
