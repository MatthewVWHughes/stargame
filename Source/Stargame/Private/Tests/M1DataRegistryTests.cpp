#if WITH_DEV_AUTOMATION_TESTS

#include "Data/StarCatalogSubsystem.h"
#include "Data/StargameDataAssets.h"
#include "Engine/AssetManagerSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flight/ShipFlightModeComponent.h"
#include "Flight/SpaceFlightPawn.h"
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

	ASpaceFlightPawn* SpawnM5PawnAt(UWorld* World, const FFrameResolvedTransform& Transform, const FVector& VelocityCmPerSec)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASpaceFlightPawn* Pawn = World->SpawnActor<ASpaceFlightPawn>(ASpaceFlightPawn::StaticClass(), FTransform(Transform.Rotation, Transform.PositionCm), SpawnParameters);
		if (Pawn)
		{
			Pawn->SetFlightTestTransformAndVelocity(FTransform(Transform.Rotation, Transform.PositionCm), VelocityCmPerSec);
		}
		return Pawn;
	}

	void AdvanceM5Docking(UStargameSessionSubsystem* Session, ASpaceFlightPawn* Pawn, double TotalSeconds, double StepSeconds)
	{
		if (!Pawn)
		{
			return;
		}
		const int32 Steps = FMath::CeilToInt(TotalSeconds / FMath::Max(StepSeconds, SMALL_NUMBER));
		for (int32 StepIndex = 0; StepIndex < Steps; ++StepIndex)
		{
			if (Session)
			{
				Session->AdvanceSimulationClock(StepSeconds);
			}
			Pawn->TickDockingForTest(static_cast<float>(StepSeconds));
		}
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

	FString BuildM6SystemSignature(const FStarSystemDefinition& SystemDefinition)
	{
		TArray<FString> Parts;
		Parts.Add(SystemDefinition.SystemId.ToString());
		Parts.Add(FString::FromInt(SystemDefinition.Seed));
		Parts.Add(SystemDefinition.GeneratedSourceMetadata.GeneratorVersion.ToString());
		Parts.Add(SystemDefinition.GeneratedSourceMetadata.GeneratedAtUtc);
		Parts.Add(SystemDefinition.GeneratedSourceMetadata.SourceFingerprint);

		for (const FBodyDefinition& Body : SystemDefinition.Bodies)
		{
			Parts.Add(FString::Printf(TEXT("B:%s:%s:%s:%s:%.3f:%.3f:%.3f"),
				*Body.BodyId.ToString(),
				*Body.BodyType.ToString(),
				*Body.AnchorId.ToString(),
				*Body.Orbit.ParentId.ToString(),
				Body.Orbit.SemiMajorAxisCm,
				Body.Orbit.PeriodSeconds,
				Body.Orbit.PhaseOffsetRadians));
		}
		for (const FStationDefinition& Station : SystemDefinition.Stations)
		{
			Parts.Add(FString::Printf(TEXT("S:%s:%s:%.3f:%.3f:%.3f:%d"),
				*Station.StationId.ToString(),
				*Station.AnchorId.ToString(),
				Station.Orbit.SemiMajorAxisCm,
				Station.Orbit.PeriodSeconds,
				Station.Orbit.PhaseOffsetRadians,
				Station.DockingPorts.Num()));
		}
		for (const FGateDefinition& Gate : SystemDefinition.Gates)
		{
			Parts.Add(FString::Printf(TEXT("G:%s:%s:%s"),
				*Gate.GateId.ToString(),
				*Gate.AnchorId.ToString(),
				*Gate.Transform.GetLocation().ToCompactString()));
		}
		for (const FGravityWellDefinition& Well : SystemDefinition.GravityWells)
		{
			Parts.Add(FString::Printf(TEXT("W:%s:%s:%.3f:%.3f:%.3f"),
				*Well.WellId.ToString(),
				*Well.AnchorBodyId.ToString(),
				Well.SlowdownRadiusCm,
				Well.LockoutRadiusCm,
				Well.DropoutRadiusCm));
		}
		for (const FResourceZoneDefinition& Zone : SystemDefinition.ResourceZones)
		{
			Parts.Add(FString::Printf(TEXT("R:%s:%s:%s:%.3f"),
				*Zone.ZoneId.ToString(),
				*Zone.AnchorId.ToString(),
				*Zone.Transform.GetLocation().ToCompactString(),
				Zone.RadiusCm));
		}
		for (const FSpawnZoneDefinition& SpawnZone : SystemDefinition.SpawnZones)
		{
			Parts.Add(FString::Printf(TEXT("Z:%s:%s:%s"),
				*SpawnZone.SpawnZoneId.ToString(),
				*SpawnZone.AnchorId.ToString(),
				*SpawnZone.Transform.GetLocation().ToCompactString()));
		}
		for (const FMapEntryDefinition& MapEntry : SystemDefinition.MapEntries)
		{
			Parts.Add(FString::Printf(TEXT("M:%s:%s:%s"),
				*MapEntry.MapEntryId.ToString(),
				*MapEntry.SourceId.ToString(),
				*MapEntry.EntryType.ToString()));
		}
		return FString::Join(Parts, TEXT("|"));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM5CatalogValidationTest,
	"Stargame.M5.Catalog.ValidatesDockingFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM5CatalogValidationTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	const FStargameValidationReport Report = Catalog->ValidateM5Fixture(FName(TEXT("frontier_test_01")));
	TestFalse(TEXT("M5 validation report has no blocking issues"), Report.HasBlockingIssues());
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
	FM5DockingEligibilityAndAssistTest,
	"Stargame.M5.Docking.RejectsInvalidApproachAndDocksMovingPort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM5DockingEligibilityAndAssistTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M5 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}
	TestTrue(TEXT("M5 active system build succeeds"), StarSystem->BuildSystem(SystemDefinition));

	UStargameSessionSubsystem* Session = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	const double SimulationTimeSeconds = Session ? Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds : 0.0;
	const FSimulationClockSnapshot Clock = Session ? Session->GetSimulationClockSnapshot() : UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, SimulationTimeSeconds);
	FFrameResolvedTransform ApproachFrame;
	FFrameResolvedTransform DockedFrame;
	TestTrue(TEXT("M5 approach frame resolves"), UOrbitRouteFrameQueryService::ResolveDockingPortTransform(SystemDefinition, TEXT("brink_watch"), TEXT("brink_watch_port_a"), EDockingPortTransformKind::Approach, Clock, SimulationTimeSeconds, ApproachFrame));
	TestTrue(TEXT("M5 docked frame resolves"), UOrbitRouteFrameQueryService::ResolveDockingPortFrame(SystemDefinition, TEXT("brink_watch"), TEXT("brink_watch_port_a"), Clock, SimulationTimeSeconds, DockedFrame));

	ASpaceFlightPawn* FastPawn = SpawnM5PawnAt(World, ApproachFrame, FVector(3000.0, 0.0, 0.0));
	TestNotNull(TEXT("M5 fast pawn spawns"), FastPawn);
	TestFalse(TEXT("M5 rejects excessive approach speed"), FastPawn && FastPawn->RequestDocking(TEXT("brink_watch"), TEXT("brink_watch_port_a")));
	TestTrue(TEXT("M5 fast rejection records reason"), FastPawn && FastPawn->GetDockingOperationState().LastFailureReason.Contains(TEXT("speed")));
	if (FastPawn)
	{
		FastPawn->Destroy();
	}

	ASpaceFlightPawn* MisalignedPawn = SpawnM5PawnAt(World, ApproachFrame, FVector::ZeroVector);
	TestNotNull(TEXT("M5 misaligned pawn spawns"), MisalignedPawn);
	if (MisalignedPawn)
	{
		MisalignedPawn->SetActorRotation((DockedFrame.Rotation + FRotator(0.0, 90.0, 0.0)).Quaternion());
	}
	TestFalse(TEXT("M5 rejects wrong alignment"), MisalignedPawn && MisalignedPawn->RequestDocking(TEXT("brink_watch"), TEXT("brink_watch_port_a")));
	TestTrue(TEXT("M5 alignment rejection records reason"), MisalignedPawn && MisalignedPawn->GetDockingOperationState().LastFailureReason.Contains(TEXT("alignment")));
	if (MisalignedPawn)
	{
		MisalignedPawn->Destroy();
	}

	ASpaceFlightPawn* DockingPawn = SpawnM5PawnAt(World, ApproachFrame, FVector::ZeroVector);
	TestNotNull(TEXT("M5 docking pawn spawns"), DockingPawn);
	if (!DockingPawn)
	{
		StarSystem->TearDownActiveSystem();
		return false;
	}
	DockingPawn->SetActorRotation(DockedFrame.Rotation);
	TestTrue(TEXT("M5 valid request starts final assist"), DockingPawn->RequestDocking(TEXT("brink_watch"), TEXT("brink_watch_port_a")));
	TestEqual(TEXT("M5 enters final assist"), DockingPawn->GetDockingState(), EDockingState::FinalAssist);

	AdvanceM5Docking(Session, DockingPawn, 2.5, 0.25);
	TestEqual(TEXT("M5 final assist reaches docked"), DockingPawn->GetDockingState(), EDockingState::Docked);

	FDockingPortRuntimeState RuntimeState;
	TestTrue(TEXT("M5 runtime state resolves"), StarSystem->FindDockingPortRuntimeState(TEXT("brink_watch"), TEXT("brink_watch_port_a"), RuntimeState));
	TestEqual(TEXT("M5 port is occupied by player ship"), RuntimeState.OccupyingShipId, FName(TEXT("player_ship")));

	const double LiveTime = Session ? Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds : 2.5;
	FFrameResolvedTransform LiveDockedFrame;
	TestTrue(TEXT("M5 live docked frame resolves after assist"), UOrbitRouteFrameQueryService::ResolveDockingPortFrame(SystemDefinition, TEXT("brink_watch"), TEXT("brink_watch_port_a"), UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, LiveTime), LiveTime, LiveDockedFrame));
	TestTrue(TEXT("M5 docked pawn matches moving live port"), DockingPawn->GetActorLocation().Equals(LiveDockedFrame.PositionCm, 1.0));

	DockingPawn->Destroy();
	StarSystem->TearDownActiveSystem();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM5DockedSaveReloadUndockLoopTest,
	"Stargame.M5.Docking.SaveReloadAndRepeatUndock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM5DockedSaveReloadUndockLoopTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition SystemDefinition;
	TestTrue(TEXT("M5 system resolves"), Catalog && Catalog->ResolveSystemDefinition(FName(TEXT("frontier_test_01")), SystemDefinition));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}
	TestTrue(TEXT("M5 active system build succeeds"), StarSystem->BuildSystem(SystemDefinition));

	UStargameSessionSubsystem* Session = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UStargameSessionSubsystem>() : nullptr;
	if (Session)
	{
		Session->AdvanceSimulationClock(0.0);
	}

	for (int32 Iteration = 0; Iteration < 5; ++Iteration)
	{
		const double SimulationTimeSeconds = Session ? Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds : 0.0;
		const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, SimulationTimeSeconds);
		FFrameResolvedTransform ApproachFrame;
		FFrameResolvedTransform DockedFrame;
		TestTrue(TEXT("M5 loop approach frame resolves"), UOrbitRouteFrameQueryService::ResolveDockingPortTransform(SystemDefinition, TEXT("brink_watch"), TEXT("brink_watch_port_a"), EDockingPortTransformKind::Approach, Clock, SimulationTimeSeconds, ApproachFrame));
		TestTrue(TEXT("M5 loop docked frame resolves"), UOrbitRouteFrameQueryService::ResolveDockingPortFrame(SystemDefinition, TEXT("brink_watch"), TEXT("brink_watch_port_a"), Clock, SimulationTimeSeconds, DockedFrame));

		ASpaceFlightPawn* Pawn = SpawnM5PawnAt(World, ApproachFrame, FVector::ZeroVector);
		TestNotNull(TEXT("M5 loop pawn spawns"), Pawn);
		if (!Pawn)
		{
			StarSystem->TearDownActiveSystem();
			return false;
		}
		Pawn->SetActorRotation(DockedFrame.Rotation);
		TestTrue(TEXT("M5 loop docking request succeeds"), Pawn->RequestDocking(TEXT("brink_watch"), TEXT("brink_watch_port_a")));
		AdvanceM5Docking(Session, Pawn, 2.5, 0.25);
		TestEqual(TEXT("M5 loop reaches docked"), Pawn->GetDockingState(), EDockingState::Docked);

		FShipSaveLocation SaveLocation;
		SaveLocation.SystemId = SystemDefinition.SystemId;
		SaveLocation.LocationMode = EShipLocationMode::StationDocked;
		SaveLocation.DockedStationId = TEXT("brink_watch");
		SaveLocation.DockingPortId = TEXT("brink_watch_port_a");
		SaveLocation.DockingState = EDockingState::Docked;
		SaveLocation.AuthoritativeSimulationTimeSeconds = Session ? Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds : 0.0;
		TestEqual(TEXT("M5 save captures docked mode"), SaveLocation.LocationMode, EShipLocationMode::StationDocked);
		TestEqual(TEXT("M5 save captures docked station"), SaveLocation.DockedStationId, FName(TEXT("brink_watch")));
		TestEqual(TEXT("M5 save captures docked port"), SaveLocation.DockingPortId, FName(TEXT("brink_watch_port_a")));

		if (Session)
		{
			Session->AdvanceSimulationClock(5.0);
		}
		const double ReloadTimeSeconds = Session ? Session->GetSimulationClockSnapshot().AuthoritativeSimulationTimeSeconds : SaveLocation.AuthoritativeSimulationTimeSeconds + 5.0;
		FFrameResolvedTransform ExpectedReloadDockedFrame;
		TestTrue(TEXT("M5 reload expected frame resolves"), UOrbitRouteFrameQueryService::ResolveDockingPortFrame(SystemDefinition, SaveLocation.DockedStationId, SaveLocation.DockingPortId, UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(SystemDefinition.SystemId, ReloadTimeSeconds), ReloadTimeSeconds, ExpectedReloadDockedFrame));
		TestTrue(TEXT("M5 restore while docked succeeds after time advance"), Pawn->RestoreDockedAt(SaveLocation.DockedStationId, SaveLocation.DockingPortId, ReloadTimeSeconds));
		TestTrue(TEXT("M5 restored pawn matches live port"), Pawn->GetActorLocation().Equals(ExpectedReloadDockedFrame.PositionCm, 1.0));

		TestTrue(TEXT("M5 undock succeeds"), Pawn->Undock());
		TestEqual(TEXT("M5 undock returns to normal mode"), Pawn->GetFlightMode(), EShipFlightMode::Normal);
		FDockingPortRuntimeState RuntimeState;
		TestTrue(TEXT("M5 loop runtime resolves after undock"), StarSystem->FindDockingPortRuntimeState(TEXT("brink_watch"), TEXT("brink_watch_port_a"), RuntimeState));
		TestTrue(TEXT("M5 loop releases port after undock"), RuntimeState.OccupyingShipId.IsNone());

		Pawn->Destroy();
	}

	StarSystem->TearDownActiveSystem();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM6SeededGenerationValidationTest,
	"Stargame.M6.Generation.ValidatesPhysicalSystemDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM6SeededGenerationValidationTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	FStarSystemDefinition Generated;
	TestTrue(TEXT("M6 seeded physical system generates"), Catalog->GenerateSeededPhysicalSystem(6001, Generated));
	TestEqual(TEXT("M6 generated source type"), Generated.SourceType, ESystemSourceType::Generated);
	TestEqual(TEXT("M6 generated profile metadata"), Generated.GeneratedSourceMetadata.GenerationProfile, FName(TEXT("M6")));
	TestEqual(TEXT("M6 generated seed metadata"), Generated.GeneratedSourceMetadata.GeneratedSeed, 6001);
	TestFalse(TEXT("M6 declares authored overrides"), Generated.GeneratedSourceMetadata.AuthoredOverrideIds.IsEmpty());
	TestFalse(TEXT("M6 declares stable generation inputs"), Generated.GeneratedSourceMetadata.GenerationInputIds.IsEmpty());
	TestFalse(TEXT("M6 source fingerprint emitted"), Generated.GeneratedSourceMetadata.SourceFingerprint.IsEmpty());
	TestFalse(TEXT("M6 does not depend on array order"), Generated.GeneratedSourceMetadata.bDependsOnArrayOrder);
	TestFalse(TEXT("M6 does not depend on actor names"), Generated.GeneratedSourceMetadata.bDependsOnActorNames);
	TestFalse(TEXT("M6 does not depend on editor-only state"), Generated.GeneratedSourceMetadata.bDependsOnEditorOnlyState);

	const FStargameValidationReport Report = Catalog->ValidateM6GeneratedSystemDefinition(Generated);
	TestFalse(TEXT("M6 validation report has no blocking issues"), Report.HasBlockingIssues());
	if (Report.HasBlockingIssues())
	{
		for (const FStargameValidationIssue& Issue : Report.Issues)
		{
			AddError(Issue.Message);
		}
	}

	TestTrue(TEXT("M6 has star"), Generated.Bodies.ContainsByPredicate([](const FBodyDefinition& Body) { return Body.BodyType == FName(TEXT("star")); }));
	TestTrue(TEXT("M6 has moon hierarchy"), Generated.Bodies.ContainsByPredicate([](const FBodyDefinition& Body) { return Body.BodyType == FName(TEXT("moon")) && !Body.Orbit.ParentId.IsNone(); }));
	TestTrue(TEXT("M6 has moving station"), Generated.Stations.ContainsByPredicate([](const FStationDefinition& Station) { return Station.Orbit.SemiMajorAxisCm > 0.0 && Station.Orbit.PeriodSeconds > 0.0; }));
	TestTrue(TEXT("M6 has gate"), !Generated.Gates.IsEmpty());
	TestTrue(TEXT("M6 has gravity well lockout"), Generated.GravityWells.ContainsByPredicate([](const FGravityWellDefinition& Well) { return Well.SlowdownRadiusCm >= Well.LockoutRadiusCm && Well.LockoutRadiusCm >= Well.DropoutRadiusCm && Well.DropoutRadiusCm > 0.0; }));
	TestTrue(TEXT("M6 has resource zone"), !Generated.ResourceZones.IsEmpty());
	TestTrue(TEXT("M6 has spawn zone"), !Generated.SpawnZones.IsEmpty());
	TestTrue(TEXT("M6 has map metadata"), Generated.MapEntries.Num() >= Generated.Bodies.Num() + Generated.Stations.Num() + Generated.Gates.Num() + Generated.ResourceZones.Num());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM6SeededGenerationRejectsInvalidSourceMetadataTest,
	"Stargame.M6.Generation.RejectsInvalidSourceMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM6SeededGenerationRejectsInvalidSourceMetadataTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	FStarSystemDefinition MissingOverrides;
	TestTrue(TEXT("M6 baseline system generates for missing override case"), Catalog->GenerateSeededPhysicalSystem(6001, MissingOverrides));
	MissingOverrides.GeneratedSourceMetadata.AuthoredOverrideIds.Reset();
	TestTrue(TEXT("M6 rejects missing authored overrides"), Catalog->ValidateM6GeneratedSystemDefinition(MissingOverrides).HasBlockingIssues());

	FStarSystemDefinition MissingInputs;
	TestTrue(TEXT("M6 baseline system generates for missing input case"), Catalog->GenerateSeededPhysicalSystem(6001, MissingInputs));
	MissingInputs.GeneratedSourceMetadata.GenerationInputIds.Reset();
	TestTrue(TEXT("M6 rejects missing generation inputs"), Catalog->ValidateM6GeneratedSystemDefinition(MissingInputs).HasBlockingIssues());

	FStarSystemDefinition ActorDependent;
	TestTrue(TEXT("M6 baseline system generates for actor dependency case"), Catalog->GenerateSeededPhysicalSystem(6001, ActorDependent));
	ActorDependent.GeneratedSourceMetadata.bDependsOnActorNames = true;
	TestTrue(TEXT("M6 rejects actor-name generation dependency"), Catalog->ValidateM6GeneratedSystemDefinition(ActorDependent).HasBlockingIssues());

	FStarSystemDefinition InvalidTimestamp;
	TestTrue(TEXT("M6 baseline system generates for timestamp case"), Catalog->GenerateSeededPhysicalSystem(6001, InvalidTimestamp));
	InvalidTimestamp.GeneratedSourceMetadata.GeneratedAtUtc = TEXT("not-a-utc-timestamp");
	TestTrue(TEXT("M6 rejects invalid generated timestamp"), Catalog->ValidateM6GeneratedSystemDefinition(InvalidTimestamp).HasBlockingIssues());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM6SeededGenerationDeterminismTest,
	"Stargame.M6.Generation.SameSeedProducesSameDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM6SeededGenerationDeterminismTest::RunTest(const FString& Parameters)
{
	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	TestNotNull(TEXT("Catalog subsystem object created"), Catalog);
	if (!Catalog)
	{
		return false;
	}

	FStarSystemDefinition First;
	FStarSystemDefinition Second;
	FStarSystemDefinition Different;
	TestTrue(TEXT("M6 first system generates"), Catalog->GenerateSeededPhysicalSystem(6001, First));
	TestTrue(TEXT("M6 second system generates"), Catalog->GenerateSeededPhysicalSystem(6001, Second));
	TestTrue(TEXT("M6 different seed system generates"), Catalog->GenerateSeededPhysicalSystem(6002, Different));

	TestEqual(TEXT("M6 same seed signature matches"), BuildM6SystemSignature(First), BuildM6SystemSignature(Second));
	TestNotEqual(TEXT("M6 different seed signature differs"), BuildM6SystemSignature(First), BuildM6SystemSignature(Different));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FM6GeneratedRegistryCompatibilityTest,
	"Stargame.M6.Generation.BuildsActiveSystemRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FM6GeneratedRegistryCompatibilityTest::RunTest(const FString& Parameters)
{
	UWorld* World = FindM1AutomationWorld();
	TestNotNull(TEXT("Automation world exists"), World);
	if (!World)
	{
		return false;
	}

	UStarCatalogSubsystem* Catalog = CreateM1Catalog();
	FStarSystemDefinition Generated;
	TestTrue(TEXT("M6 generated system resolves"), Catalog && Catalog->GenerateSeededPhysicalSystem(6001, Generated));

	UStarSystemSubsystem* StarSystem = World->GetSubsystem<UStarSystemSubsystem>();
	TestNotNull(TEXT("Star system subsystem exists"), StarSystem);
	if (!StarSystem)
	{
		return false;
	}
	TestTrue(TEXT("M6 generated active system build succeeds"), StarSystem->BuildSystem(Generated));

	TArray<FName> EntityIds;
	StarSystem->GetRegisteredEntityIds(EntityIds);
	TestEqual(TEXT("M6 generated entity registry count"), EntityIds.Num(), Generated.Bodies.Num() + Generated.Stations.Num() + Generated.Gates.Num() + Generated.ResourceZones.Num());

	TArray<FName> ResourceZoneIds;
	StarSystem->GetRegisteredResourceZoneIds(ResourceZoneIds);
	TestEqual(TEXT("M6 generated resource zone registry count"), ResourceZoneIds.Num(), Generated.ResourceZones.Num());

	TArray<FName> MapEntryIds;
	StarSystem->GetRegisteredMapEntryIds(MapEntryIds);
	TestEqual(TEXT("M6 generated map registry count"), MapEntryIds.Num(), Generated.MapEntries.Num());

	TArray<FSystemMapEntryViewModel> MapView;
	const FSimulationClockSnapshot Clock = UOrbitRouteFrameQueryService::MakeDefaultClockSnapshot(Generated.SystemId, 0.0);
	UOrbitRouteFrameQueryService::BuildSystemMapViewModel(Generated, Clock, 0.0, MapView);
	TestEqual(TEXT("M6 generated map view resolves all entries"), MapView.Num(), Generated.MapEntries.Num());

	const FStationDefinition* MovingStation = Generated.Stations.Num() > 0 ? &Generated.Stations[0] : nullptr;
	TestNotNull(TEXT("M6 generated moving station exists"), MovingStation);
	if (MovingStation)
	{
		FFrameResolvedTransform StationAt0;
		FFrameResolvedTransform StationAt30;
		TestTrue(TEXT("M6 generated station resolves at t0"), UOrbitRouteFrameQueryService::ResolveEntityFrame(Generated, MovingStation->StationId, Clock, 0.0, StationAt0));
		TestTrue(TEXT("M6 generated station resolves at t30"), UOrbitRouteFrameQueryService::ResolveEntityFrame(Generated, MovingStation->StationId, Clock, 30.0, StationAt30));
		TestFalse(TEXT("M6 generated station moves"), StationAt0.PositionCm.Equals(StationAt30.PositionCm, 0.01));

		FFrameResolvedTransform SpawnTransform;
		TestTrue(TEXT("M6 generated spawn resolves"), UOrbitRouteFrameQueryService::ResolveEntityFrame(Generated, Generated.SpawnZones[0].SpawnZoneId, Clock, 0.0, SpawnTransform));
		TestTrue(TEXT("M6 generated has long supercruise leg"), FVector::Distance(SpawnTransform.PositionCm, StationAt0.PositionCm) > Generated.Scale.LocalBubbleRadiusCm);
	}

	StarSystem->TearDownActiveSystem();
	return true;
}

#endif
