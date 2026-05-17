#include "Runtime/StargameSessionSubsystem.h"

#include "Data/FrontierTestFixtureProvider.h"
#include "Data/StarCatalogSubsystem.h"
#include "Engine/Engine.h"
#include "Flight/SpaceFlightPawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/StargameM0SaveGame.h"
#include "Space/LogicalTrafficQueryService.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Space/StarSystemSubsystem.h"
#include "Space/SystemicGameplayQueryService.h"

DEFINE_LOG_CATEGORY_STATIC(LogStargameStartup, Log, All);

namespace
{
	bool ValidateNativeFallbackFixture(const UStarCatalogSubsystem* Catalog, const FStarSystemDefinition& SystemDefinition, FString& OutError)
	{
		if (Catalog && Catalog->IsUsingAssetCatalog())
		{
			return true;
		}

		return FFrontierTestFixtureProvider::ValidateM0Fixture(SystemDefinition, OutError);
	}

	bool GoalReferencesValidRoute(const FShipGoalState& Goal, const TSet<FName>& RouteIds, FString& OutError)
	{
		if (Goal.GoalKind != EShipGoalKind::TradeRoute)
		{
			return true;
		}

		if (Goal.RouteSegmentId.IsNone() || !RouteIds.Contains(Goal.RouteSegmentId))
		{
			OutError = FString::Printf(TEXT("Saved traffic goal references missing route '%s'."), *Goal.RouteSegmentId.ToString());
			return false;
		}

		return true;
	}
}

void UStargameSessionSubsystem::Tick(float DeltaTime)
{
	if (DeltaTime > 0.0f)
	{
		AdvanceSimulationClock(DeltaTime);
	}
}

TStatId UStargameSessionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UStargameSessionSubsystem, STATGROUP_Tickables);
}

bool UStargameSessionSubsystem::IsTickable() const
{
	return !IsTemplate() && !CurrentSystemId.IsNone();
}

EStartSessionResult UStargameSessionSubsystem::StartNewSession(FName InStartProfileId)
{
	LastSessionError.Reset();
	LastStartSessionResult = EStartSessionResult::ValidationFailed;

	const FName RequestedStartProfileId = InStartProfileId.IsNone()
		? FFrontierTestFixtureProvider::DefaultStartProfileId
		: InStartProfileId;

	FStartProfileDefinition StartProfile;
	UStarCatalogSubsystem* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStarCatalogSubsystem>() : nullptr;
	const bool bResolvedStartProfile = Catalog
		? Catalog->ResolveStartProfile(RequestedStartProfileId, StartProfile)
		: FFrontierTestFixtureProvider::ResolveStartProfile(RequestedStartProfileId, StartProfile);
	if (!bResolvedStartProfile)
	{
		LastStartSessionResult = EStartSessionResult::MissingStartProfile;
		ReportStartupError(FString::Printf(TEXT("Missing start profile '%s'."), *RequestedStartProfileId.ToString()));
		return LastStartSessionResult;
	}

	if (!BuildAndSpawnFromStartProfile(StartProfile))
	{
		return LastStartSessionResult;
	}

	LastStartSessionResult = EStartSessionResult::Success;
	UE_LOG(LogStargameStartup, Display, TEXT("Started session '%s' in system '%s' at spawn zone '%s'."),
		*StartProfileId.ToString(),
		*CurrentSystemId.ToString(),
		*CurrentSpawnZoneId.ToString());
	return LastStartSessionResult;
}

bool UStargameSessionSubsystem::SaveDevelopmentSlot()
{
	return SaveDevelopmentSlot(MakeCurrentM0SaveState());
}

void UStargameSessionSubsystem::AdvanceSimulationClock(double DeltaSeconds)
{
	ClockSnapshot.AuthoritativeSimulationTimeSeconds += FMath::Max(0.0, DeltaSeconds) * ClockSnapshot.TimeScale;

	UWorld* World = GetWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (StarSystem && StarSystem->GetActiveSystemId() == CurrentSystemId)
	{
		StarSystem->RefreshRegisteredEntityTransforms(ClockSnapshot.AuthoritativeSimulationTimeSeconds);
		ULogicalTrafficQueryService::RefreshTransientRouteSamples(
			StarSystem->GetActiveSystemDefinition(),
			ClockSnapshot,
			ClockSnapshot.AuthoritativeSimulationTimeSeconds,
			ActiveTrafficState);
	}
}

void UStargameSessionSubsystem::ClearSessionState()
{
	CurrentSystemId = NAME_None;
	CurrentSpawnZoneId = NAME_None;
	SelectedTargetId = NAME_None;
	ClockSnapshot = FSimulationClockSnapshot();
	ActiveTrafficState = FActiveTrafficSimulationState();
	SystemicGameplayState = FSystemicGameplayState();
}

bool UStargameSessionSubsystem::SelectNavigationTargetById(FName TargetId)
{
	if (TargetId.IsNone())
	{
		SelectedTargetId = NAME_None;
		return true;
	}

	const UWorld* World = GetWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	FNavigationTargetDefinition Target;
	if (!StarSystem || !StarSystem->FindNavigationTarget(TargetId, Target) || !Target.bCanTarget)
	{
		return false;
	}

	SelectedTargetId = TargetId;
	return true;
}

bool UStargameSessionSubsystem::CycleNavigationTarget()
{
	const UWorld* World = GetWorld();
	const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem)
	{
		return false;
	}

	TArray<FNavigationTargetDefinition> Targets;
	StarSystem->GetNavigationTargets(Targets);
	Targets.RemoveAll([](const FNavigationTargetDefinition& Target)
	{
		return !Target.bCanTarget || !Target.bShowInHud;
	});

	if (Targets.IsEmpty())
	{
		SelectedTargetId = NAME_None;
		return true;
	}

	int32 NextIndex = 0;
	for (int32 Index = 0; Index < Targets.Num(); ++Index)
	{
		if (Targets[Index].TargetId == SelectedTargetId)
		{
			NextIndex = (Index + 1) % Targets.Num();
			break;
		}
	}

	SelectedTargetId = Targets[NextIndex].TargetId;
	return true;
}

bool UStargameSessionSubsystem::LoadDevelopmentSlot()
{
	FStargameM0SaveState LoadedState;
	if (!LoadDevelopmentSlot(LoadedState))
	{
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	UStarCatalogSubsystem* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStarCatalogSubsystem>() : nullptr;
	bool bResolvedSystem = Catalog
		? Catalog->ResolveSystemDefinition(LoadedState.SystemId, SystemDefinition)
		: FFrontierTestFixtureProvider::ResolveSystemDefinition(LoadedState.SystemId, SystemDefinition);
	if (!bResolvedSystem && Catalog && LoadedState.bSavedSystemIsGenerated)
	{
		FStarSystemDefinition RegeneratedSystem;
		if (Catalog->GenerateSeededPhysicalSystem(LoadedState.GeneratedSystemSourceMetadata.GeneratedSeed, RegeneratedSystem) &&
			RegeneratedSystem.SystemId == LoadedState.SystemId &&
			RegeneratedSystem.GeneratedSourceMetadata.GeneratorVersion == LoadedState.GeneratedSystemSourceMetadata.GeneratorVersion &&
			RegeneratedSystem.GeneratedSourceMetadata.SourceFingerprint == LoadedState.GeneratedSystemSourceMetadata.SourceFingerprint)
		{
			SystemDefinition = RegeneratedSystem;
			bResolvedSystem = true;
		}
	}
	if (!bResolvedSystem)
	{
		ReportStartupError(FString::Printf(TEXT("Saved system '%s' could not be resolved."), *LoadedState.SystemId.ToString()));
		return false;
	}

	FString ValidationError;
	if (!ValidateNativeFallbackFixture(Catalog, SystemDefinition, ValidationError))
	{
		ReportStartupError(ValidationError);
		return false;
	}

	FSimulationClockSnapshot RestoredClock = LoadedState.ClockSnapshot;
	if (RestoredClock.ClockOwner.IsNone())
	{
		RestoredClock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(LoadedState.SystemId, LoadedState.ShipLocation.AuthoritativeSimulationTimeSeconds);
	}
	FActiveTrafficSimulationState RestoredTrafficState = LoadedState.ActiveTrafficState;
	if (RestoredTrafficState.SystemId.IsNone())
	{
		RestoredTrafficState.SystemId = LoadedState.SystemId;
		RestoredTrafficState.Ships = SystemDefinition.LogicalTraffic;
		RestoredTrafficState.Groups = SystemDefinition.ShipGroups;
	}
	FSystemicGameplayState RestoredSystemicState = LoadedState.SystemicGameplayState;
	if (RestoredSystemicState.Factions.IsEmpty())
	{
		RestoredSystemicState = USystemicGameplayQueryService::MakeM9FixtureState(SystemDefinition);
	}
	ULogicalTrafficQueryService::RefreshTransientRouteSamples(SystemDefinition, RestoredClock, RestoredClock.AuthoritativeSimulationTimeSeconds, RestoredTrafficState);
	if (!ValidateLoadedSystemicState(SystemDefinition, RestoredSystemicState, ValidationError))
	{
		ReportStartupError(ValidationError);
		return false;
	}
	if (!ValidateLoadedTrafficState(SystemDefinition, RestoredTrafficState, ValidationError))
	{
		ReportStartupError(ValidationError);
		return false;
	}

	UWorld* World = GetWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem || !StarSystem->BuildSystem(SystemDefinition, RestoredClock.AuthoritativeSimulationTimeSeconds))
	{
		ReportStartupError(StarSystem ? StarSystem->GetLastBuildError() : TEXT("Missing star system subsystem."));
		ClearSessionState();
		return false;
	}
	if (!StarSystem->RefreshRegisteredEntityTransforms(RestoredClock.AuthoritativeSimulationTimeSeconds))
	{
		ReportStartupError(TEXT("Saved system could not refresh moving entity transforms at the restored simulation time."));
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}

	TSubclassOf<APawn> PawnClass = nullptr;
	FStartProfileDefinition StartProfile;
	const FName LoadStartProfileId = StartProfileId.IsNone()
		? FFrontierTestFixtureProvider::DefaultStartProfileId
		: StartProfileId;
	const bool bResolvedLoadStartProfile = Catalog
		? Catalog->ResolveStartProfile(LoadStartProfileId, StartProfile)
		: FFrontierTestFixtureProvider::ResolveStartProfile(LoadStartProfileId, StartProfile);
	if (bResolvedLoadStartProfile && StartProfile.SystemId == LoadedState.SystemId && StartProfile.SpawnZoneId == LoadedState.SpawnZoneId)
	{
		FShipArchetypeDefinition Ship;
		if (Catalog && Catalog->ResolveShipArchetype(StartProfile.ShipArchetypeId, Ship))
		{
			PawnClass = Ship.PawnClass.LoadSynchronous();
		}
	}

	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!StarSystem->SpawnPlayerAtSpawnZone(LoadedState.SpawnZoneId, PlayerController, PawnClass))
	{
		ReportStartupError(StarSystem->GetLastBuildError());
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}

	CurrentSystemId = LoadedState.SystemId;
	CurrentSpawnZoneId = LoadedState.SpawnZoneId;
	SelectedTargetId = LoadedState.SelectedTargetId;
	ClockSnapshot = RestoredClock;
	ActiveTrafficState = RestoredTrafficState;
	SystemicGameplayState = RestoredSystemicState;

	FString RestoreError;
	if (!RestoreSavedShipLocation(LoadedState.ShipLocation, PlayerController, RestoreError))
	{
		ReportStartupError(RestoreError);
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}

	return true;
}

bool UStargameSessionSubsystem::SaveDevelopmentSlot(const FStargameM0SaveState& State)
{
	UStargameM0SaveGame* SaveGame = Cast<UStargameM0SaveGame>(UGameplayStatics::CreateSaveGameObject(UStargameM0SaveGame::StaticClass()));
	if (!SaveGame)
	{
		ReportStartupError(TEXT("Failed to create M0 save game object."));
		return false;
	}

	SaveGame->State = State;

	constexpr int32 UserIndex = 0;
	const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, DevelopmentSlotName, UserIndex);
	if (!bSaved)
	{
		ReportStartupError(FString::Printf(TEXT("Failed to save development slot '%s'."), DevelopmentSlotName));
	}
	return bSaved;
}

bool UStargameSessionSubsystem::LoadDevelopmentSlot(FStargameM0SaveState& OutState)
{
	constexpr int32 UserIndex = 0;
	UStargameM0SaveGame* SaveGame = Cast<UStargameM0SaveGame>(UGameplayStatics::LoadGameFromSlot(DevelopmentSlotName, UserIndex));
	if (!SaveGame)
	{
		ReportStartupError(FString::Printf(TEXT("Failed to load development slot '%s'."), DevelopmentSlotName));
		return false;
	}

	OutState = SaveGame->State;
	return true;
}

FStargameM0SaveState UStargameSessionSubsystem::MakeCurrentM0SaveState() const
{
	FStargameM0SaveState State;
	State.SystemId = CurrentSystemId;
	State.SpawnZoneId = CurrentSpawnZoneId;
	State.SelectedTargetId = SelectedTargetId;
	State.ClockSnapshot = ClockSnapshot;
	State.ActiveTrafficState = ULogicalTrafficQueryService::SanitizeTrafficStateForSave(ActiveTrafficState);
	State.SystemicGameplayState = SystemicGameplayState;
	State.ShipLocation.SystemId = CurrentSystemId;
	State.ShipLocation.CoordinateFrame.FrameType = TEXT("system_barycentric");
	State.ShipLocation.CoordinateFrame.AnchorId = CurrentSystemId;
	State.ShipLocation.LocationMode = EShipLocationMode::Respawn;
	State.ShipLocation.AnchorId = CurrentSpawnZoneId;
	State.ShipLocation.VelocityFrame = State.ShipLocation.CoordinateFrame;
	State.ShipLocation.AuthoritativeSimulationTimeSeconds = ClockSnapshot.AuthoritativeSimulationTimeSeconds;

	const UWorld* World = GetWorld();
	if (const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr)
	{
		const FStarSystemDefinition& ActiveDefinition = StarSystem->GetActiveSystemDefinition();
		if (ActiveDefinition.SystemId == CurrentSystemId && ActiveDefinition.SourceType == ESystemSourceType::Generated)
		{
			State.bSavedSystemIsGenerated = true;
			State.GeneratedSystemSourceMetadata = ActiveDefinition.GeneratedSourceMetadata;
		}
	}
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	const ASpaceFlightPawn* FlightPawn = PlayerController ? Cast<ASpaceFlightPawn>(PlayerController->GetPawn()) : nullptr;
	if (FlightPawn)
	{
		State.ShipLocation.PositionCm = FlightPawn->GetLogicalSystemPositionCm();
		State.ShipLocation.Rotation = FlightPawn->GetActorRotation();
		State.ShipLocation.LinearVelocityCmPerSec = FlightPawn->GetLinearVelocityCmPerSec();
		State.ShipLocation.CoordinateFrame.AnchorId = CurrentSystemId;
		State.ShipLocation.LocationMode = EShipLocationMode::FreeFlight;
		State.ShipLocation.AnchorId = CurrentSystemId;
		State.ShipLocation.VelocityFrame = State.ShipLocation.CoordinateFrame;
		const FDockingOperationState Docking = FlightPawn->GetDockingOperationState();
		if (Docking.DockingState == EDockingState::Docked && !Docking.StationId.IsNone() && !Docking.PortId.IsNone())
		{
			State.ShipLocation.CoordinateFrame.FrameType = TEXT("station_relative");
			State.ShipLocation.CoordinateFrame.AnchorId = Docking.StationId;
			State.ShipLocation.LocationMode = EShipLocationMode::StationDocked;
			State.ShipLocation.AnchorId = Docking.StationId;
			State.ShipLocation.VelocityFrame = State.ShipLocation.CoordinateFrame;
			State.ShipLocation.DockedStationId = Docking.StationId;
			State.ShipLocation.DockingPortId = Docking.PortId;
			State.ShipLocation.DockedShipInstanceId = Docking.ShipInstanceId;
			State.ShipLocation.DockingClearanceId = Docking.ClearanceId;
			State.ShipLocation.DockingState = EDockingState::Docked;
			State.ShipLocation.PortRelativeTransform = Docking.CapturedPortRelativeTransform;
			if (const UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr)
			{
				FDockingPortRuntimeState RuntimeState;
				if (StarSystem->FindDockingPortRuntimeState(Docking.StationId, Docking.PortId, RuntimeState))
				{
					State.ShipLocation.DockingReservedShipId = RuntimeState.ReservedShipId;
					State.ShipLocation.DockingOccupyingShipId = RuntimeState.OccupyingShipId;
					State.ShipLocation.DockingReservationExpiryTimeSeconds = RuntimeState.ReservationExpiryTimeSeconds;
					State.ShipLocation.DockingPortRuntimeState = RuntimeState.DockingState;
				}
			}
		}
	}
	return State;
}

bool UStargameSessionSubsystem::ValidateLoadedTrafficState(const FStarSystemDefinition& SystemDefinition, const FActiveTrafficSimulationState& TrafficState, FString& OutError) const
{
	if (TrafficState.SystemId != SystemDefinition.SystemId)
	{
		OutError = FString::Printf(TEXT("Saved traffic system '%s' does not match loaded system '%s'."),
			*TrafficState.SystemId.ToString(),
			*SystemDefinition.SystemId.ToString());
		return false;
	}

	TSet<FName> RouteIds;
	for (const FTrafficRouteSegmentDefinition& Route : SystemDefinition.TrafficRoutes)
	{
		RouteIds.Add(Route.RouteSegmentId);
	}

	TSet<FName> ShipIds;
	for (const FShipTrafficInstance& Ship : TrafficState.Ships)
	{
		if (Ship.ShipInstanceId.IsNone())
		{
			OutError = TEXT("Saved traffic contains an empty ship instance ID.");
			return false;
		}
		if (Ship.SystemId != SystemDefinition.SystemId)
		{
			OutError = FString::Printf(TEXT("Saved traffic ship '%s' belongs to system '%s', expected '%s'."),
				*Ship.ShipInstanceId.ToString(),
				*Ship.SystemId.ToString(),
				*SystemDefinition.SystemId.ToString());
			return false;
		}
		if (ShipIds.Contains(Ship.ShipInstanceId))
		{
			OutError = FString::Printf(TEXT("Saved traffic contains duplicate ship instance ID '%s'."), *Ship.ShipInstanceId.ToString());
			return false;
		}
		if (!GoalReferencesValidRoute(Ship.CurrentGoal, RouteIds, OutError) ||
			(Ship.bHasSavedGoal && !GoalReferencesValidRoute(Ship.SavedGoal, RouteIds, OutError)))
		{
			OutError = FString::Printf(TEXT("Saved traffic ship '%s' has an invalid route goal: %s"),
				*Ship.ShipInstanceId.ToString(),
				*OutError);
			return false;
		}
		if (Ship.CurrentGoal.GoalKind == EShipGoalKind::TradeRoute)
		{
			if (!FMath::IsWithinInclusive(Ship.CurrentGoal.RouteProgress01, 0.0, 1.0))
			{
				OutError = FString::Printf(TEXT("Saved traffic ship '%s' has route progress outside [0,1]."), *Ship.ShipInstanceId.ToString());
				return false;
			}
			if (Ship.CurrentGoal.TargetFrame.RouteSegmentId != Ship.CurrentGoal.RouteSegmentId)
			{
				OutError = FString::Printf(TEXT("Saved traffic ship '%s' target frame route does not match current goal route."), *Ship.ShipInstanceId.ToString());
				return false;
			}
			if (Ship.LogicalLocation.RouteSegmentId != Ship.CurrentGoal.RouteSegmentId ||
				Ship.LogicalLocation.TargetType != FName(TEXT("route_sample")))
			{
				OutError = FString::Printf(TEXT("Saved traffic ship '%s' route goal is missing durable route logical location."), *Ship.ShipInstanceId.ToString());
				return false;
			}
		}
		else if (!Ship.bRouteRecoverable && Ship.LogicalLocation.TargetType != FName(TEXT("off_route_free_flight")))
		{
			OutError = FString::Printf(TEXT("Saved traffic ship '%s' is non-recoverable without an off-route logical location."), *Ship.ShipInstanceId.ToString());
			return false;
		}
		if (Ship.TrafficTier == ELogicalTrafficTier::Tier1Realized || !Ship.RealizationToken.IsNone())
		{
			OutError = FString::Printf(TEXT("Saved traffic ship '%s' contains transient realization state."), *Ship.ShipInstanceId.ToString());
			return false;
		}
		ShipIds.Add(Ship.ShipInstanceId);
	}

	TSet<FName> GroupIds;
	for (const FShipGroupState& Group : TrafficState.Groups)
	{
		if (Group.GroupId.IsNone())
		{
			OutError = TEXT("Saved traffic contains an empty group ID.");
			return false;
		}
		if (GroupIds.Contains(Group.GroupId))
		{
			OutError = FString::Printf(TEXT("Saved traffic contains duplicate group ID '%s'."), *Group.GroupId.ToString());
			return false;
		}
		if (!Group.LeaderShipInstanceId.IsNone() && !ShipIds.Contains(Group.LeaderShipInstanceId))
		{
			OutError = FString::Printf(TEXT("Saved traffic group '%s' references missing leader '%s'."),
				*Group.GroupId.ToString(),
				*Group.LeaderShipInstanceId.ToString());
			return false;
		}
		for (const FName MemberShipInstanceId : Group.MemberShipInstanceIds)
		{
			if (MemberShipInstanceId.IsNone() || !ShipIds.Contains(MemberShipInstanceId))
			{
				OutError = FString::Printf(TEXT("Saved traffic group '%s' references missing member '%s'."),
					*Group.GroupId.ToString(),
					*MemberShipInstanceId.ToString());
				return false;
			}
		}
		for (const FShipFormationSlot& Slot : Group.FormationSlots)
		{
			if (!Slot.ShipInstanceId.IsNone() && !ShipIds.Contains(Slot.ShipInstanceId))
			{
				OutError = FString::Printf(TEXT("Saved traffic group '%s' slot '%s' references missing ship '%s'."),
					*Group.GroupId.ToString(),
					*Slot.SlotId.ToString(),
					*Slot.ShipInstanceId.ToString());
				return false;
			}
		}
		GroupIds.Add(Group.GroupId);
	}

	return true;
}

bool UStargameSessionSubsystem::ValidateLoadedSystemicState(const FStarSystemDefinition& SystemDefinition, const FSystemicGameplayState& LoadedSystemicState, FString& OutError) const
{
	if (!USystemicGameplayQueryService::ValidateSystemicGameplayState(SystemDefinition, LoadedSystemicState, OutError))
	{
		OutError = FString::Printf(TEXT("Saved systemic gameplay state is invalid: %s"), *OutError);
		return false;
	}
	return true;
}

bool UStargameSessionSubsystem::RestoreSavedShipLocation(const FShipSaveLocation& ShipLocation, APlayerController* PlayerController, FString& OutError)
{
	if (ShipLocation.LocationMode == EShipLocationMode::Respawn)
	{
		return true;
	}

	APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	ASpaceFlightPawn* FlightPawn = Cast<ASpaceFlightPawn>(Pawn);
	if (!FlightPawn)
	{
		OutError = TEXT("Saved ship location requires a space flight pawn.");
		return false;
	}

	if (ShipLocation.LocationMode == EShipLocationMode::StationDocked)
	{
		if (ShipLocation.DockedStationId.IsNone() || ShipLocation.DockingPortId.IsNone())
		{
			OutError = TEXT("Saved docked location is missing station or docking port ID.");
			return false;
		}

		if (!FlightPawn->RestoreDockedAt(
			ShipLocation.DockedStationId,
			ShipLocation.DockingPortId,
			ShipLocation.DockedShipInstanceId,
			ShipLocation.DockingClearanceId,
			ClockSnapshot.AuthoritativeSimulationTimeSeconds))
		{
			OutError = TEXT("Saved docked location could not resolve its live docking port.");
			return false;
		}
		return true;
	}

	if (ShipLocation.LocationMode == EShipLocationMode::FreeFlight || ShipLocation.LocationMode == EShipLocationMode::GateArrival)
	{
		if (ShipLocation.SystemId != CurrentSystemId)
		{
			OutError = FString::Printf(TEXT("Saved ship location system '%s' does not match loaded system '%s'."),
				*ShipLocation.SystemId.ToString(),
				*CurrentSystemId.ToString());
			return false;
		}
		const bool bSystemFrame = ShipLocation.CoordinateFrame.FrameType == FName(TEXT("system_barycentric")) && ShipLocation.CoordinateFrame.AnchorId == CurrentSystemId;
		const bool bLegacySystemAnchoredFreeFlight = ShipLocation.CoordinateFrame.FrameType == FName(TEXT("local_free_flight")) && ShipLocation.CoordinateFrame.AnchorId == CurrentSystemId;
		if (!bSystemFrame && !bLegacySystemAnchoredFreeFlight)
		{
			OutError = TEXT("Saved free-flight location must use a system_barycentric coordinate frame.");
			return false;
		}

		FlightPawn->SetFlightTestTransformAndVelocity(
			FTransform(ShipLocation.Rotation, ShipLocation.PositionCm),
			ShipLocation.LinearVelocityCmPerSec);
		return true;
	}

	OutError = TEXT("Unsupported saved ship location mode.");
	return false;
}

FString UStargameSessionSubsystem::GetM0DebugSummary() const
{
	const FString SaveSlotStatus = UGameplayStatics::DoesSaveGameExist(DevelopmentSlotName, 0)
		? TEXT("present")
		: TEXT("missing");

	return FString::Printf(
		TEXT("StartProfile=%s\nCurrentSystem=%s\nSpawnZone=%s\nSelectedTarget=%s\nClockOwner=%s\nSimulationTimeSeconds=%.2f\nLastStartResult=%d\nLastSessionError=%s\nSaveSlot=%s"),
		*StartProfileId.ToString(),
		*CurrentSystemId.ToString(),
		*CurrentSpawnZoneId.ToString(),
		*SelectedTargetId.ToString(),
		*ClockSnapshot.ClockOwner.ToString(),
		ClockSnapshot.AuthoritativeSimulationTimeSeconds,
		static_cast<int32>(LastStartSessionResult),
		*LastSessionError,
		*SaveSlotStatus);
}

bool UStargameSessionSubsystem::BuildAndSpawnFromStartProfile(const FStartProfileDefinition& StartProfile)
{
	FStarSystemDefinition SystemDefinition;
	UStarCatalogSubsystem* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UStarCatalogSubsystem>() : nullptr;
	const bool bResolvedSystem = Catalog
		? Catalog->ResolveSystemDefinition(StartProfile.SystemId, SystemDefinition)
		: FFrontierTestFixtureProvider::ResolveSystemDefinition(StartProfile.SystemId, SystemDefinition);
	if (!bResolvedSystem)
	{
		LastStartSessionResult = EStartSessionResult::MissingSystemDefinition;
		ReportStartupError(FString::Printf(TEXT("Missing system definition '%s'."), *StartProfile.SystemId.ToString()));
		return false;
	}

	FString ValidationError;
	if (!ValidateNativeFallbackFixture(Catalog, SystemDefinition, ValidationError))
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(ValidationError);
		return false;
	}

	UWorld* World = GetWorld();
	UStarSystemSubsystem* StarSystem = World ? World->GetSubsystem<UStarSystemSubsystem>() : nullptr;
	if (!StarSystem)
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(TEXT("Missing star system subsystem."));
		return false;
	}

	if (!StarSystem->BuildSystem(SystemDefinition, 0.0))
	{
		LastStartSessionResult = EStartSessionResult::ValidationFailed;
		ReportStartupError(StarSystem->GetLastBuildError());
		ClearSessionState();
		return false;
	}

	FSpawnZoneDefinition SpawnZone;
	if (!StarSystem->FindSpawnZone(StartProfile.SpawnZoneId, SpawnZone))
	{
		LastStartSessionResult = EStartSessionResult::MissingSpawnZone;
		ReportStartupError(FString::Printf(TEXT("Missing spawn zone '%s'."), *StartProfile.SpawnZoneId.ToString()));
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}

	StartProfileId = StartProfile.StartProfileId;
	CurrentSystemId = StartProfile.SystemId;
	CurrentSpawnZoneId = StartProfile.SpawnZoneId;
	SelectedTargetId = NAME_None;
	ClockSnapshot = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(CurrentSystemId, 0.0);
	ActiveTrafficState.SystemId = CurrentSystemId;
	ActiveTrafficState.Ships = SystemDefinition.LogicalTraffic;
	ActiveTrafficState.Groups = SystemDefinition.ShipGroups;
	ULogicalTrafficQueryService::RefreshTransientRouteSamples(SystemDefinition, ClockSnapshot, ClockSnapshot.AuthoritativeSimulationTimeSeconds, ActiveTrafficState);
	SystemicGameplayState = USystemicGameplayQueryService::MakeM9FixtureState(SystemDefinition);

	TSubclassOf<APawn> PawnClass = nullptr;
	if (Catalog)
	{
		FShipArchetypeDefinition Ship;
		if (Catalog->ResolveShipArchetype(StartProfile.ShipArchetypeId, Ship))
		{
			PawnClass = Ship.PawnClass.LoadSynchronous();
		}
	}

	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!StarSystem->SpawnPlayerAtSpawnZone(CurrentSpawnZoneId, PlayerController, PawnClass))
	{
		LastStartSessionResult = EStartSessionResult::MissingSpawnZone;
		ReportStartupError(StarSystem->GetLastBuildError());
		StarSystem->TearDownActiveSystem();
		ClearSessionState();
		return false;
	}

	return true;
}

void UStargameSessionSubsystem::ReportStartupError(const FString& Error)
{
	LastSessionError = Error;
	UE_LOG(LogStargameStartup, Error, TEXT("%s"), *LastSessionError);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			8.0f,
			FColor::Red,
			FString::Printf(TEXT("Stargame startup error: %s"), *LastSessionError));
	}
}
