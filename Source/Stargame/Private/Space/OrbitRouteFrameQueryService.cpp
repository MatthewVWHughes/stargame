#include "Space/OrbitRouteFrameQueryService.h"

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

	const FTransform ResolvedTransform = DockingPort->DockedTransform * FTransform(StationTransform.Rotation, StationTransform.PositionCm);
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

FString UOrbitRouteFrameQueryService::GetScaleDebugSummary(const FStargameScaleContract& Scale)
{
	return FString::Printf(
		TEXT("LocalBubbleRadiusCm=%.0f\nOriginShiftThresholdCm=%.0f\nStationApproachBubbleRadiusCm=%.0f\nDockingCorridorLengthCm=%.0f\nSupercruiseMinSpeedCmPerSec=%.0f\nSupercruiseMaxSpeedCmPerSec=%.0f\nGravitySlowdownRadiusCm=%.0f\nGravityLockoutRadiusCm=%.0f\nAtmosphereTransitionRadiusCm=%.0f\nMapDistanceScaleCmPerUnit=%.0f\nMapMinIconScale=%.2f\nMapMaxIconScale=%.2f"),
		Scale.LocalBubbleRadiusCm,
		Scale.OriginShiftThresholdCm,
		Scale.StationApproachBubbleRadiusCm,
		Scale.DockingCorridorLengthCm,
		Scale.SupercruiseMinSpeedCmPerSec,
		Scale.SupercruiseMaxSpeedCmPerSec,
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

	return false;
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
