#if WITH_DEV_AUTOMATION_TESTS

#include "Data/StarCatalogSubsystem.h"
#include "Data/StargameDataAssets.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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
	TestEqual(TEXT("First build gravity well count"), GravityWellIds.Num(), 2);

	TArray<FName> DockingPortIds;
	StarSystem->GetRegisteredDockingPortIds(DockingPortIds);
	TestEqual(TEXT("First build docking port count"), DockingPortIds.Num(), 2);
	TestTrue(TEXT("First build docking port registry ID"), DockingPortIds.Contains(FName(TEXT("brink_watch/pad_01"))));

	TArray<FName> MapEntryIds;
	StarSystem->GetRegisteredMapEntryIds(MapEntryIds);
	TestEqual(TEXT("First build map entry count"), MapEntryIds.Num(), 7);

	const FString DebugSummary = StarSystem->GetM1DebugSummary();
	TestTrue(TEXT("M1 debug summary comes from registry entities"), DebugSummary.Contains(TEXT("Entities=")) && DebugSummary.Contains(TEXT("brink_minor")) && DebugSummary.Contains(TEXT("wayfarer_depot")));
	TestTrue(TEXT("M1 debug summary includes spawn zones"), DebugSummary.Contains(TEXT("SpawnZones=spawn_deep_space")));
	TestTrue(TEXT("M1 debug summary includes docking ports"), DebugSummary.Contains(TEXT("DockingPorts=brink_watch/pad_01")));
	TestTrue(TEXT("M1 debug summary includes gravity wells"), DebugSummary.Contains(TEXT("GravityWells=")) && DebugSummary.Contains(TEXT("ember_gravity_well")) && DebugSummary.Contains(TEXT("brink_gravity_well")));
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
	TestTrue(TEXT("M2 resolves docking port through headless service"), UOrbitRouteFrameQueryService::ResolveDockingPortFrame(SystemDefinition, FName(TEXT("brink_watch")), FName(TEXT("pad_01")), Clock, 30.0, DockingPort));
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

#endif
