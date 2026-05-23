#include "Space/OrbitRouteFrameQueryService.h"

namespace
{
	constexpr double FixtureRouteTravelSpeedCmPerSec = 5000000.0;
	constexpr int32 RouteLengthSegments = 24;

	FVector EvaluateQuadraticBezier(const FVector& P0, const FVector& Pc, const FVector& P1, double Progress)
	{
		const double T = FMath::Clamp(Progress, 0.0, 1.0);
		const double InvT = 1.0 - T;
		return (InvT * InvT * P0) + (2.0 * InvT * T * Pc) + (T * T * P1);
	}

	FVector EvaluateQuadraticBezierDerivative(const FVector& P0, const FVector& Pc, const FVector& P1, double Progress)
	{
		const double T = FMath::Clamp(Progress, 0.0, 1.0);
		return 2.0 * ((1.0 - T) * (Pc - P0) + T * (P1 - Pc));
	}

	FRotator MakeRouteRotation(const FVector& Tangent, const FVector& ArcNormal)
	{
		const FVector Forward = Tangent.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FVector Up = (ArcNormal - Forward * FVector::DotProduct(ArcNormal, Forward)).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		return FRotationMatrix::MakeFromXZ(Forward, Up).Rotator();
	}
}

FSimulationClockSnapshot UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(FName SystemId, double SimulationTimeSeconds)
{
	FSimulationClockSnapshot Snapshot;
	Snapshot.AuthoritativeSimulationTimeSeconds = SimulationTimeSeconds;
	Snapshot.TimeScale = 1.0;
	Snapshot.ClockOwner = TEXT("session");
	Snapshot.RngStreamId = SystemId;
	Snapshot.RngCounter = 0;
	Snapshot.ProcessedEventWatermark = 0;
	return Snapshot;
}

FStargameScaleContract UOrbitRouteFrameQueryService::MakeDefaultScaleContract()
{
	return FStargameScaleContract();
}

bool UOrbitRouteFrameQueryService::ResolveEntityFrame(
	const FStarSystemDefinition& SystemDefinition,
	FName EntityId,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FFrameResolvedTransform& OutTransform)
{
	TSet<FName> VisitedIds;
	return ResolveEntityFrameInternal(SystemDefinition, EntityId, ClockSnapshot, RequestedSimulationTimeSeconds, VisitedIds, OutTransform);
}

bool UOrbitRouteFrameQueryService::ResolveDockingPortFrame(
	const FStarSystemDefinition& SystemDefinition,
	FName StationId,
	FName DockingPortId,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FFrameResolvedTransform& OutTransform)
{
	return ResolveDockingPortTransform(SystemDefinition, StationId, DockingPortId, EDockingPortTransformKind::Docked, ClockSnapshot, RequestedSimulationTimeSeconds, OutTransform);
}

bool UOrbitRouteFrameQueryService::ResolveDockingPortTransform(
	const FStarSystemDefinition& SystemDefinition,
	FName StationId,
	FName DockingPortId,
	EDockingPortTransformKind TransformKind,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FFrameResolvedTransform& OutTransform)
{
	FFrameResolvedTransform StationTransform;
	if (!ResolveEntityFrame(SystemDefinition, StationId, ClockSnapshot, RequestedSimulationTimeSeconds, StationTransform))
	{
		return false;
	}

	const FStationDefinition* Station = SystemDefinition.Stations.FindByPredicate([StationId](const FStationDefinition& Candidate)
	{
		return Candidate.StationId == StationId;
	});
	if (!Station)
	{
		return false;
	}

	const FDockingPortDefinition* DockingPort = Station->DockingPorts.FindByPredicate([DockingPortId](const FDockingPortDefinition& Candidate)
	{
		return Candidate.PortId == DockingPortId;
	});
	if (!DockingPort)
	{
		return false;
	}

	const FTransform* LocalPortTransform = &DockingPort->DockedTransform;
	switch (TransformKind)
	{
	case EDockingPortTransformKind::Approach:
		LocalPortTransform = &DockingPort->ApproachTransform;
		break;
	case EDockingPortTransformKind::Undock:
		LocalPortTransform = &DockingPort->UndockTransform;
		break;
	case EDockingPortTransformKind::Docked:
	default:
		LocalPortTransform = &DockingPort->DockedTransform;
		break;
	}

	const FTransform ResolvedTransform = *LocalPortTransform * FTransform(StationTransform.Rotation, StationTransform.PositionCm);
	OutTransform = StationTransform;
	OutTransform.CoordinateFrame.FrameType = TEXT("station_relative");
	OutTransform.CoordinateFrame.AnchorId = StationId;
	OutTransform.AnchorId = StationId;
	OutTransform.PositionCm = ResolvedTransform.GetLocation();
	OutTransform.Rotation = ResolvedTransform.Rotator();
	OutTransform.bActorSpaceValid = true;
	OutTransform.ActorSpaceTransform = FTransform(OutTransform.Rotation, OutTransform.PositionCm);
	return true;
}

bool UOrbitRouteFrameQueryService::PredictMovingFrameTarget(
	const FStarSystemDefinition& SystemDefinition,
	const FMovingFrameTarget& Target,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FAIPredictedTransform& OutPrediction)
{
	OutPrediction = FAIPredictedTransform();
	OutPrediction.Target = Target;
	OutPrediction.SimulationTimeSeconds = RequestedSimulationTimeSeconds;

	if (Target.TargetType == FName(TEXT("route_sample")))
	{
		FRouteSample RouteSample;
		if (!EvaluateRoute(SystemDefinition, Target.RouteSegmentId, Target.RouteProgress01, ClockSnapshot, RequestedSimulationTimeSeconds, RouteSample))
		{
			OutPrediction.InvalidReason = TEXT("route_sample_unresolved");
			return false;
		}

		OutPrediction.ResolvedTransform = RouteSample.ResolvedTransform;
		OutPrediction.bValid = true;
		return true;
	}

	const FName EntityId = !Target.AnchorId.IsNone() ? Target.AnchorId : Target.TargetId;
	FFrameResolvedTransform AnchorTransform;
	if (!ResolveEntityFrame(SystemDefinition, EntityId, ClockSnapshot, RequestedSimulationTimeSeconds, AnchorTransform))
	{
		OutPrediction.InvalidReason = TEXT("anchor_unresolved");
		return false;
	}

	const FTransform ResolvedTransform(FQuat(AnchorTransform.Rotation), AnchorTransform.PositionCm);
	const FVector PredictedPosition = ResolvedTransform.TransformPosition(Target.LocalOffsetCm);
	OutPrediction.ResolvedTransform = AnchorTransform;
	OutPrediction.ResolvedTransform.CoordinateFrame = Target.CoordinateFrame;
	OutPrediction.ResolvedTransform.AnchorId = EntityId;
	OutPrediction.ResolvedTransform.PositionCm = PredictedPosition;
	OutPrediction.ResolvedTransform.ActorSpaceTransform = FTransform(OutPrediction.ResolvedTransform.Rotation, PredictedPosition);
	OutPrediction.bValid = true;
	return true;
}

bool UOrbitRouteFrameQueryService::EvaluateRoute(
	const FStarSystemDefinition& SystemDefinition,
	FName RouteSegmentId,
	double RouteProgress01,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FRouteSample& OutSample)
{
	const FTrafficRouteSegmentDefinition* Route = FindRouteDefinition(SystemDefinition, RouteSegmentId);
	if (!Route || Route->RouteGeometryPolicyId != FName(TEXT("fixture_dynamic_arc_v1")))
	{
		return false;
	}

	FFrameResolvedTransform SourceTransform;
	FFrameResolvedTransform DestinationTransform;
	if (!ResolveRouteEndpoint(SystemDefinition, *Route, true, ClockSnapshot, RequestedSimulationTimeSeconds, SourceTransform) ||
		!ResolveRouteEndpoint(SystemDefinition, *Route, false, ClockSnapshot, RequestedSimulationTimeSeconds, DestinationTransform))
	{
		return false;
	}

	FVector P0;
	FVector P1;
	FVector ControlPoint;
	FVector ArcNormal;
	if (!BuildRouteArc(*Route, SourceTransform, DestinationTransform, P0, P1, ControlPoint, ArcNormal))
	{
		return false;
	}

	const double Progress = FMath::Clamp(RouteProgress01, 0.0, 1.0);
	const FVector Position = EvaluateQuadraticBezier(P0, ControlPoint, P1, Progress);
	const FVector Derivative = EvaluateQuadraticBezierDerivative(P0, ControlPoint, P1, Progress);
	const FVector Tangent = Derivative.GetSafeNormal(UE_SMALL_NUMBER, (P1 - P0).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));
	const double TravelTimeSeconds = FMath::Max(EstimateRouteLengthCm(*Route, SourceTransform, DestinationTransform) / FixtureRouteTravelSpeedCmPerSec, SMALL_NUMBER);
	const FVector TravelVelocity = Derivative / TravelTimeSeconds;
	FVector MovingRouteVelocity = FVector::ZeroVector;
	constexpr double RouteVelocityDeltaSeconds = 0.1;
	FFrameResolvedTransform FutureSourceTransform;
	FFrameResolvedTransform FutureDestinationTransform;
	FVector FutureP0;
	FVector FutureP1;
	FVector FutureControlPoint;
	FVector FutureArcNormal;
	if (ResolveRouteEndpoint(SystemDefinition, *Route, true, ClockSnapshot, RequestedSimulationTimeSeconds + RouteVelocityDeltaSeconds, FutureSourceTransform) &&
		ResolveRouteEndpoint(SystemDefinition, *Route, false, ClockSnapshot, RequestedSimulationTimeSeconds + RouteVelocityDeltaSeconds, FutureDestinationTransform) &&
		BuildRouteArc(*Route, FutureSourceTransform, FutureDestinationTransform, FutureP0, FutureP1, FutureControlPoint, FutureArcNormal))
	{
		MovingRouteVelocity = (EvaluateQuadraticBezier(FutureP0, FutureControlPoint, FutureP1, Progress) - Position) / RouteVelocityDeltaSeconds;
	}

	OutSample = FRouteSample();
	OutSample.RouteSegmentId = Route->RouteSegmentId;
	OutSample.SimulationTimeSeconds = RequestedSimulationTimeSeconds;
	OutSample.RouteProgress01 = Progress;
	OutSample.RouteDistanceCm = EstimateRouteLengthCm(*Route, SourceTransform, DestinationTransform) * Progress;
	OutSample.RouteProgressSemantic = Route->RouteProgressSemantic;
	OutSample.ResolvedTransform.CoordinateFrame.FrameType = TEXT("route_sample");
	OutSample.ResolvedTransform.CoordinateFrame.AnchorId = Route->RouteSegmentId;
	OutSample.ResolvedTransform.AnchorId = Route->RouteSegmentId;
	OutSample.ResolvedTransform.SimulationTimeSeconds = RequestedSimulationTimeSeconds;
	OutSample.ResolvedTransform.PositionCm = Position;
	OutSample.ResolvedTransform.Rotation = MakeRouteRotation(Tangent, ArcNormal);
	OutSample.ResolvedTransform.LinearVelocityCmPerSec = MovingRouteVelocity + TravelVelocity;
	OutSample.ResolvedTransform.bActorSpaceValid = true;
	OutSample.ResolvedTransform.ActorSpaceTransform = FTransform(OutSample.ResolvedTransform.Rotation, Position);
	OutSample.Tangent = Tangent;
	OutSample.EstimatedVelocityCmPerSec = OutSample.ResolvedTransform.LinearVelocityCmPerSec;
	OutSample.SourceAnchorTransform = SourceTransform;
	OutSample.DestinationAnchorTransform = DestinationTransform;
	OutSample.RouteGeometryPolicyId = Route->RouteGeometryPolicyId;
	OutSample.TravelModelId = Route->TravelModelId;
	OutSample.SecurityRating = Route->SecurityRating;
	OutSample.RiskScore = 0.0;
	return true;
}

bool UOrbitRouteFrameQueryService::EstimateRouteTravelTime(
	const FStarSystemDefinition& SystemDefinition,
	FName RouteSegmentId,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	double& OutTravelTimeSeconds)
{
	const FTrafficRouteSegmentDefinition* Route = FindRouteDefinition(SystemDefinition, RouteSegmentId);
	if (!Route)
	{
		return false;
	}

	FFrameResolvedTransform SourceTransform;
	FFrameResolvedTransform DestinationTransform;
	if (!ResolveRouteEndpoint(SystemDefinition, *Route, true, ClockSnapshot, RequestedSimulationTimeSeconds, SourceTransform) ||
		!ResolveRouteEndpoint(SystemDefinition, *Route, false, ClockSnapshot, RequestedSimulationTimeSeconds, DestinationTransform))
	{
		return false;
	}

	OutTravelTimeSeconds = EstimateRouteLengthCm(*Route, SourceTransform, DestinationTransform) / FixtureRouteTravelSpeedCmPerSec;
	return OutTravelTimeSeconds > 0.0;
}

bool UOrbitRouteFrameQueryService::FindClosestRouteProgress(
	const FStarSystemDefinition& SystemDefinition,
	FName RouteSegmentId,
	const FVector& SystemPositionCm,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FRouteClosestProgressResult& OutResult)
{
	OutResult = FRouteClosestProgressResult();
	OutResult.RouteSegmentId = RouteSegmentId;

	auto DistanceSquaredAtProgress = [&SystemDefinition, RouteSegmentId, &ClockSnapshot, RequestedSimulationTimeSeconds, &SystemPositionCm](double Progress, bool& bOutValid)
	{
		FRouteSample Sample;
		if (!EvaluateRoute(SystemDefinition, RouteSegmentId, Progress, ClockSnapshot, RequestedSimulationTimeSeconds, Sample))
		{
			bOutValid = false;
			return TNumericLimits<double>::Max();
		}

		bOutValid = true;
		return FVector::DistSquared(SystemPositionCm, Sample.ResolvedTransform.PositionCm);
	};

	bool bValid = true;
	double Low = 0.0;
	double High = 1.0;
	for (int32 Iteration = 0; Iteration < 48; ++Iteration)
	{
		const double Left = Low + (High - Low) / 3.0;
		const double Right = High - (High - Low) / 3.0;
		bool bLeftValid = false;
		bool bRightValid = false;
		const double LeftDistance = DistanceSquaredAtProgress(Left, bLeftValid);
		const double RightDistance = DistanceSquaredAtProgress(Right, bRightValid);
		if (!bLeftValid || !bRightValid)
		{
			return false;
		}

		if (LeftDistance < RightDistance)
		{
			High = Right;
		}
		else
		{
			Low = Left;
		}
	}
	const double BestProgress = FMath::Clamp((Low + High) * 0.5, 0.0, 1.0);
	const double BestDistanceSquared = DistanceSquaredAtProgress(BestProgress, bValid);
	if (!bValid)
	{
		return false;
	}

	FRouteSample BestSample;
	if (!EvaluateRoute(SystemDefinition, RouteSegmentId, BestProgress, ClockSnapshot, RequestedSimulationTimeSeconds, BestSample))
	{
		return false;
	}

	OutResult.RouteProgress01 = BestProgress;
	OutResult.DistanceErrorCm = FMath::Sqrt(BestDistanceSquared);
	OutResult.BasisErrorDegrees = 0.0;
	OutResult.VelocityErrorCmPerSec = 0.0;
	OutResult.bValid = true;
	return true;
}

FString UOrbitRouteFrameQueryService::BuildRoutePredictionDebugSummary(
	const FStarSystemDefinition& SystemDefinition,
	FName RouteSegmentId,
	FName TargetId,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds)
{
	double TravelTimeSeconds = 0.0;
	EstimateRouteTravelTime(SystemDefinition, RouteSegmentId, ClockSnapshot, RequestedSimulationTimeSeconds, TravelTimeSeconds);

	auto FormatTargetAt = [&SystemDefinition, &ClockSnapshot, TargetId](double TimeSeconds)
	{
		FFrameResolvedTransform Transform;
		if (!ResolveEntityFrame(SystemDefinition, TargetId, ClockSnapshot, TimeSeconds, Transform))
		{
			return FString::Printf(TEXT("unresolved target=%s"), *TargetId.ToString());
		}
		return FString::Printf(TEXT("target=%s t=%.1f pos=(%.1f,%.1f,%.1f)"),
			*TargetId.ToString(),
			TimeSeconds,
			Transform.PositionCm.X,
			Transform.PositionCm.Y,
			Transform.PositionCm.Z);
	};

	FRouteSample NowSample;
	EvaluateRoute(SystemDefinition, RouteSegmentId, 0.5, ClockSnapshot, RequestedSimulationTimeSeconds, NowSample);

	return FString::Printf(
		TEXT("Now: %s\nNow + 60s: %s\nestimated-arrival: %.1fs route=%s sample=(%.1f,%.1f,%.1f)\nETA + 60s: %s"),
		*FormatTargetAt(RequestedSimulationTimeSeconds),
		*FormatTargetAt(RequestedSimulationTimeSeconds + 60.0),
		RequestedSimulationTimeSeconds + TravelTimeSeconds,
		*RouteSegmentId.ToString(),
		NowSample.ResolvedTransform.PositionCm.X,
		NowSample.ResolvedTransform.PositionCm.Y,
		NowSample.ResolvedTransform.PositionCm.Z,
		*FormatTargetAt(RequestedSimulationTimeSeconds + TravelTimeSeconds + 60.0));
}

void UOrbitRouteFrameQueryService::BuildSystemMapViewModel(
	const FStarSystemDefinition& SystemDefinition,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	TArray<FSystemMapEntryViewModel>& OutEntries)
{
	OutEntries.Reset();
	const FStargameScaleContract Scale = SystemDefinition.Scale;

	for (const FMapEntryDefinition& MapEntry : SystemDefinition.MapEntries)
	{
		if (!MapEntry.bVisibleInSystemMap)
		{
			continue;
		}

		FFrameResolvedTransform ResolvedTransform;
		if (!ResolveEntityFrame(SystemDefinition, MapEntry.SourceId, ClockSnapshot, RequestedSimulationTimeSeconds, ResolvedTransform))
		{
			continue;
		}

		FName EntityType;
		FName AnchorId;
		FTransform LocalTransform;
		FOrbitDefinition Orbit;
		double VisualRadiusCm = 0.0;
		FindEntityDefinition(SystemDefinition, MapEntry.SourceId, EntityType, AnchorId, LocalTransform, Orbit, VisualRadiusCm);

		FSystemMapEntryViewModel ViewModel;
		ViewModel.EntryId = MapEntry.MapEntryId;
		ViewModel.SourceId = MapEntry.SourceId;
		ViewModel.ParentId = !Orbit.ParentId.IsNone() ? Orbit.ParentId : AnchorId;
		ViewModel.EntryType = MapEntry.EntryType;
		ViewModel.PositionCm = ResolvedTransform.PositionCm;
		ViewModel.MapPosition = FVector2D(ResolvedTransform.PositionCm.X, ResolvedTransform.PositionCm.Y) / Scale.MapDistanceScaleCmPerUnit;
		const double RadiusScale = VisualRadiusCm > 0.0 ? FMath::Sqrt(VisualRadiusCm / 12000.0) : 1.0;
		ViewModel.IconScale = FMath::Clamp(RadiusScale, Scale.MapMinIconScale, Scale.MapMaxIconScale);
		OutEntries.Add(ViewModel);
	}

	OutEntries.Sort([](const FSystemMapEntryViewModel& Left, const FSystemMapEntryViewModel& Right)
	{
		return Left.EntryId.LexicalLess(Right.EntryId);
	});
}

bool UOrbitRouteFrameQueryService::ProjectSystemPositionToLocalBubble(
	const FStargameScaleContract& Scale,
	const FVector& BubbleOriginSystemPositionCm,
	const FVector& SystemPositionCm,
	FVector& OutActorPositionCm)
{
	OutActorPositionCm = SystemPositionCm - BubbleOriginSystemPositionCm;
	return OutActorPositionCm.Size() <= Scale.LocalBubbleRadiusCm;
}

bool UOrbitRouteFrameQueryService::QueryNearestGravityWell(
	const FStarSystemDefinition& SystemDefinition,
	const FVector& ShipSystemPositionCm,
	const FVector& ShipSystemVelocityCmPerSec,
	EShipFlightMode FlightMode,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FGravityWellQueryResult& OutResult)
{
	OutResult = FGravityWellQueryResult();
	OutResult.SpeedCeilingCmPerSec = SystemDefinition.Scale.SupercruiseMaxSpeedCmPerSec;

	double BestDistance = TNumericLimits<double>::Max();
	double BestSpeedCeilingCmPerSec = SystemDefinition.Scale.SupercruiseMaxSpeedCmPerSec;
	double BestSlowdownFactor = 1.0;
	bool bFoundWell = false;
	bool bInsideAnySlowdown = false;
	bool bInsideAnyLockout = false;
	bool bInsideAnyDropout = false;
	bool bForcedDropout = false;
	for (const FGravityWellDefinition& GravityWell : SystemDefinition.GravityWells)
	{
		FFrameResolvedTransform AnchorTransform;
		if (!ResolveEntityFrame(SystemDefinition, GravityWell.AnchorBodyId, ClockSnapshot, RequestedSimulationTimeSeconds, AnchorTransform))
		{
			continue;
		}

		bFoundWell = true;
		const double DistanceCm = FVector::Distance(ShipSystemPositionCm, AnchorTransform.PositionCm);
		const bool bInsideSlowdown = GravityWell.SlowdownRadiusCm > 0.0 && DistanceCm <= GravityWell.SlowdownRadiusCm;
		const bool bInsideLockout = GravityWell.LockoutRadiusCm > 0.0 && DistanceCm <= GravityWell.LockoutRadiusCm;
		const bool bInsideDropout = GravityWell.DropoutRadiusCm > 0.0 && DistanceCm <= GravityWell.DropoutRadiusCm;

		double SlowdownFactor = 1.0;
		if (bInsideSlowdown && GravityWell.SlowdownRadiusCm > GravityWell.LockoutRadiusCm)
		{
			const double SlowdownAlpha = FMath::Clamp(
				(DistanceCm - GravityWell.LockoutRadiusCm) / (GravityWell.SlowdownRadiusCm - GravityWell.LockoutRadiusCm),
				0.0,
				1.0);
			SlowdownFactor = FMath::InterpEaseIn(0.0, 1.0, SlowdownAlpha, 1.65);
		}

		const double SpeedCeilingCmPerSec = FMath::Lerp(
			SystemDefinition.Scale.SupercruiseMinSpeedCmPerSec,
			SystemDefinition.Scale.SupercruiseMaxSpeedCmPerSec,
			SlowdownFactor);

		if (DistanceCm < BestDistance)
		{
			BestDistance = DistanceCm;
			OutResult.NearestWellId = GravityWell.WellId;
			OutResult.AnchorBodyId = GravityWell.AnchorBodyId;
			OutResult.DistanceCm = DistanceCm;
		}
		if (SpeedCeilingCmPerSec < BestSpeedCeilingCmPerSec)
		{
			BestSpeedCeilingCmPerSec = SpeedCeilingCmPerSec;
			BestSlowdownFactor = SlowdownFactor;
		}

		bInsideAnySlowdown = bInsideAnySlowdown || bInsideSlowdown;
		bInsideAnyLockout = bInsideAnyLockout || bInsideLockout;
		bInsideAnyDropout = bInsideAnyDropout || bInsideDropout;
		bForcedDropout = bForcedDropout || (FlightMode == EShipFlightMode::Supercruise && bInsideDropout);
	}

	OutResult.bInsideSlowdown = bInsideAnySlowdown;
	OutResult.bInsideLockout = bInsideAnyLockout;
	OutResult.bInsideDropout = bInsideAnyDropout;
	OutResult.bForcedDropout = bForcedDropout;
	OutResult.SlowdownFactor = BestSlowdownFactor;
	OutResult.SpeedCeilingCmPerSec = BestSpeedCeilingCmPerSec;
	return bFoundWell && !OutResult.NearestWellId.IsNone();
}

FSupercruiseTargetTelemetry UOrbitRouteFrameQueryService::BuildSupercruiseTargetTelemetry(
	const FStargameScaleContract& Scale,
	const FNavigationTargetViewModel& TargetViewModel)
{
	FSupercruiseTargetTelemetry Telemetry;
	Telemetry.TargetId = TargetViewModel.TargetId;
	Telemetry.DistanceCm = TargetViewModel.DistanceCm;
	Telemetry.ClosingSpeedCmPerSec = TargetViewModel.ClosingSpeedCmPerSec;
	Telemetry.bInsideDropoutBand =
		TargetViewModel.DistanceCm >= Scale.SupercruiseTargetDropoutMinRadiusCm &&
		TargetViewModel.DistanceCm <= Scale.SupercruiseTargetDropoutMaxRadiusCm;
	Telemetry.DistanceToDropoutBandCm = TargetViewModel.DistanceCm > Scale.SupercruiseTargetDropoutMaxRadiusCm
		? TargetViewModel.DistanceCm - Scale.SupercruiseTargetDropoutMaxRadiusCm
		: FMath::Max(0.0, Scale.SupercruiseTargetDropoutMinRadiusCm - TargetViewModel.DistanceCm);
	Telemetry.bApproachReady = Telemetry.bInsideDropoutBand && FMath::Abs(TargetViewModel.ClosingSpeedCmPerSec) <= Scale.SupercruiseMinSpeedCmPerSec;
	return Telemetry;
}

FString UOrbitRouteFrameQueryService::GetScaleDebugSummary(const FStargameScaleContract& Scale)
{
	return FString::Printf(
		TEXT("NormalFlightMaxSpeedCmPerSec=%.0f\nLocalBubbleRadiusCm=%.0f\nOriginShiftThresholdCm=%.0f\nStationApproachBubbleRadiusCm=%.0f\nDockingCorridorLengthCm=%.0f\nSupercruiseMinSpeedCmPerSec=%.0f\nSupercruiseMaxSpeedCmPerSec=%.0f\nSupercruiseSpoolSeconds=%.1f\nDropoutCooldownSeconds=%.1f\nSupercruiseTargetDropoutMinRadiusCm=%.0f\nSupercruiseTargetDropoutMaxRadiusCm=%.0f\nSupercruiseAccelerationCmPerSec2=%.0f\nSupercruiseBrakingCmPerSec2=%.0f\nGravitySlowdownRadiusCm=%.0f\nGravityLockoutRadiusCm=%.0f\nAtmosphereTransitionRadiusCm=%.0f\nMapDistanceScaleCmPerUnit=%.0f\nMapMinIconScale=%.2f\nMapMaxIconScale=%.2f"),
		Scale.NormalFlightMaxSpeedCmPerSec,
		Scale.LocalBubbleRadiusCm,
		Scale.OriginShiftThresholdCm,
		Scale.StationApproachBubbleRadiusCm,
		Scale.DockingCorridorLengthCm,
		Scale.SupercruiseMinSpeedCmPerSec,
		Scale.SupercruiseMaxSpeedCmPerSec,
		Scale.SupercruiseSpoolSeconds,
		Scale.DropoutCooldownSeconds,
		Scale.SupercruiseTargetDropoutMinRadiusCm,
		Scale.SupercruiseTargetDropoutMaxRadiusCm,
		Scale.SupercruiseAccelerationCmPerSec2,
		Scale.SupercruiseBrakingCmPerSec2,
		Scale.GravitySlowdownRadiusCm,
		Scale.GravityLockoutRadiusCm,
		Scale.AtmosphereTransitionRadiusCm,
		Scale.MapDistanceScaleCmPerUnit,
		Scale.MapMinIconScale,
		Scale.MapMaxIconScale);
}

bool UOrbitRouteFrameQueryService::ResolveEntityFrameInternal(
	const FStarSystemDefinition& SystemDefinition,
	FName EntityId,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	TSet<FName>& VisitedIds,
	FFrameResolvedTransform& OutTransform)
{
	if (EntityId.IsNone() || VisitedIds.Contains(EntityId))
	{
		return false;
	}
	VisitedIds.Add(EntityId);

	FName EntityType;
	FName AnchorId;
	FTransform LocalTransform;
	FOrbitDefinition Orbit;
	double VisualRadiusCm = 0.0;
	if (!FindEntityDefinition(SystemDefinition, EntityId, EntityType, AnchorId, LocalTransform, Orbit, VisualRadiusCm))
	{
		return false;
	}

	FVector ParentPosition = FVector::ZeroVector;
	FRotator ParentRotation = FRotator::ZeroRotator;
	FVector ParentVelocity = FVector::ZeroVector;
	const FName ParentId = !Orbit.ParentId.IsNone() ? Orbit.ParentId : AnchorId;
	if (!ParentId.IsNone())
	{
		FFrameResolvedTransform ParentTransform;
		if (!ResolveEntityFrameInternal(SystemDefinition, ParentId, ClockSnapshot, RequestedSimulationTimeSeconds, VisitedIds, ParentTransform))
		{
			return false;
		}
		ParentPosition = ParentTransform.PositionCm;
		ParentRotation = ParentTransform.Rotation;
		ParentVelocity = ParentTransform.LinearVelocityCmPerSec;
	}

	const FVector OrbitalOffset = Orbit.ParentId.IsNone() ? FVector::ZeroVector : EvaluateOrbitPosition(Orbit, RequestedSimulationTimeSeconds);
	const FVector OrbitalVelocity = Orbit.ParentId.IsNone() ? FVector::ZeroVector : EvaluateOrbitVelocity(Orbit, RequestedSimulationTimeSeconds);
	const FVector LocalOffset = Orbit.ParentId.IsNone() ? LocalTransform.GetLocation() : OrbitalOffset + LocalTransform.GetLocation();

	OutTransform = FFrameResolvedTransform();
	OutTransform.CoordinateFrame.FrameType = ParentId.IsNone() ? TEXT("system_barycentric") : FName(*FString::Printf(TEXT("%s_relative"), *EntityType.ToString()));
	OutTransform.CoordinateFrame.AnchorId = ParentId;
	OutTransform.AnchorId = ParentId;
	OutTransform.SimulationTimeSeconds = RequestedSimulationTimeSeconds;
	OutTransform.PositionCm = ParentPosition + ParentRotation.RotateVector(LocalOffset);
	OutTransform.Rotation = (LocalTransform.GetRotation() * ParentRotation.Quaternion()).Rotator();
	OutTransform.LinearVelocityCmPerSec = ParentVelocity + ParentRotation.RotateVector(OrbitalVelocity);
	OutTransform.bActorSpaceValid = true;
	OutTransform.ActorSpaceTransform = FTransform(OutTransform.Rotation, OutTransform.PositionCm);
	return true;
}

bool UOrbitRouteFrameQueryService::FindEntityDefinition(
	const FStarSystemDefinition& SystemDefinition,
	FName EntityId,
	FName& OutEntityType,
	FName& OutAnchorId,
	FTransform& OutTransform,
	FOrbitDefinition& OutOrbit,
	double& OutVisualRadiusCm)
{
	if (const FBodyDefinition* Body = SystemDefinition.Bodies.FindByPredicate([EntityId](const FBodyDefinition& Candidate)
	{
		return Candidate.BodyId == EntityId;
	}))
	{
		OutEntityType = TEXT("body");
		OutAnchorId = Body->AnchorId;
		OutTransform = Body->Transform;
		OutOrbit = Body->Orbit;
		OutVisualRadiusCm = Body->VisualRadiusCm;
		return true;
	}

	if (const FStationDefinition* Station = SystemDefinition.Stations.FindByPredicate([EntityId](const FStationDefinition& Candidate)
	{
		return Candidate.StationId == EntityId;
	}))
	{
		OutEntityType = TEXT("station");
		OutAnchorId = Station->AnchorId;
		OutTransform = Station->Transform;
		OutOrbit = Station->Orbit;
		OutVisualRadiusCm = Station->VisualRadiusCm;
		return true;
	}

	if (const FGateDefinition* Gate = SystemDefinition.Gates.FindByPredicate([EntityId](const FGateDefinition& Candidate)
	{
		return Candidate.GateId == EntityId;
	}))
	{
		OutEntityType = TEXT("gate");
		OutAnchorId = Gate->AnchorId;
		OutTransform = Gate->Transform;
		OutOrbit = FOrbitDefinition();
		OutVisualRadiusCm = Gate->VisualRadiusCm;
		return true;
	}

	if (const FSpawnZoneDefinition* SpawnZone = SystemDefinition.SpawnZones.FindByPredicate([EntityId](const FSpawnZoneDefinition& Candidate)
	{
		return Candidate.SpawnZoneId == EntityId;
	}))
	{
		OutEntityType = TEXT("spawn_zone");
		OutAnchorId = SpawnZone->AnchorId;
		OutTransform = SpawnZone->Transform;
		OutOrbit = FOrbitDefinition();
		OutVisualRadiusCm = SpawnZone->RadiusCm;
		return true;
	}

	if (const FResourceZoneDefinition* ResourceZone = SystemDefinition.ResourceZones.FindByPredicate([EntityId](const FResourceZoneDefinition& Candidate)
	{
		return Candidate.ZoneId == EntityId;
	}))
	{
		OutEntityType = TEXT("resource");
		OutAnchorId = ResourceZone->AnchorId;
		OutTransform = ResourceZone->Transform;
		OutOrbit = FOrbitDefinition();
		OutVisualRadiusCm = ResourceZone->RadiusCm;
		return true;
	}

	return false;
}

const FTrafficRouteSegmentDefinition* UOrbitRouteFrameQueryService::FindRouteDefinition(const FStarSystemDefinition& SystemDefinition, FName RouteSegmentId)
{
	return SystemDefinition.TrafficRoutes.FindByPredicate([RouteSegmentId](const FTrafficRouteSegmentDefinition& Candidate)
	{
		return Candidate.RouteSegmentId == RouteSegmentId;
	});
}

bool UOrbitRouteFrameQueryService::ResolveRouteEndpoint(
	const FStarSystemDefinition& SystemDefinition,
	const FTrafficRouteSegmentDefinition& Route,
	bool bSource,
	const FSimulationClockSnapshot& ClockSnapshot,
	double RequestedSimulationTimeSeconds,
	FFrameResolvedTransform& OutTransform)
{
	const FName AnchorId = bSource ? Route.SourceAnchorId : Route.DestinationAnchorId;
	const FVector LocalOffsetCm = bSource ? Route.SourceLocalOffsetCm : Route.DestinationLocalOffsetCm;
	if (!ResolveEntityFrame(SystemDefinition, AnchorId, ClockSnapshot, RequestedSimulationTimeSeconds, OutTransform))
	{
		return false;
	}

	const FTransform AnchorTransform(FQuat(OutTransform.Rotation), OutTransform.PositionCm);
	OutTransform.PositionCm = AnchorTransform.TransformPosition(LocalOffsetCm);
	OutTransform.ActorSpaceTransform = FTransform(OutTransform.Rotation, OutTransform.PositionCm);
	return true;
}

bool UOrbitRouteFrameQueryService::BuildRouteArc(
	const FTrafficRouteSegmentDefinition& Route,
	const FFrameResolvedTransform& Source,
	const FFrameResolvedTransform& Destination,
	FVector& OutP0,
	FVector& OutP1,
	FVector& OutControlPoint,
	FVector& OutArcNormal)
{
	OutP0 = Source.PositionCm;
	OutP1 = Destination.PositionCm;
	const FVector Chord = OutP1 - OutP0;
	const double ChordLength = Chord.Size();
	if (ChordLength <= SMALL_NUMBER)
	{
		return false;
	}

	const FVector ChordDirection = Chord / ChordLength;
	FVector ArcNormal = Route.ControlData.ArcNormalHint - ChordDirection * FVector::DotProduct(Route.ControlData.ArcNormalHint, ChordDirection);
	if (!ArcNormal.Normalize())
	{
		ArcNormal = Source.Rotation.RotateVector(FVector::UpVector);
		ArcNormal = ArcNormal - ChordDirection * FVector::DotProduct(ArcNormal, ChordDirection);
	}
	if (!ArcNormal.Normalize())
	{
		ArcNormal = (GetTypeHash(Route.RouteSegmentId) & 1) == 0 ? FVector::UpVector : FVector::RightVector;
		ArcNormal = ArcNormal - ChordDirection * FVector::DotProduct(ArcNormal, ChordDirection);
	}
	if (!ArcNormal.Normalize())
	{
		ArcNormal = FVector::ForwardVector ^ ChordDirection;
		ArcNormal.Normalize();
	}

	const double ArcHeightCm = Route.ControlData.ArcHeightCm > 0.0 ? Route.ControlData.ArcHeightCm : 0.15 * ChordLength;
	OutArcNormal = ArcNormal;
	OutControlPoint = FMath::Lerp(OutP0, OutP1, 0.5) + OutArcNormal * ArcHeightCm;
	return true;
}

double UOrbitRouteFrameQueryService::EstimateRouteLengthCm(
	const FTrafficRouteSegmentDefinition& Route,
	const FFrameResolvedTransform& Source,
	const FFrameResolvedTransform& Destination)
{
	FVector P0;
	FVector P1;
	FVector ControlPoint;
	FVector ArcNormal;
	if (!BuildRouteArc(Route, Source, Destination, P0, P1, ControlPoint, ArcNormal))
	{
		return 0.0;
	}

	double LengthCm = 0.0;
	FVector Previous = P0;
	for (int32 Index = 1; Index <= RouteLengthSegments; ++Index)
	{
		const double Progress = static_cast<double>(Index) / static_cast<double>(RouteLengthSegments);
		const FVector Current = EvaluateQuadraticBezier(P0, ControlPoint, P1, Progress);
		LengthCm += FVector::Distance(Previous, Current);
		Previous = Current;
	}
	return LengthCm;
}

FVector UOrbitRouteFrameQueryService::EvaluateOrbitPosition(const FOrbitDefinition& Orbit, double RequestedSimulationTimeSeconds)
{
	if (Orbit.SemiMajorAxisCm <= 0.0 || Orbit.PeriodSeconds <= 0.0)
	{
		return FVector::ZeroVector;
	}

	const double AngleRadians = Orbit.PhaseOffsetRadians + (RequestedSimulationTimeSeconds / Orbit.PeriodSeconds) * UE_DOUBLE_TWO_PI;
	return FVector(FMath::Cos(AngleRadians) * Orbit.SemiMajorAxisCm, FMath::Sin(AngleRadians) * Orbit.SemiMajorAxisCm, 0.0);
}

FVector UOrbitRouteFrameQueryService::EvaluateOrbitVelocity(const FOrbitDefinition& Orbit, double RequestedSimulationTimeSeconds)
{
	if (Orbit.SemiMajorAxisCm <= 0.0 || Orbit.PeriodSeconds <= 0.0)
	{
		return FVector::ZeroVector;
	}

	const double AngleRadians = Orbit.PhaseOffsetRadians + (RequestedSimulationTimeSeconds / Orbit.PeriodSeconds) * UE_DOUBLE_TWO_PI;
	const double AngularSpeedRadiansPerSecond = UE_DOUBLE_TWO_PI / Orbit.PeriodSeconds;
	return FVector(
		-FMath::Sin(AngleRadians) * Orbit.SemiMajorAxisCm * AngularSpeedRadiansPerSecond,
		FMath::Cos(AngleRadians) * Orbit.SemiMajorAxisCm * AngularSpeedRadiansPerSecond,
		0.0);
}
