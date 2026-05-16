#include "Space/StarSystemSubsystem.h"

#include "Flight/SpaceFlightPawn.h"
#include "GameFramework/PlayerController.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Space/M0SystemMarkerActor.h"

void UStarSystemSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UStarSystemSubsystem::Deinitialize()
{
	TearDownActiveSystem();
	Super::Deinitialize();
}

bool UStarSystemSubsystem::BuildSystem(const FStarSystemDefinition& SystemDefinition)
{
	TearDownActiveSystem();
	++ActiveBuildGeneration;
	LastBuildError.Reset();

	if (!ValidateSystemForBuild(SystemDefinition, LastBuildError))
	{
		return false;
	}

	ActiveSystemDefinition = SystemDefinition;
	const FSimulationClockSnapshot BuildClock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	for (const FBodyDefinition& Body : SystemDefinition.Bodies)
	{
		FFrameResolvedTransform ResolvedTransform;
		if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, Body.BodyId, BuildClock, BuildClock.AuthoritativeSimulationTimeSeconds, ResolvedTransform))
		{
			TearDownActiveSystem();
			return false;
		}

		if (!RegisterEntity(Body.BodyId, TEXT("body"), FTransform(ResolvedTransform.Rotation, ResolvedTransform.PositionCm), Body.VisualRadiusCm) ||
			!RegisterNavigationTarget(Body.NavigationTarget))
		{
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		FFrameResolvedTransform ResolvedTransform;
		if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, Station.StationId, BuildClock, BuildClock.AuthoritativeSimulationTimeSeconds, ResolvedTransform))
		{
			TearDownActiveSystem();
			return false;
		}

		if (!RegisterEntity(Station.StationId, TEXT("station"), FTransform(ResolvedTransform.Rotation, ResolvedTransform.PositionCm), Station.VisualRadiusCm) ||
			!RegisterNavigationTarget(Station.NavigationTarget))
		{
			TearDownActiveSystem();
			return false;
		}

		for (const FDockingPortDefinition& DockingPort : Station.DockingPorts)
		{
			if (!RegisterDockingPort(Station.StationId, DockingPort))
			{
				TearDownActiveSystem();
				return false;
			}
		}
	}

	for (const FGateDefinition& Gate : SystemDefinition.Gates)
	{
		FFrameResolvedTransform ResolvedTransform;
		if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, Gate.GateId, BuildClock, BuildClock.AuthoritativeSimulationTimeSeconds, ResolvedTransform))
		{
			TearDownActiveSystem();
			return false;
		}

		if (!RegisterEntity(Gate.GateId, TEXT("gate"), FTransform(ResolvedTransform.Rotation, ResolvedTransform.PositionCm), Gate.VisualRadiusCm) ||
			!RegisterNavigationTarget(Gate.NavigationTarget))
		{
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FSpawnZoneDefinition& SpawnZone : SystemDefinition.SpawnZones)
	{
		if (!RegisterSpawnZone(SpawnZone))
		{
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FGravityWellDefinition& GravityWell : SystemDefinition.GravityWells)
	{
		if (!RegisterGravityWell(GravityWell))
		{
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FMapEntryDefinition& MapEntry : SystemDefinition.MapEntries)
	{
		if (!RegisterMapEntry(MapEntry))
		{
			TearDownActiveSystem();
			return false;
		}
	}

	bBuildComplete = true;
	OnSystemBuildComplete.Broadcast();
	return true;
}

void UStarSystemSubsystem::TearDownActiveSystem()
{
	for (TObjectPtr<AActor> Actor : SpawnedSystemActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}

	SpawnedSystemActors.Reset();
	RegisteredEntities.Reset();
	NavigationTargetsById.Reset();
	SpawnZonesById.Reset();
	DockingPortsById.Reset();
	GravityWellsById.Reset();
	MapEntriesById.Reset();
	ActiveSystemDefinition = FStarSystemDefinition();
	bBuildComplete = false;
}

bool UStarSystemSubsystem::SpawnPlayerAtSpawnZone(FName SpawnZoneId, APlayerController* PlayerController, TSubclassOf<APawn> PawnClass)
{
	FSpawnZoneDefinition SpawnZone;
	if (!FindSpawnZone(SpawnZoneId, SpawnZone))
	{
		LastBuildError = FString::Printf(TEXT("Missing spawn zone '%s'."), *SpawnZoneId.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LastBuildError = TEXT("Cannot spawn player without a world.");
		return false;
	}

	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!Pawn)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		TSubclassOf<APawn> SpawnClass = PawnClass;
		if (!SpawnClass)
		{
			SpawnClass = ASpaceFlightPawn::StaticClass();
		}
		Pawn = World->SpawnActor<APawn>(SpawnClass, SpawnZone.Transform, SpawnParameters);
		if (PlayerController && Pawn)
		{
			PlayerController->Possess(Pawn);
		}
	}
	else
	{
		Pawn->SetActorTransform(SpawnZone.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	return Pawn != nullptr;
}

void UStarSystemSubsystem::GetNavigationTargets(TArray<FNavigationTargetDefinition>& OutTargets) const
{
	NavigationTargetsById.GenerateValueArray(OutTargets);
	OutTargets.Sort([](const FNavigationTargetDefinition& Left, const FNavigationTargetDefinition& Right)
	{
		return Left.TargetId.LexicalLess(Right.TargetId);
	});
}

bool UStarSystemSubsystem::FindNavigationTarget(FName TargetId, FNavigationTargetDefinition& OutTarget) const
{
	if (const FNavigationTargetDefinition* Target = NavigationTargetsById.Find(TargetId))
	{
		OutTarget = *Target;
		return true;
	}
	return false;
}

bool UStarSystemSubsystem::BuildNavigationTargetViewModel(
	FName TargetId,
	FName SelectedTargetId,
	const FVector& ObserverSystemPositionCm,
	const FVector& ObserverVelocityCmPerSec,
	double SimulationTimeSeconds,
	FNavigationTargetViewModel& OutViewModel) const
{
	FNavigationTargetDefinition Target;
	if (!FindNavigationTarget(TargetId, Target))
	{
		return false;
	}

	FFrameResolvedTransform ResolvedTransform;
	const FSimulationClockSnapshot ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(ActiveSystemDefinition.SystemId, SimulationTimeSeconds);
	if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(ActiveSystemDefinition, Target.TargetId, ClockSnapshot, SimulationTimeSeconds, ResolvedTransform))
	{
		return false;
	}

	FVector ActorPositionCm = FVector::ZeroVector;
	const bool bProjectionValid = UOrbitRouteFrameQueryService::ProjectSystemPositionToLocalBubble(
		ActiveSystemDefinition.Scale,
		ObserverSystemPositionCm,
		ResolvedTransform.PositionCm,
		ActorPositionCm);

	const FVector RelativePositionCm = ResolvedTransform.PositionCm - ObserverSystemPositionCm;
	const FVector RelativeVelocityCmPerSec = ResolvedTransform.LinearVelocityCmPerSec - ObserverVelocityCmPerSec;
	const double DistanceCm = RelativePositionCm.Size();
	const double ClosingSpeedCmPerSec = DistanceCm > SMALL_NUMBER
		? -FVector::DotProduct(RelativeVelocityCmPerSec, RelativePositionCm / DistanceCm)
		: 0.0;

	OutViewModel = FNavigationTargetViewModel();
	OutViewModel.TargetId = Target.TargetId;
	OutViewModel.DisplayName = Target.DisplayName;
	OutViewModel.TargetType = Target.TargetType;
	OutViewModel.TargetFrame.FrameType = Target.FrameType;
	OutViewModel.TargetFrame.AnchorId = Target.AnchorId;
	OutViewModel.bIsSelected = Target.TargetId == SelectedTargetId;
	OutViewModel.bResolved = true;
	OutViewModel.DistanceCm = DistanceCm;
	OutViewModel.ClosingSpeedCmPerSec = ClosingSpeedCmPerSec;
	OutViewModel.ResolvedTargetTransform = ResolvedTransform;
	OutViewModel.ActorSpaceProjection = FTransform(ResolvedTransform.Rotation, ActorPositionCm);
	OutViewModel.bActorSpaceProjectionValid = bProjectionValid;
	OutViewModel.bInsideLocalBubble = bProjectionValid;
	OutViewModel.bInsideStationApproachBubble = Target.TargetType == FName(TEXT("station")) && DistanceCm <= ActiveSystemDefinition.Scale.StationApproachBubbleRadiusCm;
	OutViewModel.bInsideGateActivationRange = false;
	if (Target.TargetType == FName(TEXT("gate")))
	{
		if (const FGateDefinition* Gate = ActiveSystemDefinition.Gates.FindByPredicate([Target](const FGateDefinition& Candidate)
		{
			return Candidate.GateId == Target.TargetId;
		}))
		{
			OutViewModel.bInsideGateActivationRange = DistanceCm <= Gate->ActivationRangeCm;
		}
	}
	return true;
}

void UStarSystemSubsystem::BuildNavigationTargetViewModels(
	FName SelectedTargetId,
	const FVector& ObserverSystemPositionCm,
	const FVector& ObserverVelocityCmPerSec,
	double SimulationTimeSeconds,
	TArray<FNavigationTargetViewModel>& OutViewModels) const
{
	OutViewModels.Reset();

	TArray<FNavigationTargetDefinition> Targets;
	GetNavigationTargets(Targets);
	for (const FNavigationTargetDefinition& Target : Targets)
	{
		if (!Target.bCanTarget || !Target.bShowInHud)
		{
			continue;
		}

		FNavigationTargetViewModel ViewModel;
		if (BuildNavigationTargetViewModel(Target.TargetId, SelectedTargetId, ObserverSystemPositionCm, ObserverVelocityCmPerSec, SimulationTimeSeconds, ViewModel))
		{
			OutViewModels.Add(ViewModel);
		}
	}
}

bool UStarSystemSubsystem::QueryNearestGravityWell(
	const FVector& ShipSystemPositionCm,
	const FVector& ShipSystemVelocityCmPerSec,
	EShipFlightMode FlightMode,
	double SimulationTimeSeconds,
	FGravityWellQueryResult& OutResult) const
{
	const FSimulationClockSnapshot ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(ActiveSystemDefinition.SystemId, SimulationTimeSeconds);
	return UOrbitRouteFrameQueryService::QueryNearestGravityWell(
		ActiveSystemDefinition,
		ShipSystemPositionCm,
		ShipSystemVelocityCmPerSec,
		FlightMode,
		ClockSnapshot,
		SimulationTimeSeconds,
		OutResult);
}

bool UStarSystemSubsystem::BuildSupercruiseTargetTelemetry(
	FName TargetId,
	const FVector& ObserverSystemPositionCm,
	const FVector& ObserverVelocityCmPerSec,
	double SimulationTimeSeconds,
	FSupercruiseTargetTelemetry& OutTelemetry) const
{
	FNavigationTargetViewModel TargetViewModel;
	if (!BuildNavigationTargetViewModel(TargetId, TargetId, ObserverSystemPositionCm, ObserverVelocityCmPerSec, SimulationTimeSeconds, TargetViewModel))
	{
		return false;
	}

	OutTelemetry = UOrbitRouteFrameQueryService::BuildSupercruiseTargetTelemetry(ActiveSystemDefinition.Scale, TargetViewModel);
	return true;
}

bool UStarSystemSubsystem::RefreshRegisteredEntityTransforms(double SimulationTimeSeconds)
{
	if (!bBuildComplete)
	{
		LastBuildError = TEXT("Cannot refresh entity transforms before a system is built.");
		return false;
	}

	const FSimulationClockSnapshot ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(ActiveSystemDefinition.SystemId, SimulationTimeSeconds);
	for (TPair<FName, FActiveSystemEntityEntry>& EntryPair : RegisteredEntities)
	{
		FFrameResolvedTransform ResolvedTransform;
		if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(ActiveSystemDefinition, EntryPair.Key, ClockSnapshot, SimulationTimeSeconds, ResolvedTransform))
		{
			LastBuildError = FString::Printf(TEXT("Failed to refresh entity transform for '%s'."), *EntryPair.Key.ToString());
			return false;
		}

		if (AActor* Actor = EntryPair.Value.Actor.Get())
		{
			Actor->SetActorTransform(FTransform(ResolvedTransform.Rotation, ResolvedTransform.PositionCm), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	return true;
}

void UStarSystemSubsystem::GetRegisteredEntityIds(TArray<FName>& OutEntityIds) const
{
	RegisteredEntities.GenerateKeyArray(OutEntityIds);
	OutEntityIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
}

void UStarSystemSubsystem::GetRegisteredEntities(TArray<FActiveSystemEntityEntry>& OutEntities) const
{
	RegisteredEntities.GenerateValueArray(OutEntities);
	OutEntities.Sort([](const FActiveSystemEntityEntry& Left, const FActiveSystemEntityEntry& Right)
	{
		return Left.EntityId.LexicalLess(Right.EntityId);
	});
}

void UStarSystemSubsystem::GetRegisteredSpawnZoneIds(TArray<FName>& OutSpawnZoneIds) const
{
	SpawnZonesById.GenerateKeyArray(OutSpawnZoneIds);
	OutSpawnZoneIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
}

void UStarSystemSubsystem::GetRegisteredGravityWellIds(TArray<FName>& OutGravityWellIds) const
{
	GravityWellsById.GenerateKeyArray(OutGravityWellIds);
	OutGravityWellIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
}

void UStarSystemSubsystem::GetRegisteredDockingPortIds(TArray<FName>& OutDockingPortIds) const
{
	DockingPortsById.GenerateKeyArray(OutDockingPortIds);
	OutDockingPortIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
}

void UStarSystemSubsystem::GetRegisteredDockingPorts(TArray<FDockingPortRegistryEntry>& OutDockingPorts) const
{
	DockingPortsById.GenerateValueArray(OutDockingPorts);
	OutDockingPorts.Sort([](const FDockingPortRegistryEntry& Left, const FDockingPortRegistryEntry& Right)
	{
		return MakeDockingPortRegistryId(Left.StationId, Left.PortId).LexicalLess(MakeDockingPortRegistryId(Right.StationId, Right.PortId));
	});
}

void UStarSystemSubsystem::GetRegisteredMapEntryIds(TArray<FName>& OutMapEntryIds) const
{
	MapEntriesById.GenerateKeyArray(OutMapEntryIds);
	OutMapEntryIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
}

bool UStarSystemSubsystem::FindSpawnZone(FName SpawnZoneId, FSpawnZoneDefinition& OutSpawnZone) const
{
	if (const FSpawnZoneDefinition* SpawnZone = SpawnZonesById.Find(SpawnZoneId))
	{
		OutSpawnZone = *SpawnZone;
		return true;
	}
	return false;
}

FString UStarSystemSubsystem::GetM0DebugSummary() const
{
	TArray<FActiveSystemEntityEntry> Entities;
	GetRegisteredEntities(Entities);

	TArray<FNavigationTargetDefinition> Targets;
	GetNavigationTargets(Targets);

	TArray<FString> EntityLines;
	for (const FActiveSystemEntityEntry& Entity : Entities)
	{
		EntityLines.Add(FString::Printf(TEXT("%s:%s"), *Entity.EntityId.ToString(), *Entity.EntityType.ToString()));
	}

	TArray<FString> TargetLines;
	for (const FNavigationTargetDefinition& Target : Targets)
	{
		if (Target.bCanTarget && Target.bShowInHud)
		{
			TargetLines.Add(FString::Printf(TEXT("%s:%s"), *Target.TargetId.ToString(), *Target.DisplayName.ToString()));
		}
	}

	return FString::Printf(
		TEXT("ActiveSystem=%s\nBuildComplete=%s\nLastBuildError=%s\nEntities=%s\nTargets=%s"),
		*GetActiveSystemId().ToString(),
		bBuildComplete ? TEXT("true") : TEXT("false"),
		*LastBuildError,
		*FString::Join(EntityLines, TEXT(",")),
			*FString::Join(TargetLines, TEXT(",")));
}

FString UStarSystemSubsystem::GetM1DebugSummary() const
{
	TArray<FName> EntityIds;
	GetRegisteredEntityIds(EntityIds);

	TArray<FName> SpawnZoneIds;
	GetRegisteredSpawnZoneIds(SpawnZoneIds);

	TArray<FName> DockingPortIds;
	GetRegisteredDockingPortIds(DockingPortIds);

	TArray<FName> GravityWellIds;
	GetRegisteredGravityWellIds(GravityWellIds);

	TArray<FName> MapEntryIds;
	GetRegisteredMapEntryIds(MapEntryIds);

	TArray<FNavigationTargetDefinition> Targets;
	GetNavigationTargets(Targets);

	TArray<FString> TargetIds;
	for (const FNavigationTargetDefinition& Target : Targets)
	{
		TargetIds.Add(Target.TargetId.ToString());
	}

	auto JoinNames = [](const TArray<FName>& Names)
	{
		TArray<FString> Strings;
		for (const FName Name : Names)
		{
			Strings.Add(Name.ToString());
		}
		return FString::Join(Strings, TEXT(","));
	};

	return FString::Printf(
		TEXT("ActiveSystem=%s\nBuildComplete=%s\nEntities=%s\nSpawnZones=%s\nDockingPorts=%s\nGravityWells=%s\nMapEntries=%s\nNavigationTargets=%s"),
		*GetActiveSystemId().ToString(),
		bBuildComplete ? TEXT("true") : TEXT("false"),
		*JoinNames(EntityIds),
		*JoinNames(SpawnZoneIds),
		*JoinNames(DockingPortIds),
		*JoinNames(GravityWellIds),
		*JoinNames(MapEntryIds),
		*FString::Join(TargetIds, TEXT(",")));
}

FString UStarSystemSubsystem::GetM3DebugSummary(
	FName SelectedTargetId,
	const FVector& ObserverSystemPositionCm,
	const FVector& ObserverVelocityCmPerSec,
	double SimulationTimeSeconds) const
{
	TArray<FNavigationTargetViewModel> TargetViewModels;
	BuildNavigationTargetViewModels(SelectedTargetId, ObserverSystemPositionCm, ObserverVelocityCmPerSec, SimulationTimeSeconds, TargetViewModels);

	TArray<FString> TargetLines;
	for (const FNavigationTargetViewModel& ViewModel : TargetViewModels)
	{
		TargetLines.Add(FString::Printf(
			TEXT("%s:%s:%.0f:%s:%s"),
			*ViewModel.TargetId.ToString(),
			*ViewModel.TargetType.ToString(),
			ViewModel.DistanceCm,
			ViewModel.bActorSpaceProjectionValid ? TEXT("projected") : TEXT("outside_bubble"),
			ViewModel.bIsSelected ? TEXT("selected") : TEXT("unselected")));
	}

	return FString::Printf(
		TEXT("ActiveSystem=%s\nBuildComplete=%s\nFlightMode=Normal\nSelectedTarget=%s\nObserverPositionCm=%s\nObserverVelocityCmPerSec=%s\nLocalBubbleRadiusCm=%.0f\nOriginShiftThresholdCm=%.0f\nStationApproachBubbleRadiusCm=%.0f\nNormalFlightMaxSpeedCmPerSec=%.0f\nTargetViewModels=%s"),
		*GetActiveSystemId().ToString(),
		bBuildComplete ? TEXT("true") : TEXT("false"),
		*SelectedTargetId.ToString(),
		*ObserverSystemPositionCm.ToCompactString(),
		*ObserverVelocityCmPerSec.ToCompactString(),
		ActiveSystemDefinition.Scale.LocalBubbleRadiusCm,
		ActiveSystemDefinition.Scale.OriginShiftThresholdCm,
		ActiveSystemDefinition.Scale.StationApproachBubbleRadiusCm,
		ActiveSystemDefinition.Scale.NormalFlightMaxSpeedCmPerSec,
		*FString::Join(TargetLines, TEXT(",")));
}

FString UStarSystemSubsystem::GetM4DebugSummary(
	FName SelectedTargetId,
	const FVector& ObserverSystemPositionCm,
	const FVector& ObserverVelocityCmPerSec,
	double SimulationTimeSeconds,
	const FSupercruiseTelemetry& Telemetry) const
{
	return FString::Printf(
		TEXT("ActiveSystem=%s\nBuildComplete=%s\nFlightMode=%s\nSupercruiseState=%s\nSelectedTarget=%s\nObserverPositionCm=%s\nObserverVelocityCmPerSec=%s\nSupercruiseSpeedCmPerSec=%.0f\nSpeedLimitCmPerSec=%.0f\nNearestGravityWell=%s\nGravityDistanceCm=%.0f\nInsideLockout=%s\nInsideDropout=%s\nTargetDistanceCm=%.0f\nTargetApproachReady=%s\nSupercruiseMinSpeedCmPerSec=%.0f\nSupercruiseMaxSpeedCmPerSec=%.0f\nDropoutBandCm=%.0f-%.0f"),
		*GetActiveSystemId().ToString(),
		bBuildComplete ? TEXT("true") : TEXT("false"),
		*UEnum::GetValueAsString(Telemetry.FlightMode),
		*UEnum::GetValueAsString(Telemetry.SupercruiseState),
		*SelectedTargetId.ToString(),
		*ObserverSystemPositionCm.ToCompactString(),
		*ObserverVelocityCmPerSec.ToCompactString(),
		Telemetry.CurrentSpeedCmPerSec,
		Telemetry.SpeedLimitCmPerSec,
		*Telemetry.Gravity.NearestWellId.ToString(),
		Telemetry.Gravity.DistanceCm,
		Telemetry.Gravity.bInsideLockout ? TEXT("true") : TEXT("false"),
		Telemetry.Gravity.bInsideDropout ? TEXT("true") : TEXT("false"),
		Telemetry.Target.DistanceCm,
		Telemetry.Target.bApproachReady ? TEXT("true") : TEXT("false"),
		ActiveSystemDefinition.Scale.SupercruiseMinSpeedCmPerSec,
		ActiveSystemDefinition.Scale.SupercruiseMaxSpeedCmPerSec,
		ActiveSystemDefinition.Scale.SupercruiseTargetDropoutMinRadiusCm,
		ActiveSystemDefinition.Scale.SupercruiseTargetDropoutMaxRadiusCm);
}

bool UStarSystemSubsystem::ValidateSystemForBuild(const FStarSystemDefinition& SystemDefinition, FString& OutError) const
{
	if (SystemDefinition.SystemId.IsNone())
	{
		OutError = TEXT("System definition has an empty system ID.");
		return false;
	}

	TSet<FName> EntityIds;
	auto AddEntityId = [&EntityIds, &OutError](FName EntityId, const TCHAR* EntityType)
	{
		if (EntityId.IsNone())
		{
			OutError = FString::Printf(TEXT("%s contains an empty ID."), EntityType);
			return false;
		}

		if (EntityIds.Contains(EntityId))
		{
			OutError = FString::Printf(TEXT("Duplicate active system entity ID '%s'."), *EntityId.ToString());
			return false;
		}

		EntityIds.Add(EntityId);
		return true;
	};

	TSet<FName> TargetIds;
	auto AddTargetId = [&TargetIds, &OutError](const FNavigationTargetDefinition& Target)
	{
		if (!Target.bCanTarget)
		{
			return true;
		}

		if (Target.TargetId.IsNone())
		{
			OutError = TEXT("Navigation target contains an empty ID.");
			return false;
		}

		if (TargetIds.Contains(Target.TargetId))
		{
			OutError = FString::Printf(TEXT("Duplicate navigation target ID '%s'."), *Target.TargetId.ToString());
			return false;
		}

		TargetIds.Add(Target.TargetId);
		return true;
	};

	for (const FBodyDefinition& Body : SystemDefinition.Bodies)
	{
		if (!AddEntityId(Body.BodyId, TEXT("Body")) || !AddTargetId(Body.NavigationTarget))
		{
			return false;
		}
	}

	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		if (!AddEntityId(Station.StationId, TEXT("Station")) || !AddTargetId(Station.NavigationTarget))
		{
			return false;
		}

		TSet<FName> PortIds;
		for (const FDockingPortDefinition& DockingPort : Station.DockingPorts)
		{
			if (DockingPort.PortId.IsNone())
			{
				OutError = FString::Printf(TEXT("Station '%s' contains an empty docking port ID."), *Station.StationId.ToString());
				return false;
			}
			if (PortIds.Contains(DockingPort.PortId))
			{
				OutError = FString::Printf(TEXT("Station '%s' contains duplicate docking port ID '%s'."), *Station.StationId.ToString(), *DockingPort.PortId.ToString());
				return false;
			}
			PortIds.Add(DockingPort.PortId);
		}
	}

	for (const FGateDefinition& Gate : SystemDefinition.Gates)
	{
		if (!AddEntityId(Gate.GateId, TEXT("Gate")) || !AddTargetId(Gate.NavigationTarget))
		{
			return false;
		}
	}

	TSet<FName> SpawnZoneIds;
	for (const FSpawnZoneDefinition& SpawnZone : SystemDefinition.SpawnZones)
	{
		if (SpawnZone.SpawnZoneId.IsNone())
		{
			OutError = TEXT("Spawn zone contains an empty ID.");
			return false;
		}

		if (SpawnZoneIds.Contains(SpawnZone.SpawnZoneId))
		{
			OutError = FString::Printf(TEXT("Duplicate spawn zone ID '%s'."), *SpawnZone.SpawnZoneId.ToString());
			return false;
		}

		SpawnZoneIds.Add(SpawnZone.SpawnZoneId);
	}

	TSet<FName> GravityWellIds;
	for (const FGravityWellDefinition& GravityWell : SystemDefinition.GravityWells)
	{
		if (GravityWell.WellId.IsNone())
		{
			OutError = TEXT("Gravity well contains an empty ID.");
			return false;
		}
		if (GravityWellIds.Contains(GravityWell.WellId))
		{
			OutError = FString::Printf(TEXT("Duplicate gravity well ID '%s'."), *GravityWell.WellId.ToString());
			return false;
		}
		GravityWellIds.Add(GravityWell.WellId);
	}

	TSet<FName> MapEntryIds;
	for (const FMapEntryDefinition& MapEntry : SystemDefinition.MapEntries)
	{
		if (MapEntry.MapEntryId.IsNone())
		{
			OutError = TEXT("Map entry contains an empty ID.");
			return false;
		}
		if (MapEntryIds.Contains(MapEntry.MapEntryId))
		{
			OutError = FString::Printf(TEXT("Duplicate map entry ID '%s'."), *MapEntry.MapEntryId.ToString());
			return false;
		}
		MapEntryIds.Add(MapEntry.MapEntryId);
	}

	return true;
}

bool UStarSystemSubsystem::RegisterEntity(FName EntityId, FName EntityType, const FTransform& Transform, double VisualRadiusCm)
{
	if (EntityId.IsNone())
	{
		LastBuildError = FString::Printf(TEXT("Cannot register %s with an empty ID."), *EntityType.ToString());
		return false;
	}

	if (RegisteredEntities.Contains(EntityId))
	{
		LastBuildError = FString::Printf(TEXT("Duplicate active system entity ID '%s'."), *EntityId.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LastBuildError = TEXT("Cannot register active system entity without a world.");
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AM0SystemMarkerActor* Marker = World->SpawnActor<AM0SystemMarkerActor>(AM0SystemMarkerActor::StaticClass(), Transform, SpawnParameters);
	if (!Marker)
	{
		LastBuildError = FString::Printf(TEXT("Failed to spawn marker for '%s'."), *EntityId.ToString());
		return false;
	}

	Marker->InitializeMarker(EntityId, EntityType, VisualRadiusCm);
	SpawnedSystemActors.Add(Marker);

	FActiveSystemEntityEntry Entry;
	Entry.EntityId = EntityId;
	Entry.EntityType = EntityType;
	Entry.BuildGeneration = ActiveBuildGeneration;
	Entry.Actor = Marker;
	RegisteredEntities.Add(EntityId, Entry);
	return true;
}

bool UStarSystemSubsystem::RegisterNavigationTarget(const FNavigationTargetDefinition& Target)
{
	if (!Target.bCanTarget)
	{
		return true;
	}

	if (Target.TargetId.IsNone())
	{
		LastBuildError = TEXT("Cannot register navigation target with an empty ID.");
		return false;
	}

	if (NavigationTargetsById.Contains(Target.TargetId))
	{
		LastBuildError = FString::Printf(TEXT("Duplicate navigation target ID '%s'."), *Target.TargetId.ToString());
		return false;
	}

	NavigationTargetsById.Add(Target.TargetId, Target);
	return true;
}

bool UStarSystemSubsystem::RegisterSpawnZone(const FSpawnZoneDefinition& SpawnZone)
{
	if (SpawnZone.SpawnZoneId.IsNone())
	{
		LastBuildError = TEXT("Cannot register spawn zone with an empty ID.");
		return false;
	}
	if (SpawnZonesById.Contains(SpawnZone.SpawnZoneId))
	{
		LastBuildError = FString::Printf(TEXT("Duplicate spawn zone ID '%s'."), *SpawnZone.SpawnZoneId.ToString());
		return false;
	}
	SpawnZonesById.Add(SpawnZone.SpawnZoneId, SpawnZone);
	return true;
}

bool UStarSystemSubsystem::RegisterDockingPort(FName StationId, const FDockingPortDefinition& DockingPort)
{
	if (StationId.IsNone() || DockingPort.PortId.IsNone())
	{
		LastBuildError = TEXT("Cannot register docking port with an empty station or port ID.");
		return false;
	}

	const FName RegistryId = MakeDockingPortRegistryId(StationId, DockingPort.PortId);
	if (DockingPortsById.Contains(RegistryId))
	{
		LastBuildError = FString::Printf(TEXT("Duplicate docking port registry ID '%s'."), *RegistryId.ToString());
		return false;
	}

	FDockingPortRegistryEntry Entry;
	Entry.StationId = StationId;
	Entry.PortId = DockingPort.PortId;
	Entry.BuildGeneration = ActiveBuildGeneration;
	Entry.Definition = DockingPort;
	DockingPortsById.Add(RegistryId, Entry);
	return true;
}

bool UStarSystemSubsystem::RegisterGravityWell(const FGravityWellDefinition& GravityWell)
{
	if (GravityWell.WellId.IsNone())
	{
		LastBuildError = TEXT("Cannot register gravity well with an empty ID.");
		return false;
	}
	if (GravityWellsById.Contains(GravityWell.WellId))
	{
		LastBuildError = FString::Printf(TEXT("Duplicate gravity well ID '%s'."), *GravityWell.WellId.ToString());
		return false;
	}
	GravityWellsById.Add(GravityWell.WellId, GravityWell);
	return true;
}

bool UStarSystemSubsystem::RegisterMapEntry(const FMapEntryDefinition& MapEntry)
{
	if (MapEntry.MapEntryId.IsNone())
	{
		LastBuildError = TEXT("Cannot register map entry with an empty ID.");
		return false;
	}
	if (MapEntriesById.Contains(MapEntry.MapEntryId))
	{
		LastBuildError = FString::Printf(TEXT("Duplicate map entry ID '%s'."), *MapEntry.MapEntryId.ToString());
		return false;
	}
	MapEntriesById.Add(MapEntry.MapEntryId, MapEntry);
	return true;
}

FName UStarSystemSubsystem::MakeDockingPortRegistryId(FName StationId, FName PortId)
{
	return FName(*FString::Printf(TEXT("%s/%s"), *StationId.ToString(), *PortId.ToString()));
}
