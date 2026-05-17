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

bool UStarSystemSubsystem::BuildSystem(const FStarSystemDefinition& SystemDefinition, double InitialSimulationTimeSeconds)
{
	TearDownActiveSystem();
	++ActiveBuildGeneration;
	LastBuildError.Reset();
	LastBuildValidationReport = FStargameValidationReport();
	LastBuildValidationReport.Profile = EStargameValidationProfile::Build;
	LastBuildValidationReport.SystemId = SystemDefinition.SystemId;
	LastBuildValidationReport.CheckedSystemCount = SystemDefinition.SystemId.IsNone() ? 0 : 1;

	if (!ValidateSystemForBuild(SystemDefinition, LastBuildError))
	{
		AddBuildValidationIssue(TEXT("active_system_build_validation_failed"), SystemDefinition.SystemId, LastBuildError);
		return false;
	}

	ActiveSystemDefinition = SystemDefinition;
	const FSimulationClockSnapshot BuildClock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, InitialSimulationTimeSeconds);
	for (const FBodyDefinition& Body : SystemDefinition.Bodies)
	{
		FFrameResolvedTransform ResolvedTransform;
		if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, Body.BodyId, BuildClock, BuildClock.AuthoritativeSimulationTimeSeconds, ResolvedTransform))
		{
			LastBuildError = FString::Printf(TEXT("Failed to resolve body frame '%s' before active system build."), *Body.BodyId.ToString());
			AddBuildValidationIssue(TEXT("active_system_body_frame_unresolved"), Body.BodyId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}

		if (!RegisterEntity(Body.BodyId, TEXT("body"), FTransform(ResolvedTransform.Rotation, ResolvedTransform.PositionCm), Body.VisualRadiusCm) ||
			!RegisterNavigationTarget(Body.NavigationTarget))
		{
			AddBuildValidationIssue(TEXT("active_system_body_registration_failed"), Body.BodyId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FStationDefinition& Station : SystemDefinition.Stations)
	{
		FFrameResolvedTransform ResolvedTransform;
		if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, Station.StationId, BuildClock, BuildClock.AuthoritativeSimulationTimeSeconds, ResolvedTransform))
		{
			LastBuildError = FString::Printf(TEXT("Failed to resolve station frame '%s' before active system build."), *Station.StationId.ToString());
			AddBuildValidationIssue(TEXT("active_system_station_frame_unresolved"), Station.StationId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}

		if (!RegisterEntity(Station.StationId, TEXT("station"), FTransform(ResolvedTransform.Rotation, ResolvedTransform.PositionCm), Station.VisualRadiusCm) ||
			!RegisterNavigationTarget(Station.NavigationTarget))
		{
			AddBuildValidationIssue(TEXT("active_system_station_registration_failed"), Station.StationId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}

		for (const FDockingPortDefinition& DockingPort : Station.DockingPorts)
		{
			if (!RegisterDockingPort(Station.StationId, DockingPort))
			{
				AddBuildValidationIssue(TEXT("active_system_docking_port_registration_failed"), DockingPort.PortId, LastBuildError);
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
			LastBuildError = FString::Printf(TEXT("Failed to resolve gate frame '%s' before active system build."), *Gate.GateId.ToString());
			AddBuildValidationIssue(TEXT("active_system_gate_frame_unresolved"), Gate.GateId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}

		if (!RegisterEntity(Gate.GateId, TEXT("gate"), FTransform(ResolvedTransform.Rotation, ResolvedTransform.PositionCm), Gate.VisualRadiusCm) ||
			!RegisterNavigationTarget(Gate.NavigationTarget))
		{
			AddBuildValidationIssue(TEXT("active_system_gate_registration_failed"), Gate.GateId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FSpawnZoneDefinition& SpawnZone : SystemDefinition.SpawnZones)
	{
		if (!RegisterSpawnZone(SpawnZone))
		{
			AddBuildValidationIssue(TEXT("active_system_spawn_zone_registration_failed"), SpawnZone.SpawnZoneId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FGravityWellDefinition& GravityWell : SystemDefinition.GravityWells)
	{
		if (!RegisterGravityWell(GravityWell))
		{
			AddBuildValidationIssue(TEXT("active_system_gravity_well_registration_failed"), GravityWell.WellId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FResourceZoneDefinition& ResourceZone : SystemDefinition.ResourceZones)
	{
		FFrameResolvedTransform ResolvedTransform;
		if (!UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, ResourceZone.ZoneId, BuildClock, BuildClock.AuthoritativeSimulationTimeSeconds, ResolvedTransform))
		{
			LastBuildError = FString::Printf(TEXT("Failed to resolve resource zone frame '%s' before active system build."), *ResourceZone.ZoneId.ToString());
			AddBuildValidationIssue(TEXT("active_system_resource_zone_frame_unresolved"), ResourceZone.ZoneId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}

		if (!RegisterEntity(ResourceZone.ZoneId, TEXT("resource"), FTransform(ResolvedTransform.Rotation, ResolvedTransform.PositionCm), ResourceZone.RadiusCm) ||
			!RegisterNavigationTarget(ResourceZone.NavigationTarget) ||
			!RegisterResourceZone(ResourceZone))
		{
			AddBuildValidationIssue(TEXT("active_system_resource_zone_registration_failed"), ResourceZone.ZoneId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}
	}

	for (const FMapEntryDefinition& MapEntry : SystemDefinition.MapEntries)
	{
		if (!RegisterMapEntry(MapEntry))
		{
			AddBuildValidationIssue(TEXT("active_system_map_entry_registration_failed"), MapEntry.MapEntryId, LastBuildError);
			TearDownActiveSystem();
			return false;
		}
	}

	bBuildComplete = true;
	LastBuildValidationReport.RefreshSummary();
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
	ActivePlayerPawn.Reset();
	RegisteredEntities.Reset();
	NavigationTargetsById.Reset();
	SpawnZonesById.Reset();
	DockingPortsById.Reset();
	DockingPortRuntimeStatesById.Reset();
	GravityWellsById.Reset();
	ResourceZonesById.Reset();
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
			LastBuildError = TEXT("Cannot spawn player without an authored pawn class.");
			return false;
		}
		Pawn = World->SpawnActor<APawn>(SpawnClass, SpawnZone.Transform, SpawnParameters);
		if (PlayerController && Pawn)
		{
			PlayerController->Possess(Pawn);
		}
	}
	else
	{
		if (PawnClass && !Pawn->IsA(PawnClass))
		{
			LastBuildError = FString::Printf(TEXT("Existing player pawn '%s' does not match authored pawn class '%s'."),
				*Pawn->GetClass()->GetName(),
				*PawnClass->GetName());
			return false;
		}
		Pawn->SetActorTransform(SpawnZone.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	ActivePlayerPawn = Pawn;
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

void UStarSystemSubsystem::GetRegisteredResourceZoneIds(TArray<FName>& OutResourceZoneIds) const
{
	ResourceZonesById.GenerateKeyArray(OutResourceZoneIds);
	OutResourceZoneIds.Sort([](const FName& Left, const FName& Right)
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

bool UStarSystemSubsystem::FindDockingPort(FName StationId, FName PortId, FDockingPortRegistryEntry& OutDockingPort) const
{
	if (const FDockingPortRegistryEntry* Entry = DockingPortsById.Find(MakeDockingPortRegistryId(StationId, PortId)))
	{
		OutDockingPort = *Entry;
		return true;
	}
	return false;
}

bool UStarSystemSubsystem::FindDockingPortRuntimeState(FName StationId, FName PortId, FDockingPortRuntimeState& OutRuntimeState) const
{
	if (const FDockingPortRuntimeState* RuntimeState = DockingPortRuntimeStatesById.Find(MakeDockingPortRegistryId(StationId, PortId)))
	{
		OutRuntimeState = *RuntimeState;
		return true;
	}
	return false;
}

bool UStarSystemSubsystem::TryReserveDockingPort(FName StationId, FName PortId, FName ShipInstanceId, double SimulationTimeSeconds, FDockingPortRuntimeState& OutRuntimeState, FString& OutFailureReason)
{
	OutRuntimeState = FDockingPortRuntimeState();
	OutFailureReason.Reset();

	FDockingPortRuntimeState* RuntimeState = DockingPortRuntimeStatesById.Find(MakeDockingPortRegistryId(StationId, PortId));
	if (!RuntimeState)
	{
		OutFailureReason = TEXT("Docking port is not registered.");
		return false;
	}
	if (!RuntimeState->OccupyingShipId.IsNone() && RuntimeState->OccupyingShipId != ShipInstanceId)
	{
		RuntimeState->LastFailureReason = TEXT("Docking port is occupied.");
		OutFailureReason = RuntimeState->LastFailureReason;
		OutRuntimeState = *RuntimeState;
		return false;
	}
	const bool bReservationExpired = RuntimeState->ReservationExpiryTimeSeconds > 0.0 && RuntimeState->ReservationExpiryTimeSeconds <= SimulationTimeSeconds;
	if (bReservationExpired)
	{
		RuntimeState->ReservedShipId = NAME_None;
		RuntimeState->ClearanceId = NAME_None;
		RuntimeState->ReservationExpiryTimeSeconds = 0.0;
	}
	if (!RuntimeState->ReservedShipId.IsNone() && RuntimeState->ReservedShipId != ShipInstanceId)
	{
		RuntimeState->LastFailureReason = TEXT("Docking port is reserved.");
		OutFailureReason = RuntimeState->LastFailureReason;
		OutRuntimeState = *RuntimeState;
		return false;
	}

	RuntimeState->ReservedShipId = ShipInstanceId;
	RuntimeState->ClearanceId = FName(*FString::Printf(TEXT("%s_%s_%d"), *StationId.ToString(), *PortId.ToString(), FMath::RoundToInt(SimulationTimeSeconds)));
	RuntimeState->ReservationExpiryTimeSeconds = SimulationTimeSeconds + 60.0;
	RuntimeState->DockingState = EDockingState::Reserved;
	RuntimeState->LastFailureReason.Reset();
	OutRuntimeState = *RuntimeState;
	return true;
}

bool UStarSystemSubsystem::OccupyDockingPort(FName StationId, FName PortId, FName ShipInstanceId, FDockingPortRuntimeState& OutRuntimeState, FString& OutFailureReason)
{
	OutRuntimeState = FDockingPortRuntimeState();
	OutFailureReason.Reset();

	FDockingPortRuntimeState* RuntimeState = DockingPortRuntimeStatesById.Find(MakeDockingPortRegistryId(StationId, PortId));
	if (!RuntimeState)
	{
		OutFailureReason = TEXT("Docking port is not registered.");
		return false;
	}
	if (!RuntimeState->OccupyingShipId.IsNone() && RuntimeState->OccupyingShipId != ShipInstanceId)
	{
		RuntimeState->LastFailureReason = TEXT("Docking port is occupied.");
		OutFailureReason = RuntimeState->LastFailureReason;
		OutRuntimeState = *RuntimeState;
		return false;
	}
	if (!RuntimeState->ReservedShipId.IsNone() && RuntimeState->ReservedShipId != ShipInstanceId)
	{
		RuntimeState->LastFailureReason = TEXT("Docking port is reserved by another ship.");
		OutFailureReason = RuntimeState->LastFailureReason;
		OutRuntimeState = *RuntimeState;
		return false;
	}

	RuntimeState->OccupyingShipId = ShipInstanceId;
	RuntimeState->ReservedShipId = NAME_None;
	RuntimeState->ReservationExpiryTimeSeconds = 0.0;
	RuntimeState->DockingState = EDockingState::Docked;
	RuntimeState->LastFailureReason.Reset();
	OutRuntimeState = *RuntimeState;
	return true;
}

bool UStarSystemSubsystem::RestoreDockingPortRuntimeState(const FDockingPortRuntimeState& SavedRuntimeState, FDockingPortRuntimeState& OutRuntimeState, FString& OutFailureReason)
{
	OutRuntimeState = FDockingPortRuntimeState();
	OutFailureReason.Reset();

	FDockingPortRuntimeState* RuntimeState = DockingPortRuntimeStatesById.Find(MakeDockingPortRegistryId(SavedRuntimeState.StationId, SavedRuntimeState.PortId));
	if (!RuntimeState)
	{
		OutFailureReason = TEXT("Docking port is not registered.");
		return false;
	}
	if (!RuntimeState->OccupyingShipId.IsNone() && RuntimeState->OccupyingShipId != SavedRuntimeState.OccupyingShipId)
	{
		RuntimeState->LastFailureReason = TEXT("Docking port is occupied by another ship.");
		OutFailureReason = RuntimeState->LastFailureReason;
		OutRuntimeState = *RuntimeState;
		return false;
	}

	RuntimeState->ReservedShipId = SavedRuntimeState.ReservedShipId;
	RuntimeState->OccupyingShipId = SavedRuntimeState.OccupyingShipId;
	RuntimeState->ClearanceId = SavedRuntimeState.ClearanceId;
	RuntimeState->ReservationExpiryTimeSeconds = SavedRuntimeState.ReservationExpiryTimeSeconds;
	RuntimeState->DockingState = SavedRuntimeState.DockingState;
	RuntimeState->LastFailureReason.Reset();
	OutRuntimeState = *RuntimeState;
	return true;
}

bool UStarSystemSubsystem::ReleaseDockingPort(FName StationId, FName PortId, FName ShipInstanceId)
{
	FDockingPortRuntimeState* RuntimeState = DockingPortRuntimeStatesById.Find(MakeDockingPortRegistryId(StationId, PortId));
	if (!RuntimeState)
	{
		return false;
	}
	if ((!RuntimeState->OccupyingShipId.IsNone() && RuntimeState->OccupyingShipId != ShipInstanceId) ||
		(!RuntimeState->ReservedShipId.IsNone() && RuntimeState->ReservedShipId != ShipInstanceId))
	{
		RuntimeState->LastFailureReason = TEXT("Docking port belongs to another ship.");
		return false;
	}

	RuntimeState->ReservedShipId = NAME_None;
	RuntimeState->OccupyingShipId = NAME_None;
	RuntimeState->ClearanceId = NAME_None;
	RuntimeState->ReservationExpiryTimeSeconds = 0.0;
	RuntimeState->DockingState = EDockingState::None;
	RuntimeState->LastFailureReason.Reset();
	return true;
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

	TArray<FName> ResourceZoneIds;
	GetRegisteredResourceZoneIds(ResourceZoneIds);

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
		TEXT("ActiveSystem=%s\nBuildComplete=%s\nEntities=%s\nSpawnZones=%s\nDockingPorts=%s\nGravityWells=%s\nResourceZones=%s\nMapEntries=%s\nNavigationTargets=%s"),
		*GetActiveSystemId().ToString(),
		bBuildComplete ? TEXT("true") : TEXT("false"),
		*JoinNames(EntityIds),
		*JoinNames(SpawnZoneIds),
		*JoinNames(DockingPortIds),
		*JoinNames(GravityWellIds),
		*JoinNames(ResourceZoneIds),
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

	TSet<FName> ResourceZoneIds;
	for (const FResourceZoneDefinition& ResourceZone : SystemDefinition.ResourceZones)
	{
		if (!AddEntityId(ResourceZone.ZoneId, TEXT("Resource zone")) || !AddTargetId(ResourceZone.NavigationTarget))
		{
			return false;
		}
		if (ResourceZoneIds.Contains(ResourceZone.ZoneId))
		{
			OutError = FString::Printf(TEXT("Duplicate resource zone ID '%s'."), *ResourceZone.ZoneId.ToString());
			return false;
		}
		ResourceZoneIds.Add(ResourceZone.ZoneId);
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

	TSet<FName> TrafficRouteIds;
	const FSimulationClockSnapshot ValidationClock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		if (Route.RouteSegmentId.IsNone())
		{
			OutError = TEXT("Traffic route contains an empty route segment ID.");
			return false;
		}
		if (TrafficRouteIds.Contains(Route.RouteSegmentId))
		{
			OutError = FString::Printf(TEXT("Duplicate traffic route segment ID '%s'."), *Route.RouteSegmentId.ToString());
			return false;
		}
		if (Route.SourceAnchorId.IsNone() || Route.DestinationAnchorId.IsNone())
		{
			OutError = FString::Printf(TEXT("Traffic route '%s' contains an empty endpoint anchor."), *Route.RouteSegmentId.ToString());
			return false;
		}
		FRouteSample RouteSample;
		if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Route.RouteSegmentId, 0.0, ValidationClock, 0.0, RouteSample) ||
			!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, Route.RouteSegmentId, 1.0, ValidationClock, 0.0, RouteSample))
		{
			OutError = FString::Printf(TEXT("Traffic route '%s' could not resolve its endpoint frames."), *Route.RouteSegmentId.ToString());
			return false;
		}
		TrafficRouteIds.Add(Route.RouteSegmentId);
	}

	TSet<FName> LogicalShipIds;
	for (const FShipTrafficInstance& Ship : SystemDefinition.LogicalTraffic)
	{
		if (Ship.ShipInstanceId.IsNone())
		{
			OutError = TEXT("Logical traffic contains an empty ship instance ID.");
			return false;
		}
		if (LogicalShipIds.Contains(Ship.ShipInstanceId))
		{
			OutError = FString::Printf(TEXT("Duplicate logical traffic ship instance ID '%s'."), *Ship.ShipInstanceId.ToString());
			return false;
		}
		if (Ship.CurrentGoal.GoalKind == EShipGoalKind::TradeRoute &&
			(Ship.CurrentGoal.RouteSegmentId.IsNone() || !TrafficRouteIds.Contains(Ship.CurrentGoal.RouteSegmentId)))
		{
			OutError = FString::Printf(TEXT("Logical traffic ship '%s' references missing trade route '%s'."),
				*Ship.ShipInstanceId.ToString(),
				*Ship.CurrentGoal.RouteSegmentId.ToString());
			return false;
		}
		if (Ship.bHasSavedGoal && Ship.SavedGoal.GoalKind == EShipGoalKind::TradeRoute &&
			(Ship.SavedGoal.RouteSegmentId.IsNone() || !TrafficRouteIds.Contains(Ship.SavedGoal.RouteSegmentId)))
		{
			OutError = FString::Printf(TEXT("Logical traffic ship '%s' has a saved goal referencing missing trade route '%s'."),
				*Ship.ShipInstanceId.ToString(),
				*Ship.SavedGoal.RouteSegmentId.ToString());
			return false;
		}
		LogicalShipIds.Add(Ship.ShipInstanceId);
	}

	TSet<FName> ShipGroupIds;
	for (const FShipGroupState& Group : SystemDefinition.ShipGroups)
	{
		if (Group.GroupId.IsNone())
		{
			OutError = TEXT("Ship group contains an empty group ID.");
			return false;
		}
		if (ShipGroupIds.Contains(Group.GroupId))
		{
			OutError = FString::Printf(TEXT("Duplicate ship group ID '%s'."), *Group.GroupId.ToString());
			return false;
		}
		if (!Group.LeaderShipInstanceId.IsNone() && !LogicalShipIds.Contains(Group.LeaderShipInstanceId))
		{
			OutError = FString::Printf(TEXT("Ship group '%s' references missing leader ship '%s'."),
				*Group.GroupId.ToString(),
				*Group.LeaderShipInstanceId.ToString());
			return false;
		}
		for (const FName MemberShipInstanceId : Group.MemberShipInstanceIds)
		{
			if (MemberShipInstanceId.IsNone() || !LogicalShipIds.Contains(MemberShipInstanceId))
			{
				OutError = FString::Printf(TEXT("Ship group '%s' references missing member ship '%s'."),
					*Group.GroupId.ToString(),
					*MemberShipInstanceId.ToString());
				return false;
			}
		}
		for (const FShipFormationSlot& Slot : Group.FormationSlots)
		{
			if (Slot.SlotId.IsNone() || Slot.ShipInstanceId.IsNone() || !LogicalShipIds.Contains(Slot.ShipInstanceId))
			{
				OutError = FString::Printf(TEXT("Ship group '%s' has an invalid formation slot for ship '%s'."),
					*Group.GroupId.ToString(),
					*Slot.ShipInstanceId.ToString());
				return false;
			}
		}
		ShipGroupIds.Add(Group.GroupId);
	}

	for (const FShipTrafficInstance& Ship : SystemDefinition.LogicalTraffic)
	{
		if (!Ship.GroupId.IsNone() && !ShipGroupIds.Contains(Ship.GroupId))
		{
			OutError = FString::Printf(TEXT("Logical traffic ship '%s' references missing group '%s'."),
				*Ship.ShipInstanceId.ToString(),
				*Ship.GroupId.ToString());
			return false;
		}
	}

	return true;
}

void UStarSystemSubsystem::AddBuildValidationIssue(FName Code, FName ObjectId, const FString& Message)
{
	FStargameValidationIssue Issue;
	Issue.Severity = EStargameValidationSeverity::Error;
	Issue.Code = Code;
	Issue.RuleId = Code;
	Issue.ObjectId = ObjectId;
	Issue.SystemId = LastBuildValidationReport.SystemId;
	Issue.SourceId = ObjectId;
	Issue.Message = Message;
	LastBuildValidationReport.Issues.Add(Issue);
	LastBuildValidationReport.RefreshSummary();
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

	FDockingPortRuntimeState RuntimeState;
	RuntimeState.StationId = StationId;
	RuntimeState.PortId = DockingPort.PortId;
	RuntimeState.DockingState = EDockingState::None;
	DockingPortRuntimeStatesById.Add(RegistryId, RuntimeState);
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

bool UStarSystemSubsystem::RegisterResourceZone(const FResourceZoneDefinition& ResourceZone)
{
	if (ResourceZone.ZoneId.IsNone())
	{
		LastBuildError = TEXT("Cannot register resource zone with an empty ID.");
		return false;
	}
	if (ResourceZonesById.Contains(ResourceZone.ZoneId))
	{
		LastBuildError = FString::Printf(TEXT("Duplicate resource zone ID '%s'."), *ResourceZone.ZoneId.ToString());
		return false;
	}
	ResourceZonesById.Add(ResourceZone.ZoneId, ResourceZone);
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
