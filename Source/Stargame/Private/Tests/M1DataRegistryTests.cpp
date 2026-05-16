#if WITH_DEV_AUTOMATION_TESTS

#include "Data/StarCatalogSubsystem.h"
#include "Data/StargameDataAssets.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flight/ShipFlightModeComponent.h"
#include "Misc/AutomationTest.h"
#include "Runtime/StargameSessionSubsystem.h"
#include "Space/OrbitRouteFrameQueryService.h"
#include "Space/StarSystemSubsystem.h"

namespace
{
	UStarCatalogSubsystem* CreateM1Catalog()
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>();
		UStarCatalogSubsystem* Catalog = NewObject<UStarCatalogSubsystem>(GameInstance);
		if (Catalog)
		{
			Catalog->BuildAssetCatalogCache(false);
		}
		return Catalog;
	}

	UWorld* FindM1AutomationWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}

		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			UWorld* World = WorldContext.World();
			if (World && World->PersistentLevel)
			{
				return World;
			}
		}

		return nullptr;
	}

	bool AssetManagerScansRoot(FPrimaryAssetType AssetType, const FString& RequiredRoot)
	{
		const UAssetManagerSettings* Settings = GetDefault<UAssetManagerSettings>();
		if (!Settings)
		{
			return false;
		}

		for (const FPrimaryAssetTypeInfo& TypeInfo : Settings->PrimaryAssetTypesToScan)
		{
			if (TypeInfo.PrimaryAssetType != AssetType)
			{
				continue;
			}

			for (const FDirectoryPath& Directory : TypeInfo.GetDirectories())
			{
				if (Directory.Path == RequiredRoot || RequiredRoot.StartsWith(Directory.Path + TEXT("/")))
				{
					return true;
				}
			}
		}

		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM1CatalogValidationTest,
	"Stargame.M1.Catalog.ValidatesFrontierFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM1CatalogValidationTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}
	TestTrue(TEXT("M1 catalog is Asset Manager backed"), Catalog->IsUsingAssetCatalog());

	FStartProfileDefinition StartProfile;
	TestTrue(TEXT("M1 start profile resolves"), Catalog->ResolveStartProfile(FName(TEXT("frontier_test_start")), StartProfile));
	TestEqual(TEXT("M1 start profile targets frontier fixture"), StartProfile.SystemId, FName(TEXT("frontier_test_01")));
	TestEqual(TEXT("M1 start profile uses deep-space spawn"), StartProfile.SpawnZoneId, FName(TEXT("spawn_deep_space")));
	TestEqual(TEXT("M1 start profile has ship archetype"), StartProfile.ShipArchetypeId, FName(TEXT("wayfarer_mk1")));

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M1 system resolves"), Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));
	TestTrue(TEXT("M1 body visual profiles are set"), SystemDefinition.Bodies.Num() >= 1 && SystemDefinition.Bodies.ContainsByPredicate([](const FBodyDefinition& Body) { return Body.VisualProfileId.IsValid(); }));
	TestTrue(TEXT("M1 station profiles are set"), SystemDefinition.Stations.Num() >= 1 && SystemDefinition.Stations.ContainsByPredicate([](const FStationDefinition& Station) { return Station.StationProfileId.IsValid(); }));
	TestTrue(TEXT("M1 station docking ports are authored"), SystemDefinition.Stations.ContainsByPredicate([](const FStationDefinition& Station) { return !Station.DockingPorts.IsEmpty(); }));
	TestTrue(TEXT("M1 gate profile is set"), SystemDefinition.Gates.Num() == 1 && SystemDefinition.Gates[0].GateProfileId.IsValid());
	TestTrue(TEXT("M1 gravity wells are authored"), SystemDefinition.GravityWells.Num() >= 1);
	TestTrue(TEXT("M1 map entries are authored"), SystemDefinition.MapEntries.Num() >= 3);

	FShipArchetypeDefinition Ship;
	TestTrue(TEXT("M1 ship archetype resolves"), Catalog->ResolveShipArchetype(StartProfile.ShipArchetypeId, Ship));
	TestFalse(TEXT("M1 ship pawn class is set"), Ship.PawnClass.IsNull());
	TestFalse(TEXT("M1 ship class tags are set"), Ship.ShipClassTags.IsEmpty());
	TestTrue(TEXT("M1 cargo mass is positive"), Ship.DefaultCargoCapacityMassKg > 0.0);
	TestTrue(TEXT("M1 cargo volume is positive"), Ship.DefaultCargoCapacityVolumeM3 > 0.0);

	const FStargameValidationReport Report = Catalog->ValidateM1Fixture(FName(TEXT("frontier_test_01")));
	TestFalse(TEXT("M1 validation report has no blocking issues"), Report.HasBlockingIssues());
	if (Report.HasBlockingIssues())
	{
		for (const FStargameValidationIssue& Issue : Report.Issues)
		{
			AddError(Issue.Message);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM1RegistryRebuildTest,
	"Stargame.M1.Registry.BuildTeardownRebuildInvalidatesCleanly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM1RegistryRebuildTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M1 system resolves"), Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}

	TestTrue(TEXT("First build succeeds"), StarSystem->BuildSystem(SystemDefinition));

	TArray<FName> EntityIds;
	StarSystem->GetRegisteredEntityIds(EntityIds);
	TestEqual(TEXT("First build entity count"), EntityIds.Num(), 7);

	TArray<FName> SpawnZoneIds;
	StarSystem->GetRegisteredSpawnZoneIds(SpawnZoneIds);
	TestEqual(TEXT("First build spawn zone count"), SpawnZoneIds.Num(), 1);

	TArray<FName> GravityWellIds;
	StarSystem->GetRegisteredGravityWellIds(GravityWellIds);
	TestEqual(TEXT("First build gravity well count"), GravityWellIds.Num(), 3);

	TArray<FName> DockingPortIds;
	StarSystem->GetRegisteredDockingPortIds(DockingPortIds);
	TestEqual(TEXT("First build docking port count"), DockingPortIds.Num(), 2);
	TestTrue(TEXT("First build docking port registry ID"), DockingPortIds.Contains(FName(TEXT("brink_watch/brink_watch_port_a"))));

	TArray<FName> MapEntryIds;
	StarSystem->GetRegisteredMapEntryIds(MapEntryIds);
	TestEqual(TEXT("First build map entry count"), MapEntryIds.Num(), 7);

	const FString DebugSummary = StarSystem->GetM1DebugSummary();
	TestTrue(TEXT("M1 debug summary comes from registry entities"), DebugSummary.Contains(TEXT("Entities=")) && DebugSummary.Contains(TEXT("brink_minor")) && DebugSummary.Contains(TEXT("wayfarer_depot")));
	TestTrue(TEXT("M1 debug summary includes spawn zones"), DebugSummary.Contains(TEXT("SpawnZones=spawn_deep_space")));
	TestTrue(TEXT("M1 debug summary includes docking ports"), DebugSummary.Contains(TEXT("DockingPorts=brink_watch/brink_watch_port_a")));
	TestTrue(TEXT("M1 debug summary includes gravity wells"), DebugSummary.Contains(TEXT("GravityWells=")) && DebugSummary.Contains(TEXT("ember_well")) && DebugSummary.Contains(TEXT("brink_well")) && DebugSummary.Contains(TEXT("brink_minor_well")));
	TestTrue(TEXT("M1 debug summary includes map entries"), DebugSummary.Contains(TEXT("MapEntries=")) && DebugSummary.Contains(TEXT("frontier_primary")) && DebugSummary.Contains(TEXT("frontier_gate_a")));
	TestTrue(TEXT("M1 debug summary includes navigation targets"), DebugSummary.Contains(TEXT("NavigationTargets=")) && DebugSummary.Contains(TEXT("brink_watch")) && DebugSummary.Contains(TEXT("frontier_gate_a")));

	StarSystem->TearDownActiveSystem();
	StarSystem->GetRegisteredEntityIds(EntityIds);
	StarSystem->GetRegisteredSpawnZoneIds(SpawnZoneIds);
	StarSystem->GetRegisteredGravityWellIds(GravityWellIds);
	StarSystem->GetRegisteredDockingPortIds(DockingPortIds);
	StarSystem->GetRegisteredMapEntryIds(MapEntryIds);
	TestEqual(TEXT("Teardown clears entities"), EntityIds.Num(), 0);
	TestEqual(TEXT("Teardown clears spawn zones"), SpawnZoneIds.Num(), 0);
	TestEqual(TEXT("Teardown clears gravity wells"), GravityWellIds.Num(), 0);
	TestEqual(TEXT("Teardown clears docking ports"), DockingPortIds.Num(), 0);
	TestEqual(TEXT("Teardown clears map entries"), MapEntryIds.Num(), 0);

	TestTrue(TEXT("Rebuild succeeds"), StarSystem->BuildSystem(SystemDefinition));
	StarSystem->GetRegisteredEntityIds(EntityIds);
	StarSystem->GetRegisteredMapEntryIds(MapEntryIds);
	StarSystem->GetRegisteredDockingPortIds(DockingPortIds);
	TestEqual(TEXT("Rebuild entity count"), EntityIds.Num(), 7);
	TestEqual(TEXT("Rebuild map entry count"), MapEntryIds.Num(), 7);
	TestEqual(TEXT("Rebuild docking port count"), DockingPortIds.Num(), 2);

	StarSystem->TearDownActiveSystem();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM1AssetManagerConfigTest,
	"Stargame.M1.AssetManager.RequiredTypesConfigured",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM1AssetManagerConfigTest::RunTest(const FString& Parameters)
{
	struct FRequiredAssetRoot
	{
		FPrimaryAssetType AssetType;
		FString Root;
	};
	const TArray<FRequiredAssetRoot> RequiredRoots = {
		{ UStarCatalogAsset::AssetType, TEXT("/Game/Data/Catalog") },
		{ UStartProfileAsset::AssetType, TEXT("/Game/Data/StartProfiles") },
		{ UStarSystemDefinitionAsset::AssetType, TEXT("/Game/Data/Systems") },
		{ UBodyVisualProfileAsset::AssetType, TEXT("/Game/Data/Profiles/Bodies") },
		{ UAtmosphereProfileAsset::AssetType, TEXT("/Game/Data/Profiles/Atmospheres") },
		{ UStationProfileAsset::AssetType, TEXT("/Game/Data/Profiles/Stations") },
		{ UGateProfileAsset::AssetType, TEXT("/Game/Data/Profiles/Gates") },
		{ UShipArchetypeDataAsset::AssetType, TEXT("/Game/Data/Ships/Archetypes") },
		{ UShipMovementProfileAsset::AssetType, TEXT("/Game/Data/Ships/Profiles") },
		{ UShipDurabilityProfileAsset::AssetType, TEXT("/Game/Data/Ships/Profiles") },
		{ UShipLoadoutProfileAsset::AssetType, TEXT("/Game/Data/Ships/Profiles") },
		{ UShipResourceProfileAsset::AssetType, TEXT("/Game/Data/Ships/Profiles") }
	};

	for (const FRequiredAssetRoot& RequiredRoot : RequiredRoots)
	{
		TestTrue(
			FString::Printf(TEXT("Asset Manager scans '%s' under '%s'"), *RequiredRoot.AssetType.ToString(), *RequiredRoot.Root),
			AssetManagerScansRoot(RequiredRoot.AssetType, RequiredRoot.Root));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM2FrameQueryDeterminismTest,
	"Stargame.M2.FrameQuery.DeterministicNestedTransforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM2FrameQueryDeterminismTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M2 system resolves"), Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	const FStargameValidationReport Report = Catalog->ValidateM2Fixture(FName(TEXT("frontier_test_01")));
	TestFalse(TEXT("M2 validation report has no blocking issues"), Report.HasBlockingIssues());
	if (Report.HasBlockingIssues())
	{
		for (const FStargameValidationIssue& Issue : Report.Issues)
		{
			AddError(Issue.Message);
		}
	}

	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(FName(TEXT("frontier_test_01")), 0.0);
	FFrameResolvedTransform First;
	FFrameResolvedTransform Repeat;
	FFrameResolvedTransform Future;
	TestTrue(TEXT("M2 resolves nested moon at t0"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, FName(TEXT("brink_minor")), Clock, 0.0, First));
	TestTrue(TEXT("M2 resolves nested moon at t0 repeat"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, FName(TEXT("brink_minor")), Clock, 0.0, Repeat));
	TestTrue(TEXT("M2 resolves nested moon at future time"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, FName(TEXT("brink_minor")), Clock, 60.0, Future));
	TestTrue(TEXT("Same-time frame query is deterministic"), First.PositionCm.Equals(Repeat.PositionCm, 0.01));
	TestFalse(TEXT("Future frame query does not mutate authoritative clock"), Clock.AuthoritativeSimulationTimeSeconds == 60.0);
	TestFalse(TEXT("Nested orbit changes over requested time"), First.PositionCm.Equals(Future.PositionCm, 0.01));

	FFrameResolvedTransform Station;
	FFrameResolvedTransform DockingPort;
	TestTrue(TEXT("M2 resolves orbiting station"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, FName(TEXT("brink_watch")), Clock, 30.0, Station));
	TestTrue(TEXT("M2 resolves docking port through headless service"), UOrbitRouteFrameQueryService::ResolveDockingPortFrame(SystemDefinition, FName(TEXT("brink_watch")), FName(TEXT("brink_watch_port_a")), Clock, 30.0, DockingPort));
	TestTrue(TEXT("Docking port frame is station relative"), DockingPort.CoordinateFrame.FrameType == FName(TEXT("station_relative")) && DockingPort.AnchorId == FName(TEXT("brink_watch")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM2MapAndScaleTest,
	"Stargame.M2.MapAndScale.RegistryViewModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM2MapAndScaleTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M2 system resolves"), Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));
	TestEqual(TEXT("M2 local bubble radius"), SystemDefinition.Scale.LocalBubbleRadiusCm, 5000000.0);
	TestEqual(TEXT("M2 origin shift threshold"), SystemDefinition.Scale.OriginShiftThresholdCm, 2000000.0);
	TestEqual(TEXT("M2 map distance scale"), SystemDefinition.Scale.MapDistanceScaleCmPerUnit, 1000000.0);

	const FString ScaleSummary = UOrbitRouteFrameQueryService::GetScaleDebugSummary(SystemDefinition.Scale);
	TestTrue(TEXT("Scale debug summary exposes local bubble radius"), ScaleSummary.Contains(TEXT("LocalBubbleRadiusCm=5000000")));
	TestTrue(TEXT("Scale debug summary exposes map scale"), ScaleSummary.Contains(TEXT("MapDistanceScaleCmPerUnit=1000000")));

	TArray<FSystemMapEntryViewModel> MapEntries;
	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(FName(TEXT("frontier_test_01")), 0.0);
	UOrbitRouteFrameQueryService::BuildSystemMapViewModel(SystemDefinition, Clock, 0.0, MapEntries);
	TestEqual(TEXT("M2 map entry view model count"), MapEntries.Num(), 7);
	TestTrue(TEXT("M2 map includes nested moon parent"), MapEntries.ContainsByPredicate([](const FSystemMapEntryViewModel& Entry)
	{
		return Entry.EntryId == FName(TEXT("brink_minor")) && Entry.ParentId == FName(TEXT("brink"));
	}));
	TestTrue(TEXT("M2 map includes station"), MapEntries.ContainsByPredicate([](const FSystemMapEntryViewModel& Entry)
	{
		return Entry.EntryId == FName(TEXT("brink_watch")) && Entry.EntryType == FName(TEXT("station"));
	}));
	TestTrue(TEXT("M2 map includes gate"), MapEntries.ContainsByPredicate([](const FSystemMapEntryViewModel& Entry)
	{
		return Entry.EntryId == FName(TEXT("frontier_gate_a")) && Entry.EntryType == FName(TEXT("gate"));
	}));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM2RuntimeMarkerRefreshTest,
	"Stargame.M2.FrameQuery.RuntimeMarkersRefreshFromAuthoritativeTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM2RuntimeMarkerRefreshTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M2 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}

	TestTrue(TEXT("M2 active system build succeeds"), StarSystem->BuildSystem(SystemDefinition));

	TArray<FActiveSystemEntityEntry> Entities;
	StarSystem->GetRegisteredEntities(Entities);
	const FActiveSystemEntityEntry* BrinkWatchEntry = Entities.FindByPredicate([](const FActiveSystemEntityEntry& Entry)
	{
		return Entry.EntityId == FName(TEXT("brink_watch"));
	});
	TestNotNull(TEXT("Brink Watch runtime marker is registered"), BrinkWatchEntry);
	AActor* MarkerActor = BrinkWatchEntry ? BrinkWatchEntry->Actor.Get() : nullptr;
	TestNotNull(TEXT("Brink Watch runtime marker actor exists"), MarkerActor);
	if (!MarkerActor)
	{
		StarSystem->TearDownActiveSystem();
		return false;
	}

	const double RefreshTimeSeconds = 45.0;
	FFrameResolvedTransform ExpectedTransform;
	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, RefreshTimeSeconds);
	TestTrue(TEXT("Expected station frame resolves at refresh time"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, FName(TEXT("brink_watch")), Clock, RefreshTimeSeconds, ExpectedTransform));
	TestTrue(TEXT("Runtime marker refresh succeeds"), StarSystem->RefreshRegisteredEntityTransforms(RefreshTimeSeconds));
	TestTrue(TEXT("Runtime marker matches authoritative frame after refresh"), MarkerActor->GetActorLocation().Equals(ExpectedTransform.PositionCm, 0.01));

	StarSystem->TearDownActiveSystem();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM2ClockAndSaveLocationTest,
	"Stargame.M2.ClockAndSaveLocation.SessionOwnsLogicalLocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM2ClockAndSaveLocationTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UStargameSessionSubsystem* Session = NewObject<UStargameSessionSubsystem>(GameInstance);
	TestNotNull(TEXT("Session subsystem object created"), Session);
	if (!Session)
	{
		return false;
	}

	Session->AdvanceSimulationClock(12.5);
	const FSimulationClockSnapshot Snapshot = Session->GetSimulationClockSnapshot();
	TestEqual(TEXT("Session owns clock owner"), Snapshot.ClockOwner, FName(TEXT("session")));
	TestEqual(TEXT("Session clock advances authoritatively"), Snapshot.AuthoritativeSimulationTimeSeconds, 12.5);

	FStargameM0SaveState SaveState = Session->MakeCurrentM0SaveState();
	TestEqual(TEXT("Save state preserves clock time"), SaveState.ClockSnapshot.AuthoritativeSimulationTimeSeconds, Snapshot.AuthoritativeSimulationTimeSeconds);
	TestEqual(TEXT("Save location stores logical frame, not actor path"), SaveState.ShipLocation.CoordinateFrame.FrameType, FName(TEXT("local_free_flight")));
	TestEqual(TEXT("Save location mode is explicit"), SaveState.ShipLocation.LocationMode, EShipLocationMode::Respawn);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM3CatalogValidationTest,
	"Stargame.M3.Catalog.ValidatesNormalFlightFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM3CatalogValidationTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	const FStargameValidationReport Report = Catalog->ValidateM3Fixture(FName(TEXT("frontier_test_01")));
	TestFalse(TEXT("M3 validation report has no blocking issues"), Report.HasBlockingIssues());
	if (Report.HasBlockingIssues())
	{
		for (const FStargameValidationIssue& Issue : Report.Issues)
		{
			AddError(Issue.Message);
		}
	}

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M3 system resolves"), Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));
	TestEqual(TEXT("M3 normal flight max speed"), SystemDefinition.Scale.NormalFlightMaxSpeedCmPerSec, 24000.0);
	TestEqual(TEXT("M3 station approach bubble radius"), SystemDefinition.Scale.StationApproachBubbleRadiusCm, 500000.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM3TargetViewModelTest,
	"Stargame.M3.Targeting.SelectsStableTargetsById",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM3TargetViewModelTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M3 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}

	TestTrue(TEXT("M3 active system build succeeds"), StarSystem->BuildSystem(SystemDefinition));

	for (const FName TargetId : { FName(TEXT("ember")), FName(TEXT("brink_watch")), FName(TEXT("frontier_gate_a")) })
	{
		FNavigationTargetDefinition Target;
		TestTrue(FString::Printf(TEXT("Target '%s' resolves by stable ID"), *TargetId.ToString()), StarSystem->FindNavigationTarget(TargetId, Target));

		FNavigationTargetViewModel ViewModel;
		TestTrue(FString::Printf(TEXT("Target '%s' builds HUD/debug view model"), *TargetId.ToString()), StarSystem->BuildNavigationTargetViewModel(TargetId, FName(TEXT("brink_watch")), FVector::ZeroVector, FVector::ZeroVector, 0.0, ViewModel));
		TestEqual(TEXT("View model preserves target ID"), ViewModel.TargetId, TargetId);
		TestTrue(TEXT("View model exposes distance"), ViewModel.DistanceCm > 0.0);
		TestTrue(TEXT("View model resolves frame"), ViewModel.bResolved && !ViewModel.TargetFrame.FrameType.IsNone());
	}

	FNavigationTargetViewModel BeforeRebuild;
	TestTrue(TEXT("Selected station view model before rebuild"), StarSystem->BuildNavigationTargetViewModel(FName(TEXT("brink_watch")), FName(TEXT("brink_watch")), FVector::ZeroVector, FVector::ZeroVector, 0.0, BeforeRebuild));
	TestTrue(TEXT("Selected target flag before rebuild"), BeforeRebuild.bIsSelected);

	TestTrue(TEXT("M3 active system rebuild succeeds"), StarSystem->BuildSystem(SystemDefinition));
	FNavigationTargetViewModel AfterRebuild;
	TestTrue(TEXT("Selected station view model after rebuild"), StarSystem->BuildNavigationTargetViewModel(FName(TEXT("brink_watch")), FName(TEXT("brink_watch")), FVector::ZeroVector, FVector::ZeroVector, 0.0, AfterRebuild));
	TestEqual(TEXT("Target remains stable after rebuild"), AfterRebuild.TargetId, BeforeRebuild.TargetId);
	TestTrue(TEXT("Target position remains deterministic after rebuild"), AfterRebuild.ResolvedTargetTransform.PositionCm.Equals(BeforeRebuild.ResolvedTargetTransform.PositionCm, 0.01));

	StarSystem->TearDownActiveSystem();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM3LocalApproachTest,
	"Stargame.M3.NormalFlight.LocalStationAndGateApproach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM3LocalApproachTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M3 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}
	TestTrue(TEXT("M3 active system build succeeds"), StarSystem->BuildSystem(SystemDefinition));

	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(FName(TEXT("frontier_test_01")), 0.0);
	FFrameResolvedTransform StationTransform;
	TestTrue(TEXT("M3 station frame resolves"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, FName(TEXT("brink_watch")), Clock, 0.0, StationTransform));
	const FVector StationApproachPosition = StationTransform.PositionCm + FVector(SystemDefinition.Scale.StationApproachBubbleRadiusCm * 0.5, 0.0, 0.0);

	FNavigationTargetViewModel StationViewModel;
	TestTrue(TEXT("M3 station approach view model resolves"), StarSystem->BuildNavigationTargetViewModel(FName(TEXT("brink_watch")), FName(TEXT("brink_watch")), StationApproachPosition, StationTransform.LinearVelocityCmPerSec, 0.0, StationViewModel));
	TestTrue(TEXT("Normal flight approach enters station bubble"), StationViewModel.bInsideStationApproachBubble);
	TestTrue(TEXT("Station approach inherits a moving frame velocity source"), StationViewModel.ResolvedTargetTransform.LinearVelocityCmPerSec.SizeSquared() > 1.0);

	FFrameResolvedTransform GateTransform;
	TestTrue(TEXT("M3 gate frame resolves"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, FName(TEXT("frontier_gate_a")), Clock, 0.0, GateTransform));
	const FGateDefinition* Gate = SystemDefinition.Gates.FindByPredicate([](const FGateDefinition& Candidate)
	{
		return Candidate.GateId == FName(TEXT("frontier_gate_a"));
	});
	TestNotNull(TEXT("Fixture gate exists"), Gate);
	const FVector GateApproachPosition = GateTransform.PositionCm + FVector(Gate ? Gate->ActivationRangeCm * 0.5 : 1000.0, 0.0, 0.0);

	FNavigationTargetViewModel GateViewModel;
	TestTrue(TEXT("M3 gate approach view model resolves"), StarSystem->BuildNavigationTargetViewModel(FName(TEXT("frontier_gate_a")), FName(TEXT("frontier_gate_a")), GateApproachPosition, FVector::ZeroVector, 0.0, GateViewModel));
	TestTrue(TEXT("Normal flight approach enters gate activation range"), GateViewModel.bInsideGateActivationRange);
	TestFalse(TEXT("Gate approach does not imply station approach"), GateViewModel.bInsideStationApproachBubble);

	StarSystem->TearDownActiveSystem();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM3HudDebugViewModelTest,
	"Stargame.M3.HUD.UsesCppTargetViewModels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM3HudDebugViewModelTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M3 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}
	TestTrue(TEXT("M3 active system build succeeds"), StarSystem->BuildSystem(SystemDefinition));

	TArray<FNavigationTargetViewModel> ViewModels;
	StarSystem->BuildNavigationTargetViewModels(FName(TEXT("brink_watch")), FVector::ZeroVector, FVector::ZeroVector, 0.0, ViewModels);
	TestEqual(TEXT("M3 HUD target view model count"), ViewModels.Num(), 7);
	TestTrue(TEXT("M3 selected target is represented in C++ view model"), ViewModels.ContainsByPredicate([](const FNavigationTargetViewModel& ViewModel)
	{
		return ViewModel.TargetId == FName(TEXT("brink_watch")) && ViewModel.bIsSelected && ViewModel.DistanceCm > 0.0;
	}));

	const FString DebugSummary = StarSystem->GetM3DebugSummary(FName(TEXT("brink_watch")), FVector::ZeroVector, FVector::ZeroVector, 0.0);
	TestTrue(TEXT("M3 debug summary exposes normal flight"), DebugSummary.Contains(TEXT("FlightMode=Normal")));
	TestTrue(TEXT("M3 debug summary exposes selected target"), DebugSummary.Contains(TEXT("SelectedTarget=brink_watch")));
	TestTrue(TEXT("M3 debug summary exposes C++ target view models"), DebugSummary.Contains(TEXT("TargetViewModels=")) && DebugSummary.Contains(TEXT("brink_watch:station")));
	TestTrue(TEXT("M3 debug summary exposes local bubble contract"), DebugSummary.Contains(TEXT("LocalBubbleRadiusCm=5000000")) && DebugSummary.Contains(TEXT("NormalFlightMaxSpeedCmPerSec=24000")));

	StarSystem->TearDownActiveSystem();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM4CatalogValidationTest,
	"Stargame.M4.Catalog.ValidatesSupercruiseFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM4CatalogValidationTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	const FStargameValidationReport Report = Catalog->ValidateM4Fixture(FName(TEXT("frontier_test_01")));
	TestFalse(TEXT("M4 validation report has no blocking issues"), Report.HasBlockingIssues());
	if (Report.HasBlockingIssues())
	{
		for (const FStargameValidationIssue& Issue : Report.Issues)
		{
			AddError(Issue.Message);
		}
	}

	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M4 system resolves"), Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));
	TestEqual(TEXT("M4 supercruise min speed"), SystemDefinition.Scale.SupercruiseMinSpeedCmPerSec, 100000.0);
	TestEqual(TEXT("M4 supercruise max speed"), SystemDefinition.Scale.SupercruiseMaxSpeedCmPerSec, 20000000.0);
	TestEqual(TEXT("M4 supercruise spool seconds"), SystemDefinition.Scale.SupercruiseSpoolSeconds, 3.0);
	TestEqual(TEXT("M4 dropout cooldown seconds"), SystemDefinition.Scale.DropoutCooldownSeconds, 5.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM4GravityWellQueryTest,
	"Stargame.M4.Gravity.LockoutAndSpeedCeiling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM4GravityWellQueryTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M4 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(FName(TEXT("frontier_test_01")), 0.0);
	FFrameResolvedTransform EmberTransform;
	TestTrue(TEXT("M4 ember frame resolves"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, FName(TEXT("ember")), Clock, 0.0, EmberTransform));

	FGravityWellQueryResult Outside;
	TestTrue(TEXT("M4 outside gravity query resolves nearest well"), UOrbitRouteFrameQueryService::QueryNearestGravityWell(SystemDefinition, EmberTransform.PositionCm + FVector(SystemDefinition.Scale.GravitySlowdownRadiusCm * 2.0, 0.0, 0.0), FVector::ZeroVector, EShipFlightMode::Supercruise, Clock, 0.0, Outside));
	TestFalse(TEXT("Outside slowdown is not inside lockout"), Outside.bInsideLockout);
	TestEqual(TEXT("Outside speed ceiling is max supercruise"), Outside.SpeedCeilingCmPerSec, SystemDefinition.Scale.SupercruiseMaxSpeedCmPerSec);

	FGravityWellQueryResult Slowdown;
	TestTrue(TEXT("M4 slowdown gravity query resolves"), UOrbitRouteFrameQueryService::QueryNearestGravityWell(SystemDefinition, EmberTransform.PositionCm + FVector(SystemDefinition.Scale.GravitySlowdownRadiusCm * 0.75, 0.0, 0.0), FVector::ZeroVector, EShipFlightMode::Supercruise, Clock, 0.0, Slowdown));
	TestTrue(TEXT("Slowdown query detects speed ceiling reduction"), Slowdown.bInsideSlowdown && Slowdown.SpeedCeilingCmPerSec < SystemDefinition.Scale.SupercruiseMaxSpeedCmPerSec);

	FGravityWellQueryResult Lockout;
	TestTrue(TEXT("M4 lockout gravity query resolves"), UOrbitRouteFrameQueryService::QueryNearestGravityWell(SystemDefinition, EmberTransform.PositionCm + FVector(SystemDefinition.Scale.GravityLockoutRadiusCm * 0.5, 0.0, 0.0), FVector::ZeroVector, EShipFlightMode::Normal, Clock, 0.0, Lockout));
	TestTrue(TEXT("Lockout query detects lockout"), Lockout.bInsideLockout);

	FGravityWellQueryResult Dropout;
	TestTrue(TEXT("M4 dropout gravity query resolves"), UOrbitRouteFrameQueryService::QueryNearestGravityWell(SystemDefinition, EmberTransform.PositionCm + FVector(SystemDefinition.Scale.GravityLockoutRadiusCm * 0.25, 0.0, 0.0), FVector::ZeroVector, EShipFlightMode::Supercruise, Clock, 0.0, Dropout));
	TestTrue(TEXT("Dropout query forces dropout in supercruise"), Dropout.bForcedDropout);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM4SupercruiseStateMachineTest,
	"Stargame.M4.Supercruise.StateMachineAndDropout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM4SupercruiseStateMachineTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M4 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UShipFlightModeComponent* FlightMode = NewObject<UShipFlightModeComponent>();
	TestNotNull(TEXT("M4 flight mode component created"), FlightMode);
	if (!FlightMode)
	{
		return false;
	}
	FlightMode->ConfigureSupercruiseFromScale(SystemDefinition.Scale);

	FSupercruiseTargetTelemetry DistantTarget;
	DistantTarget.TargetId = FName(TEXT("brink_watch"));
	DistantTarget.DistanceCm = SystemDefinition.Scale.LocalBubbleRadiusCm * 4.0;
	DistantTarget.DistanceToDropoutBandCm = DistantTarget.DistanceCm - SystemDefinition.Scale.SupercruiseTargetDropoutMaxRadiusCm;

	FGravityWellQueryResult LockoutGravity;
	LockoutGravity.NearestWellId = FName(TEXT("ember_well"));
	LockoutGravity.bInsideLockout = true;
	LockoutGravity.SpeedCeilingCmPerSec = SystemDefinition.Scale.SupercruiseMinSpeedCmPerSec;
	TestEqual(TEXT("M4 supercruise entry is refused inside lockout"), FlightMode->RequestEnterSupercruise(LockoutGravity, DistantTarget), ESupercruiseRequestResult::InsideLockout);
	TestEqual(TEXT("Refused entry stays normal"), FlightMode->GetFlightMode(), EShipFlightMode::Normal);

	FGravityWellQueryResult ClearGravity;
	ClearGravity.NearestWellId = FName(TEXT("ember_well"));
	ClearGravity.SpeedCeilingCmPerSec = SystemDefinition.Scale.SupercruiseMaxSpeedCmPerSec;
	TestEqual(TEXT("M4 supercruise entry succeeds outside lockout"), FlightMode->RequestEnterSupercruise(ClearGravity, DistantTarget), ESupercruiseRequestResult::Success);
	TestEqual(TEXT("M4 supercruise enters spooling"), FlightMode->GetSupercruiseState(), ESupercruiseState::Spooling);

	FlightMode->TickSupercruise(SystemDefinition.Scale.SupercruiseSpoolSeconds + 0.1, ClearGravity, DistantTarget);
	TestEqual(TEXT("M4 spooling transitions to cruising"), FlightMode->GetSupercruiseState(), ESupercruiseState::Cruising);
	TestTrue(TEXT("M4 cruising speed is above normal flight"), FlightMode->GetCurrentSupercruiseSpeedCmPerSec() > SystemDefinition.Scale.NormalFlightMaxSpeedCmPerSec);

	FGravityWellQueryResult ForcedGravity = ClearGravity;
	ForcedGravity.bInsideDropout = true;
	ForcedGravity.bForcedDropout = true;
	ForcedGravity.SpeedCeilingCmPerSec = SystemDefinition.Scale.SupercruiseMinSpeedCmPerSec;
	FlightMode->TickSupercruise(0.1, ForcedGravity, DistantTarget);
	TestEqual(TEXT("M4 gravity dropout enters cooldown"), FlightMode->GetSupercruiseState(), ESupercruiseState::DropoutCooldown);
	TestEqual(TEXT("M4 dropout returns to normal flight"), FlightMode->GetFlightMode(), EShipFlightMode::Normal);
	TestTrue(TEXT("M4 dropout clamps speed to normal range"), FlightMode->GetCurrentSupercruiseSpeedCmPerSec() <= SystemDefinition.Scale.NormalFlightMaxSpeedCmPerSec);

	TestEqual(TEXT("M4 re-entry is blocked during dropout cooldown"), FlightMode->RequestEnterSupercruise(ClearGravity, DistantTarget), ESupercruiseRequestResult::SpoolInterrupted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM4SupercruiseGuidanceTest,
	"Stargame.M4.Supercruise.GuidanceComputesSafeBrakingLimit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM4SupercruiseGuidanceTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M4 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UShipFlightModeComponent* FlightMode = NewObject<UShipFlightModeComponent>();
	TestNotNull(TEXT("M4 flight mode component created"), FlightMode);
	if (!FlightMode)
	{
		return false;
	}

	FlightMode->ConfigureSupercruiseFromScale(SystemDefinition.Scale);

	FGravityWellQueryResult Gravity;
	Gravity.SpeedCeilingCmPerSec = SystemDefinition.Scale.SupercruiseMaxSpeedCmPerSec;

	FSupercruiseTargetTelemetry Target;
	Target.TargetId = FName(TEXT("brink_watch"));
	Target.DistanceCm = 2000000.0;
	Target.DistanceToDropoutBandCm = Target.DistanceCm - SystemDefinition.Scale.SupercruiseTargetDropoutMaxRadiusCm;

	const FSupercruiseGuidanceResult Guidance = FlightMode->ComputeSupercruiseGuidance(Gravity, Target);
	const double ExpectedBrakingLimit = FMath::Sqrt(2.0 * SystemDefinition.Scale.SupercruiseBrakingCmPerSec2 * Target.DistanceToDropoutBandCm);
	TestEqual(TEXT("M4 guidance uses deterministic braking speed limit"), Guidance.BrakingSpeedLimitCmPerSec, ExpectedBrakingLimit);
	TestEqual(TEXT("M4 guidance desired speed is bounded by braking limit"), Guidance.DesiredSpeedCmPerSec, ExpectedBrakingLimit);

	Target.bInsideDropoutBand = true;
	Target.bApproachReady = true;
	const FSupercruiseGuidanceResult DropoutGuidance = FlightMode->ComputeSupercruiseGuidance(Gravity, Target);
	TestTrue(TEXT("M4 guidance reports target dropout readiness"), DropoutGuidance.bTargetDropoutReady);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM4TargetApproachTelemetryTest,
	"Stargame.M4.Supercruise.TargetApproachTelemetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM4TargetApproachTelemetryTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M4 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}
	TestTrue(TEXT("M4 active system build succeeds"), StarSystem->BuildSystem(SystemDefinition));

	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(FName(TEXT("frontier_test_01")), 0.0);
	FFrameResolvedTransform StationTransform;
	TestTrue(TEXT("M4 station frame resolves"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, FName(TEXT("brink_watch")), Clock, 0.0, StationTransform));

	const FVector ObserverPosition = StationTransform.PositionCm + FVector(SystemDefinition.Scale.SupercruiseTargetDropoutMaxRadiusCm * 0.9, 0.0, 0.0);
	FSupercruiseTargetTelemetry Telemetry;
	TestTrue(TEXT("M4 target telemetry resolves"), StarSystem->BuildSupercruiseTargetTelemetry(FName(TEXT("brink_watch")), ObserverPosition, StationTransform.LinearVelocityCmPerSec, 0.0, Telemetry));
	TestEqual(TEXT("M4 target telemetry preserves target ID"), Telemetry.TargetId, FName(TEXT("brink_watch")));
	TestTrue(TEXT("M4 target telemetry enters dropout band"), Telemetry.bInsideDropoutBand);
	TestTrue(TEXT("M4 target approach can become ready in band"), Telemetry.bApproachReady);

	FGravityWellQueryResult Gravity;
	StarSystem->QueryNearestGravityWell(ObserverPosition, FVector::ZeroVector, EShipFlightMode::Supercruise, 0.0, Gravity);
	FSupercruiseTelemetry DebugTelemetry;
	DebugTelemetry.FlightMode = EShipFlightMode::Supercruise;
	DebugTelemetry.SupercruiseState = ESupercruiseState::Cruising;
	DebugTelemetry.CurrentSpeedCmPerSec = SystemDefinition.Scale.SupercruiseMinSpeedCmPerSec;
	DebugTelemetry.SpeedLimitCmPerSec = SystemDefinition.Scale.SupercruiseMaxSpeedCmPerSec;
	DebugTelemetry.Gravity = Gravity;
	DebugTelemetry.Target = Telemetry;

	const FString DebugSummary = StarSystem->GetM4DebugSummary(FName(TEXT("brink_watch")), ObserverPosition, FVector::ZeroVector, 0.0, DebugTelemetry);
	TestTrue(TEXT("M4 debug summary exposes supercruise state"), DebugSummary.Contains(TEXT("SupercruiseState=")) && DebugSummary.Contains(TEXT("Cruising")));
	TestTrue(TEXT("M4 debug summary exposes speed limit"), DebugSummary.Contains(TEXT("SpeedLimitCmPerSec=")));
	TestTrue(TEXT("M4 debug summary exposes gravity lockout"), DebugSummary.Contains(TEXT("InsideLockout=")));
	TestTrue(TEXT("M4 debug summary exposes target distance"), DebugSummary.Contains(TEXT("TargetDistanceCm=")));

	StarSystem->TearDownActiveSystem();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM4SupercruiseDistantLegTravelTest,
	"Stargame.M4.Supercruise.DistantLegDropsWithinTargetBand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM4SupercruiseDistantLegTravelTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M4 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}
	TestTrue(TEXT("M4 active system build succeeds"), StarSystem->BuildSystem(SystemDefinition));

	const FName TargetId(TEXT("brink_watch"));
	const FSimulationClockSnapshot InitialClock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, 0.0);
	FFrameResolvedTransform InitialTargetTransform;
	TestTrue(TEXT("M4 distant leg target frame resolves"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TargetId, InitialClock, 0.0, InitialTargetTransform));
	FFrameResolvedTransform InitialAnchorTransform;
	TestTrue(TEXT("M4 distant leg anchor frame resolves"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, FName(TEXT("brink")), InitialClock, 0.0, InitialAnchorTransform));

	const FVector SafeOutboundDirection = (InitialTargetTransform.PositionCm - InitialAnchorTransform.PositionCm).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	FVector ShipPositionCm = InitialTargetTransform.PositionCm + SafeOutboundDirection * SystemDefinition.Scale.LocalBubbleRadiusCm * 3.0;
	FVector ShipVelocityCmPerSec = FVector::ZeroVector;

	UShipFlightModeComponent* FlightMode = NewObject<UShipFlightModeComponent>();
	TestNotNull(TEXT("M4 flight mode component created"), FlightMode);
	if (!FlightMode)
	{
		StarSystem->TearDownActiveSystem();
		return false;
	}
	FlightMode->ConfigureSupercruiseFromScale(SystemDefinition.Scale);

	FGravityWellQueryResult Gravity;
	TestTrue(TEXT("M4 initial gravity query resolves"), StarSystem->QueryNearestGravityWell(ShipPositionCm, ShipVelocityCmPerSec, EShipFlightMode::Normal, 0.0, Gravity));
	TestFalse(TEXT("M4 distant leg starts outside gravity lockout"), Gravity.bInsideLockout);

	FSupercruiseTargetTelemetry TargetTelemetry;
	TestTrue(TEXT("M4 initial target telemetry resolves"), StarSystem->BuildSupercruiseTargetTelemetry(TargetId, ShipPositionCm, ShipVelocityCmPerSec, 0.0, TargetTelemetry));
	TestEqual(TEXT("M4 distant leg enters supercruise"), FlightMode->RequestEnterSupercruise(Gravity, TargetTelemetry), ESupercruiseRequestResult::Success);

	const double DeltaSeconds = 0.05;
	const int32 MaxSteps = 24000;
	double SimulationTimeSeconds = 0.0;
	bool bDroppedOut = false;

	for (int32 StepIndex = 0; StepIndex < MaxSteps; ++StepIndex)
	{
		TestTrue(TEXT("M4 target telemetry resolves during distant leg"), StarSystem->BuildSupercruiseTargetTelemetry(TargetId, ShipPositionCm, ShipVelocityCmPerSec, SimulationTimeSeconds, TargetTelemetry));
		TestTrue(TEXT("M4 gravity query resolves during distant leg"), StarSystem->QueryNearestGravityWell(ShipPositionCm, ShipVelocityCmPerSec, FlightMode->GetFlightMode(), SimulationTimeSeconds, Gravity));

		FlightMode->TickSupercruise(DeltaSeconds, Gravity, TargetTelemetry);
		if (FlightMode->GetSupercruiseState() == ESupercruiseState::DropoutCooldown)
		{
			bDroppedOut = true;
			ShipVelocityCmPerSec = ShipVelocityCmPerSec.GetClampedToMaxSize(SystemDefinition.Scale.NormalFlightMaxSpeedCmPerSec);
			break;
		}

		FFrameResolvedTransform CurrentTargetTransform;
		const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, SimulationTimeSeconds);
		TestTrue(TEXT("M4 target frame resolves during distant leg"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TargetId, Clock, SimulationTimeSeconds, CurrentTargetTransform));

		const FVector ToTarget = CurrentTargetTransform.PositionCm - ShipPositionCm;
		const FVector DirectionToTarget = ToTarget.GetSafeNormal();
		ShipVelocityCmPerSec = DirectionToTarget * FlightMode->GetCurrentSupercruiseSpeedCmPerSec();
		ShipPositionCm += ShipVelocityCmPerSec * DeltaSeconds;
		SimulationTimeSeconds += DeltaSeconds;
	}

	FFrameResolvedTransform FinalTargetTransform;
	const FSimulationClockSnapshot FinalClock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, SimulationTimeSeconds);
	TestTrue(TEXT("M4 final target frame resolves"), UOrbitRouteFrameQueryService::ResolveEntityFrame(SystemDefinition, TargetId, FinalClock, SimulationTimeSeconds, FinalTargetTransform));

	const double FinalDistanceCm = FVector::Distance(ShipPositionCm, FinalTargetTransform.PositionCm);
	TestTrue(TEXT("M4 distant leg forced target dropout"), bDroppedOut);
	TestTrue(TEXT("M4 distant leg exits inside configured target dropout band"), FinalDistanceCm >= SystemDefinition.Scale.SupercruiseTargetDropoutMinRadiusCm && FinalDistanceCm <= SystemDefinition.Scale.SupercruiseTargetDropoutMaxRadiusCm);
	TestEqual(TEXT("M4 distant leg returns to normal flight after dropout"), FlightMode->GetFlightMode(), EShipFlightMode::Normal);
	TestTrue(TEXT("M4 distant leg clamps ship velocity after dropout"), ShipVelocityCmPerSec.Size() <= SystemDefinition.Scale.NormalFlightMaxSpeedCmPerSec + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("M4 flight component clamps speed after dropout"), FlightMode->GetCurrentSupercruiseSpeedCmPerSec() <= SystemDefinition.Scale.NormalFlightMaxSpeedCmPerSec);

	StarSystem->TearDownActiveSystem();
	return true;
}

#endif
