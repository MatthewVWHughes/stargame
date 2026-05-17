#pragma once

#include "CoreMinimal.h"
#include "Data/StargameDataTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OrbitRouteFrameQueryService.generated.h"

UCLASS()
class STARGAME_API UOrbitRouteFrameQueryService : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Stargame|Frame")
	static FSimulationClockSnapshot MakeDefaultClockSnapshot(FName SystemId, double SimulationTimeSeconds = 0.0);

	UFUNCTION(BlueprintPure, Category = "Stargame|Frame")
	static FStargameScaleContract MakeDefaultScaleContract();

	UFUNCTION(BlueprintPure, Category = "Stargame|Frame")
	static bool ResolveEntityFrame(
		const FStarSystemDefinition& SystemDefinition,
		FName EntityId,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FFrameResolvedTransform& OutTransform);

	UFUNCTION(BlueprintPure, Category = "Stargame|Frame")
	static bool ResolveDockingPortFrame(
		const FStarSystemDefinition& SystemDefinition,
		FName StationId,
		FName DockingPortId,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FFrameResolvedTransform& OutTransform);

	UFUNCTION(BlueprintPure, Category = "Stargame|Frame")
	static bool ResolveDockingPortTransform(
		const FStarSystemDefinition& SystemDefinition,
		FName StationId,
		FName DockingPortId,
		EDockingPortTransformKind TransformKind,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FFrameResolvedTransform& OutTransform);

	UFUNCTION(BlueprintPure, Category = "Stargame|AI")
	static bool PredictMovingFrameTarget(
		const FStarSystemDefinition& SystemDefinition,
		const FMovingFrameTarget& Target,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FAIPredictedTransform& OutPrediction);

	UFUNCTION(BlueprintPure, Category = "Stargame|AI")
	static bool EvaluateRoute(
		const FStarSystemDefinition& SystemDefinition,
		FName RouteSegmentId,
		double RouteProgress01,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FRouteSample& OutSample);

	UFUNCTION(BlueprintPure, Category = "Stargame|AI")
	static bool EstimateRouteTravelTime(
		const FStarSystemDefinition& SystemDefinition,
		FName RouteSegmentId,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		double& OutTravelTimeSeconds);

	UFUNCTION(BlueprintPure, Category = "Stargame|AI")
	static bool FindClosestRouteProgress(
		const FStarSystemDefinition& SystemDefinition,
		FName RouteSegmentId,
		const FVector& SystemPositionCm,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FRouteClosestProgressResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Stargame|AI")
	static FString BuildRoutePredictionDebugSummary(
		const FStarSystemDefinition& SystemDefinition,
		FName RouteSegmentId,
		FName TargetId,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds);

	UFUNCTION(BlueprintPure, Category = "Stargame|Map")
	static void BuildSystemMapViewModel(
		const FStarSystemDefinition& SystemDefinition,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		TArray<FSystemMapEntryViewModel>& OutEntries);

	UFUNCTION(BlueprintPure, Category = "Stargame|Frame")
	static bool ProjectSystemPositionToLocalBubble(
		const FStargameScaleContract& Scale,
		const FVector& BubbleOriginSystemPositionCm,
		const FVector& SystemPositionCm,
		FVector& OutActorPositionCm);

	UFUNCTION(BlueprintPure, Category = "Stargame|Gravity")
	static bool QueryNearestGravityWell(
		const FStarSystemDefinition& SystemDefinition,
		const FVector& ShipSystemPositionCm,
		const FVector& ShipSystemVelocityCmPerSec,
		EShipFlightMode FlightMode,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FGravityWellQueryResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "Stargame|Supercruise")
	static FSupercruiseTargetTelemetry BuildSupercruiseTargetTelemetry(
		const FStargameScaleContract& Scale,
		const FNavigationTargetViewModel& TargetViewModel);

	UFUNCTION(BlueprintPure, Category = "Stargame|Frame")
	static FString GetScaleDebugSummary(const FStargameScaleContract& Scale);

private:
	static bool ResolveEntityFrameInternal(
		const FStarSystemDefinition& SystemDefinition,
		FName EntityId,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		TSet<FName>& VisitedIds,
		FFrameResolvedTransform& OutTransform);

	static bool FindEntityDefinition(
		const FStarSystemDefinition& SystemDefinition,
		FName EntityId,
		FName& OutEntityType,
		FName& OutAnchorId,
		FTransform& OutTransform,
		FOrbitDefinition& OutOrbit,
		double& OutVisualRadiusCm);

	static const FTrafficRouteSegmentDefinition* FindRouteDefinition(const FStarSystemDefinition& SystemDefinition, FName RouteSegmentId);
	static bool ResolveRouteEndpoint(
		const FStarSystemDefinition& SystemDefinition,
		const FTrafficRouteSegmentDefinition& Route,
		bool bSource,
		const FSimulationClockSnapshot& ClockSnapshot,
		double RequestedSimulationTimeSeconds,
		FFrameResolvedTransform& OutTransform);
	static bool BuildRouteArc(
		const FTrafficRouteSegmentDefinition& Route,
		const FFrameResolvedTransform& Source,
		const FFrameResolvedTransform& Destination,
		FVector& OutP0,
		FVector& OutP1,
		FVector& OutControlPoint,
		FVector& OutArcNormal);
	static double EstimateRouteLengthCm(
		const FTrafficRouteSegmentDefinition& Route,
		const FFrameResolvedTransform& Source,
		const FFrameResolvedTransform& Destination);

	static FVector EvaluateOrbitPosition(const FOrbitDefinition& Orbit, double RequestedSimulationTimeSeconds);
	static FVector EvaluateOrbitVelocity(const FOrbitDefinition& Orbit, double RequestedSimulationTimeSeconds);
};
