#include "Space/StarSystemSubsystem.h"

#include "Data/StarCatalogSubsystem.h"
#include "Flight/SpaceFlightPawn.h"
#include "GameFramework/PlayerController.h"
#include "Space/CombatShotPresentationActor.h"
#include "Space/LogicalTrafficActor.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Space/M0SystemMarkerActor.h"
#include "Space/SectorStarAnchorActor.h"
#include "Space/SystemSpacePresentationActor.h"

namespace
{
	constexpr double DistantSystemRenderShellRadiusCm = 4000000.0;

	const FShipGroupState* FindTrafficGroup(const FActiveTrafficSimulationState& TrafficState, FName GroupId)
	{
		return TrafficState.Groups.FindByPredicate([GroupId](const FShipGroupState& Candidate)
		{
			return Candidate.GroupId == GroupId;
		});
	}

	EStargameStellarClass ResolveStellarClass(FName StellarClassName)
	{
		const FString Normalized = StellarClassName.ToString().TrimStartAndEnd().ToUpper();
		if (Normalized.Contains(TEXT("BLUE"))) { return EStargameStellarClass::BlueGiant; }
		if (Normalized.Contains(TEXT("RED"))) { return EStargameStellarClass::RedGiant; }
		if (Normalized.Contains(TEXT("WHITE"))) { return EStargameStellarClass::WhiteDwarf; }
		if (Normalized.StartsWith(TEXT("O"))) { return EStargameStellarClass::O; }
		if (Normalized.StartsWith(TEXT("B"))) { return EStargameStellarClass::B; }
		if (Normalized.StartsWith(TEXT("A"))) { return EStargameStellarClass::A; }
		if (Normalized.StartsWith(TEXT("F"))) { return EStargameStellarClass::F; }
		if (Normalized.StartsWith(TEXT("K"))) { return EStargameStellarClass::K; }
		if (Normalized.StartsWith(TEXT("M"))) { return EStargameStellarClass::M; }
		if (Normalized.StartsWith(TEXT("L"))) { return EStargameStellarClass::L; }
		return EStargameStellarClass::G;
	}

	const FShipFormationSlot* FindFormationSlot(const FShipGroupState* Group, FName ShipInstanceId, FName SlotId)
	{
		if (!Group)
		{
			return nullptr;
		}
		return Group->FormationSlots.FindByPredicate([ShipInstanceId, SlotId](const FShipFormationSlot& Candidate)
		{
			return Candidate.ShipInstanceId == ShipInstanceId && (SlotId.IsNone() || Candidate.SlotId == SlotId);
		});
	}

	bool ResolveTrafficShipTransform(
		const FStarSystemDefinition& SystemDefinition,
		const FActiveTrafficSimulationState& TrafficState,
		const FShipTrafficInstance& Ship,
		const FSimulationClockSnapshot& ClockSnapshot,
		double SimulationTimeSeconds,
		FRouteSample& OutSample,
		FTransform& OutTransform)
	{
		const FName RouteSegmentId = Ship.CurrentGoal.RouteSegmentId;
		if (RouteSegmentId.IsNone())
		{
			return false;
		}

		double RouteProgress = Ship.CurrentGoal.RouteProgress01;
		const FShipGroupState* Group = FindTrafficGroup(TrafficState, Ship.GroupId);
		const FShipFormationSlot* Slot = FindFormationSlot(Group, Ship.ShipInstanceId, Ship.FormationSlotId);
		if (Slot)
		{
			RouteProgress += Slot->RouteProgressOffset;
		}
		RouteProgress = FMath::Clamp(RouteProgress, 0.0, 1.0);

		if (!UOrbitRouteFrameQueryService::EvaluateRoute(SystemDefinition, RouteSegmentId, RouteProgress, ClockSnapshot, SimulationTimeSeconds, OutSample))
		{
			return false;
		}

		FVector PositionCm = OutSample.ResolvedTransform.PositionCm;
		if (Slot)
		{
			const FVector Forward = OutSample.Tangent.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			const FVector UpSeed = FMath::Abs(FVector::DotProduct(Forward, FVector::UpVector)) > 0.95 ? FVector::RightVector : FVector::UpVector;
			const FQuat Basis = FRotationMatrix::MakeFromXZ(Forward, UpSeed).ToQuat();
			PositionCm += Basis.RotateVector(Slot->LocalOffsetCm);
		}

		const FQuat Rotation = FRotationMatrix::MakeFromX(OutSample.Tangent.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector)).ToQuat();
		OutTransform = FTransform(Rotation, PositionCm);
		return true;
	}

	TSubclassOf<ALogicalTrafficActor> ResolveTrafficActorClass(const UGameInstance* GameInstance, const FShipTrafficInstance& Ship)
	{
		TSubclassOf<ALogicalTrafficActor> ResolvedClass;
		if (!GameInstance || Ship.ShipArchetypeId.IsNone())
		{
			return ALogicalTrafficActor::StaticClass();
		}

		const UStarCatalogSubsystem* Catalog = GameInstance->GetSubsystem<UStarCatalogSubsystem>();
		FShipArchetypeDefinition Archetype;
		if (!Catalog || !Catalog->ResolveShipArchetype(Ship.ShipArchetypeId, Archetype) || Archetype.ActorClass.IsNull())
		{
			return ALogicalTrafficActor::StaticClass();
		}

		UClass* ActorClass = Archetype.ActorClass.LoadSynchronous();
		if (ActorClass && ActorClass->IsChildOf(ALogicalTrafficActor::StaticClass()))
		{
			ResolvedClass = ActorClass;
		}
		if (!ResolvedClass)
		{
			ResolvedClass = ALogicalTrafficActor::StaticClass();
		}
		return ResolvedClass;
	}

	template <typename TActor>
	TSubclassOf<TActor> ResolveAuthoredActorClass(const TSoftClassPtr<AActor>& ActorClass)
	{
		if (ActorClass.IsNull())
		{
			return nullptr;
		}

		UClass* LoadedClass = ActorClass.LoadSynchronous();
		if (LoadedClass && LoadedClass->IsChildOf(TActor::StaticClass()))
		{
			return LoadedClass;
		}
		return nullptr;
	}

	FVector BuildEncounterOffset(const FRouteSample& Sample, double LateralOffsetCm, double ForwardOffsetCm)
	{
		const FVector Forward = Sample.Tangent.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FVector UpSeed = FMath::Abs(FVector::DotProduct(Forward, FVector::UpVector)) > 0.95 ? FVector::RightVector : FVector::UpVector;
		const FVector Right = FVector::CrossProduct(UpSeed, Forward).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		return Right * LateralOffsetCm + Forward * ForwardOffsetCm;
	}

	FTransform BuildEncounterActorTransform(const FRouteSample& Sample, double LateralOffsetCm, double ForwardOffsetCm)
	{
		const FVector Forward = Sample.Tangent.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FVector PositionCm = Sample.ResolvedTransform.PositionCm + BuildEncounterOffset(Sample, LateralOffsetCm, ForwardOffsetCm);
		return FTransform(FRotationMatrix::MakeFromX(Forward).ToQuat(), PositionCm);
	}

	bool IsEncounterRecordLive(FName State, double ExpiresAtTimeSeconds, double SimulationTimeSeconds)
	{
		return State != FName(TEXT("resolved")) &&
			State != FName(TEXT("expired")) &&
			State != FName(TEXT("cancelled")) &&
			(ExpiresAtTimeSeconds <= 0.0 || ExpiresAtTimeSeconds >= SimulationTimeSeconds);
	}

	bool IsSteeringIntentLive(FName State)
	{
		return State != FName(TEXT("complete")) &&
			State != FName(TEXT("resolved")) &&
			State != FName(TEXT("cancelled"));
	}

	const FRealizedAISteeringIntent* FindSteeringIntentForShip(
		const FSystemicGameplayState& SystemicState,
		FName ShipInstanceId,
		FName PreferredIntentType)
	{
		const FRealizedAISteeringIntent* Fallback = nullptr;
		for (const FRealizedAISteeringIntent& Intent : SystemicState.RealizedSteeringIntents)
		{
			if (Intent.ShipInstanceId != ShipInstanceId || !IsSteeringIntentLive(Intent.State))
			{
				continue;
			}
			if (!PreferredIntentType.IsNone() && Intent.IntentType == PreferredIntentType)
			{
				return &Intent;
			}
			if (!Fallback)
			{
				Fallback = &Intent;
			}
		}
		return Fallback;
	}

	FName ResolvePirateBehaviorVariant(const FInterdictionHazardRecord& Hazard)
	{
		// Enhanced Unreal posture layer: Godot supplies the hostile AI state contract,
		// while systemic scores choose local encounter staging and presentation flavor.
		if (Hazard.DynamicPirateAmbushScore >= 0.70 || Hazard.SelectionDebugReason.Contains(TEXT("CriminalPressure=")))
		{
			return TEXT("pirate_high_pressure_intercept");
		}
		if (Hazard.DynamicPirateAmbushScore >= 0.45)
		{
			return TEXT("pirate_trade_lane_probe");
		}
		return TEXT("pirate_shadow_route");
	}

	FName ResolvePirateCommsVariant(const FInterdictionHazardRecord& Hazard)
	{
		if (Hazard.DynamicPirateAmbushScore >= 0.70)
		{
			return TEXT("comms_pirate_hard_demand");
		}
		if (Hazard.DynamicPirateAmbushScore >= 0.45)
		{
			return TEXT("comms_pirate_cargo_scan");
		}
		return TEXT("comms_pirate_silent_stalk");
	}

	FVector2D ResolvePirateLocalOffsetCm(FName BehaviorVariantId)
	{
		if (BehaviorVariantId == FName(TEXT("pirate_high_pressure_intercept")))
		{
			return FVector2D(1600.0, -700.0);
		}
		if (BehaviorVariantId == FName(TEXT("pirate_shadow_route")))
		{
			return FVector2D(7200.0, -5400.0);
		}
		return FVector2D(3500.0, -1800.0);
	}

	FName ResolvePirateLocalBehaviorState(FName BehaviorVariantId)
	{
		if (BehaviorVariantId == FName(TEXT("pirate_high_pressure_intercept")))
		{
			return TEXT("local_intercept_closing");
		}
		if (BehaviorVariantId == FName(TEXT("pirate_shadow_route")))
		{
			return TEXT("local_shadowing");
		}
		return TEXT("local_probe_scan");
	}

	FString ResolvePirateCommsLine(FName CommsVariantId)
	{
		if (CommsVariantId == FName(TEXT("comms_pirate_hard_demand")))
		{
			return TEXT("Cut engines and prepare to dump cargo.");
		}
		if (CommsVariantId == FName(TEXT("comms_pirate_silent_stalk")))
		{
			return TEXT("Unmarked contact is shadowing the route.");
		}
		return TEXT("Hold course while we scan your hold.");
	}

	double ResolveEncounterSteeringSpeedCmPerSec(FName LocalBehaviorStateId)
	{
		if (LocalBehaviorStateId == FName(TEXT("local_intercept_closing")) ||
			LocalBehaviorStateId == FName(TEXT("local_priority_intercept")))
		{
			return 18000.0;
		}
		if (LocalBehaviorStateId == FName(TEXT("local_shadowing")) ||
			LocalBehaviorStateId == FName(TEXT("local_presence_patrol")))
		{
			return 6500.0;
		}
		return 11000.0;
	}

	FTransform BuildSteeredEncounterTransform(
		const FTransform& DesiredTransform,
		const FRealizedEncounterActorEntry* ExistingEntry,
		FName LocalBehaviorStateId,
		double SimulationTimeSeconds,
		double& OutDistanceToDesiredCm,
		double& OutDistanceTrendCm,
		bool& bOutSteeringActive)
	{
		const FVector DesiredPositionCm = DesiredTransform.GetLocation();
		OutDistanceToDesiredCm = 0.0;
		OutDistanceTrendCm = 0.0;
		bOutSteeringActive = false;

		if (!ExistingEntry || !ExistingEntry->Actor.IsValid())
		{
			return DesiredTransform;
		}

		const FVector CurrentPositionCm = ExistingEntry->Actor->GetActorLocation();
		const double PreviousDistanceCm = FVector::Distance(CurrentPositionCm, DesiredPositionCm);
		const double DeltaSeconds = FMath::Max(0.0, SimulationTimeSeconds - ExistingEntry->LastSteeringSimulationTimeSeconds);
		if (DeltaSeconds <= UE_SMALL_NUMBER || PreviousDistanceCm <= 1.0)
		{
			OutDistanceToDesiredCm = 0.0;
			return DesiredTransform;
		}

		const double MaxStepCm = ResolveEncounterSteeringSpeedCmPerSec(LocalBehaviorStateId) * DeltaSeconds;
		const FVector Direction = (DesiredPositionCm - CurrentPositionCm).GetSafeNormal(UE_SMALL_NUMBER, FVector::ZeroVector);
		const FVector NewPositionCm = PreviousDistanceCm <= MaxStepCm
			? DesiredPositionCm
			: CurrentPositionCm + Direction * MaxStepCm;
		OutDistanceToDesiredCm = FVector::Distance(NewPositionCm, DesiredPositionCm);
		OutDistanceTrendCm = PreviousDistanceCm - OutDistanceToDesiredCm;
		bOutSteeringActive = OutDistanceToDesiredCm > 1.0;

		return FTransform(DesiredTransform.GetRotation(), NewPositionCm, DesiredTransform.GetScale3D());
	}

	FName ResolvePatrolBehaviorVariant(const FPatrolReservationRecord& Reservation)
	{
		// Enhanced Unreal posture layer: this should steer support placement/comms,
		// not replace Godot-backed combat states such as Engage or Disengage.
		if (Reservation.DynamicPatrolScore >= 0.70 || Reservation.SelectionDebugReason.Contains(TEXT("CriminalPressure=")))
		{
			return TEXT("patrol_emergency_intercept");
		}
		if (Reservation.DynamicPatrolScore >= 0.40)
		{
			return TEXT("patrol_trade_lane_screen");
		}
		return TEXT("patrol_route_presence");
	}

	FName ResolvePatrolCommsVariant(const FPatrolReservationRecord& Reservation)
	{
		if (Reservation.DynamicPatrolScore >= 0.70)
		{
			return TEXT("comms_patrol_priority_response");
		}
		if (Reservation.DynamicPatrolScore >= 0.40)
		{
			return TEXT("comms_patrol_sector_checkin");
		}
		return TEXT("comms_patrol_passive_monitor");
	}

	FVector2D ResolvePatrolLocalOffsetCm(FName BehaviorVariantId)
	{
		if (BehaviorVariantId == FName(TEXT("patrol_emergency_intercept")))
		{
			return FVector2D(-1200.0, 500.0);
		}
		if (BehaviorVariantId == FName(TEXT("patrol_route_presence")))
		{
			return FVector2D(-5200.0, 3200.0);
		}
		return FVector2D(-2800.0, 1400.0);
	}

	FName ResolvePatrolLocalBehaviorState(FName BehaviorVariantId)
	{
		if (BehaviorVariantId == FName(TEXT("patrol_emergency_intercept")))
		{
			return TEXT("local_priority_intercept");
		}
		if (BehaviorVariantId == FName(TEXT("patrol_route_presence")))
		{
			return TEXT("local_presence_patrol");
		}
		return TEXT("local_trade_lane_screen");
	}

	FString ResolvePatrolCommsLine(FName CommsVariantId)
	{
		if (CommsVariantId == FName(TEXT("comms_patrol_priority_response")))
		{
			return TEXT("Authority patrol responding priority to your distress.");
		}
		if (CommsVariantId == FName(TEXT("comms_patrol_passive_monitor")))
		{
			return TEXT("Authority patrol holding overwatch on this route.");
		}
		return TEXT("Authority patrol screening the trade lane.");
	}

	bool ShouldEncounterActorFire(const FRealizedEncounterActorEntry& Entry)
	{
		return Entry.Role == EShipGoalKind::Pirate ||
			Entry.IntentType == FName(TEXT("attack")) ||
			Entry.BehaviorVariantId.ToString().Contains(TEXT("pirate"), ESearchCase::IgnoreCase);
	}

	double ResolveEncounterShotIntervalSeconds(const FRealizedEncounterActorEntry& Entry)
	{
		if (Entry.LocalBehaviorStateId == FName(TEXT("local_intercept_closing")) ||
			Entry.LocalBehaviorStateId == FName(TEXT("local_priority_intercept")))
		{
			return 1.8;
		}
		if (Entry.LocalBehaviorStateId == FName(TEXT("local_shadowing")))
		{
			return 3.5;
		}
		return 2.6;
	}
}

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
	if (!SpawnSystemPresentation(SystemDefinition.SystemId))
	{
		AddBuildValidationIssue(TEXT("active_system_presentation_spawn_failed"), SystemDefinition.SystemId, LastBuildError);
		TearDownActiveSystem();
		return false;
	}

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

		if (!RegisterBodyEntity(Body, FTransform(ResolvedTransform.Rotation, ResolvedTransform.PositionCm)) ||
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
	ClearRealizedTrafficActors();
	ClearRealizedEncounterActors();

	for (TPair<FName, TObjectPtr<ACombatShotPresentationActor>>& Pair : CombatShotPresentationActorsById)
	{
		if (ACombatShotPresentationActor* ShotActor = Pair.Value.Get())
		{
			SpawnedSystemActors.Remove(ShotActor);
			ShotActor->Destroy();
		}
	}
	CombatShotPresentationActorsById.Reset();

	for (TObjectPtr<AActor> Actor : SpawnedSystemActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}

	SpawnedSystemActors.Reset();
	SpacePresentationActor = nullptr;
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
		FTransform ActorSpawnTransform = SpawnZone.Transform;
		ActorSpawnTransform.SetLocation(FVector::ZeroVector);
		Pawn = World->SpawnActor<APawn>(SpawnClass, ActorSpawnTransform, SpawnParameters);
		if (ASpaceFlightPawn* SpaceFlightPawn = Cast<ASpaceFlightPawn>(Pawn))
		{
			SpaceFlightPawn->SetFlightTestLogicalTransformAndActorLocation(SpawnZone.Transform, FVector::ZeroVector, FVector::ZeroVector);
		}
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
		if (ASpaceFlightPawn* SpaceFlightPawn = Cast<ASpaceFlightPawn>(Pawn))
		{
			SpaceFlightPawn->SetFlightTestLogicalTransformAndActorLocation(SpawnZone.Transform, FVector::ZeroVector, FVector::ZeroVector);
		}
		else
		{
			Pawn->SetActorTransform(FTransform(SpawnZone.Transform.GetRotation(), FVector::ZeroVector), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	ActivePlayerPawn = Pawn;
	if (Pawn)
	{
		RefreshRegisteredEntityTransforms(0.0);
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

	const ASpaceFlightPawn* SpaceFlightPawn = Cast<ASpaceFlightPawn>(ActivePlayerPawn.Get());
	const FVector BubbleOriginSystemPositionCm = SpaceFlightPawn ? SpaceFlightPawn->GetLogicalSystemPositionCm() : FVector::ZeroVector;
	const FVector BubbleOriginActorPositionCm = ActivePlayerPawn.IsValid() ? ActivePlayerPawn->GetActorLocation() : FVector::ZeroVector;
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
			FVector ProjectedPositionCm = ResolvedTransform.PositionCm;
			double ProjectionVisualScale = 1.0;
			bool bInsideLocalBubble = true;
			if (SpaceFlightPawn)
			{
				FVector ActorOffsetCm = FVector::ZeroVector;
				bInsideLocalBubble = UOrbitRouteFrameQueryService::ProjectSystemPositionToLocalBubble(
					ActiveSystemDefinition.Scale,
					BubbleOriginSystemPositionCm,
					ResolvedTransform.PositionCm,
					ActorOffsetCm);
				if (!bInsideLocalBubble)
				{
					const double TrueDistanceCm = ActorOffsetCm.Size();
					const double FarShellRadiusCm = FMath::Min(
						ActiveSystemDefinition.Scale.LocalBubbleRadiusCm,
						DistantSystemRenderShellRadiusCm);
					if (TrueDistanceCm > UE_DOUBLE_SMALL_NUMBER)
					{
						ActorOffsetCm = ActorOffsetCm.GetSafeNormal() * FarShellRadiusCm;
						ProjectionVisualScale = FarShellRadiusCm / TrueDistanceCm;
					}
				}
				ProjectedPositionCm = BubbleOriginActorPositionCm + ActorOffsetCm;
			}

			Actor->SetActorTransform(FTransform(ResolvedTransform.Rotation, ProjectedPositionCm, FVector(ProjectionVisualScale)), false, nullptr, ETeleportType::TeleportPhysics);
			Actor->SetActorHiddenInGame(false);
			Actor->SetActorEnableCollision(bInsideLocalBubble);
			if (ASectorStarAnchorActor* StarActor = Cast<ASectorStarAnchorActor>(Actor))
			{
				const double ObserverDistanceCm = FVector::Distance(BubbleOriginSystemPositionCm, ResolvedTransform.PositionCm);
				const double RenderedDistanceCm = FVector::Distance(BubbleOriginActorPositionCm, ProjectedPositionCm);
				StarActor->UpdateRuntimeVisualLOD(ObserverDistanceCm, RenderedDistanceCm);
			}
		}
	}

	return true;
}

bool UStarSystemSubsystem::RealizeTrafficActors(const FActiveTrafficSimulationState& TrafficState, double SimulationTimeSeconds)
{
	if (!bBuildComplete)
	{
		LastBuildError = TEXT("Cannot realize traffic actors before a system is built.");
		return false;
	}
	if (TrafficState.SystemId != ActiveSystemDefinition.SystemId)
	{
		LastBuildError = FString::Printf(TEXT("Traffic state system '%s' does not match active system '%s'."),
			*TrafficState.SystemId.ToString(),
			*ActiveSystemDefinition.SystemId.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LastBuildError = TEXT("Cannot realize traffic actors without a world.");
		return false;
	}

	TSet<FName> LiveShipIds;
	const FSimulationClockSnapshot ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(ActiveSystemDefinition.SystemId, SimulationTimeSeconds);
	for (const FShipTrafficInstance& Ship : TrafficState.Ships)
	{
		if (Ship.ShipInstanceId.IsNone() || Ship.CurrentGoal.RouteSegmentId.IsNone() || Ship.TrafficTier != ELogicalTrafficTier::Tier1Realized)
		{
			continue;
		}

		FRouteSample RouteSample;
		FTransform ActorTransform;
		if (!ResolveTrafficShipTransform(ActiveSystemDefinition, TrafficState, Ship, ClockSnapshot, SimulationTimeSeconds, RouteSample, ActorTransform))
		{
			continue;
		}

		LiveShipIds.Add(Ship.ShipInstanceId);
		FRealizedTrafficActorEntry* ExistingEntry = RealizedTrafficActorsByShipId.Find(Ship.ShipInstanceId);
		ALogicalTrafficActor* Actor = ExistingEntry ? ExistingEntry->Actor.Get() : nullptr;
		if (!Actor)
		{
			TSubclassOf<ALogicalTrafficActor> SpawnClass = ResolveTrafficActorClass(World->GetGameInstance(), Ship);
			if (!SpawnClass)
			{
				continue;
			}
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FName BaseActorName = FName(*FString::Printf(TEXT("traffic_%s"), *Ship.ShipInstanceId.ToString()));
			SpawnParameters.Name = MakeUniqueObjectName(World->PersistentLevel, *SpawnClass, BaseActorName);
			Actor = World->SpawnActor<ALogicalTrafficActor>(SpawnClass, ActorTransform, SpawnParameters);
			if (!Actor)
			{
				LastBuildError = FString::Printf(TEXT("Failed to spawn logical traffic actor '%s'."), *Ship.ShipInstanceId.ToString());
				return false;
			}
			SpawnedSystemActors.Add(Actor);
		}

		Actor->ConfigureTrafficActor(Ship.ShipInstanceId, Ship.GroupId, Ship.CurrentGoal.GoalKind);
		Actor->SetActorTransform(ActorTransform, false, nullptr, ETeleportType::TeleportPhysics);

		FRealizedTrafficActorEntry Entry;
		Entry.ShipInstanceId = Ship.ShipInstanceId;
		Entry.GroupId = Ship.GroupId;
		Entry.GoalKind = Ship.CurrentGoal.GoalKind;
		Entry.RouteSegmentId = RouteSample.RouteSegmentId;
		Entry.RouteProgress01 = RouteSample.RouteProgress01;
		Entry.Actor = Actor;
		RealizedTrafficActorsByShipId.Add(Ship.ShipInstanceId, Entry);
	}

	TArray<FName> StaleShipIds;
	for (const TPair<FName, FRealizedTrafficActorEntry>& Pair : RealizedTrafficActorsByShipId)
	{
		if (!LiveShipIds.Contains(Pair.Key))
		{
			StaleShipIds.Add(Pair.Key);
		}
	}
	for (const FName StaleShipId : StaleShipIds)
	{
		if (FRealizedTrafficActorEntry* Entry = RealizedTrafficActorsByShipId.Find(StaleShipId))
		{
			if (ALogicalTrafficActor* Actor = Entry->Actor.Get())
			{
				SpawnedSystemActors.Remove(Actor);
				Actor->Destroy();
			}
		}
		RealizedTrafficActorsByShipId.Remove(StaleShipId);
	}

	LastBuildError.Reset();
	return true;
}

bool UStarSystemSubsystem::RefreshRealizedTrafficActors(const FActiveTrafficSimulationState& TrafficState, double SimulationTimeSeconds)
{
	return RealizeTrafficActors(TrafficState, SimulationTimeSeconds);
}

void UStarSystemSubsystem::ClearRealizedTrafficActors()
{
	for (TPair<FName, FRealizedTrafficActorEntry>& Pair : RealizedTrafficActorsByShipId)
	{
		if (ALogicalTrafficActor* Actor = Pair.Value.Actor.Get())
		{
			SpawnedSystemActors.Remove(Actor);
			Actor->Destroy();
		}
	}
	RealizedTrafficActorsByShipId.Reset();
}

bool UStarSystemSubsystem::RealizeSystemicEncounterActors(const FSystemicGameplayState& SystemicState, double SimulationTimeSeconds)
{
	UWorld* World = GetWorld();
	if (!World || ActiveSystemDefinition.SystemId.IsNone())
	{
		LastBuildError = TEXT("Cannot realize systemic encounter actors without an active world and system.");
		return false;
	}

	TSet<FName> LiveActorIds;
	const FSimulationClockSnapshot RouteClock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(ActiveSystemDefinition.SystemId, SimulationTimeSeconds);
	TSubclassOf<ALogicalTrafficActor> EncounterActorClass = ResolveAuthoredActorClass<ALogicalTrafficActor>(ActiveSystemDefinition.EncounterActorClass);
	if (!EncounterActorClass)
	{
		EncounterActorClass = ALogicalTrafficActor::StaticClass();
	}

	auto UpsertEncounterActor = [this, World, &LiveActorIds, SimulationTimeSeconds, EncounterActorClass](
		const FName ActorId,
		const FName EncounterId,
		const FName HazardId,
		const FName PatrolReservationId,
		EShipGoalKind Role,
		const FRouteSample& RouteSample,
		const FRouteSample& TargetRouteSample,
		const FRealizedAISteeringIntent* SteeringIntent,
		FName BehaviorVariantId,
		FName CommsVariantId,
		FName LocalBehaviorStateId,
		const FString& CommsLine,
		const FTransform& ActorTransform) -> bool
	{
		if (ActorId.IsNone())
		{
			return true;
		}

		LiveActorIds.Add(ActorId);
		FRealizedEncounterActorEntry* ExistingEntry = RealizedEncounterActorsById.Find(ActorId);
		ALogicalTrafficActor* Actor = ExistingEntry ? ExistingEntry->Actor.Get() : nullptr;
		double DistanceToDesiredCm = 0.0;
		double DistanceTrendCm = 0.0;
		bool bSteeringActive = false;
		FTransform AppliedTransform = ActorTransform;
		if (!Actor)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FName BaseActorName = FName(*FString::Printf(TEXT("encounter_%s"), *ActorId.ToString()));
			SpawnParameters.Name = MakeUniqueObjectName(World->PersistentLevel, *EncounterActorClass, BaseActorName);
			Actor = World->SpawnActor<ALogicalTrafficActor>(EncounterActorClass, ActorTransform, SpawnParameters);
			if (!Actor)
			{
				LastBuildError = FString::Printf(TEXT("Failed to spawn encounter actor '%s'."), *ActorId.ToString());
				return false;
			}
			SpawnedSystemActors.Add(Actor);
		}
		else
		{
			AppliedTransform = BuildSteeredEncounterTransform(
				ActorTransform,
				ExistingEntry,
				LocalBehaviorStateId,
				SimulationTimeSeconds,
				DistanceToDesiredCm,
				DistanceTrendCm,
				bSteeringActive);
		}

		Actor->ConfigureTrafficActor(ActorId, HazardId.IsNone() ? PatrolReservationId : HazardId, Role);
		Actor->ConfigureEncounterBehavior(
			EncounterId,
			SteeringIntent ? SteeringIntent->IntentId : NAME_None,
			SteeringIntent ? SteeringIntent->IntentType : NAME_None,
			SteeringIntent ? SteeringIntent->ThreatId : NAME_None,
			SteeringIntent ? SteeringIntent->TargetShipId : NAME_None,
			BehaviorVariantId,
			CommsVariantId,
			LocalBehaviorStateId,
			CommsLine);
		Actor->SetActorTransform(AppliedTransform, false, nullptr, ETeleportType::TeleportPhysics);

		FRealizedEncounterActorEntry Entry;
		Entry.ActorId = ActorId;
		Entry.EncounterId = EncounterId;
		Entry.HazardId = HazardId;
		Entry.PatrolReservationId = PatrolReservationId;
		Entry.Role = Role;
		Entry.IntentId = SteeringIntent ? SteeringIntent->IntentId : NAME_None;
		Entry.IntentType = SteeringIntent ? SteeringIntent->IntentType : NAME_None;
		Entry.ThreatId = SteeringIntent ? SteeringIntent->ThreatId : NAME_None;
		Entry.TargetShipId = SteeringIntent ? SteeringIntent->TargetShipId : NAME_None;
		Entry.BehaviorVariantId = BehaviorVariantId;
		Entry.CommsVariantId = CommsVariantId;
		Entry.LocalBehaviorStateId = LocalBehaviorStateId;
		Entry.CommsLine = CommsLine;
		Entry.RouteSegmentId = RouteSample.RouteSegmentId;
		Entry.RouteProgress01 = RouteSample.RouteProgress01;
		Entry.RouteSample = RouteSample;
		Entry.TargetRouteSample = TargetRouteSample;
		Entry.DesiredPositionCm = ActorTransform.GetLocation();
		Entry.DistanceToDesiredCm = DistanceToDesiredCm;
		Entry.DistanceTrendCm = DistanceTrendCm;
		Entry.bSteeringActive = bSteeringActive;
		Entry.LastSteeringSimulationTimeSeconds = SimulationTimeSeconds;
		Entry.LastShotSimulationTimeSeconds = ExistingEntry ? ExistingEntry->LastShotSimulationTimeSeconds : -1.0;
		Entry.FiredShotCount = ExistingEntry ? ExistingEntry->FiredShotCount : 0;
		Entry.Actor = Actor;
		RealizedEncounterActorsById.Add(ActorId, Entry);
		return true;
	};

	for (const FInterdictionHazardRecord& Hazard : SystemicState.InterdictionHazards)
	{
		if (Hazard.HazardId.IsNone() || Hazard.RouteSegmentId.IsNone() || !IsEncounterRecordLive(Hazard.State, Hazard.ExpiresAtTimeSeconds, SimulationTimeSeconds))
		{
			continue;
		}

		FRouteSample LiveSample;
		const double RouteProgress = FMath::Clamp(Hazard.RouteSample.RouteProgress01, 0.0, 1.0);
		if (!UOrbitRouteFrameQueryService::EvaluateRoute(ActiveSystemDefinition, Hazard.RouteSegmentId, RouteProgress, RouteClock, SimulationTimeSeconds, LiveSample))
		{
			continue;
		}

		FName EncounterId = NAME_None;
		if (const FLogicalEncounterRecord* Encounter = SystemicState.LogicalEncounters.FindByPredicate([&Hazard](const FLogicalEncounterRecord& Candidate)
		{
			return Candidate.InterdictionHazardId == Hazard.HazardId;
		}))
		{
			if (!IsEncounterRecordLive(Encounter->State, 0.0, SimulationTimeSeconds))
			{
				continue;
			}
			EncounterId = Encounter->EncounterId;
		}

		FName PirateShipId = NAME_None;
		if (const FThreatRecord* Threat = SystemicState.ThreatRecords.FindByPredicate([&Hazard](const FThreatRecord& Candidate)
		{
			return Candidate.DefenderId == Hazard.TargetShipId && Candidate.SourceEventId == Hazard.SourceEventId;
		}))
		{
			PirateShipId = Threat->AttackerId;
		}

		const FRealizedAISteeringIntent* SteeringIntent = FindSteeringIntentForShip(SystemicState, PirateShipId, TEXT("attack"));
		FRouteSample TargetSample = LiveSample;
		if (SteeringIntent && !SteeringIntent->TargetFrame.RouteSegmentId.IsNone())
		{
			UOrbitRouteFrameQueryService::EvaluateRoute(
				ActiveSystemDefinition,
				SteeringIntent->TargetFrame.RouteSegmentId,
				FMath::Clamp(SteeringIntent->TargetFrame.RouteProgress01, 0.0, 1.0),
				RouteClock,
				SimulationTimeSeconds,
				TargetSample);
		}

		const FName ActorId(*FString::Printf(TEXT("%s_pirate_wing"), *Hazard.HazardId.ToString()));
		const FName BehaviorVariantId = ResolvePirateBehaviorVariant(Hazard);
		const FName CommsVariantId = ResolvePirateCommsVariant(Hazard);
		const FVector2D LocalOffsetCm = ResolvePirateLocalOffsetCm(BehaviorVariantId);
		if (!UpsertEncounterActor(
			ActorId,
			EncounterId,
			Hazard.HazardId,
			NAME_None,
			EShipGoalKind::Pirate,
			LiveSample,
			TargetSample,
			SteeringIntent,
			BehaviorVariantId,
			CommsVariantId,
			ResolvePirateLocalBehaviorState(BehaviorVariantId),
			ResolvePirateCommsLine(CommsVariantId),
			BuildEncounterActorTransform(TargetSample, LocalOffsetCm.X, LocalOffsetCm.Y)))
		{
			return false;
		}
	}

	for (const FPatrolReservationRecord& Reservation : SystemicState.PatrolReservations)
	{
		if (Reservation.ReservationId.IsNone() || Reservation.RouteSegmentId.IsNone() || !IsEncounterRecordLive(Reservation.State, Reservation.ExpiresAtTimeSeconds, SimulationTimeSeconds))
		{
			continue;
		}

		const FRealizedAISteeringIntent* SteeringIntent = FindSteeringIntentForShip(SystemicState, Reservation.PatrolAssetId, TEXT("formation"));
		const double RouteProgress = SteeringIntent && !SteeringIntent->TargetFrame.RouteSegmentId.IsNone()
			? FMath::Clamp(SteeringIntent->TargetFrame.RouteProgress01, 0.0, 1.0)
			: (Reservation.PatrolAssetId.ToString().Contains(TEXT("02")) ? 0.72 : 0.28);
		FRouteSample LiveSample;
		if (!UOrbitRouteFrameQueryService::EvaluateRoute(ActiveSystemDefinition, Reservation.RouteSegmentId, RouteProgress, RouteClock, SimulationTimeSeconds, LiveSample))
		{
			continue;
		}

		FName EncounterId = NAME_None;
		for (const FDistressEventRecord& Distress : SystemicState.DistressEvents)
		{
			if (!Distress.RespondingPatrolReservationIds.Contains(Reservation.ReservationId))
			{
				continue;
			}
			if (const FLogicalEncounterRecord* Encounter = SystemicState.LogicalEncounters.FindByPredicate([&Distress](const FLogicalEncounterRecord& Candidate)
			{
				return Candidate.DistressEventId == Distress.DistressEventId;
			}))
			{
				if (!IsEncounterRecordLive(Encounter->State, 0.0, SimulationTimeSeconds))
				{
					EncounterId = NAME_None;
					break;
				}
				EncounterId = Encounter->EncounterId;
			}
			break;
		}

		if (EncounterId.IsNone() && SystemicState.DistressEvents.ContainsByPredicate([&Reservation](const FDistressEventRecord& Distress)
		{
			return Distress.RespondingPatrolReservationIds.Contains(Reservation.ReservationId);
		}))
		{
			continue;
		}

		const FName ActorId(*FString::Printf(TEXT("%s_patrol_response"), *Reservation.ReservationId.ToString()));
		const FName BehaviorVariantId = ResolvePatrolBehaviorVariant(Reservation);
		const FName CommsVariantId = ResolvePatrolCommsVariant(Reservation);
		const FVector2D LocalOffsetCm = ResolvePatrolLocalOffsetCm(BehaviorVariantId);
		if (!UpsertEncounterActor(
			ActorId,
			EncounterId,
			NAME_None,
			Reservation.ReservationId,
			EShipGoalKind::Patrol,
			LiveSample,
			LiveSample,
			SteeringIntent,
			BehaviorVariantId,
			CommsVariantId,
			ResolvePatrolLocalBehaviorState(BehaviorVariantId),
			ResolvePatrolCommsLine(CommsVariantId),
			BuildEncounterActorTransform(LiveSample, LocalOffsetCm.X, LocalOffsetCm.Y)))
		{
			return false;
		}
	}

	TArray<FName> StaleActorIds;
	for (const TPair<FName, FRealizedEncounterActorEntry>& Pair : RealizedEncounterActorsById)
	{
		if (!LiveActorIds.Contains(Pair.Key))
		{
			StaleActorIds.Add(Pair.Key);
		}
	}
	for (const FName StaleActorId : StaleActorIds)
	{
		if (FRealizedEncounterActorEntry* Entry = RealizedEncounterActorsById.Find(StaleActorId))
		{
			if (ALogicalTrafficActor* Actor = Entry->Actor.Get())
			{
				SpawnedSystemActors.Remove(Actor);
				Actor->Destroy();
			}
		}
		RealizedEncounterActorsById.Remove(StaleActorId);
	}

	LastBuildError.Reset();
	return true;
}

void UStarSystemSubsystem::ClearRealizedEncounterActors()
{
	for (TPair<FName, FRealizedEncounterActorEntry>& Pair : RealizedEncounterActorsById)
	{
		if (ALogicalTrafficActor* Actor = Pair.Value.Actor.Get())
		{
			SpawnedSystemActors.Remove(Actor);
			Actor->Destroy();
		}
	}
	RealizedEncounterActorsById.Reset();
}

void UStarSystemSubsystem::GetRealizedEncounterActors(TArray<FRealizedEncounterActorEntry>& OutActors) const
{
	RealizedEncounterActorsById.GenerateValueArray(OutActors);
	OutActors.Sort([](const FRealizedEncounterActorEntry& Left, const FRealizedEncounterActorEntry& Right)
	{
		return Left.ActorId.LexicalLess(Right.ActorId);
	});
}

bool UStarSystemSubsystem::ApplyEncounterResponseManeuver(FName EncounterId, FName ResponseStateId, double SimulationTimeSeconds, FVector& OutStartPositionCm, FVector& OutEndPositionCm, FName& OutManeuverStateId)
{
	OutStartPositionCm = FVector::ZeroVector;
	OutEndPositionCm = FVector::ZeroVector;
	OutManeuverStateId = NAME_None;

	if (EncounterId.IsNone() || ResponseStateId.IsNone())
	{
		LastBuildError = TEXT("Encounter response movement requires encounter and response state IDs.");
		return false;
	}

	for (TPair<FName, FRealizedEncounterActorEntry>& Pair : RealizedEncounterActorsById)
	{
		FRealizedEncounterActorEntry& Entry = Pair.Value;
		ALogicalTrafficActor* Actor = Entry.Actor.Get();
		if (Entry.EncounterId != EncounterId || !Actor)
		{
			continue;
		}

		OutStartPositionCm = Actor->GetActorLocation();
		FVector AwayFromTarget = OutStartPositionCm - Entry.TargetRouteSample.ResolvedTransform.PositionCm;
		if (AwayFromTarget.IsNearlyZero())
		{
			AwayFromTarget = OutStartPositionCm - Entry.RouteSample.ResolvedTransform.PositionCm;
		}
		const FVector AwayDirection = AwayFromTarget.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		FVector StrafeDirection = FVector::CrossProduct(AwayDirection, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		if (ResponseStateId == FName(TEXT("hostile_return_fire")))
		{
			OutManeuverStateId = TEXT("maneuver_attack_strafe");
		}
		else if (ResponseStateId == FName(TEXT("hostile_break_scan")))
		{
			OutManeuverStateId = TEXT("maneuver_break_scan");
			StrafeDirection *= -1.0;
		}
		else
		{
			OutManeuverStateId = TEXT("maneuver_evasive_reposition");
			StrafeDirection = (StrafeDirection + AwayDirection).GetSafeNormal(UE_SMALL_NUMBER, StrafeDirection);
		}

		const double ManeuverDistanceCm = ResponseStateId == FName(TEXT("hostile_return_fire")) ? 4200.0 : 3200.0;
		OutEndPositionCm = OutStartPositionCm + StrafeDirection * ManeuverDistanceCm;
		Actor->SetActorLocation(OutEndPositionCm, false, nullptr, ETeleportType::TeleportPhysics);

		Entry.DistanceToDesiredCm = FVector::Distance(OutEndPositionCm, Entry.DesiredPositionCm);
		Entry.DistanceTrendCm = FVector::Distance(OutStartPositionCm, Entry.DesiredPositionCm) - Entry.DistanceToDesiredCm;
		Entry.bSteeringActive = Entry.DistanceToDesiredCm > 1.0;
		Entry.LastSteeringSimulationTimeSeconds = SimulationTimeSeconds;
		LastBuildError.Reset();
		return true;
	}

	LastBuildError = FString::Printf(TEXT("Encounter response movement could not find realized actor for '%s'."), *EncounterId.ToString());
	return false;
}

bool UStarSystemSubsystem::UpdateFoundationalSpaceWorld(const FActiveTrafficSimulationState& TrafficState, const FSystemicGameplayState& SystemicState, double SimulationTimeSeconds)
{
	if (!RefreshRegisteredEntityTransforms(SimulationTimeSeconds))
	{
		return false;
	}

	if (!TrafficState.SystemId.IsNone() && !RefreshRealizedTrafficActors(TrafficState, SimulationTimeSeconds))
	{
		return false;
	}

	if (!RealizeSystemicEncounterActors(SystemicState, SimulationTimeSeconds))
	{
		return false;
	}

	if (!UpdateRealizedEncounterCombat(SimulationTimeSeconds))
	{
		return false;
	}

	RefreshCombatShotPresentations(SimulationTimeSeconds);
	return true;
}

bool UStarSystemSubsystem::UpdateRealizedEncounterCombat(double SimulationTimeSeconds)
{
	for (TPair<FName, FRealizedEncounterActorEntry>& Pair : RealizedEncounterActorsById)
	{
		FRealizedEncounterActorEntry& Entry = Pair.Value;
		ALogicalTrafficActor* Actor = Entry.Actor.Get();
		if (!Actor || !ShouldEncounterActorFire(Entry))
		{
			continue;
		}

		const double ShotIntervalSeconds = ResolveEncounterShotIntervalSeconds(Entry);
		if (Entry.LastShotSimulationTimeSeconds >= 0.0 &&
			SimulationTimeSeconds - Entry.LastShotSimulationTimeSeconds < ShotIntervalSeconds)
		{
			continue;
		}

		FVector StartPositionCm = FVector::ZeroVector;
		FVector EndPositionCm = FVector::ZeroVector;
		const FVector TargetPositionCm = Entry.TargetRouteSample.ResolvedTransform.PositionCm.IsNearlyZero()
			? Entry.RouteSample.ResolvedTransform.PositionCm
			: Entry.TargetRouteSample.ResolvedTransform.PositionCm;
		if (!Actor->BuildShotPresentationToLocation(TargetPositionCm, 250000.0, StartPositionCm, EndPositionCm))
		{
			continue;
		}

		++Entry.FiredShotCount;
		const FName ShotPresentationId(*FString::Printf(TEXT("%s_shot_%03d"), *Entry.ActorId.ToString(), Entry.FiredShotCount));
		FName ShotActorId;
		if (!SpawnCombatShotPresentation(ShotPresentationId, TEXT("hostile_laser_burst"), StartPositionCm, EndPositionCm, SimulationTimeSeconds, 0.35, ShotActorId))
		{
			return false;
		}
		Entry.LastShotSimulationTimeSeconds = SimulationTimeSeconds;
	}

	LastBuildError.Reset();
	return true;
}

bool UStarSystemSubsystem::SpawnCombatShotPresentation(FName ShotPresentationId, FName PresentationType, const FVector& StartPositionCm, const FVector& EndPositionCm, double StartedAtTimeSeconds, double DurationSeconds, FName& OutActorId)
{
	OutActorId = NAME_None;
	if (ShotPresentationId.IsNone())
	{
		LastBuildError = TEXT("Combat shot presentation requires a shot presentation ID.");
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LastBuildError = TEXT("Combat shot presentation requires an active world.");
		return false;
	}

	ACombatShotPresentationActor* ShotActor = CombatShotPresentationActorsById.FindRef(ShotPresentationId);
	if (!IsValid(ShotActor))
	{
		CombatShotPresentationActorsById.Remove(ShotPresentationId);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FName BaseActorName = FName(*FString::Printf(TEXT("combat_shot_%s"), *ShotPresentationId.ToString()));
		TSubclassOf<ACombatShotPresentationActor> SpawnClass = ResolveAuthoredActorClass<ACombatShotPresentationActor>(ActiveSystemDefinition.CombatShotPresentationActorClass);
		if (!SpawnClass)
		{
			SpawnClass = ACombatShotPresentationActor::StaticClass();
		}

		SpawnParameters.Name = MakeUniqueObjectName(World->PersistentLevel, *SpawnClass, BaseActorName);
		ShotActor = World->SpawnActor<ACombatShotPresentationActor>(SpawnClass, FTransform::Identity, SpawnParameters);
		if (!ShotActor)
		{
			LastBuildError = FString::Printf(TEXT("Failed to spawn combat shot presentation '%s'."), *ShotPresentationId.ToString());
			return false;
		}
		SpawnedSystemActors.Add(ShotActor);
		CombatShotPresentationActorsById.Add(ShotPresentationId, ShotActor);
	}

	ShotActor->ConfigureShotPresentation(ShotPresentationId, PresentationType, StartPositionCm, EndPositionCm, StartedAtTimeSeconds, DurationSeconds);
	OutActorId = ShotActor->GetFName();
	LastBuildError.Reset();
	return true;
}

void UStarSystemSubsystem::RefreshCombatShotPresentations(double SimulationTimeSeconds)
{
	TArray<FName> ExpiredShotIds;
	for (const TPair<FName, TObjectPtr<ACombatShotPresentationActor>>& Pair : CombatShotPresentationActorsById)
	{
		const ACombatShotPresentationActor* ShotActor = Pair.Value;
		if (!IsValid(ShotActor) || ShotActor->GetExpiresAtTimeSeconds() <= SimulationTimeSeconds)
		{
			ExpiredShotIds.Add(Pair.Key);
		}
	}

	for (const FName ExpiredShotId : ExpiredShotIds)
	{
		if (ACombatShotPresentationActor* ShotActor = CombatShotPresentationActorsById.FindRef(ExpiredShotId))
		{
			SpawnedSystemActors.Remove(ShotActor);
			ShotActor->Destroy();
		}
		CombatShotPresentationActorsById.Remove(ExpiredShotId);
	}
}

bool UStarSystemSubsystem::IsCombatShotPresentationActive(FName ShotPresentationId) const
{
	const ACombatShotPresentationActor* ShotActor = CombatShotPresentationActorsById.FindRef(ShotPresentationId);
	return !ShotPresentationId.IsNone() && IsValid(ShotActor);
}

void UStarSystemSubsystem::GetRealizedTrafficActors(TArray<FRealizedTrafficActorEntry>& OutActors) const
{
	RealizedTrafficActorsByShipId.GenerateValueArray(OutActors);
	OutActors.Sort([](const FRealizedTrafficActorEntry& Left, const FRealizedTrafficActorEntry& Right)
	{
		return Left.ShipInstanceId.LexicalLess(Right.ShipInstanceId);
	});
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
	TSubclassOf<AM0SystemMarkerActor> MarkerClass = ResolveAuthoredActorClass<AM0SystemMarkerActor>(ActiveSystemDefinition.EntityMarkerActorClass);
	if (!MarkerClass)
	{
		MarkerClass = AM0SystemMarkerActor::StaticClass();
	}

	AM0SystemMarkerActor* Marker = World->SpawnActor<AM0SystemMarkerActor>(MarkerClass, Transform, SpawnParameters);
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

bool UStarSystemSubsystem::SpawnSystemPresentation(FName SystemId)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		LastBuildError = TEXT("Cannot spawn system presentation without a world.");
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	TSubclassOf<ASystemSpacePresentationActor> PresentationClass = ResolveAuthoredActorClass<ASystemSpacePresentationActor>(ActiveSystemDefinition.PresentationActorClass);
	if (!PresentationClass)
	{
		PresentationClass = ASystemSpacePresentationActor::StaticClass();
	}

	ASystemSpacePresentationActor* Presentation = World->SpawnActor<ASystemSpacePresentationActor>(
		PresentationClass,
		FTransform::Identity,
		SpawnParameters);
	if (!Presentation)
	{
		LastBuildError = TEXT("Failed to spawn system presentation actor.");
		return false;
	}

	Presentation->ConfigureForSystem(SystemId);
	SpacePresentationActor = Presentation;
	SpawnedSystemActors.Add(Presentation);
	return true;
}

bool UStarSystemSubsystem::RegisterBodyEntity(const FBodyDefinition& Body, const FTransform& Transform)
{
	if (Body.BodyType != FName(TEXT("star")))
	{
		return RegisterEntity(Body.BodyId, TEXT("body"), Transform, Body.VisualRadiusCm);
	}

	if (Body.BodyId.IsNone())
	{
		LastBuildError = TEXT("Cannot register star body with an empty ID.");
		return false;
	}

	if (RegisteredEntities.Contains(Body.BodyId))
	{
		LastBuildError = FString::Printf(TEXT("Duplicate active system entity ID '%s'."), *Body.BodyId.ToString());
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		LastBuildError = TEXT("Cannot register active system star without a world.");
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASectorStarAnchorActor* StarActor = World->SpawnActor<ASectorStarAnchorActor>(
		ASectorStarAnchorActor::StaticClass(),
		Transform,
		SpawnParameters);
	if (!StarActor)
	{
		LastBuildError = FString::Printf(TEXT("Failed to spawn reusable star actor for '%s'."), *Body.BodyId.ToString());
		return false;
	}

	StarActor->ConfigureRuntimeStar(Body.BodyId, Body.VisualRadiusCm, ResolveStellarClass(Body.StellarClass));
	SpawnedSystemActors.Add(StarActor);

	FActiveSystemEntityEntry Entry;
	Entry.EntityId = Body.BodyId;
	Entry.EntityType = TEXT("star");
	Entry.BuildGeneration = ActiveBuildGeneration;
	Entry.Actor = StarActor;
	RegisteredEntities.Add(Body.BodyId, Entry);
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
