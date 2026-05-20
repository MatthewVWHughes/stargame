#include "Space/LogicalTrafficQueryService.h"

#include "Space/OrbitRouteFrameQueryService.h"

namespace
{
	const FName M8TradeRouteId(TEXT("m7_brink_watch_wayfarer_trade"));
	const FName M8TraderId(TEXT("trader_brink_01"));
	const FName M8TraderGroupId(TEXT("m8_brink_trade_group"));
	const FName M8TradeGoalId(TEXT("m8_trade_brink_to_wayfarer"));
	constexpr double RouteRecoveryMaxDistanceErrorCm = 250000.0;
	constexpr double RouteRecoveryMaxBasisErrorDegrees = 15.0;
	constexpr double RouteRecoveryMaxVelocityErrorCmPerSec = 2500000.0;
	constexpr double RouteRecoveryMaxSampleAgeSeconds = 1.0;
	constexpr double RealizationDespawnRadiusMultiplier = 1.4;

	void WriteRouteLogicalLocation(FShipTrafficInstance& Ship, const FRouteSample& Sample)
	{
		Ship.LogicalLocation.TargetId = Sample.RouteSegmentId;
		Ship.LogicalLocation.TargetType = TEXT("route_sample");
		Ship.LogicalLocation.RouteSegmentId = Sample.RouteSegmentId;
		Ship.LogicalLocation.RouteProgress01 = Sample.RouteProgress01;
		Ship.LogicalRotation = Sample.ResolvedTransform.Rotation;
		Ship.LogicalVelocityCmPerSec = Sample.EstimatedVelocityCmPerSec;
		Ship.VelocityFrame.FrameType = TEXT("route_sample");
		Ship.VelocityFrame.AnchorId = Sample.RouteSegmentId;
		Ship.bInheritsFrameVelocity = true;
		Ship.FrameVelocityCmPerSec = Sample.ResolvedTransform.LinearVelocityCmPerSec;
	}

	void WriteOffRouteLogicalLocation(FShipTrafficInstance& Ship, const FRouteSample& ActorRouteSample, double RequestedSimulationTimeSeconds, FName DecisionReason)
	{
		Ship.LogicalLocation = FMovingFrameTarget();
		Ship.LogicalLocation.TargetId = Ship.ShipInstanceId;
		Ship.LogicalLocation.TargetType = TEXT("off_route_free_flight");
		Ship.LogicalLocation.CoordinateFrame.FrameType = TEXT("system_barycentric");
		Ship.LogicalLocation.CoordinateFrame.AnchorId = Ship.SystemId;
		Ship.LogicalLocation.AnchorId = Ship.SystemId;
		Ship.LogicalLocation.LocalOffsetCm = ActorRouteSample.ResolvedTransform.PositionCm;
		Ship.LogicalRotation = ActorRouteSample.ResolvedTransform.Rotation;
		Ship.LogicalVelocityCmPerSec = ActorRouteSample.EstimatedVelocityCmPerSec.IsNearlyZero()
			? ActorRouteSample.ResolvedTransform.LinearVelocityCmPerSec
			: ActorRouteSample.EstimatedVelocityCmPerSec;
		Ship.VelocityFrame.FrameType = TEXT("system_barycentric");
		Ship.VelocityFrame.AnchorId = Ship.SystemId;
		Ship.FrameVelocityCmPerSec = FVector::ZeroVector;
		Ship.bInheritsFrameVelocity = false;
		Ship.TrafficTier = ELogicalTrafficTier::Tier2Logical;
		Ship.RealizationToken = NAME_None;
		Ship.LastRouteSample = FRouteSample();
		Ship.LastUpdateTimeSeconds = RequestedSimulationTimeSeconds;
		Ship.LastDecisionReason = DecisionReason;
		Ship.bRouteRecoverable = false;
	}
}

FActiveTrafficSimulationState ULogicalTrafficQueryService::MakeM8FixtureTrafficState(const FStarSystemDefinition& SystemDefinition, double SimulationTimeSeconds)
{
	FActiveTrafficSimulationState TrafficState;
	TrafficState.SystemId = SystemDefinition.SystemId;

	FShipGoalState TradeGoal;
	TradeGoal.GoalId = M8TradeGoalId;
	TradeGoal.GoalKind = EShipGoalKind::TradeRoute;
	TradeGoal.RouteSegmentId = M8TradeRouteId;
	TradeGoal.RouteProgress01 = 0.0;
	TradeGoal.DesiredRouteSpeedFractionPerSecond = 0.02;
	TradeGoal.InterruptPriority = 1;
	TradeGoal.DecisionReason = TEXT("fixture_trade_route_progress");
	TradeGoal.TargetFrame.TargetId = M8TradeRouteId;
	TradeGoal.TargetFrame.TargetType = TEXT("route_sample");
	TradeGoal.TargetFrame.RouteSegmentId = M8TradeRouteId;
	TradeGoal.TargetFrame.RouteProgress01 = 0.0;

	FShipTrafficInstance Trader;
	Trader.ShipInstanceId = M8TraderId;
	Trader.SystemId = SystemDefinition.SystemId;
	Trader.ShipArchetypeId = TEXT("wayfarer_mk1");
	Trader.CurrentGoal = TradeGoal;
	Trader.GroupId = M8TraderGroupId;
	Trader.FormationSlotId = TEXT("slot_lead_trade");
	Trader.TrafficTier = ELogicalTrafficTier::Tier2Logical;
	Trader.LastUpdateTimeSeconds = SimulationTimeSeconds;
	Trader.LastDecisionReason = TradeGoal.DecisionReason;

	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, SimulationTimeSeconds);
	UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, M8TradeRouteId, 0.0, Clock, SimulationTimeSeconds, Trader.LastRouteSample);
	WriteRouteLogicalLocation(Trader, Trader.LastRouteSample);
	TrafficState.Ships.Add(Trader);

	FShipGroupState Group;
	Group.GroupId = M8TraderGroupId;
	Group.LeaderShipInstanceId = Trader.ShipInstanceId;
	Group.GroupRole = TEXT("trade");
	Group.MemberShipInstanceIds = { Trader.ShipInstanceId };

	FShipFormationSlot Slot;
	Slot.SlotId = Trader.FormationSlotId;
	Slot.ShipInstanceId = Trader.ShipInstanceId;
	Slot.SlotFrame = TEXT("leader_route_sample");
	Slot.LocalOffsetCm = FVector::ZeroVector;
	Slot.RouteProgressOffset = 0.0;
	Group.FormationSlots = { Slot };
	TrafficState.Groups.Add(Group);

	TrafficState.LastUpdateStats.CapReason = TEXT("not_updated");
	return TrafficState;
}

EShipGoalExecutionResult ULogicalTrafficQueryService::UpdateActiveTraffic(
	const FStarSystemDefinition& SystemDefinition,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	const FActiveTrafficUpdateBudget& Budget,
	FActiveTrafficSimulationState& InOutTrafficState)
{
	if (InOutTrafficState.SystemId.IsNone())
	{
		InOutTrafficState.SystemId = SystemDefinition.SystemId;
	}

	const int32 MaxShips = FMath::Max(0, Budget.MaxShipsPerUpdate);
	FActiveTrafficUpdateStats Stats;
	Stats.CapReason = TEXT("within_budget");

	EShipGoalExecutionResult LastResult = EShipGoalExecutionResult::NoWork;
	for (FShipTrafficInstance& Ship : InOutTrafficState.Ships)
	{
		++Stats.ConsideredShips;
		if (Ship.TrafficTier == ELogicalTrafficTier::Tier1Realized)
		{
			++Stats.SkippedShips;
			Ship.LastDecisionReason = TEXT("tier1_realized_skipped");
			LastResult = LastResult == EShipGoalExecutionResult::NoWork ? EShipGoalExecutionResult::NoWork : LastResult;
			continue;
		}
		if (Stats.UpdatedShips >= MaxShips)
		{
			++Stats.SkippedShips;
			Stats.CapReason = TEXT("ship_count_capped");
			LastResult = EShipGoalExecutionResult::BudgetExceeded;
			continue;
		}

		const double PreviousTime = Ship.LastUpdateTimeSeconds > 0.0 ? Ship.LastUpdateTimeSeconds : ClockSnapshot.AuthoritativeSimulationTimeSeconds;
		const double RequestedDelta = FMath::Max(0.0, RequestedSimulationTimeSeconds - PreviousTime);
		const double AppliedDelta = FMath::Min(RequestedDelta, FMath::Max(0.0, Budget.MaxSimulationStepSeconds));
		const double AppliedTime = PreviousTime + AppliedDelta;
		Stats.RequestedDeltaSeconds = FMath::Max(Stats.RequestedDeltaSeconds, RequestedDelta);
		Stats.AppliedDeltaSeconds = FMath::Max(Stats.AppliedDeltaSeconds, AppliedDelta);
		if (RequestedDelta > AppliedDelta && Stats.CapReason != FName(TEXT("ship_count_capped")))
		{
			Stats.CapReason = TEXT("time_budget_capped");
		}

		FString Reason;
		const EShipGoalExecutionResult CanExecute = CanExecuteAutonomousGoal(Ship.CurrentGoal, Reason);
		if (CanExecute != EShipGoalExecutionResult::Success)
		{
			Ship.LastDecisionReason = FName(*Reason);
			Ship.LastUpdateTimeSeconds = AppliedTime;
			++Stats.UpdatedShips;
			LastResult = CanExecute;
			continue;
		}

		if (Ship.CurrentGoal.GoalKind == EShipGoalKind::TradeRoute &&
			UpdateTradeRouteGoal(SystemDefinition, ClockSnapshot, PreviousTime, AppliedTime, AppliedDelta, Ship))
		{
			++Stats.UpdatedShips;
			LastResult = EShipGoalExecutionResult::Success;
		}
		else
		{
			Ship.LastDecisionReason = TEXT("invalid_or_unsupported_goal");
			Ship.LastUpdateTimeSeconds = AppliedTime;
			++Stats.UpdatedShips;
			LastResult = EShipGoalExecutionResult::InvalidGoal;
		}
	}

	InOutTrafficState.LastUpdateStats = Stats;
	return LastResult;
}

bool ULogicalTrafficQueryService::ApplyScriptedInterrupt(FShipTrafficInstance& Ship, const FShipGoalState& InterruptGoal, FString& OutFailureReason)
{
	if (InterruptGoal.GoalId.IsNone())
	{
		OutFailureReason = TEXT("Interrupt goal requires a stable GoalId.");
		return false;
	}
	if (InterruptGoal.InterruptPriority <= Ship.CurrentGoal.InterruptPriority)
	{
		OutFailureReason = TEXT("Interrupt priority is not high enough to pause the current goal.");
		return false;
	}

	Ship.SavedGoal = Ship.CurrentGoal;
	Ship.bHasSavedGoal = true;
	Ship.CurrentGoal = InterruptGoal;
	Ship.LastDecisionReason = TEXT("scripted_interrupt_applied");
	return true;
}

bool ULogicalTrafficQueryService::RestoreSavedGoal(FShipTrafficInstance& Ship, FString& OutFailureReason)
{
	if (!Ship.bHasSavedGoal || Ship.SavedGoal.GoalId.IsNone())
	{
		OutFailureReason = TEXT("No saved goal is available to restore.");
		return false;
	}

	Ship.CurrentGoal = Ship.SavedGoal;
	Ship.SavedGoal = FShipGoalState();
	Ship.bHasSavedGoal = false;
	Ship.LastDecisionReason = TEXT("saved_goal_restored");
	return true;
}

bool ULogicalTrafficQueryService::PromoteLogicalTrader(
	const FStarSystemDefinition& SystemDefinition,
	FShipTrafficInstance& Ship,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FName RealizationToken,
	FRouteSample& OutTargetSample,
	FString& OutFailureReason)
{
	if (Ship.CurrentGoal.GoalKind != EShipGoalKind::TradeRoute && Ship.CurrentGoal.GoalKind != EShipGoalKind::Flee)
	{
		OutFailureReason = TEXT("Only route-sample trade or flee goals are supported by the realization harness.");
		return false;
	}
	if (RealizationToken.IsNone())
	{
		OutFailureReason = TEXT("Promotion requires a non-actor realization token.");
		return false;
	}
	const FName RouteSegmentId = Ship.CurrentGoal.RouteSegmentId.IsNone() ? Ship.CurrentGoal.TargetFrame.RouteSegmentId : Ship.CurrentGoal.RouteSegmentId;
	if (RouteSegmentId.IsNone() || !UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, RouteSegmentId, Ship.CurrentGoal.RouteProgress01, ClockSnapshot, RequestedSimulationTimeSeconds, OutTargetSample))
	{
		OutFailureReason = TEXT("Could not resolve the route sample for promotion.");
		return false;
	}

	Ship.TrafficTier = ELogicalTrafficTier::Tier1Realized;
	Ship.RealizationToken = RealizationToken;
	Ship.LastRouteSample = OutTargetSample;
	WriteRouteLogicalLocation(Ship, OutTargetSample);
	Ship.LastUpdateTimeSeconds = RequestedSimulationTimeSeconds;
	Ship.LastDecisionReason = TEXT("promoted_to_pooled_actor");
	return true;
}

bool ULogicalTrafficQueryService::DemoteLogicalTrader(
	const FStarSystemDefinition& SystemDefinition,
	FShipTrafficInstance& Ship,
	const FRouteSample& ActorRouteSample,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FName ExpectedRealizationToken,
	FString& OutFailureReason)
{
	if (Ship.CurrentGoal.GoalKind != EShipGoalKind::TradeRoute && Ship.CurrentGoal.GoalKind != EShipGoalKind::Flee)
	{
		OutFailureReason = TEXT("Only route-sample trade or flee goals are supported by the demotion harness.");
		return false;
	}
	if (Ship.TrafficTier != ELogicalTrafficTier::Tier1Realized)
	{
		OutFailureReason = TEXT("Demotion requires a currently realized traffic instance.");
		return false;
	}
	if (Ship.RealizationToken.IsNone() || Ship.RealizationToken != ExpectedRealizationToken)
	{
		OutFailureReason = TEXT("Demotion snapshot does not match the active realization token.");
		return false;
	}
	if (!Ship.bRouteRecoverable)
	{
		WriteOffRouteLogicalLocation(Ship, ActorRouteSample, RequestedSimulationTimeSeconds, TEXT("demoted_off_route_not_recoverable"));
		OutFailureReason = TEXT("Ship logical motion state was demoted to off-route free flight.");
		return true;
	}
	const FName RouteSegmentId = Ship.CurrentGoal.RouteSegmentId.IsNone() ? Ship.CurrentGoal.TargetFrame.RouteSegmentId : Ship.CurrentGoal.RouteSegmentId;
	if (RouteSegmentId.IsNone() || ActorRouteSample.RouteSegmentId != RouteSegmentId)
	{
		WriteOffRouteLogicalLocation(Ship, ActorRouteSample, RequestedSimulationTimeSeconds, TEXT("demoted_off_route_route_mismatch"));
		OutFailureReason = TEXT("Demotion sample route mismatch; ship was demoted to off-route free flight.");
		return true;
	}
	if (FMath::Abs(ActorRouteSample.SimulationTimeSeconds - RequestedSimulationTimeSeconds) > RouteRecoveryMaxSampleAgeSeconds)
	{
		WriteOffRouteLogicalLocation(Ship, ActorRouteSample, RequestedSimulationTimeSeconds, TEXT("demoted_off_route_stale_sample"));
		OutFailureReason = TEXT("Demotion sample was stale; ship was demoted to off-route free flight.");
		return true;
	}

	FRouteClosestProgressResult Closest;
	if (!UOrbitRouteFrameQueryService::FindClosestRouteProgress(SystemDefinition, RouteSegmentId, ActorRouteSample.ResolvedTransform.PositionCm, ClockSnapshot, RequestedSimulationTimeSeconds, Closest) || !Closest.bValid)
	{
		WriteOffRouteLogicalLocation(Ship, ActorRouteSample, RequestedSimulationTimeSeconds, TEXT("demoted_off_route_unrecoverable_progress"));
		OutFailureReason = TEXT("Could not reconstruct route progress; ship was demoted to off-route free flight.");
		return true;
	}

	FRouteSample RecoveredSample;
	if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, RouteSegmentId, Closest.RouteProgress01, ClockSnapshot, RequestedSimulationTimeSeconds, RecoveredSample))
	{
		WriteOffRouteLogicalLocation(Ship, ActorRouteSample, RequestedSimulationTimeSeconds, TEXT("demoted_off_route_unrecoverable_sample"));
		OutFailureReason = TEXT("Could not evaluate recovered route sample; ship was demoted to off-route free flight.");
		return true;
	}
	const double BasisDot = FVector::DotProduct(ActorRouteSample.Tangent.GetSafeNormal(), RecoveredSample.Tangent.GetSafeNormal());
	Closest.BasisErrorDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(BasisDot, -1.0, 1.0)));
	Closest.VelocityErrorCmPerSec = FVector::Distance(ActorRouteSample.EstimatedVelocityCmPerSec, RecoveredSample.EstimatedVelocityCmPerSec);
	if (Closest.DistanceErrorCm > RouteRecoveryMaxDistanceErrorCm ||
		Closest.BasisErrorDegrees > RouteRecoveryMaxBasisErrorDegrees ||
		Closest.VelocityErrorCmPerSec > RouteRecoveryMaxVelocityErrorCmPerSec)
	{
		OutFailureReason = FString::Printf(TEXT("Route recovery tolerance failed: distance=%.1f basis=%.1f velocity=%.1f."),
			Closest.DistanceErrorCm,
			Closest.BasisErrorDegrees,
			Closest.VelocityErrorCmPerSec);
		WriteOffRouteLogicalLocation(Ship, ActorRouteSample, RequestedSimulationTimeSeconds, TEXT("demoted_off_route_tolerance_failed"));
		return true;
	}

	Ship.CurrentGoal.RouteProgress01 = Closest.RouteProgress01;
	Ship.CurrentGoal.RouteSegmentId = RouteSegmentId;
	Ship.CurrentGoal.TargetFrame.RouteSegmentId = RouteSegmentId;
	Ship.CurrentGoal.TargetFrame.RouteProgress01 = Closest.RouteProgress01;
	Ship.LastRouteSample = RecoveredSample;
	Ship.LastRouteSample.RouteProgress01 = Closest.RouteProgress01;
	WriteRouteLogicalLocation(Ship, Ship.LastRouteSample);
	Ship.TrafficTier = ELogicalTrafficTier::Tier2Logical;
	Ship.RealizationToken = NAME_None;
	Ship.LastUpdateTimeSeconds = RequestedSimulationTimeSeconds;
	Ship.LastDecisionReason = TEXT("demoted_from_pooled_actor");
	Ship.bRouteRecoverable = true;
	return true;
}

FActiveTrafficSimulationState ULogicalTrafficQueryService::SanitizeTrafficStateForSave(const FActiveTrafficSimulationState& TrafficState)
{
	FActiveTrafficSimulationState Sanitized = TrafficState;
	for (FShipTrafficInstance& Ship : Sanitized.Ships)
	{
		Ship.RealizationToken = NAME_None;
		Ship.LastRouteSample = FRouteSample();
		if (Ship.TrafficTier == ELogicalTrafficTier::Tier1Realized)
		{
			Ship.TrafficTier = ELogicalTrafficTier::Tier2Logical;
			Ship.LastDecisionReason = TEXT("demoted_for_save");
		}
	}
	return Sanitized;
}

void ULogicalTrafficQueryService::RefreshTransientRouteSamples(
	const FStarSystemDefinition& SystemDefinition,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FActiveTrafficSimulationState& InOutTrafficState)
{
	for (FShipTrafficInstance& Ship : InOutTrafficState.Ships)
	{
		if (Ship.CurrentGoal.GoalKind != EShipGoalKind::TradeRoute || Ship.CurrentGoal.RouteSegmentId.IsNone())
		{
			continue;
		}

		FRouteSample Sample;
		if (UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Ship.CurrentGoal.RouteSegmentId, Ship.CurrentGoal.RouteProgress01, ClockSnapshot, RequestedSimulationTimeSeconds, Sample))
		{
			Ship.LastRouteSample = Sample;
			WriteRouteLogicalLocation(Ship, Sample);
		}
	}
}

void ULogicalTrafficQueryService::ApplyPlayerRelevanceRealization(
	const FStarSystemDefinition& SystemDefinition,
	const FSystemicGameplayState& SystemicState,
	FName ActorBudgetProfileId,
	const FVector& ObserverPositionCm,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FActiveTrafficSimulationState& InOutTrafficState)
{
	const FRealizedActorBudgetProfile* Budget = SystemicState.RealizedActorBudgetProfiles.FindByPredicate([ActorBudgetProfileId](const FRealizedActorBudgetProfile& Candidate)
	{
		return Candidate.BudgetProfileId == ActorBudgetProfileId;
	});
	if (!Budget || Budget->MaxRealizedActors <= 0 || Budget->PromotionRadiusCm <= 0.0)
	{
		return;
	}

	struct FCandidate
	{
		FShipTrafficInstance* Ship = nullptr;
		const FRealizedActorMappingRecord* Mapping = nullptr;
		FRouteSample Sample;
		double DistanceCm = 0.0;
	};

	TArray<FCandidate> Candidates;
	TSet<FName> EligibleShipIds;
	for (const FRealizedActorMappingRecord& Mapping : SystemicState.RealizedActorMappings)
	{
		if (Mapping.ActorBudgetProfileId != ActorBudgetProfileId || Mapping.State != FName(TEXT("eligible")) || Mapping.RealizationToken.IsNone())
		{
			continue;
		}
		FShipTrafficInstance* Ship = InOutTrafficState.Ships.FindByPredicate([&Mapping](const FShipTrafficInstance& Candidate)
		{
			return Candidate.ShipInstanceId == Mapping.ShipInstanceId;
		});
		if (!Ship || Ship->CurrentGoal.RouteSegmentId.IsNone())
		{
			continue;
		}

		FRouteSample Sample;
		if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Ship->CurrentGoal.RouteSegmentId, Ship->CurrentGoal.RouteProgress01, ClockSnapshot, RequestedSimulationTimeSeconds, Sample))
		{
			continue;
		}

		EligibleShipIds.Add(Ship->ShipInstanceId);
		const double DistanceCm = FVector::Distance(Sample.ResolvedTransform.PositionCm, ObserverPositionCm);
		if (DistanceCm <= Budget->PromotionRadiusCm || Ship->TrafficTier == ELogicalTrafficTier::Tier1Realized)
		{
			FCandidate Candidate;
			Candidate.Ship = Ship;
			Candidate.Mapping = &Mapping;
			Candidate.Sample = Sample;
			Candidate.DistanceCm = DistanceCm;
			Candidates.Add(Candidate);
		}
	}

	Candidates.Sort([](const FCandidate& Left, const FCandidate& Right)
	{
		if (Left.Mapping->PromotionPriority == Right.Mapping->PromotionPriority)
		{
			return Left.DistanceCm < Right.DistanceCm;
		}
		return Left.Mapping->PromotionPriority > Right.Mapping->PromotionPriority;
	});

	TSet<FName> KeepRealizedShipIds;
	const int32 MaxRealizedActors = FMath::Max(0, Budget->MaxRealizedActors);
	for (const FCandidate& Candidate : Candidates)
	{
		if (!Candidate.Ship || !Candidate.Mapping)
		{
			continue;
		}

		if (KeepRealizedShipIds.Num() >= MaxRealizedActors ||
			Candidate.DistanceCm > Budget->PromotionRadiusCm * RealizationDespawnRadiusMultiplier)
		{
			continue;
		}

		KeepRealizedShipIds.Add(Candidate.Ship->ShipInstanceId);
		Candidate.Ship->TrafficTier = ELogicalTrafficTier::Tier1Realized;
		Candidate.Ship->RealizationToken = Candidate.Mapping->RealizationToken;
		Candidate.Ship->LastRouteSample = Candidate.Sample;
		WriteRouteLogicalLocation(*Candidate.Ship, Candidate.Sample);
		Candidate.Ship->LastUpdateTimeSeconds = RequestedSimulationTimeSeconds;
		Candidate.Ship->LastDecisionReason = FName(*FString::Printf(TEXT("promoted_player_relevance_distance_%.0f"), Candidate.DistanceCm));
	}

	for (FShipTrafficInstance& Ship : InOutTrafficState.Ships)
	{
		if (Ship.TrafficTier != ELogicalTrafficTier::Tier1Realized || !EligibleShipIds.Contains(Ship.ShipInstanceId) || KeepRealizedShipIds.Contains(Ship.ShipInstanceId))
		{
			continue;
		}

		Ship.TrafficTier = ELogicalTrafficTier::Tier2Logical;
		Ship.RealizationToken = NAME_None;
		Ship.LastUpdateTimeSeconds = RequestedSimulationTimeSeconds;
		Ship.LastDecisionReason = TEXT("demoted_player_relevance_budget_or_distance");
	}
}

EShipGoalExecutionResult ULogicalTrafficQueryService::CanExecuteAutonomousGoal(const FShipGoalState& Goal, FString& OutReason)
{
	if (Goal.GoalKind == EShipGoalKind::None)
	{
		OutReason = TEXT("invalid_goal_none");
		return EShipGoalExecutionResult::InvalidGoal;
	}
	if (Goal.GoalKind == EShipGoalKind::TradeRoute)
	{
		OutReason = TEXT("trade_route_allowed");
		return EShipGoalExecutionResult::Success;
	}
	if (!Goal.bAutonomousExecutionAllowed)
	{
		if (Goal.GoalKind == EShipGoalKind::Fight)
		{
			OutReason = TEXT("fight_reserved_until_m10");
			return EShipGoalExecutionResult::ReservedUntilM10;
		}

		OutReason = TEXT("reserved_until_m9");
		return EShipGoalExecutionResult::ReservedUntilM9;
	}

	OutReason = TEXT("non_trade_autonomy_not_implemented");
	return Goal.GoalKind == EShipGoalKind::Fight
		? EShipGoalExecutionResult::ReservedUntilM10
		: EShipGoalExecutionResult::ReservedUntilM9;
}

bool ULogicalTrafficQueryService::BuildLogicalTrafficDebugView(const FShipTrafficInstance& Ship, FLogicalTrafficDebugView& OutDebugView)
{
	if (Ship.ShipInstanceId.IsNone())
	{
		return false;
	}

	OutDebugView = FLogicalTrafficDebugView();
	OutDebugView.ShipInstanceId = Ship.ShipInstanceId;
	OutDebugView.Goal = FName(GoalKindToString(Ship.CurrentGoal.GoalKind));
	OutDebugView.TargetFrame = Ship.CurrentGoal.TargetFrame;
	OutDebugView.RouteProgress01 = Ship.CurrentGoal.RouteProgress01;
	OutDebugView.GroupId = Ship.GroupId;
	OutDebugView.DecisionReason = Ship.LastDecisionReason;
	OutDebugView.DebugSummary = FString::Printf(TEXT("Ship=%s Goal=%s TargetFrame=%s RouteProgress=%.3f Group=%s DecisionReason=%s"),
		*Ship.ShipInstanceId.ToString(),
		GoalKindToString(Ship.CurrentGoal.GoalKind),
		*Ship.CurrentGoal.TargetFrame.RouteSegmentId.ToString(),
		Ship.CurrentGoal.RouteProgress01,
		*Ship.GroupId.ToString(),
		*Ship.LastDecisionReason.ToString());
	return true;
}

FString ULogicalTrafficQueryService::BuildActiveTrafficDebugSummary(const FActiveTrafficSimulationState& TrafficState)
{
	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("ActiveTraffic System=%s Ships=%d Groups=%d Updated=%d/%d AppliedDelta=%.3f Cap=%s"),
		*TrafficState.SystemId.ToString(),
		TrafficState.Ships.Num(),
		TrafficState.Groups.Num(),
		TrafficState.LastUpdateStats.UpdatedShips,
		TrafficState.LastUpdateStats.ConsideredShips,
		TrafficState.LastUpdateStats.AppliedDeltaSeconds,
		*TrafficState.LastUpdateStats.CapReason.ToString()));

	for (const FShipTrafficInstance& Ship : TrafficState.Ships)
	{
		FLogicalTrafficDebugView View;
		if (BuildLogicalTrafficDebugView(Ship, View))
		{
			Lines.Add(View.DebugSummary);
		}
	}

	return FString::Join(Lines, TEXT("\n"));
}

bool ULogicalTrafficQueryService::UpdateTradeRouteGoal(
	const FStarSystemDefinition& SystemDefinition,
	const FSimulationClockSnapshot& ClockSnapshot,
	double,
	double RequestedSimulationTimeSeconds,
	double AppliedDeltaSeconds,
	FShipTrafficInstance& Ship)
{
	if (Ship.CurrentGoal.RouteSegmentId.IsNone())
	{
		return false;
	}

	const double NewProgress = FMath::Clamp(
		Ship.CurrentGoal.RouteProgress01 + AppliedDeltaSeconds * Ship.CurrentGoal.DesiredRouteSpeedFractionPerSecond,
		0.0,
		1.0);

	FRouteSample Sample;
	if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Ship.CurrentGoal.RouteSegmentId, NewProgress, ClockSnapshot, RequestedSimulationTimeSeconds, Sample))
	{
		return false;
	}

	Ship.CurrentGoal.RouteProgress01 = NewProgress;
	Ship.CurrentGoal.TargetFrame.TargetId = Ship.CurrentGoal.RouteSegmentId;
	Ship.CurrentGoal.TargetFrame.TargetType = TEXT("route_sample");
	Ship.CurrentGoal.TargetFrame.RouteSegmentId = Ship.CurrentGoal.RouteSegmentId;
	Ship.CurrentGoal.TargetFrame.RouteProgress01 = NewProgress;
	Ship.LastRouteSample = Sample;
	WriteRouteLogicalLocation(Ship, Sample);
	Ship.LastUpdateTimeSeconds = RequestedSimulationTimeSeconds;
	Ship.LastDecisionReason = NewProgress >= 1.0 ? FName(TEXT("trade_route_arrived")) : FName(TEXT("trade_route_progress"));
	return true;
}

const TCHAR* ULogicalTrafficQueryService::GoalKindToString(EShipGoalKind GoalKind)
{
	switch (GoalKind)
	{
	case EShipGoalKind::TradeRoute:
		return TEXT("TradeRoute");
	case EShipGoalKind::Patrol:
		return TEXT("Patrol");
	case EShipGoalKind::Escort:
		return TEXT("Escort");
	case EShipGoalKind::Pirate:
		return TEXT("Pirate");
	case EShipGoalKind::Flee:
		return TEXT("Flee");
	case EShipGoalKind::Fight:
		return TEXT("Fight");
	default:
		return TEXT("None");
	}
}
